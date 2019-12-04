#define pr_fmt(fmt) "  _" fmt
#define DEBUG_FB    0
#define DEBUG_FB127 0
#define DEBUG_FB255 1

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


struct front_fb_t{
    uint32_t mem_len;

    uint8_t *ptr;
} frontFB;


struct back_fb_t{
    uint16_t x_res;
    uint16_t y_res;
    uint16_t line_length;

    uint32_t red_offset;
    uint32_t green_offset;
    uint32_t blue_offset;

    uint8_t *ptr;
} backFB;


void print_fb_info(struct fb_fix_screeninfo *fixinfo, struct fb_var_screeninfo *varinfo) {
    fprintf(stdout, "--- fb_fix_screeninfo ---\n");
    fprintf(stdout, "id:\t\t %s \n",         fixinfo->id);
    fprintf(stdout, "smem_start:\t %ld \n",  fixinfo->smem_start);
    fprintf(stdout, "smem_len:\t %d \n",     fixinfo->smem_len);
    fprintf(stdout, "visual:\t\t %d \n",     fixinfo->visual);
    fprintf(stdout, "xpanstep:\t %d \n",     fixinfo->xpanstep);
    fprintf(stdout, "ypanstep:\t %d \n",     fixinfo->ypanstep);
    fprintf(stdout, "ywrapstep:\t %d \n",    fixinfo->ywrapstep);
    fprintf(stdout, "line_length:\t %d \n",  fixinfo->line_length);
    fprintf(stdout, "mmio_start:\t %ld \n",  fixinfo->mmio_start);
    fprintf(stdout, "mmio_len:\t %d \n",     fixinfo->mmio_len);
    fprintf(stdout, "accel:\t\t %d \n",      fixinfo->accel);
    fprintf(stdout, "capabilities:\t %d \n", fixinfo->capabilities);


    fprintf(stdout, "--- fb_var_screeninfo ---\n");
    fprintf(stdout, "xres:\t\t %d \n",         varinfo->xres);
    fprintf(stdout, "yres:\t\t %d \n",         varinfo->yres);
    fprintf(stdout, "xres_virtual:\t %d \n",   varinfo->xres_virtual);
    fprintf(stdout, "yres_virtual:\t %d \n",   varinfo->yres_virtual);
    fprintf(stdout, "xoffset:\t %d \n",        varinfo->xoffset);
    fprintf(stdout, "yoffset:\t %d \n",        varinfo->yoffset);
    fprintf(stdout, "bits_per_pixel:\t %d \n", varinfo->bits_per_pixel);
    fprintf(stdout, "grayscale:\t %d \n",      varinfo->grayscale);

    fprintf(stdout, "\n\n");
}


static inline uint32_t mix_pixel_color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << backFB.red_offset) | (g << backFB.green_offset) | (b << backFB.blue_offset);
}


/*
 * Open FrameBuffer device
 * and set all global FB parameters
 *
 */
int fb_open_dev(char *fb_name) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);

    struct fb_fix_screeninfo fixinfo;
    struct fb_var_screeninfo varinfo;

    int tmp_fd = open(fb_name, O_RDWR);
    if( tmp_fd == -1 ) {
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

    if( DEBUG_FB255 ) print_fb_info(&fixinfo, &varinfo);
    frontFB.mem_len = fixinfo.smem_len;

    backFB.x_res = varinfo.xres_virtual;
    backFB.y_res = varinfo.yres_virtual;
    backFB.line_length = fixinfo.line_length;

    backFB.red_offset = varinfo.red.offset;
    backFB.green_offset = varinfo.green.offset;
    backFB.blue_offset = varinfo.blue.offset;


    frontFB.ptr = (uint8_t*)mmap(0, frontFB.mem_len, PROT_READ | PROT_WRITE, MAP_SHARED, tmp_fd, (off_t)0);
    if( frontFB.ptr == NULL ) {
        perror("mmap()");
        return -1;
    }

    backFB.ptr = (uint8_t*)malloc(frontFB.mem_len * sizeof(char));
    if( backFB.ptr == NULL ) {
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
        long location = (y_start + y) * backFB.line_length + (x_start * 4);
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

        long src_location = (src_y_pos + iter) * backFB.line_length + (src_x_pos * 4);

        long dst_location = (y_start + iter) * backFB.line_length + (x_start * 4);

        memcpy(backFB.ptr + dst_location, backFB.ptr + src_location, x_res * 4);
    }
}


