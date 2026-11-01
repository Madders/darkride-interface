//
//  FaceLandmarker.cpp
//  plugins/face-tracking-plugin/src
//
//  The steps and constants are MediaPipe's face landmarker graph's (mediapipe/tasks/cc/vision/face_landmarker and
//  face_detector, Apache-2.0): the short-range face detector's anchors and decoding, the crops (the eyes level, 1.5
//  times the face, square), the blendshape model's 146 landmarks, and the canonical face's weighted points.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "FaceLandmarker.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <tensorflow/lite/c/c_api.h>
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>

#include "TaskBundle.h"

const char* const FaceLandmarker::BLENDSHAPE_NAMES[NUM_BLENDSHAPES] = {
    "_neutral", "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight", "cheekPuff",
    "cheekSquintLeft", "cheekSquintRight", "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight",
    "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight", "eyeLookUpLeft", "eyeLookUpRight",
    "eyeSquintLeft", "eyeSquintRight", "eyeWideLeft", "eyeWideRight", "jawForward", "jawLeft", "jawOpen", "jawRight",
    "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight", "mouthFunnel", "mouthLeft",
    "mouthLowerDownLeft", "mouthLowerDownRight", "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
    "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper", "mouthSmileLeft", "mouthSmileRight",
    "mouthStretchLeft", "mouthStretchRight", "mouthUpperUpLeft", "mouthUpperUpRight", "noseSneerLeft", "noseSneerRight",
};

