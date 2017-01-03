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
    run_ftp(running, "3370");
    free(running);
    return 0;
}
