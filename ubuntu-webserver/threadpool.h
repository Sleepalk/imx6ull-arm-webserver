#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include "task.h"
#include <exception>
#include <semaphore.h>
#include <stdio.h>
#include <list>
/*
    线程池类
*/
class threadpool
{   
public:
    threadpool(int thread_numbers = 8, int max_requests = 10000);
    ~threadpool();
    void append(task* curTask);
private:
    int m_max_requests;
    int m_thread_numbers;
    pthread* m_threads;

};

threadpool::threadpool(int thread_number, int max_requests)
{
}

threadpool::~threadpool()
{
}


#endif