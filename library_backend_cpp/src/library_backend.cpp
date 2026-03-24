#include "library_backend.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include <sqlite3.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace {
struct StatementDeleter {
    void operator()(sqlite3_stmt* statement) const {
        sqlite3_finalize(statement);
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string normalizeFieldName(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    return parts;
}

std::string join(const std::vector<std::string>& values, char delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << delimiter;
        }
        out << values[i];
    }
    return out.str();
}

std::string escapeText(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '=':
            escaped += "\\=";
            break;
        default:
            escaped.push_back(c);
            break;
        }
    }
    return escaped;
}

std::string unescapeText(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    bool escaped = false;
    for (char c : value) {
        if (!escaped) {
            if (c == '\\') {
                escaped = true;
            } else {
                result.push_back(c);
            }
            continue;
        }

        switch (c) {
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        default:
            result.push_back(c);
            break;
        }
        escaped = false;
    }
    if (escaped) {
        result.push_back('\\');
    }
    return result;
}

std::string readCommandOutput(const std::string& command) {
    std::array<char, 512> buffer{};
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return {};
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    const int code = pclose(pipe);
    if (code != 0) {
        return {};
    }
    return result;
}

std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::filesystem::path logPathFromDb(const std::string& dbPath) {
    const auto p = std::filesystem::path(dbPath);
    if (p.has_parent_path()) {
        return p.parent_path() / "library.log";
    }
    return std::filesystem::path("library.log");
}

void appendLog(const std::string& level, const std::string& message, const std::string& dbPath) {
    const auto path = logPathFromDb(dbPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::app);
    if (!out.good()) {
        return;
    }
    out << currentTimestamp() << " [" << level << "] " << message << "\n";
}

std::optional<std::chrono::system_clock::time_point> parseLogTimestamp(const std::string& line) {
    if (line.size() < 19) {
        return std::nullopt;
    }
    std::tm tm{};
    std::istringstream in(line.substr(0, 19));
    in >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (in.fail()) {
        return std::nullopt;
    }
    const std::time_t t = std::mktime(&tm);
    if (t < 0) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(t);
}

void pruneOldLogs(const std::string& dbPath) {
    const auto path = logPathFromDb(dbPath);
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream in(path);
    if (!in.good()) {
        return;
    }
    const auto threshold = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(in, line)) {
        const auto ts = parseLogTimestamp(line);
        if (!ts.has_value() || ts.value() >= threshold) {
            kept.push_back(line);
        }
    }
    in.close();
    std::ofstream out(path, std::ios::trunc);
    for (const auto& item : kept) {
        out << item << "\n";
    }
}

void ensureWeeklyBackup(const std::string& dbPath) {
    const auto db = std::filesystem::path(dbPath);
    if (!std::filesystem::exists(db)) {
        return;
    }
    const auto backup = db.parent_path() / "library.db.backup";
    if (!std::filesystem::exists(backup)) {
        std::filesystem::copy_file(db, backup, std::filesystem::copy_options::overwrite_existing);
        return;
    }
    const auto dbWrite = std::filesystem::last_write_time(db);
    const auto backupWrite = std::filesystem::last_write_time(backup);
    if (dbWrite > backupWrite + std::chrono::hours(24 * 7)) {
        std::filesystem::copy_file(db, backup, std::filesystem::copy_options::overwrite_existing);
    }
}

bool envFlagEnabled(const char* name, bool defaultValue) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return defaultValue;
    }
    const std::string normalized = normalizeFieldName(value);
    if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no") {
        return false;
    }
    if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes") {
        return true;
    }
    return defaultValue;
}

bool isNetworkEnabled() {
    const bool offlineMode = envFlagEnabled("OFFLINE_MODE", false);
    const bool networkSwitch = envFlagEnabled("LIBRARY_ENABLE_NET", true);
    return !offlineMode && networkSwitch;
}

std::string shellEscape(const std::string& value) {
    std::string escaped = "'";
    for (char c : value) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(c);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

std::string jsonStringField(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const auto pos = json.find(key);
    if (pos == std::string::npos) {
        return {};
    }
    const auto colon = json.find(':', pos + key.size());
    if (colon == std::string::npos) {
        return {};
    }
    const auto firstQuote = json.find('"', colon + 1);
    if (firstQuote == std::string::npos) {
        return {};
    }
    std::string value;
    bool escaped = false;
    for (std::size_t i = firstQuote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            value.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            return value;
        }
        value.push_back(c);
    }
    return {};
}

int jsonIntegerField(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const auto pos = json.find(key);
    if (pos == std::string::npos) {
        return 0;
    }
    const auto colon = json.find(':', pos + key.size());
    if (colon == std::string::npos) {
        return 0;
    }
    std::size_t idx = colon + 1;
    while (idx < json.size() && std::isspace(static_cast<unsigned char>(json[idx]))) {
        ++idx;
    }
    std::size_t end = idx;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) {
        ++end;
    }
    return std::atoi(json.substr(idx, end - idx).c_str());
}

