 // factorial.c
#include "factorial.h"

long long factorial(int n) {
    if (n < 0) {
        // В реальном коде можно бросить ошибку или вернуть индикатор ошибки
        return -1; // Пример: отрицательный факториал не определен
    }
    if (n == 0 || n == 1) {
        return 1;
    }
    long long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}