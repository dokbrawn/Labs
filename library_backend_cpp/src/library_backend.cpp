#include "library_backend.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <libpq-fe.h>

namespace fs = std::filesystem;

static const char* LOG_FILE_NAME = "library.log";
static const char* IMAGES_DIR_NAME = "images";

static std::string nowTimestamp() {
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

static void logMessage(const std::string& level, const std::string& text) {
    std::ofstream log(LOG_FILE_NAME, std::ios::app);
    if (!log.is_open()) {
        return;
    }
    log << nowTimestamp() << " [" << level << "] " << text << "\n";
}

static std::string getEnvOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    return std::string(value);
}

static bool envFlagEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value) {
        return false;
    }
    std::string s(value);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s == "1" || s == "true" || s == "yes" || s == "on";
}

static std::string shellEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 2);
    out.push_back('\'');
    for (char c : input) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

static std::string urlEncode(const std::string& value) {
    static const char* HEX = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(HEX[c >> 4]);
            out.push_back(HEX[c & 0x0F]);
        }
    }
    return out;
}

static std::string readCommandOutput(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return {};
    }

    std::string output;
    char chunk[512];
    while (std::fgets(chunk, sizeof(chunk), pipe) != nullptr) {
        output += chunk;
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return output;
}

static std::string jsonStringFromArray(const std::string& object, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t keyPos = object.find(marker);
    if (keyPos == std::string::npos) {
        return {};
    }
    const std::size_t open = object.find('[', keyPos);
    const std::size_t quote1 = object.find('"', open);
    const std::size_t quote2 = object.find('"', quote1 + 1);
    if (open == std::string::npos || quote1 == std::string::npos || quote2 == std::string::npos) {
        return {};
    }
    return object.substr(quote1 + 1, quote2 - quote1 - 1);
}

static std::string jsonStringField(const std::string& object, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t keyPos = object.find(marker);
    if (keyPos == std::string::npos) {
        return {};
    }
    const std::size_t quote1 = object.find('"', keyPos + marker.size());
    const std::size_t quote2 = object.find('"', quote1 + 1);
    if (quote1 == std::string::npos || quote2 == std::string::npos) {
        return {};
    }
    return object.substr(quote1 + 1, quote2 - quote1 - 1);
}

static int jsonIntField(const std::string& object, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t keyPos = object.find(marker);
    if (keyPos == std::string::npos) {
        return 0;
    }
    const std::size_t colon = object.find(':', keyPos + marker.size());
    if (colon == std::string::npos) {
        return 0;
    }
    std::size_t begin = colon + 1;
    while (begin < object.size() && std::isspace(static_cast<unsigned char>(object[begin]))) {
        ++begin;
    }
    std::size_t end = begin;
    while (end < object.size() && std::isdigit(static_cast<unsigned char>(object[end]))) {
        ++end;
    }
    if (begin == end) {
        return 0;
    }
    return std::atoi(object.substr(begin, end - begin).c_str());
}

static std::vector<std::string> extractDocObjects(const std::string& json) {
    std::vector<std::string> docs;
    const std::size_t docsKey = json.find("\"docs\"");
    if (docsKey == std::string::npos) {
        return docs;
    }
    const std::size_t arrStart = json.find('[', docsKey);
    if (arrStart == std::string::npos) {
        return docs;
    }

    int braces = 0;
    std::size_t objStart = std::string::npos;
    for (std::size_t i = arrStart; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '{') {
            if (braces == 0) {
                objStart = i;
            }
            ++braces;
        } else if (c == '}') {
            --braces;
            if (braces == 0 && objStart != std::string::npos) {
                docs.push_back(json.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
        } else if (c == ']' && braces == 0) {
            break;
        }
    }
    return docs;
}

static std::string trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

static std::string normalizeLower(const std::string& value) {
    std::string out = trim(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

static std::string sqlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        if (c == '\'') {
            out += "''";
        } else {
            out += c;
        }
    }
    return out;
}

static std::string escapeValue(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '=') out += "\\=";
        else out += c;
    }
    return out;
}

static std::string unescapeValue(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[i + 1];
            if (n == 'n') {
                out += '\n';
                ++i;
            } else if (n == 'r') {
                out += '\r';
                ++i;
            } else if (n == '=') {
                out += '=';
                ++i;
            } else if (n == '\\') {
                out += '\\';
                ++i;
            } else {
                out += s[i];
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static bool fileExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return fs::exists(path, ec);
}

static bool deleteFileIfExists(const std::string& path) {
    if (path.empty()) {
        return true;
    }
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return true;
    }
    return fs::remove(path, ec);
}

