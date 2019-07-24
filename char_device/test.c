#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	char *buf = NULL;
	int fd = -1;
	char rbuf[255];
	int i =0;
	int len = 0;

	int buf_size = 1024*1024*4;
	buf = malloc(buf_size);
	memset(buf, 1, buf_size);

	sleep(1);
	if((fd = open("/dev/my_drv", O_RDWR)) < 0)
		printf("open dev fail\r\n");

	write(fd, buf, buf_size);
	//read(fd, rbuf, len);

	//for(i = 0; i < len; i++)
	//	printf("%d\r\n",rbuf[i]);

	close(fd);
}
