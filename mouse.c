#define pr_fmt(fmt) "  _" fmt
#define DEBUG_MOUSE 0
#define DEBUG_MOUSE_1 0

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/select.h>


#include "global.h"
#include "logerr.h"
#include "utils.h"
#include "stream.h"
#include "mouse.h"


enum {
    MAX_WIDTH  = 1024,
    MAX_HEIGHT = 768,
    VALUE_PRESSED  = 1,
    VALUE_RELEASED = 0,
};



/* mouse functions */
int mouse_open_dev(char *mouse_name, struct mouse_t *mouse) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    mouse_fd = open(mouse_name, O_RDONLY);
    if( mouse_fd == -1) {
        perror("opening mouse device");
        exit(EXIT_FAILURE);
    }

    mouse->current.x  = mouse->current.y  = 0;
    mouse->pressed.x  = mouse->pressed.y  = 0;
    mouse->released.x = mouse->released.y = 0;
    mouse->button_mask = 0;

    return STATUS_OK;
}



void mouse_print_state(struct mouse_t *mouse) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    uint8_t msg[6] = {0};

    //fprintf(stderr, "current(%d, %d),  mask %d \n",
            //mouse->current.x, mouse->current.y, mouse->button_mask);

    *msg = pointerEvent;
    *(msg + 1) = mouse->button_mask;
    *(uint16_t *)(msg + 2) = htons(mouse->current.x);
    *(uint16_t *)(msg + 4) = htons(mouse->current.y);

    stream_send_all(msg, sizeof(msg));

/*
    fprintf(stderr, "\033[1;1H\033[2Kcurrent(%d, %d),  mask %d",
            mouse->current.x, mouse->current.y, mouse->button_mask);

    fprintf(stderr, "\033[1;1H\033[2Kcurrent(%d, %d) pressed(%d, %d) released(%d, %d)  mask %d",
            mouse->current.x, mouse->current.y,
            mouse->pressed.x, mouse->pressed.y,
            mouse->released.x, mouse->released.y,
            mouse->button_mask);
*/
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
            BIT_SET(mouse->button_mask, 0);
        }

        if (inevnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 0);
        }
    }

    if (inevnt->code == BTN_RIGHT) {
        if (inevnt->value == VALUE_PRESSED) {
            mouse->pressed = mouse->current;
            BIT_SET(mouse->button_mask, 1);
        }

        if (inevnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 1);
        }
    }
}


int mouse_handle_event(struct mouse_t **mouse) {
    if (DEBUG_MOUSE) printf("-----\n%s() \n", __func__);

    struct input_event in_evnt;

    fd_set readfds;
    struct timeval tv;

#pragma clang diagnostic ignored "-Wmissing-noreturn"
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

        if( FD_ISSET(mouse_fd, &readfds) ) {
            read(mouse_fd, &in_evnt, sizeof(struct input_event));

            mouse_print_state(*mouse);

            switch (in_evnt.type) {
                case EV_REL:
                    mouse_update_position(&in_evnt, *mouse);
                    break;

                case EV_KEY:
                    mouse_button(&in_evnt, *mouse);
                    break;

                default:
                    break;
            }

            continue;
        }

        debug_cond(DEBUG_MOUSE_1, "Handle mouse events:  %d \n", iter);
        break;
    }
#pragma clang diagnostic pop

}
