#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#define WRITERS_NUM 3
#define READERS_NUM 5

#define READER_SLEEP_TIME 2
#define WRITER_SLEEP_TIME 2

#define ACTIVE_WRITERS 0
#define WAITING_WRITERS 1
#define ACTIVE_READERS 2
#define WAITING_READERS 3

int flag = 1;

void sig_handler(int sig_n)
{
	flag = 0;
	printf("CATCH %d, p_id = %d\n", sig_n, getpid());
}

struct sembuf writer_begin[5] = 
{
    {WAITING_WRITERS, 1, 0},
    {ACTIVE_READERS, 0, 0},
    {ACTIVE_WRITERS, 0, 0},
    {ACTIVE_WRITERS, 1, 0},
    {WAITING_WRITERS, -1, 0}
};

struct sembuf writer_release[1] =
{
    {ACTIVE_WRITERS, -1, 0}
};

struct sembuf reader_begin[5] = 
{
    {WAITING_READERS, 1, 0},
    {ACTIVE_WRITERS, 0, 0},
    {WAITING_WRITERS, 0, 0},
    {ACTIVE_READERS, 1, 0},
    {WAITING_READERS, -1, 0},
};	

struct sembuf reader_release[1] = 
{
    {ACTIVE_READERS, -1, 0}	
};

void writer(int *const counter, const int sem_id)
{
    srand(time(NULL) + getpid());
	while (flag)
	{
		int sleep_time = rand() % 3;
		sleep(sleep_time);
		if (semop(sem_id, writer_begin, 5) == -1)
		{
			perror("Something went wrong with start_write!");
			exit(1);
		}

		(*counter)++;

		if (semop(sem_id, writer_release, 1) == -1)
		{
			perror("Something went wrong with stop_write!");
			exit(1);
		}
		printf("Writer %d write: %2d\n", getpid(), *counter);
	}
	exit(0);
}

void reader(int *const counter, const int sem_id)
{
    srand(time(NULL) + getpid());
	while (flag)
	{
		int sleep_time = rand() % READER_SLEEP_TIME + 1;
		sleep(sleep_time);

		if (semop(sem_id, reader_begin, 5) == -1)
		{
			perror("Something went wrong with start_read!");
			exit(1);
		}

		printf("Reader %d  read: %2d \n", getpid(), *counter);
		
		if (semop(sem_id, reader_release, 1) == -1)
		{
			perror("Something went wrong with stop_read!");
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
	pid_t childpid;
    signal(SIGINT, sig_handler);
	int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int shmid = shmget(IPC_PRIVATE, sizeof(int), perms);
	if (shmid == -1)
	{
		perror("Failed to create shared memory!");
		exit(1);
	}

    int *counter = (int*)shmat(shmid, NULL, 0);
	if (counter == (void*)-1)
	{
		perror("Shmat failed!");
		exit(1);
	}
    *counter = 0;

    int sem_descr = semget(IPC_PRIVATE, 4, IPC_CREAT | perms);
	if (sem_descr == -1)
	{
		perror("Failed to create semaphores!");
		exit(1);
	}

	if (semctl(sem_descr, ACTIVE_READERS, SETVAL, 0) == -1)
	{
		perror("Can't set control semaphors!");
		exit(1);
	}

	if (semctl(sem_descr, ACTIVE_WRITERS, SETVAL, 0) == -1)
	{
		perror("Can't set control semaphors!");
		exit(1);
	}

	if (semctl(sem_descr, WAITING_WRITERS, SETVAL, 0) == -1)
	{
		perror("Can't set control semaphors!");
		exit(1);
	}

    if (semctl(sem_descr, WAITING_READERS, SETVAL, 0) == -1)
	{
		perror("Can't set control semaphors!");
		exit(1);
	}

	for (int i = 0; i < WRITERS_NUM; i++)	
	{	
		if ((childpid = fork()) == -1)
		{
			perror("Can't fork!");
			exit(1);
		}
		else if (childpid == 0)
		{
			writer(counter, sem_descr);
		}
	}

    for (int i = 0; i < READERS_NUM; i++)
	{
		if ((childpid = fork()) == -1)
		{
			perror("Can't fork!");
			exit(1);
		}
		else if (childpid == 0)
		{
			reader(counter, sem_descr);
		}
	}

	for (int i = 0; i < READERS_NUM + WRITERS_NUM; i++)
    {
        int status;
		pid_t child_id = wait(&status);

        print_status(status, child_id);
    }

	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		perror("Failed to mark the segment to be destroyed!");
		exit(1);
	}

	if (shmdt((void*)counter) == -1)
	{
		perror("Failed to detach the segment!");
		exit(1);
	}

	if (semctl(sem_descr, 0, IPC_RMID, 0) == -1)
	{
		perror("Failed to delete semaphores!");
		exit(1);
	}

	return 0;
}

