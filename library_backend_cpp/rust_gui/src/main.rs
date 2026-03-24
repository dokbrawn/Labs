use eframe::{egui, App};

struct LibraryGui {
    search: String,
}

impl Default for LibraryGui {
    fn default() -> Self {
        Self {
            search: String::new(),
        }
    }
}

impl App for LibraryGui {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Library (Rust GUI)");
            ui.label("Primary GUI migrated to Rust. Backend target: PostgreSQL.");
            ui.separator();
            ui.horizontal(|ui| {
                ui.label("Search:");
                ui.text_edit_singleline(&mut self.search);
            });
            ui.add_space(8.0);
            ui.label("UI scaffold is ready; backend command integration goes through C++ service binaries.");
        });
    }
}

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions::default();
    eframe::run_native(
        "Library Rust GUI",
        options,
        Box::new(|_cc| Ok(Box::new(LibraryGui::default()))),
    )
}
