"""
LibraryDB — десктопное приложение на Python + tkinter
СУБД: вложенная БД Жанр → Поджанр → Книги
Алгоритмы: MergeSort, бинарный поиск, дерево оптимального поиска
Хранение: JSON на диске
Сеть: Open Library API (обложки + метаданные при добавлении)
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import json
import os
import threading
import shutil
import math
import subprocess
import tempfile
from datetime import datetime
from pathlib import Path

# ── Опциональные зависимости ──────────────────────────────────────
try:
    from PIL import Image, ImageTk, ImageDraw, ImageFont
    PIL_OK = True
except ImportError:
    PIL_OK = False

try:
    import urllib.request
    import urllib.parse
    NET_OK = True
except ImportError:
    NET_OK = False

# ── Пути и константы ─────────────────────────────────────────────
BASE_DIR   = Path(__file__).parent
DATA_FILE  = BASE_DIR / "library_data.json"
COVERS_DIR = BASE_DIR / "covers"
COVERS_DIR.mkdir(exist_ok=True)
LICENSES_DIR = BASE_DIR / "licenses"
LICENSES_DIR.mkdir(exist_ok=True)
BUILD_DIR = BASE_DIR / "build"
BACKEND_BIN = BUILD_DIR / "library_backend"

CARD_W, CARD_H = 160, 240
COVER_W, COVER_H = 160, 120

GENRES = {
    "Художественная": ["Роман", "Повесть", "Рассказ", "Поэзия", "Пьеса"],
    "Научная":        ["Физика", "Химия", "Математика", "Биология", "Астрономия"],
    "Детская":        ["Сказки", "Приключения", "Познавательная", "Стихи"],
    "Техническая":    ["Программирование", "Электроника", "Механика", "Сети"],
    "Историческая":   ["Древний мир", "Средневековье", "Новое время", "XX век"],
}

AGE_COLORS = {"0+": "#22c55e", "6+": "#84cc16", "12+": "#eab308",
              "16+": "#f97316", "18+": "#ef4444"}

SORT_FIELDS = [
    ("title",     "Название"),
    ("author",    "Автор"),
    ("year",      "Год издания"),
    ("rating",    "Рейтинг"),
    ("price",     "Цена"),
    ("age",       "Возрастной рейтинг"),
]

INITIAL_BOOKS = [
    {"id": 1,  "title": "Мастер и Маргарита",       "author": "Михаил Булгаков",   "year": 1967, "genre": "Художественная", "subgenre": "Роман",           "rating": 4.9, "price": 590,  "age": "18+", "isbn": "978-5-04-116270-1", "publisher": "Эксмо",            "format": "84x108/32", "edition": 50000, "sign_date": "", "reprint_dates": [], "biblio": "Булгаков М. А. Мастер и Маргарита. — М.: Эксмо, 2023. — 480 с.", "cover_id": "12192618", "cover_file": ""},
    {"id": 2,  "title": "Преступление и наказание",  "author": "Ф. Достоевский",    "year": 1866, "genre": "Художественная", "subgenre": "Роман",           "rating": 4.7, "price": 450,  "age": "16+", "isbn": "978-5-08-006491-5", "publisher": "Дет. литература", "format": "70x100/16", "edition": 30000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8739200",  "cover_file": ""},
    {"id": 3,  "title": "Краткая история времени",   "author": "Стивен Хокинг",     "year": 1988, "genre": "Научная",        "subgenre": "Физика",          "rating": 4.8, "price": 680,  "age": "12+", "isbn": "978-5-17-077748-3", "publisher": "АСТ",              "format": "70x100/16", "edition": 25000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8575708",  "cover_file": ""},
    {"id": 4,  "title": "Гарри Поттер и фил. камень","author": "Дж. К. Роулинг",    "year": 1997, "genre": "Детская",        "subgenre": "Приключения",     "rating": 4.9, "price": 750,  "age": "6+",  "isbn": "978-5-353-01435-0", "publisher": "Росмэн",           "format": "60x90/16",  "edition": 100000,"sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "10521270", "cover_file": ""},
    {"id": 5,  "title": "Чистый код",                "author": "Роберт Мартин",     "year": 2008, "genre": "Техническая",    "subgenre": "Программирование","rating": 4.6, "price": 1200, "age": "0+",  "isbn": "978-5-4461-0960-9", "publisher": "Питер",            "format": "70x100/16", "edition": 15000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8950546",  "cover_file": ""},
    {"id": 6,  "title": "Дюна",                      "author": "Фрэнк Герберт",     "year": 1965, "genre": "Художественная", "subgenre": "Роман",           "rating": 4.8, "price": 820,  "age": "16+", "isbn": "978-5-17-090658-4", "publisher": "АСТ",              "format": "84x108/32", "edition": 40000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "10521270", "cover_file": ""},
    {"id": 7,  "title": "1984",                      "author": "Джордж Оруэлл",     "year": 1949, "genre": "Художественная", "subgenre": "Роман",           "rating": 4.7, "price": 420,  "age": "16+", "isbn": "978-5-17-108831-3", "publisher": "АСТ",              "format": "84x108/32", "edition": 60000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8575708",  "cover_file": ""},
    {"id": 8,  "title": "Война и мир",               "author": "Лев Толстой",       "year": 1869, "genre": "Историческая",  "subgenre": "Новое время",     "rating": 4.6, "price": 1100, "age": "12+", "isbn": "978-5-17-119218-3", "publisher": "АСТ",              "format": "84x108/32", "edition": 35000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "9255566",  "cover_file": ""},
    {"id": 9,  "title": "Cosmos",                    "author": "Карл Саган",        "year": 1980, "genre": "Научная",        "subgenre": "Астрономия",      "rating": 4.9, "price": 990,  "age": "12+", "isbn": "978-5-17-094029-0", "publisher": "АСТ",              "format": "70x100/8",  "edition": 20000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8739290",  "cover_file": ""},
    {"id": 10, "title": "Имя розы",                  "author": "Умберто Эко",       "year": 1980, "genre": "Историческая",  "subgenre": "Средневековье",   "rating": 4.5, "price": 650,  "age": "16+", "isbn": "978-5-389-01806-7", "publisher": "Азбука",           "format": "84x108/32", "edition": 25000, "sign_date": "", "reprint_dates": [], "biblio": "", "cover_id": "8739248",  "cover_file": ""},
]

# ─────────────────────────────────────────────────────────────────
# АЛГОРИТМЫ
# ─────────────────────────────────────────────────────────────────

def merge_sort(arr, key, reverse=False):
    """MergeSort — O(n log n)"""
    if len(arr) <= 1:
        return arr[:]
    mid = len(arr) // 2
    left  = merge_sort(arr[:mid], key, reverse)
    right = merge_sort(arr[mid:], key, reverse)
    return _merge(left, right, key, reverse)

def _merge(left, right, key, reverse):
    result, i, j = [], 0, 0
    while i < len(left) and j < len(right):
        a = left[i].get(key, "") or ""
        b = right[j].get(key, "") or ""
        if isinstance(a, str): a = a.lower()
        if isinstance(b, str): b = b.lower()
        cond = a <= b if not reverse else a >= b
        if cond:
            result.append(left[i]); i += 1
        else:
            result.append(right[j]); j += 1
    result.extend(left[i:]); result.extend(right[j:])
    return result

def binary_search(sorted_arr, query):
    """Бинарный поиск по названию (точное или частичное совпадение)"""
    q = query.lower().strip()
    lo, hi = 0, len(sorted_arr) - 1
    steps = []
    while lo <= hi:
        mid = (lo + hi) // 2
        title = sorted_arr[mid]["title"].lower()
        steps.append(mid)
        if title == q:
            return mid, steps
        elif title < q:
            lo = mid + 1
        else:
            hi = mid - 1
    # Частичное совпадение
    for i, b in enumerate(sorted_arr):
        if q in b["title"].lower():
            return i, steps
    return -1, steps

class BSTNode:
    def __init__(self, book):
        self.book  = book
        self.left  = None
        self.right = None

def build_bst(books):
    """Оптимальное BST — сортировка по названию, вес = рейтинг"""
    sorted_books = merge_sort(books, "title")
    def build(arr):
        if not arr: return None
        mid = len(arr) // 2
        node = BSTNode(arr[mid])
        node.left  = build(arr[:mid])
        node.right = build(arr[mid+1:])
        return node
    return build(sorted_books)

def bst_search(root, query):
    """Поиск в BST, возвращает путь"""
    q = query.lower().strip()
    path, node = [], root
    while node:
        path.append(node.book["title"])
        t = node.book["title"].lower()
        if t == q or q in t:
            return node.book, path
        elif q < t:
            node = node.left
        else:
            node = node.right
    return None, path

# ─────────────────────────────────────────────────────────────────
# РАБОТА С ДАННЫМИ
# ─────────────────────────────────────────────────────────────────

def _escape_backend_value(value):
    value = "" if value is None else str(value)
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("=", "\\=")


def ensure_backend_ready():
    if BACKEND_BIN.exists():
        return
    subprocess.run(["cmake", "-S", str(BASE_DIR), "-B", str(BUILD_DIR)], check=True, cwd=BASE_DIR)
    subprocess.run(["cmake", "--build", str(BUILD_DIR)], check=True, cwd=BASE_DIR)


def _backend_cmd(*args):
    ensure_backend_ready()
    completed = subprocess.run([str(BACKEND_BIN), *map(str, args)], cwd=BASE_DIR, text=True, capture_output=True, check=True)
    return completed.stdout


def _parse_backend_books(payload):
    books = []
    current = None
    for raw_line in payload.splitlines():
        line = raw_line.rstrip("\n")
        if line == "BEGIN_BOOK":
            current = {}
            continue
        if line == "END_BOOK":
            if current is not None:
                books.append(current)
            current = None
            continue
        if current is None or "=" not in line:
            continue
        key, value = line.split("=", 1)
        value = value.replace("\\n", "\n").replace("\\r", "\r").replace("\\=", "=").replace("\\\\", "\\")
        current[key] = value

    normalized = []
    for item in books:
        cover_url = item.get("cover_url", "")
        cover_id = ""
        if "/b/id/" in cover_url:
            cover_id = cover_url.split("/b/id/")[-1].split("-")[0]
        normalized.append({
            "id": int(item.get("id", 0) or 0),
            "title": item.get("title", ""),
            "author": item.get("author", ""),
            "genre": item.get("genre", ""),
            "subgenre": item.get("subgenre", ""),
            "publisher": item.get("publisher", ""),
            "year": int(float(item.get("year", 0) or 0)),
            "format": item.get("format", ""),
            "rating": float(item.get("rating", 0) or 0),
            "price": float(item.get("price", 0) or 0),
            "age": item.get("age_rating", "0+"),
            "isbn": item.get("isbn", ""),
            "edition": int(float(item.get("total_print_run", 0) or 0)),
            "sign_date": item.get("signed_to_print_date", ""),
            "reprint_dates": [v for v in item.get("additional_print_dates", "").split("|") if v],
            "cover_file": item.get("cover_image_path", ""),
            "license_file": item.get("license_image_path", ""),
            "biblio": item.get("bibliographic_reference", ""),
            "cover_url": cover_url,
            "cover_id": cover_id,
            "search_frequency": float(item.get("search_frequency", 1) or 1),
        })
    return normalized


def _book_to_backend_record(book):
    return {
        "id": book.get("id", 0),
        "title": book.get("title", ""),
        "author": book.get("author", ""),
        "genre": book.get("genre", ""),
        "subgenre": book.get("subgenre", ""),
        "publisher": book.get("publisher", ""),
        "year": book.get("year", 0),
        "format": book.get("format", ""),
        "rating": book.get("rating", 0),
        "price": book.get("price", 0),
        "age_rating": book.get("age", "0+"),
        "isbn": book.get("isbn", ""),
        "total_print_run": book.get("edition", 0),
        "signed_to_print_date": book.get("sign_date", ""),
        "additional_print_dates": "|".join(book.get("reprint_dates", [])),
        "cover_image_path": book.get("cover_file", ""),
        "license_image_path": book.get("license_file", ""),
        "bibliographic_reference": book.get("biblio", ""),
        "cover_url": book.get("cover_url", ""),
        "search_frequency": book.get("search_frequency", max(1.0, float(book.get("rating", 0) or 0))),
    }


def backend_list_books():
    return _parse_backend_books(_backend_cmd("list"))


def backend_search_books(query):
    return _parse_backend_books(_backend_cmd("search", query))


def backend_sort_books(field, ascending=True):
    return _parse_backend_books(_backend_cmd("sort", field, "asc" if ascending else "desc"))


def backend_upsert_book(book, fetch_network=True):
    record = _book_to_backend_record(book)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False, suffix=".book", dir=BASE_DIR) as tmp:
        tmp.write("BEGIN_BOOK\n")
        for key, value in record.items():
            tmp.write(f"{key}={_escape_backend_value(value)}\n")
        tmp.write("END_BOOK\n")
        tmp_path = tmp.name
    try:
        args = ["upsert", tmp_path]
        if fetch_network:
            args.append("--fetch-network")
        books = _parse_backend_books(_backend_cmd(*args))
        return books[0] if books else None
    finally:
        Path(tmp_path).unlink(missing_ok=True)


def backend_remove_book(book_id):
    _backend_cmd("remove", str(book_id))


def load_data():
    _backend_cmd("init")
    books = backend_list_books()
    if not books:
        for book in INITIAL_BOOKS:
            backend_upsert_book(book, fetch_network=False)
        books = backend_list_books()
    next_id = max((b["id"] for b in books), default=0) + 1
    return {"books": books, "next_id": next_id}


def save_data(data):
    data["books"] = backend_list_books()
    data["next_id"] = max((b["id"] for b in data["books"]), default=0) + 1

def fetch_book_info(query, callback):
    """Запрос к Open Library API в фоновом потоке"""
    def run():
        try:
            q = urllib.parse.quote(query)
            url = f"https://openlibrary.org/search.json?q={q}&limit=1&fields=title,author_name,first_publish_year,isbn,number_of_pages_median,cover_i,publisher,ratings_average"
            req = urllib.request.Request(url, headers={"User-Agent": "LibraryDB/1.0"})
            with urllib.request.urlopen(req, timeout=8) as r:
                data = json.loads(r.read())
            docs = data.get("docs", [])
            callback(docs[0] if docs else None, None)
        except Exception as e:
            callback(None, str(e))
    threading.Thread(target=run, daemon=True).start()

def download_cover(cover_id, book_id, callback):
    """Скачать обложку в фоновом потоке"""
    def run():
        try:
            url = f"https://covers.openlibrary.org/b/id/{cover_id}-M.jpg"
            dest = COVERS_DIR / f"{book_id}.jpg"
            req = urllib.request.Request(url, headers={"User-Agent": "LibraryDB/1.0"})
            with urllib.request.urlopen(req, timeout=10) as r:
                data = r.read()
            with open(dest, "wb") as f:
                f.write(data)
            callback(str(dest), None)
        except Exception as e:
            callback(None, str(e))
    threading.Thread(target=run, daemon=True).start()

def load_cover_image(path, w=COVER_W, h=COVER_H):
    """Загрузить PIL-изображение и вернуть PhotoImage"""
    if not PIL_OK or not path or not os.path.exists(path):
        return None
    try:
        img = Image.open(path).convert("RGB")
        img.thumbnail((w, h), Image.LANCZOS)
        # Центрировать на холсте нужного размера
        bg = Image.new("RGB", (w, h), (40, 40, 60))
        x = (w - img.width) // 2
        y = (h - img.height) // 2
        bg.paste(img, (x, y))
        return ImageTk.PhotoImage(bg)
    except Exception:
        return None

# ─────────────────────────────────────────────────────────────────
# ЦВЕТА И ТЕМА
# ─────────────────────────────────────────────────────────────────

DARK = {
    "bg":        "#0f0f13",
    "surface":   "#18181f",
    "surface2":  "#1e1e28",
    "border":    "#2a2a38",
    "text":      "#eeeef4",
    "muted":     "#7070a0",
    "accent":    "#6366f1",
    "accent2":   "#4f46e5",
    "danger":    "#ef4444",
    "success":   "#22c55e",
    "card":      "#1e1e2e",
    "card_hover":"#252535",
    "header":    "#13131a",
    "sidebar":   "#14141c",
}

LIGHT = {
    "bg":        "#f5f4f0",
    "surface":   "#ffffff",
    "surface2":  "#f0eff8",
    "border":    "#e2e1ea",
    "text":      "#1a1a2e",
    "muted":     "#6b6b8a",
    "accent":    "#4f46e5",
    "accent2":   "#3730a3",
    "danger":    "#dc2626",
    "success":   "#16a34a",
    "card":      "#ffffff",
    "card_hover":"#f5f4ff",
    "header":    "#ffffff",
    "sidebar":   "#fafafa",
}

T = DARK  # текущая тема (глобально)

def stars_text(rating):
    """★★★★☆ из рейтинга"""
    full  = int(rating)
    half  = 1 if (rating - full) >= 0.5 else 0
    empty = 5 - full - half
    return "★" * full + ("½" if half else "") + "☆" * empty

# ─────────────────────────────────────────────────────────────────
# ДИАЛОГ: ДОБАВИТЬ / РЕДАКТИРОВАТЬ КНИГУ
# ─────────────────────────────────────────────────────────────────

class BookDialog(tk.Toplevel):
    def __init__(self, master, book=None, on_save=None):
        super().__init__(master)
        self.on_save  = on_save
        self.editing  = book is not None
        self.book     = dict(book) if book else {}
        self.result   = None
        self._photo   = None  # keep reference

        self.title("Редактировать книгу" if self.editing else "Добавить книгу")
        self.configure(bg=T["surface"])
        self.resizable(True, True)
        self.geometry("780x680")
        self.minsize(680, 580)
        self.transient(master)
        self.grab_set()

        self._build()
        self._fill_fields()
        self.center()

    def center(self):
        self.update_idletasks()
        pw = self.master.winfo_width()
        ph = self.master.winfo_height()
        px = self.master.winfo_x()
        py = self.master.winfo_y()
        w, h = self.winfo_width(), self.winfo_height()
        self.geometry(f"+{px+(pw-w)//2}+{py+(ph-h)//2}")

    def _build(self):
        # Header
        hdr = tk.Frame(self, bg=T["header"], height=52)
        hdr.pack(fill="x")
        hdr.pack_propagate(False)
        lbl_title = "✏  Редактировать книгу" if self.editing else "＋  Добавить книгу"
        tk.Label(hdr, text=lbl_title, bg=T["header"], fg=T["text"],
                 font=("Georgia", 14, "bold")).pack(side="left", padx=20, pady=14)
        tk.Button(hdr, text="✕", bg=T["header"], fg=T["muted"],
                  relief="flat", cursor="hand2", font=("Arial", 14),
                  command=self.destroy).pack(side="right", padx=12)
        tk.Frame(self, bg=T["border"], height=1).pack(fill="x")

        # Body
        body = tk.Frame(self, bg=T["surface"])
        body.pack(fill="both", expand=True)

        # LEFT — cover
        left = tk.Frame(body, bg=T["surface"], width=200)
        left.pack(side="left", fill="y", padx=20, pady=20)
        left.pack_propagate(False)

        tk.Label(left, text="ОБЛОЖКА", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 9, "bold")).pack(anchor="w")

        self.cover_canvas = tk.Canvas(left, width=160, height=210,
                                      bg=T["surface2"], highlightthickness=1,
                                      highlightbackground=T["border"])
        self.cover_canvas.pack(pady=(6, 8))
        self.cover_canvas.bind("<Button-1>", lambda e: self._pick_cover_file())
        self._draw_cover_placeholder()

        tk.Button(left, text="📁  Выбрать файл", command=self._pick_cover_file,
                  bg=T["surface2"], fg=T["text"], relief="flat",
                  cursor="hand2", font=("Courier New", 9)).pack(fill="x", pady=2)

        tk.Label(left, text="Cover ID (openlibrary.org):",
                 bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8)).pack(anchor="w", pady=(10, 2))
        self.cover_id_var = tk.StringVar(value=self.book.get("cover_id", ""))
        e = tk.Entry(left, textvariable=self.cover_id_var,
                     bg=T["surface2"], fg=T["text"],
                     insertbackground=T["text"],
                     relief="flat", font=("Courier New", 10))
        e.pack(fill="x", ipady=4)

        # Fetch from API
        tk.Label(left, text="\nПодтянуть из интернета:", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8)).pack(anchor="w")
        self.api_var = tk.StringVar()
        tk.Entry(left, textvariable=self.api_var,
                 bg=T["surface2"], fg=T["text"],
                 insertbackground=T["text"],
                 relief="flat", font=("Courier New", 10)).pack(fill="x", ipady=4)
        self.fetch_btn = tk.Button(left, text="🌐  Загрузить данные",
                                   command=self._fetch_api,
                                   bg=T["accent"], fg="white", relief="flat",
                                   cursor="hand2", font=("Courier New", 9))
        self.fetch_btn.pack(fill="x", pady=(4, 0))
        self.fetch_status = tk.Label(left, text="", bg=T["surface"], fg=T["muted"],
                                     font=("Courier New", 8), wraplength=160, justify="left")
        self.fetch_status.pack(anchor="w", pady=(4, 0))

        # RIGHT — fields (scrollable)
        right_outer = tk.Frame(body, bg=T["surface"])
        right_outer.pack(side="left", fill="both", expand=True, padx=(0,16), pady=16)

        canvas = tk.Canvas(right_outer, bg=T["surface"], highlightthickness=0)
        sb = ttk.Scrollbar(right_outer, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        self.fields_frame = tk.Frame(canvas, bg=T["surface"])
        canvas_win = canvas.create_window((0,0), window=self.fields_frame, anchor="nw")

        def on_configure(e):
            canvas.configure(scrollregion=canvas.bbox("all"))
        def on_canvas_resize(e):
            canvas.itemconfig(canvas_win, width=e.width)
        self.fields_frame.bind("<Configure>", on_configure)
        canvas.bind("<Configure>", on_canvas_resize)
        canvas.bind_all("<MouseWheel>", lambda e: canvas.yview_scroll(-1*(e.delta//120), "units"))

        self.vars = {}
        self._build_fields()

        # Footer
        tk.Frame(self, bg=T["border"], height=1).pack(fill="x")
        footer = tk.Frame(self, bg=T["surface"], pady=12)
        footer.pack(fill="x", padx=20)
        tk.Button(footer, text="Отмена", command=self.destroy,
                  bg=T["surface2"], fg=T["muted"], relief="flat",
                  cursor="hand2", font=("Courier New", 10), padx=16, pady=6).pack(side="left")
        self.save_btn = tk.Button(footer, text="💾  Сохранить книгу",
                                  command=self._save,
                                  bg=T["accent"], fg="white", relief="flat",
                                  cursor="hand2", font=("Courier New", 10, "bold"),
                                  padx=20, pady=6)
        self.save_btn.pack(side="right")

    def _section(self, text):
        f = tk.Frame(self.fields_frame, bg=T["surface"])
        f.pack(fill="x", pady=(14, 4))
        tk.Label(f, text=text, bg=T["surface"], fg=T["accent"],
                 font=("Courier New", 9, "bold")).pack(side="left")
        tk.Frame(f, bg=T["border"], height=1).pack(side="left", fill="x", expand=True, padx=(8,0), pady=4)

    def _field(self, parent, label, key, row, col=0, width=28, required=False):
        frame = tk.Frame(parent, bg=T["surface"])
        frame.grid(row=row, column=col, sticky="ew", padx=(0,12), pady=4)
        lbl = label + (" *" if required else "")
        tk.Label(frame, text=lbl.upper(), bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8, "bold")).pack(anchor="w")
        var = tk.StringVar(value=str(self.book.get(key, "")))
        e = tk.Entry(frame, textvariable=var, width=width,
                     bg=T["surface2"], fg=T["text"], insertbackground=T["text"],
                     relief="flat", font=("Georgia", 11))
        e.pack(fill="x", ipady=5)
        self.vars[key] = var
        return var

    def _build_fields(self):
        f = self.fields_frame
        f.columnconfigure(0, weight=1)
        f.columnconfigure(1, weight=1)

        # ① Основное
        self._section("①  ОСНОВНОЕ")
        g1 = tk.Frame(f, bg=T["surface"]); g1.pack(fill="x"); g1.columnconfigure(0,weight=1); g1.columnconfigure(1,weight=1)
        self._field(g1, "Название",  "title",  0, 0, required=True)
        self._field(g1, "Автор",     "author", 0, 1, required=True)
        self._field(g1, "ISBN",      "isbn",   1, 0)

        # Genre + subgenre
        gf = tk.Frame(g1, bg=T["surface"]); gf.grid(row=1, column=1, sticky="ew", padx=(0,12), pady=4)
        tk.Label(gf, text="ЖАНР", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8, "bold")).pack(anchor="w")
        self.genre_var = tk.StringVar(value=self.book.get("genre", list(GENRES.keys())[0]))
        cb_genre = ttk.Combobox(gf, textvariable=self.genre_var,
                                values=list(GENRES.keys()), state="readonly",
                                font=("Georgia", 11))
        cb_genre.pack(fill="x", ipady=3)
        cb_genre.bind("<<ComboboxSelected>>", self._on_genre_change)
        self.vars["genre"] = self.genre_var

        sf = tk.Frame(f, bg=T["surface"]); sf.pack(fill="x"); sf.columnconfigure(0,weight=1)
        tk.Label(sf, text="ПОДЖАНР", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8, "bold")).pack(anchor="w", padx=(0,12))
        self.subgenre_var = tk.StringVar(value=self.book.get("subgenre", ""))
        self.cb_subgenre = ttk.Combobox(sf, textvariable=self.subgenre_var, state="readonly",
                                         font=("Georgia", 11))
        self.cb_subgenre.pack(fill="x", padx=(0,12), ipady=3)
        self.vars["subgenre"] = self.subgenre_var
        self._update_subgenres()

        # ② Детали издания
        self._section("②  ДЕТАЛИ ИЗДАНИЯ")
        g2 = tk.Frame(f, bg=T["surface"]); g2.pack(fill="x"); g2.columnconfigure(0,weight=1); g2.columnconfigure(1,weight=1)
        self._field(g2, "Издательство", "publisher", 0, 0)
        self._field(g2, "Год издания",  "year",      0, 1)
        self._field(g2, "Формат",       "format",    1, 0)
        self._field(g2, "Тираж (экз.)", "edition",   1, 1)

        # ③ Финансы и метрики
        self._section("③  ФИНАНСЫ И МЕТРИКИ")
        g3 = tk.Frame(f, bg=T["surface"]); g3.pack(fill="x"); g3.columnconfigure(0,weight=1); g3.columnconfigure(1,weight=1)
        self._field(g3, "Цена (₽)",     "price",  0, 0)
        self._field(g3, "Рейтинг (1–5)","rating", 0, 1)

        # Age rating
        af = tk.Frame(f, bg=T["surface"]); af.pack(fill="x", pady=(4,0))
        tk.Label(af, text="ВОЗРАСТНОЙ РЕЙТИНГ", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 8, "bold")).pack(anchor="w")
        abf = tk.Frame(af, bg=T["surface"]); abf.pack(anchor="w", pady=4)
        self.age_var = tk.StringVar(value=self.book.get("age", "0+"))
        for age, color in AGE_COLORS.items():
            rb = tk.Radiobutton(abf, text=age, variable=self.age_var, value=age,
                                bg=T["surface"], fg=color, selectcolor=T["surface"],
                                activebackground=T["surface"],
                                font=("Courier New", 10, "bold"),
                                cursor="hand2")
            rb.pack(side="left", padx=4)
        self.vars["age"] = self.age_var

        # ④ Даты
        self._section("④  ДАТЫ")
        g4 = tk.Frame(f, bg=T["surface"]); g4.pack(fill="x"); g4.columnconfigure(0,weight=1); g4.columnconfigure(1,weight=1)
        self._field(g4, "Дата подписи в печать", "sign_date",    0, 0)
        self._field(g4, "Дата доп. тиража",      "reprint_dates",0, 1)

        # ⑤ Библиотека
        self._section("⑤  БИБЛИОГРАФИЧЕСКАЯ ССЫЛКА")
        bf = tk.Frame(f, bg=T["surface"]); bf.pack(fill="x", pady=4)
        self.biblio_var = tk.StringVar(value=self.book.get("biblio", ""))
        te = tk.Text(bf, height=3, bg=T["surface2"], fg=T["text"],
                     insertbackground=T["text"], relief="flat",
                     font=("Georgia", 10), wrap="word")
        te.insert("1.0", self.book.get("biblio", ""))
        te.pack(fill="x", ipady=4)
        self._biblio_text = te

    def _on_genre_change(self, *_):
        self._update_subgenres()

    def _update_subgenres(self):
        subs = GENRES.get(self.genre_var.get(), [])
        self.cb_subgenre["values"] = subs
        if self.subgenre_var.get() not in subs:
            self.subgenre_var.set(subs[0] if subs else "")

    def _fill_fields(self):
        # cover preview
        cover_path = self.book.get("cover_file", "")
        if not cover_path:
            cid = self.book.get("cover_id", "")
            if cid:
                cover_path = str(COVERS_DIR / f"{self.book.get('id','0')}.jpg")
        self._show_cover(cover_path)

    def _draw_cover_placeholder(self):
        self.cover_canvas.delete("all")
        self.cover_canvas.create_rectangle(0,0,160,210, fill=T["surface2"], outline="")
        self.cover_canvas.create_text(80, 90, text="📖", font=("Arial", 36), fill=T["muted"])
        self.cover_canvas.create_text(80, 140, text="Нажмите для\nвыбора файла",
                                      font=("Courier New", 9), fill=T["muted"], justify="center")

    def _show_cover(self, path):
        if not path: return
        photo = load_cover_image(path, 160, 210)
        if photo:
            self._photo = photo
            self.cover_canvas.delete("all")
            self.cover_canvas.create_image(80, 105, image=photo)
            self.cover_file_path = path
        else:
            self._draw_cover_placeholder()

    def _pick_cover_file(self):
        path = filedialog.askopenfilename(
            title="Выбрать обложку",
            filetypes=[("Изображения", "*.jpg *.jpeg *.png *.webp"), ("Все файлы", "*")]
        )
        if path:
            self.cover_file_path = path
            self._show_cover(path)

    def _fetch_api(self):
        query = self.api_var.get().strip() or self.vars.get("title", tk.StringVar()).get()
        if not query:
            self.fetch_status.config(text="Введите название для поиска", fg=T["danger"])
            return
        self.fetch_btn.config(state="disabled", text="⏳ Загрузка...")
        self.fetch_status.config(text="Запрос к Open Library...", fg=T["muted"])

        def on_result(doc, err):
            self.after(0, lambda: self._on_api_result(doc, err))

        fetch_book_info(query, on_result)

    def _on_api_result(self, doc, err):
        self.fetch_btn.config(state="normal", text="🌐  Загрузить данные")
        if err or not doc:
            self.fetch_status.config(text=f"Не найдено: {err or 'нет результатов'}", fg=T["danger"])
            return

        # Заполнить поля из API
        if doc.get("title") and not self.vars["title"].get():
            self.vars["title"].set(doc["title"])
        if doc.get("author_name") and not self.vars["author"].get():
            self.vars["author"].set(doc["author_name"][0])
        if doc.get("first_publish_year") and not self.vars["year"].get():
            self.vars["year"].set(str(doc["first_publish_year"]))
        if doc.get("isbn"):
            self.vars["isbn"].set(doc["isbn"][0])
        if doc.get("publisher"):
            self.vars["publisher"].set(doc["publisher"][0] if isinstance(doc["publisher"], list) else doc["publisher"])
        if doc.get("number_of_pages_median"):
            pass  # нет поля pages в форме напрямую
        if doc.get("ratings_average"):
            self.vars["rating"].set(f"{doc['ratings_average']:.1f}")

        cover_id = str(doc.get("cover_i", ""))
        if cover_id:
            self.cover_id_var.set(cover_id)

        self.fetch_status.config(text="✓ Данные загружены!", fg=T["success"])

    def _save(self):
        title  = self.vars["title"].get().strip()
        author = self.vars["author"].get().strip()
        if not title or not author:
            messagebox.showwarning("Ошибка", "Заполните обязательные поля: Название и Автор", parent=self)
            return

        biblio = self._biblio_text.get("1.0", "end").strip()
        cover_file = getattr(self, "cover_file_path", self.book.get("cover_file", ""))

        result = {
            "title":         title,
            "author":        author,
            "isbn":          self.vars["isbn"].get().strip(),
            "genre":         self.vars["genre"].get(),
            "subgenre":      self.vars["subgenre"].get(),
            "publisher":     self.vars["publisher"].get().strip(),
            "year":          int(self.vars["year"].get() or 0),
            "format":        self.vars["format"].get().strip(),
            "edition":       int(self.vars["edition"].get() or 0),
            "price":         float(self.vars["price"].get() or 0),
            "rating":        float(self.vars["rating"].get() or 0),
            "age":           self.vars["age"].get(),
            "sign_date":     self.vars["sign_date"].get().strip(),
            "reprint_dates": [self.vars["reprint_dates"].get().strip()] if self.vars["reprint_dates"].get().strip() else [],
            "biblio":        biblio,
            "cover_id":      self.cover_id_var.get().strip(),
            "cover_file":    cover_file,
        }
        if self.editing:
            result["id"] = self.book["id"]
        if self.on_save:
            self.on_save(result)
        self.destroy()

# ─────────────────────────────────────────────────────────────────
# ДИАЛОГ: АЛГОРИТМЫ
# ─────────────────────────────────────────────────────────────────

class AlgoDialog(tk.Toplevel):
    def __init__(self, master, books):
        super().__init__(master)
        self.books = books
        self.title("Алгоритмы — LibraryDB")
        self.configure(bg=T["surface"])
        self.geometry("820x620")
        self.transient(master)
        self._build()

    def _build(self):
        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True, padx=12, pady=12)

        # ── Tab 1: MergeSort ──
        t1 = tk.Frame(nb, bg=T["surface"]); nb.add(t1, text="  📊 MergeSort  ")
        tk.Label(t1, text="Список книг, отсортированных по алфавиту (MergeSort)",
                 bg=T["surface"], fg=T["muted"], font=("Courier New", 9)).pack(anchor="w", padx=12, pady=(12,4))
        lb_frame = tk.Frame(t1, bg=T["surface"]); lb_frame.pack(fill="both", expand=True, padx=12, pady=4)
        sb = tk.Scrollbar(lb_frame); sb.pack(side="right", fill="y")
        lb = tk.Listbox(lb_frame, bg=T["surface2"], fg=T["text"], selectbackground=T["accent"],
                        font=("Courier New", 11), relief="flat", yscrollcommand=sb.set,
                        borderwidth=0, highlightthickness=0)
        lb.pack(fill="both", expand=True)
        sb.config(command=lb.yview)
        sorted_books = merge_sort(self.books, "title")
        for i, b in enumerate(sorted_books, 1):
            lb.insert("end", f"  {i:>3}.  {b['title']:<40}  {b['author']}")

        # ── Tab 2: Бинарный поиск ──
        t2 = tk.Frame(nb, bg=T["surface"]); nb.add(t2, text="  ⚡ Бинарный поиск  ")
        self._sorted_books = merge_sort(self.books, "title")

        top2 = tk.Frame(t2, bg=T["surface"]); top2.pack(fill="x", padx=12, pady=12)
        tk.Label(top2, text="Поиск по точному названию или подстроке:",
                 bg=T["surface"], fg=T["muted"], font=("Courier New", 9)).pack(side="left")
        self.bs_var = tk.StringVar()
        tk.Entry(top2, textvariable=self.bs_var, bg=T["surface2"], fg=T["text"],
                 insertbackground=T["text"], relief="flat",
                 font=("Georgia", 12), width=30).pack(side="left", padx=8, ipady=4)
        tk.Button(top2, text="Найти", command=self._run_bs,
                  bg=T["accent"], fg="white", relief="flat",
                  cursor="hand2", font=("Courier New", 10)).pack(side="left")

        self.bs_result = tk.Text(t2, bg=T["surface2"], fg=T["text"], relief="flat",
                                 font=("Courier New", 11), height=20, wrap="word",
                                 insertbackground=T["text"])
        self.bs_result.pack(fill="both", expand=True, padx=12, pady=(0,12))
        tk.Label(t2, text="Массив отсортирован MergeSort. Бинарный поиск: O(log n)",
                 bg=T["surface"], fg=T["muted"], font=("Courier New", 8)).pack(anchor="w", padx=12, pady=4)

        # ── Tab 3: BST ──
        t3 = tk.Frame(nb, bg=T["surface"]); nb.add(t3, text="  🌳 Дерево поиска  ")
        self.bst_root = build_bst(self.books)

        top3 = tk.Frame(t3, bg=T["surface"]); top3.pack(fill="x", padx=12, pady=12)
        tk.Label(top3, text="Поиск в оптимальном BST (вес = рейтинг):",
                 bg=T["surface"], fg=T["muted"], font=("Courier New", 9)).pack(side="left")
        self.bst_var = tk.StringVar()
        tk.Entry(top3, textvariable=self.bst_var, bg=T["surface2"], fg=T["text"],
                 insertbackground=T["text"], relief="flat",
                 font=("Georgia", 12), width=30).pack(side="left", padx=8, ipady=4)
        tk.Button(top3, text="Найти", command=self._run_bst,
                  bg=T["accent2"], fg="white", relief="flat",
                  cursor="hand2", font=("Courier New", 10)).pack(side="left")

        self.bst_result = tk.Text(t3, bg=T["surface2"], fg=T["text"], relief="flat",
                                  font=("Courier New", 11), height=20, wrap="word",
                                  insertbackground=T["text"])
        self.bst_result.pack(fill="both", expand=True, padx=12, pady=(0,12))
        tk.Label(t3, text="BST построен по алфавиту. Средняя глубина ≈ log₂(n)",
                 bg=T["surface"], fg=T["muted"], font=("Courier New", 8)).pack(anchor="w", padx=12, pady=4)

    def _run_bs(self):
        q = self.bs_var.get().strip()
        if not q: return
        idx, steps = binary_search(self._sorted_books, q)
        self.bs_result.delete("1.0", "end")
        self.bs_result.insert("end", f"Запрос: «{q}»\n")
        self.bs_result.insert("end", f"Шаги алгоритма ({len(steps)}):\n\n")
        for i, s in enumerate(steps):
            b = self._sorted_books[s]
            marker = " ◀ проверяем" + (" ✓ НАЙДЕНО" if i == len(steps)-1 and idx == s else "")
            self.bs_result.insert("end", f"  шаг {i+1:>2}: [{s:>3}] {b['title']}{marker}\n")
        self.bs_result.insert("end", "\n")
        if idx >= 0:
            b = self._sorted_books[idx]
            self.bs_result.insert("end", f"✓ НАЙДЕНО: {b['title']} — {b['author']} ({b['year']})\n")
        else:
            self.bs_result.insert("end", "✗ Не найдено\n")

    def _run_bst(self):
        q = self.bst_var.get().strip()
        if not q: return
        found, path = bst_search(self.bst_root, q)
        self.bst_result.delete("1.0", "end")
        self.bst_result.insert("end", f"Запрос: «{q}»\n")
        self.bst_result.insert("end", f"Путь по дереву ({len(path)} узлов):\n\n")
        for i, title in enumerate(path):
            arrow = "  └─ " if i > 0 else "  ├─ "
            self.bst_result.insert("end", f"{arrow}[{i}] {title}\n")
        self.bst_result.insert("end", "\n")
        if found:
            self.bst_result.insert("end", f"✓ НАЙДЕНО: {found['title']} — {found['author']}\n")
            self.bst_result.insert("end", f"   Рейтинг: {found['rating']} | Жанр: {found['genre']}\n")
        else:
            self.bst_result.insert("end", "✗ Не найдено в дереве\n")

# ─────────────────────────────────────────────────────────────────
# ГЛАВНОЕ ОКНО
# ─────────────────────────────────────────────────────────────────

class LibraryApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LibraryDB")
        self.geometry("1200x760")
        self.minsize(900, 600)
        self.configure(bg=T["bg"])

        self._data        = load_data()
        self._all_books   = self._data["books"]
        self._photos      = {}        # id → PhotoImage (keep refs)
        self._sel_genre   = None
        self._sel_sub     = None
        self._sort_key    = "title"
        self._sort_rev    = False
        self._search_q    = ""
        self._dark        = True

        self._configure_styles()
        self._build_ui()
        self._refresh_sidebar()
        self._refresh_cards()
        # Подключаем поиск только после полного построения UI
        self._search_var.trace_add("write", self._on_search)

    def _reload_books(self):
        save_data(self._data)
        self._all_books = self._data["books"]

    def _configure_styles(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TCombobox",
                        fieldbackground=T["surface2"],
                        background=T["surface2"],
                        foreground=T["text"],
                        selectbackground=T["accent"],
                        bordercolor=T["border"])
        style.configure("TScrollbar", background=T["surface2"], troughcolor=T["bg"])
        style.configure("TNotebook", background=T["surface"])
        style.configure("TNotebook.Tab",
                        background=T["surface2"], foreground=T["muted"],
                        padding=[12, 6])
        style.map("TNotebook.Tab",
                  background=[("selected", T["accent"])],
                  foreground=[("selected", "white")])

    # ── BUILD UI ────────────────────────────────────────────────

    def _build_ui(self):
        # Header
        self._build_header()
        # Body = sidebar + main
        body = tk.Frame(self, bg=T["bg"])
        body.pack(fill="both", expand=True)
        self._build_sidebar(body)
        self._build_main(body)

    def _build_header(self):
        hdr = tk.Frame(self, bg=T["header"], height=60)
        hdr.pack(fill="x")
        hdr.pack_propagate(False)

        tk.Label(hdr, text="📚  LibraryDB", bg=T["header"], fg=T["accent"],
                 font=("Georgia", 16, "bold")).pack(side="left", padx=20)

        # Search
        sf = tk.Frame(hdr, bg=T["header"]); sf.pack(side="left", padx=20)
        tk.Label(sf, text="🔍", bg=T["header"], fg=T["muted"], font=("Arial", 13)).pack(side="left")
        self._search_var = tk.StringVar()
        # trace добавим после построения UI, чтобы не срабатывало раньше времени
        self._search_entry = tk.Entry(sf, textvariable=self._search_var, width=36,
                 bg=T["surface"], fg=T["text"], insertbackground=T["text"],
                 relief="flat", font=("Georgia", 12))
        self._search_entry.pack(side="left", ipady=5, padx=6)
        # Placeholder вручную
        self._search_placeholder = "Поиск по названию или автору..."
        self._search_entry.insert(0, self._search_placeholder)
        self._search_entry.config(fg=T["muted"])
        self._search_entry.bind("<FocusIn>",  self._search_focus_in)
        self._search_entry.bind("<FocusOut>", self._search_focus_out)

        # Buttons right side
        right = tk.Frame(hdr, bg=T["header"]); right.pack(side="right", padx=16)

        tk.Button(right, text="🌙" if self._dark else "☀", command=self._toggle_theme,
                  bg=T["header"], fg=T["muted"], relief="flat",
                  font=("Arial", 15), cursor="hand2").pack(side="left", padx=6)

        tk.Button(right, text="🔬  Алгоритмы", command=self._open_algo,
                  bg=T["surface2"], fg=T["text"], relief="flat",
                  cursor="hand2", font=("Courier New", 10), padx=10, pady=6).pack(side="left", padx=4)

        tk.Button(right, text="＋  Добавить книгу", command=self._add_book,
                  bg=T["accent"], fg="white", relief="flat",
                  cursor="hand2", font=("Courier New", 10, "bold"),
                  padx=14, pady=6).pack(side="left", padx=4)

        tk.Frame(self, bg=T["border"], height=1).pack(fill="x")

    def _build_sidebar(self, parent):
        self._sidebar = tk.Frame(parent, bg=T["sidebar"], width=220)
        self._sidebar.pack(side="left", fill="y")
        self._sidebar.pack_propagate(False)

        tk.Frame(self._sidebar, bg=T["border"], width=1).pack(side="right", fill="y")

        # Scrollable
        canvas = tk.Canvas(self._sidebar, bg=T["sidebar"], highlightthickness=0, width=219)
        sb = ttk.Scrollbar(self._sidebar, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=sb.set)
        canvas.pack(side="left", fill="both", expand=True)
        # sb hidden (narrow sidebar)

        self._sidebar_frame = tk.Frame(canvas, bg=T["sidebar"])
        self._sidebar_win = canvas.create_window((0,0), window=self._sidebar_frame, anchor="nw", width=219)
        self._sidebar_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

    def _build_main(self, parent):
        main = tk.Frame(parent, bg=T["bg"])
        main.pack(side="left", fill="both", expand=True)

        # Sort bar
        sortbar = tk.Frame(main, bg=T["surface"], height=44)
        sortbar.pack(fill="x", padx=0)
        sortbar.pack_propagate(False)
        tk.Label(sortbar, text="Сортировать по:", bg=T["surface"], fg=T["muted"],
                 font=("Courier New", 9)).pack(side="left", padx=16)

        self._sort_var = tk.StringVar(value="Название")
        sort_labels = [l for _, l in SORT_FIELDS]
        cb = ttk.Combobox(sortbar, textvariable=self._sort_var, values=sort_labels,
                          state="readonly", width=16, font=("Courier New", 10))
        cb.pack(side="left", padx=4, pady=8)
        cb.bind("<<ComboboxSelected>>", self._on_sort_change)

        self._sort_dir_btn = tk.Button(sortbar, text="↑  ASC", command=self._toggle_sort_dir,
                                       bg=T["surface2"], fg=T["accent"], relief="flat",
                                       cursor="hand2", font=("Courier New", 9), padx=8)
        self._sort_dir_btn.pack(side="left", padx=4)

        self._count_label = tk.Label(sortbar, text="", bg=T["surface"], fg=T["muted"],
                                     font=("Courier New", 9))
        self._count_label.pack(side="right", padx=16)

        tk.Frame(main, bg=T["border"], height=1).pack(fill="x")

        # Cards area (scrollable canvas)
        cards_outer = tk.Frame(main, bg=T["bg"])
        cards_outer.pack(fill="both", expand=True)

        self._cards_canvas = tk.Canvas(cards_outer, bg=T["bg"], highlightthickness=0)
        vsb = ttk.Scrollbar(cards_outer, orient="vertical", command=self._cards_canvas.yview)
        self._cards_canvas.configure(yscrollcommand=vsb.set)
        vsb.pack(side="right", fill="y")
        self._cards_canvas.pack(side="left", fill="both", expand=True)

        self._cards_frame = tk.Frame(self._cards_canvas, bg=T["bg"])
        self._cards_win = self._cards_canvas.create_window((0,0), window=self._cards_frame, anchor="nw")
        self._cards_frame.bind("<Configure>", self._on_cards_configure)
        self._cards_canvas.bind("<Configure>", self._on_canvas_configure)
        self._cards_canvas.bind_all("<MouseWheel>", self._on_mousewheel)

    def _on_cards_configure(self, e):
        self._cards_canvas.configure(scrollregion=self._cards_canvas.bbox("all"))

    def _on_canvas_configure(self, e):
        self._cards_canvas.itemconfig(self._cards_win, width=e.width)

    def _on_mousewheel(self, e):
        self._cards_canvas.yview_scroll(-1 * (e.delta // 120), "units")

    # ── SIDEBAR REFRESH ─────────────────────────────────────────

    def _refresh_sidebar(self):
        for w in self._sidebar_frame.winfo_children():
            w.destroy()

        def btn(text, cmd, active=False, indent=0):
            bg = T["accent"] if active else T["sidebar"]
            fg = "white" if active else T["text"]
            b = tk.Button(self._sidebar_frame, text=(" " * indent) + text,
                          command=cmd, bg=bg, fg=fg, relief="flat",
                          anchor="w", cursor="hand2",
                          font=("Georgia", 11 if not indent else 10),
                          padx=16 + indent * 4, pady=7)
            b.pack(fill="x")
            b.bind("<Enter>", lambda e, b=b, a=active: b.config(bg=T["accent"] if a else T["surface2"]))
            b.bind("<Leave>", lambda e, b=b, a=active: b.config(bg=T["accent"] if a else T["sidebar"]))

        # All
        count_all = len(self._all_books)
        btn(f"Все книги  ({count_all})", lambda: self._select_genre(None), self._sel_genre is None)

        # Genres section
        tk.Label(self._sidebar_frame, text="ЖАНРЫ", bg=T["sidebar"], fg=T["muted"],
                 font=("Courier New", 8, "bold")).pack(anchor="w", padx=16, pady=(12, 4))

        for genre in GENRES:
            cnt = sum(1 for b in self._all_books if b["genre"] == genre)
            is_sel = self._sel_genre == genre
            btn(f"{genre}  ({cnt})", lambda g=genre: self._select_genre(g), is_sel)

            if is_sel:
                # Subgenres
                tk.Label(self._sidebar_frame, text="ПОДЖАНРЫ", bg=T["sidebar"], fg=T["muted"],
                         font=("Courier New", 7, "bold")).pack(anchor="w", padx=32, pady=(6,2))
                subs = GENRES.get(genre, [])
                if not subs:
                    tk.Label(self._sidebar_frame, text="  Нет поджанров",
                             bg=T["sidebar"], fg=T["muted"],
                             font=("Georgia", 10, "italic")).pack(anchor="w", padx=32)
                else:
                    for sub in subs:
                        s_cnt = sum(1 for b in self._all_books if b["genre"] == genre and b["subgenre"] == sub)
                        is_s = self._sel_sub == sub
                        btn(f"{sub}  ({s_cnt})", lambda s=sub: self._select_sub(s), is_s, indent=4)

    # ── CARDS REFRESH ───────────────────────────────────────────

    def _get_visible_books(self):
        books = backend_sort_books(self._sort_key, not self._sort_rev)
        if self._sel_genre:
            books = [b for b in books if b["genre"] == self._sel_genre]
        if self._sel_sub:
            books = [b for b in books if b["subgenre"] == self._sel_sub]
        if self._search_q:
            q = self._search_q.lower()
            by_title  = [b for b in books if q in b["title"].lower()]
            by_author = [b for b in books if q in b["author"].lower() and b not in by_title]
            books = by_title + by_author
        return books

    def _refresh_cards(self):
        for w in self._cards_frame.winfo_children():
            w.destroy()

        books = self._get_visible_books()
        n = len(books)
        self._count_label.config(text=f"{n} {'книга' if n==1 else 'книги' if 2<=n<=4 else 'книг'}")

        if not books:
            tk.Label(self._cards_frame, text="📭  Книги не найдены",
                     bg=T["bg"], fg=T["muted"], font=("Georgia", 16)).pack(pady=80)
            return

        # Responsive grid
        cols = max(1, self._cards_canvas.winfo_width() // (CARD_W + 20))

        for i, book in enumerate(books):
            row, col = divmod(i, cols)
            card = self._make_card(book)
            card.grid(row=row, column=col, padx=12, pady=12, sticky="n")

        for c in range(cols):
            self._cards_frame.columnconfigure(c, weight=1)

    def _make_card(self, book):
        card = tk.Frame(self._cards_frame, bg=T["card"], width=CARD_W,
                        relief="flat", cursor="hand2")
        card.pack_propagate(False)
        card.configure(height=CARD_H)

        # hover
        def on_enter(e):
            card.config(bg=T["card_hover"])
            for w in card.winfo_children():
                try: w.config(bg=T["card_hover"])
                except: pass
            del_btn.place(x=CARD_W-30, y=4)
        def on_leave(e):
            card.config(bg=T["card"])
            for w in card.winfo_children():
                try: w.config(bg=T["card"])
                except: pass
            del_btn.place_forget()

        card.bind("<Enter>", on_enter)
        card.bind("<Leave>", on_leave)
        card.bind("<Button-1>", lambda e, b=book: self._edit_book(b))

        # Cover image
        cover_frame = tk.Frame(card, bg=T["surface2"], width=CARD_W, height=COVER_H)
        cover_frame.pack_propagate(False)
        cover_frame.pack(fill="x")

        cover_path = book.get("cover_file", "")
        if not cover_path and book.get("cover_id"):
            cover_path = str(COVERS_DIR / f"{book['id']}.jpg")

        photo = None
        if cover_path and os.path.exists(cover_path):
            photo = load_cover_image(cover_path)
        elif book.get("cover_id") or book.get("cover_url"):
            # Попробовать скачать
            self._try_download_cover(book)

        if photo:
            self._photos[book["id"]] = photo
            lbl_img = tk.Label(cover_frame, image=photo, bg=T["surface2"])
            lbl_img.pack()
            lbl_img.bind("<Enter>", on_enter)
            lbl_img.bind("<Leave>", on_leave)
            lbl_img.bind("<Button-1>", lambda e, b=book: self._edit_book(b))
        else:
            tk.Label(cover_frame, text="📖", bg=T["surface2"],
                     font=("Arial", 32), fg=T["muted"]).pack(expand=True, fill="both")

        # Age badge
        age = book.get("age", "0+")
        age_color = AGE_COLORS.get(age, T["muted"])
        tk.Label(cover_frame, text=age,
                 bg=age_color, fg="white",
                 font=("Courier New", 8, "bold"),
                 padx=4, pady=1).place(x=4, y=4)

        # Info
        info = tk.Frame(card, bg=T["card"])
        info.pack(fill="x", padx=8, pady=(6,4))

        # Format + year
        meta = f"{book.get('format','')}  ·  {book.get('year','')}"
        tk.Label(info, text=meta.strip(" · "),
                 bg=T["card"], fg=T["muted"],
                 font=("Courier New", 8)).pack(anchor="w")

        # Title
        tk.Label(info, text=book["title"],
                 bg=T["card"], fg=T["text"],
                 font=("Georgia", 11, "bold"),
                 wraplength=CARD_W-16, justify="left", anchor="w").pack(anchor="w", pady=(2,0))

        # Rating stars
        tk.Label(info, text=stars_text(book.get("rating", 0)),
                 bg=T["card"], fg="#f59e0b",
                 font=("Arial", 10)).pack(anchor="w")

        # Price
        if book.get("price"):
            tk.Label(info, text=f"{int(book['price'])} ₽",
                     bg=T["card"], fg=T["accent"],
                     font=("Courier New", 11, "bold")).pack(anchor="w", pady=(2,0))

        # Delete button (hidden until hover)
        del_btn = tk.Button(card, text="🗑", bg=T["danger"], fg="white",
                            relief="flat", cursor="hand2",
                            font=("Arial", 11),
                            command=lambda b=book: self._delete_book(b["id"]))
        del_btn.place_forget()

        return card

    def _try_download_cover(self, book):
        cid = book.get("cover_id")
        bid = book.get("id")
        dest = COVERS_DIR / f"{bid}.jpg"
        if dest.exists() or not cid:
            return

        def on_done(path, err):
            if path:
                updated = dict(book)
                updated["cover_file"] = path
                saved = backend_upsert_book(updated, fetch_network=False)
                if saved:
                    self.after(100, lambda: (self._reload_books(), self._refresh_sidebar(), self._refresh_cards()))

        download_cover(cid, bid, on_done)

    # ── ACTIONS ─────────────────────────────────────────────────

    def _select_genre(self, genre):
        self._sel_genre = genre
        self._sel_sub   = None
        self._refresh_sidebar()
        self._refresh_cards()

    def _select_sub(self, sub):
        self._sel_sub = None if self._sel_sub == sub else sub
        self._refresh_sidebar()
        self._refresh_cards()

    def _search_focus_in(self, *_):
        if self._search_entry.get() == self._search_placeholder:
            self._search_entry.delete(0, "end")
            self._search_entry.config(fg=T["text"])

    def _search_focus_out(self, *_):
        if not self._search_entry.get():
            self._search_entry.insert(0, self._search_placeholder)
            self._search_entry.config(fg=T["muted"])

    def _on_search(self, *_):
        val = self._search_var.get().strip()
        self._search_q = "" if val == self._search_placeholder else val
        self._refresh_cards()

    def _on_sort_change(self, *_):
        label = self._sort_var.get()
        for k, l in SORT_FIELDS:
            if l == label:
                self._sort_key = k; break
        self._refresh_cards()

    def _toggle_sort_dir(self):
        self._sort_rev = not self._sort_rev
        self._sort_dir_btn.config(text="↓  DESC" if self._sort_rev else "↑  ASC")
        self._refresh_cards()

    def _toggle_theme(self):
        global T
        self._dark = not self._dark
        T = DARK if self._dark else LIGHT
        messagebox.showinfo("Тема", "Перезапустите приложение для смены темы.\n(Полная поддержка горячей смены — в следующей версии)")

    def _add_book(self):
        BookDialog(self, on_save=self._on_book_saved)

    def _edit_book(self, book):
        BookDialog(self, book=book, on_save=self._on_book_saved)

    def _on_book_saved(self, result):
        saved = backend_upsert_book(result, fetch_network=True)
        if not saved:
            messagebox.showerror("Ошибка", "Не удалось сохранить книгу через C++ backend.", parent=self)
            return
        if saved.get("cover_id"):
            self._try_download_cover(saved)
        self._reload_books()
        self._refresh_sidebar()
        self._refresh_cards()

    def _delete_book(self, book_id):
        if messagebox.askyesno("Удалить", "Удалить книгу из базы данных?", parent=self):
            backend_remove_book(book_id)
            # Remove cover file
            cover = COVERS_DIR / f"{book_id}.jpg"
            if cover.exists():
                cover.unlink()
            self._reload_books()
            self._refresh_sidebar()
            self._refresh_cards()

    def _open_algo(self):
        AlgoDialog(self, self._all_books)


# ─────────────────────────────────────────────────────────────────
# ЗАПУСК
# ─────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = LibraryApp()
    app.mainloop()
