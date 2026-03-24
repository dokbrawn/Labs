use eframe::{egui, App, NativeOptions};
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
    publisher: String,
    year: i32,
    format: String,
    rating: f32,
    price: f32,
    age_rating: String,
    isbn: String,
    total_circulation: i64,
    print_sign_date: String,
    additional_prints: String,
    cover_image_path: String,
    license_image_path: String,
    bibliographic_ref: String,
    created_at: String,
}

#[derive(Clone, Copy, PartialEq)]
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

#[derive(Clone, Debug, Default)]
struct OpenLibraryDoc {
    title: String,
    author: String,
    first_publish_year: i32,
    isbn: String,
    publisher: String,
    language: String,
    cover_url: String,
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
    add_publisher: String,
    add_year: String,
    add_format: String,
    add_rating: String,
    add_price: String,
    add_age_rating: String,
    add_isbn: String,
    add_total_circulation: String,
    add_print_sign_date: String,
    add_additional_prints: String,
    add_license_image_path: String,

    api_search_query: String,
    api_results: Vec<OpenLibraryDoc>,
    show_api_results: bool,
}

impl Default for LibraryGui {
    fn default() -> Self {
        Self {
            pg_conn: env::var("LIBRARY_PG_CONN").unwrap_or_else(|_| {
                "host=localhost port=5432 dbname=library user=postgres password=123".to_string()
            }),
            books: Vec::new(),
            shown_books: Vec::new(),
            status: "Нажмите Init + Refresh".to_string(),
            query: String::new(),
            selected_genre: "Все".to_string(),
            selected_subgenre: "Все".to_string(),
            sort_field: SortField::Title,
            sort_asc: true,

            add_title: String::new(),
            add_author: String::new(),
            add_genre: String::new(),
            add_subgenre: String::new(),
            add_publisher: String::new(),
            add_year: "0".to_string(),
            add_format: String::new(),
            add_rating: "0".to_string(),
            add_price: "0".to_string(),
            add_age_rating: "12+".to_string(),
            add_isbn: String::new(),
            add_total_circulation: "0".to_string(),
            add_print_sign_date: String::new(),
            add_additional_prints: "[]".to_string(),
            add_license_image_path: String::new(),
            api_search_query: String::new(),
            api_results: Vec::new(),
            show_api_results: false,
        }
    }
}

fn backend_binary_path() -> PathBuf {
    let exe = if cfg!(target_os = "windows") {
        "library_backend.exe"
    } else {
        "library_backend"
    };

    let candidates = vec![
        PathBuf::from("../build").join(exe),
        PathBuf::from("../build/Debug").join(exe),
        PathBuf::from("../build/Release").join(exe),
        PathBuf::from("build").join(exe),
    ];

    candidates
        .into_iter()
        .find(|p| p.exists())
        .unwrap_or_else(|| PathBuf::from("../build").join(exe))
}

fn run_backend(pg_conn: &str, args: &[&str]) -> Result<String, String> {
    let bin = backend_binary_path();
    let parent = bin.parent().unwrap_or_else(|| Path::new(".."));

    let output = Command::new(&bin)
        .args(args)
        .env("LIBRARY_PG_CONN", pg_conn)
        .env("LIBRARY_ENABLE_NET", "1")
        .env("OFFLINE_MODE", "0")
        .current_dir(parent)
        .output()
        .map_err(|e| format!("Не удалось запустить backend: {}", e))?;

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    if !output.status.success() && !stderr.contains("NOTICE") && !stderr.contains("ЗАМЕЧАНИЕ")
    {
        Err(stderr)
    } else {
        Ok(stdout)
    }
}

fn decode(raw: &str) -> String {
    raw.replace("\\n", "\n")
        .replace("\\r", "\r")
        .replace("\\=", "=")
        .replace("\\\\", "\\")
}

