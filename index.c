#include <stdio.h>
#include <string.h>

typedef struct {
    int index;          // индекс абонента
    char last_name[50]; // фамилия
    char first_name[50]; // имя
    char middle_name[50]; // отчество
    char phone[15];     // телефон
} Subscriber;

// Сравнение по имени (для сортировки по индексам)
int lessByName(const Subscriber *x, const Subscriber *y) {
    return strcmp(x->first_name, y->first_name) < 0;
}

// Сравнение по номеру телефона (для сортировки по индексам)
int lessByPhone(const Subscriber *x, const Subscriber *y) {
    return strcmp(x->phone, y->phone) < 0;
}

// Сравнение по фамилии и отчеству (для сортировки по индексам)
int lessByLastNameAndMiddleName(const Subscriber *x, const Subscriber *y) {
    int surname_cmp = strcmp(x->last_name, y->last_name);
    if (surname_cmp != 0) return surname_cmp < 0;
    return strcmp(x->middle_name, y->middle_name) < 0;
}

// Сортировка вставками по индексам (не изменяет исходный массив)
void indexInsertSort(int *indexArr, const Subscriber *dir, int size, 
                    int (*less)(const Subscriber *, const Subscriber *)) {
    for (int i = 1; i < size; i++) {
        int tempIdx = indexArr[i];
        int j = i - 1;

        while (j >= 0 && less(&dir[tempIdx], &dir[indexArr[j]])) {
            indexArr[j + 1] = indexArr[j];
            j--;
        }
        indexArr[j + 1] = tempIdx;
    }
}

// Вывод справочника с использованием массива индексов
void printDirectoryByIndex(const Subscriber *dir, const int *indexArr, int size) {
    printf("\n--------------------Телефонная книга-----------------------\n");
    printf("|-------|---------------|---------------|---------------|---------|\n");
    printf("|Индекс |Фамилия\t|  Имя\t\t|Отчество       |Телефон  |\n");
    printf("|-------|---------------|---------------|---------------|---------|\n");

    for (int i = 0; i < size; i++) {
        const Subscriber *sub = &dir[indexArr[i]];
        printf("|%-7d|%-15s\t| %-15s\t|%-15s\t|%s|\n",
               sub->index,
               sub->last_name,
               sub->first_name,
               sub->middle_name,
               sub->phone);
        printf("|-------|---------------|---------------|---------------|---------|\n");
    }
}

int main() {
    Subscriber directory[] = {
        {0, "Паравозов", "Аркадий", "Иванов", "+78005535"},
        {1, "Максименко", "Максим", "Викторович", "+700602"},
        {2, "Родионов", "Данила", "Юрьевич", "+780055"},
        {3, "Анисимов", "Тимур", "Юрьевич", "+7923334"},
        {4, "Беликов", "Егор", "Владимирович", "+79234"}};

    int size = sizeof(directory) / sizeof(directory[0]);

    // Инициализация массива индексов (0, 1, 2, ..., size-1)
    int indexArr[size];
    for (int i = 0; i < size; i++) {
        indexArr[i] = i;
    }

    printf("Исходный справочник:");
    printDirectoryByIndex(directory, indexArr, size);

    // Сортировка индексов по имени
    indexInsertSort(indexArr, directory, size, lessByName);
    printf("\nОтсортированный по имени (через индексы):");
    printDirectoryByIndex(directory, indexArr, size);

    // Сортировка индексов по номеру телефона
    indexInsertSort(indexArr, directory, size, lessByPhone);
    printf("\nОтсортированный по номеру телефона (через индексы):");
    printDirectoryByIndex(directory, indexArr, size);

    // Сортировка индексов по фамилии и отчеству
    indexInsertSort(indexArr, directory, size, lessByLastNameAndMiddleName);
    printf("\nОтсортированный по фамилии и отчеству (через индексы):");
    printDirectoryByIndex(directory, indexArr, size);

    return 0;
}