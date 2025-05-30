#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void* print_messages(void* arg) {
    for (int i = 0; i < 5; i++) {
        printf("Child thread: Message %d\n", i + 1);
    }
    return NULL;
}

int main() {
    pthread_t thread;

    if (pthread_create(&thread, NULL, print_messages, NULL) != 0) {
        perror("Failed to create thread");
        return 1;
    }

    for (int i = 0; i < 5; i++) {
        printf("Parent thread: Message %d\n", i + 1);
    }

    pthread_exit(NULL);
}