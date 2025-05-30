#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SIZE 5

// Структура для хранения данных абонента
typedef struct {
    char surname[50];
    char name[50];
    char phone[15];
    int age;
} Abonent;

// Глобальные переменные
Abonent phonebook[SIZE] = {
    {"Иванов", "Алексей", "123-456-789", 25},
    {"Петров", "Иван", "987-654-321", 30},
    {"Сидоров", "Петр", "555-555-555", 22},
    {"Бобров", "Сергей", "111-222-333", 28},
    {"Кузнецов", "Дмитрий", "444-444-444", 35}
};

int surname_index[SIZE];
int phone_index[SIZE];

// Функции сравнения по индексам
int compare_surnames(const void *a, const void *b) {
    int i = *(const int *)a;
    int j = *(const int *)b;
    return strcmp(phonebook[i].surname, phonebook[j].surname);
}

int compare_phones(const void *a, const void *b) {
    int i = *(const int *)a;
    int j = *(const int *)b;
    return strcmp(phonebook[i].phone, phonebook[j].phone);
}

// Инициализация индексных массивов
void init_index_arrays() {
    for (int i = 0; i < SIZE; i++) {
        surname_index[i] = i;
        phone_index[i] = i;
    }

    qsort(surname_index, SIZE, sizeof(int), compare_surnames);
    qsort(phone_index, SIZE, sizeof(int), compare_phones);
}

// Вывод справочника по индексному массиву
void print_using_index(int index_array[], const char *sort_field) {
    printf("\nСправочник, отсортированный по %s:\n", sort_field);
    printf("------------------------------------------------\n");
    printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
    printf("------------------------------------------------\n");

    for (int i = 0; i < SIZE; i++) {
        int idx = index_array[i];
        printf("%-15s %-15s %-15s %d\n",
               phonebook[idx].surname,
               phonebook[idx].name,
               phonebook[idx].phone,
               phonebook[idx].age);
    }
    printf("\n");
}

// Бинарный поиск по фамилии
void binary_search_surname(const char *target) {
    int left = 0, right = SIZE - 1, found = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int idx = surname_index[mid];
        int cmp = strcmp(phonebook[idx].surname, target);

        if (cmp == 0) {
            printf("\nНайденные абоненты с фамилией '%s':\n", target);
            printf("------------------------------------------------\n");
            printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
            printf("------------------------------------------------\n");

            // Влево
            int i = mid;
            while (i >= 0 && strcmp(phonebook[surname_index[i]].surname, target) == 0) {
                idx = surname_index[i--];
                printf("%-15s %-15s %-15s %d\n",
                       phonebook[idx].surname,
                       phonebook[idx].name,
                       phonebook[idx].phone,
                       phonebook[idx].age);
            }

            // Вправо
            i = mid + 1;
            while (i < SIZE && strcmp(phonebook[surname_index[i]].surname, target) == 0) {
                idx = surname_index[i++];
                printf("%-15s %-15s %-15s %d\n",
                       phonebook[idx].surname,
                       phonebook[idx].name,
                       phonebook[idx].phone,
                       phonebook[idx].age);
            }

            found = 1;
            break;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (!found) {
        printf("\nАбонент с фамилией '%s' не найден.\n", target);
    }
}

// Бинарный поиск по телефону
void binary_search_phone(const char *target) {
    int left = 0, right = SIZE - 1, found = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int idx = phone_index[mid];
        int cmp = strcmp(phonebook[idx].phone, target);

if (cmp == 0) {
            printf("\nНайден абонент с телефоном '%s':\n", target);
            printf("------------------------------------------------\n");
            printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
            printf("------------------------------------------------\n");
            printf("%-15s %-15s %-15s %d\n",
                   phonebook[idx].surname,
                   phonebook[idx].name,
                   phonebook[idx].phone,
                   phonebook[idx].age);
            found = 1;
            break;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (!found) {
        printf("\nАбонент с телефоном '%s' не найден.\n", target);
    }
}

int main() {
    init_index_arrays();

    printf("Исходный телефонный справочник:\n");
    printf("------------------------------------------------\n");
    printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
    printf("------------------------------------------------\n");
    for (int i = 0; i < SIZE; i++) {
        printf("%-15s %-15s %-15s %d\n",
               phonebook[i].surname,
               phonebook[i].name,
               phonebook[i].phone,
               phonebook[i].age);
    }

    print_using_index(surname_index, "фамилии");
    print_using_index(phone_index, "телефону");

    char search_surname[50];
    printf("Введите фамилию для поиска: ");
    scanf("%s", search_surname);
    binary_search_surname(search_surname);

    char search_phone[15];
    printf("\nВведите телефон для поиска: ");
    scanf("%s", search_phone);
    binary_search_phone(search_phone);

    return 0;
}