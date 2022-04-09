/** \file lv_pngle.c
 *  \brief Implementation file for LVGL decoder for PNG images using Pngle.
 *
 *  Author: Vincent Paeder
 *  License: MIT
 */
#include "lvgl.h"
#include "lv_pngle.h"
#include "external/src/pngle.h"

/** \fn lv_res_t pngle_decoder_info(struct _lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
 *  \brief Retrieves PNG image size from given source.
 *  \param decoder: underlying image decoder.
 *  \param src: pointer to image source (data buffer or file path).
 *  \param header: target structure for data.
 *  \returns LV_RES_OK if successful, LV_RES_INV if failed.
 */
static lv_res_t pngle_decoder_info(struct _lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header);

/** \fn lv_res_t pngle_decoder_open(lv_img_decoder_t * dec, lv_img_decoder_dsc_t * dsc)
 *  \brief Decodes PNG image.
 *  \param dec: underlying image decoder.
 *  \param dsc: image descriptor containing source info.
 *  \returns LV_RES_OK if successful, LV_RES_INV if failed.
 */
static lv_res_t pngle_decoder_open(lv_img_decoder_t * dec, lv_img_decoder_dsc_t * dsc);

/** \fn void pngle_decoder_close(lv_img_decoder_t * dec, lv_img_decoder_dsc_t * dsc)
 *  \brief Closes and cleans up decoding data for given descriptor.
 *  \param dec: underlying image decoder.
 *  \param dsc: image descriptor.
 */
static void pngle_decoder_close(lv_img_decoder_t * dec, lv_img_decoder_dsc_t * dsc);

/** \fn void convert_color_depth(uint8_t * img, uint32_t px_cnt)
 *  \brief Converts an image buffer to LVGL color format.
 *  \param img: pointer to image buffer.
 *  \param px_cnt: number of pixels.
 */
static void convert_color_depth(uint8_t * img, uint32_t px_cnt);

/** \brief A structure to communicate data and useful flags with Pngle.
 */
typedef struct _lv_pngle_data_t {
    /** \property bool hdr_ready
     *  \brief If true, header parsing is done.
     */
    bool hdr_ready;

    /** \property bool data_ready
     *  \brief If true, data parsing is done.
     */
    bool data_ready;

    /** \property uint8_t * data
     *  \brief Pointer to data buffer. Size must be 4 * width * height.
     */
    uint8_t * data;
} lv_pngle_data_t;


void lv_pngle_init(void) {
    lv_img_decoder_t * dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, pngle_decoder_info);
    lv_img_decoder_set_open_cb(dec, pngle_decoder_open);
    lv_img_decoder_set_close_cb(dec, pngle_decoder_close);
}

/** \fn void lv_pngle_data_init(pngle_t* pngle, lv_pngle_data_t * ud)
 *  \brief Initializes a data structure to interact with Pngle.
 *  \param pngle: pointer to a Pngle instance.
 *  \param ud: pointer to the data structure to initialize.
 */
static void lv_pngle_data_init(pngle_t* pngle, lv_pngle_data_t * ud) {
    ud = (lv_pngle_data_t*)malloc(sizeof(lv_pngle_data_t));
    ud->hdr_ready = false;
    ud->data_ready = false;
    pngle_set_user_data(pngle, ud);
}

/** \fn void lv_pngle_buffer_init(uint8_t ** buf, uint32_t n_px)
 *  \brief Initializes data buffer.
 *  \param buf: pointer to data buffer.
 *  \param n_px: number of pixels.
 */
static void lv_pngle_buffer_init(uint8_t ** buf, uint32_t n_px) {
#if LV_COLOR_DEPTH == 32
    LV_LOG_INFO("allocating memory for image: %d bytes\n", n_px*4);
    *buf = (uint8_t*)calloc(n_px*4, sizeof(uint8_t));
#elif LV_COLOR_DEPTH == 16
    LV_LOG_INFO("allocating memory for image: %d bytes\n", n_px*3);
    *buf = (uint8_t*)calloc(n_px*3, sizeof(uint8_t));
#elif LV_COLOR_DEPTH == 8
    LV_LOG_INFO("allocating memory for image: %d bytes\n", n_px*2);
    *buf = (uint8_t*)calloc(n_px*2, sizeof(uint8_t));
#elif LV_COLOR_DEPTH == 1
    LV_LOG_INFO("allocating memory for image: %d bytes\n", n_px*2);
    *buf = (uint8_t*)calloc(n_px*2, sizeof(uint8_t));
#endif
}

