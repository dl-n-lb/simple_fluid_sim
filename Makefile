CC = gcc
EMCC = emcc
CFLAGS = -Wall -Wextra
OPT_DEBUG = -O0
OPT_RELEASE = -O2

SHDC = ~/Software/sokol-tools-bin/bin/linux/sokol-shdc

LIBDIR = ../Libraries
INCLUDE = -I$(LIBDIR)/sokol -I$(LIBDIR)/cimgui
OBJECTS = $(LIBDIR)/cimgui/libcimgui.a
OBJECTS_WASM = $(LIBDIR)/cimgui/libcimgui-wasm.a
LINKER_FLAGS = -lGL -lm -lglfw -lX11 -lasound -lXi -lXcursor -pthread -lstdc++
LINKER_WASM = -lEGL -lm -s USE_WEBGL2=1 -lstdc++

debug: main.c shdc
	$(CC) main.c $(OBJECTS) -o build/debug -DSOKOL_GLCORE33 -DDEBUG $(CFLAGS) $(OPT_DEBUG) $(INCLUDE) $(LINKER_FLAGS)


release: main.c shdc
	$(CC) main.c $(OBJECTS) -o build/release -DSOKOL_GLCORE33 $(CFLAGS) $(OPT_RELEASE) $(INCLUDE) $(LINKER_FLAGS)

wasm: main.c shdc
	$(EMCC) main.c $(OBJECTS_WASM) -o build/wasm.html -DSOKOL_GLES3 $(CFLAGS) $(OPT_RELEASE) $(INCLUDE) $(LINKER_WASM)
	rm -r build/wasm.html

shdc: shader.glsl
	$(SHDC) -i shader.glsl -o shader.glsl.h -l glsl330:glsl300es
