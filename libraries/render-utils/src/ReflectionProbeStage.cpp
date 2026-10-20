//
//  ReflectionProbeStage.cpp
//  render-utils/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "ReflectionProbeStage.h"

#include <algorithm>

#include "RenderUtilsLogging.h"

std::string ReflectionProbeStage::_stageName { "REFLECTION_PROBE_STAGE" };
const ReflectionProbeStage::Index ReflectionProbeStage::INVALID_INDEX { render::indexed_container::INVALID_INDEX };

ReflectionProbeStage::Index ReflectionProbeStage::addProbe(const ProbePointer& probe) {
    return _probes.newElement(probe);
}

ReflectionProbeStage::ProbePointer ReflectionProbeStage::removeProbe(Index index) {
    if (!_probes.checkIndex(index)) {
        return nullptr;
    }
    return _probes.freeElement(index);
}

void ReflectionProbeStage::updateArray() {
    // the probes with a cube map: their textures and the textures' data make the key the array was built for
    std::vector<ArrayEntry> entries;
    const Index count = _probes.getNumAllocatedIndices();
    for (Index i = 0; i < count; i++) {
        if (!_probes.checkIndex(i)) {
            continue;
        }
        const auto& probe = _probes.get(i);
        if (!probe || !probe->cubemap || !probe->cubemap->isDefined()) {
            continue;
        }
        entries.push_back({ i, probe->cubemap.get(), probe->cubemap->getDataStamp() });
    }
    if (entries == _arrayEntries && _layers.size() == (size_t)count) {
        return;
    }
    _arrayEntries = entries;
    _layers.assign(count, -1);
    _array.reset();
    _arrayNumMips = 0;
    if (entries.empty()) {
        return;
    }

    // one size for every layer: the smallest cube map's face, at most MAX_FACE_SIZE; a bigger cube map gives its
    // smaller mips, which its GGX prefiltering made for a slightly different roughness per level (a small error);
    // one format: the first cube map's (every probe loads the ambient map's way, so they agree on one machine)
    int size = MAX_FACE_SIZE;
    for (const auto& entry : entries) {
        size = std::min(size, (int)entry.texture->getWidth());
    }
    size = std::max(size, 1);
    const gpu::Element format = entries.front().texture->getTexelFormat();
    const gpu::Element storedFormat = entries.front().texture->getStoredMipFormat();
    const uint16_t numMips = gpu::Texture::evalDimMaxNumMips((uint16_t)size);
    const uint16_t numLayers = (uint16_t)std::min((int)entries.size(), MAX_LAYERS);
    auto array = gpu::Texture::createCubeArray(format, (uint16_t)size, numLayers, numMips,
                                               gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR, gpu::Sampler::WRAP_CLAMP));
    array->setStoredMipFormat(storedFormat);
    array->setSource("reflection probes");

    int layer = 0;
    for (const auto& entry : entries) {
        if (layer >= numLayers) {
            qCWarning(renderutils) << "Reflection probes: more than" << MAX_LAYERS << "probes with a cube map; the rest are left out";
            break;
        }
        const auto& texture = *entry.texture;
        if (texture.getType() != gpu::Texture::TEX_CUBE || texture.isArray() || !(texture.getTexelFormat() == format) ||
            !(texture.getStoredMipFormat() == storedFormat)) {
            qCWarning(renderutils) << "Reflection probe cube map" << texture.source().c_str() << "is not a cube map of the probes' format; left out";
            continue;
        }
        const int width = texture.getWidth();
        const int ratio = width / std::max(size, 1);
        if (width < size || width % size != 0 || (ratio & (ratio - 1)) != 0) {
            qCWarning(renderutils) << "Reflection probe cube map" << texture.source().c_str() << "is" << width << "a face, not a power of two of" << size << "; left out";
            continue;
        }
        int shift = 0;
        for (int w = width; w > size; w >>= 1) {
            shift++;
        }
        if (texture.getNumMips() < numMips + shift) {
            continue;
        }
        bool complete = true;
        for (uint16_t mip = 0; mip < numMips && complete; mip++) {
            for (uint8_t face = 0; face < 6; face++) {
                if (!texture.isStoredMipFaceAvailable(mip + shift, face)) {
                    complete = false;
                    break;
                }
            }
        }
        if (!complete) {
            continue;   // still arriving: its data stamp will change and bring the build round again
        }
        for (uint16_t mip = 0; mip < numMips; mip++) {
            for (uint8_t face = 0; face < 6; face++) {
                auto storage = texture.accessStoredMipFace(mip + shift, face);
                array->assignStoredMipFace(mip, (uint8_t)(layer * 6 + face), storage);
            }
        }
        _layers[entry.index] = layer;
        layer++;
    }
    if (layer == 0) {
        return;
    }
    _array = array;
    _arrayNumMips = numMips;
    qCDebug(renderutils) << "Reflection probes: cube map array of" << layer << "layers," << size << "a face," << numMips << "mips";
}

const gpu::TexturePointer& ReflectionProbeStage::getEmptyArray() {
    static const gpu::TexturePointer EMPTY = [] {
        auto texture = gpu::Texture::createCubeArray(gpu::Element::COLOR_R11G11B10, 1, 1, 1,
                                                     gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR, gpu::Sampler::WRAP_CLAMP));
        texture->setStoredMipFormat(gpu::Element::COLOR_R11G11B10);
        const uint32_t black = 0;
        for (uint8_t face = 0; face < 6; face++) {
            texture->assignStoredMipFace(0, face, sizeof(black), (const gpu::Byte*)&black);
        }
        texture->setSource("no reflection probes");
        return texture;
    }();
    return EMPTY;
}

void ReflectionProbeStageSetup::run(const render::RenderContextPointer& renderContext) {
    auto stage = renderContext->_scene->getStage(ReflectionProbeStage::getName());
    if (!stage) {
        renderContext->_scene->resetStage(ReflectionProbeStage::getName(), std::make_shared<ReflectionProbeStage>());
    }
}
