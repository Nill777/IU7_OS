#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>

#define LOCKFILE "/var/run/daemon.pid"
#define CONFIGFILE "/etc/my_daemon.conf"
#define TIMEOUT 10

sigset_t mask;

int lockfile(int fd)
{
    struct flock fl;

    fl.l_type = F_WRLCK;        // блокировка на запись
    fl.l_start = 0;             // блокировка начинается с начала файла
    fl.l_whence = SEEK_SET;     // откуда считать смещение, курсор на начало файла
    fl.l_len = 0;               // длина блокируемого участка, блокировка будет устанавливаться на всю длину файла

    return fcntl(fd, F_SETLK, &fl);
}

int already_running(void)
{
    int fd;
    int conffd;
    char buf[16];
    char conf_buf[256];
    int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    fd = open(LOCKFILE, O_RDWR | O_CREAT, perms);
    if (fd == -1)
    {
        syslog(LOG_ERR, "невозможно открыть %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }

    if ((conffd = open(CONFIGFILE, 'w')) == -1) // username userid
    {
        syslog(LOG_ERR, "ошибка открытия файла %s", CONFIGFILE);
        exit(1);
    }
    ftruncate(conffd, 0);
    // sprintf(conf_buf, "Username: %s; userid: %ld", getlogin(), (long) getuid());
    sprintf(conf_buf, "%ld", (long) getuid());
    write(conffd, conf_buf, strlen(conf_buf) + 1);

    if (lockfile(fd) == -1)
    {
        if (errno == EACCES || errno == EAGAIN)
        {
            close(fd);
            return 1;
        }
        syslog(LOG_ERR, "невозможно установить блокировку на %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }

    ftruncate(fd, 0);   // изменение размера файла, обрезка до 0
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);

    return 0;
}


void daemonize(const char *cmd)
{
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    /*
    * 1. Сбросить маску режима создания файла.
    */	
    umask(0);
    
    /*
    * Получить максимально возможный номер дескриптора файла.
    */
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
    {
        printf("%s: невозможно получить максимальный номер дескриптора ", cmd);
    }
    
    /*
    * 2. Стать лидером нового сеанса, чтобы утратить управляющий терминал.
    */
    if ((pid = fork()) == -1)
    {
        printf("%s: ошибка вызова функции fork", cmd);
    }
    else if (pid > 0)
    {
        exit(0);
    }

    /*
    * Обеспечить невозможность обретения управляющего терминала в будущем.
    */
    sa.sa_handler = SIG_IGN;    // устанавливает обработчик сигнала, сигнал SIGHUP будет игнорироваться
    sigemptyset(&sa.sa_mask);   // инициализирует набор сигналов, как пустой
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) == -1) // используется для изменения выполняемого процессом действия при получении определённого сигнала
    {
        printf("%s: невозможно игнорировать сигнал SIGHUP", cmd);
    }
    
    if(setsid() == -1)  // создает новый сеанс, в котором вызывающий процесс становится лидером группы процессов без управляющего терминала
    {
    	printf("Can't call setsid()\n");
    	exit(1);
    }
    
    /*
    * 4. Назначить корневой каталог текущим рабочим каталогом,
    * чтобы впоследствии можно было отмонтировать файловую систему.
    */
    if (chdir("/") == -1)
    {
        printf("%s: невозможно сделать текущим рабочим каталогом", cmd);
    }
    
    /*
    * 5. Закрыть все открытые файловые дескрипторы.
    */
    if (rl.rlim_max == RLIM_INFINITY)
    {
        rl.rlim_max = 1024;
    }
    for (i = 0; i < rl.rlim_max; i++)
    {
        close(i);
    }
    
    /*
    * Присоединить файловые дескрипторы 0, 1 и 2 к /dev/null.
    */
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
    
    /*
    * 6. Инициализировать файл журнала.
    */
    openlog(cmd, LOG_CONS, LOG_DAEMON);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2)
    {
        syslog(LOG_ERR, "ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
        exit(1);
    }
}

void reread(void)
{
    FILE *fd;
    int uid;
    
    syslog(LOG_DAEMON, "Username: %s", getlogin());
    
    if ((fd = fopen(CONFIGFILE, "r")) == NULL)
    {
        syslog(LOG_ERR, "ошибка открытия файла %s", CONFIGFILE);
    }

    fscanf(fd, "%d", &uid);
    fclose(fd);
    syslog(LOG_INFO, "Userid: %d", uid);
}

void *thr_fn(void *arg)
{
    int err, signo;

    for (;;)
    {
        err = sigwait(&mask, &signo);
        if (err != 0)
        {
            syslog(LOG_ERR, "ошибка вызова функции sigwait");
            exit(1);
        }

        switch (signo)
        {
        case SIGHUP:
            syslog(LOG_INFO, "получен сигнал SIGHUP; чтение конфигурационного файла");
            reread();
            break;
        case SIGTERM:
            syslog(LOG_INFO, "получен SIGTERM; выход");
            exit(0);   
        default:
            syslog(LOG_INFO, "получен непредвиденный сигнал %d\n", signo);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int err;
    pthread_t tid;
    char *cmd;
    struct sigaction sa;

    if ((cmd = strrchr(argv[0], '/')) == NULL)
    {
        cmd = argv[0];
    }
    else
    {
        cmd++;
    }
    
    /*
    * Перейти в режим демона
    */
    daemonize(cmd);
    
    /*
    * Убедиться в том, что ранее не была запущена другая копия демона
    */
    if (already_running())
    {
        syslog(LOG_ERR, "демон уже запущен");
        exit(1);
    }
    
    /*
    * Восстановить действия по умолчанию для сигнала SIGHUP и заблокировать все сигналы
    */
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        syslog(LOG_SYSLOG, "невозможно восставновить действие SIG_DFL для SIGHUP");
    }

    sigfillset(&mask);  // инициализирует mask как набор сигналов, содержащий все возможные сигналы
    if ((err = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0)   // заблокировать все сигналы
    {
        printf("ошибка выполнения операции SIG_BLOCK");
    }
    
    /*
    * Создать поток, который будет заниматься обработкой SIGHUP и SIGTERM
    */
    err = pthread_create(&tid, NULL, thr_fn, NULL);
    if (err != 0)
    {
        syslog(LOG_ERR, "невозможно создать поток");
        exit(1);
    }
    pthread_detach(tid);

    time_t raw_time;
    struct tm *timeinfo;
    
    for (;;)
    {
        sleep(TIMEOUT);
        time(&raw_time);
        timeinfo = localtime(&raw_time);
        syslog(LOG_INFO, "Текущее время: %s", asctime(timeinfo));
    }
}