/** \fn void pngle_init_cb(pngle_t* pngle, uint32_t w, uint32_t h)
 *  \brief Function called when image width and height could be read from header.
 *  \param pngle: pointer to a Pngle instance.
 *  \param w: image width.
 *  \param h: image height.
 */
static void pngle_init_cb(pngle_t* pngle, uint32_t w, uint32_t h) {
    LV_LOG_INFO("PNG image header read succesfully. Size: %d x %d\n", w, h);
    lv_pngle_data_t * ud = (lv_pngle_data_t*)pngle_get_user_data(pngle);
    ud->hdr_ready = true;
}

/** \fn void pngle_draw_cb(pngle_t* pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *rgba)
 *  \brief Function called when a pixel is read.
 *  \param pngle: pointer to a Pngle instance.
 *  \param x: horizontal coordinate of pixel.
 *  \param y: vertical coordinate of pixel.
 *  \param w: image width.
 *  \param h: image height.
 *  \param rgba: pointer to pixel value.
 */
static void pngle_draw_cb(pngle_t* pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *rgba) {
    lv_pngle_data_t * ud = (lv_pngle_data_t*)pngle_get_user_data(pngle);
    LV_LOG_TRACE("received pixel (%d,%d) with rgba color (0x%02x,0x%02x,0x%02x,0x%02x)\n",
                 x, y, rgba[0], rgba[1], rgba[2], rgba[3]);
#if LV_COLOR_DEPTH == 32
    *ud->data++ = rgba[2];
    *ud->data++ = rgba[1];
    *ud->data++ = rgba[0];
    *ud->data++ = rgba[3];
#elif LV_COLOR_DEPTH == 16
    uint16_t col = ((rgba[0] & 0xf8) << 8) | ((rgba[1] & 0xfc) << 3) | ((rgba[2] & 0xf8) >> 3);
    *ud->data++ = col & 0xff;
    *ud->data++ = col >> 8;
    *ud->data++ = rgba[3];
#elif LV_COLOR_DEPTH == 8
    uint8_t col = (rgba[0] & 0xe0) | ((rgba[1] & 0xe0) >> 3) | ((rgba[2] & 0xc0) >> 6);
    *ud->data++ = col;
    *ud->data++ = rgba[3];
#elif LV_COLOR_DEPTH == 1
    uint8_t col = (rgba[0] | rgba[1] | rgba[2]) & 0x80;
    *ud->data++ = col >> 7;
    *ud->data++ = rgba[3];
#endif
}

/** \fn void pngle_done_cb(pngle_t* pngle)
 *  \brief Function called when reading image data is done.
 *  \param pngle: pointer to a Pngle instance.
 */
static void pngle_done_cb(pngle_t* pngle) {
    LV_LOG_INFO("PNG image read succesfully.");
    lv_pngle_data_t * ud = (lv_pngle_data_t*)pngle_get_user_data(pngle);
    ud->data_ready = true;
#if LV_COLOR_DEPTH == 32
    ud->data-=4*pngle_get_width(pngle)*pngle_get_height(pngle);
#elif LV_COLOR_DEPTH == 16
    ud->data-=3*pngle_get_width(pngle)*pngle_get_height(pngle);
#elif LV_COLOR_DEPTH == 8
    ud->data-=2*pngle_get_width(pngle)*pngle_get_height(pngle);
#elif LV_COLOR_DEPTH == 1
    ud->data-=2*pngle_get_width(pngle)*pngle_get_height(pngle);
#endif
}

/** \fn lv_res_t read_next_chunk(pngle_t * pngle, lv_fs_file_t * f)
 *  \brief Reads next image chunk.
 *  \param pngle: pointer to a Pngle instance.
 *  \param f: pointer to image file.
 */
