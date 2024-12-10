#include <stdio.h>
void main() {
    unsigned int a;
    scanf("%u", &a);
for(int i = 0; i < 32; i++) {
    if (i % 2 ==0)
    printf("%d", (a >> i) & 1);
}
printf("  ");

for(int i = 1; i < 32; i++) {
    printf("%d", (a << i) & 1);
}
}