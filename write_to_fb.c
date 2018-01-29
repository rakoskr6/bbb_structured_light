#include "write_to_fb.h"
#include "open_png.h"

#define DEBUG 0
#define DEBUG_TIME 0
#define EXTRA_TIME 3500

int main(int argc, char** argv) {
	// Variable declarations
	int delay = 1000000, repeat = 1, framerate=10;
	int fb = open("/dev/fb0", O_RDWR);
	struct fb_fix_screeninfo fix_info;
	struct fb_var_screeninfo var_info;
	char image_names[100][200]; // array of images. Will load from file system eventually
	int num_images = 0;
	int i = 1;
	int test_flag = 0, screen_persist = 0, kill_x = 0, trig_in = 0;
	DIR *d;
	struct dirent *dir;


	// Handle command line arguments
	if (argc <= 1) {
		printf("Usage: pattern_disp [options] framerate repetitions\n\n");
		printf("Options:\n");
		printf(" -test (-t)     run through built in test patterns\n");
		printf(" -persist (-p)  the last pattern will stay on the screen at the end of the sequence\n");
		printf(" -kill (-k)     stop running the X-server (sometimes gets better results). Restarts afterwards\n");
		return EXIT_FAILURE;
	}
	
	while (i < argc && argv[i][0] == '-') { // while there are flags to handle
		if ((strcmp("-t",argv[i]) == 0) || (strcmp("-test",argv[i]) == 0)) {
			test_flag = 1;
		}

		if ((strcmp("-p",argv[i]) == 0) || (strcmp("-persist",argv[i]) == 0)) {
			screen_persist = 1;
		}

		if ((strcmp("-k",argv[i]) == 0) || (strcmp("-kill",argv[i]) == 0)) {
			kill_x = 1;
			printf("Closing X server...\n");
			system("sudo service lightdm stop");
			usleep(1000000); // let X server close
		}

		if ((strcmp("-i",argv[i]) == 0) || (strcmp("-in",argv[i]) == 0)) {
			trig_in = 1;
		}
		i++; // increment to check additional flags
	}

	if (argc >= (i+1)) {
		framerate = (int)strtol(argv[i],NULL,10);
		delay = 1000000/framerate;
	}
	if (argc >= (i+2)) {
		repeat = (int)strtol(argv[i+1],NULL,10);
	}


	// Get screen infromation and ensure it is properly setup
	ioctl(fb, FBIOGET_FSCREENINFO, &fix_info); //Get the fixed screen information
	ioctl(fb, FBIOGET_VSCREENINFO, &var_info); // Get the variable screen information
	//if(ioctl(fb, KDSETMODE, KD_GRAPHICS) == -1) {
	//	printf("Failed to set graphics mode on fb\n"); // to be verified
	//}

	// Ensure certain parameters are properly setup (they already should be)
	var_info.grayscale = 0;
	var_info.bits_per_pixel = 32;

	if (DEBUG) {
		print_fix_info(fix_info);
		print_var_info(var_info);
	}


	// Setup mmaped buffers
	long screensize = var_info.yres_virtual * fix_info.line_length; // Determine total size of screen in bytes
	
	uint8_t* fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb, (off_t)0); // Map screenbuffer to memory with read and write access and visible to other processes
	uint8_t* buffer = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, (off_t)0); // Second buffer 

	if (fbp == MAP_FAILED || buffer == MAP_FAILED) {
		printf("mmap failed\n");
		return EXIT_FAILURE;
	} 


	// Display images
	if(test_flag) {
		test_loop(fbp, buffer, &var_info, &fix_info, delay, repeat, screensize, trig_in);
	}
	else {
		i = 0;
		d = opendir(".");
		if (d) {
			dir = readdir(d);
			if (DEBUG) {
				printf("Images to display:\n");
			}
			while (dir != NULL) {
				if (strstr(dir->d_name,".bmp") != NULL) {
					if (DEBUG) {
						printf(" %s\n", dir->d_name);
					}
					strcpy(image_names[num_images], dir->d_name);
					num_images++;
				}
				dir = readdir(d);
			}
			closedir(d);
		}
		//image_names[0] = "test_image.bmp";
		//image_names[0] = "binary_image1.bmp"; // array of images
		//image_names[1] = "binary_image2.bmp"; // array of images
		display_images(image_names, num_images, fbp, buffer, &var_info, &fix_info, delay, repeat, screensize, screen_persist, trig_in);
	}
	


	if (cleanup(fb, fbp, buffer, screensize, kill_x) == EXIT_FAILURE){
		printf("Error cleaning up files\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


int display_images(char image_names[100][200], int num_images, uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize, int screen_persist, int trig_in) {
	int i, ii;
	long x, y, location;
	long x_max = var_info->xres_virtual;
	long y_max = var_info->yres_virtual;
	uint32_t pix = 0x123456;// Pixel to draw
	pixel** img;
	struct timeval start, stop, start2, stop2;
	int usecs = 0;
	double totalusecs = 0;
	
	// Allocate image structure which will be used to load images
	img = (pixel**)malloc(IMG_Y * sizeof(pixel*));
	for (i = 0; i < IMG_Y; i++) {
		img[i] = (pixel*)malloc(IMG_X * sizeof(pixel));
	}


	// Will loop through displaying all images
	for (ii = 0; ii < repeat; ii++) {
		for (i = 0; i < num_images; i++) {
			// Open image and ensure it's successful 
			if (open_png(image_names[i], img) == EXIT_FAILURE) {
				return EXIT_FAILURE;
			}
			//printf("Image %s\n",image_names[i]);


			// Transfer image structure to the buffer
			for (y=0; y<y_max; y++) {
				//printf("y: %i ",y);
				for (x=0; x<x_max; x++) {
					location = (x+var_info->xoffset) * (var_info->bits_per_pixel / 8) + (y + var_info->yoffset) * fix_info->line_length; // offset where we write pixel value
					pix = pixel_color(img[y][x].r, img[y][x].g, img[y][x].b, var_info); // get pixel in correct format
					*((uint32_t*)(bbp + location)) = pix; // write pixel to buffer	
				}
			}
			//printf("Iteration %i, displaying image %s\n", ii, image_names[i]);
			
			// Wait until delay is over
			if (!(ii == 0 && i == 0) && !trig_in) { // as long as it's not the first time through the loop we have to wait
				do {
					usleep(10);
					gettimeofday(&stop, NULL);
					usecs = (stop.tv_usec - start.tv_usec) + (stop.tv_sec - start.tv_sec)*1000000;

				
				} while (usecs < (delay-EXTRA_TIME)); // -EXTRA_TIME which is approximate buffer load time 
				
				if (DEBUG_TIME) {
					printf("Delay goal is %ius versus actual of %ius. Difference: %.1fms\n",delay,usecs,(usecs-delay)/1000.0);
					totalusecs+=usecs;
				}
			}
			else if (trig_in) { // if we are using trigger in
				int next = 0;
				while (!next) {
					printf("1 to continue: ");
					scanf("%i",&next);
				}		
				printf("\n");
			}
			

			// Freeze update buffer
			system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x01 i");

			// Display image
			memcpy(fbp, bbp, screensize); // load framebuffer from buffered location

			// Start timer that will be used for next image
			gettimeofday(&start, NULL);

			usleep(delay/3); // allow buffer to finish loading
			system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x00 i"); // Unfreeze update buffer
		}
	}

	// Wait for last image to be done
	if (!trig_in) {
		do {
			usleep(10);
			gettimeofday(&stop, NULL);
			usecs = (stop.tv_usec - start.tv_usec) + (stop.tv_sec - start.tv_sec)*1000000;
		} while (usecs < (delay-EXTRA_TIME)); // -EXTRA_TIME which is approximate buffer load time 
	}
	
	if (DEBUG_TIME) {
		printf("Delay goal is %ius versus actual of %ius. Difference: %.1fms\n",delay,usecs,(usecs-delay)/1000.0);
		totalusecs+=usecs;
		printf("Average difference: %.1fms\n\n", (delay-totalusecs/repeat/num_images)/1000.0);
	}

	// Cleanup image memory
	for (i = 0; i < IMG_Y; i++) {
		free(img[i]);
	}
	free(img);


	if (!screen_persist && !trig_in) {
		clear_screen(fbp, bbp, var_info, fix_info, screensize);
	}
	else if (!screen_persist && trig_in) { // wait until last trigger in to clear screen otherwise
		int next = 0;
		while (!next) {
			printf("1 to continue: ");
			scanf("%i",&next);
		}		
		printf("\n");
		clear_screen(fbp, bbp, var_info, fix_info, screensize);
	}

	return EXIT_SUCCESS;
}

int test_loop(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, int delay, int repeat, long screensize, int trig_in) {
	int i, ii;
	long x, y, location;
	long x_max = var_info->xres_virtual;
	long y_max = var_info->yres_virtual;
	uint32_t pix = 0x123456;// Pixel to draw
	struct timeval start, stop;
	int usecs = 0;
	double totalusecs = 0, total_displayed = 0; 


	// Will loop through displaying all images
	for (ii = 0; ii < repeat; ii++) {
		for (i = 0; i < 6; i++) {
		// Transfer image structure to the buffer
		for (y=0; y<y_max; y++) {
			for (x=0; x<x_max; x++) {
				location = (x+var_info->xoffset) * (var_info->bits_per_pixel / 8) + (y + var_info->yoffset) * fix_info->line_length; // offset where we write pixel value
				if (i == 0) {
					pix = pixel_color(0xff, 0xff, 0xff, var_info);
				}
				else if (i == 1) {
					pix = pixel_color(0xff, 0x00, 0x00, var_info);
				}
				else if (i == 2) {
					pix = pixel_color(0x00, 0xff, 0x00, var_info);
				}
				else if (i == 3) {
					pix = pixel_color(0x00, 0x00, 0xff, var_info);
				}
				else if (i == 4) {
					pix = pixel_color(0x20, 0x20, 0x20, var_info);
				}
				else {
					pix = pixel_color(0x00, 0x00, 0x00, var_info);
				}
				

				*((uint32_t*)(bbp + location)) = pix; // write pixel to buffer	
			}
		}
		
		// Wait until delay is over
		if (!(ii == 0 && i == 0) && !trig_in) { // as long as it's not the first time through the loop we have to wait
			do {
				usleep(10);
				gettimeofday(&stop, NULL);
				usecs = (stop.tv_usec - start.tv_usec) + (stop.tv_sec - start.tv_sec)*1000000;

			
			} while (usecs < (delay-EXTRA_TIME)); // -EXTRA_TIME which is approximate buffer load time 
			
			if (DEBUG_TIME) {
				printf("Delay goal is %ius versus actual of %ius. Difference: %.1fms\n",delay,usecs,(usecs-delay)/1000.0);
				totalusecs+=usecs;
				total_displayed++;
			}
		}
		else if (trig_in) { // if we are using trigger in
			
			int next = 0;
			while (!next) {
				printf("1 to continue: ");
				scanf("%i",&next);
			}		
			printf("\n");
		}

		// Freeze update buffer
		system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x01 i");

		// Display image
		memcpy(fbp, bbp, screensize); // load framebuffer from buffered location
		
		// Start timer that will be used for next image
		gettimeofday(&start, NULL);
		
		usleep(delay/3); // allow buffer to finish loading
		system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x00 i"); // Unfreeze update buffer

		
		}
	}

	// Wait for last image to be done
	do {
		usleep(10);
		gettimeofday(&stop, NULL);
		usecs = (stop.tv_usec - start.tv_usec) + (stop.tv_sec - start.tv_sec)*1000000;
	} while (usecs < (delay-EXTRA_TIME)); // -EXTRA_TIME which is approximate buffer load time 
	
	if (DEBUG_TIME) {
		printf("Delay goal is %ius versus actual of %ius. Difference: %.1fms\n",delay,usecs,(usecs-delay)/1000.0);
		totalusecs+=usecs;
		total_displayed++;
		printf("Average difference: %.1fms\n\n", (totalusecs/total_displayed - delay)/1000.0);
	}


	return EXIT_SUCCESS;
}



int clear_screen(uint8_t* fbp, uint8_t* bbp, struct fb_var_screeninfo* var_info, struct fb_fix_screeninfo* fix_info, long screensize) {
	long x, y, location;
	long x_max = var_info->xres_virtual;
	long y_max = var_info->yres_virtual;
	uint32_t pix = 0x123456;// Pixel to draw

	// Transfer image structure to the buffer
	pix = pixel_color(0x00, 0x00, 0x00, var_info);
	for (y=0; y<y_max; y++) {
		for (x=0; x<x_max; x++) {
			location = (x+var_info->xoffset) * (var_info->bits_per_pixel / 8) + (y + var_info->yoffset) * fix_info->line_length; // offset where we write pixel value
			*((uint32_t*)(bbp + location)) = pix; // write pixel to buffer	
		}
	}

	// Display image
	memcpy(fbp, bbp, screensize); // load framebuffer from buffered location

	return EXIT_SUCCESS;
}



// Releases appropriate files
int cleanup(int fb, uint8_t *fbp, uint8_t *buffer, long screensize, int kill_x) {
	
	if (munmap(fbp, screensize) == EXIT_FAILURE) {
		printf("Unable to unmap fbp\n");
	}
	if (munmap(buffer, screensize) == EXIT_FAILURE) {
		printf("Unable to unmap buffer\n");
	}
	if (close(fb) == EXIT_FAILURE) {
		printf("Unable to close frame buffer\n");
	}
	if (kill_x == 1) { // should restart x server
		system("sudo service lightdm start");
	}
	return EXIT_SUCCESS;
}

inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *var_info) {
	return (r<<var_info->red.offset) | (g<<var_info->green.offset) | (b<<var_info->blue.offset);

}
/* This function prints some key, fixed parameters of the frame buffer. Usefule for debugging. */
void print_fix_info(struct fb_fix_screeninfo fix_info) {
	printf("-----Fixed framebuffer information:-----\n");
	printf("ID: %s\n", fix_info.id);
	printf("Framebuffer length mem: %i\n", fix_info.smem_len);
	printf("Framebuffer type (see FB_TYPE_): %i\n",fix_info.type); // probably 0 on Beagle which is packed_pixels 
	printf("Framebuffer visual (see FB_VISUAL_): %i\n",fix_info.visual); // probably 2 on Beagle which is truecolor
	printf("Line length (bytes): %i\n",fix_info.line_length);
	printf("Acceleration (see FB_ACCEL_*): %i\n",fix_info.accel); // probably 0 on Beagle which means none
	printf("\n");

	return;
}


void print_var_info(struct fb_var_screeninfo var_info) {
	printf("-----Variable framebuffer information:-----\n");
	printf("X Resolution: %i\n",var_info.xres); 
	printf("Y Resolution: %i\n",var_info.yres); 
	printf("X Resolution Virtual: %i\n",var_info.xres_virtual); 
	printf("Y Resolution Virtual: %i\n",var_info.yres_virtual); 
	printf("X Offset: %i\n",var_info.xoffset); 
	printf("Y Offset: %i\n",var_info.yoffset); 
	printf("Bits per pixel: %i\n",var_info.bits_per_pixel); 
	printf("Grayscale: %i\n",var_info.grayscale); // 0 = color
	printf("Red pixel\n");
	printf("   Offset = %i\n",var_info.red.offset);
	printf("   Length = %i\n",var_info.red.length);
	printf("   MSB Right = %i\n",var_info.red.msb_right);
	printf("Green pixel\n");
	printf("   Offset = %i\n",var_info.green.offset);
	printf("   Length = %i\n",var_info.green.length);
	printf("   MSB Right = %i\n",var_info.green.msb_right);
	printf("Blue pixel\n");
	printf("   Offset = %i\n",var_info.blue.offset);
	printf("   Length = %i\n",var_info.blue.length);
	printf("   MSB Right = %i\n",var_info.blue.msb_right);
	printf("Transparency pixel\n");
	printf("   Offset = %i\n",var_info.transp.offset);
	printf("   Length = %i\n",var_info.transp.length);
	printf("   MSB Right = %i\n",var_info.transp.msb_right);
	printf("Nonstandard format: %i\n",var_info.nonstd); // 0 = standard
	printf("Pixel Clock: %ips\n",var_info.pixclock); // 0 = color
	printf("\n");
	return;
}
