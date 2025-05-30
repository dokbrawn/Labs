#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/time.h>

#define MAX_LEN 128
#define MAX_MSG 10
#define MAX_CLIENTS 5
#define MAX_SERVERS 2
#define MAX_N 2500
#define N_PHIL 5

// Глобальные переменные и структуры
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
int sync_turn = 0;

pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_send = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_recv = PTHREAD_COND_INITIALIZER;
bool dropped = false;

// Структура очереди сообщений
typedef struct {
    char msgs[MAX_MSG][MAX_LEN];
    int front, rear, size;
} queue;

queue msg_queue = {.front = 0, .rear = 0, .size = 0};

// Функции для работы с очередью
int msgSend(queue* q, char* msg) {
    pthread_mutex_lock(&q_mutex);
    while (q->size >= MAX_MSG && !dropped) pthread_cond_wait(&cond_send, &q_mutex);
    if (dropped) { pthread_mutex_unlock(&q_mutex); return 0; }
    strncpy(q->msgs[q->rear], msg, MAX_LEN - 1);
    q->rear = (q->rear + 1) % MAX_MSG;
    q->size++;
    pthread_cond_signal(&cond_recv);
    pthread_mutex_unlock(&q_mutex);
    return strlen(msg);
}

int msgRecv(queue* q, char* buf, size_t bufsize) {
    pthread_mutex_lock(&q_mutex);
    while (q->size == 0 && !dropped) pthread_cond_wait(&cond_recv, &q_mutex);
    if (dropped) { pthread_mutex_unlock(&q_mutex); return 0; }
    strncpy(buf, q->msgs[q->front], bufsize - 1);
    q->front = (q->front + 1) % MAX_MSG;
    q->size--;
    pthread_cond_signal(&cond_send);
    pthread_mutex_unlock(&q_mutex);
    return strlen(buf);
}

void msgDrop() {
    pthread_mutex_lock(&q_mutex);
    dropped = true;
    pthread_cond_broadcast(&cond_send);
    pthread_cond_broadcast(&cond_recv);
    pthread_mutex_unlock(&q_mutex);
}

// Функция для измерения времени
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec) * 1000 + (double)(tv.tv_usec) / 1000;
}

// ========== Оценка 3 ==========

// Задача 1-2: Базовые потоки
void* basic_thread_func(void* arg) {
    for (int i = 0; i < 5; i++) {
        printf("Child thread: line %d\n", i+1);
    }
    return NULL;
}

// Задача 3: Параметры потока
void* param_thread_func(void* arg) {
    char** messages = (char**)arg;
    for (int i = 0; messages[i] != NULL; i++) {
        printf("Thread message: %s\n", messages[i]);
        sleep(1);
    }
    return NULL;
}

// Задача 4-5: Завершение потока
void cleanup_handler(void* arg) {
    printf("Cleanup: Thread is being canceled\n");
}

void* cancellable_thread_func(void* arg) {
    pthread_cleanup_push(cleanup_handler, NULL);
    while (1) {
        printf("Thread is running...\n");
        sleep(1);
    }
    pthread_cleanup_pop(0);
    return NULL;
}

// Задача 6: Sleepsort
void* sleep_sort(void* arg) {
    int val = *(int*)arg;
    usleep(val * 10000);
    pthread_mutex_lock(&print_mutex);
    printf("%d ", val);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

// ========== Оценка 4 ==========

// Задача 7: Синхронизированный вывод
void* sync_print_child(void* arg) {
    for (int i = 0; i < 5; i++) {
        pthread_mutex_lock(&sync_mutex);
        while (sync_turn != 1) pthread_cond_wait(&sync_cond, &sync_mutex);
        printf("[Child] line %d\n", i+1);
        sync_turn = 0;
        pthread_cond_signal(&sync_cond);
        pthread_mutex_unlock(&sync_mutex);
    }
    return NULL;
}

// Задача 8-9: Перемножение матриц
int A[MAX_N][MAX_N], B[MAX_N][MAX_N], C[MAX_N][MAX_N];
int N = 0, THREADS = 0;

void* multiply(void* arg) {
    int idx = *(int*)arg;
    int rows = N / THREADS;
    int start = idx * rows;
    int end = (idx == THREADS - 1) ? N : start + rows;
    
    for (int i = start; i < end; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return NULL;
}

void print_matrix(int mat[MAX_N][MAX_N], int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            printf("%d ", mat[i][j]);
        }
        printf("\n");
    }
}

// ========== Оценка 5 ==========

// Задача 10: Чат клиент-сервер
void* client(void* arg) {
    char* name = (char*)arg;
    char msg[MAX_LEN];
    for (int i = 0; i < 5; i++) {
        snprintf(msg, MAX_LEN, "%s: Message %d", name, i+1);
        msgSend(&msg_queue, msg);
        usleep(100000 + rand() % 200000);
    }
    return NULL;
}

void* server(void* arg) {
    char buf[MAX_LEN];
    while (!dropped) {
        if (msgRecv(&msg_queue, buf, MAX_LEN) > 0) {
            printf("[Server received] %s\n", buf);
            usleep(100000 + rand() % 100000);
        }
    }
    return NULL;
}



