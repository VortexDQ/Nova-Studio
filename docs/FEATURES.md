# Nova Studio Feature Catalog

Track record, edit, and export features against the full Clipchamp-style vision.
Legend: **Done** | **Partial** | **Planned** | **Plugin** (optional add-on)

---

## Record

| Feature | Status | Notes |
|---------|--------|-------|
| Screen recorder | **Partial** | Qt `QScreenCapture` (desktop app, not browser) |
| Camera recorder | **Partial** | Webcam via Qt Multimedia |
| Screen + webcam | **Partial** | Records screen; camera PiP compositing planned |
| Audio / voice recorder | **Partial** | Microphone to MP4/M4A |
| Multi-take recording | **Partial** | Each recording saved as a new media file |

---

## Edit (beginner + advanced)

| Feature | Status | Notes |
|---------|--------|-------|
| Trim | **Done** | Selection + Ripple edge drag; Roll/Slip/Slide tools (V/R/N/Y/U) |
| Rotate | **Partial** | 90° steps per clip; free 360° planned |
| Resize / crop | **Partial** | Export aspect presets; interactive crop planned |
| Green screen | **Partial** | Chroma key in preview shader |
| Split audio / video | **Done** | Extract audio to WAV; detach linked audio |
| Remove audio | **Done** | Removes linked audio from selected clip |
| Replace audio | **Planned** | Stock music library hook |
| GIF maker | **Partial** | Export selection as GIF |
| Drag clips on timeline | **Done** | Move in time and across tracks |
| Split at playhead | **Done** | Blade tool (B) |
| Delete clips | **Done** | Del / Backspace / Delete button |

---

## Finishing touches

| Feature | Status | Notes |
|---------|--------|-------|
| Text and titles | **Partial** | Presets, edit in Inspector, F2 / double-click |
| Brand kit | **Partial** | Logo + brand color in project |
| Video enhancer | **Partial** | Brightness, contrast, saturation sliders |
| Stickers / emojis / shapes | **Planned** | Milestone 8 |
| Templates | **Done** | `.nova` templates on welcome + sidebar |
| Stock library | **Partial** | Generated color plates + sample videos |
| Overlays | **Partial** | Multi-track video + text layers |

---

## Export

| Feature | Status | Notes |
|---------|--------|-------|
| MP4 | **Done** | Stream copy + trim range |
| MOV / MKV / WebM / AVI | **Done** | Via export dialog |
| MP3 | **Partial** | Audio export from clip |
| GIF | **Partial** | Clip range to animated GIF |
| Save project (.nova) | **Done** | Autosave + backups |

---

## AI tools (plugin-ready)

| Feature | Status | Notes |
|---------|--------|-------|
| AI text to speech | **Plugin** | Requires Nova AI plugin |
| AI video editor / auto compose | **Plugin** | Planned plugin |
| AI silence remover | **Plugin** | Planned plugin |
| AI background noise remover | **Plugin** | Planned plugin |
| AI subtitles | **Plugin** | Planned plugin |
| AI background removal | **Plugin** | Planned plugin |

AI features are optional plugins so the core editor stays fast and offline-first.
