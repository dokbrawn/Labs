#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define LINES 5

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int turn = 0; // 0 — родитель, 1 — поток

void* child_thread(void* arg) {
    for (int i = 0; i < LINES; i++) {
        pthread_mutex_lock(&mutex);
        while (turn != 1)
            pthread_cond_wait(&cond, &mutex);
        printf("Дочерний поток: строка %d\n", i + 1);
        turn = 0;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t thread;

    pthread_create(&thread, NULL, child_thread, NULL);

    for (int i = 0; i < LINES; i++) {
        pthread_mutex_lock(&mutex);
        while (turn != 0)
            pthread_cond_wait(&cond, &mutex);
        printf("Родительский поток: строка %d\n", i + 1);
        turn = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}