static void ensureImagesDir() {
    std::error_code ec;
    if (!fs::exists(IMAGES_DIR_NAME, ec)) {
        fs::create_directories(IMAGES_DIR_NAME, ec);
    }
}

Book LibraryStorage::readBookFromStatement(void* stmtPtr) {
    PGresult* result = static_cast<PGresult*>(stmtPtr);
    Book book;
    if (!result || PQntuples(result) <= 0) {
        return book;
    }

    auto getValue = [&](int row, const char* colName) -> std::string {
        const int col = PQfnumber(result, colName);
        if (col < 0 || PQgetisnull(result, row, col)) {
            return "";
        }
        return PQgetvalue(result, row, col);
    };

    book.id = std::atoi(getValue(0, "id").c_str());
    book.title = getValue(0, "title");
    book.author = getValue(0, "author");
    book.genre = getValue(0, "genre");
    book.subgenre = getValue(0, "subgenre");
    book.publisher = getValue(0, "publisher");
    book.year = std::atoi(getValue(0, "year").c_str());
    book.format = getValue(0, "format");
    book.rating = std::atof(getValue(0, "rating").c_str());
    book.price = std::atof(getValue(0, "price").c_str());
    book.ageRating = getValue(0, "age_rating");
    book.isbn = getValue(0, "isbn");
    book.totalPrintRun = std::atoll(getValue(0, "total_circulation").c_str());
    book.signedToPrintDate = getValue(0, "print_sign_date");
    const std::string additional = getValue(0, "additional_prints");
    if (!additional.empty()) {
        book.additionalPrintDates.push_back(additional);
    }
    book.coverImagePath = getValue(0, "cover_image_path");
    book.licenseImagePath = getValue(0, "license_image_path");
    book.bibliographicReference = getValue(0, "bibliographic_ref");
    return book;
}

LibraryStorage::LibraryStorage(std::string connectionString)
    : connectionString_(std::move(connectionString)), db_(nullptr) {
    if (connectionString_.empty()) {
        connectionString_ = getEnvOrDefault(
            "LIBRARY_PG_CONN",
            "host=localhost port=5432 dbname=library user=postgres password=123"
        );
    }
}

LibraryStorage::~LibraryStorage() {
    if (db_ != nullptr) {
        PQfinish(static_cast<PGconn*>(db_));
        db_ = nullptr;
    }
}

bool LibraryStorage::open() {
    if (db_ != nullptr) {
        return true;
    }

    PGconn* conn = PQconnectdb(connectionString_.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        logMessage("ERROR", std::string("DB connect failed: ") + PQerrorMessage(conn));
        PQfinish(conn);
        return false;
    }

    db_ = conn;
    ensureImagesDir();
    return true;
}

bool LibraryStorage::execute(const std::string& sql) const {
    if (db_ == nullptr) {
        return false;
    }

    PGresult* result = PQexec(static_cast<PGconn*>(db_), sql.c_str());
    const ExecStatusType status = PQresultStatus(result);

    const bool ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    if (!ok) {
        logMessage("ERROR", "SQL failed: " + sql + " | " + PQerrorMessage(static_cast<PGconn*>(db_)));
    }

    PQclear(result);
    return ok;
}

static bool columnExists(PGconn* conn, const std::string& table, const std::string& column) {
    std::string sql =
        "SELECT 1 FROM information_schema.columns "
        "WHERE table_schema='public' AND table_name='" + sqlEscape(table) +
        "' AND column_name='" + sqlEscape(column) + "' LIMIT 1;";

    PGresult* result = PQexec(conn, sql.c_str());
    bool exists = false;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        exists = true;
    }
    PQclear(result);
    return exists;
}

