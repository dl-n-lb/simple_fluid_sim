CC = gcc
CFLAGS = -Wall -Wextra -pthread
OPT_DEBUG = -O0
OPT_RELEASE = -O2

SHDC = ~/Software/sokol-tools-bin/bin/linux/sokol-shdc

LIBDIR = ../Libraries
INCLUDE = -I$(LIBDIR)/sokol
LINKER_FLAGS = -lGL -lm -lglfw -lX11 -lasound -lXi -lXcursor

debug: main.c shdc
	$(CC) main.c -o build/debug -DDEBUG $(CFLAGS) $(OPT_DEBUG) $(INCLUDE) $(LINKER_FLAGS)


release: main.c shdc
	$(CC) main.c -o build/release $(CFLAGS) $(OPT_RELEASE) $(INCLUDE) $(LINKER_FLAGS)

shdc: shader.glsl
	$(SHDC) -i shader.glsl -o shader.glsl.h -l glsl330:glsl300es
