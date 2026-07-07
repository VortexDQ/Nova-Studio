#include "nova/ui/MainWindow.h"
#include "nova/ui/TimelineWidget.h"
#include "nova/ui/SidebarPanel.h"
#include "nova/renderer/VideoPreviewWidget.h"
#include "nova/core/Logger.h"
#include "nova/media/AudioExtractor.h"
#include "nova/project/ProjectIO.h"
#include "nova/project/ProjectStore.h"

#include <QAction>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace nova::ui {

namespace {
constexpr const char* kModule = "ui.MainWindow";
constexpr int kAutosaveIntervalMs = 120000;
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Nova Studio");
    resize(1400, 900);

    decoder_ = std::make_unique<nova::media::Decoder>();
    project_ = nova::project::Project::createBlank("Untitled", 30.0, 1920, 1080);

    preview_ = new nova::renderer::VideoPreviewWidget(this);
    setCentralWidget(preview_);

    audioOutput_ = new QAudioOutput(this);
    audioOutput_->setVolume(0.85f);
    audioPlayer_ = new QMediaPlayer(this);
    audioPlayer_->setAudioOutput(audioOutput_);

    applyTheme();
    buildDockPanels();
    buildMenus();
    applyProjectToUi();

    connect(&playbackTimer_, &QTimer::timeout, this, &MainWindow::onPlaybackTick);
    connect(&autosaveTimer_, &QTimer::timeout, this, &MainWindow::onAutosaveTick);
    autosaveTimer_.start(kAutosaveIntervalMs);
}

MainWindow::~MainWindow() = default;

