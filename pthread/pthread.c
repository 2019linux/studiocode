#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <error.h>
#include <sys/types.h>
#include <unistd.h>



typedef struct{

	int a;
	int b;
	char c;
} date_str;

int pthread_join(pthread_t thread, void **retval);


void *thread_start(void *arg)
{
	printf("tid = %ld, pid = %d, ppid = %d\n",pthread_self(), getpid(), getppid());	
	date_str *p  = (date_str *)arg;
	
	printf("a = %d\n",p->a);

	pthread_exit((void *)1);
}


int main(int argc, char **argv)
{

	pthread_t pid;
	int ret;
	date_str data;

	data.a = 10;
	ret = pthread_create(&pid,NULL, thread_start, &data);
	if(ret != 0)
		perror("pthread create fail");

	
	ret = pthread_create(&pid,NULL, thread_start, &data);
	if(ret != 0)
		perror("pthread create fail");

	printf("main tid = %ld, pid = %d ppid=%d\n",pthread_self(), getpid(), getppid());	
	
	pthread_exit((void *)1);
	return 0;

}
