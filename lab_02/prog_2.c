#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int num;
    printf("Input number_2: ");
    if (scanf("%d", &num) != 1)
    {
        printf("Error\n");
        return 1;
    }
    printf("Number_2: %d pid - %d\n", num, getpid());
    return 0;
}
