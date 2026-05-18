/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma GCC optimize("O3")

#include "bitmapbuffer.h"
#include "lib_file.h"
#include "edgetx_helpers.h"
#include "debug.h"

#include <new>

FIL imgFile __DMA;

// #define TRACE_STB_MALLOC

#if defined(TRACE_STB_MALLOC)
#define STBI_MALLOC(sz) stb_malloc(sz)
#define STBI_REALLOC_SIZED(p, oldsz, newsz) stb_realloc(p, oldsz, newsz)
#define STBI_FREE(p) stb_free(p)

void *stb_malloc(unsigned int size)
{
  void *res = malloc(size);
  TRACE("malloc %d = %p", size, res);
  return res;
}

void stb_free(void *ptr)
{
  TRACE("free %p", ptr);
  free(ptr);
}

void *stb_realloc(void *ptr, unsigned int oldsz, unsigned int newsz)
{
  void *res = realloc(ptr, newsz);
  TRACE("realloc %p, %d -> %d = %p", ptr, oldsz, newsz, res);
  return res;
}
#endif  // #if defined(TRACE_STB_MALLOC)

#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#undef __I
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "stb/stb_image.h"

// fill 'data' with 'size' bytes.  return number of bytes actually read
int stbc_read(void *user, char *data, int size)
{
  FIL *fp = (FIL *)user;
  UINT br = 0;
  FRESULT res = f_read(fp, data, size, &br);
  if (res == FR_OK) {
    return (int)br;
  }
  return 0;
}

// skip the next 'n' bytes, or 'unget' the last -n bytes if negative
void stbc_skip(void *user, int n)
{
  FIL *fp = (FIL *)user;
  f_lseek(fp, f_tell(fp) + n);
}

// returns nonzero if we are at end of file/data
int stbc_eof(void *user)
{
  FIL *fp = (FIL *)user;
  int res = f_eof(fp);
  return res;
}

// callbacks for stb-image
const stbi_io_callbacks stbCallbacks = {stbc_read, stbc_skip, stbc_eof};

BitmapBuffer *BitmapBuffer::loadBitmap(const char *filename, BitmapFormats fmt)
{
  if ((filename == nullptr) || (filename[0] == 0)) return nullptr;

  FRESULT result = f_open(&imgFile, filename, FA_OPEN_EXISTING | FA_READ);
  if (result != FR_OK) {
    return nullptr;
  }

  int x, y, nn;
  stbi_info_from_callbacks(&stbCallbacks, &imgFile, &x, &y, &nn);
  f_lseek(&imgFile, 0);

  int w, h, n;
  unsigned char *img =
      stbi_load_from_callbacks(&stbCallbacks, &imgFile, &w, &h, &n, 4);
  f_close(&imgFile);

  if (!img) {
    TRACE_ERROR("loadBitmap(%s) failed: %s\n", filename, stbi_failure_reason());
    return nullptr;
  }

  // convert to RGB565 or ARGB4444 format
  BitmapFormats dst_fmt = fmt;
  if (dst_fmt == BMP_INVALID) {
    dst_fmt = (n == 4 ? BMP_ARGB4444 : BMP_RGB565);
  }

  BitmapBuffer *bmp = new (std::nothrow) BitmapBuffer(dst_fmt, w, h);
  if (bmp == nullptr || bmp->getData() == nullptr) {
    TRACE_ERROR("loadBitmap: malloc failed\n");
    delete bmp;
    stbi_image_free(img);
    return nullptr;
  }

  pixel_t *dest = bmp->getPixelPtrAbs(0, 0);
  const uint8_t *p = img;
  if (dst_fmt == BMP_ARGB4444) {
    for (int row = 0; row < h; ++row) {
      for (int col = 0; col < w; ++col) {
        *dest = ARGB(p[3], p[0], p[1], p[2]);
        MOVE_TO_NEXT_RIGHT_PIXEL(dest);
        p += 4;
      }
    }
  } else {  // assume 3 bytes, packed in groups of 4
    for (int row = 0; row < h; ++row) {
      for (int col = 0; col < w; ++col) {
        *dest = RGB(p[0] & 0xF8, p[1] & 0xFC, p[2] & 0xF8);
        MOVE_TO_NEXT_RIGHT_PIXEL(dest);
        p += 4;
      }
    }
  }

  stbi_image_free(img);
  return bmp;
}

//-----------------------------------------------------------------------------

