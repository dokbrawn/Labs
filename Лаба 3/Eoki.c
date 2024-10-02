#include <stdio.h>
#include <math.h>
void main() {
        int n;
        int a;
	scanf("%d",&a);
	for (n=2; n<a; n++)
	    if (a%n==0) {
		break;
}
	if (n==a) {
 		puts("1");
}
	else {
 		puts("0");
}

}
