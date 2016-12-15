/*
 * main.c
 *
 *  Created on: Nov 10, 2016
 *      Author: janne
 */

#include "../../inc/ftp_impl.h"
#include <stdlib.h>

int main()
{
	int* running = (int*)malloc(sizeof(int));
	*running = 1;
	unsigned char addr[4] = {127, 0, 0, 1};
	//unsigned char addr[4] = {192, 168, 1, 248};
	run_ftp(running, addr, "3370");

	free(running);

	return 0;
}
