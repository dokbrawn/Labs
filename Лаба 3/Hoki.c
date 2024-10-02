#include <stdio.h>
#include <math.h>
void main() {
        int n;
        int a;
        int s; 
	scanf("%d",&a);
	for (s=0,n=2; n<a; n++)
if (a%n==0) {
 s=s+1;
}
if (s==0) {
puts("1");
}
else {
 puts("0");
}

}
