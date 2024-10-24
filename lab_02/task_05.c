#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#define N 2

int flag = 0;

void signal_handler(int signal) {
    flag = 1;
    printf("Catch: %d\n", signal);
}

int main(void)
{
    pid_t childpid[N];
    int fd[N];
    char *const message[] = {"message_1_qwertyuiopasdfghjklzxcvbnm1234567890\n", "message_2\n"}; 
    
    if (pipe(fd) == -1)
    {
       perror("Can't pipe\n");
       exit(1);
    }
 
    if (signal(SIGINT, signal_handler) == -1)
    {
    	perror("Can't signal.\n");
    	exit(1);
    }
    
    sleep(2);
    
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
	        close(fd[0]);
            if (flag)
            {
                write(fd[1], message[i], strlen(message[i]));
                printf("Sent message: %s", message[i]);    
            }
            else
                printf("No signal! Message don't send!\n");       
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
        if (flag)
        {
            close(fd[1]);
            read(fd[0], message[i], sizeof(message[i]));    
            printf("Received messages: %s\n", message[i]);
        }
    }
    return 0;
}
