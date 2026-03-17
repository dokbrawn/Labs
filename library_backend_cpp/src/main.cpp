#include "library_backend.h"

#include <iostream>

namespace {
Book demoBook() {
    Book book;
    book.id = "book-demo";
    book.title = "Clean Code";
    book.author = "Robert C. Martin";
    book.genre = "Software";
    book.subgenre = "Engineering";
    book.year = 2008;
    book.format = "Hardcover";
    book.rating = 4.7;
    book.price = 34.99;
    book.ageRating = "16+";
    book.isbn = "9780132350884";
    book.totalPrintRun = 1000000;
    book.signedToPrintDate = "2008-08-01";
    book.additionalPrintDates = {"2010-01-15", "2015-06-20"};
    book.coverImagePath = "data/covers/9780132350884.jpg";
    book.licenseImagePath = "data/licenses/9780132350884.jpg";
    book.bibliographicReference = "Martin, R. C. Clean Code. Prentice Hall, 2008.";
    book.searchFrequency = 10.0;
    return book;
}
} // namespace

int main() {
    LibraryStorage storage("data/library.tsv");
    LibraryBackendService service(std::move(storage));

    if (!service.initialize()) {
        std::cerr << "Не удалось инициализировать хранилище\n";
        return 1;
    }

    if (service.allBooks().empty()) {
        service.addBook(demoBook(), false);
    }

    std::cout << "Всего книг: " << service.allBooks().size() << "\n";

    const auto found = service.searchBooks("clean");
    std::cout << "Найдено по запросу 'clean': " << found.size() << "\n";

    const auto obst = service.buildOptimalSearchTreeByIsbn();
    std::cout << "Узлов в optimal BST: " << obst.size() << "\n";

    return 0;
}
