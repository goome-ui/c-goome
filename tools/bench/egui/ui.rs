pub struct State {
    pub clicks: u32,
    pub vals: [f32; 50],
    pub checks: [bool; 50],
}

impl Default for State {
    fn default() -> Self {
        State { clicks: 0, vals: [0.0; 50], checks: [false; 50] }
    }
}

pub fn hello(ui: &mut egui::Ui, s: &mut State) {
    egui::CentralPanel::default().show(ui, |ui| {
        ui.label("hello world");
        if ui.button("Click me").clicked() {
            s.clicks += 1;
        }
        ui.label(format!("clicks: {}", s.clicks));
    });
}

pub fn dash(ui: &mut egui::Ui, s: &mut State) {
    egui::CentralPanel::default().show(ui, |ui| {
        for i in 0..50 {
            ui.push_id(i, |ui| {
                ui.horizontal(|ui| {
                    ui.label(format!("row {}", i));
                    ui.button("Open");
                    ui.checkbox(&mut s.checks[i], "On");
                    ui.add(egui::Slider::new(&mut s.vals[i], 0.0..=1.0).text("Value"));
                });
            });
        }
    });
}

pub fn input(t: f64) -> egui::RawInput {
    let mut raw = egui::RawInput::default();
    raw.screen_rect = Some(egui::Rect::from_min_size(egui::Pos2::ZERO, egui::vec2(960.0, 640.0)));
    raw.time = Some(t);
    raw
}
