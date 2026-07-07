# Third-Party Licenses

Nova Studio's own code is licensed under Apache License 2.0 (see `LICENSE`).
It links against the following third-party libraries, which retain their
own licenses:

| Library | License | Linkage | Notes |
|---|---|---|---|
| Qt 6 (Widgets, OpenGLWidgets, Core, Gui) | LGPLv3 (or GPLv3/commercial) | Dynamic | This project links Qt dynamically, which satisfies LGPLv3's relinking requirement. Do not statically link Qt into distributed binaries without reviewing LGPL compliance. |
| FFmpeg (libavcodec, libavformat, libavutil, libswscale, libswresample) | LGPLv2.1+ (GPL if built with `--enable-gpl`) | Dynamic | Ship the LGPL build of FFmpeg for distribution unless you specifically need GPL-only components and are prepared to license Nova Studio itself under GPL for that build. |

## Guidance for packagers

- If you distribute binaries, include this file (or an equivalent NOTICE)
  and the license texts for Qt and FFmpeg alongside your release.
- If you enable any GPL-only FFmpeg components (e.g. certain filters or
  x264/x265 via `--enable-gpl --enable-libx264`), the resulting binary is
  subject to GPL, not just LGPL - check before shipping.
