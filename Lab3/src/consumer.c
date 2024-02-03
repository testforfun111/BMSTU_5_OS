#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>

#include "consumer.h"
#include "buffer.h"

// Потребитель.
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

void run_consumer(buffer_t* const buf, const int sid, const int cid)
{
	int sleep_time = rand() % CONSUMER_DELAY_TIME + 1;
	// Создаем случайные задержки.
	sleep(sleep_time);

	// Получаем доступ к критической зоне.
	if (semop(sid, consumer_begin, 2) == -1)
	{
		perror("Something went wrong with consumer begin!");
		exit(-1);
	}

	// Началась критическая зона
    char ch;
    if (read_buffer(buf, &ch) == -1) 
	{
        perror("Can't read!");
        exit(-1);
    }
    printf("\e[1;34mConsumer #%d  read: %c (sleep: %d)\e[0m\n", cid,
            ch, sleep_time);

	// Закончилась критическая зона
	if (semop(sid, consumer_end, 2) == -1)
	{
		perror("Something went wrong with consumer end!");
		exit(-1);
	}
}

void create_consumer(buffer_t* const buffer, const int sid, const int cid)
{
	pid_t childpid;
	// При исполнении fork() дочернии процесс наследует пристыкованные
    // сегменты разделяемой памяти.
	// Если при порождении процесса произошла ошибка.
	if ((childpid = fork()) == -1)
	{
		perror("Can't fork!");
		exit(-1);
	}
	
    if (childpid == 0)
	{
		// Это процесс потомок.
		// Каждый потребитель потребляет ITERS_NUM товаров.
		for (int i = 0; i < ITERS_NUM; i++)
			run_consumer(buffer, sid, cid);

		exit(0);
	}
}