std::string sqlSortColumn(SortField field) {
    switch (field) {
    case SortField::Title:
        return "lower(title)";
    case SortField::Author:
        return "lower(author)";
    case SortField::Genre:
        return "lower(genre_name)";
    case SortField::Subgenre:
        return "lower(subgenre_name)";
    case SortField::Publisher:
        return "lower(publisher)";
    case SortField::Year:
        return "year";
    case SortField::Format:
        return "lower(format)";
    case SortField::Rating:
        return "rating";
    case SortField::Price:
        return "price";
    case SortField::AgeRating:
        return "lower(age_rating)";
    case SortField::Isbn:
        return "lower(isbn)";
    case SortField::TotalPrintRun:
        return "total_print_run";
    case SortField::SignedToPrintDate:
        return "signed_to_print_date";
    }
    return "id";
}

std::string baseSelectSql() {
    return R"SQL(
        SELECT
            books.id,
            books.title,
            books.author,
            genres.name AS genre_name,
            subgenres.name AS subgenre_name,
            books.publisher,
            books.year,
            books.format,
            books.rating,
            books.price,
            books.age_rating,
            books.isbn,
            books.total_print_run,
            books.signed_to_print_date,
            books.additional_print_dates,
            books.cover_image_path,
            books.license_image_path,
            books.bibliographic_reference,
            books.cover_url,
            books.search_frequency
        FROM books
        LEFT JOIN genres ON genres.id = books.genre_id
        LEFT JOIN subgenres ON subgenres.id = books.subgenre_id
    )SQL";
}

bool bindText(sqlite3_stmt* statement, int index, const std::string& value) {
    return sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}
} // namespace

LibraryStorage::LibraryStorage(std::string filePath)
    : filePath_(std::move(filePath)) {}

LibraryStorage::~LibraryStorage() {
    if (db_ != nullptr) {
        sqlite3_close(static_cast<sqlite3*>(db_));
    }
}

bool LibraryStorage::open() {
    if (db_ != nullptr) {
        return true;
    }

    const auto path = std::filesystem::path(filePath_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    pruneOldLogs(filePath_);
    ensureWeeklyBackup(filePath_);

    sqlite3* db = nullptr;
    if (sqlite3_open(filePath_.c_str(), &db) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        appendLog("ERROR", "DB open failed: " + filePath_, filePath_);
        return false;
    }

    db_ = db;
    if (!execute("PRAGMA foreign_keys = ON;")) {
        appendLog("ERROR", "Failed to enable foreign_keys pragma", filePath_);
        return false;
    }

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), "PRAGMA integrity_check;", -1, &raw, nullptr) == SQLITE_OK) {
        StatementPtr stmt(raw);
        bool integrityOk = false;
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
            integrityOk = txt != nullptr && std::string(txt) == "ok";
        }
        if (!integrityOk) {
            appendLog("ERROR", "DB integrity check failed; rotating broken DB to .bak", filePath_);
            sqlite3_close(static_cast<sqlite3*>(db_));
            db_ = nullptr;
            const auto broken = std::filesystem::path(filePath_);
            const auto backup = broken.parent_path() / "library.db.bak";
            std::error_code ec;
            std::filesystem::rename(broken, backup, ec);
            if (ec) {
                appendLog("ERROR", "Failed to rename broken DB: " + ec.message(), filePath_);
                return false;
            }
            sqlite3* recreated = nullptr;
            if (sqlite3_open(filePath_.c_str(), &recreated) != SQLITE_OK) {
                if (recreated != nullptr) {
                    sqlite3_close(recreated);
                }
                appendLog("ERROR", "Failed to recreate DB after corruption recovery", filePath_);
                return false;
            }
            db_ = recreated;
            if (!execute("PRAGMA foreign_keys = ON;")) {
                return false;
            }
        }
    }

    if (!ensureSchema()) {
        appendLog("ERROR", "ensureSchema failed", filePath_);
        return false;
    }
    appendLog("INFO", "Storage opened successfully", filePath_);
    return true;
}

