# Backend на C++ для каталога Library

Этот модуль покрывает backend-часть задачи из требований:
- хранение книг/жанров/поджанров в SQLite БД;
- сортировка по любому полю;
- бинарный поиск (сначала по названию, затем по автору);
- построение дерева оптимального поиска (optimal BST) по ISBN;
- подгрузка данных из OpenLibrary при добавлении записи;
- хранение ссылок на изображения обложки/лицензии для каждой книги.

## Что реализовано для OpenLibrary + БД/СУБД

1. **СУБД: SQLite**
   - backend теперь работает с файлом `data/library.db`;
   - при старте автоматически создаются таблицы `genres`, `subgenres`, `books` и индексы;
   - связи сделаны через `FOREIGN KEY`, чтобы сохранить иерархию жанр → поджанр.

2. **Интеграция OpenLibrary API**
   - `NetworkMetadataClient` использует endpoint `https://openlibrary.org/isbn/{isbn}.json`;
   - при `addBook(..., true)` подтягивает название (`title`) и год публикации (`publish_date` → `year`), если локально эти поля пустые.

## Сборка и запуск

```bash
cd library_backend_cpp
cmake -S . -B build
cmake --build build
./build/library_backend
```

> Нужна установленная библиотека `sqlite3` (обычно доступна в Linux по умолчанию).

## Интеграция с GUI

GUI может вызывать сервисные методы:
- `addBook(...)`, `removeBookById(...)`
- `searchBooks(query)`
- `sortedBooks(field, ascending)`
- `buildOptimalSearchTreeByIsbn()`

Так backend остается универсальным, а графический слой можно сделать на любом стеке.
