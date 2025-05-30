#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    // Проверка количества аргументов
    if (argc < 2) {
        write(STDERR_FILENO, "Использование: ", 28);
        write(STDERR_FILENO, argv[0], strlen(argv[0]));
        write(STDERR_FILENO, " <имя_файла> [строка1] [строка2] ...\n", 46);
        return 1;
    }

    // Создаем файл и записываем аргументы
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Ошибка при создании файла");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s\n", argv[i]); // Добавляем \n
        ssize_t written = write(fd, buffer, strlen(buffer));
        if (written == -1) {
            perror("Ошибка записи");
            close(fd);
            return 1;
        }
    }

    close(fd);

    // Открываем файл для чтения
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка при открытии файла");
        return 1;
    }

    // Читаем и выводим содержимое
    char buffer[1024];
    ssize_t bytes_read;

    write(STDOUT_FILENO, "\nСодержимое файла:\n", 34);
    while ((bytes_read = read(fd, buffer, sizeof(buffer)))) {
        if (bytes_read == -1) {
            perror("Ошибка чтения");
            close(fd);
            return 1;
        }
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    close(fd);
    return 0;
}