fn parse_books(payload: &str) -> Vec<Book> {
    let mut books = Vec::new();
    let mut current: Option<Book> = None;

    for line in payload.lines() {
        if line == "BEGIN_BOOK" {
            current = Some(Book::default());
            continue;
        }

        if line == "END_BOOK" {
            if let Some(book) = current.take() {
                books.push(book);
            }
            continue;
        }

        let Some(book) = current.as_mut() else {
            continue;
        };
        let Some((k, v)) = line.split_once('=') else {
            continue;
        };

        let value = decode(v);

        match k {
            "id" => book.id = value.parse().unwrap_or(0),
            "title" => book.title = value,
            "author" => book.author = value,
            "genre" => book.genre = value,
            "subgenre" => book.subgenre = value,
            "publisher" => book.publisher = value,
            "year" => book.year = value.parse().unwrap_or(0),
            "format" => book.format = value,
            "rating" => book.rating = value.parse().unwrap_or(0.0),
            "price" => book.price = value.parse().unwrap_or(0.0),
            "age_rating" => book.age_rating = value,
            "isbn" => book.isbn = value,
            "total_circulation" => book.total_circulation = value.parse().unwrap_or(0),
            "print_sign_date" => book.print_sign_date = value,
            "additional_prints" => book.additional_prints = value,
            "cover_image_path" => book.cover_image_path = value,
            "license_image_path" => book.license_image_path = value,
            "bibliographic_ref" => book.bibliographic_ref = value,
            "created_at" => book.created_at = value,
            _ => {}
        }
    }

    books
}

fn escape_value(v: &str) -> String {
    v.replace('\\', "\\\\")
        .replace('\n', "\\n")
        .replace('\r', "\\r")
        .replace('=', "\\=")
}

fn write_book_file(gui: &LibraryGui) -> Result<PathBuf, String> {
    let ts = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| e.to_string())?
        .as_millis();

    let path = env::temp_dir().join(format!("library_add_{}.book", ts));

    let payload = format!(
        "BEGIN_BOOK\n\
         title={}\n\
         author={}\n\
         genre={}\n\
         subgenre={}\n\
         publisher={}\n\
         year={}\n\
         format={}\n\
         rating={}\n\
         price={}\n\
         age_rating={}\n\
         isbn={}\n\
         total_circulation={}\n\
         print_sign_date={}\n\
         additional_prints={}\n\
         license_image_path={}\n\
         END_BOOK\n",
        escape_value(&gui.add_title),
        escape_value(&gui.add_author),
        escape_value(&gui.add_genre),
        escape_value(&gui.add_subgenre),
        escape_value(&gui.add_publisher),
        escape_value(&gui.add_year),
        escape_value(&gui.add_format),
        escape_value(&gui.add_rating),
        escape_value(&gui.add_price),
        escape_value(&gui.add_age_rating),
        escape_value(&gui.add_isbn),
        escape_value(&gui.add_total_circulation),
        escape_value(&gui.add_print_sign_date),
        escape_value(&gui.add_additional_prints),
        escape_value(&gui.add_license_image_path),
    );

    fs::write(&path, payload).map_err(|e| e.to_string())?;
    Ok(path)
}

fn parse_openlibrary_candidates(payload: &str) -> Vec<OpenLibraryDoc> {
    let mut docs = Vec::new();
    let mut current: Option<OpenLibraryDoc> = None;

    for line in payload.lines() {
        if line == "BEGIN_CANDIDATE" {
            current = Some(OpenLibraryDoc::default());
            continue;
        }
        if line == "END_CANDIDATE" {
            if let Some(doc) = current.take() {
                docs.push(doc);
            }
            continue;
        }

        let Some(doc) = current.as_mut() else {
            continue;
        };
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let value = decode(value);
        match key {
            "title" => doc.title = value,
            "author" => doc.author = value,
            "publisher" => doc.publisher = value,
            "language" => doc.language = value,
            "isbn" => doc.isbn = value,
            "cover_url" => doc.cover_url = value,
            "year" => doc.first_publish_year = value.parse().unwrap_or(0),
            _ => {}
        }
    }

    docs
}

