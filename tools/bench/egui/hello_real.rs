mod ui;

use std::io::Write;

struct App {
    s: ui::State,
    frames: u32,
}

impl eframe::App for App {
    fn ui(&mut self, u: &mut egui::Ui, _frame: &mut eframe::Frame) {
        ui::hello(u, &mut self.s);
        self.frames += 1;
        if self.frames == 1 {
            u.ctx().request_repaint();
        } else if self.frames == 2 {
            println!("READY");
            let _ = std::io::stdout().flush();
        }
    }
}

fn main() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([480.0, 200.0]).with_title("Hello"),
        ..Default::default()
    };
    eframe::run_native("Hello", options, Box::new(|_cc| Ok(Box::new(App { s: ui::State::default(), frames: 0 }))))
}