bool LibraryStorage::ensureSchema() {
    if (!execute(R"SQL(
        CREATE TABLE IF NOT EXISTS genres (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );

        CREATE TABLE IF NOT EXISTS subgenres (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            genre_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            UNIQUE(genre_id, name),
            FOREIGN KEY (genre_id) REFERENCES genres(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS books (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            genre_id INTEGER,
            subgenre_id INTEGER,
            title TEXT NOT NULL,
            author TEXT NOT NULL,
            publisher TEXT,
            year INTEGER NOT NULL DEFAULT 0,
            format TEXT,
            rating REAL NOT NULL DEFAULT 0,
            price REAL NOT NULL DEFAULT 0,
            age_rating TEXT CHECK(age_rating IN ('0+','6+','12+','16+','18+') OR age_rating = ''),
            isbn TEXT,
            total_print_run INTEGER NOT NULL DEFAULT 0,
            signed_to_print_date TEXT,
            additional_print_dates TEXT,
            cover_image_path TEXT,
            license_image_path TEXT,
            bibliographic_reference TEXT,
            cover_url TEXT,
            search_frequency REAL NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (genre_id) REFERENCES genres(id) ON DELETE SET NULL,
            FOREIGN KEY (subgenre_id) REFERENCES subgenres(id) ON DELETE SET NULL
        );

        CREATE INDEX IF NOT EXISTS idx_books_title ON books(title);
        CREATE INDEX IF NOT EXISTS idx_books_author ON books(author);
        CREATE UNIQUE INDEX IF NOT EXISTS idx_books_isbn_unique ON books(isbn) WHERE isbn IS NOT NULL AND isbn <> '';
        CREATE INDEX IF NOT EXISTS idx_subgenres_genre_id ON subgenres(genre_id);
    )SQL")) {
        return false;
    }

    execute("ALTER TABLE books ADD COLUMN created_at TEXT NOT NULL DEFAULT (datetime('now'));");
    return true;
}

bool LibraryStorage::execute(const std::string& sql) const {
    char* errorMessage = nullptr;
    const int code = sqlite3_exec(static_cast<sqlite3*>(db_), sql.c_str(), nullptr, nullptr, &errorMessage);
    if (code != SQLITE_OK) {
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}

bool LibraryStorage::ensureGenreHierarchy(const Book& book) const {
    sqlite3* db = static_cast<sqlite3*>(db_);

    if (!book.genre.empty()) {
        const char* genreSql = "INSERT OR IGNORE INTO genres(name) VALUES (?);";
        StatementPtr stmt(nullptr);
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db, genreSql, -1, &raw, nullptr) != SQLITE_OK) {
            return false;
        }
        stmt.reset(raw);
        if (!bindText(stmt.get(), 1, book.genre) || sqlite3_step(stmt.get()) == SQLITE_ERROR) {
            return false;
        }
    }

    if (!book.genre.empty() && !book.subgenre.empty()) {
        const char* subSql = R"SQL(
            INSERT OR IGNORE INTO subgenres(genre_id, name)
            VALUES ((SELECT id FROM genres WHERE name = ?), ?);
        )SQL";
        StatementPtr stmt(nullptr);
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db, subSql, -1, &raw, nullptr) != SQLITE_OK) {
            return false;
        }
        stmt.reset(raw);
        if (!bindText(stmt.get(), 1, book.genre) || !bindText(stmt.get(), 2, book.subgenre) || sqlite3_step(stmt.get()) == SQLITE_ERROR) {
            return false;
        }
    }

    return true;
}

Book LibraryStorage::readBookFromStatement(void* statementPtr) {
    auto* statement = static_cast<sqlite3_stmt*>(statementPtr);
    Book book;
    book.id = sqlite3_column_int(statement, 0);
    book.title = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1) ? sqlite3_column_text(statement, 1) : reinterpret_cast<const unsigned char*>(""));
    book.author = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2) ? sqlite3_column_text(statement, 2) : reinterpret_cast<const unsigned char*>(""));
    book.genre = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3) ? sqlite3_column_text(statement, 3) : reinterpret_cast<const unsigned char*>(""));
    book.subgenre = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4) ? sqlite3_column_text(statement, 4) : reinterpret_cast<const unsigned char*>(""));
    book.publisher = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5) ? sqlite3_column_text(statement, 5) : reinterpret_cast<const unsigned char*>(""));
    book.year = sqlite3_column_int(statement, 6);
    book.format = reinterpret_cast<const char*>(sqlite3_column_text(statement, 7) ? sqlite3_column_text(statement, 7) : reinterpret_cast<const unsigned char*>(""));
    book.rating = sqlite3_column_double(statement, 8);
    book.price = sqlite3_column_double(statement, 9);
    book.ageRating = reinterpret_cast<const char*>(sqlite3_column_text(statement, 10) ? sqlite3_column_text(statement, 10) : reinterpret_cast<const unsigned char*>(""));
    book.isbn = reinterpret_cast<const char*>(sqlite3_column_text(statement, 11) ? sqlite3_column_text(statement, 11) : reinterpret_cast<const unsigned char*>(""));
    book.totalPrintRun = sqlite3_column_int64(statement, 12);
    book.signedToPrintDate = reinterpret_cast<const char*>(sqlite3_column_text(statement, 13) ? sqlite3_column_text(statement, 13) : reinterpret_cast<const unsigned char*>(""));
    book.additionalPrintDates = split(reinterpret_cast<const char*>(sqlite3_column_text(statement, 14) ? sqlite3_column_text(statement, 14) : reinterpret_cast<const unsigned char*>("")), '|');
    book.coverImagePath = reinterpret_cast<const char*>(sqlite3_column_text(statement, 15) ? sqlite3_column_text(statement, 15) : reinterpret_cast<const unsigned char*>(""));
    book.licenseImagePath = reinterpret_cast<const char*>(sqlite3_column_text(statement, 16) ? sqlite3_column_text(statement, 16) : reinterpret_cast<const unsigned char*>(""));
    book.bibliographicReference = reinterpret_cast<const char*>(sqlite3_column_text(statement, 17) ? sqlite3_column_text(statement, 17) : reinterpret_cast<const unsigned char*>(""));
    book.coverUrl = reinterpret_cast<const char*>(sqlite3_column_text(statement, 18) ? sqlite3_column_text(statement, 18) : reinterpret_cast<const unsigned char*>(""));
    book.searchFrequency = sqlite3_column_double(statement, 19);
    return book;
}