nova::timeline::Timeline* MainWindow::activeTimeline() {
    return project_ ? project_->activeTimeline() : nullptr;
}

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newAction = fileMenu->addAction(tr("&New Project..."));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewProject);

    QAction* openAction = fileMenu->addAction(tr("&Open Project..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    QMenu* recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    for (const std::string& path : nova::project::ProjectStore::recentProjects(8)) {
        QAction* recentAction = recentMenu->addAction(QString::fromStdString(path));
        recentAction->setData(QString::fromStdString(path));
        connect(recentAction, &QAction::triggered, this, &MainWindow::onRecentProjectTriggered);
    }
    if (recentMenu->isEmpty()) {
        recentMenu->addAction(tr("(none)"))->setEnabled(false);
    }

    fileMenu->addSeparator();

    QAction* saveAction = fileMenu->addAction(tr("&Save Project"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveProject);

    QAction* saveAsAction = fileMenu->addAction(tr("Save Project &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveProjectAs);

    fileMenu->addSeparator();

    QAction* importAction = fileMenu->addAction(tr("&Import Media..."));
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportMedia);

    QAction* extractAction = fileMenu->addAction(tr("Extract &Audio..."));
    connect(extractAction, &QAction::triggered, this, &MainWindow::onExtractAudio);

    fileMenu->addSeparator();

    QAction* exportAction = fileMenu->addAction(tr("&Export Project Copy..."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportProjectCopy);

    QAction* versionsAction = fileMenu->addAction(tr("&Version History..."));
    connect(versionsAction, &QAction::triggered, this, &MainWindow::onShowVersionHistory);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* splitAction = editMenu->addAction(tr("&Split at Playhead"));
    splitAction->setShortcut(QKeySequence(Qt::Key_B));
    connect(splitAction, &QAction::triggered, this, &MainWindow::onSplitAtPlayhead);

    QMenu* sequenceMenu = menuBar()->addMenu(tr("&Sequence"));
    sequenceMenu->addAction(tr("&Add Timeline"), this, &MainWindow::onAddTimeline);
}

void MainWindow::applyTheme() {
    setStyleSheet(R"(
        QMainWindow, QDockWidget, QWidget {
            background: #15171d;
            color: #e8eaf0;
            font-family: "Segoe UI";
            font-size: 10pt;
        }
        QMenuBar, QMenu {
            background: #191c23;
            color: #f1f3f8;
        }
        QMenuBar::item:selected, QMenu::item:selected {
            background: #2b3340;
        }
        QDockWidget::title {
            background: #20242d;
            padding: 7px;
            font-weight: 600;
        }
        #sidebarRail {
            background: #12151b;
            border-right: 1px solid #2f3644;
        }
        QToolButton {
            background: transparent;
            border: none;
            color: #c8ceda;
            padding: 6px;
        }
        QToolButton:checked {
            background: #2b3340;
            color: #ffffff;
        }
        QListWidget {
            background: #101218;
            border: 1px solid #2f3644;
            border-radius: 6px;
            padding: 4px;
        }
        QListWidget::item {
            padding: 8px;
            border-radius: 4px;
        }
        QListWidget::item:selected {
            background: #3f6fd8;
        }
        QPushButton {
            background: #2b3340;
            border: 1px solid #465164;
            border-radius: 6px;
            padding: 7px 12px;
        }
        QPushButton:hover {
            background: #364154;
        }
        QPushButton:pressed {
            background: #3f6fd8;
        }
        QSlider::groove:horizontal {
            height: 5px;
            background: #303747;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #72a7ff;
            width: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QLabel {
            color: #d8dce6;
        }
        QLineEdit, QComboBox {
            background: #101218;
            border: 1px solid #2f3644;
            border-radius: 6px;
            padding: 6px;
            color: #e8eaf0;
        }
    )");
}

void MainWindow::buildDockPanels() {
    auto* libraryDock = new QDockWidget(tr("Library"), this);
    sidebar_ = new SidebarPanel(libraryDock);
    libraryDock->setWidget(sidebar_);
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock);

    connect(sidebar_->mediaList(), &QListWidget::itemDoubleClicked, this,
            &MainWindow::onMediaItemActivated);
    connect(sidebar_->mediaSearch(), &QLineEdit::textChanged, this,
            &MainWindow::onMediaSearchChanged);
    connect(sidebar_->mediaFolderFilter(), &QComboBox::currentTextChanged, this,
            &MainWindow::onMediaFolderChanged);
    connect(sidebar_, &SidebarPanel::templateActivated, this, &MainWindow::onTemplateActivated);

    auto* inspectorDock = new QDockWidget(tr("Inspector"), this);
    auto* inspectorPanel = new QWidget(inspectorDock);
    auto* inspectorLayout = new QVBoxLayout(inspectorPanel);

    projectMetaLabel_ = new QLabel(tr("Local offline project"), inspectorPanel);
    inspectorLayout->addWidget(projectMetaLabel_);

    auto addSlider = [&](const QString& label, int min, int max, int defaultValue) {
        auto* row = new QWidget(inspectorPanel);
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->addWidget(new QLabel(label, row));
        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(min, max);
        slider->setValue(defaultValue);
        rowLayout->addWidget(slider);
        inspectorLayout->addWidget(row);
        return slider;
    };

    brightnessSlider_ = addSlider(tr("Brightness"), -100, 100, 0);
    contrastSlider_ = addSlider(tr("Contrast"), -100, 100, 0);
    saturationSlider_ = addSlider(tr("Saturation"), 0, 200, 100);
    volumeSlider_ = addSlider(tr("Volume"), 0, 100, 85);
    inspectorLayout->addStretch();

    connect(brightnessSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setBrightness);
    connect(contrastSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setContrast);
    connect(saturationSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setSaturation);
    connect(volumeSlider_, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    inspectorDock->setWidget(inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    auto* timelineDock = new QDockWidget(tr("Timeline"), this);
    auto* timelinePanel = new QWidget(timelineDock);
    auto* timelineLayout = new QVBoxLayout(timelinePanel);

    auto* transportRow = new QWidget(timelinePanel);
    auto* transportLayout = new QHBoxLayout(transportRow);
    playButton_ = new QPushButton(tr("Play"), transportRow);
    splitButton_ = new QPushButton(tr("Split"), transportRow);
    extractAudioButton_ = new QPushButton(tr("Extract Audio"), transportRow);
    statusLabel_ = new QLabel(tr("No clip loaded"), transportRow);
    timeLabel_ = new QLabel(tr("00:00.00"), transportRow);
    timelineSelector_ = new QComboBox(transportRow);
    auto* addTimelineButton = new QPushButton(tr("+ Timeline"), transportRow);

    transportLayout->addWidget(playButton_);
    transportLayout->addWidget(splitButton_);
    transportLayout->addWidget(extractAudioButton_);
    transportLayout->addSpacing(12);
    transportLayout->addWidget(timeLabel_);
    transportLayout->addWidget(statusLabel_);
    transportLayout->addStretch();
    transportLayout->addWidget(new QLabel(tr("Sequence:"), transportRow));
    transportLayout->addWidget(timelineSelector_);
    transportLayout->addWidget(addTimelineButton);

    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(splitButton_, &QPushButton::clicked, this, &MainWindow::onSplitAtPlayhead);
    connect(extractAudioButton_, &QPushButton::clicked, this, &MainWindow::onExtractAudio);
    connect(timelineSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onTimelineSelectionChanged);
    connect(addTimelineButton, &QPushButton::clicked, this, &MainWindow::onAddTimeline);

    timelineWidget_ = new TimelineWidget(timelinePanel);
    connect(timelineWidget_, &TimelineWidget::seekRequested, this, &MainWindow::onTimelineSeek);

    timelineLayout->addWidget(transportRow);
    timelineLayout->addWidget(timelineWidget_);
    timelineDock->setWidget(timelinePanel);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
}

void MainWindow::markDirty() {
    if (project_) project_->dirty = true;
}

bool MainWindow::maybeSaveBeforeDiscard() {
    if (!project_ || !project_->dirty) return true;

    const auto reply = QMessageBox::question(
        this, tr("Save project?"),
        tr("The current project has unsaved changes. Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (reply == QMessageBox::Cancel) return false;
    if (reply == QMessageBox::Save) {
        if (project_->filePath.empty()) {
            onSaveProjectAs();
            return !project_->dirty;
        }
        return saveProjectToPath(QString::fromStdString(project_->filePath), true);
    }
    return true;
}

bool MainWindow::saveProjectToPath(const QString& path, bool addToRecents) {
    if (!project_) return false;

    std::string error;
    project_->filePath = path.toStdString();
    if (!nova::project::ProjectIO::save(*project_, project_->filePath, &error)) {
        QMessageBox::warning(this, tr("Save failed"), QString::fromStdString(error));
        return false;
    }

    project_->dirty = false;
    nova::project::ProjectStore::createBackup(*project_);
    if (addToRecents) {
        nova::project::ProjectStore::addRecentProject(project_->filePath);
    }
    setWindowTitle(tr("Nova Studio — %1").arg(QFileInfo(path).fileName()));
    applyProjectToUi();
    return true;
}

void MainWindow::applyProjectToUi() {
    if (!project_) return;

    timelineSelector_->blockSignals(true);
    timelineSelector_->clear();
    for (const auto& timeline : project_->timelines) {
        timelineSelector_->addItem(QString::fromStdString(timeline->name()),
                                   QString::fromStdString(timeline->id()));
    }

    int activeIndex = 0;
    for (int i = 0; i < timelineSelector_->count(); ++i) {
        if (timelineSelector_->itemData(i).toString().toStdString() == project_->activeTimelineId) {
            activeIndex = i;
            break;
        }
    }
    timelineSelector_->setCurrentIndex(activeIndex);
    timelineSelector_->blockSignals(false);

    timelineWidget_->setTimeline(activeTimeline());
    refreshMediaList();

    const QString path = project_->filePath.empty()
                             ? tr("Unsaved local project")
                             : QString::fromStdString(project_->filePath);
    projectMetaLabel_->setText(tr("Project: %1\nMedia: %2 | Timelines: %3\nCloud sync: optional (off)")
                                   .arg(QString::fromStdString(project_->name))
                                   .arg(project_->media.size())
                                   .arg(project_->timelines.size()));
    setWindowTitle(tr("Nova Studio — %1%2")
                       .arg(QString::fromStdString(project_->name))
                       .arg(project_->dirty ? " *" : ""));
}

void MainWindow::refreshMediaList() {
    if (!sidebar_ || !project_) return;

    const QString search = sidebar_->mediaSearch()->text().trimmed().toLower();
    const QString folderKey = sidebar_->mediaFolderFilter()->currentData().toString();

    sidebar_->mediaList()->clear();
    for (const auto& asset : project_->media) {
        const QString name = QString::fromStdString(asset.name).toLower();
        const QString folder = QString::fromStdString(asset.folder);
        if (!search.isEmpty()) {
            bool matches = name.contains(search)
                           || folder.toLower().contains(search);
            for (const auto& tag : asset.tags) {
                if (QString::fromStdString(tag).toLower().contains(search)) {
                    matches = true;
                    break;
                }
            }
            if (!matches) continue;
        }
        if (!folderKey.isEmpty() && folder != folderKey) {
            continue;
        }

        const QString label = QString("%1  [%2x%3 @ %4 fps]")
                                .arg(QString::fromStdString(asset.name))
                                .arg(asset.width)
                                .arg(asset.height)
                                .arg(asset.frameRate, 0, 'f', 1);
        auto* item = new QListWidgetItem(label, sidebar_->mediaList());
        item->setData(Qt::UserRole, QString::fromStdString(asset.path));
        item->setToolTip(QString::fromStdString(asset.path));
    }
}

bool MainWindow::newProjectAtPath(const QString& path, double fps, int width, int height) {
    if (!maybeSaveBeforeDiscard()) return false;

    project_ = nova::project::Project::createBlank(
        QFileInfo(path).completeBaseName().toStdString(), fps, width, height);
    project_->filePath = path.toStdString();
    project_->dirty = true;
    applyProjectToUi();
    return saveProjectToPath(path, true);
}

bool MainWindow::openProjectPath(const QString& path) {
    if (!maybeSaveBeforeDiscard()) return false;

    std::string error;
    auto loaded = nova::project::ProjectIO::load(path.toStdString(), &error);
    if (!loaded) {
        QMessageBox::warning(this, tr("Open failed"), QString::fromStdString(error));
        return false;
    }

    project_ = std::make_unique<nova::project::Project>(std::move(*loaded));
    nova::project::ProjectStore::addRecentProject(project_->filePath);
    applyProjectToUi();
    NOVA_LOG_INFO(kModule, "Opened project: " + path.toStdString());
    return true;
}

bool MainWindow::newProjectFromTemplate(const QString& templatePathOrId) {
    if (!maybeSaveBeforeDiscard()) return false;

    std::unique_ptr<nova::project::Project> created;
    if (templatePathOrId == "__builtin_blank_1080p__") {
        created = nova::project::Project::createBlank("Blank 1080p", 30.0, 1920, 1080);
    } else if (templatePathOrId == "__builtin_vertical_916__") {
        created = nova::project::Project::createBlank("Vertical Reels", 30.0, 1080, 1920);
    } else if (templatePathOrId == "__builtin_youtube_1080p__") {
        created = nova::project::Project::createBlank("YouTube 1080p", 30.0, 1920, 1080);
        created->activeTimeline()->addTrack("v2", "V2", nova::timeline::TrackType::Video);
    } else {
        std::string error;
        auto loaded = nova::project::ProjectIO::loadTemplate(templatePathOrId.toStdString(), &error);
        if (!loaded) {
            QMessageBox::warning(this, tr("Template failed"), QString::fromStdString(error));
            return false;
        }
        created = std::make_unique<nova::project::Project>(std::move(*loaded));
    }

    const QString savePath = QFileDialog::getSaveFileName(
        this, tr("Save New Project From Template"), QString(), tr("Nova Project (*.nova)"));
    if (savePath.isEmpty()) return false;

    project_ = std::move(created);
    project_->filePath = savePath.toStdString();
    project_->dirty = true;
    applyProjectToUi();
    return saveProjectToPath(savePath, true);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSaveBeforeDiscard()) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::onNewProject() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("New Project"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;
    newProjectAtPath(path, 30.0, 1920, 1080);
}

void MainWindow::onOpenProject() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;
    openProjectPath(path);
}

void MainWindow::onSaveProject() {
    if (!project_) return;
    if (project_->filePath.empty()) {
        onSaveProjectAs();
        return;
    }
    saveProjectToPath(QString::fromStdString(project_->filePath), true);
}

void MainWindow::onSaveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;
    saveProjectToPath(path, true);
}

void MainWindow::onExportProjectCopy() {
    if (!project_) return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Project Copy"), QString(), tr("Nova Project (*.nova)"));
    if (path.isEmpty()) return;

    std::string error;
    if (!nova::project::ProjectIO::save(*project_, path.toStdString(), &error)) {
        QMessageBox::warning(this, tr("Export failed"), QString::fromStdString(error));
    } else {
        QMessageBox::information(this, tr("Export complete"),
                                 tr("Project copy saved to:\n%1").arg(path));
    }
}

void MainWindow::onShowVersionHistory() {
    if (!project_ || project_->filePath.empty()) {
        QMessageBox::information(this, tr("Version History"),
                                 tr("Save the project first to enable local backups."));
        return;
    }

    const auto versions =
        nova::project::ProjectStore::versionHistory(project_->filePath);
    if (versions.empty()) {
        QMessageBox::information(this, tr("Version History"),
                                 tr("No backups yet. Backups are created on each save."));
        return;
    }

    QStringList labels;
    for (const auto& entry : versions) {
        labels << QString::fromStdString(entry.label);
    }
    bool ok = false;
    const QString picked = QInputDialog::getItem(
        this, tr("Version History"), tr("Restore a backup:"), labels, 0, false, &ok);
    if (!ok || picked.isEmpty()) return;

    for (const auto& entry : versions) {
        if (entry.label != picked.toStdString()) continue;
        std::string error;
        auto restored = nova::project::ProjectStore::restoreVersion(entry.path, &error);
        if (!restored) {
            QMessageBox::warning(this, tr("Restore failed"), QString::fromStdString(error));
            return;
        }
        project_ = std::make_unique<nova::project::Project>(std::move(*restored));
        project_->filePath = entry.path; // keep backup path distinct until user saves
        project_->dirty = true;
        applyProjectToUi();
        QMessageBox::information(this, tr("Restored"),
                                 tr("Backup loaded. Use Save Project to replace the main file."));
        break;
    }
}

void MainWindow::onRecentProjectTriggered() {
    if (auto* action = qobject_cast<QAction*>(sender())) {
        openProjectPath(action->data().toString());
    }
}

void MainWindow::onImportMedia() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Media"), QString(),
        tr("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.gif *.png *.jpg *.jpeg *.svg "
           "*.mp3 *.wav *.aac *.flac *.srt *.vtt);;All Files (*)"));
    if (path.isEmpty() || !project_) return;

    nova::media::Decoder probe;
    nova::project::MediaAsset asset;
    asset.id = "media-" + std::to_string(project_->media.size() + 1);
    asset.path = path.toStdString();
    asset.name = QFileInfo(path).fileName().toStdString();
    asset.folder = "Imports";

    if (probe.open(path.toStdString())) {
        const auto& info = probe.info();
        asset.width = info.width;
        asset.height = info.height;
        asset.frameRate = info.frameRate;
        asset.durationSeconds = info.durationSeconds;
        asset.hasVideo = info.hasVideo;
        asset.hasAudio = info.hasAudio;
    }

    project_->media.push_back(std::move(asset));
    markDirty();
    refreshMediaList();
}

void MainWindow::onMediaItemActivated() {
    auto* item = sidebar_->mediaList()->currentItem();
    if (!item) return;
    loadMediaIntoPreview(item->data(Qt::UserRole).toString());
}

void MainWindow::onMediaSearchChanged() { refreshMediaList(); }
void MainWindow::onMediaFolderChanged() { refreshMediaList(); }

void MainWindow::onTimelineSelectionChanged(int index) {
    if (!project_ || index < 0) return;
    project_->activeTimelineId = timelineSelector_->itemData(index).toString().toStdString();
    timelineWidget_->setTimeline(activeTimeline());
    markDirty();
}

void MainWindow::onAddTimeline() {
    if (!project_) return;

    const int count = static_cast<int>(project_->timelines.size()) + 1;
    const std::string id = "seq-" + std::to_string(count);
    const std::string name = "Sequence " + QString::number(count).toStdString();

    auto timeline = std::make_unique<nova::timeline::Timeline>(id, name, 30.0, 1920, 1080);
    timeline->addTrack("v1", "V1", nova::timeline::TrackType::Video);
    timeline->addTrack("a1", "A1", nova::timeline::TrackType::Audio);
    project_->timelines.push_back(std::move(timeline));
    project_->activeTimelineId = id;
    markDirty();
    applyProjectToUi();
}

void MainWindow::loadMediaIntoPreview(const QString& path) {
    playbackTimer_.stop();
    playing_ = false;
    playButton_->setText(tr("Play"));
    if (audioPlayer_) audioPlayer_->stop();

    if (!decoder_->open(path.toStdString())) {
        QMessageBox::warning(this, tr("Import failed"),
                              tr("Could not open media:\n%1")
                                  .arg(QString::fromStdString(decoder_->lastError())));
        return;
    }

    currentMediaPath_ = path;
    if (audioPlayer_) {
        audioPlayer_->setSource(QUrl::fromLocalFile(path));
        audioPlayer_->setPosition(0);
    }

    const auto& info = decoder_->info();
    statusLabel_->setText(tr("%1 — %2x%3 @ %4 fps")
                               .arg(QFileInfo(path).fileName())
                               .arg(info.width)
                               .arg(info.height)
                               .arg(info.frameRate, 0, 'f', 2));

    addClipToTimeline(path);
    timelineWidget_->setTimeline(activeTimeline());

    currentTimeSeconds_ = 0.0;
    decoder_->seek(0.0);
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
    }
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("00:00.00"));
    markDirty();
}

