/*
 * c_thread_pool.h
 *
 *  Created on: Oct 16, 2016
 *      Author: janne
 */

#ifndef INC_THREAD_STARTER_IMPL_H_
#define INC_THREAD_STARTER_IMPL_H_

#include "thread_starter.h"

static const unsigned int DETACHED = 0;
static const unsigned int POOL = 1;

//Function implementation
void init_thread_starter(struct ThreadStarter* threadStarter,
		                 unsigned int type);

#endif /* INC_THREAD_STARTER_IMPL_H_ */
