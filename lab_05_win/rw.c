#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define READERS_NUM 3
#define WRITERS_NUM 3
#define SLEEP_RAND 900
#define SLEEP_BASE 300

HANDLE can_read;
HANDLE can_write;
HANDLE mutex;

HANDLE readers_threads[READERS_NUM];
HANDLE writers_threads[WRITERS_NUM];

long waiting_readers = 0;
long waiting_writers = 0;
long active_readers = 0;

int readers_id[READERS_NUM];
int writers_id[WRITERS_NUM];

int value = 0;
int stop = 0;

void start_read();

void stop_read();

void start_write();

void stop_write();

DWORD WINAPI run_reader(const LPVOID parametr);

DWORD WINAPI run_writer(const LPVOID parametr);

int create_threads();

void close();

int main(void)
{
    srand(time(NULL));

    mutex = CreateMutex(NULL, FALSE, NULL);

    if (mutex == NULL)
    {
        perror("\nError: Create mutex\n");
        return -1;
    }

    can_write = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (can_write == NULL)
    {
        perror("\nError: Create event - can_write\n");
        return -1;
    }

    can_read = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (can_read == NULL)
    {
        perror("\nError: Create event - can_read\n");
        return -1;
    }

    int rc = create_threads();

    if (rc != 0)
    {
        return -1;
    }

    WaitForMultipleObjects(READERS_NUM, readers_threads, TRUE, INFINITE);
    WaitForMultipleObjects(WRITERS_NUM, writers_threads, TRUE, INFINITE);

    close();

    printf("\n\nProgram has finished successfully\n");

    return 0;
}

void start_read()
{
    InterlockedIncrement(&waiting_readers);
    if (waiting_writers)
    {
        WaitForSingleObject(can_read, INFINITE);
    }

    WaitForSingleObject(mutex, INFINITE);
    InterlockedDecrement(&waiting_readers);
    InterlockedIncrement(&active_readers);
    SetEvent(can_read);
    ReleaseMutex(mutex);
}


void stop_read()
{
    InterlockedDecrement(&active_readers);

    if (active_readers == 0)
    {
        SetEvent(can_write);
    }
}

void start_write()
{
    InterlockedIncrement(&waiting_writers);
    if (active_readers > 0)
    {
        WaitForSingleObject(can_write, INFINITE);
    }
    InterlockedDecrement(&waiting_writers);
}


void stop_write()
{
    ResetEvent(can_write);
    if (waiting_readers > 0)
    {
        SetEvent(can_read);
    }
    else
    {
        SetEvent(can_write);
    }
}


DWORD WINAPI run_reader(const LPVOID parametr)
{
    int id = *(int*) parametr;

    srand(GetCurrentThreadId());
    for (;!stop;)
    {
        int sleep_time = rand() % SLEEP_RAND + SLEEP_BASE;
        Sleep(sleep_time);

        start_read();

        printf("\t\tReader: %2d\n", value);

        stop_read();
    }
}


DWORD WINAPI run_writer(const LPVOID parametr)
{
    int id = *(int*) parametr;

    srand(GetCurrentThreadId());
    for (;!stop;)
    {
        int sleep_time = rand() % SLEEP_RAND + SLEEP_BASE;
        Sleep(sleep_time);

        start_write();

        value++; 
        printf("Writer: %2d\n", value);

        stop_write();
    }
}


int create_threads()
{
    for (int i = 0; i < READERS_NUM; i++)
    {
        readers_id[i] = i;
        readers_threads[i] = CreateThread(NULL, 0, run_reader, readers_id + i, 0, NULL);

        if (readers_threads[i] == NULL)
        {
            perror("\nError: Create thread - reader\n");
            return -1;
        }
    }


    for (int i = 0; i < WRITERS_NUM; i++)
    {
        writers_id[i] = i;
        writers_threads[i] = CreateThread(NULL, 0, run_writer,  writers_id + i, 0, NULL);

        if (writers_threads[i] == NULL)
        {
            perror("\nError: Create thread - writer\n");
            return -1;
        }
    }

    return 0;
}

void close()
{
    for (int i = 0; i < READERS_NUM; i++)
    {
        CloseHandle(readers_threads[i]);
    }

    

    for (int i = 0; i < WRITERS_NUM; i++)
    {
        CloseHandle(writers_threads[i]);
    }

    CloseHandle(mutex);
    CloseHandle(can_write);
    CloseHandle(can_read);
}
