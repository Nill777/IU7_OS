#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#define N 2 

int main(void)
{
    pid_t childpid[N];
    
    for (size_t i = 0; i < N; i++)
    {
        childpid[i] = fork();
	    if (childpid[i] == -1)
	    {
	        perror("Can't fork\n");
	        exit(1);
	    }
	    else if (childpid[i] == 0)
	    {
            printf("Child before sleep: id - %d, ppid - %d, gr - %d\n",
	                getpid(), getppid(), getpgrp());
	        sleep(2);
	        printf("Child after sleep: id - %d, ppid - %d, gr - %d\n",
	                getpid(), getppid(), getpgrp());
            return 0;
	    }
	    else 
	    {
	        printf("Parent: pid - %d, child - %d, gr - %d\n", 
        	    getpid(), childpid[i], getpgrp());
	    }
    }
    
    return 0;
}