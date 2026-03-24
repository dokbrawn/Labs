#include "library_backend.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string defaultDbPath() {
    const char* conn = std::getenv("LIBRARY_PG_CONN");
    if (conn != nullptr && std::string(conn).size() > 0) {
        return conn;
    }
    return "host=localhost port=5432 dbname=library user=postgres password=postgres";
}

std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return trim(input);
}

int readInt(const std::string& prompt, int fallback = 0) {
    const std::string raw = readLine(prompt);
    if (raw.empty()) {
        return fallback;
    }
    try {
        return std::stoi(raw);
    } catch (...) {
        return fallback;
    }
}

double readDouble(const std::string& prompt, double fallback = 0.0) {
    const std::string raw = readLine(prompt);
    if (raw.empty()) {
        return fallback;
    }
    try {
        return std::stod(raw);
    } catch (...) {
        return fallback;
    }
}

std::int64_t readInt64(const std::string& prompt, std::int64_t fallback = 0) {
    const std::string raw = readLine(prompt);
    if (raw.empty()) {
        return fallback;
    }
    try {
        return std::stoll(raw);
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> parsePipeList(const std::string& raw) {
    std::vector<std::string> items;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, '|')) {
        const auto cleaned = trim(item);
        if (!cleaned.empty()) {
            items.push_back(cleaned);
        }
    }
    return items;
}

void printBook(const Book& book) {
    std::cout << "[ID " << book.id << "] " << book.title << " — " << book.author << "\n"
              << "  Жанр: " << (book.genre.empty() ? "-" : book.genre)
              << " / " << (book.subgenre.empty() ? "-" : book.subgenre) << "\n"
              << "  ISBN: " << (book.isbn.empty() ? "-" : book.isbn)
              << ", Год: " << book.year
              << ", Цена: " << std::fixed << std::setprecision(2) << book.price
              << ", Рейтинг: " << std::setprecision(1) << book.rating << "\n"
              << "  Издательство: " << (book.publisher.empty() ? "-" : book.publisher)
              << ", Формат: " << (book.format.empty() ? "-" : book.format)
              << ", Возраст: " << (book.ageRating.empty() ? "-" : book.ageRating) << "\n"
              << "  Тираж: " << book.totalPrintRun
              << ", Дата подписи в печать: " << (book.signedToPrintDate.empty() ? "-" : book.signedToPrintDate) << "\n"
              << "  Обложка: " << (book.coverImagePath.empty() ? (book.coverUrl.empty() ? "-" : book.coverUrl) : book.coverImagePath) << "\n"
              << "  Лицензия: " << (book.licenseImagePath.empty() ? "-" : book.licenseImagePath) << "\n";

    if (!book.additionalPrintDates.empty()) {
        std::cout << "  Доп. тиражи: ";
        for (std::size_t i = 0; i < book.additionalPrintDates.size(); ++i) {
            if (i > 0) {
                std::cout << " | ";
            }
            std::cout << book.additionalPrintDates[i];
        }
        std::cout << "\n";
    }
    if (!book.bibliographicReference.empty()) {
        std::cout << "  Библиографическая ссылка: " << book.bibliographicReference << "\n";
    }
}

void printBooks(const std::vector<Book>& books, const std::string& header) {
    std::cout << "\n=== " << header << " ===\n";
    std::cout << "Найдено записей: " << books.size() << "\n";
    for (const auto& book : books) {
        printBook(book);
        std::cout << "----------------------------------------\n";
    }
    if (books.empty()) {
        std::cout << "(пусто)\n";
    }
}

Book promptBook(int existingId = 0) {
    Book book;
    book.id = existingId;

    if (existingId > 0) {
        std::cout << "Редактирование книги с ID=" << existingId << " (оставьте поле пустым, чтобы использовать значение по умолчанию)\n";
    }

    book.title = readLine("Название: ");
    book.author = readLine("Автор: ");
    book.genre = readLine("Жанр: ");
    book.subgenre = readLine("Поджанр: ");
    book.publisher = readLine("Издательство: ");
    book.year = readInt("Год издания (число): ", 0);
    book.format = readLine("Формат: ");
    book.rating = readDouble("Рейтинг (число): ", 0.0);
    book.price = readDouble("Цена (число): ", 0.0);
    book.ageRating = readLine("Возрастной рейтинг (например, 12+): ");
    book.isbn = readLine("ISBN: ");
    book.totalPrintRun = readInt64("Общий тираж (число): ", 0);
    book.signedToPrintDate = readLine("Дата подписи в печать: ");
    book.additionalPrintDates = parsePipeList(readLine("Доп. тиражи через | (пример: 2020-01-01|2021-03-03): "));
    book.coverImagePath = readLine("Путь к файлу обложки: ");
    book.licenseImagePath = readLine("Путь к фото лицензии: ");
    book.bibliographicReference = readLine("Библиографическая ссылка: ");
    book.coverUrl = readLine("URL обложки (если есть): ");
    book.searchFrequency = readDouble("Частота поиска для OBST (по умолчанию 1): ", 1.0);

    return book;
}