bool LibraryStorage::runMigrations() {
    if (!open()) {
        return false;
    }

    PGconn* conn = static_cast<PGconn*>(db_);

    if (!execute(
        "CREATE TABLE IF NOT EXISTS genres ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE"
        ");")) {
        return false;
    }

    if (!execute(
        "CREATE TABLE IF NOT EXISTS subgenres ("
        "id SERIAL PRIMARY KEY,"
        "genre_id INTEGER NOT NULL REFERENCES genres(id) ON DELETE CASCADE,"
        "name TEXT NOT NULL,"
        "UNIQUE(genre_id, name)"
        ");")) {
        return false;
    }

    if (!execute(
        "CREATE TABLE IF NOT EXISTS books ("
        "id SERIAL PRIMARY KEY,"
        "title TEXT NOT NULL,"
        "author TEXT NOT NULL,"
        "subgenre_id INTEGER REFERENCES subgenres(id) ON DELETE SET NULL,"
        "publisher TEXT DEFAULT '',"
        "year INTEGER DEFAULT 0,"
        "format TEXT DEFAULT '',"
        "rating DOUBLE PRECISION DEFAULT 0,"
        "price DOUBLE PRECISION DEFAULT 0,"
        "age_rating TEXT DEFAULT '0+',"
        "isbn TEXT UNIQUE,"
        "total_circulation BIGINT DEFAULT 0,"
        "print_sign_date TEXT DEFAULT '',"
        "additional_prints TEXT DEFAULT '[]',"
        "cover_image_path TEXT DEFAULT '',"
        "license_image_path TEXT DEFAULT '',"
        "bibliographic_ref TEXT DEFAULT '',"
        "created_at TIMESTAMP DEFAULT NOW()"
        ");")) {
        return false;
    }

    const std::vector<std::pair<std::string, std::string>> migrations = {
        {"books", "publisher TEXT DEFAULT ''"},
        {"books", "format TEXT DEFAULT ''"},
        {"books", "rating DOUBLE PRECISION DEFAULT 0"},
        {"books", "price DOUBLE PRECISION DEFAULT 0"},
        {"books", "age_rating TEXT DEFAULT '0+'"},
        {"books", "isbn TEXT UNIQUE"},
        {"books", "total_circulation BIGINT DEFAULT 0"},
        {"books", "print_sign_date TEXT DEFAULT ''"},
        {"books", "additional_prints TEXT DEFAULT '[]'"},
        {"books", "cover_image_path TEXT DEFAULT ''"},
        {"books", "license_image_path TEXT DEFAULT ''"},
        {"books", "bibliographic_ref TEXT DEFAULT ''"},
        {"books", "created_at TIMESTAMP DEFAULT NOW()"}
    };

    for (const auto& migration : migrations) {
        const std::string table = migration.first;
        const std::string def = migration.second;
        const std::string column = trim(def.substr(0, def.find(' ')));

        if (!columnExists(conn, table, column)) {
            if (!execute("ALTER TABLE " + table + " ADD COLUMN " + def + ";")) {
                return false;
            }
        }
    }

    execute("CREATE INDEX IF NOT EXISTS idx_books_title ON books(title);");
    execute("CREATE INDEX IF NOT EXISTS idx_books_author ON books(author);");
    execute("CREATE INDEX IF NOT EXISTS idx_books_subgenre_id ON books(subgenre_id);");
    execute("CREATE INDEX IF NOT EXISTS idx_subgenres_genre_id ON subgenres(genre_id);");

    logMessage("INFO", "Migrations completed");
    return true;
}

bool LibraryStorage::ensureSchema() {
    return runMigrations();
}

bool LibraryStorage::ensureGenreHierarchy(const Book& book) const {
    if (db_ == nullptr) {
        return false;
    }

    PGconn* conn = static_cast<PGconn*>(db_);
    const std::string genre = trim(book.genre);
    const std::string subgenre = trim(book.subgenre);

    if (genre.empty() || subgenre.empty()) {
        return true;
    }

    std::string genreSql =
        "INSERT INTO genres(name) VALUES('" + sqlEscape(genre) + "') "
        "ON CONFLICT(name) DO NOTHING;";
    PGresult* result = PQexec(conn, genreSql.c_str());
    PQclear(result);

    std::string subgenreSql =
        "INSERT INTO subgenres(genre_id, name) "
        "SELECT id, '" + sqlEscape(subgenre) + "' FROM genres WHERE name='" + sqlEscape(genre) + "' "
        "ON CONFLICT(genre_id, name) DO NOTHING;";
    result = PQexec(conn, subgenreSql.c_str());
    const bool ok = PQresultStatus(result) == PGRES_COMMAND_OK;
    PQclear(result);

    return ok;
}

bool LibraryStorage::isEmpty() const {
    if (db_ == nullptr) {
        return true;
    }

    PGresult* result = PQexec(static_cast<PGconn*>(db_), "SELECT COUNT(*) FROM books;");
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return true;
    }

    const bool empty = std::atoi(PQgetvalue(result, 0, 0)) == 0;
    PQclear(result);
    return empty;
}

