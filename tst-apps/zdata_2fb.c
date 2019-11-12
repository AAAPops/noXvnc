#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <memory.h>
#include <zlib.h>
#include <sys/stat.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))        // '!!' to make sure this returns 0 or 1

#define DECOMP_DATA_SIZE    1024 * 200 * 3

z_stream infstream;
uint8_t z_buff[15000];

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


int z_decompress(char *pixel_buff, uint32_t pixel_buff_size, uint32_t z_data_len) {
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    infstream.avail_in = (uInt)z_data_len;          // size of input
    infstream.next_in = (Bytef *)z_buff;            // input char array
    infstream.avail_out = (uInt)pixel_buff_size;    // size of output
    infstream.next_out = (Bytef *)pixel_buff;       // output char array


    // the actual DE-compression work.
    inflateInit(&infstream);
    long res = inflate(&infstream, Z_SYNC_FLUSH);
    if( res == Z_OK ) {
        printf("Total len of inflate() data = %ld \n", infstream.total_out);

    } else {
        printf("  inflate() failed, err = %ld \n", res);
        mem2hex(1, (char*)pixel_buff, 200, 30);
        //exit(-1);
    }

}


int main(int argc, char *argv[])
{
    Bytef  decompressed_buff[DECOMP_DATA_SIZE] = {'\0'};
    uLongf decompressed_buff_size = DECOMP_DATA_SIZE;

    FILE *f_ptr;

    int iter;

    f_ptr = fopen(argv[1], "rb");
    struct stat st;
    stat(argv[1], &st);
    size_t z_data_len = st.st_size;


    fread(z_buff, z_data_len, 1, f_ptr);

    printf("  z_data_len = %d,  z_buff: ", z_data_len);
    mem2hex(1, (char*)z_buff, 15, 30);

    z_decompress((char*)decompressed_buff, decompressed_buff_size, z_data_len);

    inflateEnd(&infstream);
    return 0;
}


/*
int show_fb(void) {
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


    // -------------------------------------------------------------
    long screensize = varinfo.yres_virtual * fixinfo.line_length;
    uint8_t *fb_ptr = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

    uint16_t x_start = 0;
    uint16_t y_start = 0;

    uint16_t x_res = 1024;
    uint16_t y_res = 150;

    Bytef *decomp_buff_p = decompressed_buff;

    int x, y;
    for( y = 0; y < y_res; y++ ) {
        for( x = 0; x < x_res; x++ ) {

            long location = (y_start + y) * fixinfo.line_length + (x_start + x) * 4;
            *((uint32_t*)(fb_ptr + location)) = mix_pixel_color(
                    *(unsigned char*)(decomp_buff_p + 0),
                    *(unsigned char*)(decomp_buff_p + 1),
                    *(unsigned char*)(decomp_buff_p + 2),  &varinfo);

            decomp_buff_p += 3;
        }
    }

}
*/