#include "nova/ui/TimelineWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

namespace nova::ui {

using nova::timeline::Clip;
using nova::timeline::TrackType;

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(260);
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
    painter.fillRect(rect(), QColor(16, 18, 24));

    if (!timeline_) {
        painter.setPen(QColor(140, 140, 145));
        painter.drawText(rect(), Qt::AlignCenter, tr("No sequence loaded"));
        return;
    }

    const double fps = timeline_->frameRate() > 0 ? timeline_->frameRate() : 30.0;
    const int laneWidth = std::max(0, width() - kHeaderWidth);
    int y = 4;

    painter.setPen(QColor(70, 78, 92));
    for (int second = 0; second <= std::max(1, laneWidth / static_cast<int>(pixelsPerSecond_) + 2); ++second) {
        const int x = kHeaderWidth + static_cast<int>(second * pixelsPerSecond_);
        painter.drawLine(x, 0, x, height());
        painter.setPen(QColor(135, 143, 158));
        painter.drawText(x + 4, 14, QString::number(second) + "s");
        painter.setPen(QColor(45, 51, 62));
    }

    for (const auto& track : timeline_->tracks()) {
        QRect headerRect(0, y, kHeaderWidth, kTrackHeight);
        painter.fillRect(headerRect, QColor(28, 32, 41));
        painter.setPen(QColor(210, 210, 215));
        painter.drawText(headerRect.adjusted(8, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          QString::fromStdString(track->name()));

        QRect laneRect(kHeaderWidth, y, width() - kHeaderWidth, kTrackHeight);
        painter.fillRect(laneRect, track->type() == TrackType::Video
                                        ? QColor(19, 22, 29)
                                        : QColor(17, 26, 24));

        for (const Clip& clip : track->clips()) {
            double startSec = clip.timelineStart / fps;
            double endSec = clip.timelineEnd / fps;
            int x = kHeaderWidth + static_cast<int>(startSec * pixelsPerSecond_);
            int w = std::max(2, static_cast<int>((endSec - startSec) * pixelsPerSecond_));

            QRect clipRect(x, y + 4, w, kTrackHeight - 8);
            QColor clipColor = track->type() == TrackType::Video
                                    ? QColor(64, 105, 210)
                                    : QColor(46, 168, 125);
            painter.setBrush(clipColor);
            painter.setPen(QPen(clipColor.lighter(125), 1));
            painter.drawRoundedRect(clipRect, 4, 4);

            if (track->type() == TrackType::Audio) {
                painter.setPen(QColor(190, 255, 225, 130));
                const int centerY = clipRect.center().y();
                for (int px = clipRect.left() + 6; px < clipRect.right() - 4; px += 5) {
                    const int amp = 4 + static_cast<int>(std::abs(std::sin(px * 0.17)) * (clipRect.height() / 2 - 7));
                    painter.drawLine(px, centerY - amp, px, centerY + amp);
                }
            }

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
