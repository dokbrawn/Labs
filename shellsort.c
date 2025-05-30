#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int random_int(int N) { return rand() % N; }

void FillInc(int A[], int n);
void FillDec(int A[], int n);
void FillRand(int A[], int n);
void CheckSum(int A[], int n);
void RunNumber(int A[], int n);
void PrintMas(int A[], int n);
void InsertSort(int A[], int n);
void ShellSort(int A[], int n);
void FunkDec(int n, int i);
void FunkInc(int n, int i);
void FunkRand(int n, int i);
void TablResult(int n);

int Mprak = 0, Cprak = 0;
int K_Sort(int n);

int main() {
    srand(time(NULL));
    int n = 10, A[10];
    
    FillRand(A, n);
    PrintMas(A, n);
    CheckSum(A, n);
    RunNumber(A, n);
    printf("M(teor)=%d, C(teor)=%d \n", ((n * n - n) / 2) + 2 * n - 2, (n * n - n) / 2);
    ShellSort(A, n);
    PrintMas(A, n);
    CheckSum(A, n);
    RunNumber(A, n);
    printf("M(prak)=%d, C(prak)=%d \n", Mprak, Cprak);
    printf("\n\n");
    TablResult(100);
    printf("\n\n");
    return 0;
}

void FillInc(int A[], int n) {
    for (int i = 0; i < n; i++) A[i] = i + 1;
}

void FillDec(int A[], int n) {
    for (int i = 0; i < n; i++) A[i] = n - i;
}

void FillRand(int A[], int n) {
    int a = 0, b = 10;
    for (int i = 0; i < n; i++) A[i] = random_int(b - a + 1) + a;
}

void CheckSum(int A[], int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += A[i];
    printf("\n CheckSum=%d", s);
}

void RunNumber(int A[], int n) {
    int c = 1;
    for (int i = 1; i < n; i++) if (A[i - 1] > A[i]) c++;
    printf("\n RunNumber=%d\n", c);
}

void PrintMas(int A[], int n) {
    for (int i = 0; i < n; i++) printf("%d ", A[i]);
    printf("\n");
}

void TablResult(int n) {
    printf("|  n  | K-Sort | Insert |  Shell |");
    for (; n <= 600; n += 100) {
        printf("\n| %3d |", n);
        printf("   %2d   |", K_Sort(n));
        for (int i = 0; i < 2; i++) {
            FunkRand(n, i);
            printf(" %6d |", Cprak + Mprak);
        }
    }
}

void FunkDec(int n, int i) {
    Mprak = 0; Cprak = 0;
    int A[n];
    FillDec(A, n);
    if (i == 0) InsertSort(A, n);
    else ShellSort(A, n);
}

void FunkInc(int n, int i) {
    Mprak = 0; Cprak = 0;
    int A[n];
    FillInc(A, n);
    if (i == 0) InsertSort(A, n);
    else ShellSort(A, n);
}

void FunkRand(int n, int i) {
    Mprak = 0; Cprak = 0;
    int A[n];
    FillRand(A, n);
    if (i == 0) InsertSort(A, n);
    else ShellSort(A, n);
}

void InsertSort(int A[], int n) {
    Mprak = 0, Cprak = 0;
    for (int i = 1; i < n; i++) {
        Mprak++;
        int t = A[i];
        int j = i - 1;
        while (j >= 0 && (Cprak++, t < A[j])) {
            Mprak++;
            A[j + 1] = A[j];
            j--;
        }
        Mprak++;
        A[j + 1] = t;
    }
}

void ShellSort(int A[], int n) {
    int l = K_Sort(n);
    int K[l];
    K[0] = 1;
    for (int i = 1; i < l; i++) K[i] = K[i - 1] * 2 + 1;
    for (; l > 0; l--) {
        int k = K[l - 1];
        for (int i = k; i < n; i++) {
            int t = A[i]; Mprak++;
            int j = i - k;
            while (j >= 0 && (Cprak++, t < A[j])) {
                A[j + k] = A[j];
                Mprak++;
                j -= k;
            }
            A[j + k] = t; Mprak++;
        }
    }
}

int K_Sort(int n) {
    int i = 2, j = 0;
    while (i <= n) {
        i *= 2;
        j++;
    }
    return j - 1;
}
