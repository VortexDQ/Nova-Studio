# Contributing to Nova Studio

Thanks for considering a contribution. This project is early — the current
state is a real, working vertical slice, not a finished editor — so there's
a lot of room for meaningful first contributions. See `docs/ROADMAP.md` for
prioritized milestones and "good first PR" ideas.

## Ground rules

- **No placeholder code.** Every function that ships should actually do
  what its signature promises. If a feature is out of scope for a PR, leave
  it out entirely rather than stubbing it — an honest TODO comment plus an
  issue is better than a function that silently no-ops.
- **New modules stay dependency-clean.** `nova_core` and `nova_timeline`
  must never depend on Qt or FFmpeg; `nova_media` must never depend on Qt.
  This keeps a future headless/CLI/mobile build possible without a rewrite.
- **Tests accompany behavior changes.** Extending `Timeline`/`Track`? Add
  cases to `tests/timeline_tests.cpp`. Extending `Decoder`? Add cases to
  `tests/decoder_tests.cpp` (it can generate its own synthetic fixtures via
  the `ffmpeg` CLI — see that file for the pattern).
- **Match the existing style**: RAII, smart pointers over raw `new`/`delete`,
  `const` by default, descriptive names over comments where possible.

## Workflow

1. Open an issue describing the change before large PRs, so design
   direction can be agreed on first (especially for anything touching the
   module boundaries in `docs/ARCHITECTURE.md`).
2. Branch from `main`, keep PRs focused on one milestone/feature.
3. Ensure `cmake --build build && ctest --test-dir build` passes locally.
4. Run with `-DNOVA_ENABLE_SANITIZERS=ON` at least once if you touched
   memory-sensitive code (decoder buffers, GL resource lifetime).

## Code of conduct

Be respectful, assume good faith, keep discussion focused on the code.
