<h1 align="center">goome in the browser</h1>

Compile a goome app to webassembly and drop the folder on any static host. There is no emscripten in this, just clang with the wasm32 target, wasm-ld, a tiny libc (`wasm/libc`) and about 300 lines of js.

```
wasm/web-build.sh examples/web_counter.c out/
python3 -m http.server -d out
```

`out/` ends up with `index.html`, `goome.js` and `goome.wasm`, nothing else. Needs `clang` with the wasm32 target and `wasm-ld` (`apt install clang lld`), set `CLANG=` and `WASM_LD=` if they are somewhere odd. `GOOME_WEB_CFLAGS` adds flags to every file. More source files go after the outdir: `web-build.sh app.c out/ util.c`.

## What your app looks like

Only apps work (`goome_open_app` or `.mode = GOOME_MODE_WINDOW`). The page is the window, so overlays and mod menus are refused: the build script says so, and `goome_open` returns `GOOME_ENOTSUP` for any other mode when built for the web.

A browser cant sit in a `while` loop, so the loop is split in two functions that you export. `main` can stay for the desktop build, it just isnt used on the web:

```c
int goome_web_init(void);
int goome_web_frame(void);
void goome_web_done(void);
```

- `goome_web_init` is everything before the loop. Call `goome_open` here, return 0 when it worked.
- `goome_web_frame` is one trip through the loop, called every animation frame (about every 100 ms when nothing happens). Poll with `goome_poll`, draw, return 1 to keep going, 0 to quit. Nothing blocks on the web, `goome_poll` just returns what is queued.
- `goome_web_done` is optional, called once after the frame function returned 0. Free things there, dont call `goome_close` earlier or the last picture is gone.

`goome_open` ignores the size you ask for and uses the browser window. When the window changes size you get a `GOOME_MONITORS` event, like on the desktop.

The window has no title bar of its own: goome leaves out the bar `goome_ui_app` draws (`GOOME_WINDOW_NO_TITLEBAR`, set for you in the web build) and the content fills the whole canvas. The title you pass to `goome_open` becomes the page title. If you draw your own bar with the draw api, you have to skip it yourself. `GOOME_WINDOW_NO_CONSOLE` is accepted and does nothing here.

`examples/web_counter.c` (widgets), `examples/web_draw.c` (draw api, mouse, wheel, keys, clipboard, cursor), `examples/web_game.c` (a breakout game) and `examples/web_app.c` (a task list with tabs, a table and themes) are all written like this and still build on the desktop.

## What doesnt work

- A `while (running)` loop in `main` that the page has to wait for. Without asyncify or threads there is no way to pause wasm and give the page the turn back.
- Overlay mode, capture, global hotkeys, grab, window moving/state, drag and drop of files, `goome_font_system` (only the built in font, load a .ttf with `goome_font_load_mem` from data you compiled in), files (`fopen` fails), sockets, threads, env vars, `system`.
- libc is a small one: `string.h` basics (`strlen strcpy strncpy strcat strdup strcmp memcpy` ...), `stdlib.h` (`malloc strtol strtod atoi atof rand qsort`), `ctype.h`, `stdio.h` (`printf` and friends go to `console.log`, `snprintf` is real), `math.h` (`sqrt fabs floor ceil` are instructions, `sin cos tan atan2 pow exp log` call the js Math functions). Anything else fails at link time with an undefined symbol.
- `scanf`, `strtok`, `sprintf` (use `snprintf`), `time()` (use `goome_now_ms`), `localtime`.

## The ABI

Everything below is plain wasm32, no c++ and no js from the app. Another language can plug its output into the same `index.html` and `goome.js` if the module looks like this.

**Imports** (module `env`, all optional, js supplies every one of them):

| name | signature | |
| --- | --- | --- |
| `gmw_console` | `(ptr i32, len i32)` | utf-8 text for `console.log` |
| `sin cos tan atan atan2 asin acos exp log log2 log10 pow sinh cosh tanh` | `(f64[, f64]) -> f64` | the js Math functions |

**Exports** (memory is exported as `memory`, linear memory may grow, so take views after every call):

| name | signature | |
| --- | --- | --- |
| `gmw_init` | `(w i32, h i32, scale f64, chrome i32) -> i32` | logical size, device pixel ratio, 1 = keep the goome title bar. calls `goome_web_init`. 0 = ok, -1 = it failed, -2 = it never opened a window |
| `gmw_frame` | `(ms f64) -> i32` | one frame, ms from `performance.now()`. bit 0 = the picture changed, bit 1 = the app is still running |
| `gmw_resize` | `(w, h i32, scale f64) -> i32` | logical size |
| `gmw_width` `gmw_height` | `() -> i32` | picture size in real pixels (logical size times scale) |
| `gmw_pixels` | `() -> ptr` | width * height * 4 bytes, RGBA, not premultiplied, rows packed. good until the next call |
| `gmw_push` | `(type i32, x f64, y f64, button i32, dy i32, key i32, mods i32, text ptr) -> i32` | queue an input event, see below. `text` is a nul terminated utf-8 string or 0 |
| `gmw_cursor` | `() -> i32` | `GOOME_CURSOR_*` the app asked for |
| `gmw_title` | `() -> ptr` | nul terminated title of the window |
| `gmw_clip_get` | `() -> ptr` | the clipboard text the app set last, nul terminated |
| `gmw_clip_set` | `(ptr) -> i32` | what the app sees when it reads the clipboard (js calls it on paste) |
| `gmw_alloc` `gmw_free` | `(n i32) -> ptr`, `(ptr)` | for strings going in |

**Events** go through `gmw_push`. `type` is the `GOOME_*` event number from `goome.h`: 1 down, 2 up, 3 motion, 4 scroll (`dy` in notches, + is down), 5 key, 11 focus (`button` = 1 gained, 0 lost), 13 text (finished ime text in `text`, 63 bytes at a time), 14 preedit (`text` = what is being composed, `button` = caret byte offset, empty text ends it). x and y are logical pixels from the top left of the canvas. `button` 1 left, 2 middle, 3 right. `key` is a lowercase ascii letter or a `GOOME_K_*` value, `mods` is `GOOME_SHIFT | GOOME_CTRL | GOOME_ALT | GOOME_SUPER`, `text` on a key is what it typed. Paste is `gmw_clip_set` followed by a ctrl+v key.

**The app side** is `goome_web_init` and `goome_web_frame` as above, linked together with `src/*.c` built with `-DGOOME_WEB` and `wasm/web/web.c`, which is the only file that knows about the exports. A language binding only needs to export those two functions (or call into its own init and frame from them) and link the same objects.

## What goome.js does

`GoomeWeb.boot(canvas, "goome.wasm")` loads the module and calls `gmw_init` with `innerWidth`, `innerHeight` and `devicePixelRatio`, sets the page title, and runs the frame loop. It draws the picture with `putImageData`, sets the css cursor, copies clipboard text to `navigator.clipboard`, and turns pointer, wheel, keyboard, paste and ime events into `gmw_push` calls. Keys and ime go through a hidden textarea so composition works. The window `resize` event goes to `gmw_resize`.

`GoomeWeb.attach(exports, canvas, env)` is the same thing without the loading, with `window`, `document` and `navigator` passed in. `wasm/test/web.mjs` uses it with fake ones to test everything under node, no browser.
