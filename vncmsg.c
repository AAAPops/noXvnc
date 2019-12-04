#define pr_fmt(fmt) "  _" fmt
#define DEBUG_VNCMSG     0
#define DEBUG_VNCMSG128  0
#define DEBUG_VNCMSG256  0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "global.h"
#include "logerr.h"
#include "stream.h"
#include "utils.h"
#include "fb.h"
#include "sdecoders.h"
#include "tight.h"
#include "vncmsg.h"

#define PIXEL_BUFF_SIZE 1920 * 1080 * 4 // Maximum display resolution = 1920x1080
#define MAX_RECT_INDX   50

enum clnt_2_server_msg {
    SetPixelFormat = 0,
    SetEncodings = 2,
    FramebufferUpdateRequest = 3,
    //KeyEvent = 4,
    //PointerEvent = 5,
    //ClientCutText = 6
};

char pixel_buff[PIXEL_BUFF_SIZE] = {'\0'};



/*
   7.5.  Client-to-Server Message
 +--------+--------------------------+
 | Number | Name                     |
 +--------+--------------------------+
 | 0      | SetPixelFormat           |
 | 2      | SetEncodings             |
 | 3      | FramebufferUpdateRequest |
 | 4      | KeyEvent                 |
 | 5      | PointerEvent             |
 | 6      | ClientCutText            |
 +--------+--------------------------+
*/
int msg_client_setpixelformat(int srv_sock_fd) {


/*  7.5.1.  SetPixelFormat */
    if (DEBUG_VNCMSG) printf("-----\n%s() \n", __func__);
    char out_buff[1024] = {0};

    *(out_buff + 0)  = SetPixelFormat;
    *(out_buff + 4)  = 0x20;
    *(out_buff + 5)  = 0x18;
    *(out_buff + 7)  = 0x1;
    *(out_buff + 9)  = 0xff;
    *(out_buff + 11) = 0xff;
    *(out_buff + 13) = 0xff;
    *(out_buff + 14) = 0x10;
    *(out_buff + 15) = 0x8;

    stream_send_all(out_buff, 20);
    debug_cond(DEBUG_VNCMSG, "Send 'Pixel Format' to server:");
    mem2hex(DEBUG_VNCMSG, out_buff, 20, 50);

    return STATUS_OK;
}


/* 7.5.2.  SetEncodings */
int msg_client_setencods(int srv_sock_fd) {
    if (DEBUG_VNCMSG) printf("-----\n%s() \n", __func__);
    ssize_t nbytes_read;
    char out_buff[1024] = {0};
    int enc_count = 0;

    *(out_buff + 0) = SetEncodings;

    if( progArgs.encodeRaw == 1 ) {
        uint32_t *uint32_ptr = (uint32_t *) (out_buff + 4 + enc_count * 4);
        *uint32_ptr = htonl((uint32_t) Raw);
        enc_count++;
    }

    if( progArgs.encodeCopyRect == 1 ) {
        uint32_t *uint32_ptr = (uint32_t *) (out_buff + 4 + enc_count * 4);
        *uint32_ptr = htonl((uint32_t) copyRect);
        enc_count++;
    }

    if( progArgs.encodeTight == 1 ) {
        uint32_t *uint32_ptr = (uint32_t *) (out_buff + 4 + enc_count * 4);
        *uint32_ptr = htonl((uint32_t) Tight);
        enc_count++;
    }

    // Write number-of-encodings
    uint16_t *tmp_ptr = (uint16_t *)(out_buff + 2);
    *tmp_ptr = htons(enc_count);

    int msg_len = 4 + enc_count * 4;

    stream_send_all(out_buff, msg_len);
        debug_cond(DEBUG_VNCMSG, "Send 'Encoding Types' to server:");
        mem2hex(DEBUG_VNCMSG, out_buff, msg_len, 50);

    return STATUS_OK;
}


/*  7.5.3.  FramebufferUpdateRequest */
int msg_client_fbupdaterequest(
        int srv_sock_fd, uint8_t incr,
        uint16_t x_pos, uint16_t y_pos,
        uint16_t width, uint16_t height) {

    if (DEBUG_VNCMSG) printf("-----\n%s() \n", __func__);
    char out_buff[1024] = {0};

    *(out_buff + 0) = FramebufferUpdateRequest;
    *(out_buff + 1) = incr;
    *((uint16_t *)(out_buff + 2)) = htons(x_pos);
    *((uint16_t *)(out_buff + 4)) = htons(y_pos);
    *((uint16_t *)(out_buff + 6)) = htons(width);
    *((uint16_t *)(out_buff + 8)) = htons(height);


    stream_send_all(out_buff, 10);
        debug_cond(DEBUG_VNCMSG, "Send 'Update Request' to server:");
        mem2hex(DEBUG_VNCMSG, out_buff, 10, 50);

    return STATUS_OK;
}


