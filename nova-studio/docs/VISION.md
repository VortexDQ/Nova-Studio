# Vision

Nova Studio's long-term goal is an open-source, professional-grade,
offline-first video editor — native C++23, GPU-accelerated, cross-platform
(Windows/Linux/macOS, with Android/iOS as a further-out goal), with a
plugin architecture, node-based effects, professional color management, and
a full timeline/audio/export pipeline comparable to commercial NLEs.

That is a multi-year effort. This repository is the first real milestone
toward it: an architected, buildable, tested foundation rather than a
finished product. See:

- [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) — module boundaries and what's
  implemented today vs. planned
- [`docs/ROADMAP.md`](ROADMAP.md) — milestone-by-milestone path from this
  slice to the full vision, with rough sizing

Design principles carried through from the original spec, honored in what's
built so far:

- **Offline-first, privacy-respecting** — no network calls anywhere in the
  current code; cloud sync/AI are explicitly deferred, optional, plugin-only
- **Modular** — each subsystem is its own CMake target with enforced
  dependency direction (see the diagram in ARCHITECTURE.md)
- **Frame-accurate** — the timeline model stores frame counts, not floating
  point seconds, specifically so trim/ripple math can't drift
- **No placeholder implementations** — every module that exists in this
  repo does real work end-to-end (see the README's "What works right now")
