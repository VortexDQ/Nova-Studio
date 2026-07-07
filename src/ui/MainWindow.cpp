#include "nova/ui/MainWindow.h"
#include "nova/ui/TimelineWidget.h"
#include "nova/ui/SidebarPanel.h"
#include "nova/ui/PreviewOverlay.h"
#include "nova/renderer/VideoPreviewWidget.h"
#include "nova/core/Logger.h"
#include "nova/media/AudioExtractor.h"
#include "nova/media/MediaProbe.h"
#include "nova/media/VideoExporter.h"
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
#include <QImageReader>
#include <QKeyEvent>
#include <QDir>
#include <QImage>
#include <QResizeEvent>
#include <QStandardPaths>
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
    auto* previewContainer = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewContainer);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(preview_);
    previewOverlay_ = new PreviewOverlay(preview_);
    previewOverlay_->setGeometry(preview_->rect());
    previewOverlay_->raise();
    preview_->installEventFilter(this);
    setCentralWidget(previewContainer);

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

    QAction* saveAsAction = fileMenu->addAction(tr("Save Project &As (.nova)..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveProjectAs);

    fileMenu->addSeparator();

    QAction* exportVideoAction = fileMenu->addAction(tr("Export &Video..."));
    exportVideoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportVideoAction, &QAction::triggered, this, &MainWindow::onExportVideo);

    QAction* importAction = fileMenu->addAction(tr("&Import Media..."));
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportMedia);

    QAction* extractAction = fileMenu->addAction(tr("Extract &Audio..."));
    connect(extractAction, &QAction::triggered, this, &MainWindow::onExtractAudio);

    fileMenu->addSeparator();

    QAction* exportAction = fileMenu->addAction(tr("Duplicate Project (.nova)..."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportProjectCopy);

    QAction* versionsAction = fileMenu->addAction(tr("&Version History..."));
    connect(versionsAction, &QAction::triggered, this, &MainWindow::onShowVersionHistory);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* splitAction = editMenu->addAction(tr("&Split at Playhead"));
    splitAction->setShortcut(QKeySequence(Qt::Key_B));
    connect(splitAction, &QAction::triggered, this, &MainWindow::onSplitAtPlayhead);

    QAction* deleteClipAction = editMenu->addAction(tr("&Delete Selected Clip"));
    deleteClipAction->setShortcut(QKeySequence::Delete);
    connect(deleteClipAction, &QAction::triggered, this, &MainWindow::onDeleteSelectedClip);

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
            border-radius: 4px;
            color: #9aa3b2;
            padding: 8px 4px;
        }
        QToolButton:hover {
            background: #1e232d;
            color: #e8eaf0;
        }
        QToolButton:checked {
            background: #2b3340;
            color: #ffffff;
            border-left: 3px solid #3f6fd8;
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
    auto* libraryDock = new QDockWidget(tr("Browser"), this);
    sidebar_ = new SidebarPanel(libraryDock);
    libraryDock->setWidget(sidebar_);
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock);

    connect(sidebar_->mediaList(), &QListWidget::itemClicked, this, &MainWindow::onMediaItemClicked);
    connect(sidebar_->mediaList(), &QListWidget::itemDoubleClicked, this,
            &MainWindow::onMediaItemActivated);
    connect(sidebar_, &SidebarPanel::importMediaRequested, this, &MainWindow::onImportMedia);
    connect(sidebar_, &SidebarPanel::importMediaToFolder, this, &MainWindow::onImportMediaToFolder);
    connect(sidebar_->mediaSearch(), &QLineEdit::textChanged, this,
            &MainWindow::onMediaSearchChanged);
    connect(sidebar_->mediaFolderFilter(), QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onMediaFolderChanged);
    connect(sidebar_, &SidebarPanel::templateActivated, this, &MainWindow::onTemplateActivated);
    connect(sidebar_, &SidebarPanel::textPresetActivated, this, &MainWindow::onTextPresetActivated);
    connect(sidebar_, &SidebarPanel::transitionActivated, this, &MainWindow::onTransitionActivated);
    connect(sidebar_, &SidebarPanel::libraryAssetActivated, this, &MainWindow::onLibraryAssetActivated);

    auto* inspectorDock = new QDockWidget(tr("Inspector"), this);
    auto* inspectorPanel = new QWidget(inspectorDock);
    auto* inspectorLayout = new QVBoxLayout(inspectorPanel);

    projectMetaLabel_ = new QLabel(tr("Local offline project"), inspectorPanel);
    inspectorLayout->addWidget(projectMetaLabel_);

    mediaMetaLabel_ = new QLabel(tr("Select a clip in the browser or timeline."), inspectorPanel);
    mediaMetaLabel_->setWordWrap(true);
    mediaMetaLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    inspectorLayout->addWidget(mediaMetaLabel_);

    inspectorLayout->addWidget(new QLabel(tr("Adjustments"), inspectorPanel));

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
    connect(timelineWidget_, &TimelineWidget::clipSelected, this, &MainWindow::onClipSelected);

    timelineLayout->addWidget(transportRow);
    timelineLayout->addWidget(timelineWidget_);
    timelineDock->setWidget(timelinePanel);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
}

