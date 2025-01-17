#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// SB контролирует доступ к разделяемому ресурсу
// SE пуст
// SF полон

#define SIZE_BUF 26
#define PRODUCERS_CNT 3
#define CONSUMERS_CNT 4
#define SB 0
#define SE 1
#define SF 2

int fl = 1;

struct sembuf start_produce[2] = { 
    {SE, -1, 0},
    {SB, -1, 0}    
};
struct sembuf stop_produce[2] =  {
    {SF, 1, 0},   
    {SB, 1, 0}    
};
struct sembuf start_consume[2] = { 
    {SF, -1, 0}, 
    {SB, -1, 0}
};
struct sembuf stop_consume[2] =  { 
    {SE, 1, 0}, 
    {SB, 1, 0}
};

void sig_handler(int sig_num)
{
    if (getpid() != getpgrp())
    {
        fl = 0;
        printf("Child %d catch signal %d\n", getpid(), sig_num);
    }
}

void producer(char **prod_ptr, char *alpha, int semid)
{
    srand(getpid());
    while(fl)
    {
        sleep(rand() % 2);
        if (semop(semid, start_produce, 2) == -1)
        {
            perror("Can't semop\n");
            exit(1);
        }
        if (*alpha > 'z')
            *alpha = 'a';
        **prod_ptr = *alpha; // запись очередного символа
        printf("   Producer %d - %c\n", getpid(), **prod_ptr);
        (*prod_ptr)++;  // перемещаем указатель для записи следующей буквы
        (*alpha)++; // следующая буква
        if (semop(semid, stop_produce, 2) == -1)
        {
            perror("Can't semop\n");
            exit(1);
        }
    }
    exit(0);
}

void consumer(char **cons_ptr, int semid)
{
    srand(getpid());
    while(fl)
    {
        sleep(rand() % 2);
        if (semop(semid, start_consume, 2) == -1)
        {
            perror("Can't semop\n");
            exit(1);
        }
        // вывод информации
        printf("Consumer %d - %c\n", getpid(), **cons_ptr);
        (*cons_ptr)++;
        if (semop(semid, stop_consume, 2) == -1)
        {
            perror("Can't semop\n");
            exit(1);
        }
    }
    exit(0);
}

int main()
{   
    int shmid, semid;
    int perms = S_IRUSR | S_IWUSR | S_IRGRP;
    char **prod_ptr;  // положение производителя
    char **cons_ptr;  // положение потребителя
    char *alpha;  // буквa
    // Установка обработчика сигнала SIGINT для завершения цикла работы
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        perror("Can't signal.\n");
        exit(1);
    }
    // Создание или подключение разделяемой памяти с ключом 100
    if ((shmid = shmget(100, 1024, IPC_CREAT | perms)) == -1)
    {
        perror("Can't shmget.\n");
        exit(1);
    }
    // shmaddr (второй аргумент) равно NULL, то система выбирает подходящий (неиспользуемый) адрес для подключения сегмента.
    // Подключение к разделяемой памяти с shmid
    char *buff = shmat(shmid, NULL, 0);
    if (buff == (char *) -1)
    {
        perror("Can't shmat.\n");
        exit(1);
    }
    // Инициализация указателей в разделяемой памяти
    prod_ptr = (char **) buff;                   // положение производителя
    cons_ptr = prod_ptr + sizeof(char*);         // положение потребителя
    alpha = (char*) (cons_ptr + sizeof(char*));  // символ, который производят/потребляют
    *cons_ptr = alpha + sizeof(char);
    *prod_ptr = *cons_ptr;
    *alpha = 'a';
    // Создание или получение набора из трех семафоров с ключом 100
    if ((semid = semget(100, 3, IPC_CREAT | perms)) == -1)
    {
        perror("Can't semget.\n");
        exit(1);
    }
    // Инициализация семафоров
    if (semctl(semid, SB, SETVAL, 1) == -1)            // бинарный семафор свободен
    {
        perror("Can't semctl c_bin.\n");
        exit(1);
    }
    if (semctl(semid, SE, SETVAL, SIZE_BUF) == -1)    // изначально размер пустого буфера = размеру буфера
    {
        perror("Can't semctl c_empty.\n");
        exit(1);
    }
    if (semctl(semid, SF, SETVAL, 0) == -1)            // изначально буфер не полон
    {
        perror("Can't semctl c_full.\n");
        exit(1);
    }
    pid_t chpid[CONSUMERS_CNT + PRODUCERS_CNT];
    // Создание производителей
    for (int i = 0; i < PRODUCERS_CNT; i++)
    {
        if ((chpid[i] = fork()) == -1)
        {
            perror("Can't fork producer.\n");
            exit(1);
        }
        if (chpid[i] == 0)
        {
            producer(prod_ptr, alpha, semid);
        }
    }
    // Создание потребителей
    for (int i = PRODUCERS_CNT; i < CONSUMERS_CNT + PRODUCERS_CNT; i++)
    {
        if ((chpid[i] = fork()) == -1)
        {
            perror("Can't fork consumer.\n");
            exit(1);
        }
        if (chpid[i] == 0)
        {
            consumer(cons_ptr, semid);
        }
    }
    // Ожидание завершение всех потомков
    for (int i = 0; i < (CONSUMERS_CNT + PRODUCERS_CNT); i++)
    {
        int status;
        if (waitpid(chpid[i], &status, WUNTRACED) == -1)
        {
            perror("Can't waitpid.\n");
            exit(1);
        }
        if (WIFEXITED(status)) 
            printf("Child %d finished, code: %d\n", chpid[i], WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("Child %d finished by unhandlable signal, signum: %d\n", chpid[i], WTERMSIG(status));
        else if (WIFSTOPPED(status))
            printf("Child %d finished by signal, signum: %d\n", chpid[i], WSTOPSIG(status));
    }
    // Отсоединение от разделяемой памяти
    // предназначен для исключения области разделяемой памяти из адресного пространства текущего процесса.
    if (shmdt((void *) prod_ptr) == -1)
    {
        perror("Can't shmdt.\n");
        exit(1);
    }
    
    if (semctl(semid, 1, IPC_RMID, NULL) == -1)
    {
        perror("Can't delete semafor.\n");
        exit(1);
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1) 
    {
        perror("Can't mark a segment as deleted.\n");
        exit(1);
    }
}
