CC = gcc
EMCC = emcc
CFLAGS = -Wall -Wextra
OPT_DEBUG = -O0
OPT_RELEASE = -O2

SHDC = ~/Software/sokol-tools-bin/bin/linux/sokol-shdc

LIBDIR = ../Libraries
INCLUDE = -I$(LIBDIR)/sokol
LINKER_FLAGS = -lGL -lm -lglfw -lX11 -lasound -lXi -lXcursor -pthread
LINKER_WASM = -lEGL -lm -s USE_WEBGL2=1

debug: main.c shdc
	$(CC) main.c -o build/debug -DSOKOL_GLCORE33 -DDEBUG $(CFLAGS) $(OPT_DEBUG) $(INCLUDE) $(LINKER_FLAGS)


release: main.c shdc
	$(CC) main.c -o build/release -DSOKOL_GLCORE33 $(CFLAGS) $(OPT_RELEASE) $(INCLUDE) $(LINKER_FLAGS)

wasm: main.c shdc
	$(EMCC) main.c -o build/wasm.html -DSOKOL_GLES3 $(CFLAGS) $(OPT_RELEASE) $(INCLUDE) $(LINKER_WASM)

shdc: shader.glsl
	$(SHDC) -i shader.glsl -o shader.glsl.h -l glsl330:glsl300es
