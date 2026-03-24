#include "library_backend.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <libpq-fe.h>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

void appendLog(const std::string& level, const std::string& message) {
    try {
        std::filesystem::create_directories("logs");
        std::ofstream out("logs/library.log", std::ios::app);
        if (!out) return;
        
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << " [" << level << "] " << message << "\n";
        out.flush();
    } catch (...) {}
}

std::string readCommandOutput(const std::string& command) {
    appendLog("DEBUG", "Executing command: " + command);
    
    std::array<char, 1024> buffer{};
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        appendLog("ERROR", "popen failed for command");
        return {};
    }
    
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int code = pclose(pipe);
    appendLog("DEBUG", "Command exit code: " + std::to_string(code));
    
    return result;
}

// ИСПРАВЛЕНО: Правильное экранирование для Windows PowerShell
std::string shellEscape(const std::string& value) {
    // Для PowerShell используем двойные кавычки с экранированием
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') {
            escaped += "\\\"";
        } else if (c == '\\') {
            escaped += "\\\\";
        } else if (c == '$') {
            escaped += "`$";  // PowerShell escape
        } else if (c == '`') {
            escaped += "``";  // PowerShell escape
        } else {
            escaped.push_back(c);
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string jsonStringField(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const auto pos = json.find(key);
    if (pos == std::string::npos) return {};
    
    const auto colon = json.find(':', pos + key.size());
    if (colon == std::string::npos) return {};
    
    const auto firstQuote = json.find('"', colon + 1);
    if (firstQuote == std::string::npos) return {};
    
    std::string value;
    bool escaped = false;
    for (std::size_t i = firstQuote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) { value.push_back(c); escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') return value;
        value.push_back(c);
    }
    return {};
}

int jsonIntegerField(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const auto pos = json.find(key);
    if (pos == std::string::npos) return 0;
    
    const auto colon = json.find(':', pos + key.size());
    if (colon == std::string::npos) return 0;
    
    std::size_t idx = colon + 1;
    while (idx < json.size() && std::isspace(static_cast<unsigned char>(json[idx]))) ++idx;
    
    std::size_t end = idx;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) ++end;
    
    return std::atoi(json.substr(idx, end - idx).c_str());
}

bool isNetworkEnabled() {
    const char* offline = std::getenv("OFFLINE_MODE");
    const char* net = std::getenv("LIBRARY_ENABLE_NET");
    
    if (offline && (std::string(offline) == "1" || std::string(offline) == "true")) return false;
    if (net && (std::string(net) == "0" || std::string(net) == "false")) return false;
    
    return true;
}

std::string defaultPgConn() {
    const char* conn = std::getenv("LIBRARY_PG_CONN");
    if (conn != nullptr && std::string(conn).size() > 0) return conn;
    return "host=localhost port=5432 dbname=library user=postgres password=123";
}

bool pgOk(PGresult* result) {
    const auto status = PQresultStatus(result);
    return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
}

void ignorePgNotice(void*, const char*) {}

std::string nullToEmpty(const char* value) {
    return value == nullptr ? std::string{} : std::string(value);
}

Book rowToBook(PGresult* result, int row) {
    Book book;
    auto val = [&](int col) -> std::string {
        if (PQgetisnull(result, row, col) != 0) return {};
        return nullToEmpty(PQgetvalue(result, row, col));
    };
    
    book.id = std::atoi(val(0).c_str());
    book.title = val(1);
    book.author = val(2);
    book.genre = val(3);
    book.subgenre = val(4);
    book.publisher = val(5);
    book.year = std::atoi(val(6).c_str());
    book.format = val(7);
    book.rating = std::atof(val(8).c_str());
    book.price = std::atof(val(9).c_str());
    book.ageRating = val(10);
    book.isbn = val(11);
    book.totalPrintRun = std::atoll(val(12).c_str());
    book.signedToPrintDate = val(13);
    
    std::string dates = val(14);
    if (!dates.empty()) {
        std::stringstream ss(dates);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) book.additionalPrintDates.push_back(item);
        }
    }
    
    book.coverImagePath = val(15);
    book.licenseImagePath = val(16);
    book.bibliographicReference = val(17);
    book.coverUrl = val(18);
    book.searchFrequency = std::atof(val(19).c_str());
    
    return book;
}

