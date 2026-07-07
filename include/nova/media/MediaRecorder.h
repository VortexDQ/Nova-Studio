#pragma once

#include <QObject>
#include <QString>
#include <memory>

class QMediaCaptureSession;
class QMediaRecorder;
class QCamera;
class QAudioInput;
class QScreenCapture;

namespace nova::media {

enum class RecordMode { Screen, Camera, ScreenAndCamera, AudioOnly };

class MediaRecorder : public QObject {
    Q_OBJECT

public:
    explicit MediaRecorder(QObject* parent = nullptr);
    ~MediaRecorder() override;

    bool isRecording() const { return recording_; }
    QString startRecording(RecordMode mode, const QString& outputDirectory);
    void stopRecording();

signals:
    void recordingFinished(const QString& path, bool success, const QString& errorMessage);
    void statusChanged(const QString& status);

private:
    void resetSession();
    bool setupRecorder(const QString& outputPath);

    std::unique_ptr<QMediaCaptureSession> session_;
    std::unique_ptr<QMediaRecorder> recorder_;
    std::unique_ptr<QCamera> camera_;
    std::unique_ptr<QAudioInput> audioInput_;
    std::unique_ptr<QScreenCapture> screenCapture_;
    QString outputPath_;
    bool recording_ = false;
};

} // namespace nova::media
