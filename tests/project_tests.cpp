#include <cassert>
#include <iostream>
#include <filesystem>

#include "nova/project/Project.h"
#include "nova/project/ProjectIO.h"
#include "nova/timeline/Timeline.h"

using namespace nova::timeline;

namespace {
int g_failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++g_failures;
    } else {
        std::cout << "ok - " << description << '\n';
    }
}
} // namespace

int main() {
    auto project = nova::project::Project::createBlank("Test", 30.0, 1920, 1080);
    expect(project != nullptr, "createBlank returns project");
    expect(project->timelines.size() == 1, "blank project has one timeline");

    auto& timeline = *project->timelines.front();
    Track& v1 = timeline.addTrack("v2", "V2", TrackType::Video);

    Clip clip;
    clip.id = "clip-a";
    clip.name = "Clip A";
    clip.timelineStart = 0;
    clip.timelineEnd = 60;
    v1.addClip(clip);

    nova::project::MediaAsset asset;
    asset.id = "media-1";
    asset.path = "/tmp/sample.mp4";
    asset.name = "sample.mp4";
    asset.folder = "Imports";
    asset.tags = {"drone"};
    project->media.push_back(asset);

    const std::string tempPath =
        (std::filesystem::temp_directory_path() / "nova_project_test.nova").string();

    std::string error;
    expect(nova::project::ProjectIO::save(*project, tempPath, &error), "save project");
    auto loaded = nova::project::ProjectIO::load(tempPath, &error);
    expect(loaded.has_value(), "load project");
    expect(loaded->media.size() == 1, "loaded media count");
    expect(loaded->timelines.size() == 1, "loaded timeline count");
    expect(loaded->timelines.front()->tracks().size() == 3, "loaded track count includes added v2");

    std::filesystem::remove(tempPath);
    return g_failures == 0 ? 0 : 1;
}
