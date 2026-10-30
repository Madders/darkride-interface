//
//  FaceLandmarker.h
//  plugins/face-tracking-plugin/src
//
//  MediaPipe's face landmarker (face_landmarker.task: a face detector, 478 face landmarks and ARKit's 52 blendshapes) on
//  TensorFlow Lite, for one face. The detector finds the face; after that each frame's landmarks place the next frame's
//  crop, until the face is lost. The head's rotation comes from the landmarks fitted to the bundle's canonical face.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#pragma once

#include <array>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtGui/QImage>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct TfLiteModel;
struct TfLiteInterpreter;
struct TfLiteDelegate;

class FaceLandmarker {
public:
    static const int NUM_BLENDSHAPES = 52;
    static const int NUM_LANDMARKS = 478;

    // the model's blendshapes in its output order: "_neutral", then ARKit's names
    static const char* const BLENDSHAPE_NAMES[NUM_BLENDSHAPES];

    struct Result {
        bool face { false };
        std::array<float, NUM_BLENDSHAPES> blendshapes {};  // 0 to 1
        // the face's rotation from the canonical face, in the camera's frame: x right, y up, z toward the camera
        glm::quat rotation;
    };

    FaceLandmarker() = default;
    FaceLandmarker(const FaceLandmarker&) = delete;
    FaceLandmarker& operator=(const FaceLandmarker&) = delete;
    ~FaceLandmarker();

    // false, with error saying why, unless the bundle's three models and its canonical face load
    bool load(const QByteArray& task, int threads, QString& error);

    // one camera frame (any QImage format; RGB32 needs no conversion), as the camera sees it: not mirrored
    Result process(const QImage& frame);

    // forget the face: the next frame looks for one again
    void reset() { _tracking = false; }

private:
    // a square in the frame, in pixels, turned clockwise (the frame's y points down) by rotation, in radians
    struct Square {
        glm::vec2 center;
        float size { 0.0f };
        float rotation { 0.0f };
    };

    struct Model {
        QByteArray data;
        TfLiteModel* model { nullptr };
        TfLiteInterpreter* interpreter { nullptr };
        TfLiteDelegate* delegate { nullptr };
    };

    bool loadModel(Model& model, const QByteArray& data, int threads, QString& error);
    void freeModel(Model& model);
    bool loadGeometry(const QByteArray& metadata, QString& error);

    bool detect(const QImage& frame, Square& face);
    bool landmarks(const QImage& frame, const Square& face);
    bool blendshapes(Result& result);
    glm::quat headRotation() const;
    Square squareAroundLandmarks() const;

    Model _detector;
    Model _landmarker;
    Model _blendshaper;
    std::vector<glm::vec4> _anchors;  // the detector's: x and y of each anchor's center, then its width and height

    // the canonical face's points (cm; y up, z toward the viewer) and weights for the head's fit, by landmark
    std::vector<int> _fitLandmarks;
    std::vector<float> _fitWeights;
    std::vector<glm::vec3> _fitCanonical;

    std::array<glm::vec3, NUM_LANDMARKS> _landmarks;  // the frame's pixels; z in pixels, smaller is nearer the camera
    std::vector<float> _input;
    bool _tracking { false };
    Square _face;
};