SortField askSortField() {
    std::cout << "\nПоля сортировки:\n"
              << "  title, author, genre, subgenre, publisher, year, format, rating,\n"
              << "  price, age_rating, isbn, total_print_run, signed_to_print_date\n";
    const std::string field = readLine("Введите поле сортировки: ");
    return LibraryBackendService::parseSortField(field);
}

void printMenu() {
    std::cout << "\n==============================\n"
              << " Library Backend Console Menu\n"
              << "==============================\n"
              << "1. Показать все книги\n"
              << "2. Поиск книг\n"
              << "3. Сортировка книг\n"
              << "4. Добавить новую книгу\n"
              << "5. Обновить существующую книгу\n"
              << "6. Удалить книгу\n"
              << "7. Подгрузить книгу из API (OpenLibrary)\n"
              << "8. Показать OBST\n"
              << "0. Выход\n";
}
} // namespace

int main() {
    const std::string dbPath = defaultDbPath();
    LibraryStorage storage(dbPath);
    LibraryBackendService service(std::move(storage));

    if (!service.initialize()) {
        std::cerr << "Не удалось инициализировать backend (SQLite).\n";
        return 1;
    }

    std::cout << "Backend инициализирован. БД: " << dbPath << "\n";

    while (true) {
        printMenu();
        const int action = readInt("Выберите действие: ", -1);

        switch (action) {
        case 1: {
            printBooks(service.allBooks(), "Список всех книг");
            break;
        }
        case 2: {
            const std::string query = readLine("Введите поисковый запрос: ");
            printBooks(service.searchBooks(query), "Результаты поиска");
            break;
        }
        case 3: {
            const SortField field = askSortField();
            const std::string dir = readLine("Направление (asc/desc): ");
            const bool ascending = dir != "desc" && dir != "DESC";
            printBooks(service.sortedBooks(field, ascending), "Результаты сортировки");
            break;
        }
        case 4: {
            Book book = promptBook(0);
            const std::string fetch = readLine("Подтянуть метаданные из API? (y/n): ");
            const bool fetchNetwork = (fetch == "y" || fetch == "Y");
            if (!service.addOrUpdateBook(book, fetchNetwork)) {
                std::cout << "Ошибка: не удалось добавить книгу.\n";
            } else {
                printBooks({book}, "Книга успешно добавлена");
            }
            break;
        }
        case 5: {
            const int id = readInt("Введите ID книги для обновления: ", 0);
            if (id <= 0) {
                std::cout << "Неверный ID.\n";
                break;
            }
            Book book = promptBook(id);
            const std::string fetch = readLine("Подтянуть метаданные из API? (y/n): ");
            const bool fetchNetwork = (fetch == "y" || fetch == "Y");
            if (!service.addOrUpdateBook(book, fetchNetwork)) {
                std::cout << "Ошибка: не удалось обновить книгу.\n";
            } else {
                printBooks({book}, "Книга успешно обновлена");
            }
            break;
        }
        case 6: {
            const int id = readInt("Введите ID книги для удаления: ", 0);
            if (id <= 0) {
                std::cout << "Неверный ID.\n";
                break;
            }
            if (!service.removeBookById(id)) {
                std::cout << "Удаление не выполнено (возможно, ID не найден).\n";
            } else {
                std::cout << "Книга удалена.\n";
                printBooks(service.allBooks(), "Актуальный список книг");
            }
            break;
        }
        case 7: {
            Book book;
            book.title = readLine("Введите название/запрос для API: ");
            book.author = readLine("Введите автора (опционально): ");
            if (!service.addOrUpdateBook(book, true)) {
                std::cout << "Ошибка: не удалось загрузить или сохранить книгу из API.\n";
            } else {
                printBooks({book}, "Результат API-загрузки");
            }
            break;
        }
        case 8: {
            const auto nodes = service.buildOptimalSearchTreeByIsbn();
            std::cout << "\n=== OBST ===\n";
            std::cout << "Узлов: " << nodes.size() << "\n";
            for (const auto& node : nodes) {
                std::cout << "key=" << node.key << ",book_id=" << node.bookId
                          << ",left=" << node.left << ",right=" << node.right << "\n";
            }
            break;
        }
        case 0:
            std::cout << "Выход.\n";
            return 0;
        default:
            std::cout << "Неизвестный пункт меню.\n";
            break;
        }
    }
}