static lv_res_t read_next_chunk(pngle_t * pngle, lv_fs_file_t * f) {
    // chunk structure: length (4 bytes) | chunk type (4 bytes) | chunk data (length) | CRC (4 bytes)
    // we read 4 bytes to get length and add 8 bytes to account for type and CRC
    char buf[1024];
    uint32_t rb, btr;
    lv_fs_read(f, &buf, 8, &rb);
    int chunk_length = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]) + 4; // add 4 for CRC
    if (chunk_length < 0) return LV_RES_INV;
    if (pngle_feed(pngle, buf, 8) < 0) {
        LV_LOG_ERROR("error reading PNG image: couldn't parse chunk header.\n");
        return LV_RES_INV;
    }
    LV_LOG_INFO("PNG header chunk size: %d", chunk_length);
    while (chunk_length > 0) {
        btr = (chunk_length < 1024) ? chunk_length : 1024;
        lv_fs_read(f, &buf, btr, &rb);
        chunk_length -= btr;
        if (pngle_feed(pngle, buf, btr) < 0) {
            LV_LOG_ERROR("error reading PNG image: couldn't parse chunk data.\n");
            return LV_RES_INV;
        }
    }
    return LV_RES_OK;
}


/** \fn lv_res_t get_pngle_header(pngle_t * pngle, lv_fs_file_t * f)
 *  \brief Reads PNG image header.
 *  \param pngle: pointer to a Pngle instance.
 *  \param f: pointer to image file.
 */
static lv_res_t get_pngle_header(pngle_t * pngle, lv_fs_file_t * f) {
    LV_LOG_INFO("reading PNG image header...\n");
    char buf[8];
    uint32_t rb;
    LV_LOG_INFO("reading file signature...\n");
    lv_fs_read(f, &buf, 8, &rb);
    if (pngle_feed(pngle, buf, 8) < 0) {
        LV_LOG_ERROR("error reading PNG header: couldn't parse file signature.\n");
        return LV_RES_INV;
    }
    while (!(((lv_pngle_data_t*)pngle_get_user_data(pngle))->hdr_ready)) {
        LV_LOG_INFO("reading PNG header: read next chunk.\n");
        if (read_next_chunk(pngle, f) != LV_RES_OK) return LV_RES_INV;
    }
    return LV_RES_OK;
}


/** \fn lv_res_t get_pngle_data(pngle_t * pngle, lv_fs_file_t * f)
 *  \brief Reads PNG image data.
 *  \param pngle: pointer to a Pngle instance.
 *  \param f: pointer to image file.
 */
static lv_res_t get_pngle_data(pngle_t * pngle, lv_fs_file_t * f) {
    LV_LOG_INFO("reading PNG image data...\n");
    do {
        if (read_next_chunk(pngle, f) != LV_RES_OK) return LV_RES_INV;
    } while (!(((lv_pngle_data_t*)pngle_get_user_data(pngle))->data_ready));
    return LV_RES_OK;
}


static lv_res_t pngle_decoder_info(struct _lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header) {
    LV_UNUSED(decoder);
    lv_img_src_t src_type = lv_img_src_get_type(src);

    if(src_type == LV_IMG_SRC_FILE) {
        const char * fn = src;
        if(!strcmp(&fn[strlen(fn) - 3], "png")) {
            LV_LOG_INFO("reading PNG image info from file: %s\n", fn);
            pngle_t * pngle = pngle_new();
            if (pngle == NULL) {
                LV_LOG_ERROR("couldn't create Pngle instance.\n");
                return LV_RES_INV;
            }

            lv_pngle_data_t ud;
            lv_pngle_data_init(pngle, &ud);

            pngle_set_init_callback(pngle, pngle_init_cb);

            lv_fs_file_t f;
            bool failed = false;
            if(lv_fs_open(&f, fn, LV_FS_MODE_RD) == LV_FS_RES_OK)  {
                if (get_pngle_header(pngle, &f) == LV_RES_OK) {
                    header->always_zero = 0;
                    header->cf = LV_IMG_CF_RAW_ALPHA;
                    header->w = (lv_coord_t)pngle_get_width(pngle);
                    header->h = (lv_coord_t)pngle_get_height(pngle);
                } else {
                    LV_LOG_ERROR("couldn't access header from: %s\n", fn);
                    failed = true;
                }
                lv_fs_close(&f);
            } else {
                LV_LOG_ERROR("couldn't access PNG file: %s\n", fn);
                failed = true;
            }
            pngle_destroy(pngle);

            return failed ? LV_RES_INV : LV_RES_OK;
        }
    } else if(src_type == LV_IMG_SRC_VARIABLE) {
        LV_LOG_INFO("reading PNG image info from buffer...\n");
        const lv_img_dsc_t * img_dsc = src;
        const uint8_t magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
        if(memcmp(magic, img_dsc->data, sizeof(magic))) return LV_RES_INV;
        header->always_zero = 0;
        header->cf = img_dsc->header.cf;
        header->w = img_dsc->header.w;
        header->h = img_dsc->header.h;
        return LV_RES_OK;
    }

    return LV_RES_INV;
}


