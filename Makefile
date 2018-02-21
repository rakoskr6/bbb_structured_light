TARGET = pattern_disp
CC=gcc
CFLAGS=-O3
DEPS = open_bmp.h display_core.h display_app.h
OBJ = open_bmp.c display_core.c display_app.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


all: $(OBJ)
	gcc -o $(TARGET) $^ $(CFLAGS)

clean:
	-rm -f *.o
	-rm -f $(TARGET)