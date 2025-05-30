#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define EMPTY INT_MIN

/* Хеш-функция */
int hash(int key, int m) {
    int h = key % m;
    return (h < 0) ? h + m : h;
}

/* Вставка методом линейной пробы */
void insert_linear(int *T, int m, int key, int *collisions) {
    int i = 0, idx;
    idx = hash(key, m);
    while (T[idx] != EMPTY) {
        (*collisions)++;
        i++;
        idx = (idx + 1) % m;
    }
    T[idx] = key;
}

/* Вставка методом квадратичной пробы */
void insert_quadratic(int *T, int m, int key, int *collisions) {
    int i = 0, idx, h0 = hash(key, m);
    do {
        idx = (h0 + i*i) % m;
        if (T[idx] == EMPTY) {
            T[idx] = key;
            return;
        }
        (*collisions)++;
        i++;
    } while (i < m);
    /* Если таблица полна — не вставляем */
}

/* Печать хеш-таблицы */
void print_table(int *T, int m) {
    printf("idx:\t");
    for (int i = 0; i < m; i++)
        printf("%4d", i);
    printf("\nval:\t");
    for (int i = 0; i < m; i++) {
        if (T[i] == EMPTY) printf("   .");
        else               printf("%4d", T[i]);
    }
    printf("\n");
}

/* Поиск в таблице линейной пробой */
int search_linear(int *T, int m, int key) {
    int i = 0, idx = hash(key, m);
    while (i < m && T[idx] != EMPTY) {
        if (T[idx] == key) return idx;
        i++;
        idx = (idx + 1) % m;
    }
    return -1;
}

/* Поиск в таблице квадратичной пробой */
int search_quadratic(int *T, int m, int key) {
    int i = 0, idx, h0 = hash(key, m);
    while (i < m) {
        idx = (h0 + i*i) % m;
        if (T[idx] == EMPTY) break;
        if (T[idx] == key) return idx;
        i++;
    }
    return -1;
}

int main() {
    int n, m;
    printf("Введите число ключей n: ");
    if (scanf("%d", &n) != 1 || n <= 0) return 0;

    int *keys = malloc(n * sizeof(int));
    printf("Введите %d целых ключей:\n", n);
    for (int i = 0; i < n; i++)
        scanf("%d", &keys[i]);

    printf("Введите размер хеш-таблицы m: ");
    scanf("%d", &m);
    if (m <= 0) return 0;

    /* Выделяем и инициализируем две таблицы */
    int *T_lin = malloc(m * sizeof(int));
    int *T_quad = malloc(m * sizeof(int));
    for (int i = 0; i < m; i++) {
        T_lin[i]  = EMPTY;
        T_quad[i] = EMPTY;
    }

    int c_lin = 0, c_quad = 0;
    /* Пункт 2: заполнение таблиц и подсчет коллизий */
    for (int i = 0; i < n; i++) {
        insert_linear(T_lin, m, keys[i], &c_lin);
        insert_quadratic(T_quad, m, keys[i], &c_quad);
    }

    printf("\n— Заполненные таблицы:\n");
    printf("Линейная проба (коллизий = %d):\n", c_lin);
    print_table(T_lin, m);
    printf("Квадратичная проба (коллизий = %d):\n", c_quad);
    print_table(T_quad, m);

    /* Пункт 3: эксперимент для 10 простых чисел */
    int primes[10] = {11,17,23,29,37,43,53,61,73,101};
    printf("\n— Сравнение коллизий для разных размеров:\n");
    printf(" m  |   n  | линейн. | квадр.\n");
    printf("--------------------------------\n");
    for (int pi = 0; pi < 10; pi++) {
        int pm = primes[pi];
        int *A = malloc(pm * sizeof(int));
        int *B = malloc(pm * sizeof(int));
        for (int i = 0; i < pm; i++) {
            A[i] = EMPTY;
            B[i] = EMPTY;
        }
        int cc_lin = 0, cc_quad = 0;
        for (int i = 0; i < n; i++) {
            insert_linear(A, pm, keys[i], &cc_lin);
            insert_quadratic(B, pm, keys[i], &cc_quad);
        }
        printf("%3d | %4d | %7d | %7d\n",
               pm, n, cc_lin, cc_quad);
        free(A); free(B);
    }

    /* Пункт 4: поиск элемента */
    int k;
    printf("\nВведите ключ для поиска: ");
    if (scanf("%d", &k) == 1) {
        int pos1 = search_linear(T_lin, m, k);
        if (pos1 >= 0)
            printf("Линейная проба: найден в ячейке %d\n", pos1);
        else
            printf("Линейная проба: не найден\n");

        int pos2 = search_quadratic(T_quad, m, k);
        if (pos2 >= 0)
            printf("Квадратичная проба: найден в ячейке %d\n", pos2);
        else
            printf("Квадратичная проба: не найден\n");
    }

    free(keys);
    free(T_lin);
    free(T_quad);
    return 0;
}
