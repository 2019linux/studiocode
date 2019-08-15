#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

#define dev_name "/dev/crystal_cdev"
#define MEM_CLEAR 0x1
#define TOTAL_LEN   (100*1024*1024)
#define MEM_SIZE    (4*1024*1024)
#define NUM_THREADS 20

static int fd = 0;
static char *w_buf = NULL;
static char *r_buf = NULL;
static unsigned int g_w_len = 1024;
static unsigned int g_r_len = 1024;

/*生成随机字符*/
static int gen_random_str(unsigned int len,char *str)
{
    int i,flag;
    srand((unsigned)time(NULL));
    for(i = 0; i<len; i++) {
        flag=rand() % 3;
        switch(flag) {
            case 0:
                str[i] = rand()& + 'a';
                break;
            case 1:
                str[i] = rand()& + 'A';
                break;
            case 2:
                str[i] = rand() + '0';
                break;
            default:
                break;
        }
    }
    return 0;
}

/*输入提示信息*/
static void print_usage(void)
{
    printf("\nUsage:\n");
    printf("p : procfile test\n");
    printf("m : multi wirte and poll read && signalio test\n");
    printf("s : write read speed test\n");
    printf("h : print usage\n");
    printf("q : quit\n");
    printf("\nPlease input your select ...\n");
}

/*写入测试*/
static unsigned int test_write(void *args)
{
    int thread_arg;
    char str[40] ;
    thread_arg = (int)(*(int *)args);
    sprintf(str, "Thread num = %d   Read result : ok ", thread_arg);
    return write(fd, str, strlen(str));
}

/*写入测试，休眠时间随机*/
static void *rw_thread(void *args)
{
    int rm_num,ret;
    rm_num = rand();
    sleep(rm_num%10);
    ret = test_write(args);
    if(!ret)
        printf("test_write error\n");
    return NULL;
}

/*异步信号接收提示*/
static void signalio_handler(int signum)
{
    printf("pid[%d] receive a signal from crystal_cdev,signalnum :%d\n",getpid(), signum);
}

/*poll 异步信号 多线程写入测试*/
static void mul_thr_poll_signalio_test(void)
{
    int ret,i,out_t,r_len = 0;
    int oflags;
    char buf[MEM_SIZE];
    int a[NUM_THREADS];
    pthread_t thread[NUM_THREADS];
    fd_set rfds;			// select 文件描述符集、timeval定义 
    struct timeval tv_poll;		// 创建线程 
    srand((unsigned)time(NULL));
    for( i = 0; i< NUM_THREADS; i++) {
        printf("Creating thread %d\n", i);
        a[i] = i;
        ret = pthread_create(&thread[i], NULL, rw_thread, &a[i]);
        if (ret) {
            printf("ERROR; return code is %d\n", ret);
            return ;
        }
    }
    ret = out_t = 0;
    while(1) {
        /*异步信号测试*/
        signal(SIGIO, signalio_handler);
        fcntl(fd, F_SETOWN, getpid());
        oflags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, oflags | FASYNC);
        /*设置poll等待时间*/ 
        tv_poll.tv_sec = 0;
        tv_poll.tv_usec = 400000;
        /*设置文件描述符集合*/ 
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        ret = select(fd+1, &rfds, NULL, NULL, &tv_poll);
        if (ret == -1)
            printf("select() error");
        else if (ret) {
            if (FD_ISSET(fd, &rfds)){
                printf("Data is available now.\n");
                r_len = read(fd, buf, MEM_SIZE);
                if(r_len == -1)
                    printf("read data error\n");
                else if(r_len) {
                    buf[r_len] = '\0';
                    printf("%s\n",buf);
                } else
                    printf("read 0 data\n");
            }
        } else {
            printf("No data within 0.4s \n");
            out_t ++;
        }
        /*如果长时间没有数据，则退出轮询所在的循环*/
        if(out_t > 40) {
            printf("There is no data in long time\n");
            break;
        }
    }
    /*线程退出*/ 
    for( i = 0; i<NUM_THREADS; i++)
        pthread_join(thread[i], NULL);
}

