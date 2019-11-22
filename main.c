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


int vnc_main_loop(struct mouse_t *mouse);


int main(int argc, char *argv[])
{
    if (DEBUG_MAIN) printf("-----\n%s() \n", __func__);

    /*
     *   Set Default Vars
     */
    {
        progArgs.encodeRaw = 0;
        progArgs.encodeCopyRect = 0;
        progArgs.encodeTight = 1;
        strcpy(progArgs.fb_name, "/dev/fb0");
        strcpy(progArgs.mouse_name, "/dev/input/event6");
        progArgs.sharedflag = 0;
    }

    /*
     *  Parsing program arguments
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

        debug_cond(DEBUG_MAIN, "Server %s:[%d] \n", progArgs.server, progArgs.port );
    }

    /*  Init FrameBuffer device  */
    fb_open_dev(progArgs.fb_name);

    /*
     * Init Mouse device
     */
    struct mouse_t mouse;
    mouse_open_dev(progArgs.mouse_name, &mouse);


    /*
     * Init VNC server
     */
    stream_init_srv(progArgs.server, progArgs.port);


    /*
     *  Init 4 Z-streams
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


    /*
     * Handshake with VNC server
     */
    if( handshake_with_srv(srv_fd) < 0 )
        err_exit_2("handshake_with_srv");


    /*  Start the main VNC loop  */
    vnc_main_loop(&mouse);


    printf("\n--- The End ---\n");
    return 0;
}


int vnc_main_loop(struct mouse_t *mouse_ptr) {
    if (DEBUG_MAIN) printf("-----\n%s() \n", __func__);

    int increment = 0;
    int max_fd = 0;

    fd_set readfds;
    struct timeval tv;

    if( msg_client_setpixelformat(srv_fd) < 0 )
        err_exit();

    if( msg_client_setencods(srv_fd) < 0 )
        err_exit();


    //printf("srv_sock_fd = %d, mouse_fd = %d,  max_fd = %d \n", srv_sock_fd, mouse_fd, max_fd);


#pragma clang diagnostic ignored "-Wmissing-noreturn"
    for(;;) {
    //for( uint8_t iter = 0; iter < 1; iter++ ) {
        tv.tv_sec = 0;       // ?????
        tv.tv_usec = 200000; // ~60 FramebufferUpdateRequest for 1 sec.

        msg_client_fbupdaterequest(srv_fd, increment, 0, 0, 1024, 768);
        increment = 1;

        FD_ZERO(&readfds);
        FD_SET(srv_fd, &readfds);
        FD_SET(mouse_fd, &readfds);

        if (srv_fd > max_fd)
            max_fd = srv_fd;
        if (mouse_fd > max_fd)
            max_fd = mouse_fd;

        if (select(max_fd + 1, &readfds, NULL, NULL, &tv) == -1) {
            perror("select()");
            continue;
        }

        if( FD_ISSET(mouse_fd, &readfds) ) {
            mouse_handle_event(&mouse_ptr);
        }

        if( FD_ISSET(srv_fd, &readfds) ) {
            msg_srv_get(srv_fd);
        }

    }
#pragma clang diagnostic pop

    return STATUS_OK;
}