void MainWindow::addClipToTimeline(const QString& path) {
    auto* timeline = activeTimeline();
    if (!timeline || timeline->tracks().empty()) return;

    const auto& info = decoder_->info();

    nova::timeline::Clip clip;
    clip.id = "clip-" + path.toStdString();
    clip.name = QFileInfo(path).fileName().toStdString();
    clip.type = nova::timeline::ClipType::Video;
    clip.mediaPath = path.toStdString();
    clip.sourceIn = 0;
    clip.sourceOut = static_cast<int64_t>(info.durationSeconds * info.frameRate);
    clip.timelineStart = 0;
    clip.timelineEnd = static_cast<int64_t>(info.durationSeconds * timeline->frameRate());
    if (info.hasAudio) {
        clip.linkedClipId = "audio-" + path.toStdString();
    }

    auto* v1 = timeline->tracks().front().get();
    const auto& clips = v1->clips();
    if (!clips.empty()) v1->removeClip(clips.front().id);
    v1->addClip(clip);

    if (info.hasAudio && timeline->tracks().size() > 1) {
        nova::timeline::Clip audioClip = clip;
        audioClip.id = "audio-" + path.toStdString();
        audioClip.name = QFileInfo(path).completeBaseName().toStdString() + " audio";
        audioClip.type = nova::timeline::ClipType::Audio;
        audioClip.linkedClipId = clip.id;

        auto* a1 = timeline->tracks()[1].get();
        const auto& audioClips = a1->clips();
        if (!audioClips.empty()) a1->removeClip(audioClips.front().id);
        a1->addClip(audioClip);
    }
}

