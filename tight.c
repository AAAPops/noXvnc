#define pr_fmt(fmt) "  _" fmt
#define DEBUG_TIGHT  1
#define DEBUG_TIGHT128  0
#define DEBUG_TIGHT255  0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>

#include "global.h"
#include "logerr.h"
#include "stream.h"
#include "utils.h"
#include "vncmsg.h"

#define Z_BUFF_SIZE     4096

char z_buff[Z_BUFF_SIZE] = {'\0'};

int z_decompress(char *pixel_buff, uint32_t pixel_buff_size, uint32_t z_buff_len);


// 7.6.7   Tight Encoding
int decoder_tight(uint16_t num_of_rect, char *pixel_buff, uint32_t pixel_buff_size, struct rect_info_t *rectInfo) {
    if (DEBUG_TIGHT) printf("%s() \n", __FUNCTION__);

    char rect_hdr[12];
    char tight_hdr[5];
    uint8_t reset_stream[4] = {0};
    uint16_t iter;

    size_t z_buff_offset = 0;
    printf(".....num_of_rect = %d \n", num_of_rect);

    for( iter = 0; iter < num_of_rect; iter++ ) {
    //for( iter = 0; iter < 1; iter++ ) {
        printf(".....iter = %d \n", iter);

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
        rectInfo[iter].raw_data_len = rectInfo[iter].x_res * rectInfo[iter].y_res * 3;
        {
            debug_cond(DEBUG_TIGHT, "---\n");
            debug_cond(DEBUG_TIGHT, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                       iter, rectInfo[iter].x_start, rectInfo[iter].y_start,
                       rectInfo[iter].x_res, rectInfo[iter].y_res,
                       rectInfo[iter].encode_type);
            debug_cond(DEBUG_TIGHT, "Raw Data Len: %d \n", rectInfo[iter].raw_data_len);
            mem2hex(DEBUG_TIGHT255, rect_hdr, nbytes_recv, 16);
        }



        nbytes_recv = stream_getN_bytes(tight_hdr, 5, MSG_PEEK); // Dry read 5 bytes from queue
        if (nbytes_recv < 0) {
            perror("stream_getN_bytes()");
            return -1;
        }
        {
            debug_cond(DEBUG_TIGHT, "Tight header:");
            mem2hex(DEBUG_TIGHT, tight_hdr, 5, 16);
        }

        uint8_t compress_control = *(tight_hdr + 0);

        for ( uint8_t iter2 = 0; iter2 < 4; iter2++) {
            if (BIT_CHECK(compress_control, iter2)) {
                reset_stream[iter2] = 1;
                debug_cond(DEBUG_TIGHT255, "Reset stream %d = %d \n", iter2, reset_stream[iter2]);
            }
        }


        if( BIT_CHECK(compress_control, 7u) == 1 ) {
            debug_cond(DEBUG_TIGHT, "FillCompression or JpegCompression \n");

            printf(".....Not implemented yet!!! \n");
            exit(-1);
        }


        if( BIT_CHECK(compress_control, 7u) == 0 ) {
            debug_cond(DEBUG_TIGHT, "BasicCompression \n");

            // Find out 'read-filter-id' value
            uint8_t read_filter_id = 0;
            if (BIT_CHECK(compress_control, 6u))
                read_filter_id = 1;
            debug_cond(DEBUG_TIGHT, "read_filter_id = %d \n", read_filter_id);

            // Find out 'Use stream' value
            BIT_CLEAR(compress_control, 6u);
            uint8_t use_stream = compress_control >> 4u;
            debug_cond(DEBUG_TIGHT255, "use_stream = %d \n", use_stream);

            // *** Calculate zip compressed data length***
            // 1 byte  example:
            //*(msg + 1) = 0x4A; // => 74
            // 2 bytes example:
            //*(msg + 1) = 0x90; *(msg + 2) = 0x4e; // => 10.000
            //*(msg + 1) = 0xde; *(msg + 2) = 0x42; // => 8.542
            //*(msg + 1) = 0xcf; *(msg + 2) = 0x39; // => 7.375
            // 3 bytes example:
            //*(msg + 1) = 0x90; *(msg + 2) = 0x8b; *(msg + 3) = 0xc1; // => 3.163.536

            unsigned char *len_ptr = (unsigned char*)(tight_hdr + read_filter_id + 1);
            uint32_t z_data_len = 0;
            uint8_t z_len_123_bytes;

            if ((BIT_CHECK(*(len_ptr), 7u) == 1) && (BIT_CHECK(*(len_ptr + 1), 7u) == 1)) {
                debug_cond(DEBUG_TIGHT, "Length represent as 3 bytes \n");
                BIT_CLEAR(*(len_ptr), 7u);
                BIT_CLEAR(*(len_ptr + 1), 7u);
                z_len_123_bytes = 3;
                z_data_len = (*(len_ptr + 2) << 14u) | (*(len_ptr + 1) << 7u) | *(len_ptr);

            } else if ((BIT_CHECK(*(len_ptr), 7u) == 1) && (BIT_CHECK(*(len_ptr + 1), 7u) == 0)) {
                debug_cond(DEBUG_TIGHT, "Length represent as 2 bytes \n");
                BIT_CLEAR(*(len_ptr), 7u);
                z_len_123_bytes = 2;
                z_data_len = (*(len_ptr + 1) << 7u) | *(len_ptr);

            } else {
                debug_cond(DEBUG_TIGHT, "Length represent as 1 byte \n");
                z_len_123_bytes = 1;
                z_data_len = *(len_ptr);
            }

            // Actual read Tight header to '/dev/null'
            nbytes_recv = stream_getN_bytes(tight_hdr, 1 + read_filter_id + z_len_123_bytes, 0);
            if (nbytes_recv < 0) {
                perror("stream_getN_bytes()");
                return -1;
            }


            // Read Z data
            debug_cond(DEBUG_TIGHT, "z_data_len = %d \n", z_data_len);
            nbytes_recv = stream_getN_bytes(z_buff + z_buff_offset, z_data_len, 0);
            if (nbytes_recv < 0) {
                perror("stream_getN_bytes()");
                return -1;
            }
            debug_cond(DEBUG_TIGHT, "z_data:");
            mem2hex(DEBUG_TIGHT, z_buff + z_buff_offset, 15, 30);

            //z_buff_offset += z_data_len;

            z_decompress(pixel_buff, pixel_buff_size,  z_data_len);
            z_buff_offset = 0;
        }
    }

    //printf(".....z_buff_len = %d \n", z_buff_offset);
    // z_buff_offset == z_buff data len
    z_decompress(pixel_buff, pixel_buff_size,  z_buff_offset);

    return STATUS_OK;
}



