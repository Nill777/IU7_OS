#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#define N 2 

int main(void)
{
    pid_t childpid[N];
    const char *const prog[] = {"./prog_1", "./prog_2"};

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
            printf("Child: id - %d, ppid - %d, gr - %d\n",
	                getpid(), getppid(), getpgrp());
            int rc = execlp(prog[i], prog[i], 0);
	        if (rc == -1)
	        {
	           	perror("Can't exec\n");
	           	exit(1);
	        }
            return 0;
	    }
	    else 
	    {
	        printf("Parent: pid - %d, child - %d, gr - %d\n", 
        	    getpid(), childpid[i], getpgrp());
        }
    }
    int status;
    for (size_t i = 0; i < N; i++)
    {
        pid_t child_pid = wait(&status);
        if (child_pid == -1)
        {
            perror("Can't wait\n");
            exit(2);
        }
        printf("Finished child %zu: pid - %d\n", i + 1, child_pid);
        if (WIFEXITED(status))
            printf("Child %zu exited with code %d\n", i + 1, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("Child %zu terminated, recieved signal %d\n", i + 1, WTERMSIG(status));
        else if (WIFSTOPPED(status))
            printf("Child %zu stopped, recieved signal %d\n", i + 1, WSTOPSIG(status));  
    }
    
    return 0;
}
