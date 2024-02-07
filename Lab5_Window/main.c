#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <windows.h>
#include <signal.h>

const DWORD sleep_time_for_writer = 2000;
const DWORD sleep_time_for_reader = 2000;

#define READERS_NUM 5
#define WRITERS_NUM 3

HANDLE mutex;
HANDLE can_read; 
HANDLE can_write;

LONG waiting_writers = 0;
LONG waiting_readers = 0;
LONG active_readers = 0;
bool active_writer = false;

int counter = 0;
int flag = 1;

void sig_handler(int sig_n)
{
	flag = 0;
	printf("CATCH %d, pid = %d, tid = %d\n", sig_n, getpid(), GetCurrentThreadId());
}

void start_write(void)
{
    InterlockedIncrement(&waiting_writers);

    if (active_writer || active_readers)
        WaitForSingleObject(can_write, INFINITE);
    active_writer = true;
    InterlockedDecrement(&waiting_writers);
    ResetEvent(can_write);
}

void stop_write(void)
{
    active_writer = false;
    if (waiting_readers == 0)
        SetEvent(can_write);
    else
        SetEvent(can_read);
}

void start_read(void)
{
    InterlockedIncrement(&waiting_readers);
    
    if (active_writer || (waiting_writers && (WaitForSingleObject(can_write, 0) == WAIT_OBJECT_0)))
        WaitForSingleObject(can_read, INFINITE);

    WaitForSingleObject(mutex, INFINITE);

    InterlockedDecrement(&waiting_readers);
    InterlockedIncrement(&active_readers);

    SetEvent(can_read);
    ReleaseMutex(mutex);
}

void stop_read(void)
{
    InterlockedDecrement(&active_readers);

    if (active_readers == 0)
       SetEvent(can_write);
}

DWORD WINAPI Reader(PVOID Param)
{
    int index = *((int *) Param);
    srand(time(NULL) + index);
	while (flag)
	{
		int sleep_time = rand() % sleep_time_for_writer;
		Sleep(sleep_time);
		start_read();
        printf("Reader %d id #%d read: %d\n", index, GetCurrentThreadId(), counter);
        stop_read();
	}

    return 0;
}

DWORD WINAPI Writer(PVOID Param)
{
    int index = *((int *) Param);
    srand(time(NULL) + index);
	while (flag)
	{
		int sleep_time = rand() % sleep_time_for_writer;
		Sleep(sleep_time);

		start_write();
        counter++;
        printf("Writer %d id #%d write: %d\n", index, GetCurrentThreadId(), counter);
        stop_write();
	}
    return 0;
}

int main()
{
    HANDLE threads[8];
    DWORD threadid[8];
    DWORD threadindex[8];
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        return -1;

    if ((mutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
    {
        perror("Failed to CreateMutex");
        return -1;
    }

    if ((can_read = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        perror("Failder to CreateEVent can_read");
        return -1;
    }

    if ((can_write = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
    {
        perror("Failder to CreateEVent can_write");
        return -1;
    }

    for (size_t i = 0; i < READERS_NUM; i++)
    {
        threadindex[i] = i;
        threads[i] = CreateThread(NULL, 0, Reader, &threadindex[i], 0, &threadid[i]);

        if (threads[i] == NULL)
        {
            perror("Failed to CreateThread");
            return -1;
        }
    }

    for (size_t i = READERS_NUM; i < READERS_NUM + WRITERS_NUM; i++)
    {
        threadindex[i] = i - READERS_NUM;
        threads[i] = CreateThread(NULL, 0, Writer, &threadindex[i], 0, &threadid[i]);

        if (threads[i] == NULL)
        {
            perror("Failed to CreateThread");
            return -1;
        }
    }

    for (int i = 0; i < WRITERS_NUM + READERS_NUM; i++) 
    { 
        DWORD dw = WaitForSingleObject(threads[i], INFINITE); 
        switch (dw) 
        { 
        case WAIT_OBJECT_0: 
            printf("thread %d finished successfully\n", threadid[i]); 
            break; 
        case WAIT_TIMEOUT: 
            printf("waitThread timeout %d\n", dw); 
            break; 
        case WAIT_FAILED: 
            printf("waitThread failed %d\n", dw); 
            break;
        } 
    }
    CloseHandle(mutex);
    CloseHandle(can_read);
    CloseHandle(can_write);

    return 0;
}