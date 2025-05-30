#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Структура узла для стека, очереди и списка
typedef struct Node {
    int data;
    struct Node* next;
} Node;

// Структура стека
typedef struct {
    Node* top;
} Stack;

// Структура очереди
typedef struct {
    Node* front;
    Node* rear;
} Queue;

// 1. Функции для работы со стеком
void stack_init(Stack* s) {
    s->top = NULL;
}

void stack_push(Stack* s, int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = value;
    newNode->next = s->top;
    s->top = newNode;
}

void stack_fill_ascending(Stack* s, int n) {
    for (int i = 1; i <= n; i++) stack_push(s, i);
}

void stack_fill_descending(Stack* s, int n) {
    for (int i = n; i >= 1; i--) stack_push(s, i);
}

void stack_fill_random(Stack* s, int n, int min, int max) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) 
        stack_push(s, rand() % (max - min + 1) + min);
}

// 2. Функции для работы с очередью
void queue_init(Queue* q) {
    q->front = q->rear = NULL;
}

void enqueue(Queue* q, int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = value;
    newNode->next = NULL;
    
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

void queue_fill_ascending(Queue* q, int n) {
    for (int i = 1; i <= n; i++) enqueue(q, i);
}

void queue_fill_descending(Queue* q, int n) {
    for (int i = n; i >= 1; i--) enqueue(q, i);
}

void queue_fill_random(Queue* q, int n, int min, int max) {
    srand(time(NULL));
    for (int i = 0; i < n; i++)
        enqueue(q, rand() % (max - min + 1) + min);
}

// 3. Функции для работы со списком
void list_print(Node* head) {
    Node* current = head;
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");
}

int list_checksum(Node* head) {
    int sum = 0;
    Node* current = head;
    while (current != NULL) {
        sum += current->data;
        current = current->next;
    }
    return sum;
}

int list_count_series(Node* head) {
    if (head == NULL) return 0;
    int count = 1;
    Node* current = head;
    while (current->next != NULL) {
        if (current->data != current->next->data) count++;
        current = current->next;
    }
    return count;
}

// 4. Удаление всех элементов списка
void list_clear(Node** head) {
    Node* current = *head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
    *head = NULL;
}

// 5. Рекурсивная печать
void list_print_forward(Node* head) {
    if (head == NULL) return;
    printf("%d ", head->data);
    list_print_forward(head->next);
}

void list_print_backward(Node* head) {
    if (head == NULL) return;
    list_print_backward(head->next);
    printf("%d ", head->data);
}

int main() {
    // Пример использования
    Stack s;
    stack_init(&s);
    stack_fill_ascending(&s, 5);
    
    Queue q;
    queue_init(&q);
    queue_fill_random(&q, 3, 1, 100);
    
    Node* list = NULL; // Пример списка
    // ... добавление элементов в список ...
    
    return 0;
}
