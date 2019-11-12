/*
Send a message up to a newline to the server.
Read a reply up to a newline and print it to stdout.
Loop.
Usage:
    ./client [<server-address> [<port>]]
Default ip: localhost.
Default port: 12345
*/

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


#include <sys/time.h>
#include <sys/select.h>



/* Convert memory dump to hex  */
void mem2hex(int cond, char *str, int len, int colum_counts) {
    int x;

    if (cond) {
        printf("  ");

        for (x = 0; x < len; x++) {
            if (x > 0 && x % colum_counts == 0)
                printf("\n  ");
            printf("%02x ", (unsigned char)str[x]);
        }
        printf("\n");
    }
}

int recv_all(int sock_fd, char **buffer);

int main(int argc, char *argv[]) {
	char *buffer;
    ssize_t buff_len;

	char protoname[] = "tcp";
	struct protoent *protoent;
    char *server_hostname = "127.0.0.1";
    char *user_input = NULL;
    in_addr_t in_addr;
    in_addr_t server_addr;
    int sockfd;
    size_t getline_buffer = 0;
    ssize_t nbytes_read, i, user_input_len;
    struct hostent *hostent;
    /* This is the struct used by INet addresses. */
    struct sockaddr_in sockaddr_in;
    unsigned short server_port = 12345;

    if (argc > 1) {
        server_hostname = argv[1];
        if (argc > 2) {
            server_port = strtol(argv[2], NULL, 10);
        }
    }

    /* Get socket. */
	protoent = getprotobyname(protoname);
	if (protoent == NULL) {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
	}
    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Prepare sockaddr_in. */
    hostent = gethostbyname(server_hostname);
    if (hostent == NULL) {
        fprintf(stderr, "error: gethostbyname(\"%s\")\n", server_hostname);
        exit(EXIT_FAILURE);
    }
    in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1) {
        fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
        exit(EXIT_FAILURE);
    }
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(server_port);

    /* Do the actual connection. */
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }

    nbytes_read = recv_all(sockfd, &buffer);
    printf("\n\nGet bytes from server: %ld \n", nbytes_read);
    if( nbytes_read < 0 ) {
        perror("recv_all()");
        return EXIT_FAILURE;
    }

    printf("%s(): ", __func__);
    //mem2hex(true, buffer, nbytes_read, 30);

    free(buffer);
    return EXIT_SUCCESS;
}


int recv_all(int sock_fd, char **buff_ptr) {
    char chunk[512] = {0};
    int nbytes_recv = 0;
    int total_msg_len = 0;

    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = 10;
    tv.tv_usec = 1000;

    *buff_ptr = (char *)malloc(100 * sizeof(char));
    if( *buff_ptr == NULL) {
        perror("malloc()");
        return -1;
    }


    for(;;) {
        memset(chunk, 0, 512);

        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);

        // don't care about writefds and exceptfds:
        select(sock_fd + 1, &readfds, NULL, NULL, &tv);

        if( FD_ISSET(sock_fd, &readfds) ) {
            printf("\n-----\nThere's data in input queue \n");

            nbytes_recv = recv(sock_fd, chunk, 512, 0);
            printf("nbytes_recv() = %d \n", nbytes_recv);
            if (nbytes_recv < 0) {
                perror("recv()");
                return -1;
            }
            mem2hex(true, chunk, nbytes_recv, 30);

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
            tv.tv_usec = 1000;

        } else {
            printf("Timed out.\n");
            break;
        }
    }


    return total_msg_len;
}
