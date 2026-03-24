#include "library_backend.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <libpq-fe.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace {
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
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '=': escaped += "\\="; break;
        default: escaped.push_back(c); break;
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
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        default: result.push_back(c); break;
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

bool pgOk(PGresult* result, std::initializer_list<ExecStatusType> okStatuses) {
    const auto status = PQresultStatus(result);
    for (auto expected : okStatuses) {
        if (status == expected) {
            return true;
        }
    }
    return false;
}

std::string nullToEmpty(const char* value) {
    return value == nullptr ? std::string{} : std::string(value);
}

std::filesystem::path dataRootPath() {
    const char* fromEnv = std::getenv("LIBRARY_DATA_PATH");
    if (fromEnv != nullptr && std::string(fromEnv).size() > 0) {
        return std::filesystem::path(fromEnv);
    }
    return std::filesystem::current_path();
}

std::filesystem::path libraryDbMarkerPath() {
    return dataRootPath() / "library.db";
}

std::filesystem::path libraryLogPath() {
    return dataRootPath() / "library.log";
}

void appendLog(const std::string& level, const std::string& message) {
    try {
        std::filesystem::create_directories(dataRootPath());
        std::ofstream out(libraryLogPath(), std::ios::app);
        if (!out) {
            return;
        }
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << " [" << level << "] "
            << message << "\n";
    } catch (...) {
    }
}

void ensureLocalArtifacts() {
    try {
        const auto root = dataRootPath();
        std::filesystem::create_directories(root);
        std::filesystem::create_directories(root / "images");

        const auto marker = libraryDbMarkerPath();
        if (!std::filesystem::exists(marker)) {
            std::ofstream out(marker, std::ios::out);
            if (out) {
                out << "Library metadata marker.\n";
                out << "Storage engine: PostgreSQL\n";
                out << "Connection source: LIBRARY_PG_CONN\n";
            }
        }

        const auto backup = root / "library.db.backup";
        std::error_code ec;
        std::filesystem::copy_file(marker, backup, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            appendLog("WARN", "Could not refresh library.db.backup: " + ec.message());
        }
    } catch (...) {
    }
}

Book rowToBook(PGresult* result, int row) {
    Book book;
    auto val = [&](int col) -> std::string {
        if (PQgetisnull(result, row, col) != 0) {
            return {};
        }
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
    book.additionalPrintDates = split(val(14), '|');
    book.coverImagePath = val(15);
    book.licenseImagePath = val(16);
    book.bibliographicReference = val(17);
    book.coverUrl = val(18);
    book.searchFrequency = std::atof(val(19).c_str());
    return book;
}

std::string sqlSortColumn(SortField field) {
    switch (field) {
    case SortField::Title: return "lower(books.title)";
    case SortField::Author: return "lower(books.author)";
    case SortField::Genre: return "lower(genres.name)";
    case SortField::Subgenre: return "lower(subgenres.name)";
    case SortField::Publisher: return "lower(books.publisher)";
    case SortField::Year: return "books.year";
    case SortField::Format: return "lower(books.format)";
    case SortField::Rating: return "books.rating";
    case SortField::Price: return "books.price";
    case SortField::AgeRating: return "lower(books.age_rating)";
    case SortField::Isbn: return "lower(books.isbn)";
    case SortField::TotalPrintRun: return "books.total_print_run";
    case SortField::SignedToPrintDate: return "books.signed_to_print_date";
    }
    return "books.id";
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
    const bool ok = pgOk(result, {PGRES_COMMAND_OK, PGRES_TUPLES_OK});
    PQclear(result);
    return ok;
}

std::vector<Book> runSelectBooks(PGconn* conn, const std::string& sql, const std::vector<std::string>& params = {}) {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params) {
        values.push_back(p.c_str());
    }

    PGresult* result = PQexecParams(
        conn,
        sql.c_str(),
        static_cast<int>(values.size()),
        nullptr,
        values.empty() ? nullptr : values.data(),
        nullptr,
        nullptr,
        0
    );

    std::vector<Book> books;
    if (!pgOk(result, {PGRES_TUPLES_OK})) {
        PQclear(result);
        return books;
    }

    const int rows = PQntuples(result);
    books.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        books.push_back(rowToBook(result, i));
    }
    PQclear(result);
    return books;
}

int fetchOrCreateGenreId(PGconn* conn, const std::string& genre) {
    if (trim(genre).empty()) {
        return 0;
    }
    const char* values[] = {genre.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO genres(name) VALUES($1) ON CONFLICT(name) DO UPDATE SET name=EXCLUDED.name RETURNING id;",
        1,
        nullptr,
        values,
        nullptr,
        nullptr,
        0
    );
    if (!pgOk(result, {PGRES_TUPLES_OK}) || PQntuples(result) == 0) {
        PQclear(result);
        return 0;
    }
    const int id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    return id;
}

