#ifndef _MOUSE_INCL_GUARD
#define _MOUSE_INCL_GUARD

struct cursor_t {
    int x, y;
};

struct mouse_t {
    struct cursor_t current;
    struct cursor_t pressed, released;
    uint8_t button_mask;
};


int mouse_open_dev(char *mouse_name, struct mouse_t *mouse);

int mouse_handle_event(struct mouse_t **mouse);

#endif

