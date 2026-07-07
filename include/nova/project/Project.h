#pragma once

#include "nova/timeline/Timeline.h"

#include <memory>
#include <string>
#include <vector>

namespace nova::project {

struct MediaAsset {
    std::string id;
    std::string path;
    std::string name;
    std::string folder = "Imports";
    std::vector<std::string> tags;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    bool hasVideo = false;
    bool hasAudio = false;
};

struct ProjectMetadata {
    std::string author;
    std::string description;
    std::string createdIso;
    std::string modifiedIso;
};

// A Nova Studio project: media bin, one or more timelines, and metadata.
// Serialized as JSON (.nova). Runtime-only fields (filePath, dirty) are not
// written to disk.
struct Project {
    int formatVersion = 1;
    std::string name;
    ProjectMetadata metadata;
    std::vector<MediaAsset> media;
    std::vector<std::unique_ptr<nova::timeline::Timeline>> timelines;
    std::string activeTimelineId;

    std::string filePath;
    bool dirty = false;

    nova::timeline::Timeline* activeTimeline();
    const nova::timeline::Timeline* activeTimeline() const;

    static std::unique_ptr<Project> createBlank(const std::string& name, double fps,
                                                int width, int height);
};

} // namespace nova::project
