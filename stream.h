#include <stdbool.h>


int stream_init_srv(char server_ip[15+1], uint16_t port);


int stream_getN_bytes(char *srv_msg, int msg_len, int recv_opts);


bool stream_send_all(void* buffer, size_t buf_len);

int recv_all(char **buff_ptr);

