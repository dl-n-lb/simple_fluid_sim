# Simple 2D fluid simulation
on the gpu using [sokol](https://github.com/floooh/sokol) and [cimgui](https://github.com/cimgui/cimgui)

to build:
``` console
// download sokol and cimgui, and compile cimgui to the platform you want
// edit in the Makefile the directories for them
make debug  // for debug builds
make release  // for release builds
make wasm  // for web builds 
```

## TODO
 - FIXME: boundaries **DO NOT** behave correctly
 - add post processing