int fetchOrCreateSubgenreId(PGconn* conn, int genreId, const std::string& subgenre) {
    if (genreId <= 0 || trim(subgenre).empty()) {
        return 0;
    }
    const std::string g = std::to_string(genreId);
    const char* values[] = {g.c_str(), subgenre.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO subgenres(genre_id,name) VALUES($1::int,$2) ON CONFLICT(genre_id,name) DO UPDATE SET name=EXCLUDED.name RETURNING id;",
        2,
        nullptr,
        values,
        nullptr,
        nullptr,
        0
    );
    if (!pgOk(result, {PGRES_TUPLES_OK}) || PQntuples(result) == 0) {
        PQclear(result);
        return 0;
    }
    const int id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    return id;
}

std::string defaultPgConn() {
    const char* fromEnv = std::getenv("LIBRARY_PG_CONN");
    if (fromEnv != nullptr && std::string(fromEnv).size() > 0) {
        return fromEnv;
    }
    return "host=localhost port=5432 dbname=library user=postgres password=postgres";
}
} // namespace

LibraryStorage::LibraryStorage(std::string connectionString)
    : connectionString_(connectionString.empty() ? defaultPgConn() : std::move(connectionString)) {}

LibraryStorage::~LibraryStorage() {
    if (db_ != nullptr) {
        PQfinish(static_cast<PGconn*>(db_));
    }
}

bool LibraryStorage::open() {
    ensureLocalArtifacts();
    if (db_ != nullptr) {
        return true;
    }
    PGconn* conn = PQconnectdb(connectionString_.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        appendLog("ERROR", "PostgreSQL connect failed: " + std::string(PQerrorMessage(conn)));
        PQfinish(conn);
        return false;
    }
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
            age_rating TEXT CHECK(age_rating IN ('0+','6+','12+','16+','18+') OR age_rating = ''),
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
        CREATE INDEX IF NOT EXISTS idx_books_subgenre_id ON books(subgenre_id);
    )SQL");
}

bool LibraryStorage::execute(const std::string& sql) const {
    return runExec(static_cast<PGconn*>(db_), sql);
}

bool LibraryStorage::ensureGenreHierarchy(const Book& book) const {
    PGconn* conn = static_cast<PGconn*>(db_);
    if (trim(book.genre).empty() || trim(book.subgenre).empty()) {
        return true;
    }
    const int genreId = fetchOrCreateGenreId(conn, book.genre);
    if (genreId <= 0) {
        return false;
    }
    const int subgenreId = fetchOrCreateSubgenreId(conn, genreId, book.subgenre);
    return subgenreId > 0;
}

Book LibraryStorage::readBookFromStatement(void* /*statement*/) {
    return Book{};
}

std::vector<Book> LibraryStorage::allBooks() const {
    return runSelectBooks(static_cast<PGconn*>(db_), baseSelectSql() + " ORDER BY lower(books.title), lower(books.author), books.id;");
}

std::vector<Book> LibraryStorage::searchBooks(const std::string& query) const {
    PGconn* conn = static_cast<PGconn*>(db_);
    const std::string q = "%" + query + "%";
    auto byTitle = runSelectBooks(
        conn,
        baseSelectSql() + " WHERE lower(books.title) LIKE lower($1) ORDER BY lower(books.title), lower(books.author), books.id;",
        {q}
    );
    if (!byTitle.empty()) {
        return byTitle;
    }
    return runSelectBooks(
        conn,
        baseSelectSql() + " WHERE lower(books.author) LIKE lower($1) ORDER BY lower(books.author), lower(books.title), books.id;",
        {q}
    );
}

std::vector<Book> LibraryStorage::sortedBooks(SortField field, bool ascending) const {
    return runSelectBooks(
        static_cast<PGconn*>(db_),
        baseSelectSql() + " ORDER BY " + sqlSortColumn(field) + (ascending ? " ASC" : " DESC") + ", lower(books.title), books.id;"
    );
}

