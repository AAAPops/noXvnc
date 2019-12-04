
int fb_open_dev(char *fb_name);


int fb_update_raw(char *rect_ptr, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res);

int fb_update_tight_copyfilter(char *rect_ptr, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res);
int fb_update_tight_fillcompression(char *rect_ptr, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res);


int fb_update_tight_palette(char *rect_ptr, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res, uint8_t palette_arr[256*3]);
int fb_update_tight_palette_1_bit(char *rect_ptr, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res, uint8_t palette_arr[256*3]);


int fb_update_copyrect(uint16_t src_x_pos, uint16_t src_y_pos, uint16_t x_start, uint16_t y_start, uint16_t x_res, uint16_t y_res);


int backFB_to_frontFB(void);