namespace {

const float PI = 3.14159265358979f;

// the detector: 128 x 128, values -1 to 1, 896 anchors, each a box and six points (the eyes, the nose, the mouth, the
// ears), the subject's right eye first
const int DETECTOR_SIZE = 128;
const int NUM_ANCHORS = 896;
const int DETECTOR_COORDINATES = 16;
const float DETECTOR_MIN_SCORE = 0.5f;
const float DETECTOR_SCORE_CLIP = 100.0f;
const float DETECTOR_MIN_OVERLAP = 0.3f;  // boxes overlapping the best by more are averaged into it
const int RIGHT_EYE_POINT = 0;
const int LEFT_EYE_POINT = 1;

// the landmarks: 256 x 256, values 0 to 1; the crop is 1.5 times the face, square, its eyes level
const int LANDMARKER_SIZE = 256;
const float FACE_SCALE = 1.5f;
const float MIN_FACE_PRESENCE = 0.5f;
const int RIGHT_EYE_OUTER = 33;
const int LEFT_EYE_OUTER = 263;

// the blendshape model reads these landmarks' x and y in the frame's pixels
const int BLENDSHAPE_LANDMARKS[] = {
    0,   1,   4,   5,   6,   7,   8,   10,  13,  14,  17,  21,  33,  37,  39,  40,  46,  52,  53,  54,  55,  58,
    61,  63,  65,  66,  67,  70,  78,  80,  81,  82,  84,  87,  88,  91,  93,  95,  103, 105, 107, 109, 127, 132,
    133, 136, 144, 145, 146, 148, 149, 150, 152, 153, 154, 155, 157, 158, 159, 160, 161, 162, 163, 168, 172, 173,
    176, 178, 181, 185, 191, 195, 197, 234, 246, 249, 251, 263, 267, 269, 270, 276, 282, 283, 284, 285, 288, 291,
    293, 295, 296, 297, 300, 308, 310, 311, 312, 314, 317, 318, 321, 323, 324, 332, 334, 336, 338, 356, 361, 362,
    365, 373, 374, 375, 377, 378, 379, 380, 381, 382, 384, 385, 386, 387, 388, 389, 390, 397, 398, 400, 402, 405,
    409, 415, 454, 466, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477,
};
const int NUM_BLENDSHAPE_LANDMARKS = sizeof(BLENDSHAPE_LANDMARKS) / sizeof(BLENDSHAPE_LANDMARKS[0]);
static_assert(NUM_BLENDSHAPE_LANDMARKS == 146, "the blendshape model reads 146 landmarks");

float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

float normalizeRadians(float angle) {
    return angle - 2.0f * PI * std::floor((angle + PI) / (2.0f * PI));
}

// The short-range detector's anchors (MediaPipe's SsdAnchorsCalculator: four layers of strides 8, 16, 16 and 16, two
// anchors a layer, the layers of one stride sharing a grid; fixed size, so only the centers differ): 16 x 16 cells of 2,
// then 8 x 8 cells of 6.
std::vector<glm::vec4> detectorAnchors() {
    const int strides[] = { 8, 16, 16, 16 };
    const int numLayers = 4;
    const int anchorsPerLayer = 2;
    std::vector<glm::vec4> anchors;
    for (int layer = 0; layer < numLayers;) {
        int perCell = 0;
        int last = layer;
        while (last < numLayers && strides[last] == strides[layer]) {
            perCell += anchorsPerLayer;
            last++;
        }
        const int cells = (int)std::ceil((float)DETECTOR_SIZE / strides[layer]);
        for (int y = 0; y < cells; y++) {
            for (int x = 0; x < cells; x++) {
                for (int i = 0; i < perCell; i++) {
                    anchors.push_back(glm::vec4((x + 0.5f) / cells, (y + 0.5f) / cells, 1.0f, 1.0f));
                }
            }
        }
        layer = last;
    }
    return anchors;
}

// The square's pixels into a width x height x 3 tensor: bilinear, black outside the frame, 0 to 255 mapped to low to
// high. The frame is RGB32.
void sample(const QImage& frame, glm::vec2 center, float size, float rotation, int width, int height, float low,
            float high, float* out) {
    const float scale = (high - low) / 255.0f;
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);
    const int frameWidth = frame.width();
    const int frameHeight = frame.height();
    const uchar* bits = frame.constBits();
    const int stride = frame.bytesPerLine();
    auto pixel = [&](int x, int y) -> QRgb {
        if (x < 0 || y < 0 || x >= frameWidth || y >= frameHeight) {
            return 0;
        }
        return reinterpret_cast<const QRgb*>(bits + y * stride)[x];
    };
    for (int v = 0; v < height; v++) {
        const float ny = ((v + 0.5f) / height - 0.5f) * size;
        for (int u = 0; u < width; u++) {
            const float nx = ((u + 0.5f) / width - 0.5f) * size;
            // the pixel's center in the frame, then the four pixels around it
            const float x = center.x + c * nx - s * ny - 0.5f;
            const float y = center.y + s * nx + c * ny - 0.5f;
            const int x0 = (int)std::floor(x);
            const int y0 = (int)std::floor(y);
            const float fx = x - x0;
            const float fy = y - y0;
            const QRgb p00 = pixel(x0, y0);
            const QRgb p10 = pixel(x0 + 1, y0);
            const QRgb p01 = pixel(x0, y0 + 1);
            const QRgb p11 = pixel(x0 + 1, y0 + 1);
            auto mix = [&](int (*channel)(QRgb)) {
                const float top = channel(p00) + (channel(p10) - channel(p00)) * fx;
                const float bottom = channel(p01) + (channel(p11) - channel(p01)) * fx;
                return top + (bottom - top) * fy;
            };
            float* rgb = out + 3 * (v * width + u);
            rgb[0] = low + mix(qRed) * scale;
            rgb[1] = low + mix(qGreen) * scale;
            rgb[2] = low + mix(qBlue) * scale;
        }
    }
}

// A minimal protobuf reader: the canonical face's metadata needs varints, 32-bit floats and nested messages.
struct ProtoReader {
    const uchar* at;
    const uchar* end;

    bool done() const { return at >= end; }

    bool varint(quint64& value) {
        value = 0;
        for (int shift = 0; shift < 64 && at < end; shift += 7) {
            const uchar byte = *at++;
            value |= (quint64)(byte & 0x7f) << shift;
            if (!(byte & 0x80)) {
                return true;
            }
        }
        return false;
    }

    // the next field: its number and wire type, and its value (a varint's, a float's bits, or a length's bytes)
    bool next(int& field, int& type, quint64& value, ProtoReader& bytes) {
        quint64 key;
        if (!varint(key)) {
            return false;
        }
        field = (int)(key >> 3);
        type = (int)(key & 7);
        switch (type) {
            case 0:
                return varint(value);
            case 1:
                if (end - at < 8) {
                    return false;
                }
                at += 8;
                return true;
            case 2: {
                quint64 length;
                if (!varint(length) || (quint64)(end - at) < length) {
                    return false;
                }
                bytes = { at, at + length };
                at += length;
                return true;
            }
            case 5: {
                if (end - at < 4) {
                    return false;
                }
                quint32 bits;
                memcpy(&bits, at, 4);
                value = bits;
                at += 4;
                return true;
            }
            default:
                return false;
        }
    }
};

