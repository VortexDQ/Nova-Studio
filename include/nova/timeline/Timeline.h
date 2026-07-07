#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nova::timeline {

// Frame-accurate time value. Storing as an integer frame count (rather than
// floating point seconds) is what real NLEs do to guarantee frame accuracy
// under trimming/rippling; the frame rate is carried alongside on the
// Timeline that owns the value.
using FrameNumber = int64_t;

enum class TrackType { Video, Audio, Subtitle };

enum class ClipType { Video, Audio, Image, AdjustmentLayer, Title, Nested };

// Premiere-style trim tools for timeline edge edits.
enum class TrimTool { Selection, Ripple, Roll, Slip, Slide };

// A single clip placed on a track. `sourceIn`/`sourceOut` index into the
// referenced media at its native frame rate; `timelineStart`/`timelineEnd`
// place the clip on the parent track's frame axis. This split is what makes
// ripple/roll/slip/slide well-defined operations rather than ad-hoc math.
struct Clip {
    std::string id;
    std::string name;
    ClipType type = ClipType::Video;
    std::string mediaPath;      // empty for adjustment layers / titles

    FrameNumber sourceIn = 0;
    FrameNumber sourceOut = 0;  // exclusive
    FrameNumber timelineStart = 0;
    FrameNumber timelineEnd = 0; // exclusive

    double speed = 1.0;         // 1.0 = normal, negative = reverse
    bool locked = false;
    bool muted = false;
    int colorLabel = 0;
    std::optional<std::string> linkedClipId; // e.g. matching audio for a video clip

    std::string overlayText;       // title/caption content
    std::string stylePreset;       // e.g. lower-third-minimal
    std::string transitionIn;      // fade-in, dip-black, ...
    std::string transitionOut;     // fade-out, cross-dissolve, ...
    double transitionDurationSec = 0.5;

    double rotationDegrees = 0.0;
    bool chromaKeyEnabled = false;
    double chromaKeyStrength = 0.6;

    FrameNumber duration() const { return timelineEnd - timelineStart; }
};

class Track {
public:
    Track(std::string id, std::string name, TrackType type)
        : id_(std::move(id)), name_(std::move(name)), type_(type) {}

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    TrackType type() const { return type_; }

    bool locked() const { return locked_; }
    void setLocked(bool locked) { locked_ = locked; }
    bool muted() const { return muted_; }
    void setMuted(bool muted) { muted_ = muted; }
    bool solo() const { return solo_; }
    void setSolo(bool solo) { solo_ = solo; }

    const std::vector<Clip>& clips() const { return clips_; }

