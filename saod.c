#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char surname[50];
    char name[50];
    char phone[15];
    int age;
} Abonent;

typedef enum {
    SORT_BY_NAME,
    SORT_BY_PHONE_AGE  // Новый сложный ключ: телефон + возраст
} SortType;

typedef enum {
    ASCENDING,
    DESCENDING
} SortDirection;

// Глобальные переменные для настройки сортировки
SortType sortType = SORT_BY_NAME;
SortDirection sortDirection = ASCENDING;

// Функция сравнения для сложного ключа
int compare(const Abonent *a, const Abonent *b) {
    int result = 0;
    
    if (sortType == SORT_BY_NAME) {
        // Сначала сравниваем фамилии
        result = strcmp(a->surname, b->surname);
        if (result == 0) {
            // Если фамилии одинаковые, сравниваем имена
            result = strcmp(a->name, b->name);
        }
    } else {
        // Сложный ключ: телефон + возраст
        result = strcmp(a->phone, b->phone);
        if (result == 0) {
            // Если телефоны одинаковые, сравниваем возраст
            result = a->age - b->age;
        }
    }
    
    // Учитываем направление сортировки
    return (sortDirection == ASCENDING) ? result : -result;
}

// Сортировка вставками (вместо пузырьковой)
void insertionSort(Abonent *phonebook, int size) {
    for (int i = 1; i < size; i++) {
        Abonent key = phonebook[i];
        int j = i - 1;
        
        while (j >= 0 && compare(&phonebook[j], &key) > 0) {
            phonebook[j + 1] = phonebook[j];
            j--;
        }
        phonebook[j + 1] = key;
    }
}

// Бинарный поиск по старшей части ключа (фамилии или телефону)
int binarySearch(Abonent *phonebook, int size, const char *key) {
    int left = 0, right = size - 1;
    int result = -1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp;
        
        if (sortType == SORT_BY_NAME) {
            cmp = strcmp(phonebook[mid].surname, key);
        } else {
            cmp = strcmp(phonebook[mid].phone, key);
        }
        
        // Учитываем направление сортировки
        if (sortDirection == DESCENDING) {
            cmp = -cmp;
        }
        
        if (cmp == 0) {
            result = mid;
            right = mid - 1; // Ищем первое вхождение
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return result;
}

void printPhonebook(Abonent *phonebook, int size) {
    printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
    printf("------------------------------------------------\n");
    for (int i = 0; i < size; i++) {
        printf("%-15s %-15s %-15s %d\n", 
               phonebook[i].surname, 
               phonebook[i].name, 
               phonebook[i].phone, 
               phonebook[i].age);
    }
    printf("\n");
}

int main() {
    // Исходные данные
    Abonent phonebook[] = {
        {"Браун", "Тимур", "123-456-789", 19},
        {"Петров", "Иван", "987-654-321", 30},
        {"Сидоров", "Петр", "555-555-555", 22},
        {"Бобров", "Сергей", "111-222-333", 28},
        {"Петров", "Алексей", "444-444-444", 35},
        {"Иванов", "Андрей", "123-456-789", 25}  // Добавим запись с одинаковым телефоном
    };
    int size = sizeof(phonebook) / sizeof(phonebook[0]);

    printf("Исходная телефонная книга:\n");
    printPhonebook(phonebook, size);

    // Выбор типа сортировки
    printf("Выберите тип сортировки:\n");
    printf("1 - По фамилии и имени\n");
    printf("2 - По номеру телефона и возрасту\n");
    int type;
    scanf("%d", &type);
    sortType = (type == 2) ? SORT_BY_PHONE_AGE : SORT_BY_NAME;

    // Выбор направления сортировки
    printf("Выберите направление сортировки:\n");
    printf("1 - По возрастанию\n");
    printf("2 - По убыванию\n");
    int direction;
    scanf("%d", &direction);
    sortDirection = (direction == 2) ? DESCENDING : ASCENDING;

    // Сортировка
    insertionSort(phonebook, size);

    printf("\nОтсортированная телефонная книга:\n");
    printPhonebook(phonebook, size);

    // Бинарный поиск
    printf("\nПоиск абонента по %s: ", 
           (sortType == SORT_BY_NAME) ? "фамилии" : "номеру телефона");
    
    char searchKey[50];
    scanf("%s", searchKey);
    
    int index = binarySearch(phonebook, size, searchKey);
    
    if (index != -1) {
        printf("\nНайденные абоненты:\n");
        printf("%-15s %-15s %-15s %s\n", "Фамилия", "Имя", "Телефон", "Возраст");
        printf("------------------------------------------------\n");
        
        // Выводим все совпадения (так как их может быть несколько)
        while (index < size) {
            int cmp;
            if (sortType == SORT_BY_NAME) {
                cmp = strcmp(phonebook[index].surname, searchKey);
            } else {
                cmp = strcmp(phonebook[index].phone, searchKey);
            }
            
            if ((sortDirection == ASCENDING && cmp != 0) || 
                (sortDirection == DESCENDING && cmp != 0)) {
                break;
            }
            
            printf("%-15s %-15s %-15s %d\n", 
                   phonebook[index].surname, 
                   phonebook[index].name, 
                   phonebook[index].phone, 
                   phonebook[index].age);
            index++;
        }
    } else {
        printf("Абонент не найден.\n");
    }

    return 0;
}