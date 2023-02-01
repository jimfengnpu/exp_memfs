#include <stdio.h>

int main() {
	int a[3] = {1, 2, 3};
	int b, c;
	a[4] = 5;
	printf("%d %d %d\n", a+4, &b, &c);
}