#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Узел списка для метода цепочек */
typedef struct Node {
    int key;
    struct Node *next;
} Node;

/* Создать пустую хеш-таблицу размера m */
Node **create_table(int m) {
    Node **table = malloc(m * sizeof(Node*));
    for (int i = 0; i < m; i++)
        table[i] = NULL;
    return table;
}

/* Освободить память под хеш-таблицу */
void free_table(Node **table, int m) {
    for (int i = 0; i < m; i++) {
        Node *cur = table[i];
        while (cur) {
            Node *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }
    free(table);
}

/* Хеш-функция: остаток от деления */
int hash(int key, int m) {
    int h = key % m;
    return (h < 0) ? h + m : h;
}

/* Вставка ключа с учётом коллизий */
void insert_key(Node **table, int m, int key, int *collisions) {
    int idx = hash(key, m);
    if (table[idx] != NULL)
        (*collisions)++;
    Node *node = malloc(sizeof(Node));
    node->key = key;
    node->next = table[idx];
    table[idx] = node;
}

/* Распечатать списки одной хеш-таблицы */
void print_table(Node **table, int m) {
    for (int i = 0; i < m; i++) {
        printf("[%2d]:", i);
        for (Node *cur = table[i]; cur; cur = cur->next)
            printf(" %d", cur->key);
        printf("\n");
    }
}

/* Подсчитать коллизии без вывода, нужен для эксперимента */
int count_collisions(int *keys, int n, int m) {
    Node **tbl = create_table(m);
    int collisions = 0;
    for (int i = 0; i < n; i++)
        insert_key(tbl, m, keys[i], &collisions);
    free_table(tbl, m);
    return collisions;
}

int main() {
    int n;
    printf("Введите количество ключей: ");
    if (scanf("%d", &n) != 1 || n <= 0) return 0;

    int *keys = malloc(n * sizeof(int));
    if (keys == NULL) {
        fprintf(stderr, "Ошибка: не удалось выделить память для ключей.\n");
        return 1;
    }
    printf("Введите %d целых ключей:\n", n);
    for (int i = 0; i < n; i++)
        if (scanf("%d", &keys[i]) != 1) {
            fprintf(stderr, "Ошибка: некорректный ввод ключа.\n");
            free(keys);
            return 1;
        }

    int m0;
    if (n > 1) {
        m0 = ceil(n / log2(n));
    } else {
        fprintf(stderr, "Ошибка: n должно быть больше 1 для вычисления n/log2(n).\n");
        free(keys);
        return 1;
    }
    printf("\n-- Хеш-таблица размера m = %d (n/log2(n)) --\n", m0);

    Node **table = create_table(m0);
    int collisions0 = 0;
    for (int i = 0; i < n; i++) {
        insert_key(table, m0, keys[i], &collisions0);
    }
    printf("Количество вставок: %d, коллизий: %d\n", n, collisions0);

    printf("Построенные списки:\n");
    print_table(table, m0);
    printf("Количество вставок: %d, коллизий: %d\n", n, collisions0);

    /* 3. Эксперимент для 10 простых чисел от 11 до 101 */
    /* Prime numbers chosen to minimize collisions and ensure good distribution */
    int primes[10] = {11, 17, 23, 29, 37, 43, 53, 61, 73, 101};
    printf("\n-- Зависимость коллизий от размера таблицы --\n");
    printf(" Размер таблицы\t| Исх.символы\t| Коллизии\n");
    printf("-----------------------------\n");
    for (int i = 0; i < 10; i++) {
        int m = primes[i];
        int c = count_collisions(keys, n, m);
        printf("%3d\t| %d\t| %d\n", m, n, c);
    }
    /* 4. Поиск заданного ключа */
    int key_search;
    printf("\nВведите ключ для поиска: ");
    if (scanf("%d", &key_search) == 1) {
        int idx = hash(key_search, m0);
        Node *cur = table[idx];
        int pos = 1;
        while (cur) {
            if (cur->key == key_search) {
                printf("Ключ %d найден в списке %d на позиции %d\n", key_search, idx, pos);
                break;
            }
            cur = cur->next;
            pos++;
        }
        if (!cur) {
            printf("Ключ %d не найден в списке %d\n", key_search, idx);
        }
    }

    free_table(table, m0);
    free(keys);
    return 0;
}
