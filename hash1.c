#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define N 1024

typedef struct tData
{
    int key;
    struct tData *next;
} tData;

int collisions = 0;
int uniqueSymbols = 0;

void AddToStack(tData **head, int key)
{
    tData *p = (tData *)malloc(sizeof(tData));
    if (p == NULL)
    {
        printf("Ошибка выделения памяти\n");
        exit(1);
    }
    p->key = key;
    p->next = *head;
    *head = p;
}

void insertToHash(tData **table, int tableSize, int key)
{
    int index = key % tableSize;
    tData *head = table[index];
    int isNewKey = 1;

    tData *current = head;
    while (current != NULL)
    {
        if (current->key == key)
        {
            isNewKey = 0;
            break;
        }
        current = current->next;
    }

    if (isNewKey)
    {
        if (head != NULL)
        {
            collisions++;
        }
        uniqueSymbols++;
        AddToStack(&table[index], key);
    }
}

void searchInHash(tData **table, int tableSize, int key)
{
    int index = key % tableSize;
    int position = 0;
    tData *current = table[index];

    printf("Поиск символа '%c' (код %d):\n", key, key);
    printf("Номер списка: %d\n", index);

    while (current != NULL)
    {
        if (current->key == key)
        {
            printf("Элемент найден на позиции %d в списке\n", position);
            return;
        }
        current = current->next;
        position++;
    }

    printf("Элемент не найден в хеш-таблице\n");
}

void freeTable(tData **table, int tableSize)
{
    for (int i = 0; i < tableSize; i++)
    {
        tData *current = table[i];
        while (current)
        {
            tData *tmp = current;
            current = current->next;
            free(tmp);
        }
    }
    free(table);
}

void generateRandomText(char *buffer, size_t size)
{
    // Фиксированный набор из 40 уникальных символов
    const char uniqueChars[] = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%";
    
    // Заполняем текст случайными символами из этого набора
    for (size_t i = 0; i < size - 1; i++)
    {
        buffer[i] = uniqueChars[rand() % 40];
    }
    buffer[size - 1] = '\0';
}

void printHashTable(tData **table, int tableSize)
{
    printf("\nСодержимое хеш-таблицы (размер %d):\n", tableSize);
    printf("----------------------------------\n");
    for (int i = 0; i < tableSize; i++)
    {
        if (table[i] != NULL)
        {
            printf("[%3d]: ", i);
            tData *current = table[i];
            while (current != NULL)
            {
                printf("%c ", current->key);
                current = current->next;
            }
            printf("\n");
        }
    }
    printf("----------------------------------\n");
    printf("Уникальных символов: %d\n", uniqueSymbols);
    printf("Количество коллизий: %d\n", collisions);
}

int main()
{
    srand(time(NULL));

    // Более равномерно распределенные простые числа до 100
    int primes[] = {11, 17, 23, 29, 37, 43, 53, 59, 67, 73, 79, 89, 97};
    int primesCount = sizeof(primes) / sizeof(primes[0]);

    char *text = (char *)malloc(N + 1);
    generateRandomText(text, N + 1);

    printf("\nИсследование зависимости коллизий от размера хеш-таблицы\n");
    printf("===================================================\n");
    printf("| Размер таблицы | Уникальных символов | Коллизии |\n");
    printf("---------------------------------------------------\n");

    for (int p = 0; p < primesCount; p++)
    {
        int tableSize = primes[p];
        collisions = 0;
        uniqueSymbols = 0;

        tData **table = (tData **)calloc(tableSize, sizeof(tData *));
        if (!table)
        {
            printf("Ошибка выделения памяти для таблицы.\n");
            free(text);
            return 1;
        }

        for (int i = 0; i < N && text[i] != '\0'; i++)
        {
            insertToHash(table, tableSize, (int)text[i]);
        }

        printf("| %14d | %19d | %8d |\n", tableSize, uniqueSymbols, collisions);

        freeTable(table, tableSize);
    }
    printf("==================================================\n");
    
    // Измененный размер таблицы для демонстрации на 39
    int demoSize = 39;
    collisions = 0;
    uniqueSymbols = 0;
    tData **demoTable = (tData **)calloc(demoSize, sizeof(tData *));
    for (int i = 0; i < N && text[i] != '\0'; i++)
    {
        insertToHash(demoTable, demoSize, (int)text[i]);
    }

    printHashTable(demoTable, demoSize);

    printf("\nДемонстрация поиска элементов:\n");

    for (int i = 0; i < 5; i++)
    {
        int randomIndex = rand() % N;
        char searchChar = text[randomIndex];
        searchInHash(demoTable, demoSize, (int)searchChar);
        printf("---\n");
    }

    printf("Поиск символа, которого нет в таблице:\n");
    searchInHash(demoTable, demoSize, (int)'$');
    printf("---\n");

    freeTable(demoTable, demoSize);
    free(text);
    return 0;
}