float floatFromBits(quint64 bits) {
    quint32 word = (quint32)bits;
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

// the rotation nearest m (the orthogonal factor of its polar decomposition), by Newton's iteration
glm::mat3 nearestRotation(const glm::mat3& m) {
    glm::mat3 r = m;
    for (int i = 0; i < 30; i++) {
        const glm::mat3 next = 0.5f * (r + glm::transpose(glm::inverse(r)));
        float change = 0.0f;
        for (int column = 0; column < 3; column++) {
            change += glm::length(next[column] - r[column]);
        }
        r = next;
        if (change < 1.0e-6f) {
            break;
        }
    }
    return r;
}

}  // namespace

FaceLandmarker::~FaceLandmarker() {
    freeModel(_detector);
    freeModel(_landmarker);
    freeModel(_blendshaper);
}

bool FaceLandmarker::loadModel(Model& model, const QByteArray& data, int threads, QString& error) {
    freeModel(model);
    // TensorFlow Lite reads the model in place for as long as it lives
    model.data = data;
    model.model = TfLiteModelCreate(model.data.constData(), (size_t)model.data.size());
    if (!model.model) {
        error = "a model didn't load";
        return false;
    }
    TfLiteXNNPackDelegateOptions xnnpack = TfLiteXNNPackDelegateOptionsDefault();
    xnnpack.num_threads = threads;
    model.delegate = TfLiteXNNPackDelegateCreate(&xnnpack);
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(options, threads);
    if (model.delegate) {
        TfLiteInterpreterOptionsAddDelegate(options, model.delegate);
    }
    model.interpreter = TfLiteInterpreterCreate(model.model, options);
    TfLiteInterpreterOptionsDelete(options);
    if (!model.interpreter || TfLiteInterpreterAllocateTensors(model.interpreter) != kTfLiteOk) {
        error = "a model didn't start";
        return false;
    }
    return true;
}

void FaceLandmarker::freeModel(Model& model) {
    if (model.interpreter) {
        TfLiteInterpreterDelete(model.interpreter);
    }
    if (model.delegate) {
        TfLiteXNNPackDelegateDelete(model.delegate);
    }
    if (model.model) {
        TfLiteModelDelete(model.model);
    }
    model = Model();
}

bool FaceLandmarker::loadGeometry(const QByteArray& metadata, QString& error) {
    // GeometryPipelineMetadata: 1 the canonical mesh (1 vertex type, 2 primitive type, 3 the vertices' x, y, z, u and v,
    // 4 the triangles), 2 the weighted landmarks the head is fitted on (1 landmark, 2 weight), 3 the input source
    std::vector<float> vertices;
    _fitLandmarks.clear();
    _fitWeights.clear();
    ProtoReader top { reinterpret_cast<const uchar*>(metadata.constData()),
                      reinterpret_cast<const uchar*>(metadata.constData()) + metadata.size() };
    while (!top.done()) {
        int field, type;
        quint64 value = 0;
        ProtoReader bytes { nullptr, nullptr };
        if (!top.next(field, type, value, bytes)) {
            error = "the canonical face is damaged";
            return false;
        }
        if (field == 1 && type == 2) {
            while (!bytes.done()) {
                int meshField, meshType;
                quint64 meshValue = 0;
                ProtoReader packed { nullptr, nullptr };
                if (!bytes.next(meshField, meshType, meshValue, packed)) {
                    error = "the canonical face is damaged";
                    return false;
                }
                if (meshField == 3 && meshType == 5) {
                    vertices.push_back(floatFromBits(meshValue));
                } else if (meshField == 3 && meshType == 2) {
                    for (const uchar* p = packed.at; p + 4 <= packed.end; p += 4) {
                        quint32 bits;
                        memcpy(&bits, p, 4);
                        vertices.push_back(floatFromBits(bits));
                    }
                }
            }
        } else if (field == 2 && type == 2) {
            quint64 landmark = 0;
            float weight = 0.0f;
            while (!bytes.done()) {
                int refField, refType;
                quint64 refValue = 0;
                ProtoReader unused { nullptr, nullptr };
                if (!bytes.next(refField, refType, refValue, unused)) {
                    error = "the canonical face is damaged";
                    return false;
                }
                if (refField == 1 && refType == 0) {
                    landmark = refValue;
                } else if (refField == 2 && refType == 5) {
                    weight = floatFromBits(refValue);
                }
            }
            _fitLandmarks.push_back((int)landmark);
            _fitWeights.push_back(weight);
        }
    }
    const int VERTEX_FLOATS = 5;  // x, y, z, u, v
    const int numVertices = (int)vertices.size() / VERTEX_FLOATS;
    _fitCanonical.clear();
    for (int landmark : _fitLandmarks) {
        if (landmark < 0 || landmark >= numVertices || landmark >= NUM_LANDMARKS) {
            error = "the canonical face names a point it hasn't got";
            return false;
        }
        const float* v = vertices.data() + VERTEX_FLOATS * landmark;
        _fitCanonical.push_back(glm::vec3(v[0], v[1], v[2]));
    }
    if (_fitLandmarks.size() < 3) {
        error = "the canonical face has too few points";
        return false;
    }
    return true;
}