    // Inserts a clip, keeping clips_ sorted by timelineStart. Returns false
    // (and does not insert) if the clip overlaps an existing clip and
    // `allowOverlap` is false.
    bool addClip(Clip clip, bool allowOverlap = false) {
        if (!allowOverlap) {
            for (const auto& existing : clips_) {
                if (clip.timelineStart < existing.timelineEnd &&
                    existing.timelineStart < clip.timelineEnd) {
                    return false; // overlap
                }
            }
        }
        auto it = std::lower_bound(
            clips_.begin(), clips_.end(), clip,
            [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
        clips_.insert(it, std::move(clip));
        return true;
    }

    bool removeClip(const std::string& clipId) {
        auto it = std::find_if(clips_.begin(), clips_.end(),
                                [&](const Clip& c) { return c.id == clipId; });
        if (it == clips_.end()) return false;
        clips_.erase(it);
        return true;
    }

    Clip* findClip(const std::string& clipId) {
        auto it = std::find_if(clips_.begin(), clips_.end(),
                                [&](const Clip& c) { return c.id == clipId; });
        return it == clips_.end() ? nullptr : &(*it);
    }

    const Clip* findClipAt(FrameNumber frame) const {
        auto it = std::find_if(clips_.begin(), clips_.end(),
                               [&](const Clip& c) {
                                   return frame >= c.timelineStart && frame < c.timelineEnd;
                               });
        return it == clips_.end() ? nullptr : &(*it);
    }

    // Blade/split edit: cut a clip into left/right halves at `frame`.
    // Returns false when the cut is outside the clip or exactly on an edge.
    bool splitClipAt(const std::string& clipId, FrameNumber frame, std::string newClipId) {
        auto it = std::find_if(clips_.begin(), clips_.end(),
                               [&](const Clip& c) { return c.id == clipId; });
        if (it == clips_.end() || frame <= it->timelineStart || frame >= it->timelineEnd) {
            return false;
        }

        Clip right = *it;
        right.id = std::move(newClipId);
        right.name += " (cut)";
        right.sourceIn += frame - it->timelineStart;
        right.timelineStart = frame;

        it->sourceOut = it->sourceIn + (frame - it->timelineStart);
        it->timelineEnd = frame;

        clips_.insert(std::next(it), std::move(right));
        return true;
    }

    // Ripple trim: change clip end and shift every later clip on this track.
    bool rippleTrimEnd(const std::string& clipId, FrameNumber newEnd) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked || newEnd <= clip->timelineStart) return false;
        const FrameNumber oldEnd = clip->timelineEnd;
        const FrameNumber delta = newEnd - oldEnd;
        clip->timelineEnd = newEnd;
        clip->sourceOut += delta;
        for (auto& other : clips_) {
            if (other.id != clipId && other.timelineStart >= oldEnd) {
                other.timelineStart += delta;
                other.timelineEnd += delta;
            }
        }
        return true;
    }

