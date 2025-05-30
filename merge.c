#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

typedef struct ListElement {
    int value;
    struct ListElement* next;
} ListElement;

// Базовые операции со списком
ListElement* createElement(int val) {
    ListElement* newElem = (ListElement*)malloc(sizeof(ListElement));
    newElem->value = val;
    newElem->next = NULL;
    return newElem;
}

void addToList(ListElement** head, int val) {
    ListElement* newElem = createElement(val);
    if (*head == NULL) {
        *head = newElem;
        return;
    }
    ListElement* current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newElem;
}

void displayList(ListElement* head) {
    while (head != NULL) {
        printf("%d ", head->value);
        head = head->next;
    }
    printf("\n");
}

int getLength(ListElement* head) {
    int count = 0;
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}

long long computeSum(ListElement* head) {
    long long total = 0;
    while (head != NULL) {
        total += head->value;
        head = head->next;
    }
    return total;
}

// Разделение списка на две части
void divideList(ListElement* source, ListElement** firstHalf, ListElement** secondHalf) {
    ListElement* fastPointer;
    ListElement* slowPointer = source;
    
    if (source == NULL || source->next == NULL) {
        *firstHalf = source;
        *secondHalf = NULL;
    } else {
        fastPointer = source->next;
        while (fastPointer != NULL) {
            fastPointer = fastPointer->next;
            if (fastPointer != NULL) {
                slowPointer = slowPointer->next;
                fastPointer = fastPointer->next;
            }
        }
        *firstHalf = source;
        *secondHalf = slowPointer->next;
        slowPointer->next = NULL;
    }
}

// Слияние двух отсортированных списков
ListElement* mergeLists(ListElement* list1, ListElement* list2, int* cmpCount, int* opCount) {
    ListElement* mergedList = NULL;
    
    if (list1 == NULL) return list2;
    if (list2 == NULL) return list1;
    
    (*cmpCount)++;
    if (list1->value <= list2->value) {
        mergedList = list1;
        (*opCount)++;
        mergedList->next = mergeLists(list1->next, list2, cmpCount, opCount);
    } else {
        mergedList = list2;
        (*opCount)++;
        mergedList->next = mergeLists(list1, list2->next, cmpCount, opCount);
    }
    return mergedList;
}

// Сортировка слиянием
void sortByMerge(ListElement** headRef, int* cmpCount, int* opCount) {
    ListElement* head = *headRef;
    ListElement* part1;
    ListElement* part2;
    
    if (head == NULL || head->next == NULL) return;
    
    divideList(head, &part1, &part2);
    
    sortByMerge(&part1, cmpCount, opCount);
    sortByMerge(&part2, cmpCount, opCount);
    
    *headRef = mergeLists(part1, part2, cmpCount, opCount);
}

// Слияние серий (итеративное)
ListElement* mergeSeries(ListElement* list1, ListElement* list2, int* cmpCount, int* opCount) {
    ListElement dummy; // Временный узел для упрощения работы
    ListElement* tail = &dummy;
    dummy.next = NULL;

    while (list1 != NULL && list2 != NULL) {
        (*cmpCount)++;
        if (list1->value <= list2->value) {
            tail->next = list1;
            list1 = list1->next;
        } else {
            tail->next = list2;
            list2 = list2->next;
        }
        (*opCount)++;
        tail = tail->next;
    }

    // Присоединяем оставшиеся элементы
    if (list1 != NULL) {
        tail->next = list1;
    } else {
        tail->next = list2;
    }

    return dummy.next;
}

// Генерация тестовых данных
ListElement* generateTestData(int size, int dataType) {
    ListElement* head = NULL;
    if (dataType == 0) { // Возрастающая последовательность
        for (int i = 1; i <= size; i++) addToList(&head, i*2);
    } else if (dataType == 1) { // Убывающая последовательность
        for (int i = size; i >= 1; i--) addToList(&head, i*3);
    } else { // Случайные данные
        srand(time(NULL));
        for (int i = 0; i < size; i++) addToList(&head, rand() % 1500 + 500);
    }
    return head;
}

