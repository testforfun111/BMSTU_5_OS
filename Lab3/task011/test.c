#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#define CONSUMER_DELAY_TIME 3
#define PRODUCER_DELAY_TIME 2

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

void sig_handler(int sig_n)
{
	flag = 0;
	printf("CATCH %d, p_id = %d\n", sig_n, getpid());
}

void consumer(char **pc, const int sem_id)
{
	srand(time(NULL) + getpid());
	
	while (flag)
	{
		int sleep_time = rand() % 3;
		sleep(sleep_time);
		if (semop(sem_id, consumer_begin, 2) == -1)
		{
			perror("Something went wrong with consumer begin!");
			exit(1);
		}

		char ch = **pc;
		(*pc)++;

		if (semop(sem_id, consumer_end, 2) == -1)
		{
			perror("Something went wrong with consumer end!");
			exit(1);
		}
		printf("Sonsumer #%d  read: %c\n", getpid(), ch);
	}
	exit(0);
}

void producer(char** pp, char *symb, const int sem_id)
{
	srand(time(NULL) + getpid());
	while (flag)
	{
		int sleep_time = rand() % 3;
		sleep(sleep_time);
		if (semop(sem_id, producer_begin, 2) == -1)
		{
			perror("Something went wrong with producer begin!");
			exit(1);
		}

		**pp = *symb;
		(*symb)++;
		if ((*symb) > 'z') *symb = 'a';
		(*pp)++;

		if (semop(sem_id, producer_end, 2) == -1)
		{
			perror("Something went wrong with producer end!");
			exit(1);
		}
		printf("Producer #%d write: %c\n", getpid(), *symb - 1);
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
	char *address;
	char **pp;
	char **pc;
	char *symb;
	char *buf;
    int shmid = shmget(IPC_PRIVATE, 128, IPC_CREAT | perms);
    if (shmid == -1) 
	{
        perror("Failed to create shared memory!");
        exit(1);
    }

	if ((address = (char *)shmat(shmid, 0, 0)) == (char *)-1) 
	{
		perror("Shmat failed!");
		exit(1);
    }

	signal(SIGINT, sig_handler);

	pp = (char**) address;
	pc = (char**) address + sizeof(char *);
	symb = (char *) pc + sizeof(char *);

	buf = symb + sizeof(char);
	*pc = buf;
	*pp = buf;
	*symb = 'a';

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

    if (semctl(sem_descr, BUF_EMPTY, SETVAL, 111) == -1)
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
			producer(pp, symb, sem_descr);
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
			consumer(pc, sem_descr);
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

	if (shmdt((void*)address) == -1)
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
