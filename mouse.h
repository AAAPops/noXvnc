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

/*
 * Input:   array of 'int' to store mouse file descriptor
 * Output:  number of found mouse devices
 *          -1 if no one found
 */
int mouse_open_dev(int *fd_arr);

/*
 * Handle mouse event by mouse file descriptor
 */
void mouse_handle_event(int fd);

#endif

