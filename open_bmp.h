#define IMG_Y 360
#define IMG_X 640

typedef struct pixel {
	uint8_t r;
	uint8_t g;
	uint8_t b;

} pixel;

int open_bmp(char * file_name, pixel** img);
int verify_bmp(FILE* f, long* offset, char* file_name);
int load_image_files(int *num_images, char image_names[100][200]);