std::vector<Book> LibraryStorage::allBooks() const {
    sqlite3* db = static_cast<sqlite3*>(db_);
    std::vector<Book> books;
    sqlite3_stmt* raw = nullptr;
    const std::string sql = baseSelectSql() + " ORDER BY lower(books.title), lower(books.author), books.id;";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
        return books;
    }
    StatementPtr stmt(raw);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        books.push_back(readBookFromStatement(stmt.get()));
    }
    return books;
}

std::vector<Book> LibraryStorage::searchBooks(const std::string& query) const {
    sqlite3* db = static_cast<sqlite3*>(db_);
    std::vector<Book> books;
    const std::string q = '%' + query + '%';
    const std::string sql = baseSelectSql() + R"SQL(
        WHERE lower(books.title) LIKE lower(?)
        ORDER BY lower(books.title), lower(books.author), books.id
    )SQL";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
        return books;
    }
    StatementPtr stmt(raw);
    if (!bindText(stmt.get(), 1, q)) {
        return books;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        books.push_back(readBookFromStatement(stmt.get()));
    }
    if (!books.empty()) {
        return books;
    }

    const std::string sqlAuthor = baseSelectSql() + R"SQL(
        WHERE lower(books.author) LIKE lower(?)
        ORDER BY lower(books.author), lower(books.title), books.id
    )SQL";
    raw = nullptr;
    if (sqlite3_prepare_v2(db, sqlAuthor.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
        return books;
    }
    stmt.reset(raw);
    if (!bindText(stmt.get(), 1, q)) {
        return books;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        books.push_back(readBookFromStatement(stmt.get()));
    }
    return books;
}

std::vector<Book> LibraryStorage::sortedBooks(SortField field, bool ascending) const {
    sqlite3* db = static_cast<sqlite3*>(db_);
    std::vector<Book> books;
    sqlite3_stmt* raw = nullptr;
    const std::string sql = baseSelectSql() + " ORDER BY " + sqlSortColumn(field) + (ascending ? " ASC" : " DESC") + ", lower(title), books.id;";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
        return books;
    }
    StatementPtr stmt(raw);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        books.push_back(readBookFromStatement(stmt.get()));
    }
    return books;
}

bool LibraryStorage::upsertBook(Book& book) {
    if (!ensureGenreHierarchy(book)) {
        return false;
    }

    sqlite3* db = static_cast<sqlite3*>(db_);
    const char* sql = R"SQL(
        INSERT INTO books(
            id, genre_id, subgenre_id, title, author, publisher, year, format, rating,
            price, age_rating, isbn, total_print_run, signed_to_print_date,
            additional_print_dates, cover_image_path, license_image_path,
            bibliographic_reference, cover_url, search_frequency
        ) VALUES (
            NULLIF(?, 0),
            (SELECT id FROM genres WHERE name = ?),
            (SELECT subgenres.id FROM subgenres
                JOIN genres ON genres.id = subgenres.genre_id
             WHERE genres.name = ? AND subgenres.name = ?),
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
        )
        ON CONFLICT(id) DO UPDATE SET
            genre_id = excluded.genre_id,
            subgenre_id = excluded.subgenre_id,
            title = excluded.title,
            author = excluded.author,
            publisher = excluded.publisher,
            year = excluded.year,
            format = excluded.format,
            rating = excluded.rating,
            price = excluded.price,
            age_rating = excluded.age_rating,
            isbn = excluded.isbn,
            total_print_run = excluded.total_print_run,
            signed_to_print_date = excluded.signed_to_print_date,
            additional_print_dates = excluded.additional_print_dates,
            cover_image_path = excluded.cover_image_path,
            license_image_path = excluded.license_image_path,
            bibliographic_reference = excluded.bibliographic_reference,
            cover_url = excluded.cover_url,
            search_frequency = excluded.search_frequency;
    )SQL";

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StatementPtr stmt(raw);

    sqlite3_bind_int(stmt.get(), 1, book.id);
    bindText(stmt.get(), 2, book.genre);
    bindText(stmt.get(), 3, book.genre);
    bindText(stmt.get(), 4, book.subgenre);
    bindText(stmt.get(), 5, book.title);
    bindText(stmt.get(), 6, book.author);
    bindText(stmt.get(), 7, book.publisher);
    sqlite3_bind_int(stmt.get(), 8, book.year);
    bindText(stmt.get(), 9, book.format);
    sqlite3_bind_double(stmt.get(), 10, book.rating);
    sqlite3_bind_double(stmt.get(), 11, book.price);
    bindText(stmt.get(), 12, book.ageRating);
    bindText(stmt.get(), 13, book.isbn);
    sqlite3_bind_int64(stmt.get(), 14, book.totalPrintRun);
    bindText(stmt.get(), 15, book.signedToPrintDate);
    bindText(stmt.get(), 16, join(book.additionalPrintDates, '|'));
    bindText(stmt.get(), 17, book.coverImagePath);
    bindText(stmt.get(), 18, book.licenseImagePath);
    bindText(stmt.get(), 19, book.bibliographicReference);
    bindText(stmt.get(), 20, book.coverUrl);
    sqlite3_bind_double(stmt.get(), 21, book.searchFrequency);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        return false;
    }

    if (book.id == 0) {
        book.id = static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    return true;
}

