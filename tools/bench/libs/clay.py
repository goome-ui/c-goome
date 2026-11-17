import cb_common


def run(ctx):
    src, sha = ctx.clone("https://github.com/nicbarker/clay.git", "v0.14")
    mine = ctx.bench / "clay"
    d = ctx.work / "clay-build"
    d.mkdir(exist_ok=True)
    out = {"version": "v0.14", "commit": sha}
    out["core_loc"] = ctx.loc([src / "clay.h"])
    out.update(cb_common.null_suite(ctx, "clay", d, False, [src, mine], [], mine / "hello.c", mine / "bench.c", ["-lm"]))
    out["hello_includes"] = "clay.h implementation + hello layout + render command array, no renderer, no font (measure text callback is a fixed 8px per char stub)"
    out["frame_includes"] = "layout build + Clay_EndLayout render command array, no rasterization"
    out["stripped_bytes_hello_real"] = {"na": "Clay is a layout library with no windowing or renderer in the core"}
    out["first_frame_real"] = {"na": "Clay is a layout library with no windowing or renderer in the core"}
    out["notes"] = [
        "Clay has no input widgets: buttons, checkboxes and sliders in the scenes are plain layout boxes with text and colored rectangles, with no hover, click or drag handling",
        "checkbox and slider state is drawn from plain arrays, there is no interaction code to measure",
        "row labels are formatted with snprintf every frame because Clay text needs memory that outlives the layout pass",
        "Clay has no font: the measure text callback is a fixed-size stub (8px per char)",
        "Clay output is a command array of rectangles and text, a renderer would still have to draw it",
        "all 50 dash rows are submitted every frame; Clay lays them all out but leaves offscreen elements out of the command array",
        "built with -std=gnu99 -Os (frame_ms) and -O2 (frame_ms_O2) with -ffunction-sections and gc-sections",
    ]
    return out
