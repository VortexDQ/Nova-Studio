#pragma once

#include <QWidget>
#include "nova/timeline/Timeline.h"

namespace nova::ui {

// Renders the tracks/clips of a nova::timeline::Timeline as horizontal bars.
// This is a real, working paintEvent-based renderer (not a stub) but
// intentionally does not yet implement drag/trim/blade interactions -
// see docs/ROADMAP.md for the interaction-layer milestone.
class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setTimeline(nova::timeline::Timeline* timeline);
    void setPlayheadSeconds(double seconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void seekRequested(double seconds);

private:
    static constexpr int kTrackHeight = 56;
    static constexpr int kTrackSpacing = 4;
    static constexpr int kHeaderWidth = 96;
    double pixelsPerSecond_ = 60.0;

    nova::timeline::Timeline* timeline_ = nullptr;
    double playheadSeconds_ = 0.0;
};

} // namespace nova::ui