bool LibraryStorage::rotateBackupIfNeeded() const {
    const std::string stampFile = ".last_backup_stamp";
    const auto now = std::chrono::system_clock::now();

    bool needBackup = true;
    if (fs::exists(stampFile)) {
        std::ifstream in(stampFile);
        long long lastTs = 0;
        in >> lastTs;
        const auto last = std::chrono::system_clock::time_point(std::chrono::seconds(lastTs));
        const auto diff = std::chrono::duration_cast<std::chrono::hours>(now - last).count();
        needBackup = diff >= 24 * 7;
    }

    if (!needBackup) {
        return true;
    }

    fs::create_directories("backup");
    const std::string fileName = "backup/library_backup.sql";

    std::string command = "pg_dump \"" + connectionString_ + "\" > \"" + fileName + "\"";
    const int rc = std::system(command.c_str());

    if (rc != 0) {
        logMessage("WARNING", "Backup failed via pg_dump");
        return false;
    }

    const auto epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::ofstream out(stampFile, std::ios::trunc);
    out << epoch;

    logMessage("INFO", "Backup completed: " + fileName);
    return true;
}

static std::vector<Book> fetchBooksBySql(PGconn* conn, const std::string& sql) {
    std::vector<Book> books;

    PGresult* result = PQexec(conn, sql.c_str());
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        logMessage("ERROR", "Fetch failed: " + std::string(PQerrorMessage(conn)));
        PQclear(result);
        return books;
    }

    const int rows = PQntuples(result);

    auto getValue = [&](int row, const char* colName) -> std::string {
        const int col = PQfnumber(result, colName);
        if (col < 0 || PQgetisnull(result, row, col)) {
            return "";
        }
        return PQgetvalue(result, row, col);
    };

    for (int i = 0; i < rows; ++i) {
        Book book;
        book.id = std::atoi(getValue(i, "id").c_str());
        book.title = getValue(i, "title");
        book.author = getValue(i, "author");
        book.genre = getValue(i, "genre");
        book.subgenre = getValue(i, "subgenre");
        book.publisher = getValue(i, "publisher");
        book.year = std::atoi(getValue(i, "year").c_str());
        book.format = getValue(i, "format");
        book.rating = std::atof(getValue(i, "rating").c_str());
        book.price = std::atof(getValue(i, "price").c_str());
        book.ageRating = getValue(i, "age_rating");
        book.isbn = getValue(i, "isbn");
        book.totalPrintRun = std::atoll(getValue(i, "total_circulation").c_str());
        book.signedToPrintDate = getValue(i, "print_sign_date");
        const std::string additional = getValue(i, "additional_prints");
        if (!additional.empty()) {
            book.additionalPrintDates.push_back(additional);
        }
        book.coverImagePath = getValue(i, "cover_image_path");
        book.licenseImagePath = getValue(i, "license_image_path");
        book.bibliographicReference = getValue(i, "bibliographic_ref");
        books.push_back(book);
    }

    PQclear(result);
    return books;
}

std::vector<Book> LibraryStorage::allBooks() const {
    if (db_ == nullptr) {
        return {};
    }

    return fetchBooksBySql(
        static_cast<PGconn*>(db_),
        "SELECT b.id, b.title, b.author, g.name AS genre, sg.name AS subgenre, "
        "b.publisher, b.year, b.format, b.rating, b.price, b.age_rating, b.isbn, "
        "b.total_circulation, b.print_sign_date, b.additional_prints, "
        "b.cover_image_path, b.license_image_path, b.bibliographic_ref "
        "FROM books b "
        "LEFT JOIN subgenres sg ON b.subgenre_id = sg.id "
        "LEFT JOIN genres g ON sg.genre_id = g.id;"
    );
}

static bool compareBooksByField(const Book& a, const Book& b, SortField field, bool ascending) {
    bool result = false;

    switch (field) {
        case SortField::Title:
            result = normalizeLower(a.title) <= normalizeLower(b.title);
            break;
        case SortField::Author:
            result = normalizeLower(a.author) <= normalizeLower(b.author);
            break;
        case SortField::Genre:
            result = normalizeLower(a.genre) <= normalizeLower(b.genre);
            break;
        case SortField::Subgenre:
            result = normalizeLower(a.subgenre) <= normalizeLower(b.subgenre);
            break;
        case SortField::Publisher:
            result = normalizeLower(a.publisher) <= normalizeLower(b.publisher);
            break;
        case SortField::Year:
            result = a.year <= b.year;
            break;
        case SortField::Format:
            result = normalizeLower(a.format) <= normalizeLower(b.format);
            break;
        case SortField::Rating:
            result = a.rating <= b.rating;
            break;
        case SortField::Price:
            result = a.price <= b.price;
            break;
        case SortField::AgeRating:
            result = normalizeLower(a.ageRating) <= normalizeLower(b.ageRating);
            break;
        case SortField::Isbn:
            result = normalizeLower(a.isbn) <= normalizeLower(b.isbn);
            break;
        case SortField::TotalPrintRun:
            result = a.totalPrintRun <= b.totalPrintRun;
            break;
        case SortField::SignedToPrintDate:
            result = normalizeLower(a.signedToPrintDate) <= normalizeLower(b.signedToPrintDate);
            break;
    }

    return ascending ? result : !result;
}

