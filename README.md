# A memory-sparing PNG decoder for LVGL

This is a PNG image decoder for [LVGL](https://github.com/lvgl/lvgl) meant for systems with limited memory. It uses [Pngle](https://github.com/kikuchan/pngle) as a backend. It decodes in a streaming manner. As configured, it uses about 45kB of memory (~44k for Pngle and 1k buffer), plus the size of the buffer needed to store the image.

## How to use

First of all, don't forget to download Pngle. Using git:

```
git submodule update --init
```

Then, you need to include `lv_pngle.h` in your project and call `lv_pngle_init()` after initializing LVGL. That is:

```
#include "lv_pngle.h"

void main() {
  ...
  lv_init();
  lv_pngle_init();
  ...
}
```

Don't forget to compile and link `lv_pngle.c`, `pngle.c` and `miniz.c` as well. Also, you must deactivate the PNG decoder provided with LVGL, as it takes precedence.

Included is a CMake file for ESP-IDF, if you want to use it as a component for that platform.
