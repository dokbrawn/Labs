#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char destination[50];
    int train_number;
    int hours;
    int minutes;
} Train;

void add_records() {
    FILE* file = fopen("trains.dat", "wb");
    if (!file) {
        printf("Error creating file!\n");
        exit(1);
    }

    int n;
    printf("How many records to add? ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++) {
        Train train;
        printf("\nRecord %d:\n", i + 1);

        printf("Destination: ");
        scanf("%s", train.destination);

        printf("Train number: ");
        scanf("%d", &train.train_number);

        printf("Departure time (hours minutes): ");
        scanf("%d %d", &train.hours, &train.minutes);

        fwrite(&train, sizeof(Train), 1, file);
    }
    fclose(file);
}

void search_records() {
    FILE* file = fopen("trains.dat", "rb");
    if (!file) {
        printf("File not found! Add records first.\n");
        return;
    }

    printf("\nSearch by:\n");
    printf("1. Destination\n");
    printf("2. Train number\n");
    printf("3. Departure time\n");
    printf("Your choice: ");
    
    int choice;
    scanf("%d", &choice);

    Train train;
    int found = 0;

    if (choice == 1) {
        char query[50];
        printf("Enter destination: ");
        scanf("%s", query);

        while (fread(&train, sizeof(Train), 1, file)) {
            if (strcmp(train.destination, query) == 0) {
                printf("\nTrain found:\nDestination: %s\nNumber: %d\nTime: %02d:%02d\n",
                       train.destination, train.train_number, train.hours, train.minutes);
                found = 1;
            }
        }
    }
    else if (choice == 2) {
        int number;
        printf("Enter train number: ");
        scanf("%d", &number);

        while (fread(&train, sizeof(Train), 1, file)) {
            if (train.train_number == number) {
                printf("\nTrain found:\nDestination: %s\nNumber: %d\nTime: %02d:%02d\n",
                       train.destination, train.train_number, train.hours, train.minutes);
                found = 1;
            }
        }
    }
    else if (choice == 3) {
        int h, m;
        printf("Enter time (hours minutes): ");
        scanf("%d %d", &h, &m);

        while (fread(&train, sizeof(Train), 1, file)) {
            if (train.hours == h && train.minutes == m) {
                printf("\nTrain found:\nDestination: %s\nNumber: %d\nTime: %02d:%02d\n",
                       train.destination, train.train_number, train.hours, train.minutes);
                found = 1;
            }
        }
    }
    else {
        printf("Invalid choice!\n");
    }

    if (!found) {
        printf("\nNo matches found!\n");
    }
    fclose(file);
}

int main() {
    add_records();
    search_records();
    return 0;
}