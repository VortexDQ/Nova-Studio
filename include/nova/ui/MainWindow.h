#pragma once

#include <QMainWindow>
#include <QTimer>
#include <memory>

#include "nova/media/Decoder.h"
#include "nova/timeline/Timeline.h"

class QListWidget;
class QSlider;
class QLabel;
class QPushButton;
class QAudioOutput;
class QMediaPlayer;

namespace nova::renderer { class VideoPreviewWidget; }

namespace nova::ui {

class TimelineWidget;

// Application shell: dockable Media Bin / Timeline / Inspector panels around
// a central GPU preview. Wires a real decode -> render loop (via QTimer at
// the source frame rate) so importing a clip and pressing Play does an
// actual end-to-end playback, not a mock.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportMedia();
    void onMediaItemActivated();
    void onPlayPauseClicked();
    void onPlaybackTick();
    void onTimelineSeek(double seconds);
    void onSplitAtPlayhead();
    void onExtractAudio();
    void onVolumeChanged(int value);

private:
    void buildDockPanels();
    void buildMenus();
    void applyTheme();
    void loadMediaIntoPreview(const QString& path);
    void addClipToTimeline(const QString& path);
    void syncAudioToCurrentTime();

    nova::renderer::VideoPreviewWidget* preview_ = nullptr;
    TimelineWidget* timelineWidget_ = nullptr;
    QListWidget* mediaBin_ = nullptr;
    QSlider* brightnessSlider_ = nullptr;
    QSlider* contrastSlider_ = nullptr;
    QSlider* saturationSlider_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* splitButton_ = nullptr;
    QPushButton* extractAudioButton_ = nullptr;

    std::unique_ptr<nova::media::Decoder> decoder_;
    std::unique_ptr<nova::timeline::Timeline> timeline_;
    QMediaPlayer* audioPlayer_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    QTimer playbackTimer_;
    bool playing_ = false;
    double currentTimeSeconds_ = 0.0;
    QString currentMediaPath_;
};

} // namespace nova::ui
