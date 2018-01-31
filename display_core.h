#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h> // for open syscall
#include <linux/fb.h> // frame buffer specific
#include <stdint.h>
#include <sys/mman.h> // for mmap
#include <unistd.h> // for usleep
#include <string.h> // for memcpy
#include <sys/time.h> // measuring time
#include <sys/ioctl.h> // for ictl
#include <linux/kd.h>
#include <dirent.h>

int setup_fb(struct fb_fix_screeninfo *fix_info, struct fb_var_screeninfo *var_info, int *fb, long *screensize, uint8_t **fbp, uint8_t **buffer, int video_mode);
int setup_GPIO();
int cleanup(int fb, uint8_t *fbp, uint8_t *buffer, long screensize, int kill_x, int video_mode);
int clear_screen(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, long screensize);
void print_fix_info(struct fb_fix_screeninfo fix_info);
void print_var_info(struct fb_var_screeninfo var_info);
inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *var_info);
