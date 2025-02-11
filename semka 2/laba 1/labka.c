#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define N 100

struct Student {
    char name[100];
    int math;
    int phy;
    int inf;
    int total;
};

struct Student addStudent(const char *name, int math, int inf, int phy){
    struct Student newStudent;
    strncpy(newStudent.name, name, sizeof(newStudent.name) - 1);

    newStudent.name[sizeof(newStudent.name) - 1] = '\0';
    newStudent.math = math;
    newStudent.inf = inf;
    newStudent.phy = phy;
    newStudent.total = math + inf + phy;
    return newStudent;
};

void printStudentInfo(struct Student s) {
    printf("My name is: %s, Math: %d, Physics: %d, Total: %d\n", s.name, s.math, s.phy, s.total);
}

void printStudents(struct Student a[], int n) {
    for (int i = 0; i < n; i++)
    printStudent("%s: %d\n", a[i].name, a[i].total);
}

void Fill