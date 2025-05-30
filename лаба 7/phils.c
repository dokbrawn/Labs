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
