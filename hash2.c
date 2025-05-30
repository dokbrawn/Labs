#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1024

// Строго заданный текст
const char *FIXED_TEXT =
    "Man is a mystery. It must be unraveled, and if you spend your whole life unraveling it, don?t say you?ve wasted time. I study this mystery because I want to be human. Suffering is the sole origin of consciousness. I refuse to believe that evil is man?s normal state. Everything is in man?s hands, yet he lets it all slip?out of sheer cowardice. That?s an idea for which you could be jailed! Freedom isn?t about unrestraint, but self-mastery. Happiness isn?t found in comfort?it is bought with suffering. Such is the law of our planet, but this direct awareness, felt in life?s flow, is such joy that years of torment are worth it. Man is unhappy because he doesn?t realize he?s happy?that?s all, the whole reason! The moment he knows it, he?ll be happy, right then. Life is dull without moral purpose; mere existence isn?t worth it?even a laborer knows that. So one must have a moral task in life. To fall in love isn?t to love?you can fall in love even while hating. Love is an art demanding the full exertion of one?s soul. Love is so powerful it transforms even ourselves.";
void generateFixedText(char *buffer, size_t size) {
    strncpy(buffer, FIXED_TEXT, size - 1);
    buffer[size - 1] = '\0'; // Убедиться, что строка завершена нулём
}

// Линейная проба
int insertLinear(char *table, int tableSize, char key, int *seen) {
    if (seen[(unsigned char)key]) return 0;
    seen[(unsigned char)key] = 1;

    int h = key % tableSize;
    int collisions = 0;

    while (1) {
        if (table[h] == 0) {
            table[h] = key;
            return collisions;
        }
        collisions++;
        h = (h + 1) % tableSize;
        if (collisions >= tableSize) return collisions;
    }
}

// Квадратичная проба
int insertQuadratic(char *table, int tableSize, char key, int *seen) {
    if (seen[(unsigned char)key]) return 0;
    seen[(unsigned char)key] = 1;

    int h = key % tableSize;
    int d = 1;
    int collisions = 0;

    while (1) {
        if (table[h] == 0) {
            table[h] = key;
            return collisions;
        }
        collisions++;
        if (d > tableSize) return collisions;
        h = (h + d * d) % tableSize;
        d++;
    }
}

// Печать содержимого хэш-таблицы
void printTable(char *table, int tableSize, const char *label) {
    printf("\nСодержимое таблицы (%s):\n", label);
    printf("---------------------------\n");
    printf("| Индекс | Символ | ASCII |\n");
    printf("---------------------------\n");
    for (int i = 0; i < tableSize; i++) {
        if (table[i] != 0)
            printf("| %6d |   %3c   |  %4d |\n", i, table[i], (int)(unsigned char)table[i]);
    }
    printf("---------------------------\n");
}

// Поиск методом линейных проб
int searchLinear(char *table, int tableSize, char key) {
    int h = key % tableSize;
    int steps = 0;

    while (steps < tableSize) {
        if (table[h] == 0)
            return -1;
        if (table[h] == key)
            return h;
        h = (h + 1) % tableSize;
        steps++;
    }
    return -1;
}

// Поиск методом квадратичных проб
int searchQuadratic(char *table, int tableSize, char key) {
    int h = key % tableSize;
    int d = 1;
    int steps = 0;

    while (steps < tableSize) {
        if (table[h] == 0)
            return -1;
        if (table[h] == key)
            return h;
        h = (h + d * d) % tableSize;
        d++;
        steps++;
    }
    return -1;
}

// Демонстрация поиска
void demoSearch(char *table, int size, char key, const char *label,
                int (*searchFunc)(char *, int, char)) {
    int pos = searchFunc(table, size, key);
    if (pos >= 0)
        printf("Поиск символа '%c' (%d) в таблице %s: найден в позиции %d\n", key, key, label, pos);
    else
        printf("Поиск символа '%c' (%d) в таблице %s: не найден\n", key, key, label);
}

int main() {
    // Инициализируем фиксированный текст
    char text[N + 1];
    generateFixedText(text, N + 1);

    // Простые числа больше 35 для размеров таблиц
    int primes[] = {37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
    int primesCount = sizeof(primes) / sizeof(primes[0]);

    printf("Исследование коллизий при открытой адресации:\n");
    printf("=================================================================\n");
    printf("| Размер таблицы | Уникальных | Линейные пробы | Квадратичные пробы |\n");
    printf("-----------------------------------------------------------------\n");

    for (int i = 0; i < primesCount; i++) {
        int size = primes[i];

        char *linearTable = calloc(size, sizeof(char));
        char *quadraticTable = calloc(size, sizeof(char));

        int linearCollisions = 0;
        int quadraticCollisions = 0;
        int seenLinear[256] = {0};
        int seenQuadratic[256] = {0};
        int uniqueSymbols = 0;

        for (int j = 0; j < strlen(text); j++) {
            if (!seenLinear[(unsigned char)text[j]])
                uniqueSymbols++;
            linearCollisions += insertLinear(linearTable, size, text[j], seenLinear);
            quadraticCollisions += insertQuadratic(quadraticTable, size, text[j], seenQuadratic);
        }

        printf("| %14d | %10d | %16d | %16d |\n",
            size, uniqueSymbols, linearCollisions, quadraticCollisions);

        free(linearTable);
        free(quadraticTable);
    }

    printf("=================================================================\n");

    // Демонстрационная таблица размером 35
    int demoSize = 35;
    int n = strlen(text);

    char *linearDemo = calloc(demoSize, sizeof(char));
    char *quadraticDemo = calloc(demoSize, sizeof(char));
    int seenLinearDemo[256] = {0};
    int seenQuadraticDemo[256] = {0};

    for (int i = 0; i < n && i < demoSize * 2; i++) {
        insertLinear(linearDemo, demoSize, text[i], seenLinearDemo);
        insertQuadratic(quadraticDemo, demoSize, text[i], seenQuadraticDemo);
    }

    printTable(linearDemo, demoSize, "Линейные пробы");
    printTable(quadraticDemo, demoSize, "Квадратичные пробы");

    printf("\nДемонстрация поиска символов:\n");

    for (int i = 0; i < 5; i++) {
        char key = text[rand() % strlen(text)];
        demoSearch(linearDemo, demoSize, key, "линейные", searchLinear);
        demoSearch(quadraticDemo, demoSize, key, "квадратичные", searchQuadratic);
        printf("---\n");
    }

    printf("Поиск несуществующего символа '$':\n");
    demoSearch(linearDemo, demoSize, '$', "линейные", searchLinear);
    demoSearch(quadraticDemo, demoSize, '$', "квадратичные", searchQuadratic);

    free(linearDemo);
    free(quadraticDemo);

    return 0;
}