void runSortTest(int size, int dataType) {
    ListElement* testList = generateTestData(size, dataType);
    int comparisons = 0, operations = 0;
    
    printf("Исходные данные (%d элементов): ", size);
    if (size <= 25) displayList(testList);
    else printf("(пропущен длинный вывод)\n");
    
    sortByMerge(&testList, &comparisons, &operations);
    
    printf("После сортировки: ");
    if (size <= 25) displayList(testList);
    else printf("(пропущен длинный вывод)\n");
    
    printf("Сумма значений: %lld\n", computeSum(testList));
    printf("Сравнений: %d, Операций: %d\n", comparisons, operations);
    printf("Общая трудоемкость: %d\n", operations + comparisons);
    
    int complexity = size * (int)(log(size)/log(2));
    printf("Теоретическая оценка (n log n): %d\n\n", complexity);
    
    // Освобождение памяти
    while (testList != NULL) {
        ListElement* temp = testList;
        testList = testList->next;
        free(temp);
    }
}

// Тестирование производительности
void runPerformanceTests() {
    printf("\nТрудоемкость сортировки прямого слияния\n");
    printf("------------------------------------------------------------\n");
    printf("| Размер | Теория (n log n) |  Возр.  | Убыв.  | Случ.  |\n");
    printf("------------------------------------------------------------\n");
        
    int testSizes[] = {100, 200, 300, 400, 500};
    for (int i = 0; i < 5; i++) {
        int size = testSizes[i];
        int complexity = size * (int)(log(size)/log(2));
        
        printf("| %6d | %16d |", size, complexity);
            
        // Тестирование для разных типов данных
        for (int type = 1; type >= 0; type--) {
            ListElement* testList = generateTestData(size, type);
            int cmp = 0, ops = 0;
                
            sortByMerge(&testList, &cmp, &ops);
            printf(" %7d |", ops + cmp);
                
            // Освобождение памяти
            while (testList != NULL) {
                ListElement* temp = testList;
                testList = testList->next;
                free(temp);
            }
        }
            
        // Случайные данные
        ListElement* testList = generateTestData(size, 2);
        int cmp = 0, ops = 0;
            
        sortByMerge(&testList, &cmp, &ops);
        printf(" %7d |\n", ops + cmp);
        printf("------------------------------------------------------------\n");
            
        // Освобождение памяти
        while (testList != NULL) {
            ListElement* temp = testList;
            testList = testList->next;
            free(temp);
        }
    }
}

int main() {
    // Пример сортировки символов
    char sampleText[] = "ПримерТекста";
    int length = strlen(sampleText);
    
    ListElement* charList = NULL;
    for (int i = 0; i < length; i++) {
        addToList(&charList, (int)sampleText[i]);
    }
    
    printf("1. Сортировка символов:\n");
    printf("Исходный текст: %s\n", sampleText);
    printf("Коды символов: ");
    displayList(charList);
    
    int cmp = 0, ops = 0;
    sortByMerge(&charList, &cmp, &ops);
    
    printf("После сортировки: ");
    displayList(charList);
    printf("\n");
    
    // Освобождение памяти
    while (charList != NULL) {
        ListElement* temp = charList;
        charList = charList->next;
        free(temp);
    }
    
    // Тестирование разделения списка
    printf("2. Тест разделения списка:\n");
    ListElement* testList = generateTestData(15, 2);
    printf("Исходный список (15 элементов):\n");
    displayList(testList);
    
    ListElement* part1 = NULL;
    ListElement* part2 = NULL;
    divideList(testList, &part1, &part2);
    
    printf("Первая часть (%d элементов): ", getLength(part1));
    displayList(part1);
    printf("Вторая часть (%d элементов): ", getLength(part2));
    displayList(part2);
    printf("\n");
    
    // Освобождение памяти
    free(testList);
    free(part1);
    free(part2);
    
    // Тестирование сортировки
    printf("3-4. Тестирование сортировки:\n");
    runSortTest(15, 0); // Возрастающие
    runSortTest(15, 1); // Убывающие
    runSortTest(15, 2); // Случайные
    
    // Тестирование производительности
    printf("\n5. Тесты производительности:\n");
    runPerformanceTests();
    
    return 0;
}