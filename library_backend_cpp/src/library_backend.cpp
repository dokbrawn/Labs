#include "library_backend.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <utility>

#include <sqlite3.h>

namespace {
std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}

std::string join(const std::vector<std::string>& values, char delimiter) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << delimiter;
        }
        out << values[i];
    }
    return out.str();
}

std::string shellEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('\'');
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

bool execSql(sqlite3* db, const char* sql) {
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        if (errorMessage) {
            std::cerr << "SQLite error: " << errorMessage << "\n";
            sqlite3_free(errorMessage);
        }
        return false;
    }
    return true;
}

bool ensureSchema(sqlite3* db) {
    constexpr const char* schemaSql = R"SQL(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS genres (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );

        CREATE TABLE IF NOT EXISTS subgenres (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            genre_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            UNIQUE (genre_id, name),
            FOREIGN KEY (genre_id) REFERENCES genres(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS books (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            author TEXT NOT NULL,
            genre_id INTEGER,
            subgenre_id INTEGER,
            year INTEGER DEFAULT 0,
            format TEXT,
            rating REAL DEFAULT 0.0,
            price REAL DEFAULT 0.0,
            age_rating TEXT,
            isbn TEXT,
            total_print_run INTEGER DEFAULT 0,
            signed_to_print_date TEXT,
            additional_print_dates TEXT,
            cover_image_path TEXT,
            license_image_path TEXT,
            bibliographic_reference TEXT,
            search_frequency REAL DEFAULT 1.0,
            FOREIGN KEY (genre_id) REFERENCES genres(id),
            FOREIGN KEY (subgenre_id) REFERENCES subgenres(id)
        );

        CREATE INDEX IF NOT EXISTS idx_books_title ON books(title);
        CREATE INDEX IF NOT EXISTS idx_books_author ON books(author);
        CREATE INDEX IF NOT EXISTS idx_books_isbn ON books(isbn);
    )SQL";

    return execSql(db, schemaSql);
}

int64_t upsertGenre(sqlite3* db, const std::string& genre) {
    if (genre.empty()) {
        return -1;
    }

    sqlite3_stmt* insertStmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO genres(name) VALUES (?1)", -1, &insertStmt, nullptr);
    sqlite3_bind_text(insertStmt, 1, genre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(insertStmt);
    sqlite3_finalize(insertStmt);

    sqlite3_stmt* selectStmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT id FROM genres WHERE name = ?1", -1, &selectStmt, nullptr);
    sqlite3_bind_text(selectStmt, 1, genre.c_str(), -1, SQLITE_TRANSIENT);

    int64_t id = -1;
    if (sqlite3_step(selectStmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(selectStmt, 0);
    }

    sqlite3_finalize(selectStmt);
    return id;
}

int64_t upsertSubgenre(sqlite3* db, int64_t genreId, const std::string& subgenre) {
    if (genreId <= 0 || subgenre.empty()) {
        return -1;
    }

    sqlite3_stmt* insertStmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO subgenres(genre_id, name) VALUES (?1, ?2)", -1, &insertStmt, nullptr);
    sqlite3_bind_int64(insertStmt, 1, genreId);
    sqlite3_bind_text(insertStmt, 2, subgenre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(insertStmt);
    sqlite3_finalize(insertStmt);

    sqlite3_stmt* selectStmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT id FROM subgenres WHERE genre_id = ?1 AND name = ?2", -1, &selectStmt, nullptr);
    sqlite3_bind_int64(selectStmt, 1, genreId);
    sqlite3_bind_text(selectStmt, 2, subgenre.c_str(), -1, SQLITE_TRANSIENT);

    int64_t id = -1;
    if (sqlite3_step(selectStmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(selectStmt, 0);
    }

    sqlite3_finalize(selectStmt);
    return id;
}

std::string extractJsonString(const std::string& payload, const std::string& field) {
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    if (std::regex_search(payload, match, pattern) && match.size() > 1) {
        return match[1].str();
    }
    return {};
}

int extractYear(const std::string& publishDate) {
    const std::regex yearPattern("(19|20)\\d{2}");
    std::smatch match;
    if (std::regex_search(publishDate, match, yearPattern) && match.size() > 0) {
        return std::stoi(match[0].str());
    }
    return 0;
}


template <typename Compare>
int partitionBooks(std::vector<Book>& books, int low, int high, Compare compare) {
    const Book pivot = books[high];
    int i = low - 1;
    for (int j = low; j < high; ++j) {
        if (compare(books[j], pivot)) {
            ++i;
            std::swap(books[i], books[j]);
        }
    }
    std::swap(books[i + 1], books[high]);
    return i + 1;
}

template <typename Compare>
void quickSortBooks(std::vector<Book>& books, int low, int high, Compare compare) {
    if (low >= high) {
        return;
    }

    const int pivotIndex = partitionBooks(books, low, high, compare);
    quickSortBooks(books, low, pivotIndex - 1, compare);
    quickSortBooks(books, pivotIndex + 1, high, compare);
}

template <typename Compare>
void quickSortBooks(std::vector<Book>& books, Compare compare) {
    if (books.empty()) {
        return;
    }
    quickSortBooks(books, 0, static_cast<int>(books.size()) - 1, compare);
}
} // namespace

LibraryStorage::LibraryStorage(std::string filePath)
    : filePath_(std::move(filePath)) {}

bool LibraryStorage::load() {
    books_.clear();

    sqlite3* db = nullptr;
    if (sqlite3_open(filePath_.c_str(), &db) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return false;
    }

    if (!ensureSchema(db)) {
        sqlite3_close(db);
        return false;
    }

    constexpr const char* selectBooks = R"SQL(
        SELECT
            b.id,
            b.title,
            b.author,
            COALESCE(g.name, ''),
            COALESCE(s.name, ''),
            b.year,
            COALESCE(b.format, ''),
            b.rating,
            b.price,
            COALESCE(b.age_rating, ''),
            COALESCE(b.isbn, ''),
            b.total_print_run,
            COALESCE(b.signed_to_print_date, ''),
            COALESCE(b.additional_print_dates, ''),
            COALESCE(b.cover_image_path, ''),
            COALESCE(b.license_image_path, ''),
            COALESCE(b.bibliographic_reference, ''),
            b.search_frequency
        FROM books b
        LEFT JOIN genres g ON g.id = b.genre_id
        LEFT JOIN subgenres s ON s.id = b.subgenre_id
        ORDER BY b.rowid;
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, selectBooks, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.genre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.subgenre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.year = sqlite3_column_int(stmt, 5);
        book.format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        book.rating = sqlite3_column_double(stmt, 7);
        book.price = sqlite3_column_double(stmt, 8);
        book.ageRating = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        book.isbn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        book.totalPrintRun = sqlite3_column_int64(stmt, 11);
        book.signedToPrintDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        book.additionalPrintDates = split(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13)), '|');
        book.coverImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        book.licenseImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        book.bibliographicReference = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));
        book.searchFrequency = sqlite3_column_double(stmt, 17);

        books_.push_back(std::move(book));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

bool LibraryStorage::save() const {
    sqlite3* db = nullptr;
    if (sqlite3_open(filePath_.c_str(), &db) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return false;
    }

    if (!ensureSchema(db)) {
        sqlite3_close(db);
        return false;
    }

    if (!execSql(db, "BEGIN TRANSACTION;")) {
        sqlite3_close(db);
        return false;
    }

    if (!execSql(db, "DELETE FROM books;") || !execSql(db, "DELETE FROM subgenres;") || !execSql(db, "DELETE FROM genres;")) {
        execSql(db, "ROLLBACK;");
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* insertBookStmt = nullptr;
    constexpr const char* insertBookSql = R"SQL(
        INSERT INTO books (
            id, title, author, genre_id, subgenre_id, year, format, rating, price,
            age_rating, isbn, total_print_run, signed_to_print_date, additional_print_dates,
            cover_image_path, license_image_path, bibliographic_reference, search_frequency
        ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18)
    )SQL";

    if (sqlite3_prepare_v2(db, insertBookSql, -1, &insertBookStmt, nullptr) != SQLITE_OK) {
        execSql(db, "ROLLBACK;");
        sqlite3_close(db);
        return false;
    }

    for (const auto& book : books_) {
        const int64_t genreId = upsertGenre(db, book.genre);
        const int64_t subgenreId = upsertSubgenre(db, genreId, book.subgenre);

        sqlite3_reset(insertBookStmt);
        sqlite3_clear_bindings(insertBookStmt);

        sqlite3_bind_text(insertBookStmt, 1, book.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 2, book.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 3, book.author.c_str(), -1, SQLITE_TRANSIENT);
        if (genreId > 0) {
            sqlite3_bind_int64(insertBookStmt, 4, genreId);
        } else {
            sqlite3_bind_null(insertBookStmt, 4);
        }
        if (subgenreId > 0) {
            sqlite3_bind_int64(insertBookStmt, 5, subgenreId);
        } else {
            sqlite3_bind_null(insertBookStmt, 5);
        }
        sqlite3_bind_int(insertBookStmt, 6, book.year);
        sqlite3_bind_text(insertBookStmt, 7, book.format.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(insertBookStmt, 8, book.rating);
        sqlite3_bind_double(insertBookStmt, 9, book.price);
        sqlite3_bind_text(insertBookStmt, 10, book.ageRating.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 11, book.isbn.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insertBookStmt, 12, book.totalPrintRun);
        sqlite3_bind_text(insertBookStmt, 13, book.signedToPrintDate.c_str(), -1, SQLITE_TRANSIENT);
        const auto additionalDates = join(book.additionalPrintDates, '|');
        sqlite3_bind_text(insertBookStmt, 14, additionalDates.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 15, book.coverImagePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 16, book.licenseImagePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertBookStmt, 17, book.bibliographicReference.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(insertBookStmt, 18, book.searchFrequency);

        if (sqlite3_step(insertBookStmt) != SQLITE_DONE) {
            sqlite3_finalize(insertBookStmt);
            execSql(db, "ROLLBACK;");
            sqlite3_close(db);
            return false;
        }
    }

    sqlite3_finalize(insertBookStmt);

    if (!execSql(db, "COMMIT;")) {
        execSql(db, "ROLLBACK;");
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    return true;
}

const std::vector<Book>& LibraryStorage::books() const {
    return books_;
}

std::vector<Book>& LibraryStorage::booksMutable() {
    return books_;
}

std::optional<Book> NetworkMetadataClient::fetchByIsbn(const std::string& isbn) const {
    const std::string url = "https://openlibrary.org/isbn/" + isbn + ".json";
    const std::string command = "curl -fsSL " + shellEscape(url);

    std::array<char, 256> buffer{};
    std::string response;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        response += buffer.data();
    }

    const int code = pclose(pipe);
    if (code != 0 || response.empty()) {
        return std::nullopt;
    }

    Book remote;
    remote.isbn = isbn;
    remote.title = extractJsonString(response, "title");
    const auto publishDate = extractJsonString(response, "publish_date");
    remote.year = extractYear(publishDate);

    return remote;
}

LibraryBackendService::LibraryBackendService(LibraryStorage storage)
    : storage_(std::move(storage)) {}

bool LibraryBackendService::initialize() {
    return storage_.load();
}

bool LibraryBackendService::addBook(Book book, bool fetchFromNetwork) {
    if (fetchFromNetwork && !book.isbn.empty()) {
        const auto remote = networkClient_.fetchByIsbn(book.isbn);
        if (remote.has_value()) {
            if (book.title.empty() && !remote->title.empty()) {
                book.title = remote->title;
            }
            if (book.author.empty() && !remote->author.empty()) {
                book.author = remote->author;
            }
            if (book.year == 0 && remote->year > 0) {
                book.year = remote->year;
            }
        }
    }

    if (book.id.empty()) {
        book.id = "book-" + std::to_string(storage_.books().size() + 1);
    }

    storage_.booksMutable().push_back(std::move(book));
    return storage_.save();
}

bool LibraryBackendService::removeBookById(const std::string& id) {
    auto& books = storage_.booksMutable();
    const auto oldSize = books.size();
    books.erase(std::remove_if(books.begin(), books.end(), [&](const Book& book) {
                   return book.id == id;
               }),
               books.end());

    if (books.size() == oldSize) {
        return false;
    }

    return storage_.save();
}

std::vector<Book> LibraryBackendService::searchBooks(const std::string& query) const {
    const auto q = normalize(query);
    std::vector<Book> sorted = storage_.books();

    quickSortBooks(sorted, [](const Book& lhs, const Book& rhs) {
        return normalize(lhs.title) < normalize(rhs.title);
    });

    auto titleLowerBound = std::lower_bound(sorted.begin(), sorted.end(), q, [](const Book& book, const std::string& value) {
        return normalize(book.title) < value;
    });

    std::vector<Book> result;
    for (auto it = titleLowerBound; it != sorted.end(); ++it) {
        const auto titleNorm = normalize(it->title);
        if (titleNorm.rfind(q, 0) == 0) {
            result.push_back(*it);
        } else {
            break;
        }
    }

    if (!result.empty()) {
        return result;
    }

    quickSortBooks(sorted, [](const Book& lhs, const Book& rhs) {
        return normalize(lhs.author) < normalize(rhs.author);
    });

    auto authorLowerBound = std::lower_bound(sorted.begin(), sorted.end(), q, [](const Book& book, const std::string& value) {
        return normalize(book.author) < value;
    });

    for (auto it = authorLowerBound; it != sorted.end(); ++it) {
        const auto authorNorm = normalize(it->author);
        if (authorNorm.rfind(q, 0) == 0) {
            result.push_back(*it);
        } else {
            break;
        }
    }

    return result;
}

std::vector<Book> LibraryBackendService::sortedBooks(SortField field, bool ascending) const {
    std::vector<Book> sorted = storage_.books();
    quickSortBooks(sorted, [&](const Book& lhs, const Book& rhs) {
        return compareBooks(lhs, rhs, field, ascending);
    });
    return sorted;
}

std::vector<ObstNode> LibraryBackendService::buildOptimalSearchTreeByIsbn() const {
    auto books = storage_.books();
    quickSortBooks(books, [](const Book& lhs, const Book& rhs) {
        return lhs.isbn < rhs.isbn;
    });

    const int n = static_cast<int>(books.size());
    if (n == 0) {
        return {};
    }

    std::vector<std::vector<double>> cost(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<int>> root(n, std::vector<int>(n, -1));

    for (int i = 0; i < n; ++i) {
        cost[i][i] = books[i].searchFrequency;
        root[i][i] = i;
    }

    for (int len = 2; len <= n; ++len) {
        for (int i = 0; i + len - 1 < n; ++i) {
            int j = i + len - 1;
            double freqSum = 0.0;
            for (int k = i; k <= j; ++k) {
                freqSum += books[k].searchFrequency;
            }

            cost[i][j] = 1e18;
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

    std::function<int(int, int)> build = [&](int l, int r) -> int {
        if (l > r) {
            return -1;
        }
        const int rIdx = root[l][r];
        const int currentNodeIndex = static_cast<int>(nodes.size());
        nodes.push_back(ObstNode{books[rIdx].isbn, rIdx, -1, -1});
        nodes[currentNodeIndex].left = build(l, rIdx - 1);
        nodes[currentNodeIndex].right = build(rIdx + 1, r);
        return currentNodeIndex;
    };

    build(0, n - 1);
    return nodes;
}

const std::vector<Book>& LibraryBackendService::allBooks() const {
    return storage_.books();
}

std::string LibraryBackendService::normalize(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char c : value) {
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }
    return normalized;
}

bool LibraryBackendService::compareBooks(const Book& lhs, const Book& rhs, SortField field, bool ascending) {
    auto cmp = [&](auto l, auto r) {
        return ascending ? (l < r) : (l > r);
    };

    switch (field) {
    case SortField::Title:
        return cmp(lhs.title, rhs.title);
    case SortField::Author:
        return cmp(lhs.author, rhs.author);
    case SortField::Genre:
        return cmp(lhs.genre, rhs.genre);
    case SortField::Subgenre:
        return cmp(lhs.subgenre, rhs.subgenre);
    case SortField::Year:
        return cmp(lhs.year, rhs.year);
    case SortField::Rating:
        return cmp(lhs.rating, rhs.rating);
    case SortField::Price:
        return cmp(lhs.price, rhs.price);
    case SortField::AgeRating:
        return cmp(lhs.ageRating, rhs.ageRating);
    case SortField::Isbn:
        return cmp(lhs.isbn, rhs.isbn);
    case SortField::TotalPrintRun:
        return cmp(lhs.totalPrintRun, rhs.totalPrintRun);
    case SortField::SignedToPrintDate:
        return cmp(lhs.signedToPrintDate, rhs.signedToPrintDate);
    default:
        return cmp(lhs.id, rhs.id);
    }
}