/*写循环测试，可通过调整全局变量g_w_len调整单次读写数据长度*/
static void * write_loop(void *arg)
{
    int ret = 0;
    unsigned int i,len,t_us,speed_s;
    struct  timeval start_w;
    struct  timeval end_w;
    i = len = t_us = speed_s = 0;
    sleep(1);
    while (len < TOTAL_LEN) {
        if(i == 0)
            gettimeofday(&start_w,NULL);
        ret = write(fd, w_buf, g_w_len);
        if(ret == -1)
            printf("app write error\n");
        else if(ret == 0)
            printf("write 0 data\n");
        len += ret;
        i++;
    }
    gettimeofday(&end_w,NULL);
    t_us = (1000000 * (end_w.tv_sec-start_w.tv_sec))+ (end_w.tv_usec-start_w.tv_usec);
    speed_s = (TOTAL_LEN/1024/1024)*(1000*1000)/t_us;
    printf("write_speed = %-15uMB/s\n",  speed_s);
    return NULL;
}

/*读循环测试，可通过调整全局变量g_r_len调整单次读写数据长度*/
static void * read_loop(void *arg)
{
    int ret = 0;
    unsigned int i,len,t_us,speed_s;
    struct  timeval start_r;
    struct  timeval end_r;
    i = len = t_us = speed_s = 0;
    while (len < TOTAL_LEN) {
        ret = read(fd, r_buf, g_r_len);
        if(ret == -1)
            printf("app read error\n");
        else if(ret == 0)
            printf("read 0 data\n");
        if(i == 0) {
            gettimeofday(&start_r,NULL);
        }
        len += ret;
        i++;
    }
    gettimeofday(&end_r,NULL);
    t_us = (1000000 * (end_r.tv_sec-start_r.tv_sec))+ (end_r.tv_usec-start_r.tv_usec);
    speed_s = (TOTAL_LEN/1024/1024)*(1000*1000)/t_us;
    printf("read_speed = %-15uMB/s\n",  speed_s);
    return NULL;
}

/*读写速度测试*/
static void w_r_speed_test(void)
{
    int ch = 0;
    int i,ret;
    pthread_t thread_w, thread_r;
    /*分配写、读内存*/
    w_buf = (char *)malloc(g_w_len);
    if(!w_buf) {
        printf("write memory allocate error\n");
        return ;
    }
    r_buf = (char *)malloc(g_r_len);
    if(!r_buf) {
        printf("read memory allocate error\n");
        return ;
    }
    gen_random_str(g_w_len, w_buf);
    /*创建读写线程*/ 
    ret = pthread_create(&thread_w, NULL, write_loop, NULL);
    if (ret) {
        printf("ERROR; return code is %d\n", ret);
        return ;
    }
    ret = pthread_create(&thread_r, NULL, read_loop, NULL);
    if (ret) {
        printf("ERROR; return code is %d\n", ret);
        return ;
    }
    sleep(1);
    pthread_join(thread_w, NULL);
    pthread_join(thread_r, NULL);
    free(w_buf);
    free(r_buf);
}

/*proc文件测试，最终保持user mode*/
static void procfile_test(void)
{
    printf("echo debug > /proc/crystal_cdev/crystal_cdev_info\n");
    system("echo debug > /proc/crystal_cdev/crystal_cdev_info");
    system("cat /proc/crystal_cdev/crystal_cdev_info");
    printf("echo user > /proc/crystal_cdev/crystal_cdev_info\n");
    system("echo user > /proc/crystal_cdev/crystal_cdev_info");
    system("cat /proc/crystal_cdev/crystal_cdev_info");
}

/*主测试入口*/
int main(void)
{
    int ch;
    fd = open(dev_name, O_RDWR);
    if (fd != -1) {
        if (ioctl(fd, MEM_CLEAR, 0) < 0) 	//初始化缓冲区数据
            printf("ioctl command failed\n");	
    }else{
        printf("Device open failure\n");
        return -1;
    }
	while(1) {
        print_usage();
        ch = getchar();
        getchar();     
        if( (ch != 'p') &&(ch != 'm')  &&(ch != 's') &&(ch != 'h') && (ch != 'q'))
            continue;
        switch (ch) {
            case 'p':
                procfile_test();
                break;
            case 'm':
                mul_thr_poll_signalio_test();
                break;
            case 's':
                w_r_speed_test();
                break;
            case 'h':
                print_usage();
                break;
            case 'q':
                return 0;
            default:
                break;
        }
    }
    close(fd);
    return 0;
}
