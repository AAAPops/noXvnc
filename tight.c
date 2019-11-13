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
#include "fb.h"

#define Z_BUFF_SIZE     1024 * 100  // I think that 100KB should be enough for everything -)))
char z_buff[Z_BUFF_SIZE] = {'\0'};

enum {
    COPY_FILTER         = 0,
    PALETTE_FILTER      = 1,
    GRADIENT_FILTER     = 2,
    FILL_COMPRESSION    = 8,
    JPEG_COMPRESSION    = 9
};


ssize_t z_decompress(char* pixel_buff_p, size_t  pix_buff_len, char* z_buff_p, size_t z_data_len);

size_t jpeg_zlib_data_len(void);


// 7.6.7   Tight Encoding
int decoder_tight(uint16_t num_of_rect, char *pixel_buff, uint32_t pixel_buff_size)
{
    if (DEBUG_TIGHT) printf("%s() \n", __FUNCTION__);

    char rect_hdr[12];
    //char tight_hdr[5];
    uint8_t compression_control;
    uint8_t reset_stream[4] = {0};
    uint16_t iter;


    for( iter = 0; iter < num_of_rect; iter++ ) {
    //for( iter = 0; iter < 2; iter++ ) {
        printf(".....iter = %d \n", iter);

        ssize_t nbytes_recv = stream_getN_bytes(rect_hdr, 12, 0);
        if (nbytes_recv < 0) {
            perror("stream_getN_bytes()");
            return -1;
        }

        uint16_t x_start = ntohs(*(uint16_t *) (rect_hdr + 0));
        uint16_t y_start = ntohs(*(uint16_t *) (rect_hdr + 2));
        uint16_t x_res   = ntohs(*(uint16_t *) (rect_hdr + 4));
        uint16_t y_res   = ntohs(*(uint16_t *) (rect_hdr + 6));
        int encode_type = ntohl(*(int32_t *) (rect_hdr + 8));
        uint32_t tpixel_data_len = x_res * y_res * 3;
        {
            //debug_cond(DEBUG_TIGHT, "---\n");
            debug_cond(DEBUG_TIGHT, "Rect# %d, Xpos=%d  Ypos=%d, %dx%d, EncodeType: %d \n",
                       iter, x_start, y_start, x_res, y_res, encode_type);
            debug_cond(DEBUG_TIGHT, "Raw Data Len: %d \n", tpixel_data_len);
            mem2hex(DEBUG_TIGHT255, rect_hdr, nbytes_recv, 16);
        }



        nbytes_recv = stream_getN_bytes((char*)&compression_control, 1, 0);
        if (nbytes_recv < 0) {
            perror("stream_getN_bytes()");
            return -1;
        }
        debug_cond(DEBUG_TIGHT, "Tight header:"); mem2hex(DEBUG_TIGHT, (char*)&compression_control, 1, 10);


        for ( uint8_t iter2 = 0; iter2 < 4; iter2++) {
            if (BIT_CHECK(compression_control, iter2)) {
                reset_stream[iter2] = 1;
                debug_cond(DEBUG_TIGHT255, "Reset stream %d = %d \n", iter2, reset_stream[iter2]);
            }
        }


        if( BIT_CHECK(compression_control, 7u) == 1 ) {

            //---------  FillCompression  ---------//
            if( (compression_control >> 4u) == 8 ) {
                debug_cond(DEBUG_TIGHT, "FillCompression \n");

                nbytes_recv = stream_getN_bytes(pixel_buff, 3, 0);
                if (nbytes_recv < 0) {
                    perror("stream_getN_bytes()");
                    return -1;
                }

                debug_cond(DEBUG_TIGHT, "fill by pixel:");  mem2hex(DEBUG_TIGHT, pixel_buff, 4, 30);
                fb_update_tight_fillcompression(pixel_buff,  x_start,  y_start,  x_res,  y_res);

                //---------  JpegCompression  ---------//
            } else if( (compression_control >> 4u) == 9 ) {
                printf(".....JpegCompression not implemented yet!!! \n");
                exit(-1);

            } else {
                //---------  Invalid compression  ---------//
                fprintf(stderr, ".....Invalid compression type inside Tight!!! \n");
                exit(-1);
            }
        }


        //---------  BasicCompression ---------//
        if( BIT_CHECK(compression_control, 7u) == 0 ) {
            debug_cond(DEBUG_TIGHT, "BasicCompression...\n");

            // Find out 'filter-id' value
            uint8_t filter_id = 0;
            if (BIT_CHECK(compression_control, 6u)) {
                nbytes_recv = stream_getN_bytes((char *) &filter_id, 1, 0);
                if (nbytes_recv < 0) {
                    perror("stream_getN_bytes()");
                    return -1;
                }
            }
            debug_cond(DEBUG_TIGHT, "filter_id = %d \n", filter_id);

            // Find out 'Use stream' value
            BIT_CLEAR(compression_control, 6u);
            uint8_t use_stream = compression_control >> 4u;
            debug_cond(DEBUG_TIGHT255, "use_stream = %d \n", use_stream);


            switch( filter_id ) {
                case COPY_FILTER: {
                    debug_cond(DEBUG_TIGHT, "CopyFilter... \n");

                    size_t z_data_len = jpeg_zlib_data_len();
                    debug_cond(DEBUG_TIGHT, "z_data_len = %ld \n", z_data_len);

                    // Read Z data
                    nbytes_recv = stream_getN_bytes(z_buff, z_data_len, 0);
                    if (nbytes_recv < 0) {
                        perror("stream_getN_bytes()");
                        return -1;
                    }
                    debug_cond(DEBUG_TIGHT, "z_data:");
                    mem2hex(DEBUG_TIGHT, z_buff, 15, 30);

                    size_t uncompress_data_len = z_decompress(pixel_buff, pixel_buff_size, z_buff, z_data_len);
                    debug_cond(DEBUG_TIGHT, "Uncompressed Z_Data = %d \n", uncompress_data_len);

                    fb_update_tight_copyfilter(pixel_buff, x_start, y_start, x_res, y_res);

                    break;
                }

                case PALETTE_FILTER: {
                    debug_cond(DEBUG_TIGHT, "PaletteFilter... \n");

                    debug_cond(DEBUG_TIGHT, "Not implemented yet... \n");
                    //exit( -1 );
                    return STATUS_OK;
                    break;
                }

                case GRADIENT_FILTER: {
                    debug_cond(DEBUG_TIGHT, "GradientFilter... \n");

                    debug_cond(DEBUG_TIGHT, "Not implemented yet... \n");
                    exit( -1 );
                    break;
                }

                default: {
                    debug_cond(DEBUG_TIGHT, "Unknown Filter... \n");
                    debug_cond(DEBUG_TIGHT, "Will never implemented! \n");
                    exit( -1 );
                }
            }
        }

    }


    return STATUS_OK;
}



ssize_t z_decompress(char* pixel_buff_p, size_t  pix_buff_len, char* z_buff_p, size_t z_data_len)
{
    if (DEBUG_TIGHT) printf("%s() \n", __FUNCTION__);

    int result;
    size_t pixel_buff_offset = 0;

    zStream.next_in  = (Bytef*)z_buff_p;
    zStream.avail_in = z_data_len;

    do {
        zStream.next_out = (Bytef*)(pixel_buff_p + pixel_buff_offset);
        zStream.avail_out = pix_buff_len;

        result = inflate(&zStream, Z_NO_FLUSH);
        if(result == Z_NEED_DICT || result == Z_DATA_ERROR || result == Z_MEM_ERROR)
        {
            fprintf(stderr, "inflate(...) failed: %d\n", result);
            inflateEnd(&zStream);
            return -1;
        }

        uint32_t uncompressed_bytes = pix_buff_len - zStream.avail_out;
        pixel_buff_offset += uncompressed_bytes;

    } while (zStream.avail_out == 0);


    return pixel_buff_offset;
}


size_t jpeg_zlib_data_len(void)
{
    if (DEBUG_TIGHT) printf("%s() \n", __FUNCTION__);

    uint8_t len_arr[3] = {'\0'};
    int nbytes_recv;
    size_t z_data_len = 0;
    uint8_t z_len_123_bytes = 0;

    nbytes_recv = stream_getN_bytes((char*)len_arr, 3, MSG_PEEK);
    if (nbytes_recv < 0) {
        perror("stream_getN_bytes()");
        return -1;
    }

    // *** Calculate compressed data length***
    // 1 byte  example:
    // len_arr[0] = 0x4A; ===> 74
    // 2 bytes example:
    // len_arr[0] = 0x90; len_arr[1] = 0x4e;    ===> 10.000
    // len_arr[0] = 0xde; len_arr[1] = 0x42;    ===> 8.542
    // len_arr[0] = 0xcf; len_arr[1] = 0x39;    ===> 7.375
    // 3 bytes example:
    // len_arr[0] = 0x90; len_arr[1] = 0x8b; len_arr[2] = 0xc1; ===> 3.163.536


    if ((BIT_CHECK(len_arr[0], 7u) == 1) && (BIT_CHECK(len_arr[1], 7u) == 1)) {
        debug_cond(DEBUG_TIGHT, "Length represent as 3 bytes \n");
        BIT_CLEAR(len_arr[0], 7u);
        BIT_CLEAR(len_arr[1], 7u);
        z_len_123_bytes = 3;
        z_data_len = (len_arr[2] << 14u) | (len_arr[1] << 7u) | len_arr[0];

    } else if ((BIT_CHECK(len_arr[0], 7u) == 1) && (BIT_CHECK(len_arr[1], 7u) == 0)) {
        debug_cond(DEBUG_TIGHT, "Length represent as 2 bytes \n");
        BIT_CLEAR(len_arr[0], 7u);
        z_len_123_bytes = 2;
        z_data_len = (len_arr[1] << 7u) | len_arr[0];

    } else {
        debug_cond(DEBUG_TIGHT, "Length represent as 1 byte \n");
        z_len_123_bytes = 1;
        z_data_len = len_arr[0];
    }

    // Actual read N bytes to /dev/null
    nbytes_recv = stream_getN_bytes((char*)len_arr, z_len_123_bytes, 0);
    if (nbytes_recv < 0) {
        perror("stream_getN_bytes()");
        return -1;
    }

    return z_data_len;
}

