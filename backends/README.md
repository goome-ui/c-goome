<h1 align="center">🔌 goome backends 🔌</h1>

<p align="center">
These let you drop goome into something that already has a window and a renderer, like a game you're modding or your own engine. Same idea as Dear ImGui's backends.<br>
A <b>platform</b> backend feeds goome the input from whatever owns the window. A <b>renderer</b> backend draws the goome frame on top of whatever the game draws, as one texture with premultiplied alpha.<br>
Pick one of each. They're optional, goome.h itself still needs nothing.
</p>

## How to use one

Each one is a single header, stb style. Include it anywhere, and in exactly one .c file define the IMPLEMENTATION macro before you include it. Open goome in offscreen mode first, the backends work on that.

```c
#define GOOME_IMPLEMENTATION
#include "goome.h"
#define GOOME_IMPL_GLFW_IMPLEMENTATION
#include "goome_glfw.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"

goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 1280, .height = 720}, &err);
goome_ui *ui = goome_ui_new(g);
```

## Eaten input

Every platform backend tells you if goome ate an event. If it did, dont give it to the game. That's how dragging a slider doesn't also spin the game camera.

- mouse: eaten when it's over something goome drew (a window, a widget), or while a press that started on goome is still held
- keys and text: eaten while goome wants the keyboard (an input box has focus, or a modal is open). Needs `set_ui`
- everything still goes to goome either way, so clicking off an input box unfocuses it

## Platform backends

| file | for | eaten comes back from |
|---|---|---|
| `goome_win32.h` | any Win32 window, the usual way to mod a windows game | `goome_win32_wndproc(..., &eaten)` |
| `goome_glfw.h` | GLFW 3 | the callbacks it installs, or the `goome_glfw_*` functions |
| `goome_sdl2.h` | SDL 2 | `goome_sdl2_event` |
| `goome_sdl3.h` | SDL 3 | `goome_sdl3_event` |

All of them also have `_set_ui`, `_frame` (call it once a frame, it handles size changes, the cursor shape and the clipboard) and `_shutdown`.

**Win32**, subclass the game window and ask goome first:
```c
static WNDPROC game_proc;
static LRESULT CALLBACK my_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    int eaten;
    LRESULT r = goome_win32_wndproc(g, h, m, w, l, &eaten);
    return eaten ? r : CallWindowProcW(game_proc, h, m, w, l);
}

goome_win32_init(g, hwnd);
goome_win32_set_ui(g, ui);
game_proc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)my_proc);
```
It eats WM_INPUT too, so games that read raw mouse input dont see the drag either.

**GLFW**, pass 1 and it puts its own callbacks in front of yours. Yours only get called when goome didn't eat it:
```c
goome_glfw_init(g, window, 1);
goome_glfw_set_ui(g, ui);
```
If you'd rather keep your callbacks, pass 0 and call `goome_glfw_cursor_pos`, `_mouse_button`, `_scroll`, `_key`, `_char` from them yourself. They return 1 when it's eaten.

**SDL2 / SDL3**
```c
goome_sdl2_init(g, window);
goome_sdl2_set_ui(g, ui);
while (SDL_PollEvent(&e))
    if (!goome_sdl2_event(g, &e)) game_handle(&e);
```
SDL3 is the same with `sdl3` in the names.

## Renderer backends

| file | for | needs |
|---|---|---|
| `goome_gl3.h` | OpenGL 3.3 core and GLES 3 | nothing, it loads GL itself |
| `goome_gl2.h` | old OpenGL 2 games with fixed function (works on 1.1 too) | nothing, same |
| `goome_dx9.h` | Direct3D 9 | the game's `IDirect3DDevice9` |
| `goome_dx11.h` | Direct3D 11 | the device and immediate context |
| `goome_dx12.h` | Direct3D 12 | the device, frames in flight, rtv format and one srv slot in your heap. `render` takes the command list |
| `goome_vulkan.h` | Vulkan | a `goome_vulkan_info` with the device, queue and render pass. `render` takes the command buffer, inside the render pass |
| `goome_metal.h` | Metal on macOS | the `MTLDevice`. `render` takes the pass descriptor and the encoder |

The GL ones take a loader. Pass whatever your window lib gives you, or NULL and it uses wglGetProcAddress / glXGetProcAddress / eglGetProcAddress by itself. No glad, no glew.

```c
goome_gl3_init(g, glfwGetProcAddress);

while (running) {
    glfwPollEvents();
    goome_glfw_frame(g);
    if (goome_ui_begin(ui, 0)) {
        draw_my_menu(ui);
        goome_ui_end(ui);
    }
    draw_the_game();
    goome_gl3_render(g);
    glfwSwapBuffers(window);
}
```

`render` only uploads the part of the frame that changed (`goome_frame_dirty`), then draws one quad over the current viewport. It saves the GL state it touches and puts it back after, so the game's renderer doesn't notice. `goome_gl2_*` works the exact same way. The DirectX, Vulkan and Metal ones are the same `_init`, `_render`, `_resize`, `_shutdown` set, they draw on whatever the game is drawing to when you call `render`, the bound target or the pass you hand it. Ready made DLL mod menus that use them are in [goome-examples/mods/native](https://github.com/goome-ui/examples/tree/main/mods/native). For SDL2 wrap `SDL_GL_GetProcAddress` since it returns a `void *`.

## Your own renderer

If your engine has its own texture api, you dont need a backend at all:
```c
goome_rect r;
if (goome_frame_dirty(g, &r)) {
    goome_frame_copy(g, pixels, r.w * 4, GOOME_FMT_RGBA_PREMUL, &r);
    upload_to_texture(pixels, r.x, r.y, r.w, r.h);
}
```
Then draw it with blend `one, one minus src alpha`. Use `GOOME_FMT_RGBA` (or BGRA) if your engine wants straight alpha.

## Tests

`bash tests/backends.sh` builds a small test per backend pair, runs the ui demo on its own Xvfb with mesa llvmpipe, reads the pixels back and checks them against `goome_frame`. It also fakes a slider drag and a click on empty space to check what gets eaten. Pairs you dont have the dev files for get skipped. The win32 one runs in wine if you have mingw and wine.
