#include <stdio.h>
#include <math.h>
void main() {
        int i;
        int b;
	int n1;
	int y2;
	scanf("%d",&n1);
	scanf("%d",&y2);
		for (i = n1; !(n1 % i == 0 && y2 % i == 0); i--);
		b = i;
        	printf("%d\n", b);
}
