#define pr_fmt(fmt) "  _" fmt
#define DEBUG_MAIN  0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/select.h>
#include <zlib.h>

#include "global.h"
#include "logerr.h"
#include "utils.h"
#include "stream.h"
#include "handshake.h"
#include "vncmsg.h"
#include "fb.h"
#include "mouse.h"


int vnc_main_loop(int*);


int main(int argc, char *argv[]) {
    if (DEBUG_MAIN) printf("-----\n%s() \n", __func__);

/*
 * Set Default Vars
 *
 */
    {
        progArgs.encodeRaw = 1;
        progArgs.encodeCopyRect = 0;
        progArgs.encodeTight = 0;
        strcpy(progArgs.fb_name, "/dev/fb0");
        progArgs.sharedflag = 0;
    }

/**
 * Parsing program arguments
 *
 */
    {
        strcpy(progArgs.password, argv[1]);

        char *connect_to = argv[argc - 1];
        char substr[] = ":";
        size_t delim_pos = strcspn(connect_to, substr);
        if (delim_pos > 15) {
            fprintf(stderr, "Server IP is wrong \n");
            return -1;
        }
        strncpy(progArgs.server, connect_to, delim_pos);
        progArgs.port = 5900 + strtol(connect_to + delim_pos + 1, NULL, 10);

        debug_cond(DEBUG_MAIN, "Server %s:[%d] \n", progArgs.server, progArgs.port);
    }

    /*  Init FrameBuffer device  */
    fb_open_dev(progArgs.fb_name);



/**
 * Init Mouse devices
 *
*/
    int mouse_fd[MAX_MOUSE_DEVS] = {0};
    int mouse_fd_n = mouse_open_dev(mouse_fd);
    fprintf(stderr, "  Found %d mouse(s) \n", mouse_fd_n);

    for (int iter = 0; iter < mouse_fd_n; iter++)
        debug_cond(DEBUG_MAIN, "mouse device descriptor: %d \n", mouse_fd[iter]);


/**
 * Init VNC server
 *
 */
    stream_init_srv(progArgs.server, progArgs.port);


/**
 * Init 4 Z-streams
 *
 */
    if( progArgs.encodeTight == 1 ) {
        int result, iter;

        for( iter = 0; iter < 4; iter++) {
            result = inflateInit(&zStream[iter]);
            if (result != Z_OK) {
                fprintf(stderr, "inflateInit(%d) failed! \n", iter);
                exit(-1);
            }
        }
    }


/**
 * Handshake with VNC server
 *
 */
    if( handshake_with_srv(srv_fd) < 0 )
        err_exit_2("handshake_with_srv");


/**
 * Start the main VNC loop
 *
 */
    vnc_main_loop(mouse_fd);


    printf("\n--- The End ---\n");
    return 0;
}



/**
 * VNC main Loop
 *
 */
int vnc_main_loop(int *mouse_fd_arr) {
    if (DEBUG_MAIN) printf("-----\n%s() \n", __func__);

    int increment = 0;
    int max_fd = 0;

    fd_set readfds;
    struct timeval tv;

    if( msg_client_setpixelformat(srv_fd) < 0 )
        err_exit();

    if( msg_client_setencods(srv_fd) < 0 )
        err_exit();


#pragma clang diagnostic ignored "-Wmissing-noreturn"
    for(;;) {
    //for( uint8_t iter = 0; iter < 1; iter++ ) {
        tv.tv_sec = 0;       // ?????
        tv.tv_usec = 200000; // ~60 FramebufferUpdateRequest for 1 sec.

        msg_client_fbupdaterequest(srv_fd, increment, 0, 0, 1024, 768);
        increment = 1;

        FD_ZERO(&readfds);
        FD_SET(srv_fd, &readfds);
        if (srv_fd > max_fd)
            max_fd = srv_fd;

        for( int iter = 0; iter < MAX_MOUSE_DEVS; iter++ ) {
            if( mouse_fd_arr[iter] <= 0 )
                break;

            FD_SET(mouse_fd_arr[iter], &readfds);
            if( mouse_fd_arr[iter] > max_fd )
                max_fd = mouse_fd_arr[iter];
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, &tv) == -1) {
            perror("select()");
            continue;
        }

        for( int iter = 0; iter < MAX_MOUSE_DEVS; iter++ ) {
            if( mouse_fd_arr[iter] <= 0 )
                break;

            if (FD_ISSET(mouse_fd_arr[iter], &readfds)) {
                mouse_handle_event(mouse_fd_arr[iter]);
            }
        }

        if( FD_ISSET(srv_fd, &readfds) ) {
            msg_srv_get(srv_fd);
        }

    }
#pragma clang diagnostic pop

    return STATUS_OK;
}

