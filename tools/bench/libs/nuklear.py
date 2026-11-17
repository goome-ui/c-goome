import cb_common


def run(ctx):
    src, sha = ctx.clone("https://github.com/Immediate-Mode-UI/Nuklear.git", "v4.13.4")
    mine = ctx.bench / "nuklear"
    d = ctx.work / "nuklear-build"
    d.mkdir(exist_ok=True)
    out = {"version": "v4.13.4", "commit": sha}
    out["core_loc"] = ctx.loc([src / "nuklear.h"])
    out.update(cb_common.null_suite(ctx, "nuklear", d, False, [src, mine], [], mine / "hello.c", mine / "bench.c", ["-lm"]))
    out["hello_includes"] = "nuklear.h implementation with default font baking (stb_truetype + ProggyClean) and atlas bake to RGBA32, nk_init_default, no renderer"
    out["frame_includes"] = "ui build + nk_convert into vertex/element/command buffers + draw command walk, no rasterization"
    out["stripped_bytes_hello_real"] = {"na": "optional, not built: Nuklear's GLFW/SDL demo backends were skipped"}
    out["first_frame_real"] = {"na": "optional, not built: Nuklear has no windowing code"}
    out["notes"] = [
        "nk_convert runs with anti-aliasing on, which is Nuklear's default demo setting and costs extra vertices",
        "font atlas is baked once at startup (included in hello size and compile time, excluded from per-frame time)",
        "dash row uses a 4 column row template; Nuklear sliders have no label so the Value label is not drawn",
        "all 50 dash rows are submitted every frame; Nuklear skips drawing widgets that fall outside the window",
        "built with -std=gnu99 -Os (frame_ms) and -O2 (frame_ms_O2) with -ffunction-sections and gc-sections",
    ]
    return out