bool FaceLandmarker::load(const QByteArray& task, int threads, QString& error) {
    TaskBundle bundle;
    if (!bundle.open(task)) {
        error = "the face model is damaged: " + bundle.error();
        return false;
    }
    if (!loadModel(_detector, bundle.file("face_detector.tflite"), threads, error) ||
        !loadModel(_landmarker, bundle.file("face_landmarks_detector.tflite"), threads, error) ||
        !loadModel(_blendshaper, bundle.file("face_blendshapes.tflite"), threads, error) ||
        !loadGeometry(bundle.file("geometry_pipeline_metadata_landmarks.binarypb"), error)) {
        return false;
    }

    // the models are the ones this code was written for: their inputs' and outputs' sizes
    auto floats = [](const TfLiteTensor* tensor) {
        return tensor && TfLiteTensorType(tensor) == kTfLiteFloat32 ? (int)(TfLiteTensorByteSize(tensor) / sizeof(float))
                                                                     : -1;
    };
    if (floats(TfLiteInterpreterGetInputTensor(_detector.interpreter, 0)) != DETECTOR_SIZE * DETECTOR_SIZE * 3 ||
        TfLiteInterpreterGetOutputTensorCount(_detector.interpreter) < 2 ||
        floats(TfLiteInterpreterGetOutputTensor(_detector.interpreter, 0)) != NUM_ANCHORS * DETECTOR_COORDINATES ||
        floats(TfLiteInterpreterGetOutputTensor(_detector.interpreter, 1)) != NUM_ANCHORS ||
        floats(TfLiteInterpreterGetInputTensor(_landmarker.interpreter, 0)) != LANDMARKER_SIZE * LANDMARKER_SIZE * 3 ||
        TfLiteInterpreterGetOutputTensorCount(_landmarker.interpreter) < 2 ||
        floats(TfLiteInterpreterGetOutputTensor(_landmarker.interpreter, 0)) != NUM_LANDMARKS * 3 ||
        floats(TfLiteInterpreterGetOutputTensor(_landmarker.interpreter, 1)) != 1 ||
        floats(TfLiteInterpreterGetInputTensor(_blendshaper.interpreter, 0)) != NUM_BLENDSHAPE_LANDMARKS * 2 ||
        floats(TfLiteInterpreterGetOutputTensor(_blendshaper.interpreter, 0)) != NUM_BLENDSHAPES) {
        error = "the face model isn't the expected version";
        return false;
    }
    _anchors = detectorAnchors();
    _tracking = false;
    return true;
}

