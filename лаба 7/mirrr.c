#include <stdio.h>
#include <string.h>



int main() {
    char s[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    char *word;
    char longest_word[100] = "";
    int max_length = 0;

    // Разделяем текст на слова
    word = strtok(s, " ,.-");
    while (word != NULL) {
        int length = strlen(word);
        if (length > max_length) {
            max_length = length;
            strcpy(longest_word, word);
        }
        word = strtok(NULL, " ,.-");
    }
    printf("Longest: %s\n", longest_word);
    for (int i = 0; i < max_length; i++) {
        longest_word[i] = 'x';
    }

    // Вывод результата
    printf("Longest with replacement: %s\n", longest_word);

    return 0;
}