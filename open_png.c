#include "write_to_fb.h"
#include "open_png.h"
#define PRINTBMPINFO 0
#define PRINTBMPDATA 0



int open_png(char * file_name, pixel** img) {
	// Variable declarations
	long offset = 0;
	unsigned char buff[100];
	int x = 0, y = 0;
	FILE *f = fopen(file_name,"rb"); // open file in binary mode
	

	// Verify that the file opened is in the correct format
	if (verify_png(f, &offset, file_name) == EXIT_FAILURE) {
		printf("Unable to proceed\n");
		return EXIT_FAILURE;
	}
	

	// Read pixel data
	fseek(f, offset, SEEK_SET); // go to beginning of pixel data
	while ((fread(&buff, 3,1,f) >= 1) && y < IMG_Y) { // while not EOF (or done with image), read next pixel 
		img[y][x].r = buff[2];
		img[y][x].g = buff[1];
		img[y][x].b = buff[0];

		x++; // read next column
		if (x >= IMG_X) { // finished reading row, go to next row
			y++;
			x = 0;
			//fread(&buff, 2,1,f); // maybe needed for alignment
		}
	}

	
	if (PRINTBMPDATA) {
		for (y = 0; y < IMG_Y; y++) {
			for (x = 0; x < IMG_X; x++) {
				printf("%02x%02x%02x ",img[y][x].b,img[y][x].g,img[y][x].r);
			}
			printf("\n");
		}
		printf("\n");
	}

	// Close image file now that we have copied it to the img structure
	if (fclose(f)) {
		printf("Unable to close image file\n");
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}



int verify_png(FILE* f, long* offset, char* file_name) {
	// Variable declarations
	unsigned char buff[100];

	// Ensure file was successfully opened
	if (f == NULL) {
		printf("Unable to open image file %s\n",file_name);
		return EXIT_FAILURE;
	}


	// load file header and Windows image header into buffer
	fread(&buff, 54,1,f); 
	

	// Verify file is .bmp type
	if (PRINTBMPINFO) {
		printf("0x%02x 0x%02x\n",buff[0], buff[1]);
	}
	if (buff[0] != 'B' || buff[1] != 'M') {
		printf("Not a BMP file\n");
		fclose(f);
		return EXIT_FAILURE;
	}	


	// Determine where start of pixel information is
	*offset = buff[10] + (buff[11] << 8) + (buff[12] << 16) + (buff[13] << 24); // verify this is the correct way
	if (PRINTBMPINFO) {
		printf("Pixel data start: 0x%02x 0x%02x 0x%02x 0x%02x\n",buff[10], buff[11], buff[12], buff[13]);
		printf("Offset int: %li, offset hex: %x\n",*offset,*offset);
	}


	// Verify correct image width and height
	if (PRINTBMPINFO) {
		printf("Width: 0x%02x 0x%02x\n",buff[18], buff[19]);
	}
	if (buff[18] != 0x80 || buff[19] != 0x02) {
		printf("Incorrect width\n");
		fclose(f);
		return EXIT_FAILURE;
	}
	
	if (PRINTBMPINFO) {
		printf("Height: 0x%02x 0x%02x\n",buff[22], buff[23]);
	}
	if (buff[22] != 0x68 || buff[23] != 0x01) {
		printf("Incorrect height\n");
		fclose(f);
		return EXIT_FAILURE;
	}	


	// Verify correct bits per pixel
	if (PRINTBMPINFO) {
		printf("Bits per pixel: 0x%02x 0x%02x\n",buff[28],buff[29]);
	}
	if (buff[28] != 0x18) {
		printf("Incorrect bits per pixel. Expecting 24\n");
		fclose(f);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
