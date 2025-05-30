
# Лабораторная работа №7

## Введение
В данной лабораторной работе изучаются возможности POSIX-потоков: синхронизация, многопоточность, взаимодействие процессов с применением классических задач — обедающие философы, перемножение матриц, producer-consumer и визуализация результатов.

## Структура работы
- **phils.c** — реализация задачи обедающих философов с использованием мьютексов.
- **laba7_full.c** — основные задачи: перемножение матриц в потоках, sleepsort, producer-consumer.
- **plot_results.py** — визуализация зависимости времени от размера матрицы.
- **matrix_mult_results.txt** — лог-файл с замерами времени перемножения.
- **run_tests.sh** — автоматический запуск всех тестов.

## Постановка задачи
- Реализовать задачу обедающих философов с корректной блокировкой.
- Выполнить перемножение матриц с применением многопоточности.
- Замерить время и построить графики зависимости производительности.
- - Написать скрипт для автоматизации тестов и визуализации.

## Ход работы

### Задача обедающих философов (`phils.c`)
```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define PHILO 5
#define DELAY 30000
#define FOOD 50

pthread_mutex_t forks[PHILO];
pthread_t phils[PHILO];
void *philosopher (void *id);
int food_on_table ();
void get_forks (int, int, int);
void down_forks (int, int);
pthread_mutex_t foodlock;

int sleep_seconds = 0;

int main (int argn, char **argv)
{
  int i;
  if (argn == 2)
    sleep_seconds = atoi (argv[1]);

  pthread_mutex_init (&foodlock, NULL);
  for (i = 0; i < PHILO; i++)
    pthread_mutex_init (&forks[i], NULL);
  for (i = 0; i < PHILO; i++)
    pthread_create (&phils[i], NULL, philosopher, (void *)(long)i);
  for (i = 0; i < PHILO; i++)
    pthread_join (phils[i], NULL);
  return 0;
}

void *philosopher (void *num)
{
  int id = (int)(long)num;
  int left_fork = id;
  int right_fork = (id + 1) % PHILO;
  int f;

  printf ("Philosopher %d сидит за столом.\n", id);

  while ((f = food_on_table ())) {
    if (id == 1)
      sleep (sleep_seconds);

    printf ("Философ %d: берёт блюдо %d.\n", id, f);

    // Изменение: сначала берём вилку с меньшим номером, чтобы избежать дедлока
    if (left_fork < right_fork)
      get_forks(id, left_fork, right_fork);
    else
      get_forks(id, right_fork, left_fork);

    printf ("Философ %d: ест.\n", id);
    usleep (DELAY * (FOOD - f + 1));

    down_forks(left_fork, right_fork);
  }

  printf ("Философ %d закончил есть.\n", id);
  return NULL;
}

int food_on_table ()
{
  static int food = FOOD;
  int myfood;

  pthread_mutex_lock (&foodlock);
  if (food > 0) {
    food--;
  }
  myfood = food;
  pthread_mutex_unlock (&foodlock);
  return myfood;
}

void get_forks (int phil, int fork1, int fork2)
{
  pthread_mutex_lock (&forks[fork1]);
  printf ("Философ %d: взял вилку %d.\n", phil, fork1);
  pthread_mutex_lock (&forks[fork2]);
  printf ("Философ %d: взял вилку %d.\n", phil, fork2);
}

void down_forks (int f1, int f2)
{
  pthread_mutex_unlock (&forks[f1]);
  pthread_mutex_unlock (&forks[f2]);
} 

```
- Использованы POSIX-потоки и мьютексы.
- Обеспечена защита от взаимных блокировок.
- В лог выводится поведение философов.

### Перемножение матриц (`laba7_full.c`)
```c
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
```
- Массивы перемножаются в нескольких потоках.
- Используются `pthread_create`, `pthread_join`.
- Результаты сохраняются в `matrix_mult_results.txt`.

### Sleepsort
```c
void* sleep_sort(void* arg) {
    int val = *(int*)arg;
    usleep(val * 10000);
    pthread_mutex_lock(&print_mutex);
    printf("%d ", val);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

```
- Sleepsort реализован с использованием задержек `usleep`.
- Потоки сортируют за счёт задержки, пропорциональной значению.

### Producer–Consumer (Очередь сообщений)
```c
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

// Клиенты пишут сообщения, сервер читает
```
- Очередь реализована вручную.
- Синхронизация через мьютексы и условные переменные.

### Построение графиков (`plot_results.py`)
```python
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Загрузка данных
try:
    df = pd.read_csv('matrix_mult_results.txt')
except FileNotFoundError:
    print("Ошибка: файл 'matrix_mult_results.txt' не найден.")
    exit()

# Переименование колонок на русский
df.rename(columns={
    'N': 'Размер',
    'Threads': 'Потоки',
    'Time_ms': 'Время_мс'
}, inplace=True)

# Убедиться, что "Потоки" — строка для разделения цветов
df['Потоки'] = df['Потоки'].astype(str)

# Настройка стиля
sns.set_theme(style="whitegrid")

# Создание графика
plt.figure(figsize=(12, 8))
sns.lineplot(
    data=df,
    x='Размер',
    y='Время_мс',
    hue='Потоки',
    marker='o',
    palette='viridis',
    linewidth=2.5
)

# Подписи и оформление
plt.title('Производительность перемножения матриц', fontsize=16)
plt.xlabel('Размер матрицы (N)', fontsize=14)
plt.ylabel('Время выполнения (мс)', fontsize=14)
plt.legend(title='Количество потоков', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.grid(True, linestyle='--', alpha=0.7)
plt.tight_layout()

# Отображение
plt.show()

# Чтение matrix_mult_results.txt и построение графиков
```
- График показывает зависимость времени от числа потоков и размера матрицы.

## Скриншот работы
![Результаты выполнения](![alt text](image.png))

## Выводы
- Использование POSIX-потоков позволяет ускорить вычисления.
- Корректная синхронизация — ключ к избежанию гонок и взаимных блокировок.
- Sleepsort наглядно демонстрирует возможности задержек и потоков.
- Python-скрипт успешно визуализирует производительность.