/*
  7.6.  Server-to-Client Messages
 +--------+--------------------+
 | Number | Name               |
 +--------+--------------------+
 | 0      | FramebufferUpdate  |
 | 1      | SetColorMapEntries |
 | 2      | Bell               |
 | 3      | ServerCutText      |
 +--------+--------------------+
*/


/*  7.6.1.  FramebufferUpdate  */
int msg_framebufferupdate(int sock_fd) {
    if (DEBUG_VNCMSG) printf("%s() \n", __FUNCTION__);

    ssize_t nbytes_recv;

    char msg_hdr[4];
    char rect_hdr[12];

    struct rect_info_t rectInfo[MAX_RECT_INDX];
    //uint16_t rectInfo_indx;

    //size_t actual_pixel_buff_len = 0;


    nbytes_recv = stream_getN_bytes(msg_hdr, 4, 0);
    if (nbytes_recv < 0) {
        perror("stream_getN_bytes()");
        return -1;
    }

    uint16_t num_of_rect = ntohs(*(uint16_t *)(msg_hdr + 2));
    {
        debug_cond(DEBUG_VNCMSG, "There are #%d rectangles in this message:", num_of_rect);
        mem2hex(DEBUG_VNCMSG, msg_hdr, nbytes_recv, 16);
    }


    nbytes_recv = stream_getN_bytes(rect_hdr, 12, MSG_PEEK);
    if (nbytes_recv < 0) {
        perror("stream_getN_bytes()");
        return -1;
    }

    int32_t encode_type = ntohl(*(int32_t *) (rect_hdr + 8));
    {
        //debug_cond(DEBUG_VNCMSG, "---\n");
        debug_cond(DEBUG_VNCMSG, "EncodeType: %d \n", encode_type);
        mem2hex(DEBUG_VNCMSG, rect_hdr, nbytes_recv, 16);
    }

    switch (encode_type) {
        case Raw: {
            decoder_raw( num_of_rect, pixel_buff, rectInfo);
            if (DEBUG_VNCMSG) printf("%s() \n", __FUNCTION__);

            size_t pixel_buff_offset = 0;
            uint16_t iter;
            for( iter = 0; iter < num_of_rect; iter++ ) {
               {
                   debug_cond(DEBUG_VNCMSG, "---\n");
                   debug_cond(DEBUG_VNCMSG, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                               iter, rectInfo[iter].x_start, rectInfo[iter].y_start,
                               rectInfo[iter].x_res, rectInfo[iter].y_res,
                               rectInfo[iter].encode_type);
                    debug_cond(DEBUG_VNCMSG, "Raw Data Len: %dl \n", rectInfo[iter].raw_data_len);
               }

                fb_update_raw(pixel_buff + pixel_buff_offset,
                        rectInfo[iter].x_start, rectInfo[iter].y_start,
                        rectInfo[iter].x_res, rectInfo[iter].y_res);

                pixel_buff_offset += rectInfo[iter].raw_data_len;
            }

        }   break;

        case copyRect: {

        }   break;

        case Tight: {
            decoder_tight( num_of_rect, pixel_buff, sizeof(pixel_buff), rectInfo);
            //decoder_tight( num_of_rect, pixel_buff, sizeof(pixel_buff));
            if (DEBUG_VNCMSG128) printf("%s() \n", __FUNCTION__);

        }   break;

        case RRE:
        case Hextile:
        case TRLE:
        case ZRLE:
        case cursorPseudoEnc:
        case desctopSizePseudoEnc:
        default:
            fprintf(stderr, "This Encode type (%d) not implemented yet!!! \n", encode_type);
            exit(-1);
        break;
    }


    backFB_to_frontFB();
    return 1;
}


