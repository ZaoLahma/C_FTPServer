/*
 * thread_pool_if.h
 *
 *  Created on: Oct 16, 2016
 *      Author: janne
 */

#ifndef INC_THREAD_STARTER_H_
#define INC_THREAD_STARTER_H_

struct ThreadStarter {
	void (*execute_function)(void* (*thread_func)(void*), void*);
};

#endif /* INC_THREAD_STARTER_H_ */
