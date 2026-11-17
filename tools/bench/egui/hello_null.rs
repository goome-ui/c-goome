mod ui;

fn main() {
    let ctx = egui::Context::default();
    let mut s = ui::State::default();
    let out = ctx.run_ui(ui::input(0.0), |u| ui::hello(u, &mut s));
    let prims = ctx.tessellate(out.shapes, out.pixels_per_point);
    println!("ok {}", prims.len());
}