void MainWindow::markDirty() {
    if (project_) project_->dirty = true;
}

void MainWindow::captureSessionState() {
    if (!project_) return;
    project_->lastPlayheadSeconds = currentTimeSeconds_;
    project_->lastPreviewMediaPath = currentMediaPath_.toStdString();
    project_->selectedClipId = selectedClipId_;
}

QString MainWindow::formatBytes(int64_t bytes) const {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

void MainWindow::showMediaMetadata(const QString& path) {
    const auto meta = nova::media::MediaProbe::probe(path.toStdString());
    QStringList lines;
    lines << tr("<b>%1</b>").arg(QString::fromStdString(meta.fileName));
    lines << tr("Path: %1").arg(path);
    lines << tr("Size: %1").arg(formatBytes(meta.fileSizeBytes));
    if (!meta.formatName.empty()) lines << tr("Format: %1").arg(QString::fromStdString(meta.formatName));
    if (meta.hasVideo) {
        lines << tr("Resolution: %1 x %2").arg(meta.width).arg(meta.height);
        if (meta.frameRate > 0) lines << tr("Frame rate: %1 fps").arg(meta.frameRate, 0, 'f', 2);
        if (!meta.videoCodec.empty()) lines << tr("Video codec: %1").arg(QString::fromStdString(meta.videoCodec));
    }
    if (meta.hasAudio && !meta.audioCodec.empty()) {
        lines << tr("Audio codec: %1").arg(QString::fromStdString(meta.audioCodec));
    }
    if (meta.durationSeconds > 0) {
        lines << tr("Duration: %1 s").arg(meta.durationSeconds, 0, 'f', 2);
    }
    if (meta.isImage) lines << tr("Type: still image");
    mediaMetaLabel_->setText(lines.join("<br>"));
}

void MainWindow::showClipMetadata(const nova::timeline::Clip& clip) {
    if (!clip.mediaPath.empty()) {
        showMediaMetadata(QString::fromStdString(clip.mediaPath));
    } else {
        mediaMetaLabel_->setText(tr("<b>%1</b><br>Title overlay")
                                     .arg(QString::fromStdString(clip.name)));
    }
    mediaMetaLabel_->setText(mediaMetaLabel_->text()
                             + tr("<br><br>Timeline: %1 - %2 frames")
                                   .arg(clip.timelineStart)
                                   .arg(clip.timelineEnd));
}

void MainWindow::refreshMediaMetadataFromFiles() {
    if (!project_) return;
    for (auto& asset : project_->media) {
        if (!QFileInfo::exists(QString::fromStdString(asset.path))) continue;
        const auto meta = nova::media::MediaProbe::probe(asset.path);
        asset.width = meta.width;
        asset.height = meta.height;
        asset.frameRate = meta.frameRate;
        asset.durationSeconds = meta.durationSeconds;
        asset.hasVideo = meta.hasVideo;
        asset.hasAudio = meta.hasAudio;
        if (asset.name.empty()) asset.name = meta.fileName;
    }
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

    captureSessionState();
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
    setWindowTitle(tr("Nova Studio - %1").arg(QFileInfo(path).fileName()));
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
    projectMetaLabel_->setText(tr("Project: %1\nMedia: %2  ·  Sequences: %3")
                                   .arg(QString::fromStdString(project_->name))
                                   .arg(project_->media.size())
                                   .arg(project_->timelines.size()));
    setWindowTitle(tr("Nova Studio - %1%2")
                       .arg(QString::fromStdString(project_->name))
                       .arg(project_->dirty ? " *" : ""));
}

void MainWindow::restoreProjectSession() {
    if (!project_) return;

    refreshMediaMetadataFromFiles();
    refreshMediaList();

    QString mediaToLoad;
    if (!project_->lastPreviewMediaPath.empty()
        && QFileInfo::exists(QString::fromStdString(project_->lastPreviewMediaPath))) {
        mediaToLoad = QString::fromStdString(project_->lastPreviewMediaPath);
    } else {
        const auto* timeline = activeTimeline();
        if (timeline) {
            for (const auto& track : timeline->tracks()) {
                if (track->type() != nova::timeline::TrackType::Video) continue;
                for (const auto& clip : track->clips()) {
                    if (!clip.mediaPath.empty() && QFileInfo::exists(QString::fromStdString(clip.mediaPath))) {
                        mediaToLoad = QString::fromStdString(clip.mediaPath);
                        break;
                    }
                }
                if (!mediaToLoad.isEmpty()) break;
            }
        }
    }

    if (!mediaToLoad.isEmpty()) {
        previewMediaFile(mediaToLoad, project_->lastPlayheadSeconds);
        showMediaMetadata(mediaToLoad);

        for (int i = 0; i < sidebar_->mediaList()->count(); ++i) {
            auto* item = sidebar_->mediaList()->item(i);
            if (item && item->data(Qt::UserRole).toString() == mediaToLoad) {
                sidebar_->mediaList()->setCurrentItem(item);
                break;
            }
        }
    }

    if (!project_->selectedClipId.empty()) {
        selectedClipId_ = project_->selectedClipId;
        timelineWidget_->setSelectedClipId(selectedClipId_);
        if (const auto* clip = findClipById(selectedClipId_)) {
            showClipMetadata(*clip);
        }
    }

    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
    updatePreviewEffects();
    updateTitleOverlay();
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

        const QString label = asset.hasVideo && asset.width > 0
                                  ? QString("%1  [%2x%3 @ %4 fps]")
                                        .arg(QString::fromStdString(asset.name))
                                        .arg(asset.width)
                                        .arg(asset.height)
                                        .arg(asset.frameRate, 0, 'f', 1)
                                  : QString("%1  [%2]")
                                        .arg(QString::fromStdString(asset.name))
                                        .arg(asset.hasAudio ? tr("audio") : tr("media"));
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
    restoreProjectSession();
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
    captureSessionState();
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
    captureSessionState();
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

void MainWindow::onExportVideo() {
    if (currentMediaPath_.isEmpty()) {
        QMessageBox::information(this, tr("Export Video"),
                                 tr("Load or select a video clip first."));
        return;
    }

    const QString filter = tr("MP4 Video (*.mp4);;MOV Video (*.mov);;MKV Video (*.mkv);;"
                              "WebM Video (*.webm);;AVI Video (*.avi)");
    QString path = QFileDialog::getSaveFileName(this, tr("Export Video"), QString(), filter);
    if (path.isEmpty()) return;

    nova::media::ExportFormat format = nova::media::ExportFormat::Mp4;
    if (path.endsWith(".mov", Qt::CaseInsensitive)) format = nova::media::ExportFormat::Mov;
    else if (path.endsWith(".mkv", Qt::CaseInsensitive)) format = nova::media::ExportFormat::Mkv;
    else if (path.endsWith(".webm", Qt::CaseInsensitive)) format = nova::media::ExportFormat::Webm;
    else if (path.endsWith(".avi", Qt::CaseInsensitive)) format = nova::media::ExportFormat::Avi;
    else if (!path.contains('.')) path += ".mp4";

    QString inputPath = currentMediaPath_;
    double startSec = 0.0;
    double endSec = 0.0;

    const nova::timeline::Clip* exportClip = nullptr;
    if (!selectedClipId_.empty()) {
        exportClip = findClipById(selectedClipId_);
    }
    if (!exportClip) {
        exportClip = findVideoClipAt(currentTimeSeconds_);
    }

    const auto meta = nova::media::MediaProbe::probe(inputPath.toStdString());
    endSec = meta.durationSeconds > 0 ? meta.durationSeconds : 5.0;

    if (exportClip && !exportClip->mediaPath.empty()) {
        inputPath = QString::fromStdString(exportClip->mediaPath);
        if (auto* timeline = activeTimeline()) {
            const double fps = std::max(1.0, timeline->frameRate());
            startSec = exportClip->sourceIn / fps;
            endSec = exportClip->sourceOut / fps;
        }
    }

    if (endSec <= startSec) {
        QMessageBox::warning(this, tr("Export Video"), tr("Invalid export range."));
        return;
    }

    nova::media::VideoExporter exporter;
    std::string error;
    if (!exporter.exportClip(inputPath.toStdString(), path.toStdString(), format,
                             startSec, endSec, &error)) {
        QMessageBox::warning(this, tr("Export failed"), QString::fromStdString(error));
        return;
    }

    QMessageBox::information(this, tr("Export complete"),
                             tr("Video saved to:\n%1").arg(path));
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
        restoreProjectSession();
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
    pendingImportFolder_ = QStringLiteral("Imports");
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Media"), QString(),
        tr("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.gif *.png *.jpg *.jpeg *.svg "
           "*.mp3 *.wav *.aac *.flac *.srt *.vtt);;All Files (*)"));
    if (path.isEmpty()) return;
    importMediaFile(path, pendingImportFolder_);
}

void MainWindow::onImportMediaToFolder(const QString& folder) {
    pendingImportFolder_ = folder;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Media"), QString(),
        tr("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.gif *.png *.jpg *.jpeg *.svg "
           "*.mp3 *.wav *.aac *.flac);;All Files (*)"));
    if (path.isEmpty()) return;
    importMediaFile(path, folder);
}