    // Ripple trim start: move the in-point and ripple the rest of the sequence.
    bool rippleTrimStart(const std::string& clipId, FrameNumber newStart) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked || newStart < 0 || newStart >= clip->timelineEnd) {
            return false;
        }
        const FrameNumber oldStart = clip->timelineStart;
        const FrameNumber oldEnd = clip->timelineEnd;
        const FrameNumber delta = newStart - oldStart;
        if (delta == 0) return true;

        clip->timelineStart = newStart;
        clip->sourceIn += delta;
        clip->timelineEnd = oldEnd - delta;
        for (auto& other : clips_) {
            if (other.id != clipId && other.timelineStart >= oldEnd) {
                other.timelineStart -= delta;
                other.timelineEnd -= delta;
            }
        }
        return true;
    }

    // Roll trim: move the cut between two abutting clips without changing sequence length.
    bool rollTrimAt(FrameNumber oldCut, FrameNumber newCut) {
        if (newCut == oldCut) return true;
        Clip* left = nullptr;
        Clip* right = nullptr;
        for (auto& clip : clips_) {
            if (clip.timelineEnd == oldCut) left = &clip;
            if (clip.timelineStart == oldCut) right = &clip;
        }
        if (!left || !right || left->locked || right->locked) return false;
        if (newCut <= left->timelineStart || newCut >= right->timelineEnd) return false;

        const FrameNumber delta = newCut - oldCut;
        left->timelineEnd = newCut;
        left->sourceOut = left->sourceIn + (left->timelineEnd - left->timelineStart);

        right->timelineStart = newCut;
        right->sourceIn += delta;
        return true;
    }

    // Slip trim: shift source in/out while keeping timeline placement fixed.
    bool slipTrim(const std::string& clipId, FrameNumber sourceDelta) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked || sourceDelta == 0) return false;
        const FrameNumber newIn = clip->sourceIn + sourceDelta;
        const FrameNumber newOut = clip->sourceOut + sourceDelta;
        if (newIn < 0 || newOut <= newIn) return false;
        clip->sourceIn = newIn;
        clip->sourceOut = newOut;
        return true;
    }

    bool slipTrimTo(const std::string& clipId, FrameNumber newSourceIn) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked) return false;
        const FrameNumber duration = clip->sourceOut - clip->sourceIn;
        if (newSourceIn < 0 || duration <= 0) return false;
        clip->sourceIn = newSourceIn;
        clip->sourceOut = newSourceIn + duration;
        return true;
    }

    // Slide trim: move a clip between two neighbors; neighbors absorb the delta.
    bool slideClip(const std::string& clipId, FrameNumber newStart) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked) return false;

        const FrameNumber oldStart = clip->timelineStart;
        const FrameNumber oldEnd = clip->timelineEnd;
        const FrameNumber duration = clip->duration();
        if (duration <= 0) return false;

        newStart = std::max<FrameNumber>(0, newStart);
        const FrameNumber newEnd = newStart + duration;
        if (newStart == oldStart) return true;

        Clip* prev = nullptr;
        Clip* next = nullptr;
        for (auto& other : clips_) {
            if (other.id == clipId) continue;
            if (other.timelineEnd == oldStart) prev = &other;
            if (other.timelineStart == oldEnd) next = &other;
        }
        if (!prev || !next || prev->locked || next->locked) return false;

        const FrameNumber delta = newStart - oldStart;
        if (newStart < prev->timelineStart || newEnd > next->timelineEnd) return false;

        prev->timelineEnd = newStart;
        prev->sourceOut = prev->sourceIn + (prev->timelineEnd - prev->timelineStart);

        clip->timelineStart = newStart;
        clip->timelineEnd = newEnd;

        next->timelineStart = newEnd;
        next->sourceIn += delta;
        return true;
    }

    // Standard trim (no ripple) — already used by playhead trim menu actions.
    bool trimClipStart(const std::string& clipId, FrameNumber frame) {
        return trimClipStartTo(clipId, frame);
    }

    bool trimClipEnd(const std::string& clipId, FrameNumber frame) {
        return trimClipEndTo(clipId, frame);
    }

    bool trimClipStartTo(const std::string& clipId, FrameNumber newStart) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked || newStart < 0 || newStart >= clip->timelineEnd) {
            return false;
        }
        const FrameNumber delta = newStart - clip->timelineStart;
        clip->timelineStart = newStart;
        clip->sourceIn += delta;
        return true;
    }

    bool trimClipEndTo(const std::string& clipId, FrameNumber newEnd) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked || newEnd <= clip->timelineStart) return false;
        clip->timelineEnd = newEnd;
        clip->sourceOut = clip->sourceIn + (newEnd - clip->timelineStart);
        return true;
    }

    // Move a clip horizontally on this track. Preserves clip duration.
    bool moveClipPosition(const std::string& clipId, FrameNumber newStart, bool allowOverlap = false) {
        Clip* clip = findClip(clipId);
        if (!clip || clip->locked) return false;

        const FrameNumber duration = clip->duration();
        newStart = std::max<FrameNumber>(0, newStart);
        const FrameNumber newEnd = newStart + duration;

        if (!allowOverlap) {
            for (const auto& other : clips_) {
                if (other.id == clipId) continue;
                if (newStart < other.timelineEnd && other.timelineStart < newEnd) {
                    return false;
                }
            }
        }

        clip->timelineStart = newStart;
        clip->timelineEnd = newEnd;
        std::stable_sort(clips_.begin(), clips_.end(),
                         [](const Clip& a, const Clip& b) { return a.timelineStart < b.timelineStart; });
        return true;
    }

private:
    std::string id_;
    std::string name_;
    TrackType type_;
    bool locked_ = false;
    bool muted_ = false;
    bool solo_ = false;
    std::vector<Clip> clips_;
};

// The Timeline (a.k.a. Sequence) owns a set of tracks and the frame rate /
// resolution the edit is authored at. Nested sequences (compound clips) are
// represented as ClipType::Nested clips whose mediaPath references another
// Timeline's id, resolved by the owning Project.
class Timeline {
public:
    Timeline(std::string id, std::string name, double frameRate, int width, int height)
        : id_(std::move(id)), name_(std::move(name)), frameRate_(frameRate),
          width_(width), height_(height) {}

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    double frameRate() const { return frameRate_; }
    int width() const { return width_; }
    int height() const { return height_; }

