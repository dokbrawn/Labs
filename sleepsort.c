#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define THREAD_COUNT 4
#define MAX_LINES 5

typedef struct {
    char* lines[MAX_LINES];
    int line_count;
} thread_data_t;

// Функция очистки, вызываемая при отмене потока
void cleanup_handler(void* arg) {
    printf("Поток %d завершён через pthread_cancel()\n", *(int*)arg);
}

void* print_lines(void* arg) {
    static int thread_num = 0; // Для сообщения об окончании
    int id = __sync_fetch_and_add(&thread_num, 1); // атомарно увеличиваем
    pthread_cleanup_push(cleanup_handler, &id);

    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < data->line_count; i++) {
        printf("%s\n", data->lines[i]);
        sleep(1);  // Точка отмены
    }

    pthread_cleanup_pop(0); // Удалить обработчик без вызова (поток завершился сам)
    printf("Поток %d завершился сам.\n", id);
    return NULL;
}

int main() {
    pthread_t threads[THREAD_COUNT];
    thread_data_t args[THREAD_COUNT] = {
        { .lines = {"[1] A", "[1] B", "[1] C", "[1] D", "[1] E"}, .line_count = 5 },
        { .lines = {"[2] A", "[2] B", "[2] C", "[2] D", "[2] E"}, .line_count = 5 },
        { .lines = {"[3] A", "[3] B", "[3] C", "[3] D", "[3] E"}, .line_count = 5 },
        { .lines = {"[4] A", "[4] B", "[4] C", "[4] D", "[4] E"}, .line_count = 5 },
    };

    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, print_lines, &args[i]) != 0) {
            perror("Ошибка создания потока");
            exit(1);
        }
    }

    sleep(2);  // Ждём 2 секунды

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_cancel(threads[i]); // Прерываем
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL); // Ждём завершения
    }

    printf("Основной поток завершён.\n");
    return 0;
}
