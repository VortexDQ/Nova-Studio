#include "nova/ui/TimelineWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

namespace nova::ui {

using nova::timeline::Clip;
using nova::timeline::ClipType;
using nova::timeline::FrameNumber;
using nova::timeline::TrackType;
using nova::timeline::TrimTool;

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(260);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void TimelineWidget::setTimeline(nova::timeline::Timeline* timeline) {
    timeline_ = timeline;
    updateHeightForTracks();
    update();
}

void TimelineWidget::setPlayheadSeconds(double seconds) {
    playheadSeconds_ = std::max(0.0, seconds);
    update();
}

void TimelineWidget::setSelectedClipId(const std::string& clipId) {
    selectedClipId_ = clipId.empty() ? std::nullopt : std::optional<std::string>(clipId);
    update();
}

void TimelineWidget::setTrimTool(TrimTool tool) {
    if (trimTool_ == tool) return;
    trimTool_ = tool;
    update();
    emit trimToolChanged(tool);
}

void TimelineWidget::updateHeightForTracks() {
    const int trackCount = timeline_ ? static_cast<int>(timeline_->tracks().size()) : 2;
    setMinimumHeight(24 + trackCount * (kTrackHeight + kTrackSpacing) + 16);
}

FrameNumber TimelineWidget::frameFromX(int x) const {
    if (!timeline_) return 0;
    const double fps = std::max(1.0, timeline_->frameRate());
    return std::max<FrameNumber>(0, static_cast<FrameNumber>(std::llround(secondsFromX(x) * fps)));
}

double TimelineWidget::secondsFromX(int x) const {
    return std::max(0.0, (x - kHeaderWidth) / pixelsPerSecond_);
}

int TimelineWidget::playheadX() const {
    return kHeaderWidth + static_cast<int>(playheadSeconds_ * pixelsPerSecond_);
}

bool TimelineWidget::isNearPlayhead(int x) const {
    return std::abs(x - playheadX()) <= kPlayheadHitPx;
}

int TimelineWidget::trackY(int trackIndex) const {
    return 4 + trackIndex * (kTrackHeight + kTrackSpacing);
}

QRect TimelineWidget::clipRect(const Clip& clip, int y) const {
    if (!timeline_) return {};
    const double fps = timeline_->frameRate() > 0 ? timeline_->frameRate() : 30.0;
    const double startSec = clip.timelineStart / fps;
    const double endSec = clip.timelineEnd / fps;
    const int x = kHeaderWidth + static_cast<int>(startSec * pixelsPerSecond_);
    const int w = std::max(2, static_cast<int>((endSec - startSec) * pixelsPerSecond_));
    return QRect(x, y + 4, w, kTrackHeight - 8);
}

bool TimelineWidget::clipDragEnabled() const {
    return trimTool_ == TrimTool::Selection;
}

TimelineWidget::TrimEdge TimelineWidget::trimEdgeAt(const QPoint& pos) const {
    if (!timeline_ || trimTool_ == TrimTool::Roll || trimTool_ == TrimTool::Slip
        || trimTool_ == TrimTool::Slide) {
        return TrimEdge::None;
    }

    int trackIndex = 0;
    for (const auto& track : timeline_->tracks()) {
        const int y = trackY(trackIndex);
        for (const Clip& clip : track->clips()) {
            const QRect rect = clipRect(clip, y);
            if (!rect.contains(pos)) continue;
            if (pos.x() - rect.left() <= kTrimHandlePx) return TrimEdge::Start;
            if (rect.right() - pos.x() <= kTrimHandlePx) return TrimEdge::End;
            return TrimEdge::None;
        }
        ++trackIndex;
    }
    return TrimEdge::None;
}

std::optional<FrameNumber> TimelineWidget::rollCutAt(const QPoint& pos) const {
    if (!timeline_ || trimTool_ != TrimTool::Roll) return std::nullopt;

    int trackIndex = 0;
    for (const auto& track : timeline_->tracks()) {
        const int y = trackY(trackIndex);
        if (pos.y() < y || pos.y() >= y + kTrackHeight) {
            ++trackIndex;
            continue;
        }
        for (const Clip& clip : track->clips()) {
            const QRect rect = clipRect(clip, y);
            const int cutX = rect.right();
            if (std::abs(pos.x() - cutX) <= kTrimHandlePx) {
                return clip.timelineEnd;
            }
        }
        ++trackIndex;
    }
    return std::nullopt;
}

std::optional<TimelineWidget::HitClip> TimelineWidget::hitTestClip(const QPoint& pos) const {
    if (!timeline_ || pos.x() < kHeaderWidth) return std::nullopt;

    int trackIndex = 0;
    for (const auto& track : timeline_->tracks()) {
        const int y = trackY(trackIndex);
        for (const Clip& clip : track->clips()) {
            if (clipRect(clip, y).contains(pos)) {
                return HitClip{clip.id, track->id(), clip.mediaPath};
            }
        }
        ++trackIndex;
    }
    return std::nullopt;
}

std::optional<std::string> TimelineWidget::trackIdAtY(int y) const {
    if (!timeline_) return std::nullopt;
    int trackIndex = 0;
    for (const auto& track : timeline_->tracks()) {
        const int top = trackY(trackIndex);
        if (y >= top && y < top + kTrackHeight) {
            return track->id();
        }
        ++trackIndex;
    }
    return std::nullopt;
}

void TimelineWidget::drawTrimHandles(QPainter& painter, const QRect& rect, bool selected) const {
    if (!selected) return;
    painter.setBrush(QColor(255, 220, 120));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect.left(), rect.top() + 4, 4, rect.height() - 8);
    painter.drawRect(rect.right() - 3, rect.top() + 4, 4, rect.height() - 8);
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(16, 18, 24));

    if (!timeline_) {
        painter.setPen(QColor(140, 140, 145));
        painter.drawText(rect(), Qt::AlignCenter, tr("No sequence loaded"));
        return;
    }

    const int laneWidth = std::max(0, width() - kHeaderWidth);

    painter.setPen(QColor(70, 78, 92));
    for (int second = 0; second <= std::max(1, laneWidth / static_cast<int>(pixelsPerSecond_) + 2);
         ++second) {
        const int x = kHeaderWidth + static_cast<int>(second * pixelsPerSecond_);
        painter.drawLine(x, 0, x, height());
        painter.setPen(QColor(135, 143, 158));
        painter.drawText(x + 4, 14, QString::number(second) + "s");
        painter.setPen(QColor(45, 51, 62));
    }

    int trackIndex = 0;
    for (const auto& track : timeline_->tracks()) {
        const int y = trackY(trackIndex);
        QRect headerRect(0, y, kHeaderWidth, kTrackHeight);
        painter.fillRect(headerRect, QColor(28, 32, 41));
        painter.setPen(QColor(210, 210, 215));
        painter.drawText(headerRect.adjusted(8, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         QString::fromStdString(track->name()));

        QRect laneRect(kHeaderWidth, y, width() - kHeaderWidth, kTrackHeight);
        painter.fillRect(laneRect, track->type() == TrackType::Video
                                        ? QColor(19, 22, 29)
                                        : track->type() == TrackType::Subtitle
                                              ? QColor(24, 22, 18)
                                              : QColor(17, 26, 24));

        for (const Clip& clip : track->clips()) {
            const QRect rect = clipRect(clip, y);
            const bool selected = selectedClipId_ && *selectedClipId_ == clip.id;
            const bool dragging = (dragMode_ == DragMode::Clip || dragMode_ == DragMode::Slide
                                   || dragMode_ == DragMode::Slip || dragMode_ == DragMode::TrimStart
                                   || dragMode_ == DragMode::TrimEnd)
                                  && draggingClipId_ == clip.id;
            const bool pending = dragMode_ == DragMode::PendingClip && draggingClipId_ == clip.id;

            QColor clipColor = track->type() == TrackType::Video
                                   ? QColor(64, 105, 210)
                                   : track->type() == TrackType::Subtitle
                                         ? QColor(210, 130, 50)
                                         : QColor(46, 168, 125);
            if (selected || dragging || pending) clipColor = clipColor.lighter(130);

            painter.setBrush(clipColor);
            painter.setPen(QPen(selected || dragging || pending ? QColor(255, 220, 120)
                                                                : clipColor.lighter(125),
                                selected || dragging || pending ? 2 : 1));
            painter.drawRoundedRect(rect, 4, 4);

            if (trimTool_ == TrimTool::Selection || trimTool_ == TrimTool::Ripple) {
                drawTrimHandles(painter, rect, selected);
            }

            if (track->type() == TrackType::Audio) {
                painter.setPen(QColor(190, 255, 225, 130));
                const int centerY = rect.center().y();
                for (int px = rect.left() + 6; px < rect.right() - 4; px += 5) {
                    const int amp = 4 + static_cast<int>(std::abs(std::sin(px * 0.17))
                                                         * (rect.height() / 2 - 7));
                    painter.drawLine(px, centerY - amp, px, centerY + amp);
                }
            }

            painter.setPen(Qt::white);
            QFontMetrics fm(painter.font());
            const QString label = clip.type == ClipType::Title
                                      ? QString::fromStdString(
                                            clip.overlayText.empty() ? clip.name : clip.overlayText)
                                      : QString::fromStdString(clip.name);
            const QString elided = fm.elidedText(label, Qt::ElideRight, rect.width() - 8);
            painter.drawText(rect.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, elided);
        }

        ++trackIndex;
    }

    const int phX = playheadX();
    painter.setPen(QPen(QColor(230, 90, 60), 2));
    painter.drawLine(phX, 0, phX, height());
    painter.setBrush(QColor(230, 90, 60));
    painter.setPen(Qt::NoPen);
    const QPoint handle[3] = {
        QPoint(phX - 6, 0),
        QPoint(phX + 6, 0),
        QPoint(phX, 10),
    };
    painter.drawPolygon(handle, 3);
}