    Track& addTrack(std::string id, std::string name, TrackType type) {
        tracks_.push_back(std::make_unique<Track>(std::move(id), std::move(name), type));
        return *tracks_.back();
    }

    const std::vector<std::unique_ptr<Track>>& tracks() const { return tracks_; }
    std::vector<std::unique_ptr<Track>>& tracks() { return tracks_; }

    void clearTracks() { tracks_.clear(); }

    static bool clipMatchesTrack(ClipType clipType, TrackType trackType) {
        switch (clipType) {
        case ClipType::Video:
        case ClipType::Image:
        case ClipType::AdjustmentLayer:
        case ClipType::Nested:
            return trackType == TrackType::Video;
        case ClipType::Audio:
            return trackType == TrackType::Audio;
        case ClipType::Title:
            return trackType == TrackType::Subtitle;
        }
        return false;
    }

    Track* findTrackById(const std::string& trackId) {
        for (auto& track : tracks_) {
            if (track->id() == trackId) return track.get();
        }
        return nullptr;
    }

    struct ClipRef {
        Track* track = nullptr;
        Clip* clip = nullptr;
    };

    std::optional<ClipRef> findClipRef(const std::string& clipId) {
        for (auto& track : tracks_) {
            if (Clip* clip = track->findClip(clipId)) {
                return ClipRef{track.get(), clip};
            }
        }
        return std::nullopt;
    }