int main(int argc, char* argv[]) {
    srand(time(NULL));

    // ========== Оценка 3 ==========
    printf("=== Оценка 3 ===\n");
    
    // Задача 1-2: Базовые потоки
    printf("\nЗадача 1-2: Базовые потоки\n");
    pthread_t basic_thread;
    pthread_create(&basic_thread, NULL, basic_thread_func, NULL);
    pthread_join(basic_thread, NULL);
    printf("Main thread: all lines printed after child\n");
    
    // Задача 3: Параметры потока
    printf("\nЗадача 3: Параметры потока\n");
    char* messages1[] = {"Message 1-1", "Message 1-2", "Message 1-3", NULL};
    char* messages2[] = {"Message 2-1", "Message 2-2", NULL};
    char* messages3[] = {"Message 3-1", "Message 3-2", "Message 3-3", "Message 3-4", NULL};
    char* messages4[] = {"Message 4-1", NULL};
    
    pthread_t param_threads[4];
    pthread_create(&param_threads[0], NULL, param_thread_func, messages1);
    pthread_create(&param_threads[1], NULL, param_thread_func, messages2);
    pthread_create(&param_threads[2], NULL, param_thread_func, messages3);
    pthread_create(&param_threads[3], NULL, param_thread_func, messages4);
    
    for (int i = 0; i < 4; i++) {
        pthread_join(param_threads[i], NULL);
    }
    
    // Задача 4-5: Завершение потока
    printf("\nЗадача 4-5: Завершение потока\n");
    pthread_t cancel_thread;
    pthread_create(&cancel_thread, NULL, cancellable_thread_func, NULL);
    sleep(2);
    pthread_cancel(cancel_thread);
    pthread_join(cancel_thread, NULL);
    
    // Задача 6: Sleepsort
    printf("\nЗадача 6: Sleepsort\n");
    int arr[] = {3, 1, 4, 2, 5};
    pthread_t ss[5];
    printf("Original array: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\nSorted array: ");
    for (int i = 0; i < 5; i++) pthread_create(&ss[i], NULL, sleep_sort, &arr[i]);
    for (int i = 0; i < 5; i++) pthread_join(ss[i], NULL);
    printf("\n");
    
    // ========== Оценка 4 ==========
    printf("\n=== Оценка 4 ===\n");
    
    // Задача 7: Синхронизированный вывод
    printf("\nЗадача 7: Синхронизированный вывод\n");
    pthread_t sync_thread;
    pthread_create(&sync_thread, NULL, sync_print_child, NULL);
    
    for (int i = 0; i < 5; i++) {
        pthread_mutex_lock(&sync_mutex);
        while (sync_turn != 0) pthread_cond_wait(&sync_cond, &sync_mutex);
        printf("[Main] line %d\n", i+1);
        sync_turn = 1;
        pthread_cond_signal(&sync_cond);
        pthread_mutex_unlock(&sync_mutex);
    }
    
    pthread_join(sync_thread, NULL);
    
    // Задача 8-9: Перемножение матриц
    if (argc >= 3) {
        printf("\nЗадача 8-9: Перемножение матриц\n");
        N = atoi(argv[1]);
        THREADS = atoi(argv[2]);
        
        if (N <= 0 || N > MAX_N || THREADS <= 0 || THREADS > 128) {
            printf("Invalid matrix size or thread count\n");
            return 1;
        }
        
        // Инициализация матриц
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                A[i][j] = 1;
                B[i][j] = 1;
            }
        }
        
        if (N < 5) {
            printf("Matrix A:\n");
            print_matrix(A, N);
            printf("Matrix B:\n");
            print_matrix(B, N);
        }
        
        pthread_t threads[THREADS];
        int thread_ids[THREADS];
        
        double start_time = get_time_ms();
        
        for (int i = 0; i < THREADS; i++) {
            thread_ids[i] = i;
            pthread_create(&threads[i], NULL, multiply, &thread_ids[i]);
        }
        
        for (int i = 0; i < THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        double end_time = get_time_ms();
        
        if (N < 5) {
            printf("Result matrix C:\n");
            print_matrix(C, N);
        }
        
        printf("Matrix size: %dx%d, Threads: %d, Time: %.2f ms\n", N, N, THREADS, end_time - start_time);
    }
    
    // ========== Оценка 5 ==========
    printf("\n=== Оценка 5 ===\n");
    
    // Задача 10: Чат клиент-сервер
    printf("\nЗадача 10: Чат клиент-сервер\n");
    pthread_t clients[MAX_CLIENTS], servers[MAX_SERVERS];
    char* client_names[MAX_CLIENTS] = {"Client1", "Client2", "Client3", "Client4", "Client5"};
    
    for (int i = 0; i < MAX_SERVERS; i++) {
        pthread_create(&servers[i], NULL, server, NULL);
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_create(&clients[i], NULL, client, client_names[i]);
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(clients[i], NULL);
    }
    
    sleep(1);
    msgDrop();
    
    for (int i = 0; i < MAX_SERVERS; i++) {
        pthread_join(servers[i], NULL);
    }
    
    
    return 0;
}