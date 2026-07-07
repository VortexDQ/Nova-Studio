#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>
#include <memory>
#include <optional>

#include "nova/media/Decoder.h"
#include "nova/project/Project.h"

class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QAudioOutput;
class QMediaPlayer;

namespace nova::renderer { class VideoPreviewWidget; }

namespace nova::ui {

class TimelineWidget;
class SidebarPanel;
class PreviewOverlay;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openProjectPath(const QString& path);
    bool newProjectAtPath(const QString& path, double fps, int width, int height);
    bool newProjectFromTemplate(const QString& templatePathOrId);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onExportProjectCopy();
    void onShowVersionHistory();
    void onRecentProjectTriggered();
    void onImportMedia();
    void onImportMediaToFolder(const QString& folder);
    void onMediaItemActivated();
    void onMediaSearchChanged();
    void onMediaFolderChanged();
    void onTimelineSelectionChanged(int index);
    void onAddTimeline();
    void onPlayPauseClicked();
    void onPlaybackTick();
    void onTimelineSeek(double seconds);
    void onSplitAtPlayhead();
    void onExtractAudio();
    void onVolumeChanged(int value);
    void onAutosaveTick();
    void onTemplateActivated(const QString& templatePath);
    void onTextPresetActivated(const QString& presetId, const QString& defaultText);
    void onTransitionActivated(const QString& transitionId);
    void onLibraryAssetActivated(const QString& assetId);

private:
    void buildDockPanels();
    void buildMenus();
    void applyTheme();
    void markDirty();
    bool maybeSaveBeforeDiscard();
    bool saveProjectToPath(const QString& path, bool addToRecents);
    void applyProjectToUi();
    void refreshMediaList();
    void loadMediaIntoPreview(const QString& path);
    void addClipToTimeline(const QString& path);
    void syncAudioToCurrentTime();
    void updatePreviewEffects();
    void updateTitleOverlay();
    nova::timeline::Timeline* activeTimeline();
    nova::timeline::Track* findOrAddTitleTrack(nova::timeline::Timeline* timeline);
    const nova::timeline::Clip* findVideoClipAt(double seconds) const;
    const nova::timeline::Clip* findTitleClipAt(double seconds) const;
    void importMediaFile(const QString& path, const QString& folder);
    QString ensureStockAsset(const QString& assetId);

    nova::renderer::VideoPreviewWidget* preview_ = nullptr;
    PreviewOverlay* previewOverlay_ = nullptr;
    TimelineWidget* timelineWidget_ = nullptr;
    SidebarPanel* sidebar_ = nullptr;
    QComboBox* timelineSelector_ = nullptr;
    QSlider* brightnessSlider_ = nullptr;
    QSlider* contrastSlider_ = nullptr;
    QSlider* saturationSlider_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QLabel* projectMetaLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* splitButton_ = nullptr;
    QPushButton* extractAudioButton_ = nullptr;

    std::unique_ptr<nova::project::Project> project_;
    std::unique_ptr<nova::media::Decoder> decoder_;
    QMediaPlayer* audioPlayer_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    QTimer playbackTimer_;
    QTimer autosaveTimer_;
    bool playing_ = false;
    double currentTimeSeconds_ = 0.0;
    QString currentMediaPath_;
    QString pendingImportFolder_;

    // Wall-clock master clock for A/V sync. Video frames are presented when
    // the master clock reaches their PTS, keeping video locked to real time
    // (and, when present, to the audio player) instead of drifting by decode
    // latency. `pendingFrame_` holds a decoded-ahead frame not yet due.
    QElapsedTimer playbackClock_;
    double playbackStartSeconds_ = 0.0;   // media time when playback (re)started
    std::optional<nova::media::VideoFrame> pendingFrame_;
    double masterClockSeconds() const;
};

} // namespace nova::ui
