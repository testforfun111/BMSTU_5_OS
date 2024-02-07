#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include "constants.h"

int flag = 1;
char* addr;
int* buffer;
int* read_pos;
int* write_pos;

void sig_handler(int sig_n)
{
	flag = 0;
	printf("CATCH %d, p_id = %d\n", sig_n, getpid());
}

void consumer(const int sem_id, const int cons_id)
{
	srand(time(NULL) + cons_id);
	
	while (flag)
	{
		int sleep_time = rand() % CONSUMER_DELAY_TIME + 1;
		sleep(sleep_time);
		if (semop(sem_id, consumer_begin, 2) == -1)
		{
			perror("Something went wrong with consumer begin!");
			exit(1);
		}

		char ch = buffer[(*read_pos)++];
		*read_pos %= 128;
		printf("Consumer #%d  read: %c (sleep: %d)\n", cons_id, ch, sleep_time);

		if (semop(sem_id, consumer_end, 2) == -1)
		{
			perror("Something went wrong with consumer end!");
			exit(1);
		}
	}
	exit(0);
}

void producer(const int sem_id, const int prod_id)
{
	srand(time(NULL) + prod_id);
	while (flag)
	{
		int sleep_time = rand() % PRODUCER_DELAY_TIME + 1;
		sleep(sleep_time);
		if (semop(sem_id, producer_begin, 2) == -1)
		{
			perror("Something went wrong with producer begin!");
			exit(1);
		}

		char ch = 'a' + ((*write_pos) % 26);
		buffer[(*write_pos)++] = ch;
		*write_pos %= 128;
		printf("Producer #%d write: %c (sleep: %d)\n", prod_id, ch, sleep_time);

		if (semop(sem_id, producer_end, 2) == -1)
		{
			perror("Something went wrong with producer end!");
			exit(1);
		}
	}
	exit(0);
}

void print_status(int status, int child_id)
{
    if (WIFEXITED(status))
        printf("Child %d finished, code: %d\n", child_id, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("Child %d terminated, signal %d\n", child_id, WTERMSIG(status));
    else if (WIFSTOPPED(status))
        printf("Child %d stopped, signal %d\n", child_id, WSTOPSIG(status));
}

int main(void)
{
	int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int childpid[3];
    int shmid = shmget(IPC_PRIVATE, 128, IPC_CREAT | perms);
    if (shmid == -1) 
	{
        perror("Failed to create shared memory!");
        exit(1);
    }

	if ((addr = (char *)shmat(shmid, 0, 0)) == (char *)-1) 
	{
		perror("Shmat failed!");
		exit(1);
    }

	signal(SIGINT, sig_handler);

    write_pos = (int *) addr;
    read_pos = (int *) addr + sizeof(int);
    buffer = (int *) addr + 2 * sizeof(int);
    *write_pos = 0;
    *read_pos = 0;

    int sem_descr = semget(IPC_PRIVATE, 3, IPC_CREAT | perms);
	if (sem_descr == -1)
	{
		perror("Failed to create semaphores!");
		exit(1);
	}
	
	if (semctl(sem_descr, BIN_SEM, SETVAL, 1) == -1)
	{
		perror("Can't set control bin_sem!");
		exit(1);
	}

    if (semctl(sem_descr, BUF_FULL, SETVAL, 0) == -1)
	{
		perror("Can't set control buf_full semaphore!");
		exit(1);
	}

    if (semctl(sem_descr, BUF_EMPTY, SETVAL, 128) == -1)
	{
		perror("Can't set control buf_empty semaphor.");
		exit(1);
	}

    for (int i = 0; i < PRODUCER_NUM; i++)
	{
		if ((childpid[i] = fork()) == -1)
		{
			perror("Can't fork!");
			exit(1);
		}
		else if (childpid[i] == 0)
		{
			producer(sem_descr, getpid());
		}
	}

	for (int i = 0; i < CONSUMER_NUM; i++)
	{
		if ((childpid[i] = fork()) == -1)
		{
			perror("Can't fork!");
			exit(1);
		}
		else if (childpid[i] == 0)
		{
			consumer(sem_descr, getpid());
		}
	}
    for (size_t i = 0; i < PRODUCER_NUM + CONSUMER_NUM; i++)
    {
        int status;
		pid_t child_id = wait(&status);

		print_status(status, child_id);
    }

	if (shmctl(shmid, IPC_RMID, NULL))
	{
		perror("Failed to mark the segment to be destroyed!");
		exit(1);
	}

	if (shmdt((void*)addr) == -1)
	{
		perror("Failed to detachess the segment!");
		exit(1);
	}

	if (semctl(sem_descr, 0, IPC_RMID, 0) == -1)
	{
		perror("Failed to delete semaphores!");
		exit(1);
	}

	return 0;
}
