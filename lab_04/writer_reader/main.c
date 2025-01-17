#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// ACTIVE_READERS - активные читатели
// ACTIVE_WRITER - активные писатели
// WRITERS_QUEUE - очередь писателей 
// READERS_QUEUE - очередь читателей
// BIN_WRITER - бинарный семафора для блокировки процесса-писателя.
#define ACTIVE_READERS  0
#define ACTIVE_WRITER   1
#define WRITERS_QUEUE   2
#define READERS_QUEUE   3
#define BIN_WRITER      4

int fl = 1; 

struct sembuf start_read[] = 
{
    {READERS_QUEUE, 1, 0},  // Инкрементируем счетчик ждущих читателей в очереди
    {BIN_WRITER, 0, 0},     // Ожидаем, пока писатель не завершит работу
    {WRITERS_QUEUE, 0, 0},  // Ожидаем, пока писатели не будут в очереди
    {ACTIVE_READERS, 1, 0}, // Инкрементируем количество активных читателей
    {READERS_QUEUE, -1, 0}  // Декрементируем счетчик ждущих читателей в очереди
};

struct sembuf stop_read[] = 
{
    {ACTIVE_READERS, -1, 0} // Декрементируем количество активных читателей
};

struct sembuf start_write[] = 
{
    {WRITERS_QUEUE, 1, 0},  // Инкрементируем счетчик писателей в очереди
    {ACTIVE_READERS, 0, 0}, // Ожидаем, пока не завершатся все читатели
    {BIN_WRITER, 0, 0},     // Ожидаем, пока писатель не завершит работу
    {ACTIVE_WRITER, -1, 0}, // Декрементируем количество активных писателей
    {BIN_WRITER, 1, 0},     // Блокируем писателя
    {WRITERS_QUEUE, -1, 0}  // Декрементируем счетчик писателей в очереди
};

struct sembuf stop_write[] = 
{
    {BIN_WRITER, -1, 0},    // Разблокируем писателя
    {ACTIVE_WRITER, 1, 0}   // Инкрементируем количество активных писателей
};

void sig_handler(int signum)
{
    if (getpid() != getpgrp())
    {
        fl = 0;
        printf("\nChild %d catch signal: %d\n", getpid(), signum);
    }
}

void reader(int *shm, int semid, int pid)
{
    srand(time(NULL));
    while (fl)
    {
        sleep(rand() % 2 + 1);
        if (semop(semid, start_read, 5) == -1)
        {
            perror("Can't start_read.\n");
            exit(1);
        }
        printf("Reader %d read: %5d\n", pid, *shm);
        if (semop(semid, stop_read, 1) == -1)
        {
            perror("Can't stop_read.\n\n");
            exit(1);
        }
    }
    exit(0);
}

void writer(int *shm, int semid, int pid)
{
    srand(time(NULL));
    while (fl)
    {
        sleep(rand() % 2 + 1);
        if (semop(semid, start_write, 6) == -1)
        {
            perror("Can't start write\n");
            exit(1);
        }
        printf("Writer %d wrote:  %5d\n", pid, ++(*shm));
        if (semop(semid, stop_write, 2) == -1)
        {
            perror("Can't stop_write.\n");
            exit(1);
        }
    }
    exit(0);
}

int main()
{
    pid_t childpid[8];
    int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd, semid;
    int *shm = NULL;
    int i, status;

    // Установка обработчика сигнала SIGINT
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        perror("Can't signal.\n");
        exit(1);
    }

    // Создание разделяемой памяти
    fd = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | perms);
    if (fd == -1)
    {
        perror("Can't shmget\n");
        exit(1);
    }

    // Присоединение разделяемой памяти к адресному пространству процесса
    shm = (int*) (shmat(fd, 0, 0));
    if (shm == (int*) -1)
    {
        perror("Can't shmat\n");
        exit(1);
    }
    *shm = 0;

    // Создание набора семафоров
    semid = semget(IPC_PRIVATE, 5, IPC_CREAT | perms);
    if (semid == -1)
    {
        perror("Can't semget\n");
        exit(1);
    }

    // Инициализация семафоров
    if (semctl(semid, ACTIVE_READERS, SETVAL, 0) == -1)
    {
        perror("Can't semctl active_readers\n");
        exit(1);
    }
    if (semctl(semid, ACTIVE_WRITER, SETVAL, 1) == -1)
    {
        perror("Can't semctl active_writer\n");
        exit(1);
    }
    if (semctl(semid, WRITERS_QUEUE, SETVAL, 0) == -1)
    {
        perror("Can't semctl write_queue\n");
        exit(1);
    }
    if (semctl(semid, READERS_QUEUE, SETVAL, 0) == -1)
    {
        perror("Can't semctl read_queue\n");
        exit(1);
    }
    if (semctl(semid, BIN_WRITER, SETVAL, 0) == -1)
    {
        perror("Can't semctl bin_writer\n");
        exit(1);
    }
    
    for ( i = 0; i < 3; i++)
    {
        if ((childpid[i] = fork()) == -1)
        {
            perror("Can't fork writer\n");
            exit(1);
        }
        else if (childpid[i] == 0)
        {
            writer(shm, semid, getpid());
            exit(0);
        }
    }
    for ( i = 3; i < 8; i++)
    {
        if ((childpid[i] = fork()) == -1)
        {
            perror("Can't fork reader\n");
            exit(1);
        }
        else if (childpid[i] == 0)
        {
            reader(shm, semid, getpid());
            exit(0);
        }
    }

    for ( i = 0; i < 8; i++)
    {
        pid_t w_pid = waitpid(childpid[i], &status, WUNTRACED);
        if (w_pid == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        printf("Child %d finished: PID = %d\n", i, w_pid);
        if (WIFEXITED(status))
        {
            printf("exit, status=%d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("kill signal %d\n", WTERMSIG(status));
        }
        else if (WIFSTOPPED(status))
        {
            printf("stop signal %d\n", WSTOPSIG(status));
        }
    }

    // Отсоединение разделяемой памяти
    if (shmdt(shm) == -1)
    {
        perror("Can't shmdt\n");
        exit(1);
    }
    if (semctl(semid, IPC_RMID, 0) == -1)
    {
        perror("Can't delete semaphore\n");
        exit(1);
    }
    if (shmctl(fd, IPC_RMID, NULL))
    {
        perror("Can't mark a segment as deleted.\n");
        exit(1);
    }
}
