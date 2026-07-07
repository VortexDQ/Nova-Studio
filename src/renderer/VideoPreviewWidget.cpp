#include "nova/renderer/VideoPreviewWidget.h"
#include "nova/core/Logger.h"

#include <algorithm>

namespace nova::renderer {

namespace {
constexpr const char* kModule = "renderer.VideoPreviewWidget";

// Fullscreen-quad vertex shader: two triangles covering clip space, with UVs
// flipped vertically to match the top-down RGBA rows produced by sws_scale.
constexpr const char* kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

// The "one working effect": brightness / contrast / saturation, applied in
// linear steps that mirror how a real color-correction node would be
// structured (brightness offset -> contrast around mid-gray -> saturation
// via luma mix). Real ACES/HDR-aware grading would do this in scene-linear
// space with a proper luminance weighting per color space; Rec.709 luma
// weights are used here as the correct default for SDR.
constexpr const char* kFragmentShader = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uBrightness; // -1..1
uniform float uContrast;   // 0..2
uniform float uSaturation; // 0..2
uniform float uOpacity;    // 0..1
uniform float uDipMix;     // 0 = show video, 1 = full dip
uniform float uDipWhite;   // 0 = black dip, 1 = white dip
uniform float uRotation;   // radians
uniform float uChromaMix;    // 0 = off, 1 = full key
uniform vec3 uChromaColor;   // key color (green screen)

void main() {
    vec2 uv = vUV - 0.5;
    float c = cos(uRotation);
    float s = sin(uRotation);
    uv = vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y) + 0.5;
    vec3 color = texture(uTexture, uv).rgb;

    color += uBrightness;
    color = (color - 0.5) * uContrast + 0.5;

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, uSaturation);
    color = clamp(color, 0.0, 1.0);

    float chromaDist = distance(color, uChromaColor);
  if (uChromaMix > 0.0 && chromaDist < 0.35 * uChromaMix) {
        color = mix(color, vec3(0.0), (0.35 * uChromaMix - chromaDist) / max(0.001, 0.35 * uChromaMix));
    }

    vec3 dipColor = mix(vec3(0.0), vec3(1.0), uDipWhite);
    color = mix(dipColor, color, (1.0 - uDipMix) * uOpacity);

    FragColor = vec4(color, 1.0);
}
)GLSL";

} // namespace

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent) : QOpenGLWidget(parent) {}

VideoPreviewWidget::~VideoPreviewWidget() {
    makeCurrent();
    texture_.reset();
    program_.reset();
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    doneCurrent();
}

void VideoPreviewWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.08f, 0.08f, 0.09f, 1.0f);

    program_ = std::make_unique<QOpenGLShaderProgram>();
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)) {
        NOVA_LOG_ERROR(kModule, "Vertex shader compile failed: " + program_->log().toStdString());
    }
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)) {
        NOVA_LOG_ERROR(kModule, "Fragment shader compile failed: " + program_->log().toStdString());
    }
    if (!program_->link()) {
        NOVA_LOG_ERROR(kModule, "Shader program link failed: " + program_->log().toStdString());
    }

    // Two triangles, position (x,y) + UV, forming a fullscreen quad.
    static const float vertices[] = {
        // pos        // uv
        -1.f, -1.f,   0.f, 1.f,
         1.f, -1.f,   1.f, 1.f,
         1.f,  1.f,   1.f, 0.f,

        -1.f, -1.f,   0.f, 1.f,
         1.f,  1.f,   1.f, 0.f,
        -1.f,  1.f,   0.f, 0.f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void VideoPreviewWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void VideoPreviewWidget::setFrame(const nova::media::VideoFrame& frame) {
    makeCurrent();
    if (!texture_ || texture_->width() != frame.width || texture_->height() != frame.height) {
        texture_ = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        texture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
        texture_->setSize(frame.width, frame.height);
        texture_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    }
    texture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, frame.rgba.data());
    doneCurrent();

    hasFrame_ = true;
    update();
}

void VideoPreviewWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (!hasFrame_ || !texture_ || !program_) return;

    program_->bind();
    texture_->bind(0);
    program_->setUniformValue("uTexture", 0);
    program_->setUniformValue("uBrightness", brightness_);
    program_->setUniformValue("uContrast", contrast_);
    program_->setUniformValue("uSaturation", saturation_);
    program_->setUniformValue("uOpacity", clipOpacity_);
    program_->setUniformValue("uDipMix", dipMix_);
    program_->setUniformValue("uDipWhite", dipWhite_);
    program_->setUniformValue("uRotation", rotationRadians_);
    program_->setUniformValue("uChromaMix", chromaMix_);
    program_->setUniformValue("uChromaColor", chromaColor_[0], chromaColor_[1], chromaColor_[2]);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    texture_->release();
    program_->release();
}

void VideoPreviewWidget::setBrightness(int value) {
    brightness_ = std::clamp(value, -100, 100) / 100.0f;
    update();
}

void VideoPreviewWidget::setContrast(int value) {
    contrast_ = 1.0f + std::clamp(value, -100, 100) / 100.0f;
    update();
}

void VideoPreviewWidget::setSaturation(int value) {
    saturation_ = std::clamp(value, 0, 200) / 100.0f;
    update();
}

void VideoPreviewWidget::setClipOpacity(float opacity) {
    clipOpacity_ = std::clamp(opacity, 0.0f, 1.0f);
    update();
}

void VideoPreviewWidget::setDipMix(float mix, bool white) {
    dipMix_ = std::clamp(mix, 0.0f, 1.0f);
    dipWhite_ = white ? 1.0f : 0.0f;
    update();
}

void VideoPreviewWidget::setRotationDegrees(float degrees) {
    rotationRadians_ = static_cast<float>(degrees * 3.141592653589793 / 180.0);
    update();
}

void VideoPreviewWidget::setChromaKey(float mix, float greenR, float greenG, float greenB) {
    chromaMix_ = std::clamp(mix, 0.0f, 1.0f);
    chromaColor_[0] = greenR;
    chromaColor_[1] = greenG;
    chromaColor_[2] = greenB;
    update();
}

} // namespace nova::renderer