std::string baseSelectSql() {
    return R"SQL(
SELECT 
    books.id,
    books.title,
    books.author,
    COALESCE(genres.name, '') AS genre_name,
    COALESCE(subgenres.name, '') AS subgenre_name,
    COALESCE(books.publisher, ''),
    COALESCE(books.year, 0),
    COALESCE(books.format, ''),
    COALESCE(books.rating, 0),
    COALESCE(books.price, 0),
    COALESCE(books.age_rating, ''),
    COALESCE(books.isbn, ''),
    COALESCE(books.total_print_run, 0),
    COALESCE(books.signed_to_print_date, ''),
    COALESCE(books.additional_print_dates, ''),
    COALESCE(books.cover_image_path, ''),
    COALESCE(books.license_image_path, ''),
    COALESCE(books.bibliographic_reference, ''),
    COALESCE(books.cover_url, ''),
    COALESCE(books.search_frequency, 1)
FROM books
LEFT JOIN subgenres ON subgenres.id = books.subgenre_id
LEFT JOIN genres ON genres.id = subgenres.genre_id
)SQL";
}

bool runExec(PGconn* conn, const std::string& sql) {
    PGresult* result = PQexec(conn, sql.c_str());
    const bool ok = pgOk(result);
    if (!ok) {
        appendLog("ERROR", "SQL failed: " + sql + " | Error: " + std::string(PQerrorMessage(conn)));
    }
    PQclear(result);
    return ok;
}

std::vector<Book> runSelectBooks(PGconn* conn, const std::string& sql, const std::vector<std::string>& params = {}) {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params) values.push_back(p.c_str());
    
    PGresult* result = PQexecParams(conn, sql.c_str(), 
        static_cast<int>(values.size()), nullptr,
        values.empty() ? nullptr : values.data(), nullptr, nullptr, 0);
    
    std::vector<Book> books;
    if (!pgOk(result)) { 
        appendLog("ERROR", "Select failed: " + std::string(PQerrorMessage(conn)));
        PQclear(result); 
        return books; 
    }
    
    const int rows = PQntuples(result);
    books.reserve(rows);
    for (int i = 0; i < rows; ++i) books.push_back(rowToBook(result, i));
    
    PQclear(result);
    return books;
}

int fetchOrCreateGenreId(PGconn* conn, const std::string& genre) {
    if (trim(genre).empty()) {
        appendLog("DEBUG", "Empty genre, returning 0");
        return 0;
    }
    
    appendLog("DEBUG", "Creating/fetching genre: " + genre);
    
    const char* values[] = {genre.c_str()};
    PGresult* result = PQexecParams(conn,
        "INSERT INTO genres(name) VALUES($1) ON CONFLICT(name) DO UPDATE SET name=EXCLUDED.name RETURNING id;",
        1, nullptr, values, nullptr, nullptr, 0);
    
    if (!pgOk(result) || PQntuples(result) == 0) { 
        appendLog("ERROR", "Genre insert failed: " + std::string(PQerrorMessage(conn)));
        PQclear(result); 
        return 0; 
    }
    
    const int id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    appendLog("DEBUG", "Genre ID: " + std::to_string(id));
    return id;
}

int fetchOrCreateSubgenreId(PGconn* conn, int genreId, const std::string& subgenre) {
    if (genreId <= 0 || trim(subgenre).empty()) {
        appendLog("DEBUG", "Invalid genreId or empty subgenre");
        return 0;
    }
    
    appendLog("DEBUG", "Creating/fetching subgenre: " + subgenre + " (genre_id=" + std::to_string(genreId) + ")");
    
    const std::string g = std::to_string(genreId);
    const char* values[] = {g.c_str(), subgenre.c_str()};
    PGresult* result = PQexecParams(conn,
        "INSERT INTO subgenres(genre_id,name) VALUES($1::int,$2) ON CONFLICT(genre_id,name) DO UPDATE SET name=EXCLUDED.name RETURNING id;",
        2, nullptr, values, nullptr, nullptr, 0);
    
    if (!pgOk(result) || PQntuples(result) == 0) { 
        appendLog("ERROR", "Subgenre insert failed: " + std::string(PQerrorMessage(conn)));
        PQclear(result); 
        return 0; 
    }
    
    const int id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    appendLog("DEBUG", "Subgenre ID: " + std::to_string(id));
    return id;
}

