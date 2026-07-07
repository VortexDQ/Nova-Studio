#pragma once

#include <QMainWindow>
#include <QTimer>
#include <memory>

#include "nova/media/Decoder.h"
#include "nova/project/Project.h"

class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QAudioOutput;
class QMediaPlayer;
class QListWidget;

namespace nova::renderer { class VideoPreviewWidget; }

namespace nova::ui {

class TimelineWidget;
class SidebarPanel;

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

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onExportProjectCopy();
    void onShowVersionHistory();
    void onRecentProjectTriggered();
    void onImportMedia();
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
    nova::timeline::Timeline* activeTimeline();

    nova::renderer::VideoPreviewWidget* preview_ = nullptr;
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
};

} // namespace nova::ui