bool LibraryStorage::removeBookById(int id) {
    sqlite3* db = static_cast<sqlite3*>(db_);
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM books WHERE id = ?;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StatementPtr stmt(raw);
    sqlite3_bind_int(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        return false;
    }
    return sqlite3_changes(db) > 0;
}

bool LibraryStorage::isEmpty() const {
    sqlite3* db = static_cast<sqlite3*>(db_);
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM books;", -1, &raw, nullptr) != SQLITE_OK) {
        return true;
    }
    StatementPtr stmt(raw);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return true;
    }
    return sqlite3_column_int(stmt.get(), 0) == 0;
}

std::optional<Book> NetworkMetadataClient::fetchByQuery(const Book& draft) const {
    if (!isNetworkEnabled()) {
        appendLog("WARNING", "Network disabled by OFFLINE_MODE/LIBRARY_ENABLE_NET. API enrichment skipped.", "data/library.db");
        return std::nullopt;
    }
    StatementPtr stmt(raw);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        books.push_back(readBookFromStatement(stmt.get()));
    }
    return books;
}

    std::string query = draft.isbn;
    if (query.empty()) {
        query = draft.title;
        if (!draft.author.empty()) {
            query += " " + draft.author;
        }
    }
    query = trim(query);
    if (query.empty()) {
        return std::nullopt;
    }

    std::string encoded = query;
    std::replace(encoded.begin(), encoded.end(), ' ', '+');
    const std::string url = "https://openlibrary.org/search.json?q=" + encoded + "&limit=1&fields=title,author_name,first_publish_year,isbn,publisher,cover_i";
    std::string response;
    int httpCode = 0;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        const std::string command =
            "curl -sS --max-time 5 -H 'User-Agent: LibraryBackendCPP/1.0' " + shellEscape(url) +
            " -w '\\n__HTTP_CODE__:%{http_code}\\n'";
        const std::string raw = readCommandOutput(command);
        if (raw.empty()) {
            appendLog("WARNING", "API timeout/empty response, attempt " + std::to_string(attempt), "data/library.db");
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        const std::string marker = "__HTTP_CODE__:";
        const auto markerPos = raw.rfind(marker);
        if (markerPos != std::string::npos) {
            response = raw.substr(0, markerPos);
            const auto codePart = trim(raw.substr(markerPos + marker.size()));
            try {
                httpCode = std::stoi(codePart);
            } catch (...) {
                httpCode = 0;
            }
        } else {
            response = raw;
            httpCode = 0;
        }

        if (httpCode == 200) {
            break;
        }
        if (httpCode == 404) {
            appendLog("INFO", "OpenLibrary returned 404 for query: " + query, "data/library.db");
            return std::nullopt;
        }
        if (httpCode == 429) {
            appendLog("WARNING", "OpenLibrary rate limited (429), attempt " + std::to_string(attempt), "data/library.db");
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            continue;
        }
        if (httpCode >= 500 || httpCode == 0) {
            appendLog("WARNING", "OpenLibrary server/network error code=" + std::to_string(httpCode) + ", attempt " + std::to_string(attempt), "data/library.db");
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }
        if (httpCode >= 400) {
            appendLog("WARNING", "OpenLibrary bad request code=" + std::to_string(httpCode), "data/library.db");
            return std::nullopt;
        }
    }

    if (response.empty() || httpCode != 200) {
        appendLog("WARNING", "API enrichment fallback used for query: " + query, "data/library.db");
        return std::nullopt;
    }
    StatementPtr stmt(raw);

    Book remote;
    remote.title = jsonStringField(response, "title");
    remote.author = jsonStringField(response, "author_name");
    remote.publisher = jsonStringField(response, "publisher");
    remote.year = jsonIntegerField(response, "first_publish_year");

    const std::string isbn = jsonStringField(response, "isbn");
    if (!isbn.empty()) {
        remote.isbn = isbn;
    }

    const int coverId = jsonIntegerField(response, "cover_i");
    if (coverId > 0) {
        remote.coverUrl = "https://covers.openlibrary.org/b/id/" + std::to_string(coverId) + "-L.jpg";
    }

    if (remote.title.empty() && remote.author.empty() && remote.year == 0 && remote.isbn.empty() && remote.coverUrl.empty()) {
        return std::nullopt;
    }
    return remote;
}

