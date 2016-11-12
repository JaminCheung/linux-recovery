#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <utils/png_decode.h>
#include <utils/log.h>
#include <utils/common.h>
#include <graphics/gr_drawer.h>

#define LOG_TAG "test_gr_drawer"

static struct gr_drawer* gr_drawer;
static uint32_t spinner_frames;
static struct gr_surface** g_surface;

int main(int argc, char *argv[]) {
    gr_drawer = _new(struct gr_drawer, gr_drawer);
    gr_drawer->init(gr_drawer);

    char buf[PATH_MAX] = {0};
    for (int i = 0;;i++) {
        sprintf(buf, "/res/image/spinner_recovery_%02d.png", i);
        if (access(buf, F_OK | R_OK)) {
            LOGI("total frames: %d\n", i);
            spinner_frames = i;
            break;
        }
    }

    if (spinner_frames > 0) {
        g_surface = (struct gr_surface**) calloc(spinner_frames,
                sizeof(struct gr_surface*));

        for (int i = 0; i < spinner_frames; i++) {
            char name[NAME_MAX];
            sprintf(name, "/res/image/spinner_recovery_%02d.png", i);\
            if (png_decode_image(name, g_surface + i) < 0) {
                LOGE("Failed to decode png %s\n", name);
                return -1;
            }
        }

    } else {
        LOGE("Failed to found spinner image /res/image/spinner_recovery_XX.png\n");
        return -1;
    }

    uint32_t image_width = (*g_surface)->width;
    uint32_t image_height = (*g_surface)->height;
    uint32_t pos_x = (gr_drawer->get_fb_width(gr_drawer) - image_width) / 2;
    uint32_t pos_y = (gr_drawer->get_fb_height(gr_drawer) - image_height) / 2;

    gr_drawer->set_pen_color(gr_drawer, 0x00, 0x00, 0x00);
    gr_drawer->fill_screen(gr_drawer);

    gr_drawer->set_pen_color(gr_drawer, 0x55, 0x55, 0xff);
    gr_drawer->draw_text(gr_drawer, 10, 20, "Ingenic Linux Recovery", 0);

    gr_drawer->set_pen_color(gr_drawer, 0xff, 0, 0);
    gr_drawer->draw_text(gr_drawer, 90, 50, "Testing", 0);

    gr_drawer->display(gr_drawer);

    gr_drawer->set_pen_color(gr_drawer, 0x55, 0x55, 0xff);
    gr_drawer->print_text(gr_drawer,"       Waiting...\n");

    for (;;) {
        for (int i = 0; i < spinner_frames; i++) {
            if (gr_drawer->draw_png(gr_drawer, *(g_surface + i), pos_x,
                    pos_y) < 0) {

                LOGE("Failed to draw png image\n");
                return -1;
            }

            usleep(300000);
        }
    }

    return 0;
}
