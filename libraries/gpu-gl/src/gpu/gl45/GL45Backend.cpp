//
//  Created by Sam Gateau on 10/27/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "GL45Backend.h"

#include <mutex>
#include <queue>
#include <list>
#include <functional>
#include <glm/gtc/type_ptr.hpp>

Q_LOGGING_CATEGORY(gpugl45logging, "hifi.gpu.gl45")

using namespace gpu;
using namespace gpu::gl45;

GLint GL45Backend::MAX_COMBINED_SHADER_STORAGE_BLOCKS{ 0 };
GLint GL45Backend::MAX_UNIFORM_LOCATIONS{ 0 };

static void staticInit() {
    static std::once_flag once;
    std::call_once(once, [&] {
        glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &GL45Backend::MAX_COMBINED_SHADER_STORAGE_BLOCKS);
        glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &GL45Backend::MAX_UNIFORM_LOCATIONS);
    });
}
const std::string GL45Backend::GL45_VERSION { "GL45" };

GL45Backend::GL45Backend(bool syncCache) : Parent(syncCache) {
    Backend::depthClipZeroToOne = true;
    staticInit();
}

GL45Backend::GL45Backend() : Parent() {
    Backend::depthClipZeroToOne = true;
    staticInit();
}

void GL45Backend::recycle() const {
    Parent::recycle();
}

void GL45Backend::draw(GLenum mode, uint32 numVertices, uint32 startVertex) {
    if (isStereo()) {
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glDrawArraysInstanced(mode, startVertex, numVertices, 2);
#else
        setupStereoSide(0);
        glDrawArrays(mode, startVertex, numVertices);
        setupStereoSide(1);
        glDrawArrays(mode, startVertex, numVertices);
#endif

        _stats._DSNumTriangles += 2 * numVertices / 3;
        _stats._DSNumDrawcalls += 2;

    } else {
        glDrawArrays(mode, startVertex, numVertices);
        _stats._DSNumTriangles += numVertices / 3;
        _stats._DSNumDrawcalls++;
    }
    _stats._DSNumAPIDrawcalls++;

    (void)CHECK_GL_ERROR();
}

void GL45Backend::do_draw(const Batch& batch, size_t paramOffset) {
    Primitive primitiveType = (Primitive)batch._params[paramOffset + 2]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[primitiveType];
    uint32 numVertices = batch._params[paramOffset + 1]._uint;
    uint32 startVertex = batch._params[paramOffset + 0]._uint;

    draw(mode, numVertices, startVertex);
}

void GL45Backend::do_drawIndexed(const Batch& batch, size_t paramOffset) {
    Primitive primitiveType = (Primitive)batch._params[paramOffset + 2]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[primitiveType];
    uint32 numIndices = batch._params[paramOffset + 1]._uint;
    uint32 startIndex = batch._params[paramOffset + 0]._uint;

    GLenum glType = gl::ELEMENT_TYPE_TO_GL[_input._indexBufferType];
    
    auto typeByteSize = TYPE_SIZE[_input._indexBufferType];
    GLvoid* indexBufferByteOffset = reinterpret_cast<GLvoid*>(startIndex * typeByteSize + _input._indexBufferOffset);

    if (isStereo()) {
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glDrawElementsInstanced(mode, numIndices, glType, indexBufferByteOffset, 2);
#else
        setupStereoSide(0);
        glDrawElements(mode, numIndices, glType, indexBufferByteOffset);
        setupStereoSide(1);
        glDrawElements(mode, numIndices, glType, indexBufferByteOffset);
#endif
        _stats._DSNumTriangles += 2 * numIndices / 3;
        _stats._DSNumDrawcalls += 2;
    } else {
        glDrawElements(mode, numIndices, glType, indexBufferByteOffset);
        _stats._DSNumTriangles += numIndices / 3;
        _stats._DSNumDrawcalls++;
    }
    _stats._DSNumAPIDrawcalls++;

    (void) CHECK_GL_ERROR();
}

void GL45Backend::do_drawInstanced(const Batch& batch, size_t paramOffset) {
    GLint numInstances = batch._params[paramOffset + 4]._uint;
    Primitive primitiveType = (Primitive)batch._params[paramOffset + 3]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[primitiveType];
    uint32 numVertices = batch._params[paramOffset + 2]._uint;
    uint32 startVertex = batch._params[paramOffset + 1]._uint;


    if (isStereo()) {
        GLint trueNumInstances = 2 * numInstances;

#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glDrawArraysInstanced(mode, startVertex, numVertices, trueNumInstances);
#else
        setupStereoSide(0);
        glDrawArraysInstanced(mode, startVertex, numVertices, numInstances);
        setupStereoSide(1);
        glDrawArraysInstanced(mode, startVertex, numVertices, numInstances);
#endif

        _stats._DSNumTriangles += (trueNumInstances * numVertices) / 3;
        _stats._DSNumDrawcalls += trueNumInstances;
    } else {
        glDrawArraysInstanced(mode, startVertex, numVertices, numInstances);
        _stats._DSNumTriangles += (numInstances * numVertices) / 3;
        _stats._DSNumDrawcalls += numInstances;
    }
    _stats._DSNumAPIDrawcalls++;

    (void) CHECK_GL_ERROR();
}

