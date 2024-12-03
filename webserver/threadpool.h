#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <exception>
#include <stdio.h>
#include <list>
#include "locker.h"
//线程池类,定义成模板类是为了代码的复用，模板参数T是任务类
template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8,int max_requests = 10000);
    ~threadpool();
    bool append(T* request);//添加任务
private:
    static void* worker(void* arg);
    void run();//线程池执行函数
private:
    //线程的数量
    int m_thread_number;

    //线程池数组，大小为m_thread_number
    pthread_t* m_threads;

    //任务队列中最多允许的，等待处理的任务数量
    int m_max_requests;

    //任务队列
    std::list<T*> m_workqueue;

    //互斥锁
    locker m_queuelocker;//队列互斥锁

    //信号量，用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number,int max_requests):
    m_thread_number(thread_number),m_max_requests(max_requests)
    ,m_stop(false),m_threads(NULL){

    if((thread_number <= 0) || (max_requests <= 0)){
        throw std::exception();//抛出异常
    }
    m_threads = new pthread_t(m_thread_number);//初始化线程数量
    if(!m_threads){
        throw std::exception();
    }
    //创建thread_number个线程,并将他们设置为线程脱离
    for(int i = 0; i < thread_number; ++i){
        printf("create the %dth thread\n",i);
        if(pthread_create(m_threads + i,NULL,worker,this)){
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete m_threads;
    m_stop = true;

}
template<typename T>
bool threadpool<T>::append(T* request){
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests){
        //如果任务队列中的数量满了，就解锁
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//信号量+1
    return true;
}
template<typename T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();//信号量-1
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();//先获取第一个任务
        m_workqueue.pop_front();//删掉第一个
        m_queuelocker.unlock();
        if(!request){
            //如果任务没获取到
            continue;
        }
        request->process();//任务开始运行
    }
}
#endif
