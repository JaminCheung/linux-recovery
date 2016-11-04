#include <stdlib.h>
#include <string.h>

#include <utils/log.h>
#include <utils/common.h>
#include <fb/fb_manager.h>

#define LOG_TAG "test_fb_manager"

static struct fb_manager* fb_manager;

int main(int argc, char *argv[]) {
    fb_manager = _new(struct fb_manager, fb_manager);

    if (fb_manager->init(fb_manager) < 0) {
        LOGE("Failed to init fb_manager\n");
        return -1;
    }

    fb_manager->dump(fb_manager);

    for (int i = 0; i < fb_manager->screen_size; i++) {
        *(fb_manager->fbmem + i) = 0xaa;
    }
    return 0;
}