static void quickSortBooks(std::vector<Book>& books, int left, int right, SortField field, bool ascending) {
    if (left >= right) {
        return;
    }

    int i = left;
    int j = right;
    Book pivot = books[(left + right) / 2];

    while (i <= j) {
        while (compareBooksByField(books[i], pivot, field, ascending) &&
               !(compareBooksByField(pivot, books[i], field, ascending) &&
                 compareBooksByField(books[i], pivot, field, ascending))) {
            ++i;
            if (i > right) break;
        }

        while (compareBooksByField(pivot, books[j], field, ascending) &&
               !(compareBooksByField(books[j], pivot, field, ascending) &&
                 compareBooksByField(pivot, books[j], field, ascending))) {
            --j;
            if (j < left) break;
        }

        if (i <= j && i <= right && j >= left) {
            std::swap(books[i], books[j]);
            ++i;
            --j;
        }
    }

    if (left < j) quickSortBooks(books, left, j, field, ascending);
    if (i < right) quickSortBooks(books, i, right, field, ascending);
}

std::vector<Book> LibraryStorage::sortedBooks(SortField field, bool ascending) const {
    std::vector<Book> books = allBooks();
    if (!books.empty()) {
        quickSortBooks(books, 0, static_cast<int>(books.size()) - 1, field, ascending);
    }
    return books;
}

static int binarySearchFirstTitle(const std::vector<Book>& books, const std::string& q) {
    int left = 0;
    int right = static_cast<int>(books.size()) - 1;
    int found = -1;

    while (left <= right) {
        const int mid = left + (right - left) / 2;
        const std::string value = normalizeLower(books[mid].title);

        if (value.find(q) != std::string::npos) {
            found = mid;
            right = mid - 1;
        } else if (value < q) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return found;
}

std::vector<Book> LibraryStorage::searchBooks(const std::string& query) const {
    const std::string q = normalizeLower(query);
    if (q.empty()) {
        return allBooks();
    }

    std::vector<Book> byTitle = allBooks();
    if (!byTitle.empty()) {
        quickSortBooks(byTitle, 0, static_cast<int>(byTitle.size()) - 1, SortField::Title, true);
    }

    std::vector<Book> result;
    std::set<int> usedIds;

    const int pos = binarySearchFirstTitle(byTitle, q);
    if (pos >= 0) {
        for (int i = pos; i < static_cast<int>(byTitle.size()); ++i) {
            if (normalizeLower(byTitle[i].title).find(q) != std::string::npos) {
                if (usedIds.insert(byTitle[i].id).second) {
                    result.push_back(byTitle[i]);
                }
            } else {
                break;
            }
        }
    } else {
        for (const auto& book : byTitle) {
            if (normalizeLower(book.title).find(q) != std::string::npos) {
                if (usedIds.insert(book.id).second) {
                    result.push_back(book);
                }
            }
        }
    }

    std::vector<Book> all = allBooks();
    for (const auto& book : all) {
        if (usedIds.count(book.id) == 0 && normalizeLower(book.author).find(q) != std::string::npos) {
            usedIds.insert(book.id);
            result.push_back(book);
        }
    }

    return result;
}

static int getSubgenreId(PGconn* conn, const std::string& genre, const std::string& subgenre) {
    if (genre.empty() || subgenre.empty()) {
        return 0;
    }

    std::string sql =
        "SELECT sg.id "
        "FROM subgenres sg "
        "JOIN genres g ON g.id = sg.genre_id "
        "WHERE g.name='" + sqlEscape(genre) + "' AND sg.name='" + sqlEscape(subgenre) + "' "
        "LIMIT 1;";

    PGresult* result = PQexec(conn, sql.c_str());
    int id = 0;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        id = std::atoi(PQgetvalue(result, 0, 0));
    }
    PQclear(result);
    return id;
}