void GL45Backend::do_drawIndexedInstanced(const Batch& batch, size_t paramOffset) {
    GLint numInstances = batch._params[paramOffset + 4]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[(Primitive)batch._params[paramOffset + 3]._uint];
    uint32 numIndices = batch._params[paramOffset + 2]._uint;
    uint32 startIndex = batch._params[paramOffset + 1]._uint;
    uint32 startInstance = batch._params[paramOffset + 0]._uint;
    GLenum glType = gl::ELEMENT_TYPE_TO_GL[_input._indexBufferType];
    auto typeByteSize = TYPE_SIZE[_input._indexBufferType];
    GLvoid* indexBufferByteOffset = reinterpret_cast<GLvoid*>(startIndex * typeByteSize + _input._indexBufferOffset);
 
    if (isStereo()) {
        GLint trueNumInstances = 2 * numInstances;

#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, trueNumInstances, 0, startInstance);
#else
        setupStereoSide(0);
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, startInstance);
        setupStereoSide(1);
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, startInstance);
#endif
        _stats._DSNumTriangles += (trueNumInstances * numIndices) / 3;
        _stats._DSNumDrawcalls += trueNumInstances;
    } else {
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, startInstance);
        _stats._DSNumTriangles += (numInstances * numIndices) / 3;
        _stats._DSNumDrawcalls += numInstances;
    }

    _stats._DSNumAPIDrawcalls++;

    (void)CHECK_GL_ERROR();
}

// GPU instancing (INSTANCING.md 3.4): an indexed draw of the batch's inline instance list, one instance per entry
void GL45Backend::do_drawIndexedInstances(const Batch& batch, size_t paramOffset) {
    GLint numInstances = batch._params[paramOffset + 4]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[(Primitive)batch._params[paramOffset + 3]._uint];
    uint32 numIndices = batch._params[paramOffset + 2]._uint;
    uint32 startIndex = batch._params[paramOffset + 1]._uint;
    uint32 listOffset = batch._params[paramOffset + 0]._uint;
    GLenum glType = gl::ELEMENT_TYPE_TO_GL[_input._indexBufferType];
    auto typeByteSize = TYPE_SIZE[_input._indexBufferType];
    GLvoid* indexBufferByteOffset = reinterpret_cast<GLvoid*>(startIndex * typeByteSize + _input._indexBufferOffset);

    // the draw-call-info attribute reads this draw's list, an entry per instance (per two in stereo), as it reads a
    // named call's; updateTransform set it constant before this draw and sets it constant again for the next plain one
    if (!_transform._enabledDrawcallInfoBuffer) {
        glEnableVertexAttribArray(gpu::Stream::DRAW_CALL_INFO);
        glVertexAttribIFormat(gpu::Stream::DRAW_CALL_INFO, 2, GL_UNSIGNED_SHORT, 0);
        glVertexAttribBinding(gpu::Stream::DRAW_CALL_INFO, gpu::Stream::DRAW_CALL_INFO);
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glVertexBindingDivisor(gpu::Stream::DRAW_CALL_INFO, (isStereo() ? 2 : 1));
#else
        glVertexBindingDivisor(gpu::Stream::DRAW_CALL_INFO, 1);
#endif
        _transform._enabledDrawcallInfoBuffer = true;
    }
    glBindVertexBuffer(gpu::Stream::DRAW_CALL_INFO, _transform._drawCallInfoBuffer,
        (GLintptr)(_transform._instanceDrawCallInfoOffset + listOffset * sizeof(Batch::DrawCallInfo)), 2 * sizeof(GLushort));

    if (isStereo()) {
        GLint trueNumInstances = 2 * numInstances;
#ifdef GPU_STEREO_DRAWCALL_INSTANCED
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, trueNumInstances, 0, 0);
#else
        setupStereoSide(0);
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, 0);
        setupStereoSide(1);
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, 0);
#endif
        _stats._DSNumTriangles += (trueNumInstances * numIndices) / 3;
        _stats._DSNumDrawcalls += trueNumInstances;
    } else {
        glDrawElementsInstancedBaseVertexBaseInstance(mode, numIndices, glType, indexBufferByteOffset, numInstances, 0, 0);
        _stats._DSNumTriangles += (numInstances * numIndices) / 3;
        _stats._DSNumDrawcalls += numInstances;
    }
    _stats._DSNumAPIDrawcalls++;
    _stats._DSNumInstancedDrawcalls++;
    (void)CHECK_GL_ERROR();
}

void GL45Backend::do_multiDrawIndirect(const Batch& batch, size_t paramOffset) {
    uint commandCount = batch._params[paramOffset + 0]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[(Primitive)batch._params[paramOffset + 1]._uint];
    glMultiDrawArraysIndirect(mode, reinterpret_cast<GLvoid*>(_input._indirectBufferOffset), commandCount, (GLsizei)_input._indirectBufferStride);
    _stats._DSNumDrawcalls += commandCount;
    _stats._DSNumAPIDrawcalls++;
    (void)CHECK_GL_ERROR();
}

void GL45Backend::do_multiDrawIndexedIndirect(const Batch& batch, size_t paramOffset) {
    uint commandCount = batch._params[paramOffset + 0]._uint;
    GLenum mode = gl::PRIMITIVE_TO_GL[(Primitive)batch._params[paramOffset + 1]._uint];
    GLenum indexType = gl::ELEMENT_TYPE_TO_GL[_input._indexBufferType];
    glMultiDrawElementsIndirect(mode, indexType, reinterpret_cast<GLvoid*>(_input._indirectBufferOffset), commandCount, (GLsizei)_input._indirectBufferStride);
    _stats._DSNumDrawcalls += commandCount;
    _stats._DSNumAPIDrawcalls++;
    (void)CHECK_GL_ERROR();
}