void MainWindow::onPlayPauseClicked() {
    if (!decoder_->isOpen()) return;

    playing_ = !playing_;
    playButton_->setText(playing_ ? tr("Pause") : tr("Play"));

    if (playing_) {
        syncAudioToCurrentTime();
        if (audioPlayer_ && decoder_->info().hasAudio) {
            audioPlayer_->play();
        }
        const double intervalMs = 1000.0 / std::max(1.0, decoder_->info().frameRate);
        playbackTimer_.start(static_cast<int>(intervalMs));
    } else {
        playbackTimer_.stop();
        if (audioPlayer_) audioPlayer_->pause();
    }
}

void MainWindow::onPlaybackTick() {
    auto frame = decoder_->nextFrame();
    if (!frame) {
        decoder_->seek(0.0);
        frame = decoder_->nextFrame();
        if (!frame) {
            playbackTimer_.stop();
            if (audioPlayer_) audioPlayer_->stop();
            playing_ = false;
            playButton_->setText(tr("Play"));
            return;
        }
    }
    preview_->setFrame(*frame);
    currentTimeSeconds_ = frame->timeSeconds;
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
}

void MainWindow::onTimelineSeek(double seconds) {
    if (!decoder_->isOpen()) return;
    const bool wasPlaying = playing_;
    if (audioPlayer_) audioPlayer_->pause();
    decoder_->seek(seconds);
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
        currentTimeSeconds_ = frame->timeSeconds;
        timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
        timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
        syncAudioToCurrentTime();
        if (wasPlaying && audioPlayer_ && decoder_->info().hasAudio) {
            audioPlayer_->play();
        }
    }
}