int fb_update_tight_copyfilter(char *rect_ptr,
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

            long location = (y_start + y) * backFB.line_length + (x_start + x) * 4;
            *((uint32_t *)(backFB.ptr + location)) = mix_pixel_color(
                    *(unsigned char*)(rect_ptr + 0),
                    *(unsigned char*)(rect_ptr + 1),
                    *(unsigned char*)(rect_ptr + 2));

            rect_ptr += 3;
        }
    }

    debug_cond(DEBUG_FB, "Output backFB:"); mem2hex(DEBUG_FB, backFB.ptr, 30, 50);
}


int fb_update_tight_fillcompression(char *rect_ptr,  uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res)
{
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    debug_cond(DEBUG_FB, "Input Rect: X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);

    uint32_t fill_pixel = mix_pixel_color(
            *(unsigned char*)(rect_ptr + 0),
            *(unsigned char*)(rect_ptr + 1),
            *(unsigned char*)(rect_ptr + 2));
    debug_cond(DEBUG_FB, "Fill rect by pixel: 0x%08x \n", fill_pixel);


    int x, y;
    for (y = 0; y < y_res; y++) {
        for (x = 0; x < x_res; x++) {

            long location = (y_start + y) * backFB.line_length + (x_start + x) * 4;
            *((uint32_t *)(backFB.ptr + location)) = fill_pixel;
        }
    }
}


int fb_update_tight_palette(char *rect_ptr,
                               uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res,
                               uint8_t palette_arr[256*3])
{
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    {
        debug_cond(DEBUG_FB, "Input Rect: X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);

        debug_cond(DEBUG_FB, "Palette array (first 5 colors):");
        mem2hex(DEBUG_FB, palette_arr, 15, 15);
    }

    int x, y;
    for (y = 0; y < y_res; y++) {
        for (x = 0; x < x_res; x++) {

            long location = (y_start + y) * backFB.line_length + (x_start + x) * 4;

            uint16_t indexed_color = *(unsigned char*)rect_ptr;

            *((uint32_t *)(backFB.ptr + location)) = mix_pixel_color(
                    palette_arr[3 * indexed_color + 0],
                    palette_arr[3 * indexed_color + 1],
                    palette_arr[3 * indexed_color + 2]);

            rect_ptr++;
        }
    }

    debug_cond(DEBUG_FB, "Output backFB: \n"); mem2hex(DEBUG_FB, backFB.ptr, 30, 50);
}


int fb_update_tight_palette_1_bit(char *rect_ptr,
                            uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res,
                            uint8_t palette_arr[256*3])
{
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);
    {
        debug_cond(DEBUG_FB, "Input Rect: X=%d, Y=%d, %dx%d \n", x_start, y_start, x_res, y_res);

        debug_cond(DEBUG_FB, "Palette array (two colors):");
        mem2hex(DEBUG_FB, palette_arr, 6, 15);
    }

    uint32_t color_0 = mix_pixel_color( palette_arr[0], palette_arr[1], palette_arr[2]);
    uint32_t color_1 = mix_pixel_color( palette_arr[3], palette_arr[4], palette_arr[5]);

    int shift = 0;
    int x, y;
    for (y = 0; y < y_res; y++) {
        for (x = 0; x < x_res; x++)
        {
            long location = (y_start + y) * backFB.line_length + (x_start + x) * 4;

            if( (x != 0) && ((x % 8) == 0) )
                shift++;

            if( BIT_CHECK( *(rect_ptr + shift), x % 8) == 0 )
                *((uint32_t *)(backFB.ptr + location)) = color_0;
            else
                *((uint32_t *)(backFB.ptr + location)) = color_1;

            debug_cond(DEBUG_FB, "  %dx%d,\t shift: %d,  bit-N: %d \n",  x, y,  shift, x%8);

            rect_ptr++;
        }

        printf("------- x = %d \n",x);
        shift++;
    }

    return STATUS_OK;
}


int backFB_to_frontFB(void) {
    if (DEBUG_FB) printf("-----\n%s() \n", __func__);

    long screensize = backFB.line_length * backFB.y_res * sizeof(char);
    memcpy(frontFB.ptr, backFB.ptr, screensize);

    {
        debug_cond(DEBUG_FB, "Actual videoFB \n");
        mem2hex(DEBUG_FB, frontFB.ptr, 30, 50);
    }
}
