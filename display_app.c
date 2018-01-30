#include "display_app.h"
#include "open_bmp.h"



int main(int argc, char** argv) {
	// Variable declarations
	int delay = 1000000, repeat = 1, framerate=10;
	int fb = open("/dev/fb0", O_RDWR);
	struct fb_fix_screeninfo fix_info;
	struct fb_var_screeninfo var_info;
	char image_names[100][200]; // array of images. Will load from file system eventually
	int num_images = 0;
	int i = 1;
	int test_flag = 0, screen_persist = 0, kill_x = 0, trig_in = 0, video_mode = 0;
	DIR *d;
	struct dirent *dir;


	// Handle command line arguments
	if (argc <= 1) {
		printf("Usage: pattern_disp [options] framerate repetitions\n\n");
		printf("Options:\n");
		printf(" -test (-t)     run through built in test patterns\n");
		printf(" -in (-i)       use the trigger in (GPIO"GPIO_IN") to advance to next pattern instead of set framerate\n");
		printf(" -persist (-p)  the last pattern will stay on the screen at the end of the sequence\n");
		printf(" -kill (-k)     stop running the X-server (sometimes gets better results). Restarts afterwards\n");
		printf(" -video (-v)    don't modify the EVM display and keep the display in video mode\n");
		return EXIT_FAILURE;
	}
	
	// Setup EVM for output from Beagle
	system("i2cset -y 2 0x1b 0x0b 0x00 0x00 0x00 0x00 i");
	system("i2cset -y 2 0x1b 0x0c 0x00 0x00 0x00 0x1b i");

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

		if ((strcmp("-v",argv[i]) == 0) || (strcmp("-video",argv[i]) == 0)) {
			video_mode = 1;
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


	// Ensure certain parameters are properly setup (they already should be)
	var_info.grayscale = 0;
	var_info.bits_per_pixel = 32;

	if (DEBUG) {
		print_fix_info(fix_info);
		print_var_info(var_info);
	}


	// Change some video settings for better "pixel accurate" mode
	if (!video_mode) {
		system("i2cset -y 2 0x1b 0x7e 0x00 0x00 0x00 0x02 i"); // disable temporal dithering
		system("i2cset -y 2 0x1b 0x50 0x00 0x00 0x00 0x06 i"); // disable automatic gain control
		system("i2cset -y 2 0x1b 0x5e 0x00 0x00 0x00 0x00 i"); // disable color coordinate adjustment (CCA) function
	}

	// Setup mmaped buffers
	long screensize = var_info.yres_virtual * fix_info.line_length; // Determine total size of screen in bytes
	uint8_t* fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb, (off_t)0); // Map screenbuffer to memory with read and write access and visible to other processes
	uint8_t* buffer = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, (off_t)0); // Second buffer 

	if (fbp == MAP_FAILED || buffer == MAP_FAILED) {
		printf("mmap failed\n");
		return EXIT_FAILURE;
	} 

	setupGPIO(); // setup input and output triggers

	// Display images
	if(test_flag) {
		test_loop(fbp, buffer, &var_info, &fix_info, delay, repeat, screensize, trig_in);
	}
	else {
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

		display_images(image_names, num_images, fbp, buffer, &var_info, &fix_info, delay, repeat, screensize, screen_persist, trig_in);
	}
	


	if (cleanup(fb, fbp, buffer, screensize, kill_x, video_mode) == EXIT_FAILURE){
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
	FILE *fp_trig_in; 
	char trig_in_value = '0';
	
	if (trig_in) { // if there is a trigger in delay should coorspond to a relativly high framerate (~50fps works)
		delay = 20000;
	}

	// Allocate image structure which will be used to load images
	img = (pixel**)malloc(IMG_Y * sizeof(pixel*));
	for (i = 0; i < IMG_Y; i++) {
		img[i] = (pixel*)malloc(IMG_X * sizeof(pixel));
	}


	// Will loop through displaying all images
	for (ii = 0; ii < repeat; ii++) {
		for (i = 0; i < num_images; i++) {
			// Open image and ensure it's successful. Inefficent to load file everytime but fine at low framerates
			if (open_png(image_names[i], img) == EXIT_FAILURE) {
				return EXIT_FAILURE;
			}
			system("echo 0 > /sys/class/gpio/gpio"GPIO_OUT"/value"); // set trigger outpu low since we have completed the trigger


			// Transfer image structure to the buffer
			for (y=0; y<y_max; y++) {
				for (x=0; x<x_max; x++) {
					location = (x+var_info->xoffset) * (var_info->bits_per_pixel / 8) + (y + var_info->yoffset) * fix_info->line_length; // offset where we write pixel value
					pix = pixel_color(img[y][x].r, img[y][x].g, img[y][x].b, var_info); // get pixel in correct format
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
				}
			}
			else if (trig_in) { // if we are using trigger in
					while (trig_in_value == '0') { // wait until trigger in is asserted
						fp_trig_in = fopen("/sys/class/gpio/gpio"GPIO_IN"/value","r");
						trig_in_value = fgetc(fp_trig_in);
						fclose(fp_trig_in);
						usleep(1000);
					}
					trig_in_value = '0';
			}
			

			// Freeze update buffer
			system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x01 i");

			// Display image
			memcpy(fbp, bbp, screensize); // load framebuffer from buffered location

			// Start timer that will be used for next image
			gettimeofday(&start, NULL);

			usleep(delay/3); // allow buffer to finish loading
			system("i2cset -y 2 0x1b 0xa3 0x00 0x00 0x00 0x00 i"); // Unfreeze update buffer
			system("echo 1 > /sys/class/gpio/gpio"GPIO_OUT"/value"); // set trigger high to indicate image done loading
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
	
	if (DEBUG_TIME && !trig_in) {
		printf("Delay goal is %ius versus actual of %ius. Difference: %.1fms\n",delay,usecs,(usecs-delay)/1000.0);
		totalusecs+=usecs;
		printf("Average difference: %.1fms\n\n", (delay-totalusecs/repeat/num_images)/1000.0);
	}

	// Cleanup image memory
	for (i = 0; i < IMG_Y; i++) {
		free(img[i]);
	}
	free(img);

	system("echo 0 > /sys/class/gpio/gpio"GPIO_OUT"/value"); // set trigger low since we have completed the trigger

	if (!screen_persist && !trig_in) {
		clear_screen(fbp, bbp, var_info, fix_info, screensize);
	}
	else if (!screen_persist && trig_in) { // wait until last trigger in to clear screen otherwise
		while (trig_in_value == '0') { // wait until trigger in is asserted
			fp_trig_in = fopen("/sys/class/gpio/gpio"GPIO_IN"/value","r");
			trig_in_value = fgetc(fp_trig_in);
			fclose(fp_trig_in);
			usleep(1000);
		}
		clear_screen(fbp, bbp, var_info, fix_info, screensize);
	}

	return EXIT_SUCCESS;
}

