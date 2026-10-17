//
//  AndroidWebView.cpp
//  interface/src/android
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AndroidWebView.h"

#if defined(Q_OS_ANDROID)

#include <mutex>
#include <unordered_map>

#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLExtraFunctions>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleTextureNode>

static const char* JAVA_CLASS = "io/highfidelity/hifiinterface/web/DarkrideWebView";

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

// The views by id, for frames reported from Java, which can outlive a view
static std::mutex viewsMutex;
static std::unordered_map<jlong, QPointer<AndroidWebView>> views;
static std::atomic<jlong> nextId { 1 };

namespace {

// Draws the WebView's frame (an external OES texture, with SurfaceTexture's transform) into a 2D texture, the page's
// top in the texture's first row as the scene graph expects
const char* VERTEX_SHADER = R"(#version 300 es
uniform mat4 texMatrix;
out vec2 texCoord;
void main() {
    const vec2 CORNERS[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    vec2 corner = CORNERS[gl_VertexID];
    texCoord = (texMatrix * vec4(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5, 0.0, 1.0)).xy;
    gl_Position = vec4(corner, 0.0, 1.0);
}
)";

const char* FRAGMENT_SHADER = R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
uniform samplerExternalOES frame;
in vec2 texCoord;
out vec4 color;
void main() {
    color = texture(frame, texCoord);
}
)";

GLuint compileShader(QOpenGLExtraFunctions* gl, GLenum type, const char* source) {
    GLuint shader = gl->glCreateShader(type);
    gl->glShaderSource(shader, 1, &source, nullptr);
    gl->glCompileShader(shader);
    GLint ok = 0;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        gl->glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qWarning() << "AndroidWebView: shader didn't compile:" << log;
    }
    return shader;
}

// Holds the GL resources; its child draws the texture, once there's a frame in it
class AndroidWebViewNode : public QSGNode {
public:
    ~AndroidWebViewNode() override {
        // on the render thread, with its context current
        if (surfaceTexture.isValid()) {
            surfaceTexture.callMethod<void>("release");
        }
        auto context = QOpenGLContext::currentContext();
        if (!context) {
            return;
        }
        auto gl = context->extraFunctions();
        gl->glDeleteFramebuffers(1, &framebuffer);
        gl->glDeleteTextures(1, &texture);
        gl->glDeleteTextures(1, &oesTexture);
        gl->glDeleteProgram(program);
    }

    // The SurfaceTexture, given to the WebView, and the texture its frames are copied into, at this size
    void prepare(QQuickWindow* window, const QAndroidJniObject& javaView, const QSize& newSize) {
        auto gl = QOpenGLContext::currentContext()->extraFunctions();

        if (!program) {
            program = gl->glCreateProgram();
            GLuint vertex = compileShader(gl, GL_VERTEX_SHADER, VERTEX_SHADER);
            GLuint fragment = compileShader(gl, GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
            gl->glAttachShader(program, vertex);
            gl->glAttachShader(program, fragment);
            gl->glLinkProgram(program);
            gl->glDeleteShader(vertex);
            gl->glDeleteShader(fragment);
            texMatrixLocation = gl->glGetUniformLocation(program, "texMatrix");
            frameLocation = gl->glGetUniformLocation(program, "frame");
        }

        if (!oesTexture) {
            gl->glGenTextures(1, &oesTexture);
            gl->glBindTexture(GL_TEXTURE_EXTERNAL_OES, oesTexture);
            gl->glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl->glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            // attached to this context: frames are taken (updateTexImage) on this thread
            surfaceTexture = QAndroidJniObject("android/graphics/SurfaceTexture", "(I)V", (jint)oesTexture);
            javaView.callMethod<void>("setSurfaceTexture", "(Landroid/graphics/SurfaceTexture;)V", surfaceTexture.object());
        }

        if (newSize != size) {
            size = newSize;
            surfaceTexture.callMethod<void>("setDefaultBufferSize", "(II)V", (jint)size.width(), (jint)size.height());
            if (!texture) {
                gl->glGenTextures(1, &texture);
                gl->glGenFramebuffers(1, &framebuffer);
            }
            gl->glBindTexture(GL_TEXTURE_2D, texture);
            gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl->glBindTexture(GL_TEXTURE_2D, 0);
            if (textureNode) {
                removeChildNode(textureNode);
                delete textureNode;
                textureNode = nullptr;
            }
            hasFrame = false;
        }
    }

    // The WebView's latest frame into the texture
    void copyFrame(QQuickWindow* window) {
        auto gl = QOpenGLContext::currentContext()->extraFunctions();
        surfaceTexture.callMethod<void>("updateTexImage");

        QAndroidJniEnvironment env;
        jfloatArray matrixArray = env->NewFloatArray(16);
        surfaceTexture.callMethod<void>("getTransformMatrix", "([F)V", matrixArray);
        GLfloat matrix[16];
        env->GetFloatArrayRegion(matrixArray, 0, 16, matrix);
        env->DeleteLocalRef(matrixArray);

        GLint previousFramebuffer = 0;
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        gl->glViewport(0, 0, size.width(), size.height());
        gl->glDisable(GL_BLEND);
        gl->glDisable(GL_DEPTH_TEST);
        gl->glDisable(GL_SCISSOR_TEST);
        gl->glDisable(GL_STENCIL_TEST);
        gl->glDisable(GL_CULL_FACE);
        gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl->glBindVertexArray(0);
        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
        gl->glUseProgram(program);
        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_EXTERNAL_OES, oesTexture);
        gl->glUniform1i(frameLocation, 0);
        gl->glUniformMatrix4fv(texMatrixLocation, 1, GL_FALSE, matrix);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl->glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
        window->resetOpenGLState();
        hasFrame = true;
        if (!textureNode) {
            textureNode = new QSGSimpleTextureNode();
            textureNode->setTexture(window->createTextureFromId(texture, size, QQuickWindow::TextureHasAlphaChannel));
            textureNode->setOwnsTexture(true);
            appendChildNode(textureNode);
        }
        textureNode->markDirty(QSGNode::DirtyMaterial);
    }

