mod ui;

use std::hint::black_box;
use std::time::Instant;

fn frame(ctx: &egui::Context, scene: fn(&mut egui::Ui, &mut ui::State), s: &mut ui::State, t: f64) -> usize {
    let out = ctx.run_ui(ui::input(t), |u| scene(u, s));
    let prims = ctx.tessellate(out.shapes, out.pixels_per_point);
    prims.len() + out.textures_delta.set.len()
}

fn measure(name: &str, scene: fn(&mut egui::Ui, &mut ui::State), frames: u32) {
    let ctx = egui::Context::default();
    let mut s = ui::State::default();
    let start = Instant::now();
    for _ in 0..100 {
        black_box(frame(&ctx, scene, &mut s, start.elapsed().as_secs_f64()));
    }
    let t0 = Instant::now();
    for _ in 0..frames {
        black_box(frame(&ctx, scene, &mut s, start.elapsed().as_secs_f64()));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    println!("{{\"lib\":\"egui\",\"scene\":\"{}\",\"frames\":{},\"ms_per_frame\":{:.5}}}", name, frames, ms / frames as f64);
}

fn main() {
    let frames: u32 = std::env::args().nth(1).and_then(|a| a.parse().ok()).unwrap_or(2000);
    measure("hello", ui::hello, frames);
    measure("dash", ui::dash, frames);
}
