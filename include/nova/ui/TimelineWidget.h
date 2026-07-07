#pragma once

#include <QWidget>
#include <optional>
#include <string>

#include "nova/timeline/Timeline.h"

namespace nova::ui {

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setTimeline(nova::timeline::Timeline* timeline);
    void setPlayheadSeconds(double seconds);
    void setSelectedClipId(const std::string& clipId);
    std::optional<std::string> selectedClipId() const { return selectedClipId_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void seekRequested(double seconds);
    void clipSelected(const QString& clipId, const QString& mediaPath);

private:
    struct HitClip {
        std::string id;
        std::string mediaPath;
    };
    std::optional<HitClip> hitTestClip(const QPoint& pos) const;
    QRect clipRect(const nova::timeline::Clip& clip, nova::timeline::TrackType type, int y) const;

    static constexpr int kTrackHeight = 56;
    static constexpr int kTrackSpacing = 4;
    static constexpr int kHeaderWidth = 96;
    double pixelsPerSecond_ = 60.0;

    nova::timeline::Timeline* timeline_ = nullptr;
    double playheadSeconds_ = 0.0;
    std::optional<std::string> selectedClipId_;
};

} // namespace nova::ui
