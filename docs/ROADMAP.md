# Roadmap

This maps the full Nova Studio vision onto milestones. Rough sizing is in
engineer-weeks for a single experienced C++/graphics engineer; parallelize
across a team to compress wall-clock time.

**Legend:** ✅ shipped in this repo · 🚧 in progress · ⬜ planned

---

## Current vertical slice (shipped)

| Area | Status |
|------|--------|
| FFmpeg decode + GPU preview | ✅ |
| Basic timeline (tracks, clips, split, ripple-trim API) | ✅ |
| Audio playback (Qt Multimedia) + extract to WAV | ✅ |
| Dark editor shell + Clipchamp-style sidebar rail | ✅ |
| **Project file (.nova JSON)** - save/open, autosave, backups, version restore | ✅ |
| **Recent projects** + welcome screen | ✅ |
| **Multiple timelines per project** | ✅ |
| **Media library** - import, folders, tags, search, metadata probe | ✅ (basic) |
| **Project templates** - blank 1080p, vertical reels, YouTube | ✅ |
| Local offline projects (no cloud required) | ✅ |

---

## Milestone A - Project & media foundation (~2-3 weeks)

Expand what `.nova` projects can express and how media is managed offline.

| Feature | Status |
|---------|--------|
| New / open / save / save as | ✅ |
| Autosave (`.autosave.nova`) | ✅ |
| Rolling backups on save (`*_backups/`) | ✅ |
| Version history restore | ✅ |
| Recent projects | ✅ |
| Project templates | ✅ |
| Multiple timelines | ✅ |
| Import/export project copy | ✅ |
| Project metadata (author, description, timestamps) | ✅ |
| Media folders, tags, in-project search | ✅ (basic) |
| Thumbnail previews | ⬜ |
| Video preview in media bin | ⬜ |
| Waveform generation | ⬜ |
| Proxy generation on import | ⬜ |
| Media relinking + offline detection | ⬜ |
| Project archive (zip bundle) | ⬜ |
| Cloud sync (optional plugin) | ⬜ future |

**Supported import formats (via FFmpeg, expand testing):** MP4, MOV, MKV, AVI,
WebM, GIF, PNG, JPG, SVG, WAV, MP3, FLAC, SRT/VTT subtitles. ProRes, H.264,
H.265, AV1, VP9 depend on FFmpeg build - validated per platform in CI.

---

## Milestone 1 - Timeline interaction layer (~3-4 weeks)

| Feature | Status |
|---------|--------|
| Drag-to-move clips | ✅ |
| Blade/split at playhead | ✅ |
| Ripple / roll / slip / slide trim | ✅ (UI + keyboard shortcuts) |
| Multi-clip selection, grouping | ⬜ |
| Snapping, magnetic timeline | ⬜ |
| Markers, color labels | ⬜ |
| Track lock / solo / mute in UI | ⬜ (model exists) |
| Track colors, naming, grouping | ⬜ |
| Unlimited video/audio tracks | ⬜ (model supports many tracks) |
| Nested sequences / compound clips | ⬜ (model hook exists) |
| Multicam | ⬜ |
| Adjustment layers, pre-compositions | ⬜ |
| Timeline-driven playback (playhead scrubs preview) | ✅ |
| Select + delete clips | ✅ |
| Edit text clips (Inspector, F2, double-click) | ✅ |
| Stock library (generated plates + sample videos) | ✅ (basic) |

---

## Milestone 2 - Audio pipeline (~4-6 weeks)

| Feature | Status |
|---------|--------|
| Qt Multimedia preview sync | ✅ (single-clip) |
| `nova_audio` FFmpeg decode + mixer | ⬜ |
| Per-clip gain/pan, master bus | ⬜ |
| Waveform lanes | ⬜ |
| Fade in/out, ducking, EQ, compressor | ⬜ |
| Noise removal, voice enhancement | ⬜ |
| Beat detection | ⬜ |
| Surround / stereo control | ⬜ |
| AI voice cleanup, auto subtitles | ⬜ plugin |

---

## Milestone 3 - Transitions (~2-3 weeks)

Cross dissolve, fade in/out, dip to black/white, wipe, slide, push, zoom,
spin, blur, glitch, light leak, film burn, morph, custom transitions, duration
control. Sidebar **Transitions** panel lists presets; engine lands here.

---

## Milestone 4 - Video effects (~6-8 weeks)

