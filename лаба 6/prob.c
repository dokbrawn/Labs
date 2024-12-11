#include <stdio.h>
#include <math.h>

int o(int num) {
    if (num <= 1) 
    return 0; // число меньше 2 не является простым
    for (int i = 2; i <= sqrt(num); i++) {
        if (num % i == 0) 
        return 0; // делится на число меньше себя, значит не простое
    }
    return 1; // простое число
}

int main() {
    int A[] = {2, 7, 8, 9, 4, 1};
    int n = sizeof(A) / sizeof(A[0]); // Размер массива A

    int B[10];
    int k = 0;
    int primes_A = 0, primes_B = 0, sum_A = 0, sum_B = 0;

    printf("Array A: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", A[i]);
        sum_A += A[i];
        if (o(A[i])) primes_A++;  // Подсчёт простых чисел в A
        if (i % 2 == 1) { 
            B[k++] = A[i];
        }
    }

    printf("\nArray B: ");
    for (int i = 0; i < k; i++) {
        printf("%d ", B[i]);
        sum_B += B[i];
        if (o(B[i])) primes_B++;  // Подсчёт простых чисел в B
    }
    double avg_A = (double)sum_A / n;
    double avg_B = (double)sum_B / k;
    // Вывод количества простых чисел
    printf("\nPrimes in A: %d\n", primes_A);
    printf("Primes in B: %d\n", primes_B);
    printf("Average of A: %.2f\n", avg_A);
    printf("Average of B: %.2f\n", avg_B);

    return 0;
}