#include "nova/ui/MainWindow.h"
#include "nova/ui/TimelineWidget.h"
#include "nova/renderer/VideoPreviewWidget.h"
#include "nova/core/Logger.h"

#include <QDockWidget>
#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QFileInfo>
#include <QMessageBox>
#include <algorithm>

namespace nova::ui {

namespace {
constexpr const char* kModule = "ui.MainWindow";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Nova Studio");
    resize(1400, 900);

    decoder_ = std::make_unique<nova::media::Decoder>();
    // A single default sequence: 1920x1080 @ 30fps, one video + one audio
    // track. Real projects would come from Project Manager / New Sequence
    // dialogs; this is the correct minimal default for the vertical slice.
    timeline_ = std::make_unique<nova::timeline::Timeline>("seq-1", "Sequence 01", 30.0, 1920, 1080);
    timeline_->addTrack("v1", "V1", nova::timeline::TrackType::Video);
    timeline_->addTrack("a1", "A1", nova::timeline::TrackType::Audio);

    preview_ = new nova::renderer::VideoPreviewWidget(this);
    setCentralWidget(preview_);

    buildDockPanels();
    buildMenus();

    connect(&playbackTimer_, &QTimer::timeout, this, &MainWindow::onPlaybackTick);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* importAction = fileMenu->addAction(tr("&Import Media..."));
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportMedia);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
}

void MainWindow::buildDockPanels() {
    // --- Media Bin ---
    auto* mediaDock = new QDockWidget(tr("Media Library"), this);
    mediaBin_ = new QListWidget(mediaDock);
    mediaDock->setWidget(mediaBin_);
    addDockWidget(Qt::LeftDockWidgetArea, mediaDock);
    connect(mediaBin_, &QListWidget::itemDoubleClicked, this, &MainWindow::onMediaItemActivated);

    // --- Inspector ---
    auto* inspectorDock = new QDockWidget(tr("Inspector"), this);
    auto* inspectorPanel = new QWidget(inspectorDock);
    auto* inspectorLayout = new QVBoxLayout(inspectorPanel);

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
    inspectorLayout->addStretch();

    connect(brightnessSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setBrightness);
    connect(contrastSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setContrast);
    connect(saturationSlider_, &QSlider::valueChanged, preview_,
            &nova::renderer::VideoPreviewWidget::setSaturation);

    inspectorDock->setWidget(inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    // --- Timeline (bottom) ---
    auto* timelineDock = new QDockWidget(tr("Timeline"), this);
    auto* timelinePanel = new QWidget(timelineDock);
    auto* timelineLayout = new QVBoxLayout(timelinePanel);

    auto* transportRow = new QWidget(timelinePanel);
    auto* transportLayout = new QHBoxLayout(transportRow);
    playButton_ = new QPushButton(tr("Play"), transportRow);
    statusLabel_ = new QLabel(tr("No clip loaded"), transportRow);
    transportLayout->addWidget(playButton_);
    transportLayout->addWidget(statusLabel_);
    transportLayout->addStretch();
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);

    timelineWidget_ = new TimelineWidget(timelinePanel);
    timelineWidget_->setTimeline(timeline_.get());
    connect(timelineWidget_, &TimelineWidget::seekRequested, this, &MainWindow::onTimelineSeek);

    timelineLayout->addWidget(transportRow);
    timelineLayout->addWidget(timelineWidget_);
    timelineDock->setWidget(timelinePanel);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
}

void MainWindow::onImportMedia() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Import Media"), QString(),
        tr("Video Files (*.mp4 *.mov *.mkv *.avi *.webm);;All Files (*)"));
    if (path.isEmpty()) return;

    auto* item = new QListWidgetItem(QFileInfo(path).fileName(), mediaBin_);
    item->setData(Qt::UserRole, path);
    mediaBin_->addItem(item);
}

void MainWindow::onMediaItemActivated() {
    auto* item = mediaBin_->currentItem();
    if (!item) return;
    loadMediaIntoPreview(item->data(Qt::UserRole).toString());
}

void MainWindow::loadMediaIntoPreview(const QString& path) {
    playbackTimer_.stop();
    playing_ = false;
    playButton_->setText(tr("Play"));

    if (!decoder_->open(path.toStdString())) {
        QMessageBox::warning(this, tr("Import failed"),
                              tr("Could not open media:\n%1").arg(QString::fromStdString(decoder_->lastError())));
        return;
    }

    const auto& info = decoder_->info();
    statusLabel_->setText(tr("%1 — %2x%3 @ %4 fps")
                               .arg(QFileInfo(path).fileName())
                               .arg(info.width)
                               .arg(info.height)
                               .arg(info.frameRate, 0, 'f', 2));

    // Place the clip on V1 starting at frame 0, spanning its full duration
    // at the sequence's frame rate. A real Import flow would let the user
    // drag it to an arbitrary timeline position; dropping it at 0 is the
    // correct default for "just imported a single clip."
    nova::timeline::Clip clip;
    clip.id = "clip-" + path.toStdString();
    clip.name = QFileInfo(path).fileName().toStdString();
    clip.type = nova::timeline::ClipType::Video;
    clip.mediaPath = path.toStdString();
    clip.sourceIn = 0;
    clip.sourceOut = static_cast<int64_t>(info.durationSeconds * info.frameRate);
    clip.timelineStart = 0;
    clip.timelineEnd = static_cast<int64_t>(info.durationSeconds * timeline_->frameRate());

    if (auto* v1 = timeline_->tracks().front().get()) {
        // Remove any previously loaded single clip for this simple slice.
        const auto& clips = v1->clips();
        if (!clips.empty()) v1->removeClip(clips.front().id);
        v1->addClip(clip);
    }
    timelineWidget_->setTimeline(timeline_.get());

    currentTimeSeconds_ = 0.0;
    decoder_->seek(0.0);
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
    }
    NOVA_LOG_INFO(kModule, "Loaded media into preview: " + path.toStdString());
}

void MainWindow::onPlayPauseClicked() {
    if (!decoder_->isOpen()) return;

    playing_ = !playing_;
    playButton_->setText(playing_ ? tr("Pause") : tr("Play"));

    if (playing_) {
        double intervalMs = 1000.0 / std::max(1.0, decoder_->info().frameRate);
        playbackTimer_.start(static_cast<int>(intervalMs));
    } else {
        playbackTimer_.stop();
    }
}

void MainWindow::onPlaybackTick() {
    auto frame = decoder_->nextFrame();
    if (!frame) {
        // End of stream: loop back to start, matching the "Loop playback"
        // requirement for the vertical slice.
        decoder_->seek(0.0);
        frame = decoder_->nextFrame();
        if (!frame) {
            playbackTimer_.stop();
            playing_ = false;
            playButton_->setText(tr("Play"));
            return;
        }
    }
    preview_->setFrame(*frame);
    currentTimeSeconds_ = frame->timeSeconds;
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
}

void MainWindow::onTimelineSeek(double seconds) {
    if (!decoder_->isOpen()) return;
    decoder_->seek(seconds);
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
        currentTimeSeconds_ = frame->timeSeconds;
        timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    }
}

} // namespace nova::ui