bool LibraryStorage::upsertBook(Book& book) {
    PGconn* conn = static_cast<PGconn*>(db_);
    if (!runExec(conn, "BEGIN;")) {
        return false;
    }

    const int genreId = fetchOrCreateGenreId(conn, book.genre);
    const int subgenreId = fetchOrCreateSubgenreId(conn, genreId, book.subgenre);

    const std::string id = book.id > 0 ? std::to_string(book.id) : "";
    const std::string sid = subgenreId > 0 ? std::to_string(subgenreId) : "";
    const std::string year = std::to_string(book.year);
    const std::string rating = std::to_string(book.rating);
    const std::string price = std::to_string(book.price);
    const std::string printRun = std::to_string(book.totalPrintRun);
    const std::string searchFrequency = std::to_string(book.searchFrequency);
    const std::string additional = join(book.additionalPrintDates, '|');

    const char* values[] = {
        id.c_str(), sid.c_str(), book.title.c_str(), book.author.c_str(), book.publisher.c_str(),
        year.c_str(), book.format.c_str(), rating.c_str(), price.c_str(), book.ageRating.c_str(),
        book.isbn.c_str(), printRun.c_str(), book.signedToPrintDate.c_str(), additional.c_str(),
        book.coverImagePath.c_str(), book.licenseImagePath.c_str(), book.bibliographicReference.c_str(),
        book.coverUrl.c_str(), searchFrequency.c_str()
    };

    PGresult* result = PQexecParams(
        conn,
        R"SQL(
            INSERT INTO books(
                id, subgenre_id, title, author, publisher, year, format, rating, price,
                age_rating, isbn, total_print_run, signed_to_print_date, additional_print_dates,
                cover_image_path, license_image_path, bibliographic_reference, cover_url, search_frequency
            ) VALUES (
                NULLIF($1,'')::int,
                NULLIF($2,'')::int,
                $3,$4,$5,
                $6::int,$7,$8::real,$9::real,
                $10,$11,
                $12::bigint,$13,$14,
                $15,$16,$17,$18,$19::real
            )
            ON CONFLICT(id) DO UPDATE SET
                subgenre_id = EXCLUDED.subgenre_id,
                title = EXCLUDED.title,
                author = EXCLUDED.author,
                publisher = EXCLUDED.publisher,
                year = EXCLUDED.year,
                format = EXCLUDED.format,
                rating = EXCLUDED.rating,
                price = EXCLUDED.price,
                age_rating = EXCLUDED.age_rating,
                isbn = EXCLUDED.isbn,
                total_print_run = EXCLUDED.total_print_run,
                signed_to_print_date = EXCLUDED.signed_to_print_date,
                additional_print_dates = EXCLUDED.additional_print_dates,
                cover_image_path = EXCLUDED.cover_image_path,
                license_image_path = EXCLUDED.license_image_path,
                bibliographic_reference = EXCLUDED.bibliographic_reference,
                cover_url = EXCLUDED.cover_url,
                search_frequency = EXCLUDED.search_frequency
            RETURNING id;
        )SQL",
        19,
        nullptr,
        values,
        nullptr,
        nullptr,
        0
    );

    if (!pgOk(result, {PGRES_TUPLES_OK}) || PQntuples(result) == 0) {
        appendLog("ERROR", "Book upsert failed.");
        PQclear(result);
        runExec(conn, "ROLLBACK;");
        return false;
    }

    book.id = std::atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    return runExec(conn, "COMMIT;");
}

bool LibraryStorage::removeBookById(int id) {
    PGconn* conn = static_cast<PGconn*>(db_);
    const std::string idText = std::to_string(id);
    const char* values[] = {idText.c_str()};
    PGresult* result = PQexecParams(conn, "DELETE FROM books WHERE id = $1::int;", 1, nullptr, values, nullptr, nullptr, 0);
    if (!pgOk(result, {PGRES_COMMAND_OK})) {
        appendLog("ERROR", "Book remove query failed for id=" + std::to_string(id));
        PQclear(result);
        return false;
    }
    const int affected = std::atoi(PQcmdTuples(result));
    PQclear(result);
    return affected > 0;
}

bool LibraryStorage::isEmpty() const {
    PGconn* conn = static_cast<PGconn*>(db_);
    PGresult* result = PQexec(conn, "SELECT COUNT(*) FROM books;");
    if (!pgOk(result, {PGRES_TUPLES_OK}) || PQntuples(result) == 0) {
        PQclear(result);
        return true;
    }
    const bool empty = std::atoi(PQgetvalue(result, 0, 0)) == 0;
    PQclear(result);
    return empty;
}

std::optional<Book> NetworkMetadataClient::fetchByQuery(const Book& draft) const {
    if (!isNetworkEnabled()) {
        return std::nullopt;
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
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        const auto markerPos = raw.rfind("__HTTP_CODE__:");
        if (markerPos == std::string::npos) {
            response = raw;
            httpCode = 0;
        } else {
            response = raw.substr(0, markerPos);
            httpCode = std::atoi(trim(raw.substr(markerPos + std::string("__HTTP_CODE__:").size())).c_str());
        }

        if (httpCode == 200) {
            break;
        }
        if (httpCode == 404 || httpCode == 400) {
            return std::nullopt;
        }
        if (httpCode == 429) {
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            continue;
        }
        if ((httpCode >= 500 || httpCode == 0) && attempt < 3) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    if (httpCode != 200 || response.empty()) {
        appendLog("WARN", "OpenLibrary unavailable, fallback to manual entry. HTTP=" + std::to_string(httpCode));
        return std::nullopt;
    }

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
            if (book.title.empty()) book.title = remote->title;
            if (book.author.empty()) book.author = remote->author;
            if (book.publisher.empty()) book.publisher = remote->publisher;
            if (book.year == 0) book.year = remote->year;
            if (book.isbn.empty()) book.isbn = remote->isbn;
            if (book.coverUrl.empty()) book.coverUrl = remote->coverUrl;
        }
    }

    if (trim(book.title).empty() || trim(book.author).empty()) {
        return false;
    }

    return storage_.upsertBook(book);
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
    if (!storage_.removeBookById(id)) {
        return false;
    }
    auto removeFile = [](const std::string& pathValue) {
        if (trim(pathValue).empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(pathValue, ec);
    };
    removeFile(coverPath);
    removeFile(licensePath);
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
