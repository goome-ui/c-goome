# how goome works

goome puts one see-through window over every monitor. Only the rects you pass as hits take clicks, everywhere else is click-through. On top of that it does pointer, keys, hotkeys and screen grabs.

It started as the overlay part of [open-chease](https://github.com/sevvy-wevvy/open-chease). Most of the weird stuff further down is stuff open-chease hit first.

## The API

Its all in [include/goome.h](../include/goome.h), go read that, its short. The rough shape:

- `goome_open` picks a backend and opens the overlay. `goome_backend_name` and `goome_caps` tell you what you got. Check the `GOOME_CAP_` bits before using something, dont guess from the name.
- `goome_monitors` gives logical desktop coords plus `scale` (real px per logical px) and `rotation`. When the list changes you get a `GOOME_MONITORS` event.
- Every frame: `goome_begin(g, mon)` hands you that monitors canvas already cleared, you draw, then `goome_end(g, c, hits, n)` sends it. No hits means fully click-through. A canvas with nothing drawn costs basically nothing, the backends drop their buffers or unmap the window.
- `goome_poll` gives you clicks, motion and scroll over your hits, keys while `goome_keyboard(g, 1)` is on, and hotkeys, control lines, monitor changes and quit at any time.
- `goome_capture` grabs a monitor or a rect of it. It always comes out as `0xFFRRGGBB` no matter what the desktop hands back. Capture only starts on the first grab, so an overlay that never grabs never gets a "share your screen" popup. `no_capture` makes sure of it.

Positions are logical everywhere. Canvases are real pixels with `ox`, `oy` and `scale` on them, and the draw functions take logical coords and scale them for you.

Damage tracking is what keeps it cheap. The draw functions mark what they touch (`dx0..dy1`), `goome_begin` only wipes what got drawn last time (`cx0..cy1`) and `goome_end` only uploads the union. If you write `c->px` yourself you have to call `goome_damage` or it wont show up. open-chease had a 117 ms redraw on a 5120x1440 screen before this, almost all of it copying the whole surface every frame.

The drawing layer (src/draw.c and the 6 built in fonts) is optional, `GOOME_NO_DRAW` leaves it out. Measure text with `goome_text_width` on the canvas you draw on, past scale 1.75 the font doubles and width / scale is just wrong.

One `goome` per process, one thread. On macOS it has to be the main thread.

## Picking a backend

Inside there are two halves, an overlay (`gm_ov`) and a capture (`gm_cap`), see [src/internal.h](../src/internal.h). Most desktops use a matching pair but GNOME and KDE mix them, so `gm_pick` in src/goome.c tries pairs in order and the first one that opens wins. Opening means the overlay came up with at least one monitor and the capture's `probe` said yes (probes are cheap and never pop anything up).

Windows and Mac only have one choice each. On Linux:

1. X11 session (`XDG_SESSION_TYPE=x11`, or `DISPLAY` set without `WAYLAND_DISPLAY`): x11 + x11.
2. No wayland display either: give up, `GOOME_ENODISPLAY`.
3. GNOME on wayland: x11 overlay on XWayland with mutter capture, then x11 + portal, then x11 with no capture.
4. layer-shell + wlr-screencopy (sway, Hyprland, river, wayfire, labwc, niri).
5. layer-shell + portal (KDE, COSMIC).
6. x11 on XWayland + portal (Cinnamon and anything else without layer-shell).
7. Last try is an overlay with no capture at all, layer-shell then x11. You can still draw, grabs just return `GOOME_ENOTSUP`.

`GOOME_BACKEND=wayland|gnome|portal|x11|test` (or `opts.backend`) forces one, and then it fails instead of falling back. Setting any of the `GOOME_TEST_*` env vars switches to the test backend too.

## The backends

Nothing gets linked. Linux dlopens libwayland-client, libX11 and friends, xkbcommon, gio/glib/gstreamer. Windows loads gdi32, shcore and the rest with LoadLibrary. macOS dlopens the frameworks and talks to them through the objc runtime, so theres no .m file and no `-framework` flags. If a lib is missing that backend fails to open and the next one gets a go.

### Wayland (src/wl.c)

Overlay is one layer-shell surface per output on the OVERLAY layer, anchored to all 4 edges with exclusive zone -1. `goome_end` sets the input region to your hits and attaches one of two shm buffers. Buffer scale is set to the output's integer scale so text is sharp on 2x screens. The protocol code is generated from protocols/*.xml by protocols/gen.py into src/wl_proto.h, and since theres no wayland header the bits of libwayland we call are spelled out at the top of wl.c.

Clicks and motion only come in over the input region (and while a press holds the implicit grab), so no global pointer. Except on Hyprland, where its IPC socket tells us `cursorpos`, so the cap is on there. Keys: keyboard interactivity goes to exclusive only while `goome_keyboard` is on. Keymaps come from the compositor through xkbcommon, with a plain US table if xkbcommon isnt there.

Hotkeys dont exist on wayland, see the control fifo below.

Capture is wlr-screencopy with the cursor off, `capture_output` or `capture_output_region`. Our own overlay is always in it (`has_self = 1`).

### X11 (src/x11.c)

One ARGB override-redirect window over the root, root sized minus 1 px. `goome_end` sets an XFixes input shape to the hits, puts the pixels in a pixmap, copies them over with one XCopyArea and raises it. It used to shrink to the drawn box, but a resize wipes the window and the chunked XPutImage showed half drawn frames on XWayland, so it flickered. It only unmaps once the overlay has been empty for 1 s. Monitors come from XRandR (`XRRGetMonitors`, primary first), and each canvas is a view into one root sized buffer, thats why canvases have a `stride`. No XRandR means one monitor covering the root. Hotplug comes from randr events plus a slow look at the root size every 2 s for servers that dont send them.

Hotkeys are `XGrabKey` on the root. `GOOME_CAP_GLOBAL_POINTER` is `XQueryPointer`. Capture is `XGetImage`, with XShm when its there because its way faster for full screens. Our window is in the grab, the cursor isnt.

Keyboard is the annoying one. With a window manager running, override-redirect windows never get focus, so goome maps a 1x1 see-through managed window under the pointer and asks the wm to activate it with the time of your last click on us. Without a wm it falls back to `XGrabKeyboard`.

### GNOME (src/gnome.c, mutter)

Overlay is the x11 backend on XWayland. Capture talks to mutter directly over D-Bus: `DisplayConfig.GetCurrentState` for the monitors, then `ScreenCast.CreateSession` and `RecordMonitor` per monitor with the cursor hidden, then a gstreamer `pipewiresrc ! videorate ! videoconvert ! appsink` pipeline per stream. No popup ever, which is why GNOME gets its own path.

The streams keep running between grabs and `goome_capture` crops from the newest frame, so the first grab right after opening can return `GOOME_EAGAIN`. `capture_fps` sets the videorate (default 5). Frames come back either in logical size or real size depending on GNOME's scaling mode, so streams get matched to monitors by overlap, also trying the stream scaled to the monitor width.

### Portal (src/gnome.c, portal)

Same pipeline, but the session comes from `org.freedesktop.portal.ScreenCast`: CreateSession, SelectSources (monitors, multiple, cursor hidden, persist mode 2), Start, OpenPipeWireRemote. The first Start pops the "share your screen" dialog. With `state_dir` set the restore token gets saved there, so it only asks once. Without it, every launch asks again.

Overlay is layer-shell if the compositor has it, x11 on XWayland if not.

### Windows (src/win32.c)

One layered topmost toolwindow popup per monitor, `UpdateLayeredWindowIndirect` with the dirty rect. Windows hit tests layered windows by alpha, so inside your hits the empty pixels go out as alpha 1 for the upload and get put back to 0 after.

Click-through needs babysitting: every poll looks at the mouse and takes `WS_EX_TRANSPARENT` off the window while its over a hit. Thats why you have to call `goome_poll` every ~30 ms or so. If a click lands on us off a hit anyway (mouse moved between polls) it gets passed on with `SendInput`.

DPI is per monitor (`GetDpiForMonitor`). Windows gives real px in one desktop, so each monitor keeps its real origin as its logical origin and goes real px / its own scale inside. A screen next to a scaled one gets a gap instead of an overlap, so every logical point maps back to exactly one real px.

Hotkeys are `RegisterHotKey` with no-repeat. Capture is BitBlt with CAPTUREBLT, no cursor. `hide_self` uses `SetWindowDisplayAffinity` with exclude-from-capture, but only on build 19041 (2004) and up, older ones black the window out instead. `GOOME_CAP_HIDE_SELF` only shows up once that actually worked.

### macOS (src/macos.c)

One borderless nonactivating NSPanel per screen at screensaver level, on all spaces and next to fullscreen apps. Its plain C calling objc_msgSend, with blocks built by hand, so any C compiler works.

Click-through flips `ignoresMouseEvents` on whichever screen the mouse is over a hit on, same polling deal as Windows. A missed click cant be passed on without accessibility access, so it just goes nowhere.

Capture is ScreenCaptureKit (`SCScreenshotManager`, macOS 14+) with the cursor off, and `hide_self` passes our windows to `excludingWindows`. Hotkeys are NSEvent global + local monitors. The global one only fires with accessibility access and theres no way to tell if you have it, so `goome_hotkey` says OK either way.

### Test (src/test.c)

Fakes a whole screen. `test_frames` is a folder of `000000.ppm, 000001.ppm...` (or one .ppm that repeats, or nothing and you get a gradient with a grid). `test_shots` gets a ppm after every `goome_end` with the overlay blended on top, like a person would see it. `test_script` is what the "user" does, one step a line keyed by frame (`;` works as a line break too):

```
screen 1920x1080 2
3 click 400,300
4 key hello\n
5 hover 120,80
6 hold 200,200 9
7 scroll 400,300 -1
8 hotkey alt+h
9 send hide
end 12
```

`hold x,y end` is down on that frame and up on the end frame. Clicks off your hits get dropped like on a real screen, and keys only arrive while the keyboard is on. Blending is integer only so the shots are byte for byte the same on Linux, Windows and both kinds of Mac. CI compares them.

### Hotkeys on wayland: the control fifo

Wayland apps cant grab keys system wide, and XWayland grabs only see X windows. So if you give goome a `state_dir` it makes a fifo at `state_dir/control` and `goome_poll` turns each line written to it into a `GOOME_CONTROL` event. Your app tells the user to bind something like `bindsym alt+h exec myapp --send hide`, and `--send` calls `goome_send(state_dir, "hide")`. `goome_control_path` gives you the path to show them. It works on X11 too, it just isnt needed there.

## Weird stuff we work around

### wayland

ARGB8888 is 0 in wl_shm. open-chease used `format == 0` to mean "no format yet", and on Hyprland (which offers ARGB8888 first) every grab failed without saying anything. There's a separate have-format flag now.

10 bit panels hand out XRGB2101010 / ARGB2101010, those get squashed to 8 bits. Theres also y_invert, where row 0 is the bottom.

screencopy before v3 has no buffer_done, so you copy right after the buffer event. A sleeping screen never answers at all, grabs give up after 600 ms and destroy the frame so a late copy cant write into freed memory.

Exclusive zone has to be -1, waybar pushes the overlay around otherwise. The input region starts out empty since the first frame used to eat clicks.

Two buffers means two sets of damage. A frame drawn while one buffer was still held by the compositor left stale pixels when that buffer came back, so each one keeps its own pending rect.

Shm buffers only exist while something is drawn. Two full screen buffers per 5K output for an empty overlay was most of the idle memory.

Keyboard interactivity is double buffered and needs a commit. Also only go exclusive while needed or the game under you loses its keys.

wtype and other virtual keyboards put keys on whatever keycode they like, so keys get read by keysym when xkb is around.

Set our own cursor on every pointer enter. Roblox hides it and it stays hidden over us otherwise.

Hyprland with software cursors draws the mouse into screencopy even when you ask it not to. We ask its IPC where the mouse is and report it as `has_cursor`.

The seat is bound at v5 so axis_value120 never comes. Wheel notches come from discrete events, or from adding up axis values for touchpads.

Hotplug: unplugged slots get reused so the other monitor indexes dont move, a pending frame gets destroyed before its slot gets zeroed, and a layer surface the compositor closed gets made again on the next `goome_begin`.

Rotated outputs come back unrotated from screencopy and goome undoes it (checked all 8 against grim).

You cant keep a layer surface out of screencopy. Hyprland's noscreenshare just paints it black. So `has_self` is always 1 and you mask with `goome_alpha_at`.

### X11

No compositor means no alpha. If nobody owns `_NET_WM_CM_Sn` the window's bounding shape gets cut down to the solid pixels.

Without `_NET_WM_BYPASS_COMPOSITOR = 2` xfwm unredirects us and the screen goes black. xfwm also thinks a screen sized window is fullscreen, so it's 1 px shorter.

Raise on every commit, anything raised since then covers us.

The default X error handler calls exit(). A resolution change or a hotkey someone else already has (BadAccess) would kill the app, so theres a quiet one. Same with the io error handler, a dead display gets noticed with poll before xlib sees it.

Key grabs are exact, so each combo gets grabbed 4 times for every caps/num lock mix.

On GNOME the keyboard trick above can fail, mutter just wont focus the window sometimes. Then you get `GOOME_EDENIED` instead of a grab, because an XWayland grab would only see X windows anyway.

Depth 30 visuals get converted. Remote displays have no shared memory, XShm fails and it falls back to plain XGetImage.

### GNOME and portal

Subscribe to PipeWireStreamAdded before Start or the node id can go by before you listen.

D-Bus signals only get delivered from the glib main context, so every grab pumps `g_main_context_iteration`.

Theres no gstreamer header, so `GstMapInfo` is spelled out and has to match the real struct byte for byte.

GPU frames can have padded rows. The stride gets worked out from the buffer size instead of trusting the width.

Monitor changes come in bursts. Streams restart 700 ms after the last one, unsubscribe from Closed before Stop (or that kicks off another restart), and if mutter is still mid reconfigure it tries 3 quick times then once more about 4.7 s out.

Portal calls answer later with a Response signal on a path built from our bus name. Start waits for a person to click, so it gets minutes, not seconds.

The restore token should be a uuid. Anything else gets thrown out before it goes into a GVariant format string.

### Windows

`SendInput` takes physical buttons, so left handed mice need the swap. If the button is already up by the time we pass the click on, a lone down would stay stuck, so it sends both.

`WM_MOUSEACTIVATE` returns no-activate unless the keyboard is on. Clicking us would steal focus from the game. When the keyboard goes off, the window that had focus gets it back.

Browsers going fullscreen push us under, TOPMOST gets put back on every commit and every half second.

`DisableProcessWindowsGhosting` at startup. Without it Windows swaps in a frozen ghost copy when a frame is slow.

`WM_DISPLAYCHANGE` doesnt always come. Theres a slow look every 2 s too.

AltGr shows up as ctrl+alt, that gets folded back. `VkKeyScan` returns 0xFF in the low byte when a key isnt on this keyboard.

`GetVersionEx` lies without a manifest, the build number comes from `RtlGetVersion`. And GetTickCount wraps after 49 days, so `goome_now_ms` counts from start.

### macOS

- Cocoa y goes up from the bottom of the main display, CoreGraphics goes down from its top
- the panel only becomes key while the keyboard is on, and the view has to accept first responder or keyDown just beeps
- screen recording permission is per app, so your terminal needs it, not goome. Without it grabs return `GOOME_EDENIED` right away
- the screenshot callback can land after we gave up waiting, whoever finishes last frees it
- new screens show up in CG before NSScreen knows them, so making the panel gets retried
- no `[NSApp run]` = no autorelease pools, so theres one around every call into AppKit
- on arm64 objc_msgSend cant go through a varargs cast. every call gets its own

### everywhere

Linux builds have to run on glibc 2.34. New glibc secretly sends `sscanf`, `strtol`, `atoi` and friends to `__isoc23_*` versions that need 2.38, so numbers get parsed by hand (`gm_num`). CI runs `objdump -T` on the binaries and fails on anything past 2.34.

## Drawing, images, fonts and the ui kit

All four live in their own file (src/draw.c, src/image.c, src/font.c, src/ui.c) and sit on top of the canvas, the backends dont know about any of it. `GOOME_NO_DRAW` drops all four, `GOOME_NO_UI` only the ui.

The rule for all of them: no libraries and no libm. goome is one header you drop in, the second it needs `-lm`, zlib or freetype thats one more thing to break on someones Windows box. So the sqrt, sin, erf and friends in draw.c are small hand rolled ones, png gets its own inflate, jpeg its own huffman and integer IDCT, and fonts their own outline parser and rasterizer. The unit tests also hash the output on Linux, Windows and both Macs, and libm, fused multiply adds and freetype versions all change the last bit somewhere. So the math is either integer or done in a fixed order, and draw.c, font.c and ui.c turn off FP contraction for clang and MSVC.

**draw.c** only does coverage math on edges. Rounded rects, circles and shadows work out per row which part is fully covered and fill that as a plain span, so a big panel costs about as much as a memset. Shadows are a gaussian like falloff on the distance to the rect, like css box-shadow, not a real blur. Clipping is a 16 deep stack of whole pixel rects on the canvas, every primitive and `goome_text` respects it.

**image.c** reads PNG (every color type, bit depth, palette with alpha, interlaced), baseline and progressive JPEG, BMP, PPM and QOI into premultiplied `goome_bitmap`s. It picks the format by the first bytes. Every read is bounds checked, images are capped at 16384x16384 and the whole thing got fuzzed under ASan and UBSan with mutated files. A file it cant read gives an error string from `goome_bitmap_error`. Decoded bitmaps belong to you, keep them around instead of loading every frame.

**font.c** reads TrueType (glyf) and CFF outlines from .ttf, .otf and .ttc, kerning from GPOS or the old kern table. The rasterizer is an exact area coverage sweep in integers, so every cpu gets the same bytes. Glyphs get cached per size. `goome_font_system` asks fontconfig when libfontconfig is around (dlopen, so its not a link dependency), otherwise it looks in the usual font folders for the usual names (Segoe UI on Windows, SF / Helvetica on Mac). NULL as the font means the 6 built in bitmap fonts, so text always works even with nothing installed.

**ui.c** is immediate mode. You call the widgets every frame with your own variables, the ui only keeps small state by id (hover and press animations, scroll offsets, the text cursor, which popup is open). Widgets record draw commands per window, then `goome_ui_end` plays them back in z order and hands every window, popup and tooltip to `goome_end` as hits, so the rest of the screen stays click-through. `goome_ui_new` loads the system font into the theme and frees it in `goome_ui_free`. Heights come from the font line height and the small stuff (checkboxes, toggles, knobs) grows with the font size. On the test backend the ui uses a fixed 16 ms clock so scripted runs give the same shots every time. The unit test sets the font to NULL since system fonts differ between machines.

## Building and testing

```sh
./build.sh          # lib, examples and tests into build/
./build.sh test     # same, then runs the unit tests
./build.sh win      # windows exes with mingw into build-win/
./build.sh single   # just remakes goome.h
```

`WERROR=1` turns warnings into errors, CI always sets it. There's also a normal CMake project (`GOOME_NO_DRAW`, `GOOME_EXAMPLES`, `GOOME_TESTS` options) with presets for Visual Studio, Rider and VS Code, and `ctest` runs the unit tests.

The backend tests are scripts in tests/, each one runs the real backend somewhere it cant mess with your actual desktop:

- `tests/x11.sh` on Xvfb, once with picom and once with no compositor. `tests/x11.sh asan` builds with asan+ubsan first.
- `tests/wayland.sh` on headless sway with the pixman renderer, checked with grim. `SAN=1` for sanitizers.
- `tests/gnome.sh` grabs from the live GNOME session through mutter (nothing gets drawn or pops up), then runs the portal code against tests/fakeportal.py on a private bus.
- `tests/win32.sh` runs the windows build under wine on its own Xvfb, `tests/win32.ps1` is the same on real Windows.
- `tests/macos.sh` for the mac runners. They cant record the screen, so it checks grabs come back denied without hanging.

These are heavy (Xvfb, sway, wine), dont run them all at once.

CI does linux (build, unit, glibc check, mingw, cmake, x11, wayland), windows (msvc, mingw, the one file with cl, a live desktop run) and macos.

## The one file goome.h

The goome.h in the repo root gets made by `tools/single.py`. It takes include/goome.h, then pastes every src/*.c behind `#ifdef GOOME_IMPLEMENTATION` with their local includes (internal.h, fonts.h, wl_proto.h) inlined once. build.sh runs it every time if python is around. Dont edit the root goome.h by hand, change src/ and run it again. CI fails if its out of date.