impl LibraryGui {
    fn init_backend(&mut self) {
        match run_backend(&self.pg_conn, &["init"]) {
            Ok(_) => {
                self.status = "База данных инициализирована".to_string();
                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка init: {}", e.trim()),
        }
    }

    fn refresh_books(&mut self) {
        match run_backend(&self.pg_conn, &["list"]) {
            Ok(out) => {
                self.books = parse_books(&out);
                self.apply_filters_local();
                self.status = format!("Книг в библиотеке: {}", self.books.len());
            }
            Err(e) => self.status = format!("Ошибка list: {}", e.trim()),
        }
    }

    fn apply_filters_local(&mut self) {
        let mut filtered = self.books.clone();

        if self.selected_genre != "Все" {
            filtered.retain(|b| b.genre == self.selected_genre);
        }

        if self.selected_subgenre != "Все" {
            filtered.retain(|b| b.subgenre == self.selected_subgenre);
        }

        if !self.query.trim().is_empty() {
            let q = self.query.trim();
            match run_backend(&self.pg_conn, &["search", q]) {
                Ok(out) => {
                    let mut searched = parse_books(&out);

                    if self.selected_genre != "Все" {
                        searched.retain(|b| b.genre == self.selected_genre);
                    }
                    if self.selected_subgenre != "Все" {
                        searched.retain(|b| b.subgenre == self.selected_subgenre);
                    }

                    self.shown_books = searched;
                }
                Err(e) => {
                    self.status = format!("Ошибка search: {}", e.trim());
                    self.shown_books = filtered;
                }
            }
        } else {
            self.shown_books = filtered;
        }

        self.sort_books_via_backend();
    }

    fn sort_books_via_backend(&mut self) {
        let order = if self.sort_asc { "asc" } else { "desc" };

        match run_backend(
            &self.pg_conn,
            &["sort", self.sort_field.backend_name(), order],
        ) {
            Ok(out) => {
                let mut sorted = parse_books(&out);

                if self.selected_genre != "Все" {
                    sorted.retain(|b| b.genre == self.selected_genre);
                }
                if self.selected_subgenre != "Все" {
                    sorted.retain(|b| b.subgenre == self.selected_subgenre);
                }

                if !self.query.trim().is_empty() {
                    let ids: BTreeSet<i32> = self.shown_books.iter().map(|b| b.id).collect();
                    sorted.retain(|b| ids.contains(&b.id));
                }

                self.shown_books = sorted;
            }
            Err(e) => {
                self.status = format!("Ошибка sort: {}", e.trim());
            }
        }
    }

    fn add_book(&mut self) {
        if self.add_title.trim().is_empty() || self.add_author.trim().is_empty() {
            self.status = "Введите название и автора".to_string();
            return;
        }

        let file = match write_book_file(self) {
            Ok(p) => p,
            Err(e) => {
                self.status = format!("Ошибка файла: {}", e);
                return;
            }
        };

        let result = run_backend(
            &self.pg_conn,
            &["upsert", file.to_str().unwrap(), "--fetch-network"],
        );

        let _ = fs::remove_file(file);

        match result {
            Ok(out) => {
                let msg = out.trim();
                if msg.is_empty() {
                    self.status = "Книга успешно добавлена".to_string();
                } else {
                    self.status = msg.to_string();
                }

                self.add_title.clear();
                self.add_author.clear();
                self.add_genre.clear();
                self.add_subgenre.clear();
                self.add_publisher.clear();
                self.add_year = "0".to_string();
                self.add_format.clear();
                self.add_rating = "0".to_string();
                self.add_price = "0".to_string();
                self.add_age_rating = "12+".to_string();
                self.add_isbn.clear();
                self.add_total_circulation = "0".to_string();
                self.add_print_sign_date.clear();
                self.add_additional_prints = "[]".to_string();
                self.add_license_image_path.clear();

                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка добавления: {}", e.trim()),
        }
    }

    fn search_books_in_openlibrary(&mut self) {
        let query = self.api_search_query.trim();
        if query.is_empty() {
            self.status = "Введите запрос для OpenLibrary".to_string();
            return;
        }

        match run_backend(&self.pg_conn, &["lookup", query, "15"]) {
            Ok(payload) => {
                self.api_results = parse_openlibrary_candidates(&payload);
                self.show_api_results = true;
                if self.api_results.is_empty() {
                    self.status = "По API ничего не найдено".to_string();
                } else {
                    self.status = format!(
                        "Найдено {} вариантов из OpenLibrary",
                        self.api_results.len()
                    );
                }
            }
            Err(error) => {
                self.status = format!("Поиск в OpenLibrary не удался: {error}");
                self.api_results.clear();
                self.show_api_results = false;
            }
        }
    }

    fn apply_openlibrary_doc(&mut self, doc: &OpenLibraryDoc) {
        if !doc.title.trim().is_empty() {
            self.add_title = doc.title.clone();
        }

        if !doc.author.trim().is_empty() {
            self.add_author = doc.author.clone();
        }

        if doc.first_publish_year > 0 {
            self.add_year = doc.first_publish_year.to_string();
        }

        if !doc.isbn.trim().is_empty() {
            self.add_isbn = doc.isbn.clone();
        }

        if !doc.publisher.trim().is_empty() {
            self.add_publisher = doc.publisher.clone();
        }

        if !doc.cover_url.trim().is_empty() {
            self.add_license_image_path = doc.cover_url.clone();
        }

        if self.add_format.trim().is_empty() {
            self.add_format = "Печатная книга".to_string();
        }

        self.status = "Данные из OpenLibrary перенесены в форму".to_string();
    }

    fn remove_book(&mut self, id: i32) {
        match run_backend(&self.pg_conn, &["remove", &id.to_string()]) {
            Ok(_) => {
                self.status = format!("Книга {} удалена", id);
                self.refresh_books();
            }
            Err(e) => self.status = format!("Ошибка удаления: {}", e.trim()),
        }
    }

    fn genres(&self) -> Vec<String> {
        let mut set = BTreeSet::new();
        set.insert("Все".to_string());
        for b in &self.books {
            if !b.genre.trim().is_empty() {
                set.insert(b.genre.clone());
            }
        }
        set.into_iter().collect()
    }

    fn subgenres(&self) -> Vec<String> {
        let mut set = BTreeSet::new();
        set.insert("Все".to_string());

        for b in &self.books {
            if (self.selected_genre == "Все" || b.genre == self.selected_genre)
                && !b.subgenre.trim().is_empty()
            {
                set.insert(b.subgenre.clone());
            }
        }

        set.into_iter().collect()
    }
}

impl App for LibraryGui {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        ctx.set_visuals(egui::Visuals::dark());

        egui::TopBottomPanel::top("header").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.heading("📚 Library");
                ui.label("Rust GUI + PostgreSQL backend");
                ui.separator();

                ui.label("PG_CONN:");
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

        egui::SidePanel::left("sidebar")
            .resizable(true)
            .default_width(280.0)
            .show(ctx, |ui| {
                ui.heading("Фильтры");

                ui.label("Жанр");
                egui::ComboBox::from_id_salt("genre")
                    .selected_text(&self.selected_genre)
                    .show_ui(ui, |ui| {
                        for g in self.genres() {
                            if ui.selectable_label(self.selected_genre == g, &g).clicked() {
                                self.selected_genre = g;
                                self.selected_subgenre = "Все".to_string();
                                self.apply_filters_local();
                            }
                        }
                    });

                ui.label("Поджанр");
                egui::ComboBox::from_id_salt("subgenre")
                    .selected_text(&self.selected_subgenre)
                    .show_ui(ui, |ui| {
                        for sg in self.subgenres() {
                            if ui
                                .selectable_label(self.selected_subgenre == sg, &sg)
                                .clicked()
                            {
                                self.selected_subgenre = sg;
                                self.apply_filters_local();
                            }
                        }
                    });

                ui.separator();
                ui.label("Поиск");
                if ui.text_edit_singleline(&mut self.query).changed() {
                    self.apply_filters_local();
                }

                ui.separator();
                ui.label("Сортировка");
                egui::ComboBox::from_id_salt("sort")
                    .selected_text(self.sort_field.label())
                    .show_ui(ui, |ui| {
                        for f in [
                            SortField::Title,
                            SortField::Author,
                            SortField::Year,
                            SortField::Rating,
                            SortField::Price,
                        ] {
                            if ui
                                .selectable_label(self.sort_field == f, f.label())
                                .clicked()
                            {
                                self.sort_field = f;
                                self.apply_filters_local();
                            }
                        }
                    });

                ui.horizontal(|ui| {
                    if ui.selectable_label(self.sort_asc, "↑ ASC").clicked() {
                        self.sort_asc = true;
                        self.apply_filters_local();
                    }
                    if ui.selectable_label(!self.sort_asc, "↓ DESC").clicked() {
                        self.sort_asc = false;
                        self.apply_filters_local();
                    }
                });

                ui.separator();
                ui.heading("Добавить книгу");

                ui.horizontal(|ui| {
                    ui.label("Поиск в OpenLibrary");
                    let changed = ui
                        .text_edit_singleline(&mut self.api_search_query)
                        .changed();
                    if ui.button("🔎 Найти").clicked()
                        || (changed && ui.input(|i| i.key_pressed(egui::Key::Enter)))
                    {
                        self.search_books_in_openlibrary();
                    }
                });
                ui.label("Введите слово/фразу → Найти → выберите книгу из списка");
                ui.separator();

                ui.text_edit_singleline(&mut self.add_title);
                ui.label("Название");

                ui.text_edit_singleline(&mut self.add_author);
                ui.label("Автор");

                ui.text_edit_singleline(&mut self.add_genre);
                ui.label("Жанр");

                ui.text_edit_singleline(&mut self.add_subgenre);
                ui.label("Поджанр");

                ui.text_edit_singleline(&mut self.add_publisher);
                ui.label("Издатель");

                ui.text_edit_singleline(&mut self.add_year);
                ui.label("Год");

                ui.text_edit_singleline(&mut self.add_format);
                ui.label("Формат");

                ui.text_edit_singleline(&mut self.add_rating);
                ui.label("Рейтинг");

                ui.text_edit_singleline(&mut self.add_price);
                ui.label("Цена");

                ui.text_edit_singleline(&mut self.add_age_rating);
                ui.label("Возрастной рейтинг");

                ui.text_edit_singleline(&mut self.add_isbn);
                ui.label("ISBN");

                ui.text_edit_singleline(&mut self.add_total_circulation);
                ui.label("Тираж");

                ui.text_edit_singleline(&mut self.add_print_sign_date);
                ui.label("Дата подписи в печать");

                ui.text_edit_singleline(&mut self.add_additional_prints);
                ui.label("Доп. тиражи JSON");

                ui.text_edit_singleline(&mut self.add_license_image_path);
                ui.label("Путь к лицензии");

                if ui.button("➕ Добавить книгу").clicked() {
                    self.add_book();
                }
            });

        if self.show_api_results {
            egui::Window::new("Результаты OpenLibrary")
                .collapsible(false)
                .resizable(true)
                .default_size([760.0, 420.0])
                .open(&mut self.show_api_results)
                .show(ctx, |ui| {
                    ui.label("Выберите подходящую книгу — поля формы заполнятся автоматически.");
                    ui.separator();

                    egui::ScrollArea::vertical().show(ui, |ui| {
                        for doc in self.api_results.clone() {
                            let authors = if doc.author.trim().is_empty() {
                                "Неизвестный автор".to_string()
                            } else {
                                doc.author.clone()
                            };

                            let year = if doc.first_publish_year > 0 {
                                doc.first_publish_year.to_string()
                            } else {
                                "—".to_string()
                            };

                            let language = if doc.language.trim().is_empty() {
                                "—".to_string()
                            } else {
                                doc.language.clone()
                            };

                            ui.group(|ui| {
                                ui.horizontal(|ui| {
                                    ui.vertical(|ui| {
                                        ui.label(egui::RichText::new(&doc.title).strong());
                                        ui.label(format!("Автор: {authors}"));
                                        ui.label(format!("Год: {year}"));
                                        ui.label(format!("Язык: {language}"));
                                        if !doc.isbn.trim().is_empty() {
                                            ui.label(format!("ISBN: {}", doc.isbn));
                                        }
                                    });

                                    ui.with_layout(
                                        egui::Layout::right_to_left(egui::Align::Center),
                                        |ui| {
                                            if ui.button("Выбрать").clicked() {
                                                self.apply_openlibrary_doc(&doc);
                                                self.show_api_results = false;
                                            }
                                        },
                                    );
                                });
                            });
                            ui.add_space(8.0);
                        }
                    });
                });
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading(format!("Книги ({})", self.shown_books.len()));

            egui::ScrollArea::vertical().show(ui, |ui| {
                let card_width = 280.0;
                let cols = (ui.available_width() / card_width).max(1.0) as usize;

                let mut i = 0;
                while i < self.shown_books.len() {
                    ui.horizontal_wrapped(|ui| {
                        for _ in 0..cols {
                            if i >= self.shown_books.len() {
                                break;
                            }

                            let b = &self.shown_books[i];

                            egui::Frame::group(ui.style())
                                .fill(egui::Color32::from_rgb(30, 35, 50))
                                .stroke(egui::Stroke::new(
                                    1.0,
                                    egui::Color32::from_rgb(70, 80, 110),
                                ))
                                .corner_radius(10.0)
                                .inner_margin(12.0)
                                .show(ui, |ui| {
                                    ui.set_min_width(250.0);
                                    ui.set_min_height(240.0);

                                    ui.label(egui::RichText::new(&b.title).strong().size(17.0));
                                    ui.label(format!("✍ {}", b.author));
                                    ui.label(format!("🏷 {} / {}", b.genre, b.subgenre));
                                    ui.label(format!("📅 {}", b.year));
                                    ui.label(format!("⭐ {:.1}", b.rating));
                                    ui.label(format!("💰 {:.2}", b.price));
                                    ui.label(format!("Формат: {}", b.format));
                                    ui.label(format!("Возраст: {}", b.age_rating));

                                    if !b.isbn.is_empty() {
                                        ui.label(format!("ISBN: {}", b.isbn));
                                    }
                                    if !b.publisher.is_empty() {
                                        ui.label(format!("Издатель: {}", b.publisher));
                                    }
                                    if !b.print_sign_date.is_empty() {
                                        ui.label(format!("В печать: {}", b.print_sign_date));
                                    }
                                    if !b.bibliographic_ref.is_empty() {
                                        ui.label(format!("ГОСТ: {}", b.bibliographic_ref));
                                    }

                                    if ui.button("🗑 Удалить").clicked() {
                                        self.remove_book(b.id);
                                    }
                                });

                            i += 1;
                        }
                    });

                    ui.add_space(12.0);
                }
            });
        });
    }
}

fn main() -> eframe::Result<()> {
    let options = NativeOptions::default();

    eframe::run_native(
        "Library — PostgreSQL + Rust",
        options,
        Box::new(|_cc| Ok(Box::new(LibraryGui::default()))),
    )
}
