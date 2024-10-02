#include <stdio.h>

void main() {

    int x;
    printf("Ведите год: ");
    scanf("%d", &x);


if (x % 400 == 0) {
   printf("YES\n");

} if (x % 100 == 0) {;
   printf("NO\n");
} if (x % 4 == 0) {;
   printf("YES\n");
} else {
   printf("NO\n");
}

}