**Visual:** blur, motion blur, sharpen, noise reduction, film grain, vignette,
glow, lens flare, chromatic aberration, pixelate, distortion, mirror,
kaleidoscope, VHS/retro/cyberpunk/cinematic looks.

**AI (plugin):** background removal, object tracking, scene detection,
enhancement, stabilization, upscaling, frame interpolation, color correction.

Generalize the renderer into a stackable, keyframeable effect graph (ping-pong
FBOs). Port existing brightness/contrast/saturation as first nodes.

---

## Milestone 5 - Motion & animation (~3-4 weeks)

Keyframe editor, position/scale/rotation/opacity, anchor point, motion paths,
preset animations, easing curves, camera moves, parallax.

---

## Milestone 6 - Color (~5-7 weeks)

Brightness/contrast/exposure/saturation/temperature/tint/highlights/shadows/
whites/blacks (partially in preview shader today). Color wheels, curves, LUTs,
HDR, scopes (histogram, waveform, vectorscope), selective color, skin tone.

---

## Milestone 7 - Text & titles (~3-4 weeks)

Sidebar **Text** panel mirrors Clipchamp categories: lower thirds, quotes,
ratings, credits, timers, meme bars, intro/outro. Animated text, captions,
auto captions, translation, font library, outlines/shadows, 3D text.

---

## Milestone 8 - Graphics (~2-3 weeks)

Stickers, shapes, emojis, icons, logos, watermarks, PNG/SVG layers, vector
overlays, template marketplace hooks.

---

## Milestone 9 - Green screen & compositing (~4-5 weeks)

Chroma key, AI segmentation, background replace, virtual backgrounds, shape/
freehand masks, feathering, motion-tracked masks, blend modes, alpha channels.

---

## Milestone 10 - Recording (Clipchamp-style) (~4-6 weeks)

| Feature | Status |
|---------|--------|
| Screen capture | ✅ (partial — Qt `QScreenCapture`) |
| Webcam capture | ✅ (partial) |
| Screen + webcam PiP | 🚧 (records screen; compositing planned) |
| Microphone / voice | ✅ (partial) |
| Multi-take → media library | ✅ |

---

## Milestone 11 - Proxy & performance (~3-4 weeks)

GPU acceleration (Vulkan/OpenGL today; CUDA/DirectX/Metal encode later), proxy
editing, background render queue, RAM/disk cache, crash recovery, low-end PC
mode, multi-thread rendering.

---

## Milestone 12 - Export & social (~4-5 weeks)

**Formats:** MP4, MOV, WebM, GIF, MP3 (audio), image sequences.

| Feature | Status |
|---------|--------|
| MP4 / MOV / MKV / WebM / AVI export | ✅ |
| MP3 audio export | ✅ (partial — via ffmpeg) |
| GIF export | ✅ (partial — via ffmpeg) |

**Presets:** 480p–8K, 24/30/60/120 fps, bitrate/codec, hardware encode.

**Social:** YouTube, TikTok, Reels, Shorts, Twitch, X, Facebook - aspect ratio
conversion, vertical editor, safe zones, thumbnail creator.

---

## Milestone 13 - Plugin SDK (~4-6 weeks)

Stable C ABI for effects, transitions, exporters, importers, cloud/AI plugins.
Example plugin in `/plugins`.

---

## Milestone 14 - Cross-platform (~6-10 weeks)

Windows ✅ (primary dev), Linux/macOS CI legs, Android/iOS via headless
`nova_core`/`nova_timeline`/`nova_media` + separate mobile UI.

---

## Professional / future (explicitly optional)

Collaborative editing, live multiplayer timeline, plugin marketplace, asset/
template marketplace, scripting API, virtual production, 3D/VR timeline,
Fusion-style compositing, After Effects-style motion design, cloud rendering,
AI editing agents.

These are **never hard dependencies** of the offline-first core.

---

## Technology stack (unchanged)

| Layer | Choice |
|-------|--------|
| Core engine | C++23 |
| UI | Qt 6 Widgets |
| Media | FFmpeg |
| Preview | OpenGL 3.3 (Vulkan later) |
| GPU encode | NVENC / QuickSync / VideoToolbox via FFmpeg |
| Apple | Metal path TBD |
| Cloud / AI | Optional plugins only |

---

## Immediate next PRs

1. Thumbnail + waveform in media library
2. Interactive crop/resize + drag trim handles
3. Screen + webcam PiP compositing during record
4. Stickers, emojis, shapes (Milestone 8)
5. Second GLSL effect (Gaussian blur) to validate effect graph
6. Windows + macOS GitHub Actions build legs
