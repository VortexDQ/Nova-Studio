#include "nova/media/MediaRecorder.h"
#include "nova/core/Logger.h"

#include <QAudioInput>
#include <QCamera>
#include <QDateTime>
#include <QDir>
#include <QMediaCaptureSession>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QScreenCapture>
#include <QUrl>

namespace nova::media {

namespace {
constexpr const char* kModule = "media.MediaRecorder";
} // namespace

MediaRecorder::MediaRecorder(QObject* parent) : QObject(parent) {}

MediaRecorder::~MediaRecorder() {
    if (recording_) stopRecording();
    resetSession();
}

void MediaRecorder::resetSession() {
    if (recorder_ && recording_) recorder_->stop();
    recorder_.reset();
    camera_.reset();
    audioInput_.reset();
    screenCapture_.reset();
    session_.reset();
    recording_ = false;
}

bool MediaRecorder::setupRecorder(const QString& outputPath) {
    session_ = std::make_unique<QMediaCaptureSession>();
    recorder_ = std::make_unique<QMediaRecorder>();
    recorder_->setOutputLocation(QUrl::fromLocalFile(outputPath));
    recorder_->setMediaFormat(QMediaFormat::MPEG4);
    recorder_->setQuality(QMediaRecorder::HighQuality);
    session_->setRecorder(recorder_.get());

    connect(recorder_.get(), &QMediaRecorder::recorderStateChanged, this,
            [this](QMediaRecorder::RecorderState state) {
                if (state == QMediaRecorder::RecordingState) {
                    emit statusChanged(QStringLiteral("Recording…"));
                } else if (state == QMediaRecorder::StoppedState && recording_) {
                    recording_ = false;
                    emit recordingFinished(outputPath_, true, {});
                }
            });
    connect(recorder_.get(), &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error, const QString& errorString) {
                recording_ = false;
                emit recordingFinished(outputPath_, false, errorString);
            });
    return true;
}

QString MediaRecorder::startRecording(RecordMode mode, const QString& outputDirectory) {
    if (recording_) return {};

    QDir().mkpath(outputDirectory);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString fileName;
    switch (mode) {
    case RecordMode::AudioOnly:
        fileName = QStringLiteral("voice_") + stamp + QStringLiteral(".m4a");
        break;
    default:
        fileName = QStringLiteral("recording_") + stamp + QStringLiteral(".mp4");
        break;
    }
    outputPath_ = QDir(outputDirectory).filePath(fileName);

    resetSession();
    if (!setupRecorder(outputPath_)) return {};

    audioInput_ = std::make_unique<QAudioInput>();
    session_->setAudioInput(audioInput_.get());

    switch (mode) {
    case RecordMode::Screen:
    case RecordMode::ScreenAndCamera:
        screenCapture_ = std::make_unique<QScreenCapture>();
        session_->setScreenCapture(screenCapture_.get());
        if (mode == RecordMode::ScreenAndCamera) {
            camera_ = std::make_unique<QCamera>();
            session_->setCamera(camera_.get());
            emit statusChanged(QStringLiteral("Recording screen + camera…"));
        } else {
            emit statusChanged(QStringLiteral("Recording screen…"));
        }
        break;
    case RecordMode::Camera:
        camera_ = std::make_unique<QCamera>();
        session_->setCamera(camera_.get());
        emit statusChanged(QStringLiteral("Recording camera…"));
        break;
    case RecordMode::AudioOnly:
        emit statusChanged(QStringLiteral("Recording microphone…"));
        break;
    }

    recorder_->record();
    recording_ = true;
    NOVA_LOG_INFO(kModule, "Started recording: " + outputPath_.toStdString());
    return outputPath_;
}

void MediaRecorder::stopRecording() {
    if (!recorder_ || !recording_) return;
    emit statusChanged(QStringLiteral("Stopping recording…"));
    recorder_->stop();
}

} // namespace nova::media
