#define DEBUG 1
#define DEBUG_TIME 0
#define EXTRA_TIME 3500
#define GPIO_OUT "49"
#define GPIO_IN "115"
#include <stdint.h>

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


int test_loop(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize, int trig_in);
int display_images(char image_names[100][200], int num_images, uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize, int screen_persist, int trig_in);
