#include "library_backend.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <utility>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

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
} // namespace

LibraryStorage::LibraryStorage(std::string filePath)
    : filePath_(std::move(filePath)) {}

bool LibraryStorage::load() {
    books_.clear();
    std::ifstream in(filePath_);
    if (!in.good()) {
        return true;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        auto fields = split(line, '\t');
        if (fields.size() < 17) {
            continue;
        }

        Book book;
        book.id = fields[0];
        book.title = fields[1];
        book.author = fields[2];
        book.genre = fields[3];
        book.subgenre = fields[4];
        book.year = std::stoi(fields[5]);
        book.format = fields[6];
        book.rating = std::stod(fields[7]);
        book.price = std::stod(fields[8]);
        book.ageRating = fields[9];
        book.isbn = fields[10];
        book.totalPrintRun = std::stoll(fields[11]);
        book.signedToPrintDate = fields[12];
        book.additionalPrintDates = split(fields[13], '|');
        book.coverImagePath = fields[14];
        book.licenseImagePath = fields[15];
        book.bibliographicReference = fields[16];
        if (fields.size() > 17) {
            book.searchFrequency = std::stod(fields[17]);
        }

        books_.push_back(std::move(book));
    }
    return true;
}

bool LibraryStorage::save() const {
    std::ofstream out(filePath_, std::ios::trunc);
    if (!out.good()) {
        return false;
    }

    for (const auto& book : books_) {
        out << book.id << '\t'
            << book.title << '\t'
            << book.author << '\t'
            << book.genre << '\t'
            << book.subgenre << '\t'
            << book.year << '\t'
            << book.format << '\t'
            << book.rating << '\t'
            << book.price << '\t'
            << book.ageRating << '\t'
            << book.isbn << '\t'
            << book.totalPrintRun << '\t'
            << book.signedToPrintDate << '\t'
            << join(book.additionalPrintDates, '|') << '\t'
            << book.coverImagePath << '\t'
            << book.licenseImagePath << '\t'
            << book.bibliographicReference << '\t'
            << book.searchFrequency
            << '\n';
    }

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

    const auto titlePos = response.find("\"title\"");
    if (titlePos != std::string::npos) {
        const auto firstQuote = response.find('"', response.find(':', titlePos) + 1);
        const auto secondQuote = response.find('"', firstQuote + 1);
        if (firstQuote != std::string::npos && secondQuote != std::string::npos) {
            remote.title = response.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        }
    }

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

    std::sort(sorted.begin(), sorted.end(), [](const Book& lhs, const Book& rhs) {
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

    std::sort(sorted.begin(), sorted.end(), [](const Book& lhs, const Book& rhs) {
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
    std::sort(sorted.begin(), sorted.end(), [&](const Book& lhs, const Book& rhs) {
        return compareBooks(lhs, rhs, field, ascending);
    });
    return sorted;
}

std::vector<ObstNode> LibraryBackendService::buildOptimalSearchTreeByIsbn() const {
    auto books = storage_.books();
    std::sort(books.begin(), books.end(), [](const Book& lhs, const Book& rhs) {
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
