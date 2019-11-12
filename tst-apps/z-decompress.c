#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <zlib.h>

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


z_stream zStream = { 0 };


ssize_t z_decompress(char* pixel_buff_p, size_t  pix_buff_len, char* z_buff_p, size_t z_data_len)
{
    printf("  z_decompress()\n");

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
        printf("  ...pixel_buff_offset = %ld \n",  pixel_buff_offset);

    } while (zStream.avail_out == 0);


    return pixel_buff_offset;
}



#define PIXEL_BUFF_SIZE     1024 * 500
char pixel_buff[PIXEL_BUFF_SIZE];


int main(int argc, char* argv[])
{
    FILE *in_file;
    FILE *out_file = fopen("_my_pixel_buff.bin", "wb");

    uint8_t iter;
    size_t uncompress_data_len;
    ssize_t pixel_buff_len = sizeof(pixel_buff);


	if(argc >= 2 && strcmp(argv[1], "-h") == 0)
	{
		fprintf(stderr, "Usage: %s  z_data_1.bin  z_data_2.bin   z_data_N.bin \n", argv[0]);
		exit(1);
	}

    //--------------------------------------------
    int result = inflateInit(&zStream);
    if (result != Z_OK)
    {
        fprintf(stderr, "inflateInit(...) failed!\n");
        return false;
    }


    //--------------------------------------------
    for( iter = 1; iter < argc; iter++) {
        in_file = fopen(argv[iter], "rb");
        struct stat st;
        stat(argv[iter], &st);
        size_t z_data_len = st.st_size;
        printf("File '%s' [len# %ld] \n", argv[iter], z_data_len);

        char *z_data_p = (char*)malloc(st.st_size * sizeof(char));
        fread(z_data_p, 1, z_data_len, in_file);
        if (ferror(in_file))
        {
            fprintf(stderr, "fread(...) failed!\n");
            inflateEnd(&zStream);
            return false;
        }

        uncompress_data_len = z_decompress(pixel_buff, pixel_buff_len, z_data_p, z_data_len);
        fprintf(stderr, "File '%s' uncompressed = %ld \n\n",  argv[iter], uncompress_data_len);

        fwrite(pixel_buff, 1, uncompress_data_len, out_file);


        free(z_data_p);
        fclose(in_file);
        printf("\n");
    }

    //--------------------------------------------


    fclose(out_file);
    inflateEnd(&zStream);
	return 0;
}