static lv_res_t pngle_decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc) {
    LV_UNUSED(decoder);
    if (dsc->src_type != LV_IMG_SRC_FILE && dsc->src_type != LV_IMG_SRC_VARIABLE)
        return LV_RES_INV;
    
    pngle_t * pngle = pngle_new();
    if (pngle == NULL) {
        LV_LOG_ERROR("couldn't create Pngle instance.\n");
        return LV_RES_INV;
    }

    lv_pngle_data_t ud = {false, false, NULL};
    lv_pngle_data_init(pngle, &ud);
    pngle_set_draw_callback(pngle, pngle_draw_cb);
    pngle_set_init_callback(pngle, pngle_init_cb);
    pngle_set_done_callback(pngle, pngle_done_cb);

    uint32_t png_width=0, png_height=0;
    bool failed = false;

    if(dsc->src_type == LV_IMG_SRC_FILE) {
        const char * fn = dsc->src;
        if(!strcmp(&fn[strlen(fn) - 3], "png")) {
            LV_LOG_INFO("reading PNG image data from file: %s\n", fn);
            lv_fs_file_t f;
            if(lv_fs_open(&f, fn, LV_FS_MODE_RD) == LV_FS_RES_OK) {
                if(get_pngle_header(pngle, &f) == LV_RES_OK) {
                    png_width = pngle_get_width(pngle);
                    png_height = pngle_get_height(pngle);
                    lv_pngle_buffer_init(&ud.data, png_width*png_height);
                    // this has to be done here otherwise buffer address isn't stored
                    pngle_set_user_data(pngle, &ud);
                    if (get_pngle_data(pngle, &f) != LV_RES_OK) {
                        LV_LOG_ERROR("reading PNG data failed.\n");
                        failed = true;
                    }
                } else {
                    LV_LOG_ERROR("reading PNG header failed.\n");
                    failed = true;
                }
            } else {
                failed = true;
            }
            lv_fs_close(&f);
        }
    } else if(dsc->src_type == LV_IMG_SRC_VARIABLE) {
        LV_LOG_INFO("reading PNG image data from buffer...\n");
        const lv_img_dsc_t * img_src = dsc->src;
        png_width = dsc->header.w;
        png_height = dsc->header.h;
        lv_pngle_buffer_init(&ud.data, png_width*png_height);
        // this has to be done here otherwise buffer address isn't stored
        pngle_set_user_data(pngle, &ud);
        // feed Pngle with data until image is complete
        uint32_t pos = 0, sz = img_src->data_size, btr;
        while (!ud.data_ready) {
            btr = (sz < 1024) ? sz : 1024;
            if (pngle_feed(pngle, img_src->data+pos, btr) < 0) {
                failed = true;
                LV_LOG_ERROR("Pngle returned an error.\n");
                break;
            }
            pos += btr;
        }
    }
    
    if (failed) {
        LV_LOG_ERROR("PNG decoding failed.\n");
        if (ud.data != NULL)
            free(ud.data);
    } else {
        LV_LOG_INFO("PNG decoding succeeded. Converting.\n");
        dsc->img_data = ud.data;
    }
    pngle_destroy(pngle);
    return failed ? LV_RES_INV : LV_RES_OK;
}


static void pngle_decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    if(dsc->img_data) {
        lv_mem_free((uint8_t *)dsc->img_data);
        dsc->img_data = NULL;
    }
}
