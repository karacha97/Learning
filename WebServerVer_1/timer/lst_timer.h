#ifndef LST_TIMER_H
#define LST_TIMER_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
class util_timer;
struct client_data
{
    //客户的信息
    sockaddr_in address;
    int sockfd;        
    util_timer *timer;//客户对应的定时器
};


class util_timer
{
public:
    time_t expire;  //何时超时（绝对时间）
    void (* cb_func)(client_data* );    //超时后的回调函数，用来处理超时的客户
    client_data* user_data;
    util_timer(/* args */):prev(NULL),next(NULL){}
    ~util_timer(){}
    util_timer *prev;
    util_timer *next;
};


class sort_timer_lst
{
    //维护一个双向升序链表，每次超时信号触发时，只需要从小到大查就好了
public:
    sort_timer_lst(/* args */);
    ~sort_timer_lst();

    void add_timer(util_timer* timer);//加一个timer在链表里，且需要维持升序
    void adj_timer(util_timer* timer);//如果某客户发生了读写或其他请求，更新其timer
    void del_timer(util_timer* timer);//删除一个timer
    void tick();
private:
    void add_timer(util_timer* timer,util_timer* lst_head);

    util_timer* head;
    util_timer* tail;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif