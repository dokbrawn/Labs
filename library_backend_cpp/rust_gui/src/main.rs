use eframe::{egui, App};
use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Clone, Debug, Default)]
struct Book {
    id: i32,
    title: String,
    author: String,
    genre: String,
    subgenre: String,
    year: i32,
    format: String,
    rating: f32,
    price: f32,
    isbn: String,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum SortField {
    Title,
    Author,
    Year,
    Rating,
    Price,
}

impl SortField {
    fn label(self) -> &'static str {
        match self {
            SortField::Title => "Название",
            SortField::Author => "Автор",
            SortField::Year => "Год",
            SortField::Rating => "Рейтинг",
            SortField::Price => "Цена",
        }
    }

    fn backend_name(self) -> &'static str {
        match self {
            SortField::Title => "title",
            SortField::Author => "author",
            SortField::Year => "year",
            SortField::Rating => "rating",
            SortField::Price => "price",
        }
    }
}

struct LibraryGui {
    pg_conn: String,
    books: Vec<Book>,
    shown_books: Vec<Book>,
    status: String,
    query: String,
    selected_genre: String,
    selected_subgenre: String,
    sort_field: SortField,
    sort_asc: bool,
    add_title: String,
    add_author: String,
    add_genre: String,
    add_subgenre: String,
    add_price: String,
    add_rating: String,
    add_year: String,
}

impl Default for LibraryGui {
    fn default() -> Self {
        Self {
            pg_conn: env::var("LIBRARY_PG_CONN").unwrap_or_else(|_| {
                "host=localhost port=5432 dbname=library user=postgres password=postgres".to_string()
            }),
            books: Vec::new(),
            shown_books: Vec::new(),
            status: "Нажмите Init + Refresh.".to_string(),
            query: String::new(),
            selected_genre: "Все".to_string(),
            selected_subgenre: "Все".to_string(),
            sort_field: SortField::Title,
            sort_asc: true,
            add_title: String::new(),
            add_author: String::new(),
            add_genre: String::new(),
            add_subgenre: String::new(),
            add_price: "0".to_string(),
            add_rating: "0".to_string(),
            add_year: "0".to_string(),
        }
    }
}

fn backend_binary_path() -> PathBuf {
    let exe = if cfg!(target_os = "windows") {
        "library_backend.exe"
    } else {
        "library_backend"
    };
    let mut candidates = vec![
        PathBuf::from("../build").join(exe),
        PathBuf::from("../build/Debug").join(exe),
        PathBuf::from("../build/Release").join(exe),
        PathBuf::from("../build/RelWithDebInfo").join(exe),
        PathBuf::from("../build/MinSizeRel").join(exe),
    ];
    if let Ok(path) = env::var("LIBRARY_BACKEND_BIN") {
        candidates.insert(0, PathBuf::from(path));
    }
    candidates
        .into_iter()
        .find(|p| p.exists())
        .unwrap_or_else(|| PathBuf::from("../build").join(exe))
}

fn run_backend(pg_conn: &str, args: &[&str]) -> Result<String, String> {
    let bin = backend_binary_path();
    let parent = bin.parent().unwrap_or_else(|| Path::new(".."));
    let out = Command::new(&bin)
        .args(args)
        .env("LIBRARY_PG_CONN", pg_conn)
        .current_dir(parent)
        .output()
        .map_err(|e| format!("Не удалось запустить backend: {e}"))?;
    if out.status.success() {
        Ok(String::from_utf8_lossy(&out.stdout).to_string())
    } else {
        Err(String::from_utf8_lossy(&out.stderr).to_string())
    }
}

fn decode(raw: &str) -> String {
    raw.replace("\\n", "\n")
        .replace("\\r", "\r")
        .replace("\\=", "=")
        .replace("\\\\", "\\")
}