void MainWindow::importMediaFile(const QString& path, const QString& folder) {
    if (!project_) return;

    const auto meta = nova::media::MediaProbe::probe(path.toStdString());
    nova::project::MediaAsset asset;
    asset.id = "media-" + std::to_string(project_->media.size() + 1);
    asset.path = path.toStdString();
    asset.name = meta.fileName.empty() ? QFileInfo(path).fileName().toStdString() : meta.fileName;
    asset.folder = folder.toStdString();
    asset.width = meta.width;
    asset.height = meta.height;
    asset.frameRate = meta.frameRate;
    asset.durationSeconds = meta.durationSeconds;
    asset.hasVideo = meta.hasVideo;
    asset.hasAudio = meta.hasAudio;

    project_->media.push_back(std::move(asset));
    markDirty();
    refreshMediaList();
    previewMediaFile(path, 0.0);
    showMediaMetadata(path);
    statusLabel_->setText(tr("Imported %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::onMediaItemClicked() {
    auto* item = sidebar_->mediaList()->currentItem();
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    previewMediaFile(path, 0.0);
    showMediaMetadata(path);
}

void MainWindow::onMediaItemActivated() {
    auto* item = sidebar_->mediaList()->currentItem();
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    previewMediaFile(path, currentTimeSeconds_);
    showMediaMetadata(path);
    addMediaToTimeline(path);
}

void MainWindow::onClipSelected(const QString& clipId, const QString& mediaPath) {
    selectedClipId_ = clipId.toStdString();
    if (project_) project_->selectedClipId = selectedClipId_;
    if (const auto* clip = findClipById(selectedClipId_)) {
        showClipMetadata(*clip);
        auto* timeline = activeTimeline();
        if (timeline && timeline->frameRate() > 0) {
            const double clipStart = clip->timelineStart / timeline->frameRate();
            onTimelineSeek(clipStart);
        }
    }
    if (!mediaPath.isEmpty()) {
        previewMediaFile(mediaPath, currentTimeSeconds_);
    }
    markDirty();
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

bool MainWindow::previewImageFile(const QString& path) {
    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull()) return false;

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    nova::media::VideoFrame frame;
    frame.width = rgba.width();
    frame.height = rgba.height();
    frame.rgba.assign(rgba.constBits(), rgba.constBits() + rgba.sizeInBytes());
    frame.timeSeconds = 0.0;
    preview_->setFrame(frame);
    currentMediaPath_ = path;
    if (audioPlayer_) audioPlayer_->stop();
    return true;
}

bool MainWindow::previewMediaFile(const QString& path, double seekSeconds) {
    playbackTimer_.stop();
    playing_ = false;
    playButton_->setText(tr("Play"));
    pendingFrame_.reset();
    playbackStartSeconds_ = seekSeconds;
    if (audioPlayer_) audioPlayer_->pause();

    const auto meta = nova::media::MediaProbe::probe(path.toStdString());
    if (meta.isImage) {
        if (!previewImageFile(path)) {
            QMessageBox::warning(this, tr("Preview failed"), tr("Could not open image:\n%1").arg(path));
            return false;
        }
        currentTimeSeconds_ = 0.0;
    } else {
        if (!decoder_->open(path.toStdString())) {
            QMessageBox::warning(this, tr("Preview failed"),
                                  tr("Could not open media:\n%1")
                                      .arg(QString::fromStdString(decoder_->lastError())));
            return false;
        }
        currentMediaPath_ = path;
        if (audioPlayer_ && meta.hasAudio) {
            audioPlayer_->setSource(QUrl::fromLocalFile(path));
            audioPlayer_->setPosition(static_cast<qint64>(std::max(0.0, seekSeconds) * 1000.0));
        } else if (audioPlayer_) {
            audioPlayer_->stop();
        }
        decoder_->seek(seekSeconds);
        if (auto frame = decoder_->nextFrame()) {
            preview_->setFrame(*frame);
            currentTimeSeconds_ = frame->timeSeconds;
        } else {
            currentTimeSeconds_ = seekSeconds;
        }
    }

    statusLabel_->setText(tr("%1 - %2x%3 @ %4 fps")
                               .arg(QFileInfo(path).fileName())
                               .arg(meta.width)
                               .arg(meta.height)
                               .arg(meta.frameRate > 0 ? meta.frameRate : 30.0, 0, 'f', 2));
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
    updatePreviewEffects();
    updateTitleOverlay();
    return true;
}

void MainWindow::addMediaToTimeline(const QString& path) {
    auto* timeline = activeTimeline();
    if (!timeline || timeline->tracks().empty()) return;

    const auto meta = nova::media::MediaProbe::probe(path.toStdString());
    const double fps = std::max(1.0, timeline->frameRate());
    auto frameAtPlayhead = static_cast<nova::timeline::FrameNumber>(
        std::llround(currentTimeSeconds_ * fps));
    const double durationSec = meta.durationSeconds > 0 ? meta.durationSeconds : 5.0;
    const auto durationFrames = static_cast<nova::timeline::FrameNumber>(
        std::max<int64_t>(1, std::llround(durationSec * fps)));

    nova::timeline::Clip clip;
    clip.id = "clip-" + std::to_string(frameAtPlayhead) + "-" + path.toStdString();
    clip.name = QFileInfo(path).fileName().toStdString();
    clip.type = meta.isImage ? nova::timeline::ClipType::Image : nova::timeline::ClipType::Video;
    clip.mediaPath = path.toStdString();
    clip.sourceIn = 0;
    clip.sourceOut = durationFrames;
    clip.timelineStart = frameAtPlayhead;
    clip.timelineEnd = frameAtPlayhead + durationFrames;
    if (meta.hasAudio) {
        clip.linkedClipId = "audio-" + clip.id;
    }

    nova::timeline::Track* videoTrack = nullptr;
    for (const auto& track : timeline->tracks()) {
        if (track->type() == nova::timeline::TrackType::Video) {
            videoTrack = track.get();
            break;
        }
    }
    if (!videoTrack) return;

    if (!videoTrack->addClip(clip)) {
        nova::timeline::FrameNumber end = 0;
        for (const auto& existing : videoTrack->clips()) {
            end = std::max(end, existing.timelineEnd);
        }
        clip.timelineStart = end;
        clip.timelineEnd = end + durationFrames;
        clip.id = "clip-" + std::to_string(end) + "-" + path.toStdString();
        if (meta.hasAudio) clip.linkedClipId = "audio-" + clip.id;
        videoTrack->addClip(clip, true);
    }

    if (meta.hasAudio) {
        nova::timeline::Track* audioTrack = nullptr;
        for (const auto& track : timeline->tracks()) {
            if (track->type() == nova::timeline::TrackType::Audio) {
                audioTrack = track.get();
                break;
            }
        }
        if (audioTrack) {
            nova::timeline::Clip audioClip = clip;
            audioClip.id = *clip.linkedClipId;
            audioClip.name = QFileInfo(path).completeBaseName().toStdString() + " audio";
            audioClip.type = nova::timeline::ClipType::Audio;
            audioClip.linkedClipId = clip.id;
            audioTrack->addClip(std::move(audioClip), true);
        }
    }

    selectedClipId_ = clip.id;
    timelineWidget_->setSelectedClipId(selectedClipId_);
    timelineWidget_->setTimeline(timeline);
    showClipMetadata(clip);
    markDirty();
}

double MainWindow::masterClockSeconds() const {
    // Prefer the audio player's real position as the master clock so video
    // is slaved to audio (the canonical A/V-sync arrangement). Fall back to a
    // wall-clock elapsed timer for silent clips.
    if (audioPlayer_ && decoder_->info().hasAudio
        && audioPlayer_->playbackState() == QMediaPlayer::PlayingState) {
        return audioPlayer_->position() / 1000.0;
    }
    if (!playbackClock_.isValid()) return currentTimeSeconds_;
    return playbackStartSeconds_ + playbackClock_.elapsed() / 1000.0;
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
        // Anchor the master clock to the current media time.
        playbackStartSeconds_ = currentTimeSeconds_;
        playbackClock_.restart();
        pendingFrame_.reset();
        // Tick well above the frame rate so we can present frames precisely
        // when they come due rather than one-frame-per-tick.
        playbackTimer_.start(5);
    } else {
        playbackTimer_.stop();
        if (audioPlayer_) audioPlayer_->pause();
    }
}

void MainWindow::onPlaybackTick() {
    const double target = masterClockSeconds();

    // Present the latest frame whose PTS is at or before the master clock,
    // decoding ahead and dropping frames that fell behind. Hold the current
    // frame if the next decoded one isn't due yet.
    std::optional<nova::media::VideoFrame> toShow;
    bool reachedEnd = false;

    while (true) {
        if (!pendingFrame_) {
            pendingFrame_ = decoder_->nextFrame();
            if (!pendingFrame_) {
                reachedEnd = true;
                break;
            }
        }
        if (pendingFrame_->timeSeconds <= target) {
            toShow = std::move(pendingFrame_);
            pendingFrame_.reset();
            // Keep draining until we reach a frame that's still in the future,
            // so we drop stale frames instead of falling behind.
            continue;
        }
        break; // next frame not due yet
    }

    if (reachedEnd && !toShow) {
        // Loop back to the start, restarting both clocks in lock-step.
        decoder_->seek(0.0);
        pendingFrame_.reset();
        currentTimeSeconds_ = 0.0;
        playbackStartSeconds_ = 0.0;
        playbackClock_.restart();
        if (audioPlayer_ && decoder_->info().hasAudio) {
            audioPlayer_->setPosition(0);
            audioPlayer_->play();
        } else if (audioPlayer_) {
            audioPlayer_->stop();
        }
        auto frame = decoder_->nextFrame();
        if (!frame) {
            playbackTimer_.stop();
            if (audioPlayer_) audioPlayer_->stop();
            playing_ = false;
            playButton_->setText(tr("Play"));
            return;
        }
        toShow = std::move(frame);
    }

    if (!toShow) return; // nothing due this tick; hold current frame

    preview_->setFrame(*toShow);
    currentTimeSeconds_ = toShow->timeSeconds;
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
    updatePreviewEffects();
    updateTitleOverlay();
}

void MainWindow::onTimelineSeek(double seconds) {
    if (!decoder_->isOpen()) return;
    const bool wasPlaying = playing_;
    if (audioPlayer_) audioPlayer_->pause();
    decoder_->seek(seconds);
    pendingFrame_.reset();
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
        currentTimeSeconds_ = frame->timeSeconds;
        timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
        timeLabel_->setText(tr("%1s").arg(currentTimeSeconds_, 0, 'f', 2));
        syncAudioToCurrentTime();
        updatePreviewEffects();
        updateTitleOverlay();
        // Re-anchor the master clock to the new position so playback resumes
        // in sync rather than jumping by the seek delta.
        playbackStartSeconds_ = currentTimeSeconds_;
        playbackClock_.restart();
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

void MainWindow::onDeleteSelectedClip() {
    if (selectedClipId_.empty()) {
        statusLabel_->setText(tr("Select a clip on the timeline first"));
        return;
    }

    auto* timeline = activeTimeline();
    if (!timeline) return;

    const auto* clip = findClipById(selectedClipId_);
    std::optional<std::string> linkedId = clip ? clip->linkedClipId : std::nullopt;

    bool removed = false;
    for (const auto& track : timeline->tracks()) {
        if (track->removeClip(selectedClipId_)) removed = true;
        if (linkedId && track->removeClip(*linkedId)) removed = true;
    }

    if (!removed) {
        statusLabel_->setText(tr("Could not delete clip"));
        return;
    }

    selectedClipId_.clear();
    if (project_) project_->selectedClipId.clear();
    timelineWidget_->setSelectedClipId("");
    timelineWidget_->setTimeline(timeline);
    statusLabel_->setText(tr("Clip deleted"));
    markDirty();
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
    captureSessionState();
    std::string error;
    if (nova::project::ProjectStore::autosave(*project_, &error)) {
        NOVA_LOG_INFO(kModule, "Autosaved project");
    }
}

void MainWindow::onTemplateActivated(const QString& templatePath) {
    newProjectFromTemplate(templatePath);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (preview_ && previewOverlay_) {
        previewOverlay_->setGeometry(preview_->rect());
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == preview_ && event->type() == QEvent::Resize && previewOverlay_) {
        previewOverlay_->setGeometry(preview_->rect());
    }
    return QMainWindow::eventFilter(watched, event);
}

nova::timeline::Track* MainWindow::findOrAddTitleTrack(nova::timeline::Timeline* timeline) {
    if (!timeline) return nullptr;
    for (const auto& track : timeline->tracks()) {
        if (track->type() == nova::timeline::TrackType::Subtitle) {
            return track.get();
        }
    }
    return &timeline->addTrack("t1", "T1", nova::timeline::TrackType::Subtitle);
}

const nova::timeline::Clip* MainWindow::findClipById(const std::string& id) const {
    const auto* timeline = project_ ? project_->activeTimeline() : nullptr;
    if (!timeline || id.empty()) return nullptr;
    for (const auto& track : timeline->tracks()) {
        for (const auto& clip : track->clips()) {
            if (clip.id == id) return &clip;
        }
    }
    return nullptr;
}

const nova::timeline::Clip* MainWindow::findVideoClipAt(double seconds) const {
    const auto* timeline = project_ ? project_->activeTimeline() : nullptr;
    if (!timeline || timeline->tracks().empty()) return nullptr;
    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(seconds * timeline->frameRate()));
    for (const auto& track : timeline->tracks()) {
        if (track->type() != nova::timeline::TrackType::Video) continue;
        if (const auto* clip = track->findClipAt(frame)) return clip;
    }
    return nullptr;
}

const nova::timeline::Clip* MainWindow::findTitleClipAt(double seconds) const {
    const auto* timeline = project_ ? project_->activeTimeline() : nullptr;
    if (!timeline) return nullptr;
    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(seconds * timeline->frameRate()));
    for (const auto& track : timeline->tracks()) {
        if (track->type() != nova::timeline::TrackType::Subtitle) continue;
        if (const auto* clip = track->findClipAt(frame)) return clip;
    }
    return nullptr;
}