void MainWindow::syncAudioToCurrentTime() {
    if (!audioPlayer_) return;
    audioPlayer_->setPosition(static_cast<qint64>(std::max(0.0, currentTimeSeconds_) * 1000.0));
}

void MainWindow::onVolumeChanged(int value) {
    if (!audioOutput_) return;
    audioOutput_->setVolume(std::clamp(value, 0, 100) / 100.0);
}

void MainWindow::onSplitAtPlayhead() {
    auto* timeline = activeTimeline();
    if (!timeline || timeline->tracks().empty()) return;

    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(currentTimeSeconds_ * timeline->frameRate()));

    auto* v1 = timeline->tracks().front().get();
    const auto* clip = v1->findClipAt(frame);
    if (!clip) {
        statusLabel_->setText(tr("No clip under playhead to split"));
        return;
    }

    const std::string rightId = clip->id + "-cut-" + std::to_string(frame);
    if (v1->splitClipAt(clip->id, frame, rightId)) {
        if (timeline->tracks().size() > 1) {
            auto* a1 = timeline->tracks()[1].get();
            if (const auto* audioClip = a1->findClipAt(frame)) {
                const std::string audioId = audioClip->id;
                a1->splitClipAt(audioId, frame, audioId + "-cut-" + std::to_string(frame));
            }
        }
        timelineWidget_->setTimeline(timeline);
        statusLabel_->setText(tr("Split at %1s").arg(currentTimeSeconds_, 0, 'f', 2));
        markDirty();
    }
}