    QAndroidJniObject surfaceTexture;
    GLuint oesTexture { 0 };
    GLuint texture { 0 };
    GLuint framebuffer { 0 };
    GLuint program { 0 };
    GLint texMatrixLocation { -1 };
    GLint frameLocation { -1 };
    QSize size;
    bool hasFrame { false };
    QSGSimpleTextureNode* textureNode { nullptr };  // a child, deleted with this
};

}  // namespace

AndroidWebView::AndroidWebView(QQuickItem* parent) : QQuickItem(parent), _id(nextId++) {
    setFlag(ItemHasContents, true);
    {
        std::lock_guard<std::mutex> lock(viewsMutex);
        views[_id] = this;
    }
    QSize size = pixelSize();
    _javaView = QAndroidJniObject(JAVA_CLASS, "(JII)V", _id, (jint)size.width(), (jint)size.height());
}

AndroidWebView::~AndroidWebView() {
    {
        std::lock_guard<std::mutex> lock(viewsMutex);
        views.erase(_id);
    }
    if (_javaView.isValid()) {
        _javaView.callMethod<void>("destroy");
    }
}

void AndroidWebView::setUrl(const QString& url) {
    if (url == _url) {
        return;
    }
    _url = url;
    if (_javaView.isValid()) {
        _javaView.callMethod<void>("loadUrl", "(Ljava/lang/String;)V", QAndroidJniObject::fromString(url).object<jstring>());
    }
    emit urlChanged();
}

QSize AndroidWebView::pixelSize() const {
    return QSize(std::max(1, (int)width()), std::max(1, (int)height()));
}

void AndroidWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size() && _javaView.isValid()) {
        QSize size = pixelSize();
        _javaView.callMethod<void>("resize", "(II)V", (jint)size.width(), (jint)size.height());
        update();
    }
}

void AndroidWebView::frameAvailable(jlong id) {
    std::lock_guard<std::mutex> lock(viewsMutex);
    auto found = views.find(id);
    if (found != views.end() && found->second) {
        found->second->_frameAvailable = true;
        QMetaObject::invokeMethod(found->second.data(), "update", Qt::QueuedConnection);
    }
}

QSGNode* AndroidWebView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto node = static_cast<AndroidWebViewNode*>(oldNode);
    if (!_javaView.isValid() || !window() || !QOpenGLContext::currentContext()) {
        delete node;
        return nullptr;
    }
    if (!node) {
        node = new AndroidWebViewNode();
    }
    node->prepare(window(), _javaView, pixelSize());
    if (_frameAvailable.exchange(false)) {
        node->copyFrame(window());
    }
    // until the page's first frame, the item shows what's under it (Web3DSurface's placeholder)
    if (node->textureNode) {
        node->textureNode->setRect(boundingRect());
    }
    return node;
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_web_DarkrideWebView_nativeFrameAvailable(JNIEnv*, jclass, jlong id) {
    AndroidWebView::frameAvailable(id);
}

#endif