void MainWindow::updatePreviewEffects() {
    preview_->setClipOpacity(1.0f);
    preview_->setDipMix(0.0f, false);

    const auto* clip = findVideoClipAt(currentTimeSeconds_);
    if (!clip || !activeTimeline()) return;

    const double fps = activeTimeline()->frameRate();
    const double clipStart = clip->timelineStart / fps;
    const double clipEnd = clip->timelineEnd / fps;
    const double dur = std::max(0.05, clip->transitionDurationSec);
    float opacity = 1.0f;
    float dipMix = 0.0f;
    bool dipWhite = false;

    if (!clip->transitionIn.empty()) {
        const double t = (currentTimeSeconds_ - clipStart) / dur;
        if (t < 1.0) {
            if (clip->transitionIn == "dip-black") {
                dipMix = static_cast<float>(1.0 - t);
            } else if (clip->transitionIn == "dip-white") {
                dipMix = static_cast<float>(1.0 - t);
                dipWhite = true;
            } else {
                opacity = static_cast<float>(std::clamp(t, 0.0, 1.0));
            }
        }
    }

    if (!clip->transitionOut.empty()) {
        const double t = (clipEnd - currentTimeSeconds_) / dur;
        if (t < 1.0) {
            const auto& out = clip->transitionOut;
            if (out == "dip-black") {
                dipMix = std::max(dipMix, static_cast<float>(1.0 - t));
            } else if (out == "dip-white") {
                dipMix = std::max(dipMix, static_cast<float>(1.0 - t));
                dipWhite = true;
            } else {
                opacity = std::min(opacity, static_cast<float>(std::clamp(t, 0.0, 1.0)));
            }
        }
    }

    preview_->setClipOpacity(opacity);
    preview_->setDipMix(dipMix, dipWhite);
}

