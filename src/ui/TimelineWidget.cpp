#include "nova/ui/TimelineWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>

namespace nova::ui {

using nova::timeline::Clip;
using nova::timeline::TrackType;

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
    setMouseTracking(true);
}

void TimelineWidget::setTimeline(nova::timeline::Timeline* timeline) {
    timeline_ = timeline;
    update();
}

void TimelineWidget::setPlayheadSeconds(double seconds) {
    playheadSeconds_ = seconds;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(30, 30, 34));

    if (!timeline_) {
        painter.setPen(QColor(140, 140, 145));
        painter.drawText(rect(), Qt::AlignCenter, tr("No sequence loaded"));
        return;
    }

    const double fps = timeline_->frameRate() > 0 ? timeline_->frameRate() : 30.0;
    int y = 4;

    for (const auto& track : timeline_->tracks()) {
        QRect headerRect(0, y, kHeaderWidth, kTrackHeight);
        painter.fillRect(headerRect, QColor(42, 42, 48));
        painter.setPen(QColor(210, 210, 215));
        painter.drawText(headerRect.adjusted(8, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          QString::fromStdString(track->name()));

        QRect laneRect(kHeaderWidth, y, width() - kHeaderWidth, kTrackHeight);
        painter.fillRect(laneRect, track->type() == TrackType::Video
                                        ? QColor(24, 26, 30)
                                        : QColor(22, 28, 26));

        for (const Clip& clip : track->clips()) {
            double startSec = clip.timelineStart / fps;
            double endSec = clip.timelineEnd / fps;
            int x = kHeaderWidth + static_cast<int>(startSec * pixelsPerSecond_);
            int w = std::max(2, static_cast<int>((endSec - startSec) * pixelsPerSecond_));

            QRect clipRect(x, y + 4, w, kTrackHeight - 8);
            QColor clipColor = track->type() == TrackType::Video
                                    ? QColor(70, 120, 200)
                                    : QColor(80, 170, 120);
            painter.setBrush(clipColor);
            painter.setPen(clipColor.darker(140));
            painter.drawRoundedRect(clipRect, 4, 4);

            painter.setPen(Qt::white);
            QFontMetrics fm(painter.font());
            QString elided = fm.elidedText(QString::fromStdString(clip.name), Qt::ElideRight,
                                            clipRect.width() - 8);
            painter.drawText(clipRect.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, elided);
        }

        y += kTrackHeight + kTrackSpacing;
    }

    // Playhead
    int playheadX = kHeaderWidth + static_cast<int>(playheadSeconds_ * pixelsPerSecond_);
    painter.setPen(QPen(QColor(230, 90, 60), 2));
    painter.drawLine(playheadX, 0, playheadX, height());
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (event->pos().x() < kHeaderWidth) return;
    double seconds = (event->pos().x() - kHeaderWidth) / pixelsPerSecond_;
    emit seekRequested(std::max(0.0, seconds));
}

} // namespace nova::ui
