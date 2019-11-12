#ifndef _VNCMSG_INCL_GUARD
#define _VNCMSG_INCL_GUARD


struct rect_info_t {
    uint16_t indx_num;
    uint16_t x_start;
    uint16_t y_start;
    uint16_t x_res;
    uint16_t y_res;
    uint32_t raw_data_len;
    int encode_type;
};

int msg_client_setpixelformat(int srv_sock_fd);

int msg_client_setencods(int srv_sock_fd);

int msg_client_fbupdaterequest(
        int srv_sock_fd, uint8_t incr,
        uint16_t x_pos, uint16_t y_pos,
        uint16_t width, uint16_t height);



int msg_srv_get(int srv_sock_fd);

#endif
