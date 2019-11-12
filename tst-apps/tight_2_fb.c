#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "memory.h"
#include <zlib.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))        // '!!' to make sure this returns 0 or 1



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


static inline uint32_t mix_pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *varinfo)
{
    return (r << varinfo->red.offset) | (g << varinfo->green.offset) | (b << varinfo->blue.offset);
}


#define  MSG_LEN 8506

int main(int argc, char *argv[])
{
    unsigned char msg[MSG_LEN];
    uint8_t compress_control;
    uint8_t reset_stream[4] = {0};

    Bytef *decompress_buff = NULL;

    unsigned int iter;

    FILE *ptr;
    ptr = fopen("_tight_rect.bin", "rb");
    fread(msg, sizeof(msg), 1, ptr);
    printf("  First 15 bytes of 'Tight msg': \n");
    mem2hex(1, msg, 15, 30);


    compress_control = *(msg);

    for( iter = 0; iter < 4; iter++ ) {
        if( BIT_CHECK(compress_control, iter) )
            reset_stream[iter] = 1;
    }
    printf("\n");
    for( iter = 0; iter < 4; iter++ )
        printf("Reset stream %d = %d \n", iter, reset_stream[iter]);
    printf("\n");


    if( BIT_CHECK(compress_control, 7u) ) {
        printf("  FillCompression or JpegCompression \n");

    } else {
        printf("  BasicCompression \n");

        uint8_t read_filter_id = 0;
        if (BIT_CHECK(compress_control, 6u))
            read_filter_id = 1;
        printf("   read_filter_id = %d \n", read_filter_id);

        BIT_CLEAR(compress_control, 6u);
        uint8_t use_stream = compress_control >> 4u;
        printf("   use_stream = %d \n", use_stream);

        // *** Calculate zip compressed data length***
        // 1 byte  example:
        //*(msg + 1) = 0x4A; // => 74
        // 2 bytes example:
        //*(msg + 1) = 0x90; *(msg + 2) = 0x4e; // => 10.000
        //*(msg + 1) = 0xde; *(msg + 2) = 0x42; // => 8.542
        // 3 bytes example:
        //*(msg + 1) = 0x90; *(msg + 2) = 0x8b; *(msg + 3) = 0xc1; // => 3.163.536

        unsigned char *len_ptr = msg + 1;
        uint32_t zlib_data_len = 0;
        unsigned char *zlib_buff;

        if ((BIT_CHECK(*(len_ptr), 7u) == 1) && (BIT_CHECK(*(len_ptr + 1), 7u) == 1)) {
            printf("Length represent as 3 bytes \n");
            BIT_CLEAR(*(len_ptr), 7u);
            BIT_CLEAR(*(len_ptr + 1), 7u);
            zlib_data_len = (*(len_ptr + 2) << 14u) | (*(len_ptr + 1) << 7u) | *(len_ptr);

            zlib_buff = msg + 4;
        } else if ((BIT_CHECK(*(len_ptr), 7u) == 1) && (BIT_CHECK(*(len_ptr + 1), 7u) == 0)) {
            printf("Length represent as 2 bytes \n");
            BIT_CLEAR(*(len_ptr), 7u);
            zlib_data_len = (*(len_ptr + 1) << 7u) | *(len_ptr);

            zlib_buff = msg + 3;
        } else {
            printf("Length represent as 1 byte \n");
            zlib_data_len = *(len_ptr);

            zlib_buff = msg + 2;
        }

        printf("   zlib_data_len = %d \n", zlib_data_len);
        mem2hex(1, (char*)zlib_buff, 10, 30);


        //---------------------------
        uLongf decompressed_buff_size = 1024 * 64 * 5;
        decompress_buff = (Bytef*) malloc(decompressed_buff_size * sizeof(char));
        memset(decompress_buff, '\0', decompressed_buff_size);
        if (decompress_buff == NULL) {
            fprintf(stderr, "malloc(decompress_buff_size) failed, decompress_buff_size = %lu\n",
                    decompressed_buff_size);
            exit(1);
        }


        z_stream infstream;
        infstream.zalloc = Z_NULL;
        infstream.zfree = Z_NULL;
        infstream.opaque = Z_NULL;

        infstream.avail_in = (uInt)zlib_data_len;           // size of input
        infstream.next_in = (Bytef *)zlib_buff;             // input char array
        infstream.avail_out = (uInt)decompressed_buff_size; // size of output
        infstream.next_out = (Bytef *)decompress_buff;      // output char array


        // the actual DE-compression work.
        inflateInit(&infstream);
        long res = inflate(&infstream, Z_SYNC_FLUSH);
        if( res == Z_OK ) {
            printf("Total len of decompress data = %ld \n", infstream.total_out);

        } else {
            fprintf(stderr, "inflate(...) failed, res = %ld \n", res);
            mem2hex(1, (char*)decompress_buff, 10, 30);
            //exit(1);
        }
        inflateEnd(&infstream);


    }



    // ----------------------------------------
    struct fb_fix_screeninfo fixinfo;
    struct fb_var_screeninfo varinfo;

    int fb_fd = open("/dev/fb0",O_RDWR);

    // Get variable screen information
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &varinfo);
    varinfo.grayscale=0;
    varinfo.bits_per_pixel=32;
    ioctl(fb_fd, FBIOPUT_VSCREENINFO, &varinfo);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &varinfo);

    ioctl(fb_fd, FBIOGET_FSCREENINFO, &fixinfo);
