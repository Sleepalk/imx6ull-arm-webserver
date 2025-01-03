#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include "task.h"
#include <exception>
#include <semaphore.h>
#include <stdio.h>
#include <list>
#include <iostream>
/*
    线程池类
*/
class threadpool
{   
public:
    threadpool(int thread_numbers = 8, int max_requests = 10000);
    ~threadpool();
    bool append(task* curTask);
    static void* worker(void* arg);
    void run();
private:
    int m_max_requests;
    int m_thread_numbers;
    pthread_t* m_threads = NULL;
    std::list<task*> m_request_queue;
    bool m_iStop = false;
    pthread_mutex_t m_mutex;//互斥锁
    sem_t m_sem;//信号量
};

threadpool::threadpool(int thread_number, int max_requests)
{
    m_max_requests = max_requests;
    m_thread_numbers = thread_number;
    //初始化互斥锁,NULL为默认属性
    pthread_mutex_init(&m_mutex, NULL);
    //初始化信号量,初始化为0，互斥
    sem_init(&m_sem, 0, 0);
    //预先创建出几个线程
    m_threads = new pthread_t[m_thread_numbers];
    if(!m_threads) { throw std::exception(); }
    for(auto i = 0; i != m_thread_numbers; ++i){
        std::cout << "create thread " << i + 1 << "th" << std::endl;
        if(pthread_create(m_threads + i, NULL, worker, this)){
            delete[] m_threads;
            throw std::exception();//抛出异常
        }
        //实现线程分离，用完自动释放资源
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();//抛出异常
        }
    }
}

threadpool::~threadpool()
{
    m_iStop = true;
    //销毁互斥锁
    pthread_mutex_destroy(&m_mutex);
    //销毁信号量
    sem_destroy(&m_sem);
    delete[] m_threads;
}

void *threadpool::worker(void *arg)
{
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

void threadpool::run(){
    while(!m_iStop){
        sem_wait(&m_sem);//信号量-1
        pthread_mutex_lock(&m_mutex);
        if(m_request_queue.empty()){
            pthread_mutex_unlock(&m_mutex);
            continue;
        }
        task* curTask = m_request_queue.front();//先获取第一个任务
        m_request_queue.pop_front();//删掉第一个
        pthread_mutex_unlock(&m_mutex);
        if(!curTask){
            continue;
        }
        curTask->process();
    }
}

bool threadpool::append(task* curTask){
    pthread_mutex_lock(&m_mutex);
    if(m_request_queue.size() > m_max_requests){
        pthread_mutex_unlock(&m_mutex);
        return false;
    }
    m_request_queue.push_back(curTask);
    pthread_mutex_unlock(&m_mutex);
    sem_post(&m_sem);//信号量+1
    return true;
}
#endif