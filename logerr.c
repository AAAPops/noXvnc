#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "logerr.h"
#include "global.h"


/* Close all open sockets and exit with negative code */
void err_exit() {

//    if( g_srv_fd > 0 )
//        close(g_srv_fd);

    exit(-1);
};


void err_exit_2(char *string) {
    fprintf(stderr, "\nError in function '%s()' \n", string);

    exit(-1);
};