static lv_result_t decoder_info(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc,
                             lv_image_header_t *header)
{
  LV_UNUSED(decoder); /*Unused*/

  lv_image_src_t src_type = dsc->src_type;

  /*If it's a file...*/
  if (src_type == LV_IMAGE_SRC_FILE) {
    const char *fn = ((const char *)dsc->src) + 1;
    FIL imgFile;

    FRESULT result = f_open(&imgFile, fn, FA_OPEN_EXISTING | FA_READ);
    if (result == FR_OK) {
      int x, y, nn;
      int res = stbi_info_from_callbacks(&stbCallbacks, &imgFile, &x, &y, &nn);
      f_close(&imgFile);

      if (res == 0) {
        TRACE_ERROR("decoder_info(%s) failed: %s\n", fn, stbi_failure_reason());
        return LV_RESULT_INVALID;
      }

      header->cf =
          (nn == 4) ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
      header->w = x;
      header->h = y;

      return LV_RESULT_OK;
    } else {
      TRACE_ERROR("decoder_info(%s) failed to open image file\n", fn);
    }
  }
  /*If it's a file in a C array...*/
  else if (src_type == LV_IMAGE_SRC_VARIABLE) {
    // Not implemented...
  }

  return LV_RESULT_INVALID;
}

static uint8_t *fill_draw_buf(lv_draw_buf_t *decoded, uint8_t *img, int n)
{
  int32_t w = decoded->header.w;
  int32_t h = decoded->header.h;
  uint8_t *dst = decoded->data;
  const uint8_t *p = img;

  // Fill RGB565 pixels
  for (int32_t row = 0; row < h; ++row) {
    for (int32_t col = 0; col < w; ++col) {
      uint16_t c = RGB(p[0], p[1], p[2]);
      *dst++ = c & 0xFF;
      *dst++ = c >> 8;
      p += 4;
    }
  }

  // For RGB565A8, fill alpha plane after RGB data
  if (n == 4) {
    uint8_t *aplane = decoded->data + w * h * 2;
    const uint8_t *ap = img;
    for (int32_t row = 0; row < h; ++row) {
      for (int32_t col = 0; col < w; ++col) {
        *aplane++ = ap[3];  // alpha byte
        ap += 4;
      }
    }
  }

  return decoded->data;
}

static lv_result_t decoder_open(lv_image_decoder_t *decoder,
                             lv_image_decoder_dsc_t *dsc)
{
  LV_UNUSED(decoder); /*Unused*/

  /*If it's a file...*/
  if (dsc->src_type == LV_IMAGE_SRC_FILE) {
    const char *fn = ((const char *)dsc->src) + 1;
    FIL imgFile;

    FRESULT result = f_open(&imgFile, fn, FA_OPEN_EXISTING | FA_READ);
    if (result == FR_OK) {
      int w, h, n;
      unsigned char *img =
          stbi_load_from_callbacks(&stbCallbacks, &imgFile, &w, &h, &n, 4);
      f_close(&imgFile);

      if (!img) {
        TRACE_ERROR("decoder_open(%s) failed: %s\n", fn, stbi_failure_reason());
        return LV_RESULT_INVALID;
      }

      lv_color_format_t cf = (n == 4) ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
      lv_draw_buf_t *decoded = lv_draw_buf_create(w, h, cf, LV_STRIDE_AUTO);
      if (!decoded) {
        stbi_image_free(img);
        TRACE_ERROR("decoder_open(%s) failed to allocate draw buf\n", fn);
        return LV_RESULT_INVALID;
      }

      fill_draw_buf(decoded, img, n);
      stbi_image_free(img);

      dsc->decoded = decoded;
      return LV_RESULT_OK;
    } else {
      TRACE_ERROR("decoder_open(%s) failed to open image file\n", fn);
    }
  }
  /*If it's a file in a C array...*/
  else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
    // Not implemented...
  }

  return LV_RESULT_INVALID;
}

static void decoder_close(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
  LV_UNUSED(decoder); /*Unused*/
  if (dsc->decoded) {
    lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    dsc->decoded = NULL;
  }
}

/**
 * Register the STB decoder functions in LVGL
 */
void lv_stb_init(void)
{
  lv_image_decoder_t *dec = lv_image_decoder_create();
  lv_image_decoder_set_info_cb(dec, decoder_info);
  lv_image_decoder_set_open_cb(dec, decoder_open);
  lv_image_decoder_set_close_cb(dec, decoder_close);
}