namespace {
Book makeDemoBook(
    const std::string& title,
    const std::string& author,
    const std::string& genre,
    const std::string& subgenre,
    const std::string& publisher,
    int year,
    const std::string& format,
    double rating,
    double price,
    const std::string& ageRating,
    const std::string& isbn,
    std::int64_t totalPrintRun,
    const std::string& bibliographicReference,
    const std::string& coverUrl) {
    Book book;
    book.title = title;
    book.author = author;
    book.genre = genre;
    book.subgenre = subgenre;
    book.publisher = publisher;
    book.year = year;
    book.format = format;
    book.rating = rating;
    book.price = price;
    book.ageRating = ageRating;
    book.isbn = isbn;
    book.totalPrintRun = totalPrintRun;
    book.bibliographicReference = bibliographicReference;
    book.coverUrl = coverUrl;
    book.searchFrequency = rating;
    return book;
}

std::vector<Book> demoBooks() {
    std::vector<Book> books;
    books.reserve(10);
    books.push_back(makeDemoBook("Мастер и Маргарита", "Михаил Булгаков", "Художественная", "Роман", "Эксмо", 1967, "84x108/32", 4.9, 590.0, "18+", "978-5-04-116270-1", 50000, "Булгаков М. А. Мастер и Маргарита. — М.: Эксмо, 2023. — 480 с.", "https://covers.openlibrary.org/b/id/12192618-L.jpg"));
    books.push_back(makeDemoBook("Преступление и наказание", "Ф. Достоевский", "Художественная", "Роман", "Дет. литература", 1866, "70x100/16", 4.7, 450.0, "16+", "978-5-08-006491-5", 30000, "", "https://covers.openlibrary.org/b/id/8739200-L.jpg"));
    books.push_back(makeDemoBook("Краткая история времени", "Стивен Хокинг", "Научная", "Физика", "АСТ", 1988, "70x100/16", 4.8, 680.0, "12+", "978-5-17-077748-3", 25000, "", "https://covers.openlibrary.org/b/id/8575708-L.jpg"));
    books.push_back(makeDemoBook("Гарри Поттер и фил. камень", "Дж. К. Роулинг", "Детская", "Приключения", "Росмэн", 1997, "60x90/16", 4.9, 750.0, "6+", "978-5-353-01435-0", 100000, "", "https://covers.openlibrary.org/b/id/10521270-L.jpg"));
    books.push_back(makeDemoBook("Clean Code", "Robert C. Martin", "Техническая", "Программирование", "Питер", 2008, "70x100/16", 4.6, 1200.0, "0+", "978-5-4461-0960-9", 15000, "", "https://covers.openlibrary.org/b/id/8950546-L.jpg"));
    books.push_back(makeDemoBook("Дюна", "Фрэнк Герберт", "Художественная", "Роман", "АСТ", 1965, "84x108/32", 4.8, 820.0, "16+", "978-5-17-090658-4", 40000, "", "https://covers.openlibrary.org/b/id/10521270-L.jpg"));
    books.push_back(makeDemoBook("1984", "Джордж Оруэлл", "Художественная", "Роман", "АСТ", 1949, "84x108/32", 4.7, 420.0, "16+", "978-5-17-108831-3", 60000, "", "https://covers.openlibrary.org/b/id/8575708-L.jpg"));
    books.push_back(makeDemoBook("Война и мир", "Лев Толстой", "Историческая", "Новое время", "АСТ", 1869, "84x108/32", 4.6, 1100.0, "12+", "978-5-17-119218-3", 35000, "", "https://covers.openlibrary.org/b/id/9255566-L.jpg"));
    books.push_back(makeDemoBook("Cosmos", "Карл Саган", "Научная", "Астрономия", "АСТ", 1980, "70x100/8", 4.9, 990.0, "12+", "978-5-17-094029-0", 20000, "", "https://covers.openlibrary.org/b/id/8739290-L.jpg"));
    books.push_back(makeDemoBook("Имя розы", "Умберто Эко", "Историческая", "Средневековье", "Азбука", 1980, "84x108/32", 4.5, 650.0, "16+", "978-5-389-01806-7", 25000, "", "https://covers.openlibrary.org/b/id/8739248-L.jpg"));
    return books;
}
} // namespace

LibraryBackendService::LibraryBackendService(LibraryStorage storage)
    : storage_(std::move(storage)) {}

bool LibraryBackendService::initialize() {
    if (!storage_.open()) {
        return false;
    }
    if (!storage_.isEmpty()) {
        return true;
    }

    for (auto book : demoBooks()) {
        if (!storage_.upsertBook(book)) {
            return false;
        }
    }
    return true;
}

bool LibraryBackendService::addOrUpdateBook(Book& book, bool fetchFromNetwork) {
    if (fetchFromNetwork && isNetworkEnabled()) {
        const auto remote = networkClient_.fetchByQuery(book);
        if (remote.has_value()) {
            if (book.title.empty()) {
                book.title = remote->title;
            }
            if (book.author.empty()) {
                book.author = remote->author;
            }
            if (book.publisher.empty()) {
                book.publisher = remote->publisher;
            }
            if (book.year == 0) {
                book.year = remote->year;
            }
            if (book.isbn.empty()) {
                book.isbn = remote->isbn;
            }
            if (book.coverUrl.empty()) {
                book.coverUrl = remote->coverUrl;
            }
        }
    } else if (fetchFromNetwork && !isNetworkEnabled()) {
        appendLog("WARNING", "Offline mode enabled; book saved without API enrichment", "data/library.db");
    }
    query = trim(query);
    if (query.empty()) {
        return std::nullopt;
    }

    if (trim(book.title).empty() || trim(book.author).empty()) {
        return false;
    }

    const bool ok = storage_.upsertBook(book);
    if (ok) {
        appendLog("INFO", "Book upserted id=" + std::to_string(book.id) + " title=" + book.title, "data/library.db");
    } else {
        appendLog("ERROR", "Book upsert failed title=" + book.title, "data/library.db");
    }
    return ok;
}

