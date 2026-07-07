#pragma once

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMainWindow>
#include <QTimer>
#include <memory>
#include <optional>
#include <string>

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
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onExportVideo();
    void onExportProjectCopy();
    void onShowVersionHistory();
    void onRecentProjectTriggered();
    void onImportMedia();
    void onImportMediaToFolder(const QString& folder);
    void onMediaItemClicked();
    void onMediaItemActivated();
    void onClipSelected(const QString& clipId, const QString& mediaPath);
    void onMediaSearchChanged();
    void onMediaFolderChanged();
    void onTimelineSelectionChanged(int index);
    void onAddTimeline();
    void onPlayPauseClicked();
    void onPlaybackTick();
    void onTimelineSeek(double seconds);
    void onSplitAtPlayhead();
    void onDeleteSelectedClip();
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
    void captureSessionState();
    bool maybeSaveBeforeDiscard();
    bool saveProjectToPath(const QString& path, bool addToRecents);
    void applyProjectToUi();
    void restoreProjectSession();
    void refreshMediaList();
    void refreshMediaMetadataFromFiles();
    void showMediaMetadata(const QString& path);
    void showClipMetadata(const nova::timeline::Clip& clip);
    bool previewMediaFile(const QString& path, double seekSeconds);
    bool previewImageFile(const QString& path);
    void addMediaToTimeline(const QString& path);
    void syncAudioToCurrentTime();
    void updatePreviewEffects();
    void updateTitleOverlay();
    nova::timeline::Timeline* activeTimeline();
    nova::timeline::Track* findOrAddTitleTrack(nova::timeline::Timeline* timeline);
    const nova::timeline::Clip* findVideoClipAt(double seconds) const;
    const nova::timeline::Clip* findTitleClipAt(double seconds) const;
    const nova::timeline::Clip* findClipById(const std::string& id) const;
    void importMediaFile(const QString& path, const QString& folder);
    QString ensureStockAsset(const QString& assetId);
    QString formatBytes(int64_t bytes) const;

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
    QLabel* mediaMetaLabel_ = nullptr;
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
    std::string selectedClipId_;

    QElapsedTimer playbackClock_;
    double playbackStartSeconds_ = 0.0;
    std::optional<nova::media::VideoFrame> pendingFrame_;
    double masterClockSeconds() const;
};

} // namespace nova::ui