bool LibraryStorage::upsertBook(Book& book) {
    if (!open()) {
        return false;
    }

    if (!ensureGenreHierarchy(book)) {
        return false;
    }

    PGconn* conn = static_cast<PGconn*>(db_);
    const int subgenreId = getSubgenreId(conn, trim(book.genre), trim(book.subgenre));

    std::ostringstream sql;
    if (book.id > 0) {
        sql << "UPDATE books SET "
            << "title='" << sqlEscape(book.title) << "', "
            << "author='" << sqlEscape(book.author) << "', "
            << "subgenre_id=" << (subgenreId > 0 ? std::to_string(subgenreId) : "NULL") << ", "
            << "publisher='" << sqlEscape(book.publisher) << "', "
            << "year=" << book.year << ", "
            << "format='" << sqlEscape(book.format) << "', "
            << "rating=" << book.rating << ", "
            << "price=" << book.price << ", "
            << "age_rating='" << sqlEscape(book.ageRating) << "', "
            << "isbn=" << (book.isbn.empty() ? "NULL" : "'" + sqlEscape(book.isbn) + "'") << ", "
            << "total_circulation=" << book.totalPrintRun << ", "
            << "print_sign_date='" << sqlEscape(book.signedToPrintDate) << "', "
            << "additional_prints='" << sqlEscape(book.additionalPrintDates.empty() ? "[]" : book.additionalPrintDates[0]) << "', "
            << "cover_image_path='" << sqlEscape(book.coverImagePath) << "', "
            << "license_image_path='" << sqlEscape(book.licenseImagePath) << "', "
            << "bibliographic_ref='" << sqlEscape(book.bibliographicReference) << "' "
            << "WHERE id=" << book.id << ";";
    } else {
        sql << "INSERT INTO books("
            << "title, author, subgenre_id, publisher, year, format, rating, price, age_rating, isbn, "
            << "total_circulation, print_sign_date, additional_prints, cover_image_path, license_image_path, bibliographic_ref"
            << ") VALUES ("
            << "'" << sqlEscape(book.title) << "', "
            << "'" << sqlEscape(book.author) << "', "
            << (subgenreId > 0 ? std::to_string(subgenreId) : "NULL") << ", "
            << "'" << sqlEscape(book.publisher) << "', "
            << book.year << ", "
            << "'" << sqlEscape(book.format) << "', "
            << book.rating << ", "
            << book.price << ", "
            << "'" << sqlEscape(book.ageRating) << "', "
            << (book.isbn.empty() ? "NULL" : "'" + sqlEscape(book.isbn) + "'") << ", "
            << book.totalPrintRun << ", "
            << "'" << sqlEscape(book.signedToPrintDate) << "', "
            << "'" << sqlEscape(book.additionalPrintDates.empty() ? "[]" : book.additionalPrintDates[0]) << "', "
            << "'" << sqlEscape(book.coverImagePath) << "', "
            << "'" << sqlEscape(book.licenseImagePath) << "', "
            << "'" << sqlEscape(book.bibliographicReference) << "'"
            << ") RETURNING id;";
    }

    PGresult* result = PQexec(conn, sql.str().c_str());
    const ExecStatusType status = PQresultStatus(result);

    bool ok = false;
    if (book.id > 0) {
        ok = status == PGRES_COMMAND_OK;
    } else {
        ok = status == PGRES_TUPLES_OK && PQntuples(result) > 0;
        if (ok) {
            book.id = std::atoi(PQgetvalue(result, 0, 0));
        }
    }

    if (!ok) {
        logMessage("ERROR", std::string("Upsert failed: ") + PQerrorMessage(conn));
    } else {
        logMessage("INFO", "Book id=" + std::to_string(book.id) + " added/updated");
    }

    PQclear(result);
    return ok;
}

bool LibraryStorage::removeBookById(int id) {
    if (!open()) {
        return false;
    }

    PGconn* conn = static_cast<PGconn*>(db_);

    std::string query =
        "SELECT cover_image_path, license_image_path FROM books WHERE id=" + std::to_string(id) + " LIMIT 1;";
    PGresult* result = PQexec(conn, query.c_str());

    std::string coverPath;
    std::string licensePath;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        if (!PQgetisnull(result, 0, 0)) coverPath = PQgetvalue(result, 0, 0);
        if (!PQgetisnull(result, 0, 1)) licensePath = PQgetvalue(result, 0, 1);
    }
    PQclear(result);

    std::string removeSql = "DELETE FROM books WHERE id=" + std::to_string(id) + ";";
    result = PQexec(conn, removeSql.c_str());
    const bool ok = PQresultStatus(result) == PGRES_COMMAND_OK;
    PQclear(result);

    if (ok) {
        deleteFileIfExists(coverPath);
        deleteFileIfExists(licensePath);
        logMessage("INFO", "Book id=" + std::to_string(id) + " removed");
    } else {
        logMessage("ERROR", "Book removal failed for id=" + std::to_string(id));
    }

    return ok;
}