std::vector<Book> demoBooks() {
    std::vector<Book> books;
    
    auto makeBook = [&](const char* title, const char* author, const char* genre, 
                        const char* subgenre, const char* publisher, int year,
                        double rating, double price, const char* isbn) {
        Book book;
        book.title = title;
        book.author = author;
        book.genre = genre;
        book.subgenre = subgenre;
        book.publisher = publisher;
        book.year = year;
        book.rating = rating;
        book.price = price;
        book.isbn = isbn;
        book.searchFrequency = rating;
        book.ageRating = "12+";
        book.format = "70x100/16";
        return book;
    };
    
    books.push_back(makeBook("1984", "George Orwell", "Fiction", "Novel", 
                             "AST", 1949, 4.7, 420.0, "978-5-17-108831-3"));
    
    books.push_back(makeBook("Clean Code", "Robert Martin", "Technical", "Programming",
                             "Piter", 2008, 4.6, 1200.0, "978-5-4461-0960-9"));
    
    return books;
}

} // namespace

// LibraryStorage implementation
LibraryStorage::LibraryStorage(std::string connectionString)
    : connectionString_(connectionString.empty() ? defaultPgConn() : std::move(connectionString)) {}

LibraryStorage::~LibraryStorage() {
    if (db_ != nullptr) PQfinish(static_cast<PGconn*>(db_));
}

bool LibraryStorage::open() {
    if (db_ != nullptr) return true;
    
    PGconn* conn = PQconnectdb(connectionString_.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        appendLog("ERROR", "PostgreSQL connect failed: " + std::string(PQerrorMessage(conn)));
        PQfinish(conn);
        return false;
    }
    
    PQsetNoticeProcessor(conn, ignorePgNotice, nullptr);
    db_ = conn;
    appendLog("INFO", "PostgreSQL connection established.");
    return ensureSchema();
}

