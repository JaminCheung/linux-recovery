#include <stdlib.h>
#include <string.h>

#include <utils/log.h>
#include <utils/common.h>
#include <fb/fb_manager.h>

#define LOG_TAG "test_fb_manager"

static struct fb_manager* fb_manager;

static inline int  make_pixel(unsigned int a, unsigned int r, unsigned int g, unsigned int b)
{
    return (unsigned int)(((r>>3)<<11)|((g>>2)<<5|(b>>3)));
}

static void fill_pixel(unsigned int pixel, int x0, int y0, int w, int h)
{
    int i, j;
    unsigned short *pbuf = (unsigned short *)fb_manager->fbmem;
    for (i = y0; i < h; i ++) {
        for (j = x0; j < w; j ++) {
            pbuf[i * w + j] = pixel;
        }
    }
}

int main(int argc, char *argv[]) {
    fb_manager = _new(struct fb_manager, fb_manager);

    if (fb_manager->init(fb_manager) < 0) {
        LOGE("Failed to init fb_manager\n");
        return -1;
    }

    fb_manager->dump(fb_manager);

    fill_pixel(make_pixel(0, 0, 0,0xff), 0, 0, 240, 240);

    return 0;
}
