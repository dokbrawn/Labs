#include <stdio.h>
void main() {
    unsigned int a;
    scanf("%u", &a);
int q = 0;
for(; a > 0; a >>= 1) {
    q += a & 1;
}
printf("%d\n", q);
printf("%d\n", (sizeof(a)*8 - q));    

}