bool LibraryBackendService::removeBookById(int id) {
    std::string coverPath;
    std::string licensePath;
    for (const auto& book : storage_.allBooks()) {
        if (book.id == id) {
            coverPath = book.coverImagePath;
            licensePath = book.licenseImagePath;
            break;
        }
    }
    return true;
}

    if (!storage_.removeBookById(id)) {
        return false;
    }

    auto removeFileIfExists = [&](const std::string& pathValue) {
        if (trim(pathValue).empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(pathValue, ec);
        if (ec) {
            appendLog("WARNING", "Failed to remove file on book delete: " + pathValue + " (" + ec.message() + ")", "data/library.db");
        }
    };

    removeFileIfExists(coverPath);
    removeFileIfExists(licensePath);
    appendLog("INFO", "Book removed id=" + std::to_string(id), "data/library.db");
    return true;
}

std::vector<Book> LibraryBackendService::searchBooks(const std::string& query) const {
    return storage_.searchBooks(query);
}

std::vector<Book> LibraryBackendService::sortedBooks(SortField field, bool ascending) const {
    return storage_.sortedBooks(field, ascending);
}

std::vector<Book> LibraryBackendService::allBooks() const {
    return storage_.allBooks();
}

std::vector<ObstNode> LibraryBackendService::buildOptimalSearchTreeByIsbn() const {
    auto books = storage_.allBooks();
    std::sort(books.begin(), books.end(), [](const Book& lhs, const Book& rhs) {
        return lhs.isbn < rhs.isbn;
    });

    const int n = static_cast<int>(books.size());
    if (n == 0) {
        return {};
    }

    std::vector<std::vector<double>> cost(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<int>> root(n, std::vector<int>(n, -1));
    std::vector<double> prefix(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        prefix[i + 1] = prefix[i] + std::max(books[i].searchFrequency, 0.1);
        cost[i][i] = std::max(books[i].searchFrequency, 0.1);
        root[i][i] = i;
    }

    for (int len = 2; len <= n; ++len) {
        for (int i = 0; i + len - 1 < n; ++i) {
            const int j = i + len - 1;
            const double freqSum = prefix[j + 1] - prefix[i];
            cost[i][j] = std::numeric_limits<double>::max();
            for (int r = i; r <= j; ++r) {
                const double left = (r > i) ? cost[i][r - 1] : 0.0;
                const double right = (r < j) ? cost[r + 1][j] : 0.0;
                const double current = left + right + freqSum;
                if (current < cost[i][j]) {
                    cost[i][j] = current;
                    root[i][j] = r;
                }
            }
        }
    }

    std::vector<ObstNode> nodes;
    nodes.reserve(n);
    std::function<int(int, int)> build = [&](int left, int right) -> int {
        if (left > right) {
            return -1;
        }
        const int pivot = root[left][right];
        const int idx = static_cast<int>(nodes.size());
        nodes.push_back(ObstNode{books[pivot].isbn, books[pivot].id, -1, -1});
        nodes[idx].left = build(left, pivot - 1);
        nodes[idx].right = build(pivot + 1, right);
        return idx;
    };

    build(0, n - 1);
    return nodes;
}

SortField LibraryBackendService::parseSortField(const std::string& value) {
    const std::string key = normalizeFieldName(value);
    static const std::map<std::string, SortField> mapping = {
        {"title", SortField::Title},
        {"author", SortField::Author},
        {"genre", SortField::Genre},
        {"subgenre", SortField::Subgenre},
        {"publisher", SortField::Publisher},
        {"year", SortField::Year},
        {"format", SortField::Format},
        {"rating", SortField::Rating},
        {"price", SortField::Price},
        {"age", SortField::AgeRating},
        {"agerating", SortField::AgeRating},
        {"isbn", SortField::Isbn},
        {"edition", SortField::TotalPrintRun},
        {"totalprintrun", SortField::TotalPrintRun},
        {"signdate", SortField::SignedToPrintDate},
        {"signedtoprintdate", SortField::SignedToPrintDate},
    };
    const auto it = mapping.find(key);
    return it == mapping.end() ? SortField::Title : it->second;
}

std::string LibraryBackendService::normalize(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (unsigned char c : value) {
        result.push_back(static_cast<char>(std::tolower(c)));
    }
    return result;
}

std::string serializeBookList(const std::vector<Book>& books) {
    std::ostringstream out;
    for (const auto& book : books) {
        out << "BEGIN_BOOK\n";
        out << "id=" << book.id << "\n";
        out << "title=" << escapeText(book.title) << "\n";
        out << "author=" << escapeText(book.author) << "\n";
        out << "genre=" << escapeText(book.genre) << "\n";
        out << "subgenre=" << escapeText(book.subgenre) << "\n";
        out << "publisher=" << escapeText(book.publisher) << "\n";
        out << "year=" << book.year << "\n";
        out << "format=" << escapeText(book.format) << "\n";
        out << "rating=" << book.rating << "\n";
        out << "price=" << book.price << "\n";
        out << "age_rating=" << escapeText(book.ageRating) << "\n";
        out << "isbn=" << escapeText(book.isbn) << "\n";
        out << "total_print_run=" << book.totalPrintRun << "\n";
        out << "signed_to_print_date=" << escapeText(book.signedToPrintDate) << "\n";
        out << "additional_print_dates=" << escapeText(join(book.additionalPrintDates, '|')) << "\n";
        out << "cover_image_path=" << escapeText(book.coverImagePath) << "\n";
        out << "license_image_path=" << escapeText(book.licenseImagePath) << "\n";
        out << "bibliographic_reference=" << escapeText(book.bibliographicReference) << "\n";
        out << "cover_url=" << escapeText(book.coverUrl) << "\n";
        out << "search_frequency=" << book.searchFrequency << "\n";
        out << "END_BOOK\n";
    }
    return out.str();
}

std::optional<Book> parseBookFile(const std::string& filePath) {
    std::ifstream input(filePath);
    if (!input.good()) {
        return std::nullopt;
    }

    Book book;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line == "BEGIN_BOOK" || line == "END_BOOK") {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, pos));
        const std::string value = unescapeText(line.substr(pos + 1));

        if (key == "id") {
            book.id = std::atoi(value.c_str());
        } else if (key == "title") {
            book.title = value;
        } else if (key == "author") {
            book.author = value;
        } else if (key == "genre") {
            book.genre = value;
        } else if (key == "subgenre") {
            book.subgenre = value;
        } else if (key == "publisher") {
            book.publisher = value;
        } else if (key == "year") {
            book.year = std::atoi(value.c_str());
        } else if (key == "format") {
            book.format = value;
        } else if (key == "rating") {
            book.rating = std::atof(value.c_str());
        } else if (key == "price") {
            book.price = std::atof(value.c_str());
        } else if (key == "age_rating") {
            book.ageRating = value;
        } else if (key == "isbn") {
            book.isbn = value;
        } else if (key == "total_print_run") {
            book.totalPrintRun = std::atoll(value.c_str());
        } else if (key == "signed_to_print_date") {
            book.signedToPrintDate = value;
        } else if (key == "additional_print_dates") {
            book.additionalPrintDates = split(value, '|');
        } else if (key == "cover_image_path") {
            book.coverImagePath = value;
        } else if (key == "license_image_path") {
            book.licenseImagePath = value;
        } else if (key == "bibliographic_reference") {
            book.bibliographicReference = value;
        } else if (key == "cover_url") {
            book.coverUrl = value;
        } else if (key == "search_frequency") {
            book.searchFrequency = std::atof(value.c_str());
        }
    }

    if (trim(book.title).empty() && trim(book.author).empty()) {
        return std::nullopt;
    }
    return book;
}