LibraryBackendService::LibraryBackendService(LibraryStorage storage)
    : storage_(std::move(storage)) {}

bool LibraryBackendService::initialize() {
    if (!storage_.open()) {
        return false;
    }
    if (!storage_.ensureSchema()) {
        return false;
    }
    storage_.rotateBackupIfNeeded();
    return true;
}

static std::string makeGostReference(const Book& book) {
    std::ostringstream out;
    out << book.author;
    if (!book.author.empty() && !book.title.empty()) {
        out << " ";
    }
    out << book.title;
    if (book.year > 0) {
        out << ". — " << book.year;
    }
    if (!book.publisher.empty()) {
        out << ". — " << book.publisher;
    }
    if (!book.isbn.empty()) {
        out << ". — ISBN " << book.isbn;
    }
    return out.str();
}

bool LibraryBackendService::addOrUpdateBook(Book& book, bool /*fetchFromNetwork*/) {
    if (book.bibliographicReference.empty()) {
        book.bibliographicReference = makeGostReference(book);
    }
    return storage_.upsertBook(book);
}

bool LibraryBackendService::removeBookById(int id) {
    return storage_.removeBookById(id);
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
    std::vector<Book> books = storage_.sortedBooks(SortField::Isbn, true);
    std::vector<ObstNode> nodes;

    std::function<int(int, int)> build = [&](int left, int right) -> int {
        if (left > right) {
            return -1;
        }
        int mid = (left + right) / 2;
        ObstNode node;
        node.isbn = books[mid].isbn;
        node.bookId = books[mid].id;
        nodes.push_back(node);
        int index = static_cast<int>(nodes.size()) - 1;
        nodes[index].left = build(left, mid - 1);
        nodes[index].right = build(mid + 1, right);
        return index;
    };

    if (!books.empty()) {
        build(0, static_cast<int>(books.size()) - 1);
    }

    return nodes;
}

std::vector<OpenLibraryCandidate> LibraryBackendService::lookupOpenLibrary(const std::string& query, int limit) const {
    const std::string normalized = trim(query);
    if (normalized.empty()) {
        return {};
    }

    if (limit <= 0) {
        limit = 1;
    }
    if (limit > 50) {
        limit = 50;
    }

    const std::string url = "https://openlibrary.org/search.json?q=" + urlEncode(normalized) +
                            "&limit=" + std::to_string(limit) +
                            "&fields=title,author_name,first_publish_year,isbn,publisher,cover_i,language";
    const std::string command = "curl -fsSL --max-time 5 -H 'User-Agent: LibraryBackendCPP/1.0' " + shellEscape(url);
    const std::string response = readCommandOutput(command);
    if (response.empty()) {
        logMessage("WARNING", "OpenLibrary lookup failed for query=" + normalized);
        return {};
    }

    std::vector<OpenLibraryCandidate> results;
    const auto docs = extractDocObjects(response);
    results.reserve(docs.size());

    for (const auto& doc : docs) {
        OpenLibraryCandidate candidate;
        candidate.title = jsonStringField(doc, "title");
        candidate.author = jsonStringFromArray(doc, "author_name");
        candidate.publisher = jsonStringFromArray(doc, "publisher");
        candidate.language = jsonStringFromArray(doc, "language");
        candidate.isbn = jsonStringFromArray(doc, "isbn");
        candidate.year = jsonIntField(doc, "first_publish_year");

        const int coverId = jsonIntField(doc, "cover_i");
        if (coverId > 0) {
            candidate.coverUrl = "https://covers.openlibrary.org/b/id/" + std::to_string(coverId) + "-L.jpg";
        }

        if (!candidate.title.empty() || !candidate.author.empty()) {
            results.push_back(std::move(candidate));
        }
    }
    return results;
}