void MainWindow::updateTitleOverlay() {
    if (!previewOverlay_) return;
    if (const auto* title = findTitleClipAt(currentTimeSeconds_)) {
        previewOverlay_->setTitle(QString::fromStdString(title->overlayText),
                                  QString::fromStdString(title->stylePreset));
    } else {
        previewOverlay_->clearTitle();
    }
}

void MainWindow::onTextPresetActivated(const QString& presetId, const QString& defaultText) {
    auto* timeline = activeTimeline();
    if (!timeline) return;

    bool ok = false;
    const QString text = QInputDialog::getText(
        this, tr("Edit text"), tr("Title text:"), QLineEdit::Normal, defaultText, &ok);
    if (!ok || text.trimmed().isEmpty()) return;

    auto* titleTrack = findOrAddTitleTrack(timeline);
    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(currentTimeSeconds_ * timeline->frameRate()));
    const auto durationFrames = static_cast<nova::timeline::FrameNumber>(
        std::llround(4.0 * timeline->frameRate()));

    nova::timeline::Clip titleClip;
    titleClip.id = "title-" + presetId.toStdString() + "-" + std::to_string(frame);
    titleClip.name = text.toStdString();
    titleClip.type = nova::timeline::ClipType::Title;
    titleClip.overlayText = text.toStdString();
    titleClip.stylePreset = presetId.toStdString();
    titleClip.timelineStart = frame;
    titleClip.timelineEnd = frame + durationFrames;

    titleTrack->addClip(std::move(titleClip), true);
    timelineWidget_->setTimeline(timeline);
    updateTitleOverlay();
    statusLabel_->setText(tr("Added text: %1").arg(text));
    markDirty();
}