std::optional<Book> parseBookFile(const std::string& filePath) {
    std::ifstream input(filePath);
    if (!input.good()) {
        return std::nullopt;
    }

    Book book;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line == "BEGIN_BOOK" || line == "END_BOOK") {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, pos));
        const std::string value = unescapeText(line.substr(pos + 1));

        if (key == "id") {
            book.id = std::atoi(value.c_str());
        } else if (key == "title") {
            book.title = value;
        } else if (key == "author") {
            book.author = value;
        } else if (key == "genre") {
            book.genre = value;
        } else if (key == "subgenre") {
            book.subgenre = value;
        } else if (key == "publisher") {
            book.publisher = value;
        } else if (key == "year") {
            book.year = std::atoi(value.c_str());
        } else if (key == "format") {
            book.format = value;
        } else if (key == "rating") {
            book.rating = std::atof(value.c_str());
        } else if (key == "price") {
            book.price = std::atof(value.c_str());
        } else if (key == "age_rating") {
            book.ageRating = value;
        } else if (key == "isbn") {
            book.isbn = value;
        } else if (key == "total_print_run") {
            book.totalPrintRun = std::atoll(value.c_str());
        } else if (key == "signed_to_print_date") {
            book.signedToPrintDate = value;
        } else if (key == "additional_print_dates") {
            book.additionalPrintDates = split(value, '|');
        } else if (key == "cover_image_path") {
            book.coverImagePath = value;
        } else if (key == "license_image_path") {
            book.licenseImagePath = value;
        } else if (key == "bibliographic_reference") {
            book.bibliographicReference = value;
        } else if (key == "cover_url") {
            book.coverUrl = value;
        } else if (key == "search_frequency") {
            book.searchFrequency = std::atof(value.c_str());
        }
    }

    if (trim(book.title).empty() && trim(book.author).empty()) {
        return std::nullopt;
    }
    return book;
}