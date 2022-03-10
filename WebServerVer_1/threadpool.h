#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <list>
#include <cstdio>
#include <exception>
#include "./lock/locker.h"

template <typename T>
class threadpool{
public:
    threadpool(int actor_model,int thread_number=8,int max_request=10000);
    ~threadpool();
    bool append_p(T* request);//将任务插入任务队列/proactor模式下
    bool append(T* request,int state);
private:
    static void* worker(void* arg);//传入线程的函数
    //因为传入线程的handler需要是void* (*hander)(void*)所以用这种形式;
    //如果传入成员函数的话会隐式传入this指针
    void run();
//--------------------------------------------------------------
    int m_thread_number;
    int m_max_requests;
    int m_actor_model;
    pthread_t *m_threads;
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理

};

template <typename T>
threadpool<T>::threadpool(int actor_model,int thread_number,int max_requests):
m_actor_model(actor_model),m_thread_number(thread_number),m_max_requests(max_requests)
{
    if (m_thread_number<=0|| m_max_requests<=0)
    {
        throw std::exception();
        /* code */
    }
    m_threads = new pthread_t[m_thread_number];
    //开辟线程池
    if(!m_threads){
        throw std::exception();
        //开辟失败
    }

    for (int i = 0; i < m_thread_number; i++)
    {
        if(pthread_create(m_threads+i,NULL,worker,this)!=0){
            //创建子线程 i 失败
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            //设置成脱离模式
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    //队列上锁
    if (m_workqueue.size() >= m_max_requests)
    {
        //超过最大可支持的请求个数，false
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    //请求入队
    m_queuelocker.unlock();
    m_queuestat.post();//任务数量加一
    return true;
}
template <typename T>
bool threadpool<T>::append(T *request,int state){
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run(){
    while (true)
    {
        m_queuestat.wait();//队列中的任务数量减一
        //一开始没有任务，会阻塞，但是当调用append后，信号量+1，某个线程会开始执行任务
        m_queuelocker.lock();
        //先上锁
        if (m_workqueue.empty())
        {
            //请求队列中没有任务
            m_queuelocker.unlock();
            continue;
        }
        T* request=m_workqueue.front();
        //获得队首的任务
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            //出错。读到的任务是空
            continue;
        }
        if (1 == m_actor_model)
        {   //reactor模式，子线程读写和处理！
            if (0 == request->m_state)
            //检测当前请要做什么，因为是电平触发模式，所以需要区分；读为0, 写为1
            {
                //如果是读的话，先一口气读完；
                if (request->read())
                {
                    printf("成功读入\n");
                    request->improv = 1;
                    //connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                    //读完处理
                    //printf("成功处理\n");
                }else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
  
            }
            else
            {
                if (request->write()){
                    request->improv = 1;
                }else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
                   
            }
        }
        else
        {
            //proactor模式，由主线程读完
           
            request->process();
        }
    }
    
}


#endif