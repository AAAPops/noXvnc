#define pr_fmt(fmt) "  _" fmt
#define DEBUG_DECOD     0
#define DEBUG_DECOD128  0
#define DEBUG_DECOD256  0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "global.h"
#include "logerr.h"
#include "stream.h"
#include "utils.h"
#include "vncmsg.h"


int decoder_raw(uint16_t num_of_rect, char *pixel_buff, struct rect_info_t *rectInfo) {
    if (DEBUG_DECOD) printf("%s() \n", __FUNCTION__);

    uint16_t iter;
    char rect_hdr[12];
    size_t pixel_buff_offset = 0;

    for( iter = 0; iter < num_of_rect; iter++ ) {
    //for( iter = 0; iter < 1; iter++ ) {

        ssize_t nbytes_recv = stream_getN_bytes(rect_hdr, 12, 0);
        if (nbytes_recv < 0) {
            perror("stream_getN_bytes()");
            return -1;
        }

        rectInfo[iter].x_start = ntohs(*(uint16_t *) (rect_hdr + 0));
        rectInfo[iter].y_start = ntohs(*(uint16_t *) (rect_hdr + 2));
        rectInfo[iter].x_res   = ntohs(*(uint16_t *) (rect_hdr + 4));
        rectInfo[iter].y_res   = ntohs(*(uint16_t *) (rect_hdr + 6));
        rectInfo[iter].encode_type = ntohl(*(int32_t *) (rect_hdr + 8));
        rectInfo[iter].raw_data_len = rectInfo[iter].x_res * rectInfo[iter].y_res * 4;
        {
            debug_cond(DEBUG_DECOD, "---\n");
            debug_cond(DEBUG_DECOD, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                       iter, rectInfo[iter].x_start, rectInfo[iter].y_start,
                       rectInfo[iter].x_res, rectInfo[iter].y_res,
                       rectInfo[iter].encode_type);
            debug_cond(DEBUG_DECOD, "Raw Data Len: %dl \n", rectInfo[iter].raw_data_len);
            mem2hex(DEBUG_DECOD, rect_hdr, nbytes_recv, 16);
        }


        nbytes_recv = stream_getN_bytes(pixel_buff + pixel_buff_offset, rectInfo[iter].raw_data_len, 0);
        if (nbytes_recv < 0) {
            perror("stream_getN_bytes()");
            return -1;
        }
        {
            debug_cond(DEBUG_DECOD, "Actually bytes get = %dl \n", nbytes_recv);
            debug_cond(DEBUG_DECOD, "10 first bytes:");
            mem2hex(DEBUG_DECOD, pixel_buff + pixel_buff_offset, 10, 16);
        }

        pixel_buff_offset += rectInfo[iter].raw_data_len;

    }


    return STATUS_OK;
};