/*
    printf("fb_fix_screeninfo \n");
    printf("  id: %s \n", fixinfo.id);
    printf("  smem_start: %ld \n", fixinfo.smem_start);
    printf("  smem_len: %d \n", fixinfo.smem_len);
    printf("  xpanstep: %d \n", fixinfo.xpanstep);
    printf("  line_length: %d \n", fixinfo.line_length);

    printf("fb_var_screeninfo \n");
    printf("  xres: %d \n", varinfo.xres);
    printf("  yres: %d \n", varinfo.yres);
    printf("  xres_virtual: %d \n", varinfo.xres_virtual);
    printf("  yres_virtual: %d \n", varinfo.yres_virtual);
    printf("  xoffset: %d \n", varinfo.xoffset);
    printf("  yoffset: %d \n", varinfo.yoffset);
*/


    // -------------------------------------------------------------
    long screensize = varinfo.yres_virtual * fixinfo.line_length;
    uint8_t *fb_ptr = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

    // Rect# 1, Xpos=0  Ypos=0, 1024x64, EncodeType: 7

    uint16_t x_start = 0;
    uint16_t y_start = 0;

    uint16_t x_res = 1024;
    uint16_t y_res = 64;


    int x, y;
    for( y = 0; y < y_res; y++ ) {
        for( x = 0; x < x_res; x++ ) {

            long location = (y_start + y) * fixinfo.line_length + (x_start + x) * 4;
            *((uint32_t*)(fb_ptr + location)) = mix_pixel_color(
                    *(unsigned char*)(decompress_buff + 0),
                    *(unsigned char*)(decompress_buff + 1),
                    *(unsigned char*)(decompress_buff + 2),  &varinfo);

            decompress_buff +=3;
        }
    }


    return 0;
}


/*
        //---  Try to decompress here  ---//
        //zlib_buff = msg + 4;
        long res = uncompress(decompress_buff, &decompressed_buff_size, zlib_buff, zlib_data_len);
        if( res == Z_OK ) {
            printf("Total len of decompress data = %ld \n", res);
            fprintf(stderr, "decompressed_buff_size = %ld \n", decompressed_buff_size);

        } else {
            fprintf(stderr, "uncompress(...) failed, err = %ld \n", res);
            fprintf(stderr, "decompressed_buff_size = %ld \n", decompressed_buff_size);
            mem2hex(1, (char*)decompress_buff, 10, 30);
            //exit(1);
        }
*/