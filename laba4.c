#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Функция для замены первого и последнего слова в строке
void swap_first_last_words(char **str_ptr) {
    char *str = *str_ptr;
    char *words[100]; // Массив для хранения слов
    int word_count = 0;

    // Разделяем строку на слова
    char *token = strtok(str, " ");
    while (token != NULL) {
        words[word_count++] = token;
        token = strtok(NULL, " ");
    }

    if (word_count < 2) {
        // Если слов меньше двух, ничего не меняем
        return;
    }

    // Меняем местами первое и последнее слово
    char *temp = words[0];
    words[0] = words[word_count - 1];
    words[word_count - 1] = temp;

    // Вычисляем общую длину новой строки
    size_t total_length = 0;
    for (int i = 0; i < word_count; i++) {
        total_length += strlen(words[i]) + 1; // +1 для пробела или \0
    }

    // Выделяем память под новую строку
    char *new_str = (char *)malloc(total_length);
    if (!new_str) {
        perror("Ошибка выделения памяти");
        exit(1);
    }

    new_str[0] = '\0';
    for (int i = 0; i < word_count; i++) {
        strcat(new_str, words[i]);
        if (i < word_count - 1) {
            strcat(new_str, " ");
        }
    }

    // Освобождаем старую строку и заменяем на новую
    free(*str_ptr);
    *str_ptr = new_str;
}

int main(int argc, char *argv[]) {
    // Проверка количества аргументов
    if (argc != 2) {
        printf("Использование: %s <путь_к_файлу>\n", argv[0]);
        return 1;
    }

    // Открываем файл для чтения
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("Ошибка: не удалось открыть файл %s.\n", argv[1]);
        return 1;
    }

    char **lines = NULL; // Динамический массив для строк
    int line_count = 0;  // Количество строк
    size_t max_length = 0; // Длина самой длинной строки
    int longest_line_index = -1; // Индекс самой длинной строки

    // Чтение файла построчно
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        // Удаляем символ новой строки, если он есть
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Добавляем строку в массив
        char **temp = realloc(lines, (line_count + 1) * sizeof(char *));
        if (!temp) {
            printf("Ошибка: не удалось выделить память.\n");
            fclose(file);
            for (int i = 0; i < line_count; i++) {
                free(lines[i]);
            }
            free(lines);
            return 1;
        }
        lines = temp;
        lines[line_count] = strdup(line);
        if (!lines[line_count]) {
            printf("Ошибка: не удалось выделить память.\n");
            fclose(file);
            for (int i = 0; i < line_count; i++) {
                free(lines[i]);
            }
            free(lines);
            return 1;
        }

        // Обновляем информацию о самой длинной строке
        if (strlen(lines[line_count]) > max_length) {
            max_length = strlen(lines[line_count]);
            longest_line_index = line_count;
        }

        line_count++;
    }

    // Проверка на ошибки чтения файла
    if (!feof(file)) {
        printf("Ошибка: файл содержит некорректные данные.\n");
        fclose(file);
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return 1;
    }

    fclose(file);

    // Если файл пуст
    if (line_count == 0) {
        printf("Ошибка: файл пуст.\n");
        return 1;
    }

    // Меняем местами первое и последнее слово в самой длинной строке
    swap_first_last_words(&lines[longest_line_index]);

    // Открываем файл для записи
    file = fopen(argv[1], "w");
    if (!file) {
        printf("Ошибка: не удалось открыть файл %s для записи.\n", argv[1]);
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return 1;
    }

    // Записываем строки обратно в файл
    for (int i = 0; i < line_count; i++) {
        fprintf(file, "%s\n", lines[i]);
    }

    fclose(file);

    // Освобождаем память
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);

    printf("First and last words were succesfully swapped.\n");
    return 0;
}