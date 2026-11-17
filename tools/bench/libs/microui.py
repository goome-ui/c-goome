import cb_common


def run(ctx):
    src, sha = ctx.clone("https://github.com/rxi/microui.git")
    mine = ctx.bench / "microui"
    d = ctx.work / "microui-build"
    d.mkdir(exist_ok=True)
    core = [src / "src" / "microui.c"]
    out = {"version": "HEAD (no releases or tags)", "commit": sha}
    out["core_loc"] = ctx.loc(core + [src / "src" / "microui.h"])
    out.update(cb_common.null_suite(ctx, "microui", d, False, [src / "src", mine], core, mine / "hello.c", mine / "bench.c", ["-lm"]))
    out["hello_includes"] = "microui.c + hello scene + command iteration, no renderer, no font (text width callback is a fixed 8px per char stub)"
    out["frame_includes"] = "ui build + command list iteration via mu_next_command, no rasterization"
    out["stripped_bytes_hello_real"] = {"na": "optional, not built: microui ships only an SDL2 demo with its own atlas renderer"}
    out["first_frame_real"] = {"na": "optional, not built: microui has no windowing code"}
    out["notes"] = [
        "microui has no font: the text width/height callbacks are fixed-size stubs (8px wide, 16px tall), so text measurement is cheaper than a real font",
        "hello and dash windows use the default microui theme; sliders and checkboxes are drawn in the command list, the Value label is not drawn (mu_slider has no label)",
        "all 50 dash rows are submitted every frame; microui clips commands to the window rect but still runs layout for hidden rows",
        "built with -std=gnu99 -Os (frame_ms) and -O2 (frame_ms_O2) with -ffunction-sections and gc-sections",
    ]
    return out
