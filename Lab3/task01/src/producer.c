#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>  

#include "producer.h"
#include "buffer.h"

struct sembuf producer_begin[2] = 
{
	{BUF_EMPTY, -1, 0}, // Ожидает освобождения хотя бы одной ячейки буфера.
	{BIN_SEM, -1, 0}    // Ожидает, пока другой производитель или потребитель выйдет из критической зоны.
};

struct sembuf producer_end[2] = 
{
	{BIN_SEM, 1, 0},	// Освобождает критическую зону.
	{BUF_FULL, 1, 0}	// Увеличивает кол-во заполненных ячеек.
};

void run_producer(buffer_t* const buf, const int sid, const int pdid)
{
	int sleep_time = rand() % PRODUCER_DELAY_TIME;
	sleep(sleep_time);

	if (semop(sid, producer_begin, 2) == -1)
	{
		perror("Something went wrong with producer begin!");
		exit(-1);
	}

    char ch = 'a' + buf->write_pos % 26;
    if (write_buffer(buf, ch) == -1) 
    {
        perror("Can't write!");
        exit(-1);
    }

    printf("\e[1;35mProducer #%d write: %c (sleep: %d)\e[0m\n", pdid, ch, sleep_time);

	if (semop(sid, producer_end, 2) == -1)
	{
		perror("Something went wrong with producer end!");
		exit(-1);
	}
}

void create_producer(buffer_t* const buf, const int sid, const int pdid)
{
	pid_t childpid;
	if ((childpid = fork()) == -1)
	{
		perror("Can't fork!");
		exit(-1);
	}
	
    if (childpid == 0)
	{
		for (int i = 0; i < ITERS_NUM; i++)
			run_producer(buf, sid, pdid);

		exit(0);
	}
}
