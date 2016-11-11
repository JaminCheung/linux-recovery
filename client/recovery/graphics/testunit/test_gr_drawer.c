#include <stdlib.h>
#include <unistd.h>

#include <utils/png_decode.h>
#include <utils/log.h>
#include <utils/common.h>
#include <graphics/gr_drawer.h>

#define LOG_TAG "test_gr_drawer"

static struct gr_drawer* gr_drawer;

static void print_help() {
    fprintf(stderr, "test_gr_drawer PNG_IMG\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return -1;
    }

    gr_drawer = _new(struct gr_drawer, gr_drawer);
    gr_drawer->init(gr_drawer);

    const char* png_path = argv[1];

    struct gr_surface* surface = NULL;
    if (png_decode(png_path, &surface) < 0) {
        LOGE("Failed to decode png %s\n", png_path);
        return -1;
    }

    for (;;) {
        gr_drawer->set_color(gr_drawer, 0xff, 0x55, 0x00, 0xff);
        gr_drawer->draw_text(gr_drawer, 0, 0, "12345", 0);
        //gr_drawer->print_text(gr_drawer,"12345");

        return 0;

        if (gr_drawer->draw_png(gr_drawer, surface, 0, 0) < 0) {
            LOGE("Failed to draw png image\n");
            return -1;
        }

        usleep(300000);

        gr_drawer->set_color(gr_drawer, 0xff, 0x55, 0x00, 0x00);
        gr_drawer->fill_screen(gr_drawer);

        usleep(300000);

        gr_drawer->set_color(gr_drawer, 0x55, 0x33, 0xff, 0x00);
        gr_drawer->fill_screen(gr_drawer);

        usleep(300000);
    }
    return 0;
}
