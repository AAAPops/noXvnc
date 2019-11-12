#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>


/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))        // '!!' to make sure this returns 0 or 1



enum {
    MAX_WIDTH  = 1024,
    MAX_HEIGHT = 768,
    VALUE_PRESSED  = 1,
    VALUE_RELEASED = 0,
};


struct cursor_t {
    int x, y;
};

struct mouse_t {
    struct cursor_t current;
    struct cursor_t pressed, released;
    uint8_t button_mask;
};


/* mouse functions */
void init_mouse(struct mouse_t *mouse)
{
    mouse->current.x  = mouse->current.y  = 0;
    mouse->pressed.x  = mouse->pressed.y  = 0;
    mouse->released.x = mouse->released.y = 0;
    mouse->button_mask = 0;
}



void print_mouse_state(struct mouse_t *mouse)
{
    fprintf(stderr, "\033[1;1H\033[2Kcurrent(%d, %d),  mask %d",
            mouse->current.x, mouse->current.y, mouse->button_mask);

/*
    fprintf(stderr, "\033[1;1H\033[2Kcurrent(%d, %d) pressed(%d, %d) released(%d, %d)  mask %d",
            mouse->current.x, mouse->current.y,
            mouse->pressed.x, mouse->pressed.y,
            mouse->released.x, mouse->released.y,
            mouse->button_mask);
*/
}

void cursor(struct input_event *in_evnt, struct mouse_t *mouse)
{
    if (in_evnt->code == REL_X)
        mouse->current.x += in_evnt->value;

    if (mouse->current.x < 0)
        mouse->current.x = 0;
    else if (mouse->current.x >= MAX_WIDTH)
        mouse->current.x = MAX_WIDTH - 1;

    if (in_evnt->code == REL_Y)
        mouse->current.y += in_evnt->value;

    if (mouse->current.y < 0)
        mouse->current.y = 0;
    else if (mouse->current.y >= MAX_HEIGHT)
        mouse->current.y = MAX_HEIGHT - 1;
}


void button(struct input_event *in_evnt, struct mouse_t *mouse)
{
    if (in_evnt->code == BTN_LEFT) {
        if (in_evnt->value == VALUE_PRESSED) {
            mouse->pressed = mouse->current;
            BIT_SET(mouse->button_mask, 0);
        }

        if (in_evnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 0);
        }
    }

    if (in_evnt->code == BTN_RIGHT) {
        if (in_evnt->value == VALUE_PRESSED) {
            mouse->pressed = mouse->current;
            BIT_SET(mouse->button_mask, 1);
        }

        if (in_evnt->value == VALUE_RELEASED) {
            mouse->released = mouse->current;
            BIT_CLEAR(mouse->button_mask, 1);
        }
    }
}


int main(int argc, char *argv[])
{
    int mouse_fd;
    const char *dev;
    const char *mouse_dev = "/dev/input/event5";
    struct input_event in_evnt;
    struct mouse_t mouse;

    dev = (argc > 1) ? argv[1]: mouse_dev;

    if ((mouse_fd = open(dev, O_RDONLY)) == -1) {
        perror("opening device");
        exit(EXIT_FAILURE);
    }

    init_mouse(&mouse);

    while ( read(mouse_fd, &in_evnt, sizeof(struct input_event)) ) {
        print_mouse_state(&mouse);

        switch (in_evnt.type) {
            case EV_REL:
                cursor(&in_evnt, &mouse);
            break;

            case EV_KEY:
                button(&in_evnt, &mouse);
            break;

            default:
            break;
        }


    }

    return EXIT_SUCCESS;
}