#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/stat.h>
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
#endif