void MainWindow::onTransitionActivated(const QString& transitionId) {
    auto* timeline = activeTimeline();
    if (!timeline) return;

    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(currentTimeSeconds_ * timeline->frameRate()));

    nova::timeline::Clip* target = nullptr;
    bool applyIn = true;

    for (const auto& track : timeline->tracks()) {
        if (track->type() != nova::timeline::TrackType::Video) continue;
        for (const auto& candidate : track->clips()) {
            if (frame >= candidate.timelineStart && frame < candidate.timelineEnd) {
                target = track->findClip(candidate.id);
                const double fps = timeline->frameRate();
                const double clipStart = candidate.timelineStart / fps;
                const double clipEnd = candidate.timelineEnd / fps;
                applyIn = (currentTimeSeconds_ - clipStart) <= (clipEnd - currentTimeSeconds_);
                break;
            }
            if (candidate.timelineStart == frame) {
                target = track->findClip(candidate.id);
                applyIn = true;
                break;
            }
            if (candidate.timelineEnd == frame) {
                target = track->findClip(candidate.id);
                applyIn = false;
                break;
            }
        }
        if (target) break;
    }

    if (!target) {
        statusLabel_->setText(tr("Move playhead onto a video clip or cut point"));
        return;
    }

    const std::string id = transitionId.toStdString();
    if (applyIn) {
        target->transitionIn = id;
    } else {
        target->transitionOut = id;
    }
    target->transitionDurationSec = 0.5;

    timelineWidget_->setTimeline(timeline);
    updatePreviewEffects();
    statusLabel_->setText(tr("Applied %1").arg(transitionId));
    markDirty();
}

QString MainWindow::ensureStockAsset(const QString& assetId) {
    const QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/stock";
    QDir().mkpath(cacheDir);

    QColor color(0, 0, 0);
    QString fileName = assetId + ".png";
    if (assetId == "white-5s") color = QColor(255, 255, 255);
    else if (assetId == "blue-5s") color = QColor(30, 90, 200);
    else if (assetId == "gray-5s") color = QColor(120, 120, 125);
    else if (assetId == "red-5s") color = QColor(180, 40, 40);

    const QString path = cacheDir + "/" + fileName;
    if (!QFileInfo::exists(path)) {
        QImage img(1920, 1080, QImage::Format_RGB32);
        img.fill(color);
        img.save(path);
    }
    return path;
}

void MainWindow::onLibraryAssetActivated(const QString& assetId) {
    const QString path = ensureStockAsset(assetId);
    importMediaFile(path, QStringLiteral("Stock"));
    addMediaToTimeline(path);
    previewImageFile(path);
    statusLabel_->setText(tr("Inserted stock clip at playhead"));
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        onDeleteSelectedClip();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace nova::ui
