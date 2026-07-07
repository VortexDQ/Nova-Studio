#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <memory>

#include "nova/media/Decoder.h"

namespace nova::renderer {

// Real-time GPU preview surface. Uploads decoded RGBA8 frames as a texture
// and runs a single GLSL fragment shader effect over them each paint. This
// is the "one working effect, end to end" slice: decode -> GPU upload ->
// shader -> present, with parameters exposed to the Inspector panel.
//
// The effect stack in the full design (docs/ROADMAP.md) generalizes this to
// a chain of shader passes rendered into ping-ponged framebuffers; a single
// pass is the correct minimal version of that architecture.
class VideoPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget* parent = nullptr);
    ~VideoPreviewWidget() override;

    // Uploads a newly decoded frame to be displayed on next paint.
    void setFrame(const nova::media::VideoFrame& frame);

public slots:
    void setBrightness(int value);   // -100..100, UI range
    void setContrast(int value);     // -100..100, UI range
    void setSaturation(int value);   // 0..200, UI range (100 = neutral)
    void setClipOpacity(float opacity);  // 0..1
    void setDipMix(float mix, bool white);  // mix 0..1, white vs black dip

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLTexture> texture_;
    unsigned int vbo_ = 0;
    unsigned int vao_ = 0;

    float brightness_ = 0.0f;   // -1..1
    float contrast_ = 1.0f;     // 0..2
    float saturation_ = 1.0f;   // 0..2
    float clipOpacity_ = 1.0f;
    float dipMix_ = 0.0f;
    float dipWhite_ = 0.0f;

    int pendingWidth_ = 0;
    int pendingHeight_ = 0;
    bool hasFrame_ = false;
};

} // namespace nova::renderer
