#define pr_fmt(fmt) "  _" fmt
#define DEBUG_MOUSE     0
#define DEBUG_MOUSE127  0

#define _GNU_SOURCE     // for 'versionsort' support
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/time.h>
#include <sys/select.h>


#include "global.h"
#include "logerr.h"
#include "utils.h"
#include "stream.h"
#include "mouse.h"

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME  "event"

enum {
    MAX_WIDTH  = 1024,
    MAX_HEIGHT = 768,
    VALUE_PRESSED  = 1,
    VALUE_RELEASED = 0,
};

struct mouse_t mouse_event;


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



/*
 * Search 'event*' files in '/dev/input/' directory
 * and test witch one is Mouse devices
 *
 * Output: array of strings with records:
 * '/dev/input/event5'
 * '/dev/input/event22'
 */
int find_mouse_dev(char *ev_name[], int ev_name_max)
{
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    struct dirent **namelist;
    int iter;
    int ndev;
    int ev_name_iter = 0;
    int retval = -1;

    uint32_t ev_type;
    uint8_t ev_code[KEY_MAX/8 + 1];

    ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);
    if (ndev <= 0)
        return -1;

    for (iter = 0; iter < ndev; iter++) {
        char fname[64];
        int fd = -1;

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


        if( BIT_CHECK(ev_type, EV_KEY) ) {
            memset(ev_code, 0, sizeof(ev_code));

            if( ioctl(fd, EVIOCGBIT(EV_KEY, (KEY_MAX/8+1)), ev_code) < 0 ) {
                perror("evdev ioctl(EVIOCGBIT) ");
                retval = -1;
                goto out;
            }

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



/*
 * Set file descriptors for mouse devices
 * '/dev/input/event5'  ==> 4
 * '/dev/input/event22' ==> 5
 *
 * Output: Number of mouse file descriptors
 *
 */
int mouse_open_dev(int *fd_arr) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    int iter;
    int opened_n_mouse = 0;

    // allocate space for N pointers to strings
    char **strings = (char**)malloc(MAX_MOUSE_DEVS * sizeof(char*));
    //char *strings[MAX_MOUSE_EVS];

    //allocate space for each string
    for(iter = 0; iter < MAX_MOUSE_DEVS; iter++)
        strings[iter] = (char*)malloc(256 * sizeof(char));

    int ret = find_mouse_dev(strings, MAX_MOUSE_DEVS);
    if( ret < 1 ) {
        printf("Mouse device not found! \n");
        exit(-1);
    }

    debug_cond(DEBUG_MOUSE, "Find mouse events: \n");
    for( iter = 0; iter < ret; iter++ ) {
        fd_arr[iter] = open(strings[iter], O_RDONLY);
        opened_n_mouse++;

        debug_cond(DEBUG_MOUSE, "   %s [fd: %d] \n", strings[iter], fd_arr[iter] );

        free(strings[iter]);
    }
    free(strings);


    mouse_event.current.x  = mouse_event.current.y  = 70;
    mouse_event.pressed.x  = mouse_event.pressed.y  = 0;
    mouse_event.released.x = mouse_event.released.y = 0;
    mouse_event.button_mask = 0;

    return opened_n_mouse;
}



void mouse_send_pointerevent(struct mouse_t *mouse) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    uint8_t msg[6] = {0};

    //fprintf(stderr, "current(%d, %d),  mask %d \n",
    //mouse->current.x, mouse->current.y, mouse->button_mask);

    *msg = pointerEvent;
    *(msg + 1) = mouse->button_mask;
    *(uint16_t *) (msg + 2) = htons(mouse->current.x);
    *(uint16_t *) (msg + 4) = htons(mouse->current.y);

    stream_send_all(msg, sizeof(msg));

    // For debug purpose only
    if( DEBUG_MOUSE127 ) {
    printf("\033[1;1H\033[2Kcurrent(%d, %d),  mask %d",
            mouse->current.x, mouse->current.y, mouse->button_mask);

    printf("\033[1;1H\033[2Kcurrent(%d, %d) pressed(%d, %d) released(%d, %d)  mask %d",
            mouse->current.x, mouse->current.y,
            mouse->pressed.x, mouse->pressed.y,
            mouse->released.x, mouse->released.y,
            mouse->button_mask);
    }
}

void mouse_update_position(struct input_event *inevnt, struct mouse_t *mouse)
{
    if (inevnt->code == REL_X)
        mouse->current.x += inevnt->value;

    if (mouse->current.x < 0)
        mouse->current.x = 0;
    else if (mouse->current.x >= MAX_WIDTH)
        mouse->current.x = MAX_WIDTH - 1;

    if (inevnt->code == REL_Y)
        mouse->current.y += inevnt->value;

    if (mouse->current.y < 0)
        mouse->current.y = 0;
    else if (mouse->current.y >= MAX_HEIGHT)
        mouse->current.y = MAX_HEIGHT - 1;
}


void mouse_button(struct input_event *inevnt, struct mouse_t *mouse)
{
    if (inevnt->code == BTN_LEFT) {
        if (inevnt->value == VALUE_PRESSED) {
            mouse->pressed = mouse->current;
            BIT_SET(mouse->button_mask, 0u);
        }

        if (inevnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 0u);
        }
    }

    if (inevnt->code == BTN_RIGHT) {
        if (inevnt->value == VALUE_PRESSED) {
            mouse->pressed = mouse->current;
            BIT_SET(mouse->button_mask, 1u);
        }

        if (inevnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 1u);
        }
    }
}



void mouse_handle_event(int mouse_fd) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    struct input_event in_evnt;

    fd_set readfds;
    struct timeval tv;


    int iter;
    for (iter = 0; iter < 25; iter++) {
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        FD_ZERO(&readfds);
        FD_SET(mouse_fd, &readfds);


        if (select(mouse_fd + 1, &readfds, NULL, NULL, &tv) == -1) {
            perror("select()");
            break;
        }

        if( FD_ISSET(mouse_fd, &readfds) )
        {
            read(mouse_fd, &in_evnt, sizeof(struct input_event));
            mouse_send_pointerevent(&mouse_event);

            switch (in_evnt.type) {
                case EV_REL:
                    mouse_update_position(&in_evnt, &mouse_event);
                    break;

                case EV_KEY:
                    mouse_button(&in_evnt, &mouse_event);
                    break;

                default:
                    break;
            }

            continue;
        }

        debug_cond(DEBUG_MOUSE, "Handle mouse events:  %d \n", iter);
        break;
    }
}
