/*
 * thread_pool_impl.c
 *
 *  Created on: Oct 16, 2016
 *      Author: janne
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "../inc/thread_starter_impl.h"

//Local variables
static struct QueueItem* queue = 0;
static pthread_mutex_t mutex;
static struct PoolThreadFunc* threads;
static int num_threads = 0;
static int initialized = 0;

//Detached thread impl
static void execute_job_detached_thread_impl(void* (*thread_func)(void*), void* arg)
{
	pthread_t thread;
	int threadStatus = pthread_create(&thread, 0, thread_func, arg);
	threadStatus = pthread_detach(thread);
}

//Thread pool impl
struct QueueItem
{
	void* (*thread_func)(void*);
	void* args;
	struct QueueItem* next;
};

static void queue_execution(struct QueueItem* item)
{
	pthread_mutex_lock(&mutex);
	//printf("queue_execution called\n");

	struct QueueItem* lastItem = queue;

	if(lastItem == 0)
	{
		//printf("First queue item\n");
		queue = item;
		pthread_mutex_unlock(&mutex);
		return;
	}

	while(lastItem->next != 0)
	{
		//printf("Stepping through queue\n");
		lastItem = lastItem->next;
	}

	lastItem->next = item;
	pthread_mutex_unlock(&mutex);
}

static struct QueueItem* pop_queue()
{
	//printf("pop_queue called\n");
	pthread_mutex_lock(&mutex);
	struct QueueItem* retVal = queue;
	if(queue != 0)
	{
		queue = queue->next;
	}

	pthread_mutex_unlock(&mutex);
	//printf("pop_queue returning: %p\n", retVal);
	return retVal;
}

struct PoolThreadFunc
{
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int busy;
	struct PoolThreadFunc* next;
};

static void* pool_thread_func(void* arg)
{
	//printf("pool_thread_func called\n");
	struct QueueItem* queuePtr = 0;
	struct PoolThreadFunc* poolThreadFunc = (struct PoolThreadFunc*)arg;
	poolThreadFunc->busy = 1;
	while(1)
	{
		queuePtr = pop_queue();
		while(queuePtr != 0)
		{
			//printf("Executing queue: %p\n", queuePtr);
			queuePtr->thread_func(queuePtr->args);
			free(queuePtr);
			queuePtr = pop_queue();
		}

		pthread_mutex_lock(&poolThreadFunc->mutex);
		poolThreadFunc->busy = 0;
		//printf("thread_func waiting for cond\n");
		pthread_cond_wait(&poolThreadFunc->cond,
						  &poolThreadFunc->mutex);
		pthread_mutex_unlock(&poolThreadFunc->mutex);
		poolThreadFunc->busy = 1;
	}

	return 0;
}

static void execute_job_thread_pool_impl(void* (*thread_func)(void*), void* arg)
{
	struct QueueItem* queueItem = (struct QueueItem*)malloc(sizeof(struct QueueItem));

	queueItem->thread_func = thread_func;
	queueItem->args = arg;
	queueItem->next = 0;

	queue_execution(queueItem);

	pthread_mutex_lock(&mutex);

	struct PoolThreadFunc* thread = threads;
	while(thread != 0)
	{
		//printf("execute_job_thread_pool_impl taking mutex\n");
		if(thread->busy == 0)
		{
			pthread_mutex_lock(&thread->mutex);
			pthread_cond_signal(&thread->cond);
			pthread_mutex_unlock(&thread->mutex);
			pthread_mutex_unlock(&mutex);
			return;
		}
		//printf("execute_job_thread_pool_impl leaving mutex\n");
		thread = thread->next;
		//printf("thread: %p\n", thread);

	}
	//printf("execute_job_thread_pool_impl exit\n");

	num_threads++;
	printf("num_threads: %d\n", num_threads);

	thread = (struct PoolThreadFunc*)malloc(sizeof(struct PoolThreadFunc));
	thread->busy = 0;
	thread->next = 0;
	pthread_mutex_init(&thread->mutex, 0);
	pthread_cond_init(&thread->cond, 0);
	pthread_create(&thread->thread, 0, &pool_thread_func, thread);
	if(0 == threads)
	{
		threads = thread;
	}
	else
	{
		struct PoolThreadFunc* lastElem = threads;
		int numThreads = 1;

		while(lastElem->next != 0)
		{
			lastElem = lastElem->next;
			numThreads++;
		}
		lastElem->next = thread;
	}

	pthread_mutex_unlock(&mutex);
}

//The API
void init_thread_starter(struct ThreadStarter* threadStarter,
		                 unsigned int type)
{
	switch(type)
	{
	case DETACHED:
		threadStarter->execute_function = &execute_job_detached_thread_impl;
		break;
	case POOL:
        if(0 == initialized)
        {
            initialized = 1;
            pthread_mutex_init(&mutex, 0);
		}
		threadStarter->execute_function = &execute_job_thread_pool_impl;
		break;
	default:
		threadStarter->execute_function = 0;
		break;
	}
}
