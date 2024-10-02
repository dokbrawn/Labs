#include <stdio.h>
#include <math.h>
void main() {
float x;
float y;
float a = 0.0;
float b = 0.0;
float r = 1.0;
printf("Введите координаты: ");
scanf("%f", &x);
scanf("%f", &y);

if (pow(x,2)+pow(y,2) < pow(r,2))
if (x <= 1 && x >= -0.5)
if (y <= 1 && y >= 0) {
printf("YES\n");
} else {
printf("NO\n");
}

}
