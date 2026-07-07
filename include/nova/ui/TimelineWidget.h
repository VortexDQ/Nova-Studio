#pragma once

#include <QWidget>
#include <optional>
#include <string>

#include <QKeyEvent>
#include <QPoint>

#include "nova/timeline/Timeline.h"

namespace nova::ui {

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setTimeline(nova::timeline::Timeline* timeline);
    void setPlayheadSeconds(double seconds);
    void setSelectedClipId(const std::string& clipId);
    void setTrimTool(nova::timeline::TrimTool tool);
    nova::timeline::TrimTool trimTool() const { return trimTool_; }
    std::optional<std::string> selectedClipId() const { return selectedClipId_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    void seekRequested(double seconds);
    void scrubbing(double seconds);
    void clipSelected(const QString& clipId, const QString& mediaPath);
    void selectionCleared();
    void deleteSelectedRequested();
    void clipEditRequested(const QString& clipId);
    void timelineEdited();
    void trimToolChanged(nova::timeline::TrimTool tool);

private:
    enum class DragMode {
        None,
        Playhead,
        Clip,
        PendingClip,
        TrimStart,
        TrimEnd,
        RollCut,
        Slip,
        Slide,
    };

    enum class TrimEdge { None, Start, End };

    struct HitClip {
        std::string id;
        std::string trackId;
        std::string mediaPath;
    };

    nova::timeline::FrameNumber frameFromX(int x) const;
    double secondsFromX(int x) const;
    int playheadX() const;
    bool isNearPlayhead(int x) const;
    std::optional<HitClip> hitTestClip(const QPoint& pos) const;
    std::optional<std::string> trackIdAtY(int y) const;
    QRect clipRect(const nova::timeline::Clip& clip, int y) const;
    int trackY(int trackIndex) const;
    void updateHeightForTracks();
    void applyClipDrag(const QPoint& pos);
    void finishClipDrag();
    void clearSelection();
    TrimEdge trimEdgeAt(const QPoint& pos) const;
    std::optional<nova::timeline::FrameNumber> rollCutAt(const QPoint& pos) const;
    void applyTrimDrag(const QPoint& pos);
    void applyRollDrag(const QPoint& pos);
    void applySlipDrag(const QPoint& pos);
    void applySlideDrag(const QPoint& pos);
    void drawTrimHandles(QPainter& painter, const QRect& rect, bool selected) const;
    bool clipDragEnabled() const;

    static constexpr int kTrackHeight = 56;
    static constexpr int kTrackSpacing = 4;
    static constexpr int kHeaderWidth = 96;
    static constexpr int kPlayheadHitPx = 10;
    static constexpr int kDragThresholdPx = 4;
    static constexpr int kTrimHandlePx = 8;
    double pixelsPerSecond_ = 60.0;

    nova::timeline::Timeline* timeline_ = nullptr;
    double playheadSeconds_ = 0.0;
    std::optional<std::string> selectedClipId_;
    nova::timeline::TrimTool trimTool_ = nova::timeline::TrimTool::Selection;

    DragMode dragMode_ = DragMode::None;
    QPoint pressPos_;
    std::string draggingClipId_;
    std::string draggingFromTrackId_;
    int dragGrabOffsetPx_ = 0;
    nova::timeline::FrameNumber trimAnchorFrame_ = 0;
    nova::timeline::FrameNumber slipSourceAtPress_ = 0;
    nova::timeline::FrameNumber slideStartAtPress_ = 0;
    nova::timeline::FrameNumber rollCutAtPress_ = 0;
};

} // namespace nova::ui
