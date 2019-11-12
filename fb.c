#define pr_fmt(fmt) "  _" fmt
#define DEBUG_FB 0
#define DEBUG_FB1 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "global.h"
#include "logerr.h"
#include "utils.h"


uint8_t *videoFB_ptr;

struct fb_fix_screeninfo fixinfo;
struct fb_var_screeninfo varinfo;


struct memory_fb{
    int x_res;
    int y_res;

    uint8_t *ptr;
} backFB;



static inline uint32_t mix_pixel_color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << varinfo.red.offset) | (g << varinfo.green.offset) | (b << varinfo.blue.offset);
}


int fb_open_dev(char *fb_name) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);

    int tmp_fd = open(fb_name, O_RDWR);
    if( tmp_fd == -1) {
        perror("opening framebuffer device");
        exit(EXIT_FAILURE);
    }

    // Get variable screen information
    ioctl(tmp_fd, FBIOGET_VSCREENINFO, &varinfo);
    varinfo.grayscale=0;
    varinfo.bits_per_pixel=32;
    ioctl(tmp_fd, FBIOPUT_VSCREENINFO, &varinfo);
    ioctl(tmp_fd, FBIOGET_VSCREENINFO, &varinfo);

    ioctl(tmp_fd, FBIOGET_FSCREENINFO, &fixinfo);

    //debug_cond(DEBUG_FB, "Server %s:[%d] \n");

    long screensize = varinfo.yres_virtual * fixinfo.line_length;
        debug_cond(DEBUG_FB, "line_length = %d, yres_virtual = %d \n", fixinfo.line_length, varinfo.yres_virtual);

    videoFB_ptr = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, tmp_fd, (off_t)0);


    backFB.x_res = fixinfo.line_length;
    backFB.y_res = varinfo.yres_virtual;

    backFB.ptr = (uint8_t *)malloc(backFB.x_res * backFB.y_res * sizeof(char));
    if (backFB.ptr == NULL) {
        perror("malloc()");
        return -1;
    }


    return tmp_fd;
}



int fb_update_raw(char *rect_ptr,
        uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    {
        debug_cond(DEBUG_FB, "Input Rect: X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);
        debug_cond(DEBUG_FB, "backFB.y_res = %d \n", backFB.y_res);
        mem2hex(DEBUG_FB, rect_ptr, 30, 50);
    }


    int y;
    for (y = 0; y < y_res; y++) {
        long location = (x_start * 4) + (y_start + y)*fixinfo.line_length;
        memcpy( backFB.ptr + location, rect_ptr, x_res * 4 );
        rect_ptr += x_res * 4;
    }


/*
    // Universal implementation for all cases
    int x, y;
    for (y = 0; y < y_res; y++) {
        for (x = 0; x < x_res; x++) {

            long location = (x_start + x)*4 + (y_start + y)*fixinfo.line_length;

            *((uint32_t *)(backFB.ptr + location)) =
                    mix_pixel_color( *(rect_ptr + 2), *(rect_ptr + 1), *rect_ptr );

            rect_ptr += 4;
        }
    }
*/

    debug_cond(DEBUG_FB, "Output backFB \n");
    mem2hex(DEBUG_FB, backFB.ptr, 30, 50);
}


int fb_update_copyrect(uint16_t src_x_pos, uint16_t src_y_pos,
                       uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    {
        debug_cond(DEBUG_FB, "CopyRect - from  X=%d, Y=%d, %dx%d \n", src_x_pos, src_y_pos, x_res, y_res);
        debug_cond(DEBUG_FB, "CopyRect -   to  X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);
        //mem2hex(DEBUG_FB, rect_ptr, 30, 50);
    }

    int iter;
    for (iter = 0; iter < y_res; iter++) {

        long src_location = (src_y_pos + iter) * fixinfo.line_length + (src_x_pos * 4);

        long dst_location =   (y_start + iter) * fixinfo.line_length + (x_start * 4);

        memcpy(backFB.ptr + dst_location, backFB.ptr + src_location, x_res * 4);
    }
}


int fb_update_tight(char *rect_ptr,
                     uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    {
        debug_cond(DEBUG_FB, "Input Rect: X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);
        debug_cond(DEBUG_FB, "backFB.y_res = %d \n", backFB.y_res);
    }


    // Universal implementation for all cases
    int x, y;
    for (y = 0; y < y_res; y++) {
        for (x = 0; x < x_res; x++) {

            long location = (y_start + y) * fixinfo.line_length + (x_start + x) * 4;
            *((uint32_t *)(backFB.ptr + location)) = mix_pixel_color(
                    *(unsigned char*)(rect_ptr + 0),
                    *(unsigned char*)(rect_ptr + 1),
                    *(unsigned char*)(rect_ptr + 2));

            rect_ptr += 3;
        }
    }

    debug_cond(DEBUG_FB, "Output backFB \n");
    mem2hex(DEBUG_FB, backFB.ptr, 30, 50);
}


int fb_mem2video(void) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);

    long screensize = backFB.x_res * backFB.y_res * sizeof(char);
    memcpy(videoFB_ptr, backFB.ptr, screensize);

    {
        debug_cond(DEBUG_FB, "Actual videoFB \n");
        mem2hex(DEBUG_FB, videoFB_ptr, 30, 50);
    }
}
