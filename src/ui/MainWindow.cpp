#include "nova/ui/MainWindow.h"
#include "nova/ui/TimelineWidget.h"
#include "nova/renderer/VideoPreviewWidget.h"
#include "nova/core/Logger.h"
#include "nova/media/AudioExtractor.h"

#include <QAction>
#include <QAudioOutput>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QListWidget>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QUrl>
#include <QWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

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

    audioOutput_ = new QAudioOutput(this);
    audioOutput_->setVolume(0.85f);
    audioPlayer_ = new QMediaPlayer(this);
    audioPlayer_->setAudioOutput(audioOutput_);

    applyTheme();
    buildDockPanels();
    buildMenus();

    connect(&playbackTimer_, &QTimer::timeout, this, &MainWindow::onPlaybackTick);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* importAction = fileMenu->addAction(tr("&Import Media..."));
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportMedia);
    QAction* extractAction = fileMenu->addAction(tr("Extract &Audio..."));
    connect(extractAction, &QAction::triggered, this, &MainWindow::onExtractAudio);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* splitAction = editMenu->addAction(tr("&Split at Playhead"));
    splitAction->setShortcut(QKeySequence(Qt::Key_B));
    connect(splitAction, &QAction::triggered, this, &MainWindow::onSplitAtPlayhead);
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
    )");
}

void MainWindow::buildDockPanels() {
    // --- Media Bin ---
    auto* mediaDock = new QDockWidget(tr("Media Library"), this);
    mediaBin_ = new QListWidget(mediaDock);
    mediaBin_->setAlternatingRowColors(false);
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

    // --- Timeline (bottom) ---
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
    transportLayout->addWidget(playButton_);
    transportLayout->addWidget(splitButton_);
    transportLayout->addWidget(extractAudioButton_);
    transportLayout->addSpacing(12);
    transportLayout->addWidget(timeLabel_);
    transportLayout->addWidget(statusLabel_);
    transportLayout->addStretch();
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(splitButton_, &QPushButton::clicked, this, &MainWindow::onSplitAtPlayhead);
    connect(extractAudioButton_, &QPushButton::clicked, this, &MainWindow::onExtractAudio);

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
        tr("Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.aac *.flac);;All Files (*)"));
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
    if (audioPlayer_) {
        audioPlayer_->stop();
    }

    if (!decoder_->open(path.toStdString())) {
        QMessageBox::warning(this, tr("Import failed"),
                              tr("Could not open media:\n%1").arg(QString::fromStdString(decoder_->lastError())));
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
    timelineWidget_->setTimeline(timeline_.get());

    currentTimeSeconds_ = 0.0;
    decoder_->seek(0.0);
    if (auto frame = decoder_->nextFrame()) {
        preview_->setFrame(*frame);
    }
    timelineWidget_->setPlayheadSeconds(currentTimeSeconds_);
    timeLabel_->setText(tr("00:00.00"));
    NOVA_LOG_INFO(kModule, "Loaded media into preview: " + path.toStdString());
}

void MainWindow::addClipToTimeline(const QString& path) {
    const auto& info = decoder_->info();

    nova::timeline::Clip clip;
    clip.id = "clip-" + path.toStdString();
    clip.name = QFileInfo(path).fileName().toStdString();
    clip.type = nova::timeline::ClipType::Video;
    clip.mediaPath = path.toStdString();
    clip.sourceIn = 0;
    clip.sourceOut = static_cast<int64_t>(info.durationSeconds * info.frameRate);
    clip.timelineStart = 0;
    clip.timelineEnd = static_cast<int64_t>(info.durationSeconds * timeline_->frameRate());
    if (info.hasAudio) {
        clip.linkedClipId = "audio-" + path.toStdString();
    }

    if (!timeline_->tracks().empty()) {
        auto* v1 = timeline_->tracks().front().get();
        // Remove any previously loaded single clip for this simple slice.
        const auto& clips = v1->clips();
        if (!clips.empty()) v1->removeClip(clips.front().id);
        v1->addClip(clip);
    }

    if (info.hasAudio && timeline_->tracks().size() > 1) {
        nova::timeline::Clip audioClip = clip;
        audioClip.id = "audio-" + path.toStdString();
        audioClip.name = QFileInfo(path).completeBaseName().toStdString() + " audio";
        audioClip.type = nova::timeline::ClipType::Audio;
        audioClip.linkedClipId = clip.id;

        auto* a1 = timeline_->tracks()[1].get();
        const auto& clips = a1->clips();
        if (!clips.empty()) a1->removeClip(clips.front().id);
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
        double intervalMs = 1000.0 / std::max(1.0, decoder_->info().frameRate);
        playbackTimer_.start(static_cast<int>(intervalMs));
    } else {
        playbackTimer_.stop();
        if (audioPlayer_) {
            audioPlayer_->pause();
        }
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
            if (audioPlayer_) {
                audioPlayer_->stop();
            }
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
    if (audioPlayer_) {
        audioPlayer_->pause();
    }
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
    if (!timeline_ || timeline_->tracks().empty()) return;

    const auto frame = static_cast<nova::timeline::FrameNumber>(
        std::llround(currentTimeSeconds_ * timeline_->frameRate()));

    auto* v1 = timeline_->tracks().front().get();
    const auto* clip = v1->findClipAt(frame);
    if (!clip) {
        statusLabel_->setText(tr("No clip under playhead to split"));
        return;
    }

    const std::string rightId = clip->id + "-cut-" + std::to_string(frame);
    if (v1->splitClipAt(clip->id, frame, rightId)) {
        if (timeline_->tracks().size() > 1) {
            auto* a1 = timeline_->tracks()[1].get();
            if (const auto* audioClip = a1->findClipAt(frame)) {
                const std::string audioId = audioClip->id;
                a1->splitClipAt(audioId, frame, audioId + "-cut-" + std::to_string(frame));
            }
        }
        timelineWidget_->setTimeline(timeline_.get());
        statusLabel_->setText(tr("Split at %1s").arg(currentTimeSeconds_, 0, 'f', 2));
    }
}

void MainWindow::onExtractAudio() {
    if (currentMediaPath_.isEmpty()) {
        QMessageBox::information(this, tr("Extract Audio"), tr("Import a clip first."));
        return;
    }

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Extract Audio"),
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

} // namespace nova::ui
