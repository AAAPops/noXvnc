/*
Server that reads a message up to a newline from a client.
Reply by incrementing each byte by one (ROT-1 cypher), newline terminated.
Close connection.
Loop.
Usage:
    ./server [<port>]
Default port: 12345
If a second client comes while the first one is talking, it hangs until the first one stops.
A forking server is the way to solve this.
*/

#define _XOPEN_SOURCE 700

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>



bool send_all(int sock_fd, void* buffer, size_t buf_len);


int main(int argc, char *argv[]) {
	char *buffer_ptr;
	char protoname[] = "tcp";
	struct protoent *protoent;
    int enable = 1;
    int i;
    int newline_found = 0;
    int server_sockfd, client_sockfd;
    socklen_t client_len;
    ssize_t nbytes_read;
    struct sockaddr_in client_address, server_address;
    unsigned short server_port = 12345u;
    size_t msg_len;

    if (argc > 1) {
        msg_len = strtol(argv[1], NULL, 10);
        if( msg_len < 1 ) {
            printf("Message length has to be > 0 \n");
            return EXIT_FAILURE;;
        }

        buffer_ptr = (char *) malloc(msg_len * sizeof(char));
    } else {
        printf("Set message length in byte \n");
        return EXIT_FAILURE;
    }

	protoent = getprotobyname(protoname);
	if (protoent == NULL) {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
	}

    server_sockfd = socket(
        AF_INET,
        SOCK_STREAM,
        protoent->p_proto
        /* 0 */
    );
    if (server_sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);
    if (bind(
            server_sockfd,
            (struct sockaddr*)&server_address,
            sizeof(server_address)
        ) == -1
    ) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sockfd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "listening on port %d\n", server_port);

    client_len = sizeof(client_address);
    client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_address, &client_len);

    /* --------------------------------------------  */
    memset(buffer_ptr, '0', msg_len);
    if( msg_len == 1 )
        *(buffer_ptr) = 'A';
    else {
        *(buffer_ptr) = 'A';
        *(buffer_ptr + msg_len - 1) = 'Z';
    }

    if( !send_all(client_sockfd, (void*)buffer_ptr, msg_len) ) {
        perror("send_all()");
        return EXIT_FAILURE;
    }

    free(buffer_ptr);

    sleep(1000);
    return EXIT_SUCCESS;
}


bool send_all(int sock_fd, void* buffer, size_t buf_len) {
    char *ptr = (char*) buffer;

    while (buf_len > 0)
    {
        int i = send(sock_fd, ptr, buf_len, 0);
        if (i < 1) {
            perror("send()");
            return false;
        }

        ptr += i;
        buf_len -= i;
    }

    return true;
}
