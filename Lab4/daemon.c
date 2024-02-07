#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>

#define LOCKFILE "/var/run/daemon.pid"
#define CONFFILE "/etc/daemon.conf"

#define TIMEOUT 5

#define handle_error_en(en, msg) \
               do { errno = en; syslog(LOG_ERR, msg);; exit(EXIT_FAILURE); } while (0)

sigset_t mask;

struct thread_info 
{
	pthread_t tid;
	int tnum;
	char *argv;
};

int lockfile(int fd)
{
    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;

    return fcntl(fd, F_SETLK, &fl);
}

int already_running(void)
{
    int fd;
    char buf[16];
    int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    fd = open(LOCKFILE, O_RDWR | O_CREAT, perms);
    if (fd < 0)
    {
        handle_error_en(errno, "Невозможно открыть LOCKFILE");
    }

    if (lockfile(fd) < 0)
    {
        if (errno == EACCES || errno == EAGAIN)
        {
            close(fd);
            return 1;
        }
        handle_error_en(errno, "Невозможно установить блокировку на LOCKFILE");
    }

    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);

    return 0;
}

void daemonize(const char *cmd)
{
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
    {
        handle_error_en(errno, "Невозможно получить максимальный номер дескриптора");
    }

    if ((pid = fork()) < 0)
    {
        handle_error_en(errno, "Ошибка вызова функции fork");
    }
    else if (pid > 0)
    {
        exit(0);
    }

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) < 0)
    {
        handle_error_en(errno, "Невозможно игнорировать сигнал SIGHUP");
    }

    if(setsid() == -1)
    {
        handle_error_en(errno, "Can't call setsid()");
    }

    if (chdir("/") < 0)
    {
        handle_error_en(errno, "Невозможно сделать текущим рабочим каталогом");
    }

    if (rl.rlim_max == RLIM_INFINITY)
    {
        rl.rlim_max = 1024;
    }
    for (i = 0; i < rl.rlim_max; i++)
    {
        close(i);
    }

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    openlog(cmd, LOG_CONS, LOG_DAEMON);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2)
    {
        handle_error_en(errno, "Ошибочные файловые дескрипторы");
    }
}

void reread(void)
{
	FILE *f = fopen(CONFFILE, "r");
	fclose(f);
}

void *thr_fn(void *arg)
{
    int err, signo;

    for (;;)
    {
        err = sigwait(&mask, &signo);
        if (err != 0)
        {
        	handle_error_en(err, "Ошибка вызова функции sigwait");
        }
        switch (signo)
        {
        case SIGHUP:
            syslog(LOG_INFO, "Чтение конфигурационного файла");
            reread();
            break;
        case SIGTERM:
            syslog(LOG_INFO, "Получен SIGTERM; выход");
            exit(0);
        default:
            syslog(LOG_INFO, "Получен сигнал %d\n", signo);
        }
    }

    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    int err;
    int s;
    pthread_t tid;
    pthread_attr_t attr;
    struct thread_info t_info;
    char *cmd;
    struct sigaction sa;

    if ((cmd = strrchr(argv[0], '/')) == NULL)
    {
        cmd = argv[0];
    }
    else
    {
        cmd++;
    }

    daemonize(cmd);

    if (already_running())
    {
        handle_error_en(errno, "Демон уже запущен");
    }

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
    {
        syslog(LOG_SYSLOG, "Невозможно восставновить действие SIG_DFL для SIGHUP");
    }

    sigfillset(&mask);
    if ((err = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0)
    {
        handle_error_en(err, "Ошибка выполнения операции SIG_BLOCK");
    }

    s = pthread_attr_init(&attr);
    if (s != 0)
    {
        handle_error_en(s, "pthread_attr_init");
    }
    t_info.tnum = 1;
    t_info.argv = NULL;
    s = pthread_create(&t_info.tid, &attr, thr_fn, NULL);
    if (s != 0)
    {
        handle_error_en(s, "Невозможно создать поток");
    }
    s = pthread_attr_destroy(&attr);
    if (s != 0)
    {
        handle_error_en(s, "pthread_attr_destroy");
    }

    time_t t; 	
    for (;;)
    {
        time(&t);
        syslog(LOG_INFO, "Текущее время: %s", ctime(&t));
        sleep(TIMEOUT);
    }
}
