# Third-party notices

KamTape for PS5 is licensed under GPL-3.0-or-later (`LICENSE`). It includes or
adapts the following.

## The app (`src/`, `host/`, `tooling/`, `tools/`, `runtime/`)

| Component | Where | Copyright | Licence |
|---|---|---|---|
| ps5-native-app-boilerplate: build scripts, `tooling/native` (ps5-native-tool, PIE linker script, CRT), clean-room `runtime/libc.prx` | `tooling/`, `tools/`, `runtime/`, `src/runtime/` | (C) 2026 BlackBearReloaded | GPL-3.0-or-later |
| ps5-homebrew-ui kit: renderer, draw list, fonts, input, EGL display, pad, system calls | `src/gfx/`, `src/ui/`, `src/core/`, `src/platform/ps5/` (except `services.cpp`), `host/platform_host.cpp` | (C) 2026 BlackBearReloaded | GPL-3.0-or-later |
| Flashpoint PS5: interface sounds and their synthesiser, the audio mixer, system keyboard and browser services, PC snapshot tool | `assets/audio/`, `tools/make-audio.py`, `src/kt/audio.*`, `src/platform/ps5/services.cpp`, `host/` | Richie133 | GPL-3.0-or-later |
| PayLoader: JSON reader, PNG texture loader, job worker | `src/kt/json.*`, `src/gfx/image.*`, `src/kt/worker.*` (namespace renamed) | Marice and PayLoader contributors | GPL-3.0-or-later |
| Flashpoint PS5: the image cache the thumbnail cache is adapted from | `src/kt/thumbs.*` | Richie133 | GPL-3.0-or-later |
| ps5-native-app-boilerplate: libcurl support for native titles (console stand-ins, certificate list, non-blocking sockets, DNS) | `src/platform/ps5/console_curl.*` | (C) 2026 BlackBearReloaded | GPL-3.0-or-later |
| SponsorBlock for PS5 (ytps5): the DNS query builder `console_curl.c` uses | `src/kt/dns.*` | SponsorBlock for PS5 contributors | GPL-3.0-or-later |
| offact: the `sceImeDialog` parameter layout | `src/platform/ps5/services.cpp` | ps5-payload-dev | GPL-3.0 |
| Ruffle Flash PS5: the touchpad data offset and the `sceAudioOut` parameters | `src/platform/ps5/pad.cpp` (touch fields), `src/platform/ps5/audio_out.cpp` | ZiZc3 and Ruffle Flash PS5 contributors | GPL-3.0 |
| stb_image (PNG, JPEG and GIF decoders enabled) | `src/third_party/stb/` | Sean Barrett and contributors | Public domain or MIT (`src/third_party/stb/LICENSE`) |
| stb_image_write (PC snapshot tool only, not in the PS5 app) | `host/third_party/stb_image_write.h` | Sean Barrett and contributors | Public domain or MIT |
| Shape shader distance functions | `src/gfx/gl_batch.cpp` | Inigo Quilez | MIT |

The PS5 app statically links [ps5-opengl](https://github.com/darkxex/ps5-opengl)
(OpenGL on the PS5, built on Mesa) and PacBrew's ports of
[libcurl](https://curl.se/) (curl licence), [OpenSSL](https://www.openssl.org/)
(Apache-2.0) and their dependencies (zlib, zstd, libpsl), under those projects'
licences. `make` downloads them into `.deps/`; they are not stored in this
repository.

## Mozilla CA certificate store (`src/third_party/mozilla/ca_bundle.c`)

The root certificates of the Mozilla CA program, embedded as C, as carried by
SponsorBlock for PS5. Mozilla Public License 2.0. They let the app check
KamTape's HTTPS certificate when the console's own list can't be read.

## Sample data (`host/fixtures/`)

The videos, answers and pictures the PC build shows are invented for this
project by `host/fixtures/make_fixtures.py`, in the layout of KamTape's API.

## KamTape name and logo

The KamTape name and logo (`sce_sys/icon0.png`, `assets/logo.png`) belong to
KamTape. They are used to identify the site this app opens. This app is
unofficial and not endorsed by KamTape. The app contains no KamTape videos or
pages: it reads lists and details from KamTape's developer API and opens
kamtape.com in the PS5's own web browser.

## Fonts (`assets/fonts/`)

| Typeface | Copyright | Licence |
|---|---|---|
| Atkinson Hyperlegible Regular and Bold | (c) 2020 Braille Institute of America, Inc. | SIL Open Font License 1.1 (`assets/fonts/OFL-AtkinsonHyperlegible.txt`) |

The `.huifont` files are distance-field atlases baked from these typefaces with
`tools/font-baker`.
