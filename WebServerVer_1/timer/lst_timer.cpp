#include"lst_timer.h"
#include"../http/http_conn.h"

sort_timer_lst::sort_timer_lst(){
    this->head=nullptr;
    this->tail=nullptr;
}
sort_timer_lst::~sort_timer_lst(){
    util_timer* temp=head;
    while (temp)
    {
        head=temp->next;
        delete temp;
        temp=head;
        /* code */
    }    
}

void sort_timer_lst::add_timer(util_timer* timer){

    if (!timer)
    {
        return;
    }
    printf("加入定时器！\n");
    if(!head){
        head=timer;
        tail=timer;
        return;
    }
    if(timer->expire<head->expire){
        //插入的timer的超时时间是最小值
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    //否则视为普通情况：
    add_timer(timer,head);
}

void sort_timer_lst::add_timer(util_timer* timer,util_timer* head){
    util_timer* pre=head;
    util_timer* cur=pre->next;
    while(cur){
        if(timer->expire<cur->expire){
            pre->next=timer;
            timer->prev=pre;
            timer->next=cur;
            cur->prev=timer;
            break;
        }
        pre=cur;
        cur=cur->next;
    }
    //如果查找到了最后
    if(!cur){
        pre->next=timer;
        timer->prev=pre;
        timer->next=nullptr;
        tail=timer;
    }
}

void sort_timer_lst::adj_timer(util_timer* timer){
    if(!timer){
        return;
    }
    printf("调整定时器！\n");
    util_timer* temp=timer->next;
    if(!temp||(timer->expire<temp->expire)){
        return;//如果不是最后一个，且仍保持升序，无需调整；
    }
    if(timer==head){
        head=head->next;
        head->prev=nullptr;
        timer->next=nullptr;
        add_timer(timer,head);
        //删除并重新插入
    }else{
        //不是头结点;
        timer->prev->next=timer->next;
        timer->next->prev=timer->prev;
        //timer删除
        add_timer(timer,timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    printf("删除定时器！\n");
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    //获取当前事件
    util_timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);
        printf("超时！断开\n");
        //超时了，调用回调函数
        //删除节点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
//=========================================
void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}
//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}
void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
//回调函数;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}