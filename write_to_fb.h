#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h> // for open syscall
//#include <sys/types.h>
//#include <sys/stat.h>
#include <linux/fb.h> // frame buffer specific
#include <stdint.h>
#include <sys/mman.h> // for mmap
#include <unistd.h> // for usleep
#include <string.h> // for memcpy
#include <sys/time.h> // measuring time
#include <sys/ioctl.h> // for ictl
#include <linux/kd.h>
#include <dirent.h>


void print_fix_info(struct fb_fix_screeninfo fix_info);
void print_var_info(struct fb_var_screeninfo var_info);
inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *var_info);
int cleanup(int fb, uint8_t *fbp, uint8_t *buffer, long screensize);
int test_loop(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize);
int display_images(char image_names[100][200], int num_images, uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize, int screen_persist);
int clear_screen(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, long screensize);