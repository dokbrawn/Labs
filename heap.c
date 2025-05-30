#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int comparisons = 0; // Учет сравнения при сортировке
int moves = 0; // Учет перемещения при сортировке

int build_comparisons = 0;  // Учет сравнения при построении
int build_moves = 0; // Учет перемещения при построении

void FillInc(int n, int arr[])
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = i + 1;
    }
}

void FillDec(int n, int arr[])
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = n - i;
    }
}

void FillRand(int n, int arr[])
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % 1000;
    }
}

int CheckSum(int n, int arr[])
{
    int sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += arr[i];
    }
    return sum;
}

void PrintArray(int n, int arr[])
{
    for (int i = 0; i < n; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

double calculate_moves(int n)
{
    return n * log2(n) + 6.5 * n - 4;
}

double calculate_comparisons(int n)
{
    return 2 * n * log2(n) + n + 2;
}

double calculate_build_moves(int n)
{
    return log2(n) + 2;
}

double calculate_build_comparisons(int n)
{
    return 2 * log2(n) + 1;
}

void HeapBuild(int arr[], int left, int right)
{
    int x = arr[left];
    moves++;
    build_moves++;
    int i = left;
    while (1)
    {
        int j = 2 * i + 1;
        if (j > right)
            break;

        comparisons++;
        build_comparisons++;
        if (j < right && arr[j + 1] > arr[j])
        {
            j++;
        }

        comparisons++;
        build_comparisons++;
        if (x >= arr[j])
            break;

        arr[i] = arr[j];
        moves++;
        build_moves++;
        i = j;
    }
    
    if (i != left) {
        moves++;
        build_moves++;
    }
    moves++;
    build_moves++;
    arr[i] = x;
}

void HeapSort(int arr[], int n)
{
    int temp;
    for (int left = n / 2 - 1; left >= 0; left--)
    {
        HeapBuild(arr, left, n - 1);
    }

    for (int right = n - 1; right > 0; right--)
    {
        temp = arr[0];
        moves++;
        arr[0] = arr[right];
        moves++;
        arr[right] = temp;
        moves++;

        HeapBuild(arr, 0, right - 1);
    }
}

int main()
{
    srand(time(NULL));
    
    printf("Трудоемкость построения пирамиды\n");
    printf("+---------+--------------------------+-----------------------------------------------+\n");
    printf("|         |       Теор.              |               Фактические                  |\n");
    printf("|   Размер|                          +---------------+---------------+---------------+\n");
    printf("| массива |                             |   Убыв.        |   Случ.      | Возр.  |\n");
    printf("+---------+--------------------------+---------------+---------------+---------------+\n");
    
    for (int i = 1; i < 6; i++)
    {
        int n = 100 * i;
        int arr[n];
        int teor_moves = calculate_build_moves(n);
        int teor_comparisons = calculate_build_comparisons(n);
        int teor_total = teor_moves + teor_comparisons;

        FillDec(n, arr);
        HeapBuild(arr, 0, n-1);
        int dec_total = build_comparisons + build_moves;
        build_comparisons = 0;
        build_moves = 0;

        FillRand(n, arr);
        HeapBuild(arr, 0, n-1);
        int rand_total = build_comparisons + build_moves;
        build_comparisons = 0;
        build_moves = 0;

        FillInc(n, arr);
        HeapBuild(arr, 0, n-1);
        int inc_total = build_comparisons + build_moves;
        build_comparisons = 0;
        build_moves = 0;

        printf("| %6d  | %27d | %13d | %13d | %13d |\n", 
               n, teor_total, dec_total, rand_total, inc_total);
    }
    printf("+---------+--------------------------+---------------+---------------+---------------+\n");
    printf("\n\nТрудоемкость пирамидальной сортировки\n");
    printf("+---------+-----------------------------------------------+\n");
    printf("|         |                  Фактические                  |\n");
    printf("|Размер+---------------+---------------+---------------+\n");
    printf("| массива |   Убыв.   |   Случ.   | Возр.  |\n");
    printf("+---------+---------------+---------------+---------------+\n");

    for (int i = 1; i < 6; i++)
    {
        int n = 100 * i;
        int arr[n];
        int teor_moves = calculate_moves(n);
        int teor_comparisons = calculate_comparisons(n);
        int teor_total = teor_moves + teor_comparisons;

        FillDec(n, arr);
        HeapSort(arr, n);
        int dec_total = comparisons + moves;
        comparisons = 0;
        moves = 0;

        FillRand(n, arr);
        HeapSort(arr, n);
        int rand_total = comparisons + moves;
        comparisons = 0;
        moves = 0;

        FillInc(n, arr);
        HeapSort(arr, n);
        int inc_total = comparisons + moves;
        comparisons = 0;
        moves = 0;

        printf("| %6d  | %13d | %13d | %13d |\n", n, dec_total, rand_total, inc_total);
    }
    printf("+---------+---------------+---------------+---------------+\n");

    return 0;
}