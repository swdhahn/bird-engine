# Building for the Web (WebGL 2)

The web build in `web/` compiles the engine and the sample game from `test/` to WebAssembly with Emscripten. It renders through WebGL 2 (the OpenGL ES 3.0 profile) and runs in current Chrome, Edge, Firefox and Safari.

## Get the build from GitHub Actions

Every push to `webgl` that touches the engine, assets or `web/` runs the **WebGL build** workflow (`.github/workflows/web.yml`), which also opens the page in headless Chrome and fails unless the engine reaches its frame loop.

1. Open the repository's **Actions** tab and select the latest **WebGL build** run.
2. Download the **bird-web** artifact and unzip it (`index.html`, `index.js`, `index.wasm`, `index.data`).
3. Serve the folder over HTTP; browsers refuse to load WebAssembly from `file://`:

```bash
python3 -m http.server 8000 -d bird-web
# open http://localhost:8000
```

## Build locally

Requirements: [emsdk](https://emscripten.org/docs/getting_started/downloads.html) with Emscripten 4.0.10 (the version CI pins), CMake 3.24 or newer.

```bash
emcmake cmake -S web -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
python3 -m http.server 8000 -d build-web
```

Newer Emscripten releases ship a libc++ that no longer includes `<type_traits>` transitively, which Jolt 5.4.0 relies on.

## How the build is put together

| Piece | Location | Role |
|---|---|---|
| `WebWindow` | `src/platform/web/` | WebGL 2 context on `<canvas id="canvas">`, DOM keyboard, mouse, wheel and touch input, Pointer Lock for mouse capture |
| `WebMain.cpp` | `src/platform/web/` | `main()`: `Application::start()`, then `tick()` on every animation frame |
| `index.html` | `web/shell.html` | Full-window canvas and loading status |
| `index.data` | `web/CMakeLists.txt` | Every committed `.bird` asset, preloaded into Emscripten's file system at `/assets` |

The engine builds as a static library and links into one WebAssembly module with the game, since browsers cannot load native shared libraries. The game's `app_entry()` is called directly.

## Behaviour differences

- **No threads.** Browsers only allow threads on pages served with cross-origin isolation headers, so physics jobs and mesh chunk builds run on the main thread.
- **Canvas size.** The drawing buffer follows the canvas's CSS size times `devicePixelRatio`; UI and input coordinates are CSS pixels.
- **Mouse capture** uses the Pointer Lock API. Browsers grant it only during a click, so a capture requested from a key press takes effect on the next click.
- **Keyboard bindings** use physical key positions (`KeyboardEvent.code`), so WASD stays in place on any layout. Text input uses the typed character (`KeyboardEvent.key`).
- **Clipboard** is internal to the page for now.
- **Download size** is about 94 MB, almost all of it the sample map.
- Dart scripting is not available on the Web.