bool LibraryStorage::ensureSchema() {
    return execute(R"SQL(
CREATE TABLE IF NOT EXISTS genres (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS subgenres (
    id SERIAL PRIMARY KEY,
    genre_id INTEGER NOT NULL REFERENCES genres(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    UNIQUE(genre_id, name)
);

CREATE TABLE IF NOT EXISTS books (
    id SERIAL PRIMARY KEY,
    subgenre_id INTEGER REFERENCES subgenres(id) ON DELETE SET NULL,
    title TEXT NOT NULL,
    author TEXT NOT NULL,
    publisher TEXT,
    year INTEGER NOT NULL DEFAULT 0,
    format TEXT,
    rating REAL NOT NULL DEFAULT 0,
    price REAL NOT NULL DEFAULT 0,
    age_rating TEXT,
    isbn TEXT UNIQUE,
    total_print_run BIGINT NOT NULL DEFAULT 0,
    signed_to_print_date TEXT,
    additional_print_dates TEXT,
    cover_image_path TEXT,
    license_image_path TEXT,
    bibliographic_reference TEXT,
    cover_url TEXT,
    search_frequency REAL NOT NULL DEFAULT 1,
    created_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_books_title ON books(title);
CREATE INDEX IF NOT EXISTS idx_books_author ON books(author);
CREATE INDEX IF NOT EXISTS idx_books_isbn ON books(isbn);
CREATE INDEX IF NOT EXISTS idx_books_subgenre_id ON books(subgenre_id);
)SQL");
}

bool LibraryStorage::execute(const std::string& sql) const {
    return runExec(static_cast<PGconn*>(db_), sql);
}

bool LibraryStorage::ensureGenreHierarchy(const Book& book) const {
    PGconn* conn = static_cast<PGconn*>(db_);
    if (trim(book.genre).empty() || trim(book.subgenre).empty()) {
        appendLog("DEBUG", "Empty genre/subgenre, skipping hierarchy");
        return true;
    }
    
    const int genreId = fetchOrCreateGenreId(conn, book.genre);
    if (genreId <= 0) return false;
    
    const int subgenreId = fetchOrCreateSubgenreId(conn, genreId, book.subgenre);
    return subgenreId > 0;
}

Book LibraryStorage::readBookFromStatement(void*) { return Book{}; }

std::vector<Book> LibraryStorage::allBooks() const {
    return runSelectBooks(static_cast<PGconn*>(db_), 
        baseSelectSql() + " ORDER BY lower(books.title), lower(books.author), books.id;");
}

std::vector<Book> LibraryStorage::searchBooks(const std::string& query) const {
    PGconn* conn = static_cast<PGconn*>(db_);
    const std::string q = "%" + query + "%";
    
    auto byTitle = runSelectBooks(conn,
        baseSelectSql() + " WHERE lower(books.title) LIKE lower($1) ORDER BY lower(books.title), books.id;",
        {q});
    
    if (!byTitle.empty()) return byTitle;
    
    return runSelectBooks(conn,
        baseSelectSql() + " WHERE lower(books.author) LIKE lower($1) ORDER BY lower(books.author), books.id;",
        {q});
}

std::vector<Book> LibraryStorage::sortedBooks(SortField field, bool ascending) const {
    std::string column;
    switch (field) {
        case SortField::Title: column = "lower(books.title)"; break;
        case SortField::Author: column = "lower(books.author)"; break;
        case SortField::Year: column = "books.year"; break;
        case SortField::Rating: column = "books.rating"; break;
        case SortField::Price: column = "books.price"; break;
        default: column = "lower(books.title)";
    }
    
    return runSelectBooks(static_cast<PGconn*>(db_),
        baseSelectSql() + " ORDER BY " + column + (ascending ? " ASC" : " DESC") + ", books.id;");
}

bool LibraryStorage::upsertBook(Book& book) {
    PGconn* conn = static_cast<PGconn*>(db_);
    
    appendLog("DEBUG", "=== Starting upsert ===");
    appendLog("DEBUG", "Book title: " + book.title);
    appendLog("DEBUG", "Book author: " + book.author);
    appendLog("DEBUG", "Book genre: " + book.genre);
    appendLog("DEBUG", "Book subgenre: " + book.subgenre);
    
    if (!runExec(conn, "BEGIN;")) {
        appendLog("ERROR", "BEGIN transaction failed");
        return false;
    }
    
    const int genreId = fetchOrCreateGenreId(conn, book.genre);
    appendLog("DEBUG", "Genre ID result: " + std::to_string(genreId));
    
    const int subgenreId = fetchOrCreateSubgenreId(conn, genreId, book.subgenre);
    appendLog("DEBUG", "Subgenre ID result: " + std::to_string(subgenreId));
    
    const std::string id = book.id > 0 ? std::to_string(book.id) : "";
    const std::string sid = subgenreId > 0 ? std::to_string(subgenreId) : "";
    
    const std::string additional = [&]() {
        std::string result;
        for (std::size_t i = 0; i < book.additionalPrintDates.size(); ++i) {
            if (i > 0) result += "|";
            result += book.additionalPrintDates[i];
        }
        return result;
    }();
    
    const std::string year = std::to_string(book.year);
    const std::string rating = std::to_string(book.rating);
    const std::string price = std::to_string(book.price);
    const std::string totalPrintRun = std::to_string(book.totalPrintRun);
    const std::string searchFrequency = std::to_string(book.searchFrequency);

    const char* values[] = {
        id.c_str(), sid.c_str(), book.title.c_str(), book.author.c_str(), book.publisher.c_str(),
        year.c_str(), book.format.c_str(),
        rating.c_str(), price.c_str(),
        book.ageRating.c_str(), book.isbn.c_str(),
        totalPrintRun.c_str(), book.signedToPrintDate.c_str(),
        additional.c_str(), book.coverImagePath.c_str(), book.licenseImagePath.c_str(),
        book.bibliographicReference.c_str(), book.coverUrl.c_str(),
        searchFrequency.c_str()
    };
    
    appendLog("DEBUG", "Executing INSERT/UPDATE query");
    
    PGresult* result = PQexecParams(conn, R"SQL(
INSERT INTO books(id, subgenre_id, title, author, publisher, year, format, rating, price,
    age_rating, isbn, total_print_run, signed_to_print_date, additional_print_dates,
    cover_image_path, license_image_path, bibliographic_reference, cover_url, search_frequency)
VALUES (NULLIF($1,'')::int, NULLIF($2,'')::int, $3,$4,$5, $6::int,$7,$8::real,$9::real,
    $10,$11, $12::bigint,$13,$14, $15,$16,$17,$18,$19::real)
ON CONFLICT(id) DO UPDATE SET
    subgenre_id = EXCLUDED.subgenre_id, title = EXCLUDED.title, author = EXCLUDED.author,
    publisher = EXCLUDED.publisher, year = EXCLUDED.year, format = EXCLUDED.format,
    rating = EXCLUDED.rating, price = EXCLUDED.price, age_rating = EXCLUDED.age_rating,
    isbn = EXCLUDED.isbn, total_print_run = EXCLUDED.total_print_run,
    signed_to_print_date = EXCLUDED.signed_to_print_date,
    additional_print_dates = EXCLUDED.additional_print_dates,
    cover_image_path = EXCLUDED.cover_image_path,
    license_image_path = EXCLUDED.license_image_path,
    bibliographic_reference = EXCLUDED.bibliographic_reference,
    cover_url = EXCLUDED.cover_url, search_frequency = EXCLUDED.search_frequency
RETURNING id;
)SQL", 19, nullptr, values, nullptr, nullptr, 0);
    
    if (!pgOk(result)) {
        appendLog("ERROR", "Book upsert SQL failed: " + std::string(PQerrorMessage(conn)));
        PQclear(result);
        runExec(conn, "ROLLBACK;");
        return false;
    }
    
    if (PQntuples(result) == 0) {
        appendLog("ERROR", "Book upsert returned no rows");
        PQclear(result);
        runExec(conn, "ROLLBACK;");
        return false;
    }
    
    book.id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    
    if (!runExec(conn, "COMMIT;")) {
        appendLog("ERROR", "COMMIT failed");
        return false;
    }
    
    appendLog("DEBUG", "=== Upsert complete, book ID: " + std::to_string(book.id) + " ===");
    return true;
}

bool LibraryStorage::removeBookById(int id) {
    PGconn* conn = static_cast<PGconn*>(db_);
    const std::string idText = std::to_string(id);
    const char* values[] = {idText.c_str()};
    
    PGresult* result = PQexecParams(conn, "DELETE FROM books WHERE id = $1::int;",
        1, nullptr, values, nullptr, nullptr, 0);
    
    if (!pgOk(result)) { PQclear(result); return false; }
    
    const int affected = std::atoi(PQcmdTuples(result));
    PQclear(result);
    return affected > 0;
}

bool LibraryStorage::isEmpty() const {
    PGconn* conn = static_cast<PGconn*>(db_);
    PGresult* result = PQexec(conn, "SELECT COUNT(*) FROM books;");
    if (!pgOk(result) || PQntuples(result) == 0) { PQclear(result); return true; }
    
    const bool empty = std::atoi(PQgetvalue(result, 0, 0)) == 0;
    PQclear(result);
    return empty;
}

// NetworkMetadataClient - OpenLibrary API (ИСПРАВЛЕНО для Windows)
std::optional<Book> NetworkMetadataClient::fetchByQuery(const Book& draft) const {
    if (!isNetworkEnabled()) {
        appendLog("INFO", "Network disabled, skipping API fetch");
        return std::nullopt;
    }
    
    std::string query = draft.isbn;
    if (query.empty()) {
        query = draft.title;
        if (!draft.author.empty()) query += " " + draft.author;
    }
    
    query = trim(query);
    if (query.empty()) {
        appendLog("WARN", "Empty query for API");
        return std::nullopt;
    }
    
    // URL-encoding - БЕЗ ПРОБЕЛОВ в fields!
    std::string encoded;
    for (char c : query) {
        if (c == ' ') encoded += "+";
        else if (c == '"' || c == '<' || c == '>' || c == '&' || c == '#') {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            encoded += hex;
        }
        else encoded += c;
    }
    
    // ИСПРАВЛЕНО: Нет пробелов в fields parameter!
    const std::string url = "https://openlibrary.org/search.json?q=" + encoded + 
        "&limit=1&fields=title,author_name,first_publish_year,isbn,publisher,cover_i";
    
    appendLog("INFO", "API URL: " + url);
    
    // Retry logic (3 attempts)
    for (int attempt = 1; attempt <= 3; ++attempt) {
        // ИСПРАВЛЕНО: Правильная команда curl для Windows PowerShell
        // Используем двойные кавычки и правильный формат
        const std::string command = "curl -sS --max-time 5 "
            "-H \"User-Agent: LibraryBackendCPP/1.0\" "
            "\"" + url + "\" "
            "-w \"\\n__HTTP_CODE__:%{http_code}\\n\"";
        
        appendLog("INFO", "Executing curl (attempt " + std::to_string(attempt) + ")");
        
        const std::string raw = readCommandOutput(command);
        appendLog("INFO", "Curl response length: " + std::to_string(raw.size()));
        
        if (raw.empty()) {
            if (attempt < 3) {
                appendLog("WARN", "API request failed, retry " + std::to_string(attempt));
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            continue;
        }
        
        // Parse HTTP code
        const auto markerPos = raw.rfind("__HTTP_CODE__:");
        std::string response;
        int httpCode = 0;
        
        if (markerPos == std::string::npos) {
            response = raw;
        } else {
            response = raw.substr(0, markerPos);
            httpCode = std::atoi(trim(raw.substr(markerPos + 15)).c_str());
        }
        
        appendLog("INFO", "HTTP Code: " + std::to_string(httpCode));
        
        if (httpCode == 200) {
            Book remote;
            remote.title = jsonStringField(response, "title");
            remote.author = jsonStringField(response, "author_name");
            remote.publisher = jsonStringField(response, "publisher");
            remote.year = jsonIntegerField(response, "first_publish_year");
            
            const std::string isbn = jsonStringField(response, "isbn");
            if (!isbn.empty()) remote.isbn = isbn;
            
            const int coverId = jsonIntegerField(response, "cover_i");
            if (coverId > 0) {
                remote.coverUrl = "https://covers.openlibrary.org/b/id/" + 
                    std::to_string(coverId) + "-L.jpg";
            }
            
            if (!remote.title.empty() || !remote.author.empty() || !remote.isbn.empty()) {
                appendLog("INFO", "API fetch successful: " + remote.title);
                return remote;
            }
        }
        
        if (httpCode == 404 || httpCode == 400) {
            appendLog("WARN", "Book not found in API");
            return std::nullopt;
        }
        
        if (attempt < 3) {
            appendLog("WARN", "Retrying...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    appendLog("WARN", "API unavailable after 3 attempts, using manual data");
    return std::nullopt;
}

// LibraryBackendService implementation
LibraryBackendService::LibraryBackendService(LibraryStorage storage)
    : storage_(std::move(storage)) {}

bool LibraryBackendService::initialize() {
    appendLog("INFO", "=== Starting initialization ===");
    
    if (!storage_.open()) {
        appendLog("ERROR", "Storage open failed");
        return false;
    }
    
    appendLog("INFO", "Storage opened successfully");
    appendLog("INFO", "=== Initialization complete ===");
    
    return true;
}

bool LibraryBackendService::addOrUpdateBook(Book& book, bool fetchFromNetwork) {
    appendLog("INFO", "=== addOrUpdateBook called ===");
    appendLog("INFO", "fetchFromNetwork: " + std::string(fetchFromNetwork ? "true" : "false"));
    appendLog("INFO", "Book title: " + book.title);
    appendLog("INFO", "Book author: " + book.author);
    appendLog("INFO", "Book genre: " + book.genre);
    appendLog("INFO", "Book subgenre: " + book.subgenre);
    
    if (fetchFromNetwork && isNetworkEnabled()) {
        appendLog("INFO", "Fetching metadata from OpenLibrary API...");
        const auto remote = networkClient_.fetchByQuery(book);
        if (remote.has_value()) {
            if (book.title.empty()) book.title = remote->title;
            if (book.author.empty()) book.author = remote->author;
            if (book.publisher.empty()) book.publisher = remote->publisher;
            if (book.year == 0) book.year = remote->year;
            if (book.isbn.empty()) book.isbn = remote->isbn;
            if (book.coverUrl.empty()) book.coverUrl = remote->coverUrl;
            appendLog("INFO", "API data merged successfully");
        } else {
            appendLog("WARN", "API fetch failed, using manual data");
        }
    }
    
    if (trim(book.title).empty() || trim(book.author).empty()) {
        appendLog("ERROR", "Empty title or author");
        return false;
    }
    
    bool result = storage_.upsertBook(book);
    appendLog("INFO", "Book upsert result: " + std::string(result ? "SUCCESS" : "FAILED"));
    return result;
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
    auto books = storage_.allBooks();
    std::sort(books.begin(), books.end(), 
        [](const Book& lhs, const Book& rhs) { return lhs.isbn < rhs.isbn; });
    
    const int n = static_cast<int>(books.size());
    if (n == 0) return {};
    
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
    std::function<int(int, int)> build = [&](int left, int right) -> int {
        if (left > right) return -1;
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
    std::string key = value;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    
    static const std::map<std::string, SortField> mapping = {
        {"title", SortField::Title}, {"author", SortField::Author},
        {"year", SortField::Year}, {"rating", SortField::Rating},
        {"price", SortField::Price}, {"genre", SortField::Genre},
        {"isbn", SortField::Isbn}
    };
    
    const auto it = mapping.find(key);
    return it == mapping.end() ? SortField::Title : it->second;
}

std::string LibraryBackendService::normalize(const std::string& value) {
    std::string result;
    for (unsigned char c : value) {
        if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
    }
    return result;
}

// Serialization functions
std::string escapeText(const std::string& value) {
    std::string escaped;
    for (char c : value) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '=': escaped += "\\="; break;
            default: escaped.push_back(c);
        }
    }
    return escaped;
}

std::string unescapeText(const std::string& value) {
    std::string result;
    bool escaped = false;
    for (char c : value) {
        if (!escaped) {
            if (c == '\\') { escaped = true; continue; }
            result.push_back(c);
            continue;
        }
        switch (c) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            default: result.push_back(c);
        }
        escaped = false;
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
        
        std::string dates;
        for (std::size_t i = 0; i < book.additionalPrintDates.size(); ++i) {
            if (i > 0) dates += "|";
            dates += book.additionalPrintDates[i];
        }
        out << "additional_print_dates=" << escapeText(dates) << "\n";
        
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
    if (!input.good()) return std::nullopt;
    
    Book book;
    std::string line;
    
    while (std::getline(input, line)) {
        if (line.empty() || line == "BEGIN_BOOK" || line == "END_BOOK") continue;
        
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        const std::string key = trim(line.substr(0, pos));
        const std::string value = unescapeText(line.substr(pos + 1));
        
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
        else if (key == "total_print_run") book.totalPrintRun = std::atoll(value.c_str());
        else if (key == "signed_to_print_date") book.signedToPrintDate = value;
        else if (key == "additional_print_dates") {
            std::stringstream ss(value);
            std::string item;
            while (std::getline(ss, item, '|')) {
                if (!item.empty()) book.additionalPrintDates.push_back(item);
            }
        }
        else if (key == "cover_image_path") book.coverImagePath = value;
        else if (key == "license_image_path") book.licenseImagePath = value;
        else if (key == "bibliographic_reference") book.bibliographicReference = value;
        else if (key == "cover_url") book.coverUrl = value;
        else if (key == "search_frequency") book.searchFrequency = std::atof(value.c_str());
    }
    
    if (trim(book.title).empty() && trim(book.author).empty()) return std::nullopt;
    return book;
}
