#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>  

#include "producer.h"
#include "buffer.h"

// в структуре struct sembuf, состоящей из полей:
//     short sem_num;   /* semaphore number: 0 = первый */
//     short sem_op;    /* semaphore operation */
//     short sem_flg;   /* operation flags */

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

void run_producer(buffer_t* const buf, char *ch, const int sid, const int pdid)
{
	int sleep_time = rand() % PRODUCER_DELAY_TIME + 1;
	sleep(sleep_time);

	// Получаем доступ к критической зоне
	// совершаем неделимое действие с массивом семафоров
	if (semop(sid, producer_begin, 2) == -1)
	{
		perror("Something went wrong with producer begin!");
		exit(-1);
	}

	// Началась критическая зона
	// Положить в буфер.
	// записываем в разделяемый сегмент памяти(массив) букву
    (*ch)++;
    if (write_buffer(buf, *ch) == -1) 
    {
        perror("Can't write!");
        exit(-1);
    }

    printf("\e[1;35mProducer #%d write: %c (sleep: %d)\e[0m\n", pdid, (*ch),
            sleep_time);

	// Закончилась критическая зона
	if (semop(sid, producer_end, 2) == -1)
	{
		perror("Something went wrong with producer end!");
		exit(-1);
	}
}

void create_producer(buffer_t* const buf, char *const ch,
                     const int sid, const int pdid)
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
		// Каждый производитель производит ITERS_NUM товаров.
		for (int i = 0; i < ITERS_NUM; i++)
			run_producer(buf, ch, sid, pdid);

		exit(0);
	}
}
