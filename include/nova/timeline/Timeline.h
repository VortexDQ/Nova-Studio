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

    // Ripple trim: change a clip's timelineEnd/timelineStart and shift every
    // later clip on this track by the resulting delta. Returns false if the
    // clip isn't found or the edit would produce a negative-length clip.
    bool rippleTrimEnd(const std::string& clipId, FrameNumber newEnd) {
        Clip* clip = findClip(clipId);
        if (!clip || newEnd <= clip->timelineStart) return false;
        FrameNumber oldEnd = clip->timelineEnd;
        FrameNumber delta = newEnd - oldEnd;
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

private:
    std::string id_;
    std::string name_;
    double frameRate_;
    int width_;
    int height_;
    std::vector<std::unique_ptr<Track>> tracks_;
};

} // namespace nova::timeline
