// Minimal, dependency-free test harness (no GTest/Catch2 needed for the
// vertical slice). Real CI would use Catch2/GTest for richer reporting -
// see docs/ROADMAP.md.
#include <cassert>
#include <iostream>

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
    Timeline timeline("seq-1", "Test Sequence", 30.0, 1920, 1080);
    Track& v1 = timeline.addTrack("v1", "V1", TrackType::Video);

    expect(timeline.tracks().size() == 1, "timeline has one track after addTrack");
    expect(v1.clips().empty(), "new track starts with no clips");

    Clip clipA;
    clipA.id = "a";
    clipA.name = "Clip A";
    clipA.timelineStart = 0;
    clipA.timelineEnd = 90; // 3 seconds @ 30fps

    Clip clipB;
    clipB.id = "b";
    clipB.name = "Clip B";
    clipB.timelineStart = 90;
    clipB.timelineEnd = 150; // 2 seconds @ 30fps

    expect(v1.addClip(clipA), "clip A inserts cleanly into empty track");
    expect(v1.addClip(clipB), "clip B inserts after clip A without overlap");

    Clip overlapping;
    overlapping.id = "c";
    overlapping.timelineStart = 45;
    overlapping.timelineEnd = 100;
    expect(!v1.addClip(overlapping), "overlapping clip is rejected by default");

    expect(v1.clips().size() == 2, "track holds exactly the two non-overlapping clips");
    expect(v1.clips()[0].id == "a" && v1.clips()[1].id == "b",
           "clips remain sorted by timelineStart");

    expect(timeline.durationFrames() == 150, "timeline duration matches furthest clip end");
    expect(timeline.durationSeconds() == 5.0, "timeline duration in seconds matches frame rate");

    expect(v1.rippleTrimEnd("a", 120), "ripple-trim extends clip A by one second");
    expect(v1.findClip("a")->timelineEnd == 120, "clip A's new end is applied");
    expect(v1.findClip("b")->timelineStart == 120, "ripple shifts clip B by the same delta");
    expect(v1.findClip("b")->timelineEnd == 180, "ripple shifts clip B's end too");

    expect(v1.splitClipAt("a", 60, "a-right"), "split cuts clip A at a valid interior frame");
    expect(v1.findClip("a")->timelineStart == 0, "left split keeps original start");
    expect(v1.findClip("a")->timelineEnd == 60, "left split ends at blade frame");
    expect(v1.findClip("a-right")->timelineStart == 60, "right split starts at blade frame");
    expect(v1.findClip("a-right")->timelineEnd == 120, "right split keeps original end");
    expect(v1.findClip("a-right")->sourceIn == 60, "right split source in advances by cut offset");
    expect(!v1.splitClipAt("a", 0, "bad"), "split on clip edge is rejected");

    expect(!v1.rippleTrimEnd("nonexistent", 200), "ripple-trim on missing clip fails gracefully");

    Clip clipC;
    clipC.id = "c";
    clipC.timelineStart = 180;
    clipC.timelineEnd = 240;
    expect(v1.addClip(clipC), "clip C for ripple start test");
    expect(v1.rippleTrimStart("b", 130), "ripple-trim start shortens clip B and shifts C");
    expect(v1.findClip("b")->timelineStart == 130, "clip B start moved");
    expect(v1.findClip("c")->timelineStart == 170, "clip C shifted left after ripple start");

    Track& v2 = timeline.addTrack("v2", "V2", TrackType::Video);
    Clip rollLeft;
    rollLeft.id = "rl";
    rollLeft.timelineStart = 0;
    rollLeft.timelineEnd = 60;
    rollLeft.sourceIn = 0;
    rollLeft.sourceOut = 60;
    Clip rollRight;
    rollRight.id = "rr";
    rollRight.timelineStart = 60;
    rollRight.timelineEnd = 120;
    rollRight.sourceIn = 100;
    rollRight.sourceOut = 160;
    v2.addClip(rollLeft);
    v2.addClip(rollRight);
    expect(v2.rollTrimAt(60, 75), "roll trim moves cut between neighbors");
    expect(v2.findClip("rl")->timelineEnd == 75, "roll left clip end follows cut");
    expect(v2.findClip("rr")->timelineStart == 75, "roll right clip start follows cut");

    expect(v2.slipTrim("rr", 10), "slip trim shifts source in/out");
    expect(v2.findClip("rr")->sourceIn == 125, "slip advances source in after roll");

    Track& v3 = timeline.addTrack("v3", "V3", TrackType::Video);
    Clip slidePrev;
    slidePrev.id = "sp";
    slidePrev.timelineStart = 0;
    slidePrev.timelineEnd = 75;
    slidePrev.sourceIn = 0;
    slidePrev.sourceOut = 75;
    Clip slideMid;
    slideMid.id = "sm";
    slideMid.timelineStart = 75;
    slideMid.timelineEnd = 105;
    slideMid.sourceIn = 0;
    slideMid.sourceOut = 30;
    Clip slideNext;
    slideNext.id = "sn";
    slideNext.timelineStart = 105;
    slideNext.timelineEnd = 150;
    slideNext.sourceIn = 200;
    slideNext.sourceOut = 245;
    v3.addClip(slidePrev);
    v3.addClip(slideMid);
    v3.addClip(slideNext);
    expect(v3.slideClip("sm", 85), "slide trim moves middle clip between neighbors");
    expect(v3.findClip("sp")->timelineEnd == 85, "slide adjusts previous clip end");
    expect(v3.findClip("sm")->timelineStart == 85, "slide moves clip start");
    expect(v3.findClip("sn")->timelineStart == 115, "slide adjusts next clip start");

    expect(v1.removeClip("a"), "removeClip succeeds for an existing clip");
    expect(v1.clips().size() == 3, "track keeps right split, clip B, and clip C after removal");
    expect(!v1.removeClip("a"), "removeClip on already-removed clip fails");

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All timeline tests passed.\n";
    return 0;
}
