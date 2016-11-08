#include <string.h>

#include <utils/log.h>
#include <utils/common.h>
#include <fb/fb_manager.h>
#include <lib/png/png.h>

#define LOG_TAG "test_png_decoder"

static struct fb_manager* fb_manager;

static void print_help() {
    fprintf(stderr, "test_png_decoder PNG_IMG\n");
}

static inline uint32_t  make_pixel(uint32_t r, uint32_t g, uint32_t b)
{
    return (uint32_t)(((r >> 3) << 11) | ((g >> 2) << 5 | (b >> 3)));
}

static void transform_rgb_to_draw(unsigned char* input_row,
                                  unsigned char* output_row,
                                  int channels, int width) {
    int x;
    unsigned char* ip = input_row;
    unsigned char* op = output_row;

    switch (channels) {
        case 1:
            // expand gray level to RGBX
            for (x = 0; x < width; ++x) {
                *op++ = *ip;
                *op++ = *ip;
                *op++ = *ip;
                *op++ = 0xff;
                ip++;
            }
            break;

        case 3:
            // expand RGBA to RGBX
            for (x = 0; x < width; ++x) {
                *op++ = *ip++;
                *op++ = *ip++;
                *op++ = *ip++;
                *op++ = 0xff;
            }
            break;

        case 4:
            // copy RGBA to RGBX
            memcpy(output_row, input_row, width*4);
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return -1;
    }

    const char* png_path = argv[1];
    uint8_t header[8];

    FILE* fp = fopen(png_path, "rb");
    if (fp == NULL) {
        print_help();
        return -1;
    }

    uint32_t bytes_readed = fread(header, 1, sizeof(header), fp);
    if (bytes_readed != sizeof(header)) {
        LOGE("Failed to read image header\n");
        return -1;
    }

    if (png_sig_cmp(header, 0, sizeof(header))) {
        LOGE("Image is not png\n");
        return -1;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0,
            0);
    png_infop info_ptr = NULL;
    png_bytep row = NULL, display = NULL;

    if (png_ptr == NULL) {
        LOGI("Failed to create png structure\n");
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return -1;
    }

    png_init_io(png_ptr, fp);
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        LOGE("Failed create info structure\n");
        return -1;
    }

    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);

    png_set_sig_bytes(png_ptr, sizeof(header));
    png_read_info(png_ptr, info_ptr);

    uint32_t width = png_get_image_width(png_ptr, info_ptr);
    uint32_t height = png_get_image_height(png_ptr, info_ptr);

    uint8_t color_type = png_get_color_type(png_ptr, info_ptr);
    uint32_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    uint32_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    uint32_t channels = png_get_channels(png_ptr, info_ptr);

    LOGI("width=%d\n", width);
    LOGI("height=%d\n", height);
    LOGI("color_type=0x%x\n", color_type);
    LOGI("bit_depth=0x%x\n", bit_depth);
    LOGI("channgel=%d\n", channels);
#if 1
    if (bit_depth == 8 && channels == 3 && color_type == PNG_COLOR_TYPE_RGB) {

    } else if (bit_depth <= 8 && channels == 1 && color_type == PNG_COLOR_TYPE_GRAY) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    } else if (bit_depth <= 8 && channels == 1 && color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
        channels = 3;

    } else {
        //LOGE("unsupport\n");
        //return -1;
    }
#endif
    fb_manager = _new(struct fb_manager, fb_manager);

    fb_manager->init(fb_manager);
    fb_manager->dump(fb_manager);

    row = malloc(width * 4);
    uint8_t *mem = malloc(width * height * 4);

    png_read_row(png_ptr, row, NULL);

    for (int i = 0; i < fb_manager->get_screen_height(fb_manager); i++) {
        LOGI("i = %d\n", i);
        for (int j = 0; j < fb_manager->get_screen_width(fb_manager); j+=3) {
            uint8_t red = row[j];
            uint8_t green = row[j + 1];
            uint8_t blue = row[j + 2];

            uint16_t pixel = make_pixel(red, green, blue);

            //LOGI("j = %d\n", j);

            uint16_t *pbuf = (uint16_t *)fb_manager->fbmem;
            pbuf[2 * j] = pixel;

        }

    }

    fb_manager->display(fb_manager);

    return 0;
}