fn parse_books(payload: &str) -> Vec<Book> {
    let mut out = Vec::new();
    let mut current: Option<Book> = None;
    for line in payload.lines() {
        if line == "BEGIN_BOOK" {
            current = Some(Book::default());
            continue;
        }
        if line == "END_BOOK" {
            if let Some(book) = current.take() {
                out.push(book);
            }
            continue;
        }
        let Some(book) = current.as_mut() else { continue };
        let Some((k, v)) = line.split_once('=') else { continue };
        let value = decode(v);
        match k {
            "id" => book.id = value.parse().unwrap_or(0),
            "title" => book.title = value,
            "author" => book.author = value,
            "genre" => book.genre = value,
            "subgenre" => book.subgenre = value,
            "year" => book.year = value.parse().unwrap_or(0),
            "format" => book.format = value,
            "rating" => book.rating = value.parse().unwrap_or(0.0),
            "price" => book.price = value.parse().unwrap_or(0.0),
            "isbn" => book.isbn = value,
            _ => {}
        }
    }
    out
}

fn quicksort_books(books: &mut [Book], field: SortField, asc: bool) {
    if books.len() <= 1 {
        return;
    }
    let pivot = books.len() / 2;
    books.swap(pivot, books.len() - 1);
    let mut store = 0usize;
    for i in 0..books.len() - 1 {
        let less = match field {
            SortField::Title => books[i].title.to_lowercase() <= books[books.len() - 1].title.to_lowercase(),
            SortField::Author => books[i].author.to_lowercase() <= books[books.len() - 1].author.to_lowercase(),
            SortField::Year => books[i].year <= books[books.len() - 1].year,
            SortField::Rating => books[i].rating <= books[books.len() - 1].rating,
            SortField::Price => books[i].price <= books[books.len() - 1].price,
        };
        let cmp = if asc { less } else { !less };
        if cmp {
            books.swap(i, store);
            store += 1;
        }
    }
    books.swap(store, books.len() - 1);
    let (left, right) = books.split_at_mut(store);
    quicksort_books(left, field, asc);
    if right.len() > 1 {
        quicksort_books(&mut right[1..], field, asc);
    }
}

