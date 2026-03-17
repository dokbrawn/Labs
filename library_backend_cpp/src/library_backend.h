#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Book {
    std::string id;
    std::string title;
    std::string author;
    std::string genre;
    std::string subgenre;
    int year = 0;
    std::string format;
    double rating = 0.0;
    double price = 0.0;
    std::string ageRating;
    std::string isbn;
    int64_t totalPrintRun = 0;
    std::string signedToPrintDate;
    std::vector<std::string> additionalPrintDates;
    std::string coverImagePath;
    std::string licenseImagePath;
    std::string bibliographicReference;
    double searchFrequency = 1.0;
};

enum class SortField {
    Title,
    Author,
    Genre,
    Subgenre,
    Year,
    Rating,
    Price,
    AgeRating,
    Isbn,
    TotalPrintRun,
    SignedToPrintDate
};

class LibraryStorage {
public:
    explicit LibraryStorage(std::string filePath);
    bool load();
    bool save() const;

    const std::vector<Book>& books() const;
    std::vector<Book>& booksMutable();

private:
    std::string filePath_;
    std::vector<Book> books_;
};

class NetworkMetadataClient {
public:
    std::optional<Book> fetchByIsbn(const std::string& isbn) const;
};

struct ObstNode {
    std::string key;
    int bookIndex = -1;
    int left = -1;
    int right = -1;
};

class LibraryBackendService {
public:
    explicit LibraryBackendService(LibraryStorage storage);

    bool initialize();
    bool addBook(Book book, bool fetchFromNetwork);
    bool removeBookById(const std::string& id);

    std::vector<Book> searchBooks(const std::string& query) const;
    std::vector<Book> sortedBooks(SortField field, bool ascending) const;

    std::vector<ObstNode> buildOptimalSearchTreeByIsbn() const;

    const std::vector<Book>& allBooks() const;

private:
    LibraryStorage storage_;
    NetworkMetadataClient networkClient_;

    static std::string normalize(const std::string& value);
    static bool compareBooks(const Book& lhs, const Book& rhs, SortField field, bool ascending);
};