    // Move a clip to a (possibly different) track at `newStart` frames.
    bool moveClip(const std::string& clipId, const std::string& toTrackId,
                  FrameNumber newStart, bool allowOverlap = true) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip || ref->clip->locked) return false;

        Track* dest = findTrackById(toTrackId);
        if (!dest || !clipMatchesTrack(ref->clip->type, dest->type())) return false;

        const FrameNumber oldStart = ref->clip->timelineStart;
        const std::optional<std::string> linkedId = ref->clip->linkedClipId;
        const FrameNumber delta = newStart - oldStart;
        const FrameNumber duration = ref->clip->duration();

        Track* sourceTrack = ref->track;
        if (sourceTrack->id() == toTrackId) {
            const bool ok = sourceTrack->moveClipPosition(clipId, newStart, allowOverlap);
            if (ok && linkedId) {
                if (auto linkedRef = findClipRef(*linkedId)) {
                    linkedRef->track->moveClipPosition(linkedRef->clip->id,
                                                      linkedRef->clip->timelineStart + delta,
                                                      allowOverlap);
                }
            }
            return ok;
        }

        Clip moving = *ref->clip;
        const Clip backup = moving;
        sourceTrack->removeClip(clipId);

        moving.timelineStart = std::max<FrameNumber>(0, newStart);
        moving.timelineEnd = moving.timelineStart + duration;

        if (!dest->addClip(std::move(moving), allowOverlap)) {
            sourceTrack->addClip(backup, true);
            return false;
        }

        if (linkedId) {
            if (auto linkedRef = findClipRef(*linkedId)) {
                linkedRef->track->moveClipPosition(linkedRef->clip->id,
                                                  linkedRef->clip->timelineStart + delta,
                                                  allowOverlap);
            }
        }
        return true;
    }

    FrameNumber durationFrames() const {
        FrameNumber maxEnd = 0;
        for (const auto& track : tracks_) {
            for (const auto& clip : track->clips()) {
                maxEnd = std::max(maxEnd, clip.timelineEnd);
            }
        }
        return maxEnd;
    }

    double durationSeconds() const {
        return frameRate_ > 0.0 ? static_cast<double>(durationFrames()) / frameRate_ : 0.0;
    }

    // Apply a trim-tool edit to a clip and mirror it on a linked audio/video partner.
    bool trimClipStartWithTool(const std::string& clipId, FrameNumber frame, TrimTool tool) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip || ref->clip->locked) return false;
        bool ok = false;
        switch (tool) {
        case TrimTool::Ripple:
            ok = ref->track->rippleTrimStart(clipId, frame);
            break;
        case TrimTool::Selection:
        default:
            ok = ref->track->trimClipStart(clipId, frame);
            break;
        }
        if (ok) syncLinkedClipTrim(*ref->clip, frame, true, tool);
        return ok;
    }

    bool trimClipEndWithTool(const std::string& clipId, FrameNumber frame, TrimTool tool) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip || ref->clip->locked) return false;
        bool ok = false;
        switch (tool) {
        case TrimTool::Ripple:
            ok = ref->track->rippleTrimEnd(clipId, frame);
            break;
        case TrimTool::Selection:
        default:
            ok = ref->track->trimClipEnd(clipId, frame);
            break;
        }
        if (ok) syncLinkedClipTrim(*ref->clip, frame, false, tool);
        return ok;
    }

    bool rollTrimAtCut(FrameNumber oldCut, FrameNumber newCut) {
        bool any = false;
        for (auto& track : tracks_) {
            if (track->rollTrimAt(oldCut, newCut)) any = true;
        }
        return any;
    }

    bool slipTrimClip(const std::string& clipId, FrameNumber sourceDelta) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip) return false;
        if (!ref->track->slipTrim(clipId, sourceDelta)) return false;
        if (ref->clip->linkedClipId) {
            if (auto linked = findClipRef(*ref->clip->linkedClipId)) {
                linked->track->slipTrim(linked->clip->id, sourceDelta);
            }
        }
        return true;
    }

    bool slipTrimClipTo(const std::string& clipId, FrameNumber newSourceIn) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip) return false;
        const FrameNumber delta = newSourceIn - ref->clip->sourceIn;
        if (!ref->track->slipTrimTo(clipId, newSourceIn)) return false;
        if (ref->clip->linkedClipId) {
            if (auto linked = findClipRef(*ref->clip->linkedClipId)) {
                linked->track->slipTrimTo(linked->clip->id, linked->clip->sourceIn + delta);
            }
        }
        return true;
    }

    bool slideClipOnTimeline(const std::string& clipId, FrameNumber newStart) {
        auto ref = findClipRef(clipId);
        if (!ref || !ref->clip) return false;
        const FrameNumber delta = newStart - ref->clip->timelineStart;
        if (!ref->track->slideClip(clipId, newStart)) return false;
        if (ref->clip->linkedClipId) {
            if (auto linked = findClipRef(*ref->clip->linkedClipId)) {
                linked->track->moveClipPosition(linked->clip->id,
                                                linked->clip->timelineStart + delta, true);
            }
        }
        return true;
    }

private:
    void syncLinkedClipTrim(const Clip& source, FrameNumber frame, bool trimStart, TrimTool tool) {
        if (!source.linkedClipId) return;
        auto linked = findClipRef(*source.linkedClipId);
        if (!linked || !linked->clip) return;
        if (trimStart) {
            switch (tool) {
            case TrimTool::Ripple:
                linked->track->rippleTrimStart(linked->clip->id, frame);
                break;
            case TrimTool::Selection:
            default:
                linked->track->trimClipStart(linked->clip->id, frame);
                break;
            }
        } else {
            switch (tool) {
            case TrimTool::Ripple:
                linked->track->rippleTrimEnd(linked->clip->id, frame);
                break;
            case TrimTool::Selection:
            default:
                linked->track->trimClipEnd(linked->clip->id, frame);
                break;
            }
        }
    }

    std::string id_;
    std::string name_;
    double frameRate_;
    int width_;
    int height_;
    std::vector<std::unique_ptr<Track>> tracks_;
};

} // namespace nova::timeline
