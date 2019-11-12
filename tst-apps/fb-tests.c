#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "stdlib.h"
#include <arpa/inet.h>

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

int main(int argc, char *argv[])
{
    char *file_buff_ptr, *fl_ptr;
    size_t buf_len = strtol(argv[1] , NULL, 10);

    file_buff_ptr = (char *)malloc(buf_len * sizeof(char));
    if( file_buff_ptr == NULL) {
        perror("malloc()");
        return -1;
    }

    FILE *file_ptr;
    file_ptr = fopen("/tmp/msg_srv_fbupdate.bin","rb");
    fread(file_buff_ptr, buf_len, 1, file_ptr);
    mem2hex(1, file_buff_ptr, 100, 30);

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



    // -------------------------------------------------------------

    // Print info about 'srv_fbupdate' message
    fl_ptr = file_buff_ptr;
    uint16_t num_of_rect = ntohs( *(uint16_t *)(fl_ptr + 2) );
        printf("\n-------------------- num_of_rect: %d \n", num_of_rect);


    long screensize = varinfo.yres_virtual * fixinfo.line_length;
    uint8_t *fb_ptr = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

    int iter;
    fl_ptr += 4;
    for( iter = 1; iter <= num_of_rect; iter++ ) {
    //for( iter = 1; iter <= 2; iter++ ) {
            printf("\n  Ptr_Shift: %ld \n", fl_ptr - file_buff_ptr);
            mem2hex(1, fl_ptr, 16, 25);
        uint16_t x_position  = ntohs( *(uint16_t *)(fl_ptr + 0));
        uint16_t y_position  = ntohs( *(uint16_t *)(fl_ptr + 2));
        uint16_t width       = ntohs( *(uint16_t *)(fl_ptr + 4));
        uint16_t height      = ntohs( *(uint16_t *)(fl_ptr + 6));
        uint32_t encode_type = ntohl( *(uint32_t *)(fl_ptr + 8));
            printf("  Rect #%d: Start: X=%d  Y=%d, Resolution: %dx%d, Type: %d \n", iter, x_position, y_position, width, height, encode_type);

        fl_ptr += 12;


        int x, y;
        for( y = 0; y < height; y++ ) {
            for( x = 0; x < width; x++ )
            {
                long location = (x_position + x + varinfo.xoffset) * (varinfo.bits_per_pixel/8) +
                                (y_position + y + varinfo.yoffset) * fixinfo.line_length;
                *((uint32_t*)(fb_ptr + location)) = mix_pixel_color(*(fl_ptr+2), *(fl_ptr+1), *(fl_ptr+0), &varinfo);
                fl_ptr += 4;
            }
        }

        //fl_ptr += width * height * 4;
    }





    return 0;
}
