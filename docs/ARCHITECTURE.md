# Architecture

## Module map

| Module          | Status in this repo                              | Responsibility (full vision) |
|-----------------|---------------------------------------------------|-------------------------------|
| `nova_core`     | Implemented (logging; header-only utility target) | Logging, error handling, job system, memory pools, containers |
| `nova_timeline` | Implemented (data model + ripple-trim)             | Tracks, clips, nested sequences, compound clips, markers |
| `nova_media`    | Implemented (FFmpeg decode + seek)                 | Demux/decode for all supported formats, proxy generation, metadata/waveform extraction |
| `nova_renderer` | Implemented (GL 3.3 core, 1 shader pass)            | Full effect graph, color management, scopes, HDR pipeline |
| `nova_ui`       | Implemented (dockable shell: Media/Timeline/Inspector/Preview) | Full workspace: effects browser, color panel, audio mixer, scopes, project manager, plugin manager |
| `playback`      | Folded into `nova_ui`'s `MainWindow` for now        | Dedicated module: frame cache, RAM/disk cache, background decode threads, adaptive quality |
| `effects`       | One built-in shader (brightness/contrast/saturation) in `nova_renderer` | Node-based, stackable, keyframeable effect graph; LUTs; masks/keys |
| `animation`     | Not yet started                                     | Keyframe interpolation (linear/bezier/ease/elastic/bounce), graph editor |
| `audio`         | Not yet started                                     | Mixer, EQ, compressor/limiter, noise reduction, surround |
| `export`        | Not yet started                                     | Render queue, hardware encode, batch export, presets |
| `import`        | Folded into `nova_media`                            | Split out once more than one importer format family exists |
| `plugin`        | Not yet started                                     | Dynamic-load SDK: effects/transitions/panels/exporters/importers |
| `networking` / `cloud sync` / `ai` | Not started (future, per spec)          | Explicitly out of scope for the offline-first core |
| `testing`       | Implemented (ctest + 2 suites)                      | Expand to integration tests, fuzzing, perf benchmarks |

## Why these module boundaries

Each module is a separate CMake target so dependency direction is enforced
by the linker, not just convention:

```
nova_core  <---  nova_timeline
   ^                  ^
   |                  |
nova_media  <---  nova_renderer  <---  nova_ui  <---  nova_studio (exe)
```

`nova_core` has zero dependencies on anything else, so it (and eventually
`nova_timeline`) can be reused by a future headless render-farm binary
without pulling in Qt or FFmpeg. `nova_media` depends only on `nova_core` and
FFmpeg — no Qt — so it can be unit-tested and eventually reused by a CLI
batch-export tool.

## Data flow for the vertical slice

```
File dialog (nova_ui)
   -> Decoder::open() (nova_media, FFmpeg demux/decode)
   -> Timeline::Track::addClip() (nova_timeline, places clip at frame 0)
   -> QTimer-driven loop calls Decoder::nextFrame()
   -> VideoPreviewWidget::setFrame() uploads RGBA8 to a GL texture (nova_renderer)
   -> GLSL fragment shader applies brightness/contrast/saturation
   -> paintGL() presents to the dock-embedded QOpenGLWidget
```

## Known simplifications (see ROADMAP.md for the fix)

- Frames are converted to RGBA8 on the CPU (`sws_scale`) before GPU upload.
  Production code should keep frames in native YUV and convert in a shader,
  both for performance and for HDR/wide-gamut correctness.
- Only a single clip per track is exercised end-to-end by `MainWindow`; the
  `Timeline`/`Track` data model already supports many clips and rejects
  overlaps, but the UI doesn't yet expose drag/blade/roll interactions.
- The effect pipeline is a single shader pass, not the stackable node graph
  described in the full spec.
- Audio is entirely unimplemented; the timeline model reserves an audio
  track but nothing decodes or plays audio yet.
- Seeking decodes forward from the nearest keyframe rather than resolving
  the exact requested frame.
