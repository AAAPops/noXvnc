#define pr_fmt(fmt) "  _" fmt
#define DEBUG_STREAM  0

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>


#include <sys/time.h>
#include <sys/select.h>

#include "global.h"
#include "logerr.h"
#include "utils.h"
#include "stream.h"



int stream_init_srv(char server_ip[15+1], uint16_t port) {
    if (DEBUG_STREAM) printf(" %s() \n", __func__);

    struct sockaddr_in  server;
    fprintf(stderr, "  Connect to server: %s %d", progArgs.server, progArgs.port);

    // Create socket: AF_INET  -- IP ver.4,  SOCK_STREAM  -- TCP protocol,  IPPROTO_IP -- IP protocol
    if( (srv_fd = socket(AF_INET , SOCK_STREAM , IPPROTO_IP)) < 0 )
    {
        perror("socket()");
        err_exit();
    }

    //---  Fill in  server's info  ---/
    server.sin_addr.s_addr = inet_addr( server_ip );
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    //Connect to remote server
    if( connect(srv_fd, (struct sockaddr *) &server, sizeof(server)) < 0 )
    {
        perror("connect()");
        err_exit_2((char *)__func__);
    }

    fprintf(stderr,"  ...Ok \n");
    return STATUS_OK;
};


/*  You can be sure you will get as much bytes as you ask exactly!!!  */
int stream_getN_bytes(char *srv_msg, int msg_len, int recv_opts) {
    if (DEBUG_STREAM) printf(" %s() \n", __func__);

    int nbytes_recv;
    int iter;
    size_t total_bytes_get = 0;

    for( iter = 0; total_bytes_get != msg_len; iter++ ) {
        nbytes_recv = recv(srv_fd, srv_msg + total_bytes_get, msg_len - total_bytes_get, recv_opts);
        if (nbytes_recv < 0) {
            perror("recv()");
            return -1;
        }

        total_bytes_get += nbytes_recv;
    }
    debug_cond(DEBUG_STREAM, "Read times: #%d,  total_bytes_get = %ld \n", iter, total_bytes_get);

    return total_bytes_get;
}


bool stream_send_all(void* buffer, size_t buf_len) {
    if (DEBUG_STREAM) printf(" %s() \n", __func__);
    char *ptr = (char*) buffer;

    while (buf_len > 0)
    {
        int i = send(srv_fd, ptr, buf_len, 0);
        if (i < 1) {
            perror("send()");
            return false;
        }

        ptr += i;
        buf_len -= i;
    }

    return true;
}




int recv_all(char **buff_ptr) {
    if(DEBUG_STREAM) printf("%s() \n", __FUNCTION__);
    char chunk[512] = {0};
    int nbytes_recv = 0;
    int total_msg_len = 0;

    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    *buff_ptr = (char *)malloc(100 * sizeof(char));
    if( *buff_ptr == NULL) {
        perror("malloc()");
        return -1;
    }


    for(;;) {
        memset(chunk, 0, 512);

        FD_ZERO(&readfds);
        FD_SET(srv_fd, &readfds);

        // don't care about writefds and exceptfds:
        select(srv_fd + 1, &readfds, NULL, NULL, &tv);

        if( FD_ISSET(srv_fd, &readfds) ) {
            //printf("\n-----\nThere's data in input queue \n");

            nbytes_recv = recv(srv_fd, chunk, 512, 0);
            //printf("nbytes_recv() = %d \n", nbytes_recv);
            if (nbytes_recv < 0) {
                perror("recv()");
                return -1;
            }
            debug_cond(DEBUG_STREAM, "Get chunk from server:");
            mem2hex(DEBUG_STREAM, chunk, nbytes_recv, 30);

            if (nbytes_recv == 0)
                return -1;

            *buff_ptr = (char *) realloc(*buff_ptr, sizeof(char) * (total_msg_len + nbytes_recv));
            //printf("realloc_buff_ptr: %p \n", *buff_ptr);
            if (*buff_ptr == NULL) {
                perror("realloc()");
                return -1;
            }

            memcpy(*buff_ptr + total_msg_len, chunk, nbytes_recv);

            total_msg_len += nbytes_recv;

            tv.tv_sec = 0;
            tv.tv_usec = 20000;

        } else {
            break;
        }
    }


    return total_msg_len;
}





