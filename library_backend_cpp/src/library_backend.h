#pragma once

#include <optional>
#include <string>
#include <vector>

struct Book {
    int id = 0;

    std::string title;
    std::string author;

    std::string genre;
    std::string subgenre;

    std::string publisher;
    int year = 0;
    std::string format;

    double rating = 0.0;
    double price = 0.0;

    std::string ageRating;
    std::string isbn;

    long long totalPrintRun = 0;
    std::string signedToPrintDate;
    std::vector<std::string> additionalPrintDates;

    std::string coverImagePath;
    std::string licenseImagePath;

    std::string bibliographicReference;
};

enum class SortField {
    Title,
    Author,
    Genre,
    Subgenre,
    Publisher,
    Year,
    Format,
    Rating,
    Price,
    AgeRating,
    Isbn,
    TotalPrintRun,
    SignedToPrintDate
};

struct ObstNode {
    std::string isbn;
    int bookId = 0;
    int left = -1;
    int right = -1;
};

struct OpenLibraryCandidate {
    std::string title;
    std::string author;
    std::string publisher;
    std::string language;
    std::string isbn;
    std::string coverUrl;
    int year = 0;
};

class LibraryStorage {
public:
    explicit LibraryStorage(std::string connectionString = "");
    ~LibraryStorage();

    bool open();
    bool ensureSchema();
    bool runMigrations();

    bool upsertBook(Book& book);
    bool removeBookById(int id);

    std::vector<Book> allBooks() const;
    std::vector<Book> sortedBooks(SortField field, bool ascending) const;
    std::vector<Book> searchBooks(const std::string& query) const;

    bool isEmpty() const;
    bool rotateBackupIfNeeded() const;

    static Book readBookFromStatement(void* stmtPtr);

private:
    bool execute(const std::string& sql) const;
    bool ensureGenreHierarchy(const Book& book) const;

private:
    std::string connectionString_;
    void* db_;
};

class LibraryBackendService {
public:
    explicit LibraryBackendService(LibraryStorage storage = LibraryStorage());

    bool initialize();

    bool addOrUpdateBook(Book& book, bool fetchFromNetwork);
    bool removeBookById(int id);

    std::vector<Book> allBooks() const;
    std::vector<Book> sortedBooks(SortField field, bool ascending) const;
    std::vector<Book> searchBooks(const std::string& query) const;

    std::vector<ObstNode> buildOptimalSearchTreeByIsbn() const;
    std::vector<OpenLibraryCandidate> lookupOpenLibrary(const std::string& query, int limit = 15) const;

    static SortField parseSortField(const std::string& value);
    static std::string normalize(const std::string& value);

private:
    LibraryStorage storage_;
};

std::string serializeBookList(const std::vector<Book>& books);
std::string serializeOpenLibraryCandidates(const std::vector<OpenLibraryCandidate>& candidates);
std::optional<Book> parseBookFile(const std::string& filePath);
