#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

// Mine
#include <dirent.h>
#include <string.h>
#include <stdint.h>


/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))        // '!!' to make sure this returns 0 or 1



#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME  "event"
#define MAX_MOUSE_EVS    10


/* Convert memory dump to hex  */
void mem2hex(int cond, unsigned char *str, int len, int colum_counts) {
    int x;

    if (cond) {
        printf("  ");

        for (x = 0; x < len; x++) {
            if (x > 0 && x % colum_counts == 0)
                printf("\n  ");
            printf("%02x ", str[x]);
        }
        printf("\n");
    }
}


/**
 * Filter for the AutoDevProbe scandir on /dev/input.
 *
 * @param dir The current directory entry provided by scandir.
 *
 * @return Non-zero if the given directory entry starts with "event", or zero
 * otherwise.
 */
static int is_event_device(const struct dirent *dir) {
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}


int find_mouse_dev(char *ev_name[], int ev_name_max)
{
    struct dirent **namelist;
    int iter;
    int ndev;
    int ev_name_iter = 0;
    int retval;

    uint32_t ev_type;
    uint8_t ev_code[KEY_MAX/8 + 1];

    ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);
    if (ndev <= 0)
        return -1;

    for (iter = 0; iter < ndev; iter++) {
        char fname[64];
        int fd = -1;
        char name[256] = {"\0"};
        struct input_id device_info;

        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT, namelist[iter]->d_name);
        fd = open(fname, O_RDONLY);
        if (fd < 0)
            continue;
        //ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_type)), &ev_type) < 0) {
            perror("evdev ioctl(EVIOCGBIT)");
            retval = -1;
            goto out;
        }
        //printf("   ev_type = 0x%x, ", ev_type);
        //mem2hex(1, (char*)&ev_type, sizeof(ev_type), 32);

        if( BIT_CHECK(ev_type, EV_KEY) ) {
            memset(ev_code, 0, sizeof(ev_code));

            if( ioctl(fd, EVIOCGBIT(EV_KEY, (KEY_MAX/8+1)), ev_code) < 0 ) {
                perror("evdev ioctl(EVIOCGBIT) ");
                retval = -1;
                goto out;
            }

            //mem2hex(1, (char*)ev_code, sizeof(ev_code), 32);
            //printf("        evcode #34 = 0x%x \n", *(ev_code + 34) );
            // BTN_MOUSE = 0x110 == 272, so we have to check bit #272 (34 * 8 = 272)
            if( BIT_CHECK( *(ev_code + 34), 0) )
            {
                sprintf(ev_name[ev_name_iter], fname);
                retval = ++ev_name_iter;

                if( ev_name_iter == ev_name_max ) {
                    close(fd);
                    goto out;
                }
            }
        }

        close(fd);
    }


out:
    for (iter = 0; iter < ndev; iter++)
        free(namelist[iter]);
    free(namelist);

    return retval;
}




int main()
{
    int iter;

    // allocate space for N pointers to strings
    char **strings = (char**)malloc(MAX_MOUSE_EVS * sizeof(char*));
    //char *strings[MAX_MOUSE_EVS];

    //allocate space for each string
    for(iter = 0; iter < MAX_MOUSE_EVS; iter++)
        strings[iter] = (char*)malloc(256 * sizeof(char));

    int ret = find_mouse_dev(strings, MAX_MOUSE_EVS);
    if( ret < 1 ) {
        printf("Mouse device not found! \n");
        exit(-1);
    }

    fprintf(stderr, "Find mouse events:\n");
    for( iter = 0; iter < ret; iter++ ) {
        printf("   %s \n", strings[iter]);
        free(strings[iter]);
    }
    free(strings);

    return 0;
/*
    int fd;
    struct input_event ie;
    //
    unsigned char button, bLeft, bMiddle, bRight;
    char x, y;
    int absolute_x = 0;
    int absolute_y = 0;

    if((fd = open(MOUSEFILE, O_RDONLY)) == -1) {
        printf("Device open ERROR\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Device open OK\n");
    }
    //
    printf("right-click to set absolute x,y coordinates origin (0,0)\n");
    while(read(fd, &ie, sizeof(struct input_event)))
    {
        unsigned char *ptr = (unsigned char*)&ie;
        int i;       
        //
        button = ptr[0];
        bLeft = button & 0x1;
        bMiddle = ( button & 0x4 ) > 0;
        bRight = ( button & 0x2 ) > 0;
        x = (char) ptr[1];
        y = (char) ptr[2];
        printf("bLEFT:%d, bMIDDLE: %d, bRIGHT: %d, rx: %d  ry=%d\n",bLeft, bMiddle, bRight, x, y);
        //
        absolute_x += x;
        absolute_y -= y;

        if( bRight == 1 )
        {
            absolute_x = 0;
            absolute_y = 0;
            printf("Absolute x,y coords origin recorded \n");
        }
        //
        printf("Absolute coords from TOP_LEFT= %i %i \n",absolute_x, absolute_y);
        //
        // comment to disable the display of raw event structure datas
        //
        for(i=0; i<sizeof(ie); i++)
        {
            printf("%02X ", *ptr++);
        }
        printf("\n");
    }
*/

return 0;
}
