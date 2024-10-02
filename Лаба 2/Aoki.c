#include <stdio.h>
#include <math.h>
void main () {
  float x;
  char y;

  printf("Input temperature: ");
  scanf("%f%c", &x,&y);

  if (y == 'c' || y == 'C') {
  printf("Output temperature: %ff\n", (x * 1.8) + 32);
}
 else if (y == 'f' || y == 'F') {
  printf("Output temperature: %fc\n", (x - 32)/1.8);
}
 else {
  printf("Output error");

}

}