bool FaceLandmarker::detect(const QImage& frame, Square& face) {
    // the whole frame, letterboxed into the detector's square
    const glm::vec2 frameCenter(frame.width() * 0.5f, frame.height() * 0.5f);
    const float frameSize = (float)std::max(frame.width(), frame.height());
    _input.resize(DETECTOR_SIZE * DETECTOR_SIZE * 3);
    sample(frame, frameCenter, frameSize, 0.0f, DETECTOR_SIZE, DETECTOR_SIZE, -1.0f, 1.0f, _input.data());
    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(_detector.interpreter, 0);
    if (TfLiteTensorCopyFromBuffer(input, _input.data(), _input.size() * sizeof(float)) != kTfLiteOk ||
        TfLiteInterpreterInvoke(_detector.interpreter) != kTfLiteOk) {
        return false;
    }
    const float* boxes = static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(_detector.interpreter, 0)));
    const float* logits = static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(_detector.interpreter, 1)));

    // each anchor's box (x, y of its center, width, height) and points, in the detector's square, 0 to 1
    auto decode = [&](int i, int coordinate) {
        const float value = boxes[i * DETECTOR_COORDINATES + coordinate] / DETECTOR_SIZE;
        const bool isSize = coordinate == 2 || coordinate == 3;
        return isSize ? value * _anchors[i][2 + (coordinate & 1)] : value + _anchors[i][coordinate & 1];
    };
    auto score = [&](int i) {
        return sigmoid(glm::clamp(logits[i], -DETECTOR_SCORE_CLIP, DETECTOR_SCORE_CLIP));
    };
    int best = -1;
    for (int i = 0; i < NUM_ANCHORS; i++) {
        if (score(i) >= DETECTOR_MIN_SCORE && (best < 0 || score(i) > score(best))) {
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }
    auto overlap = [&](int a, int b) {
        const float ax0 = decode(a, 0) - decode(a, 2) * 0.5f, ax1 = decode(a, 0) + decode(a, 2) * 0.5f;
        const float ay0 = decode(a, 1) - decode(a, 3) * 0.5f, ay1 = decode(a, 1) + decode(a, 3) * 0.5f;
        const float bx0 = decode(b, 0) - decode(b, 2) * 0.5f, bx1 = decode(b, 0) + decode(b, 2) * 0.5f;
        const float by0 = decode(b, 1) - decode(b, 3) * 0.5f, by1 = decode(b, 1) + decode(b, 3) * 0.5f;
        const float w = std::max(0.0f, std::min(ax1, bx1) - std::max(ax0, bx0));
        const float h = std::max(0.0f, std::min(ay1, by1) - std::max(ay0, by0));
        const float both = w * h;
        const float either = (ax1 - ax0) * (ay1 - ay0) + (bx1 - bx0) * (by1 - by0) - both;
        return either > 0.0f ? both / either : 0.0f;
    };
    // MediaPipe's weighted non-maximum suppression, for the best face only: the boxes over it, averaged by score
    float total = 0.0f;
    float merged[DETECTOR_COORDINATES] = {};
    for (int i = 0; i < NUM_ANCHORS; i++) {
        if (score(i) >= DETECTOR_MIN_SCORE && overlap(best, i) > DETECTOR_MIN_OVERLAP) {
            const float weight = score(i);
            total += weight;
            for (int k = 0; k < DETECTOR_COORDINATES; k++) {
                merged[k] += weight * decode(i, k);
            }
        }
    }
    if (total <= 0.0f) {
        return false;
    }
    for (int k = 0; k < DETECTOR_COORDINATES; k++) {
        merged[k] /= total;
    }

    // back into the frame's pixels; the crop is 1.5 times the box's longer side, turned so the eyes are level
    auto toFrame = [&](float x, float y) {
        return frameCenter + (glm::vec2(x, y) - 0.5f) * frameSize;
    };
    const glm::vec2 rightEye = toFrame(merged[4 + 2 * RIGHT_EYE_POINT], merged[5 + 2 * RIGHT_EYE_POINT]);
    const glm::vec2 leftEye = toFrame(merged[4 + 2 * LEFT_EYE_POINT], merged[5 + 2 * LEFT_EYE_POINT]);
    face.center = toFrame(merged[0], merged[1]);
    face.size = std::max(merged[2], merged[3]) * frameSize * FACE_SCALE;
    face.rotation = normalizeRadians(-std::atan2(-(leftEye.y - rightEye.y), leftEye.x - rightEye.x));
    return true;
}

bool FaceLandmarker::landmarks(const QImage& frame, const Square& face) {
    _input.resize(LANDMARKER_SIZE * LANDMARKER_SIZE * 3);
    sample(frame, face.center, face.size, face.rotation, LANDMARKER_SIZE, LANDMARKER_SIZE, 0.0f, 1.0f, _input.data());
    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(_landmarker.interpreter, 0);
    if (TfLiteTensorCopyFromBuffer(input, _input.data(), _input.size() * sizeof(float)) != kTfLiteOk ||
        TfLiteInterpreterInvoke(_landmarker.interpreter) != kTfLiteOk) {
        return false;
    }
    const float* points = static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(_landmarker.interpreter, 0)));
    const float presence = *static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(_landmarker.interpreter, 1)));
    if (sigmoid(presence) < MIN_FACE_PRESENCE) {
        return false;
    }
    // from the crop's pixels to the frame's
    const float c = std::cos(face.rotation);
    const float s = std::sin(face.rotation);
    for (int i = 0; i < NUM_LANDMARKS; i++) {
        const float x = points[3 * i] / LANDMARKER_SIZE - 0.5f;
        const float y = points[3 * i + 1] / LANDMARKER_SIZE - 0.5f;
        const float z = points[3 * i + 2] / LANDMARKER_SIZE;
        _landmarks[i] = glm::vec3(face.center.x + (c * x - s * y) * face.size, face.center.y + (s * x + c * y) * face.size,
                                  z * face.size);
    }
    return true;
}

