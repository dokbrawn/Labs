#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

int **A, **B, **C;
int N, T;

typedef struct {
    int row_start;
    int row_end;
} ThreadData;

void* multiply(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    for (int i = data->row_start; i < data->row_end; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return NULL;
}

void allocate_matrices() {
    A = malloc(N * sizeof(int*));
    B = malloc(N * sizeof(int*));
    C = malloc(N * sizeof(int*));
    for (int i = 0; i < N; i++) {
        A[i] = malloc(N * sizeof(int));
        B[i] = malloc(N * sizeof(int));
        C[i] = malloc(N * sizeof(int));
    }
}

void fill_matrices() {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            A[i][j] = 1;
            B[i][j] = 1;
        }
}

void print_matrix(int** M, const char* name) {
    printf("Матрица %s:\n", name);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            printf("%d ", M[i][j]);
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Использование: %s <размер_матрицы N> <кол-во_потоков T>\n", argv[0]);
        return 1;
    }

    N = atoi(argv[1]);
    T = atoi(argv[2]);

    if (N <= 0 || T <= 0 || T > N) {
        printf("Некорректные параметры.\n");
        return 1;
    }

    allocate_matrices();
    fill_matrices();

    pthread_t threads[T];
    ThreadData thread_data[T];

    struct timeval start, end;
    gettimeofday(&start, NULL);  // Начало замера времени

    int base_rows = N / T;
    int leftover_rows = N % T;

    for (int i = 0; i < T; i++) {
        thread_data[i].row_start = i * base_rows;
        thread_data[i].row_end = (i == T - 1) ? ((i + 1) * base_rows + leftover_rows) : ((i + 1) * base_rows);
        pthread_create(&threads[i], NULL, multiply, &thread_data[i]);
    }

    for (int i = 0; i < T; i++)
        pthread_join(threads[i], NULL);

    gettimeofday(&end, NULL);  // Конец замера времени
    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    // Записываем результаты в CSV файл
    FILE *fp = fopen("result.csv", "a");
    if (fp == NULL) {
        printf("Ошибка при открытии файла result.csv\n");
        return 1;
    }
    fprintf(fp, "%d,%d,%.6f\n", N, T, elapsed_time);
    fclose(fp);

    if (N < 5) {
        print_matrix(A, "A");
        print_matrix(B, "B");
        print_matrix(C, "C");
    }

    for (int i = 0; i < N; i++) {
        free(A[i]);
        free(B[i]);
        free(C[i]);
    }
    free(A); free(B); free(C);

    return 0;
}