/*  7.6.  Server-to-Client Messages */
int msg_srv_get(int sock_fd) {
    if (DEBUG_VNCMSG) printf("%s() \n", __FUNCTION__);

    char msg_type;
    int nbytes_recv;

    nbytes_recv = stream_getN_bytes(&msg_type, 1, MSG_PEEK);
    if (nbytes_recv < 0) {
        perror("recv()");
        return -1;
    }

    switch (msg_type) {
        case framebufferUpdate: {
            debug_cond(DEBUG_VNCMSG, "Msg Type: 'FramebufferUpdate' \n");

            msg_framebufferupdate(sock_fd);
        }
            break;

        case setColorMapEntries:
            debug_cond(DEBUG_VNCMSG, "Msg Type: 'SetColorMapEntries' \n");

            break;

        case Bell:
            debug_cond(DEBUG_VNCMSG, "Msg Type: 'Bell' \n");

            break;

        case serverCutText:
            debug_cond(DEBUG_VNCMSG, "Msg Type: 'ServerCutText' \n");

            break;

        default:
            fprintf(stderr, "Server's message type [%d] unknown \n", msg_type);
            exit(-1);
            break;
    }

}



/*
//char msg[25000];
                //nbytes_recv = stream_getN_bytes(msg, 25000, 0);

                char *msg;
                nbytes_recv = recv_all(&msg);
                if (nbytes_recv < 0) {
                    perror("recv()");
                    return -1;
                }
                {
                    debug_cond(DEBUG_VNCMSG7, "---\n");
                    debug_cond(DEBUG_VNCMSG7, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                               iter, x_start, y_start, x_res, y_res, encode_type);
                    debug_cond(DEBUG_VNCMSG7, "10 first bytes:");
                    mem2hex(DEBUG_VNCMSG7, msg, 10, 16);
                }

                FILE *write_ptr;
                write_ptr = fopen("_tight_rect.bin", "wb");
                fwrite(msg, nbytes_recv, 1, write_ptr);

                printf("  msg[1] = 0x%02x \n", *(unsigned char *)(msg + 4));
                int iter;
                for( iter = 0; iter < 8; iter++ ) {
                    printf("  bit %d = ", iter);
                    if( BIT_CHECK(*(msg + 4), iter) )
                        printf("1\n");
                    else
                        printf("0\n");
                }


                fprintf(stderr, "This Encode type (0x%x) not implemented yet!!! \n", encode_type);
                exit(-1);
 */


/*
        switch (encode_type) {
            case Raw: {
                rect_len = x_res * y_res * 4;
                debug_cond(DEBUG_VNCMSG, "Encode: Raw, RectLen: %d,", rect_len);

                char *rect_buffer = (char *) malloc(rect_len * sizeof(char));
                if (rect_buffer == NULL) {
                    perror("malloc()");
                    return -1;
                }

                nbytes_recv = stream_getN_bytes(rect_buffer, rect_len, 0);
                if (nbytes_recv < 0) {
                    perror("recv()");
                    return -1;
                }
                {
                    debug_cond(DEBUG_VNCMSG, "Actually bytes get = %ld \n", nbytes_recv);
                    debug_cond(DEBUG_VNCMSG, "10 first bytes:");
                    mem2hex(DEBUG_VNCMSG, rect_buffer, 10, 16);
                }

                fb_update_backFB(rect_buffer, x_start, y_start, x_res, y_res);

                free(rect_buffer);

            }   break;

            case copyRect: {
                char msg[4];
                debug_cond(DEBUG_VNCMSG, "Encode: CopyRect \n");

                nbytes_recv = stream_getN_bytes(msg, 4, 0);
                if (nbytes_recv < 0) {
                    perror("recv()");
                    return -1;
                }

                uint16_t  src_x_pos = ntohs(*(uint16_t *) (msg + 0));
                uint16_t  src_y_pos = ntohs(*(uint16_t *) (msg + 2));

                fb_update_copyrect(src_x_pos, src_y_pos, x_start, y_start, x_res, y_res);

            }   break;

            case Tight: {
                {
                    debug_cond(DEBUG_VNCMSG, "-----\n");
                    debug_cond(DEBUG_VNCMSG, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                               iter, x_start, y_start, x_res, y_res, encode_type);
                }

                tpixel_buffer = (char *) realloc(tpixel_buffer, rect_info[iter].rect_len);
                if (tpixel_buffer == NULL) {
                    perror("realloc()");
                    return -1;
                }

                tight_decode(tpixel_buffer + tpixel_buff_len, rect_info[iter].rect_len);

                //fb_update_tpixel(tpixel_buffer, x_start, y_start, x_res, y_res);

            }   break;

            case RRE:
            case Hextile:
            case TRLE:
            case ZRLE:
            case cursorPseudoEnc:
            case desctopSizePseudoEnc:
            default:
                fprintf(stderr, "This Encode type (0x%x) not implemented yet!!! \n", encode_type);
                exit(-1);
            break;
        }
    }
*/