#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>
#define PRODUCER_NUM 3
#define CONSUMER_NUM 3
#define BUF_FULL  0
#define BUF_EMPTY 1
#define BIN_SEM   2
struct sembuf producer_begin[2] = 
{
	{BUF_EMPTY, -1, 0}, 
	{BIN_SEM, -1, 0}   
};

struct sembuf producer_end[2] = 
{
	{BIN_SEM, 1, 0},	
	{BUF_FULL, 1, 0}
};

struct sembuf consumer_begin[2] = 
{
	{BUF_FULL, -1, 0},
	{BIN_SEM, -1, 0}
};

struct sembuf consumer_end[2] = 
{
	{BIN_SEM, 1, 0},
	{BUF_EMPTY, 1, 0}
};

int flag = 1;
char* addr;
int* buffer;
int* read_pos;
int* write_pos;

void print_status(int status, int child_id)
{
    if (WIFEXITED(status))
        printf("Child %d finished, code: %d\n", child_id, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("Child %d terminated, signal %d\n", child_id, WTERMSIG(status));
    else if (WIFSTOPPED(status))
        printf("Child %d stopped, signal %d\n", child_id, WSTOPSIG(status));
}

void producer(const int sem_id, const int prod_id)
{
    srand(time(NULL) + prod_id);
    while (flag)
    {
        sleep(rand() % 3 + 1);
        if (semop(sem_id, producer_begin, 2) == -1)
        {
            perror("Producer semop error");
            exit(1);
        }
        char symb = 'a' + ((*write_pos) % 26);
        buffer[(*write_pos)++] = symb;
        *write_pos %= 128;
        printf("Producer %d write: %c\n", prod_id, symb);
        if (semop(sem_id, producer_end, 2) == -1)
        {
            perror("Producer semop error");
            exit(1);
        }
    }
    exit(0);
}

void consumer(const int sem_id, const int cons_id)
{
    srand(time(NULL) + cons_id);
    while (flag)
    {
        sleep(rand() % 3 + 1);
        if (semop(sem_id, consumer_begin, 2) == -1)
        {
            perror("Consumer semop error");
            exit(1);
        }
        char symb = buffer[(*read_pos)++];
        *read_pos %= 128;
        printf("Consumer %d read: %c\n", cons_id, symb);
        if (semop(sem_id, consumer_end, 2) == -1)
        {
            perror("Consumer semop error");
            exit(1);
        }
    }
    exit(0);
}

void signal_handler(int signal)
{
    flag = 0;
    printf("Catch signal: %d, p_id = %d\n", signal, getpid());
}

int main()
{
    int perms = S_IRWXU | S_IRWXG | S_IRWXO;
    int childpid[3];
    int shmfd, semfd;
    int status;

    if ((shmfd = shmget(IPC_PRIVATE, 128, IPC_CREAT | perms)) == -1)
    {
        perror("shmget error");
        exit(1);
    }

    if (*(addr = (char *)shmat(shmfd, NULL, 0)) == -1)
    {
        perror("shmat error");
        exit(1);
    }

    signal(SIGINT, signal_handler);
    write_pos = (int *) addr;
    read_pos = (int *) addr + sizeof(int);
    buffer = (int *) addr + 2 * sizeof(int);
    *write_pos = 0;
    *read_pos = 0;
    if ((semfd = semget(IPC_PRIVATE, 3, IPC_CREAT | perms)) == -1)
    {
        perror("semget error");
        exit(1);
    }
	if (semctl(semfd, BIN_SEM, SETVAL, 1) == -1)
	{
		perror("Can't set control bin_sem!");
		exit(1);
	}

    if (semctl(semfd, BUF_FULL, SETVAL, 0) == -1)
	{
		perror("Can't set control buf_full semaphore!");
		exit(1);
	}

    if (semctl(semfd, BUF_EMPTY, SETVAL, 128) == -1)
	{
		perror("Can't set control buf_empty semaphor.");
		exit(1);
	}

    for (int i = 0; i < PRODUCER_NUM; i++)
    {
        childpid[i] = fork();
        if (childpid[i] == -1)
        {
            perror("Can't fork.\n");
            exit(1);
        }
        else if (childpid[i] == 0)
            producer(semfd, getpid());
    }
    for (int j = 0; j < CONSUMER_NUM; j++)
    {
        childpid[j] = fork();
        if (childpid[j] == -1)
        {
            perror("Can't fork.\n");
            exit(1);
        }
        else if (childpid[j] == 0)
            consumer(semfd, getpid());
    }

    for (int i = 0; i < PRODUCER_NUM + CONSUMER_NUM; i++)
    {
        pid_t childpid_r = wait(&status);
        if (childpid_r == -1)
        {
            perror("Something wrong with children waiting!");
            exit(1);
        }

		print_status(status, childpid_r);

    }
    if (shmdt(addr) == -1)
    { 
        perror("Can't detach shared memory segment");
        exit(1);
    }
    return 0;
}
