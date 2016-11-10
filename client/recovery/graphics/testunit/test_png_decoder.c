#include <string.h>
#include <stdlib.h>

#include <utils/log.h>
#include <utils/common.h>
#include <fb/fb_manager.h>
#include <lib/png/png.h>

#define LOG_TAG "test_png_decoder"

struct image_info {
    uint32_t height;
    uint32_t width;
    int color_type;
    uint32_t channels;
    int bit_depth;
    uint32_t row_bytes;
};

static png_structp png_ptr;
static png_infop info_ptr;
static struct fb_manager* fb_manager;
static struct image_info image_info;
static uint8_t *g_mem;

static void print_help() {
    fprintf(stderr, "test_png_decoder PNG_IMG\n");
}

static void dump_image_info() {
    LOGI("=========================\n");
    LOGI("Dump image info\n");
    LOGI("Height:     %u\n", image_info.height);
    LOGI("Width:      %u\n", image_info.width);
    LOGI("Bit depth:  %d\n", image_info.bit_depth);
    LOGI("Channels:   %u\n", image_info.channels);
    LOGI("Row bytes:  %u\n", image_info.row_bytes);
    LOGI("Color type: %d\n", image_info.color_type);
    LOGI("=========================\n");
}

static inline uint32_t  make_pixel(uint8_t red, uint8_t green, uint8_t blue,
        uint8_t alpha) {
    uint32_t redbit_len = fb_manager->get_redbit_length(fb_manager);
    uint32_t redbit_off = fb_manager->get_redbit_offset(fb_manager);

    uint32_t greenbit_len = fb_manager->get_greenbit_length(fb_manager);
    uint32_t greenbit_off = fb_manager->get_greenbit_offset(fb_manager);

    uint32_t bluebit_len = fb_manager->get_bluebit_length(fb_manager);
    uint32_t bluebit_off = fb_manager->get_bluebit_offset(fb_manager);

    uint32_t alphabit_len = fb_manager->get_alphabit_length(fb_manager);
    uint32_t alphabit_off = fb_manager->get_alphabit_offset(fb_manager);

    uint32_t pixel = (uint32_t)(((red >> (8 - redbit_len)) << redbit_off)
            | ((green >> (8 - greenbit_len)) << greenbit_off)
            | ((blue >> (8 - bluebit_len)) << bluebit_off)
            | ((alpha >> (8 - alphabit_len)) << alphabit_off));

    return pixel;
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
            memcpy(output_row, input_row, width * 4);
            break;
    }
}

static void transform_rgba8888_to_fb() {
    uint8_t *buf = (uint8_t *) fb_manager->fbmem;
    uint32_t fb_width = fb_manager->get_screen_width(fb_manager);
    uint32_t fb_height = fb_manager->get_screen_height(fb_manager);
    uint32_t bits_per_pixel = fb_manager->get_bits_per_pixel(fb_manager);
    uint32_t bytes_per_pixel = bits_per_pixel / 8;

    memset(fb_manager->fbmem, 0xff, fb_manager->get_screen_size(fb_manager));

    for (int i = 0; i < image_info.height; i++) {
        if (i >= fb_height)
            break;

        for (int j = 0; j < image_info.width; j++) {
            if (j >= fb_width)
                break;

            uint8_t red = g_mem[4 * (j + image_info.width * i)];
            uint8_t green = g_mem[4 * (j + image_info.width * i) + 1];
            uint8_t blue = g_mem[4 * (j + image_info.width * i) + 2];
            uint8_t alpha = g_mem[4 * (j + image_info.width * i) + 3];

            uint32_t pos = i * fb_width + j;
            uint32_t pixel = make_pixel(red, green, blue, alpha);

            for (int x = 0; x < bytes_per_pixel; x++)
                buf[bytes_per_pixel * pos + x] = pixel >> (bits_per_pixel -
                        (bytes_per_pixel -x) * 8);

        }
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

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

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

    png_set_sig_bytes(png_ptr, sizeof(header));

    png_read_info(png_ptr, info_ptr);

    /*
     * Format png to RGBA8888
     */
    int bit_depth = png_get_bit_depth (png_ptr, info_ptr);
    int color_type = png_get_color_type (png_ptr, info_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb (png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8 (png_ptr);

    if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png_ptr);

    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    if(bit_depth == 16)
        png_set_strip_16(png_ptr);
    else if (bit_depth < 8)
        png_set_packing (png_ptr);

    png_read_update_info (png_ptr, info_ptr);

    /*
     * Now format is RGBA8888
     */
    png_get_IHDR(png_ptr, info_ptr, &image_info.width, &image_info.height,
            &image_info.bit_depth, &image_info.color_type, NULL, NULL, NULL);

    image_info.row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    image_info.channels = png_get_channels(png_ptr, info_ptr);

    dump_image_info();

    g_mem = (uint8_t*) calloc(1, image_info.width * image_info.height * 4);

    uint8_t * row = (uint8_t *) malloc(image_info.width * 4);

    for (int i = 0; i < image_info.height; i++) {
        png_read_row(png_ptr, row, NULL);

        transform_rgb_to_draw(row, g_mem + i * image_info.width * 4,
                image_info.channels, image_info.width);
    }
    free(row);

    fb_manager = _new(struct fb_manager, fb_manager);

    fb_manager->init(fb_manager);

    fb_manager->dump(fb_manager);

    transform_rgba8888_to_fb();

    free(g_mem);

    fb_manager->display(fb_manager);

    fb_manager->deinit(fb_manager);

    _delete(fb_manager);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return 0;
}