FaceLandmarker::Square FaceLandmarker::squareAroundLandmarks() const {
    glm::vec2 low(_landmarks[0]);
    glm::vec2 high(_landmarks[0]);
    for (const glm::vec3& point : _landmarks) {
        low = glm::min(low, glm::vec2(point));
        high = glm::max(high, glm::vec2(point));
    }
    const glm::vec3& rightEye = _landmarks[RIGHT_EYE_OUTER];
    const glm::vec3& leftEye = _landmarks[LEFT_EYE_OUTER];
    Square square;
    square.center = (low + high) * 0.5f;
    square.size = std::max(high.x - low.x, high.y - low.y) * FACE_SCALE;
    square.rotation = normalizeRadians(-std::atan2(-(leftEye.y - rightEye.y), leftEye.x - rightEye.x));
    return square;
}

bool FaceLandmarker::blendshapes(Result& result) {
    _input.resize(NUM_BLENDSHAPE_LANDMARKS * 2);
    for (int i = 0; i < NUM_BLENDSHAPE_LANDMARKS; i++) {
        _input[2 * i] = _landmarks[BLENDSHAPE_LANDMARKS[i]].x;
        _input[2 * i + 1] = _landmarks[BLENDSHAPE_LANDMARKS[i]].y;
    }
    TfLiteTensor* input = TfLiteInterpreterGetInputTensor(_blendshaper.interpreter, 0);
    if (TfLiteTensorCopyFromBuffer(input, _input.data(), _input.size() * sizeof(float)) != kTfLiteOk ||
        TfLiteInterpreterInvoke(_blendshaper.interpreter) != kTfLiteOk) {
        return false;
    }
    const TfLiteTensor* output = TfLiteInterpreterGetOutputTensor(_blendshaper.interpreter, 0);
    return TfLiteTensorCopyToBuffer(output, result.blendshapes.data(), NUM_BLENDSHAPES * sizeof(float)) == kTfLiteOk;
}

// The head's rotation: the canonical face's weighted points fitted to the landmarks (the weighted orthogonal Procrustes
// problem), in the camera's frame with y up and z toward the camera. Scale and position fall out; perspective is left
// out, which shifts the angles a little when the face is far off the frame's center.
glm::quat FaceLandmarker::headRotation() const {
    glm::vec3 canonicalCenter(0.0f);
    glm::vec3 seenCenter(0.0f);
    float total = 0.0f;
    std::vector<glm::vec3> seen(_fitLandmarks.size());
    for (size_t i = 0; i < _fitLandmarks.size(); i++) {
        const glm::vec3& point = _landmarks[_fitLandmarks[i]];
        seen[i] = glm::vec3(point.x, -point.y, -point.z);
        canonicalCenter += _fitWeights[i] * _fitCanonical[i];
        seenCenter += _fitWeights[i] * seen[i];
        total += _fitWeights[i];
    }
    canonicalCenter /= total;
    seenCenter /= total;
    glm::mat3 covariance(0.0f);
    for (size_t i = 0; i < _fitLandmarks.size(); i++) {
        covariance += _fitWeights[i] * glm::outerProduct(seen[i] - seenCenter, _fitCanonical[i] - canonicalCenter);
    }
    if (std::abs(glm::determinant(covariance)) < 1.0e-12f) {
        return glm::quat();
    }
    return glm::normalize(glm::quat_cast(nearestRotation(covariance)));
}

FaceLandmarker::Result FaceLandmarker::process(const QImage& image) {
    Result result;
    if (!_landmarker.interpreter || image.isNull()) {
        return result;
    }
    const QImage frame = image.format() == QImage::Format_RGB32 || image.format() == QImage::Format_ARGB32
                             ? image
                             : image.convertToFormat(QImage::Format_RGB32);

    // last frame's face, or a new one from the detector
    if (!_tracking && !detect(frame, _face)) {
        return result;
    }
    if (!landmarks(frame, _face)) {
        _tracking = false;
        return result;
    }
    _tracking = true;
    _face = squareAroundLandmarks();
    result.face = blendshapes(result);
    result.rotation = headRotation();
    return result;
}