int z_decompress(char *pixel_buff, uint32_t pixel_buff_size, uint32_t z_data_len) {
    if (DEBUG_TIGHT) printf("%s() \n", __FUNCTION__);
    //memset(decompressed_buff, '\0', decompressed_buff_len); // Really need ???????

    //z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    infstream.avail_in = (uInt) z_data_len;             // size of input
    infstream.next_in = (Bytef *) z_buff;               // input char array
    infstream.avail_out = (uInt) pixel_buff_size; // size of output
    infstream.next_out = (Bytef *) pixel_buff;   // output char array

    debug_cond(DEBUG_TIGHT, "z_data [len: %d]: \n", z_data_len);
    mem2hex(DEBUG_TIGHT, z_buff, 15, 30);

    char file_name[25];
    sprintf(file_name, "z_data_%d.bin", z_data_len);
    FILE *write_ptr = fopen(file_name,"wb");
    fwrite(z_buff, z_data_len, 1,write_ptr);
    fclose(write_ptr);

    // the actual DE-compression work.
    int res = inflate(&infstream, Z_NO_FLUSH);
    //int res = inflate(&infstream, Z_SYNC_FLUSH);
    if (res == Z_OK) {
        debug_cond(DEBUG_TIGHT, "Total len of decompressed data = %ld \n", infstream.total_out);

    } else {
        debug_cond(DEBUG_TIGHT, "inflate() failed, err: %d \n", res);
        mem2hex(DEBUG_TIGHT, (char *)pixel_buff, 10, 30);
        //exit(-1);
    }
   //inflateEnd(&infstream);

    return infstream.total_out;
}
