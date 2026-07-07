#include "nova/project/Project.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace nova::project {

namespace {

std::string currentIsoTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace

nova::timeline::Timeline* Project::activeTimeline() {
    if (activeTimelineId.empty()) {
        return timelines.empty() ? nullptr : timelines.front().get();
    }
    for (auto& timeline : timelines) {
        if (timeline->id() == activeTimelineId) {
            return timeline.get();
        }
    }
    return timelines.empty() ? nullptr : timelines.front().get();
}

const nova::timeline::Timeline* Project::activeTimeline() const {
    return const_cast<Project*>(this)->activeTimeline();
}

std::unique_ptr<Project> Project::createBlank(const std::string& name, double fps,
                                              int width, int height) {
    auto project = std::make_unique<Project>();
    project->name = name;
    project->metadata.createdIso = currentIsoTimestamp();
    project->metadata.modifiedIso = project->metadata.createdIso;

    auto timeline = std::make_unique<nova::timeline::Timeline>(
        "seq-1", "Sequence 01", fps, width, height);
    timeline->addTrack("v1", "V1", nova::timeline::TrackType::Video);
    timeline->addTrack("a1", "A1", nova::timeline::TrackType::Audio);

    project->activeTimelineId = timeline->id();
    project->timelines.push_back(std::move(timeline));
    return project;
}

} // namespace nova::project