void TimelineWidget::clearSelection() {
    if (!selectedClipId_) return;
    selectedClipId_.reset();
    update();
    emit selectionCleared();
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (!timeline_ || event->button() != Qt::LeftButton) return;
    setFocus();
    pressPos_ = event->pos();

    if (event->pos().x() >= kHeaderWidth && isNearPlayhead(event->pos().x())) {
        dragMode_ = DragMode::Playhead;
        setCursor(Qt::SizeHorCursor);
        const double seconds = secondsFromX(event->pos().x());
        playheadSeconds_ = seconds;
        update();
        emit scrubbing(seconds);
        return;
    }

    if (trimTool_ == TrimTool::Roll) {
        if (const auto cut = rollCutAt(event->pos())) {
            dragMode_ = DragMode::RollCut;
            rollCutAtPress_ = *cut;
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }

    const TrimEdge edge = trimEdgeAt(event->pos());
    if (edge != TrimEdge::None) {
        if (const auto hit = hitTestClip(event->pos())) {
            dragMode_ = edge == TrimEdge::Start ? DragMode::TrimStart : DragMode::TrimEnd;
            draggingClipId_ = hit->id;
            draggingFromTrackId_ = hit->trackId;
            if (const auto ref = timeline_->findClipRef(hit->id)) {
                trimAnchorFrame_ = edge == TrimEdge::Start ? ref->clip->timelineStart
                                                           : ref->clip->timelineEnd;
            }
            selectedClipId_ = hit->id;
            update();
            emit clipSelected(QString::fromStdString(hit->id),
                              QString::fromStdString(hit->mediaPath));
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }

    if (const auto hit = hitTestClip(event->pos())) {
        if (trimTool_ == TrimTool::Slip) {
            dragMode_ = DragMode::Slip;
            draggingClipId_ = hit->id;
            if (const auto ref = timeline_->findClipRef(hit->id)) {
                slipSourceAtPress_ = ref->clip->sourceIn;
            }
            selectedClipId_ = hit->id;
            update();
            emit clipSelected(QString::fromStdString(hit->id),
                              QString::fromStdString(hit->mediaPath));
            setCursor(Qt::SizeHorCursor);
            return;
        }
        if (trimTool_ == TrimTool::Slide) {
            dragMode_ = DragMode::Slide;
            draggingClipId_ = hit->id;
            int trackIndex = 0;
            for (const auto& track : timeline_->tracks()) {
                if (track->id() == hit->trackId) {
                    if (const Clip* clip = track->findClip(hit->id)) {
                        slideStartAtPress_ = clip->timelineStart;
                        dragGrabOffsetPx_ =
                            event->pos().x() - clipRect(*clip, trackY(trackIndex)).left();
                    }
                    break;
                }
                ++trackIndex;
            }
            selectedClipId_ = hit->id;
            update();
            emit clipSelected(QString::fromStdString(hit->id),
                              QString::fromStdString(hit->mediaPath));
            setCursor(Qt::ClosedHandCursor);
            return;
        }

        if (!clipDragEnabled()) {
            selectedClipId_ = hit->id;
            update();
            emit clipSelected(QString::fromStdString(hit->id),
                              QString::fromStdString(hit->mediaPath));
            return;
        }

        dragMode_ = DragMode::PendingClip;
        draggingClipId_ = hit->id;
        draggingFromTrackId_ = hit->trackId;

        int trackIndex = 0;
        for (const auto& track : timeline_->tracks()) {
            if (track->id() == hit->trackId) {
                if (const Clip* clip = track->findClip(hit->id)) {
                    dragGrabOffsetPx_ = event->pos().x() - clipRect(*clip, trackY(trackIndex)).left();
                }
                break;
            }
            ++trackIndex;
        }

        selectedClipId_ = hit->id;
        update();
        emit clipSelected(QString::fromStdString(hit->id),
                          QString::fromStdString(hit->mediaPath));
        return;
    }

    if (event->pos().x() >= kHeaderWidth) {
        clearSelection();
        dragMode_ = DragMode::Playhead;
        const double seconds = secondsFromX(event->pos().x());
        playheadSeconds_ = seconds;
        update();
        emit scrubbing(seconds);
    }
}

void TimelineWidget::applyClipDrag(const QPoint& pos) {
    if (!timeline_ || draggingClipId_.empty()) return;

    const double fps = std::max(1.0, timeline_->frameRate());
    const int timelineX = pos.x() - dragGrabOffsetPx_;
    auto newStart = static_cast<FrameNumber>(std::llround(secondsFromX(timelineX) * fps));
    newStart = std::max<FrameNumber>(0, newStart);

    std::string destTrackId = draggingFromTrackId_;
    if (const auto ref = timeline_->findClipRef(draggingClipId_)) {
        if (const auto trackId = trackIdAtY(pos.y())) {
            if (auto* track = timeline_->findTrackById(*trackId)) {
                if (nova::timeline::Timeline::clipMatchesTrack(ref->clip->type, track->type())) {
                    destTrackId = *trackId;
                }
            }
        }
    }

    if (timeline_->moveClip(draggingClipId_, destTrackId, newStart, true)) {
        draggingFromTrackId_ = destTrackId;
    }
    update();
    emit timelineEdited();
}

void TimelineWidget::applyTrimDrag(const QPoint& pos) {
    if (!timeline_ || draggingClipId_.empty()) return;
    const FrameNumber frame = frameFromX(pos.x());
    bool ok = false;
    if (dragMode_ == DragMode::TrimStart) {
        ok = timeline_->trimClipStartWithTool(draggingClipId_, frame, trimTool_);
    } else if (dragMode_ == DragMode::TrimEnd) {
        ok = timeline_->trimClipEndWithTool(draggingClipId_, frame, trimTool_);
    }
    if (ok) {
        update();
        emit timelineEdited();
    }
}

void TimelineWidget::applyRollDrag(const QPoint& pos) {
    if (!timeline_) return;
    const FrameNumber newCut = frameFromX(pos.x());
    if (timeline_->rollTrimAtCut(rollCutAtPress_, newCut)) {
        rollCutAtPress_ = newCut;
        update();
        emit timelineEdited();
    }
}

void TimelineWidget::applySlipDrag(const QPoint& pos) {
    if (!timeline_ || draggingClipId_.empty()) return;
    const FrameNumber delta = frameFromX(pos.x()) - frameFromX(pressPos_.x());
    const FrameNumber newSourceIn = slipSourceAtPress_ + delta;
    if (timeline_->slipTrimClipTo(draggingClipId_, newSourceIn)) {
        update();
        emit timelineEdited();
    }
}

void TimelineWidget::applySlideDrag(const QPoint& pos) {
    if (!timeline_ || draggingClipId_.empty()) return;
    const FrameNumber newStart = frameFromX(pos.x() - dragGrabOffsetPx_);
    if (timeline_->slideClipOnTimeline(draggingClipId_, newStart)) {
        update();
        emit timelineEdited();
    }
}

void TimelineWidget::finishClipDrag() {
    draggingClipId_.clear();
    draggingFromTrackId_.clear();
    dragGrabOffsetPx_ = 0;
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!timeline_) return;

    if (dragMode_ == DragMode::PendingClip) {
        const QPoint delta = event->pos() - pressPos_;
        if (std::abs(delta.x()) >= kDragThresholdPx || std::abs(delta.y()) >= kDragThresholdPx) {
            dragMode_ = DragMode::Clip;
            setCursor(Qt::ClosedHandCursor);
            applyClipDrag(event->pos());
        }
        return;
    }

    if (dragMode_ == DragMode::Playhead) {
        const double seconds = secondsFromX(event->pos().x());
        playheadSeconds_ = seconds;
        update();
        emit scrubbing(seconds);
        return;
    }

    if (dragMode_ == DragMode::Clip) {
        applyClipDrag(event->pos());
        return;
    }

    if (dragMode_ == DragMode::TrimStart || dragMode_ == DragMode::TrimEnd) {
        applyTrimDrag(event->pos());
        return;
    }

    if (dragMode_ == DragMode::RollCut) {
        applyRollDrag(event->pos());
        return;
    }

    if (dragMode_ == DragMode::Slip) {
        applySlipDrag(event->pos());
        return;
    }

    if (dragMode_ == DragMode::Slide) {
        applySlideDrag(event->pos());
        return;
    }

    if (event->pos().x() >= kHeaderWidth) {
        if (isNearPlayhead(event->pos().x())) {
            setCursor(Qt::SizeHorCursor);
        } else if (trimTool_ == TrimTool::Roll && rollCutAt(event->pos())) {
            setCursor(Qt::SizeHorCursor);
        } else if (trimEdgeAt(event->pos()) != TrimEdge::None) {
            setCursor(Qt::SizeHorCursor);
        } else if (hitTestClip(event->pos())) {
            if (trimTool_ == TrimTool::Slip || trimTool_ == TrimTool::Slide) {
                setCursor(Qt::SizeHorCursor);
            } else if (clipDragEnabled()) {
                setCursor(Qt::OpenHandCursor);
            } else {
                unsetCursor();
            }
        } else {
            unsetCursor();
        }
    }
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    if (dragMode_ == DragMode::Clip || dragMode_ == DragMode::TrimStart
        || dragMode_ == DragMode::TrimEnd || dragMode_ == DragMode::RollCut
        || dragMode_ == DragMode::Slip || dragMode_ == DragMode::Slide) {
        finishClipDrag();
        emit timelineEdited();
    } else if (dragMode_ == DragMode::PendingClip) {
        // Simple click: selection already applied in mousePressEvent.
    } else if (dragMode_ == DragMode::Playhead) {
        emit seekRequested(playheadSeconds_);
    }

    dragMode_ = DragMode::None;
    unsetCursor();
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!timeline_ || event->button() != Qt::LeftButton) return;

    if (const auto hit = hitTestClip(event->pos())) {
        if (const auto ref = timeline_->findClipRef(hit->id)) {
            if (ref->clip->type == ClipType::Title) {
                selectedClipId_ = hit->id;
                update();
                emit clipSelected(QString::fromStdString(hit->id),
                                  QString::fromStdString(hit->mediaPath));
                emit clipEditRequested(QString::fromStdString(hit->id));
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimelineWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (selectedClipId_) {
            emit deleteSelectedRequested();
            event->accept();
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_V:
        setTrimTool(TrimTool::Selection);
        event->accept();
        return;
    case Qt::Key_R:
        setTrimTool(TrimTool::Ripple);
        event->accept();
        return;
    case Qt::Key_N:
        setTrimTool(TrimTool::Roll);
        event->accept();
        return;
    case Qt::Key_Y:
        setTrimTool(TrimTool::Slip);
        event->accept();
        return;
    case Qt::Key_U:
        setTrimTool(TrimTool::Slide);
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

} // namespace nova::ui