fn binary_search_by_title(sorted_by_title: &[Book], query: &str) -> Option<usize> {
    let q = query.to_lowercase();
    let mut lo = 0usize;
    let mut hi = sorted_by_title.len();
    while lo < hi {
        let mid = (lo + hi) / 2;
        let t = sorted_by_title[mid].title.to_lowercase();
        if t == q {
            return Some(mid);
        }
        if t < q {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    None
}

fn write_book_file(gui: &LibraryGui) -> Result<PathBuf, String> {
    let ts = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| e.to_string())?
        .as_millis();
    let path = env::temp_dir().join(format!("library_gui_{ts}.book"));
    let payload = format!(
        "BEGIN_BOOK\n\
         title={}\n\
         author={}\n\
         genre={}\n\
         subgenre={}\n\
         year={}\n\
         rating={}\n\
         price={}\n\
         age_rating=0+\n\
         END_BOOK\n",
        gui.add_title.replace('=', "\\="),
        gui.add_author.replace('=', "\\="),
        gui.add_genre.replace('=', "\\="),
        gui.add_subgenre.replace('=', "\\="),
        gui.add_year,
        gui.add_rating,
        gui.add_price
    );
    fs::write(&path, payload).map_err(|e| e.to_string())?;
    Ok(path)
}

impl LibraryGui {
    fn refresh_books(&mut self) {
        match run_backend(&self.pg_conn, &["list"]) {
            Ok(out) => {
                self.books = parse_books(&out);
                self.apply_filters();
                self.status = format!("Книг: {}", self.books.len());
            }
            Err(e) => self.status = format!("Ошибка list: {}", e.trim()),
        }
    }

    fn apply_filters(&mut self) {
        let mut filtered: Vec<Book> = self.books.clone();
        if self.selected_genre != "Все" {
            filtered.retain(|b| b.genre == self.selected_genre);
        }
        if self.selected_subgenre != "Все" {
            filtered.retain(|b| b.subgenre == self.selected_subgenre);
        }

        let q = self.query.trim().to_lowercase();
        if !q.is_empty() {
            let mut by_title = filtered.clone();
            quicksort_books(&mut by_title, SortField::Title, true);
            if let Some(pos) = binary_search_by_title(&by_title, &q) {
                let id = by_title[pos].id;
                filtered.retain(|b| b.id == id);
            } else {
                filtered.retain(|b| {
                    b.title.to_lowercase().contains(&q) || b.author.to_lowercase().contains(&q)
                });
            }
        }

        quicksort_books(&mut filtered, self.sort_field, self.sort_asc);
        self.shown_books = filtered;
    }

    fn init_backend(&mut self) {
        match run_backend(&self.pg_conn, &["init"]) {
            Ok(_) => {
                self.status = "Init выполнен, схема создана.".to_string();
                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка init: {}", e.trim()),
        }
    }

    fn remove_book(&mut self, id: i32) {
        match run_backend(&self.pg_conn, &["remove", &id.to_string()]) {
            Ok(_) => {
                self.status = format!("Книга id={} удалена.", id);
                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка remove: {}", e.trim()),
        }
    }

    fn add_book(&mut self) {
        if self.add_title.trim().is_empty() || self.add_author.trim().is_empty() {
            self.status = "Введите хотя бы название и автора.".to_string();
            return;
        }
        let file = match write_book_file(self) {
            Ok(path) => path,
            Err(e) => {
                self.status = format!("Ошибка подготовки файла: {e}");
                return;
            }
        };
        let file_str = file.to_string_lossy().to_string();
        let result = run_backend(&self.pg_conn, &["upsert", &file_str, "--fetch-network"]);
        let _ = fs::remove_file(file);
        match result {
            Ok(_) => {
                self.status = "Книга добавлена (с попыткой обогащения из OpenLibrary).".to_string();
                self.add_title.clear();
                self.add_author.clear();
                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка upsert: {}", e.trim()),
        }
    }

    fn genres(&self) -> Vec<String> {
        let mut s = BTreeSet::new();
        s.insert("Все".to_string());
        for b in &self.books {
            if !b.genre.trim().is_empty() {
                s.insert(b.genre.clone());
            }
        }
        s.into_iter().collect()
    }

    fn subgenres(&self) -> Vec<String> {
        let mut s = BTreeSet::new();
        s.insert("Все".to_string());
        for b in &self.books {
            if self.selected_genre == "Все" || b.genre == self.selected_genre {
                if !b.subgenre.trim().is_empty() {
                    s.insert(b.subgenre.clone());
                }
            }
        }
        s.into_iter().collect()
    }
}

impl App for LibraryGui {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let mut style = (*ctx.style()).clone();
        style.spacing.item_spacing = egui::vec2(10.0, 10.0);
        style.visuals = egui::Visuals::dark();
        ctx.set_style(style);

        egui::TopBottomPanel::top("header").show(ctx, |ui| {
            ui.heading("📚 Library — Modern Rust GUI");
            ui.label("PostgreSQL backend + иерархия жанров + быстрый поиск/сортировка");
            ui.horizontal(|ui| {
                ui.label("LIBRARY_PG_CONN:");
                ui.text_edit_singleline(&mut self.pg_conn);
                if ui.button("Init").clicked() {
                    self.init_backend();
                }
                if ui.button("Refresh").clicked() {
                    self.refresh_books();
                }
            });
            ui.colored_label(egui::Color32::LIGHT_GREEN, &self.status);
        });

        egui::SidePanel::left("left_filters")
            .resizable(true)
            .default_width(240.0)
            .show(ctx, |ui| {
                ui.heading("Фильтры");
                ui.separator();

                ui.label("Жанр");
                egui::ComboBox::from_id_salt("genre_combo")
                    .selected_text(&self.selected_genre)
                    .show_ui(ui, |ui| {
                        for g in self.genres() {
                            if ui.selectable_label(self.selected_genre == g, &g).clicked() {
                                self.selected_genre = g;
                                self.selected_subgenre = "Все".to_string();
                                self.apply_filters();
                            }
                        }
                    });

                ui.label("Поджанр");
                egui::ComboBox::from_id_salt("subgenre_combo")
                    .selected_text(&self.selected_subgenre)
                    .show_ui(ui, |ui| {
                        for sg in self.subgenres() {
                            if ui.selectable_label(self.selected_subgenre == sg, &sg).clicked() {
                                self.selected_subgenre = sg;
                                self.apply_filters();
                            }
                        }
                    });

                ui.separator();
                ui.label("Поиск (приоритет: title -> author)");
                let search = ui.text_edit_singleline(&mut self.query);
                if search.changed() {
                    self.apply_filters();
                }

                ui.separator();
                ui.label("Сортировка");
                egui::ComboBox::from_id_salt("sort_field")
                    .selected_text(self.sort_field.label())
                    .show_ui(ui, |ui| {
                        for f in [
                            SortField::Title,
                            SortField::Author,
                            SortField::Year,
                            SortField::Rating,
                            SortField::Price,
                        ] {
                            if ui.selectable_label(self.sort_field == f, f.label()).clicked() {
                                self.sort_field = f;
                                self.apply_filters();
                            }
                        }
                    });
                ui.horizontal(|ui| {
                    if ui.selectable_label(self.sort_asc, "ASC").clicked() {
                        self.sort_asc = true;
                        self.apply_filters();
                    }
                    if ui.selectable_label(!self.sort_asc, "DESC").clicked() {
                        self.sort_asc = false;
                        self.apply_filters();
                    }
                });

                ui.separator();
                ui.heading("Добавить книгу");
                ui.text_edit_singleline(&mut self.add_title).on_hover_text("Название");
                ui.text_edit_singleline(&mut self.add_author).on_hover_text("Автор");
                ui.text_edit_singleline(&mut self.add_genre).on_hover_text("Жанр");
                ui.text_edit_singleline(&mut self.add_subgenre).on_hover_text("Поджанр");
                ui.horizontal(|ui| {
                    ui.label("Год");
                    ui.text_edit_singleline(&mut self.add_year);
                });
                ui.horizontal(|ui| {
                    ui.label("Рейтинг");
                    ui.text_edit_singleline(&mut self.add_rating);
                });
                ui.horizontal(|ui| {
                    ui.label("Цена");
                    ui.text_edit_singleline(&mut self.add_price);
                });
                if ui.button("➕ Сохранить").clicked() {
                    self.add_book();
                }
            });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading(format!(
                "Карточки книг ({}) • sort: {} {}",
                self.shown_books.len(),
                self.sort_field.backend_name(),
                if self.sort_asc { "asc" } else { "desc" }
            ));
            ui.separator();

            egui::ScrollArea::vertical().show(ui, |ui| {
                let card_w = 260.0;
                let available = ui.available_width().max(card_w);
                let cols = (available / card_w).floor().max(1.0) as usize;

                let mut i = 0usize;
                while i < self.shown_books.len() {
                    ui.horizontal_wrapped(|ui| {
                        for _ in 0..cols {
                            if i >= self.shown_books.len() {
                                break;
                            }
                            let b = self.shown_books[i].clone();
                            egui::Frame::group(ui.style())
                                .fill(egui::Color32::from_rgb(26, 31, 44))
                                .stroke(egui::Stroke::new(1.0, egui::Color32::from_rgb(70, 80, 110)))
                                .corner_radius(10.0)
                                .inner_margin(10.0)
                                .show(ui, |ui| {
                                    ui.set_min_size(egui::vec2(240.0, 150.0));
                                    ui.label(egui::RichText::new(&b.title).strong().size(18.0));
                                    ui.label(format!("✍ {}", b.author));
                                    ui.label(format!("🏷 {} / {}", b.genre, b.subgenre));
                                    ui.label(format!("📅 {}   ⭐ {:.1}   💰 {:.2}", b.year, b.rating, b.price));
                                    if !b.isbn.is_empty() {
                                        ui.label(format!("ISBN: {}", b.isbn));
                                    }
                                    ui.add_space(4.0);
                                    if ui.button("🗑 Удалить").clicked() {
                                        self.remove_book(b.id);
                                    }
                                });
                            i += 1;
                        }
                    });
                    ui.add_space(8.0);
                }
            });
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