SortField LibraryBackendService::parseSortField(const std::string& value) {
    const std::string v = normalize(value);
    if (v == "title") return SortField::Title;
    if (v == "author") return SortField::Author;
    if (v == "genre") return SortField::Genre;
    if (v == "subgenre") return SortField::Subgenre;
    if (v == "publisher") return SortField::Publisher;
    if (v == "year") return SortField::Year;
    if (v == "format") return SortField::Format;
    if (v == "rating") return SortField::Rating;
    if (v == "price") return SortField::Price;
    if (v == "agerating") return SortField::AgeRating;
    if (v == "isbn") return SortField::Isbn;
    if (v == "totalprintrun") return SortField::TotalPrintRun;
    if (v == "signedtoprintdate") return SortField::SignedToPrintDate;
    return SortField::Title;
}

std::string LibraryBackendService::normalize(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (!std::isspace(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

std::string serializeBookList(const std::vector<Book>& books) {
    std::ostringstream out;
    for (const auto& book : books) {
        out << "BEGIN_BOOK\n";
        out << "id=" << book.id << "\n";
        out << "title=" << escapeValue(book.title) << "\n";
        out << "author=" << escapeValue(book.author) << "\n";
        out << "genre=" << escapeValue(book.genre) << "\n";
        out << "subgenre=" << escapeValue(book.subgenre) << "\n";
        out << "publisher=" << escapeValue(book.publisher) << "\n";
        out << "year=" << book.year << "\n";
        out << "format=" << escapeValue(book.format) << "\n";
        out << "rating=" << book.rating << "\n";
        out << "price=" << book.price << "\n";
        out << "age_rating=" << escapeValue(book.ageRating) << "\n";
        out << "isbn=" << escapeValue(book.isbn) << "\n";
        out << "total_circulation=" << book.totalPrintRun << "\n";
        out << "print_sign_date=" << escapeValue(book.signedToPrintDate) << "\n";
        out << "additional_prints=" << escapeValue(book.additionalPrintDates.empty() ? "[]" : book.additionalPrintDates[0]) << "\n";
        out << "cover_image_path=" << escapeValue(book.coverImagePath) << "\n";
        out << "license_image_path=" << escapeValue(book.licenseImagePath) << "\n";
        out << "bibliographic_ref=" << escapeValue(book.bibliographicReference) << "\n";
        out << "END_BOOK\n";
    }
    return out.str();
}

std::string serializeOpenLibraryCandidates(const std::vector<OpenLibraryCandidate>& candidates) {
    std::ostringstream out;
    for (const auto& item : candidates) {
        out << "BEGIN_CANDIDATE\n";
        out << "title=" << escapeValue(item.title) << "\n";
        out << "author=" << escapeValue(item.author) << "\n";
        out << "publisher=" << escapeValue(item.publisher) << "\n";
        out << "language=" << escapeValue(item.language) << "\n";
        out << "isbn=" << escapeValue(item.isbn) << "\n";
        out << "cover_url=" << escapeValue(item.coverUrl) << "\n";
        out << "year=" << item.year << "\n";
        out << "END_CANDIDATE\n";
    }
    return out.str();
}

std::optional<Book> parseBookFile(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return std::nullopt;
    }

    Book book;
    bool inBook = false;
    std::string line;

    while (std::getline(in, line)) {
        if (line == "BEGIN_BOOK") {
            inBook = true;
            continue;
        }
        if (line == "END_BOOK") {
            break;
        }
        if (!inBook) {
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, pos);
        const std::string value = unescapeValue(line.substr(pos + 1));

        if (key == "id") book.id = std::atoi(value.c_str());
        else if (key == "title") book.title = value;
        else if (key == "author") book.author = value;
        else if (key == "genre") book.genre = value;
        else if (key == "subgenre") book.subgenre = value;
        else if (key == "publisher") book.publisher = value;
        else if (key == "year") book.year = std::atoi(value.c_str());
        else if (key == "format") book.format = value;
        else if (key == "rating") book.rating = std::atof(value.c_str());
        else if (key == "price") book.price = std::atof(value.c_str());
        else if (key == "age_rating") book.ageRating = value;
        else if (key == "isbn") book.isbn = value;
        else if (key == "total_circulation") book.totalPrintRun = std::atoll(value.c_str());
        else if (key == "print_sign_date") book.signedToPrintDate = value;
        else if (key == "additional_prints") {
            book.additionalPrintDates.clear();
            book.additionalPrintDates.push_back(value);
        }
        else if (key == "cover_image_path") book.coverImagePath = value;
        else if (key == "license_image_path") book.licenseImagePath = value;
        else if (key == "bibliographic_ref") book.bibliographicReference = value;
    }

    if (trim(book.title).empty() || trim(book.author).empty()) {
        return std::nullopt;
    }

    return book;
}
