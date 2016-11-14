/*
 * c_thread_pool.h
 *
 *  Created on: Oct 16, 2016
 *      Author: janne
 */

#ifndef INC_THREAD_STARTER_IMPL_H_
#define INC_THREAD_STARTER_IMPL_H_

#include "thread_starter.h"

typedef enum THREAD_STARTER_TYPE
{
	POOL,
	DETACHED
} THREAD_STARTER_TYPE;

//Function implementation
void init_thread_starter(struct ThreadStarter* threadStarter,
		                 unsigned int type);

#endif /* INC_THREAD_STARTER_IMPL_H_ */