void MainWindow::onExtractAudio() {
    if (currentMediaPath_.isEmpty()) {
        QMessageBox::information(this, tr("Extract Audio"), tr("Import a clip first."));
        return;
    }

    const QString outputPath = QFileDialog::getSaveFileName(
        this, tr("Extract Audio"),
        QFileInfo(currentMediaPath_).completeBaseName() + ".wav",
        tr("WAV Audio (*.wav)"));
    if (outputPath.isEmpty()) return;

    nova::media::AudioExtractor extractor;
    if (!extractor.extractWav(currentMediaPath_.toStdString(), outputPath.toStdString())) {
        QMessageBox::warning(this, tr("Extract Audio Failed"),
                             QString::fromStdString(extractor.lastError()));
        return;
    }

    QMessageBox::information(this, tr("Extract Audio"),
                             tr("Audio extracted to:\n%1").arg(outputPath));
}

void MainWindow::onAutosaveTick() {
    if (!project_ || !project_->dirty || project_->filePath.empty()) return;
    std::string error;
    if (nova::project::ProjectStore::autosave(*project_, &error)) {
        NOVA_LOG_INFO(kModule, "Autosaved project");
    }
}

void MainWindow::onTemplateActivated(const QString& templatePath) {
    newProjectFromTemplate(templatePath);
}

} // namespace nova::ui
