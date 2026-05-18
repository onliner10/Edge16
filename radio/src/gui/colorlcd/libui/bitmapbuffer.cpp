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

#include "bitmapbuffer.h"

#include <math.h>

#include <string>

#include "bitmaps.h"
#include "board.h"
#include "dma2d.h"
#include "edgetx_helpers.h"
#include "fonts.h"
#include "lvgl/src/draw/sw/lv_draw_sw.h"
#include "strhelpers.h"

#if defined(SIMU)
static bool forceBitmapBufferCanvasCreateFailure = false;
static bool forceBitmapBufferDataMallocFailure = false;
static bool forceBitmapBufferResizeMallocFailure = false;

void bitmapBufferForceCanvasCreateFailureForTest(bool force)
{
  forceBitmapBufferCanvasCreateFailure = force;
}

void bitmapBufferForceDataMallocFailureForTest(bool force)
{
  forceBitmapBufferDataMallocFailure = force;
}

void bitmapBufferForceResizeMallocFailureForTest(bool force)
{
  forceBitmapBufferResizeMallocFailure = force;
}
#endif

#if !defined(BOOT)
static lv_obj_t* createBitmapBufferCanvas(lv_obj_t* parent)
{
#if defined(SIMU)
  if (forceBitmapBufferCanvasCreateFailure) return nullptr;
#endif
  return lv_canvas_create(parent);
}
#endif

BitmapBuffer::BitmapBuffer(uint8_t format, uint16_t width, uint16_t height) :
    format(format),
    _width(width),
    _height(height),
    xmax(width),
    ymax(height),
    dataAllocated(true)
#if defined(DEBUG)
    ,
    leakReported(false)
#endif
{
#if defined(SIMU)
  data = forceBitmapBufferDataMallocFailure
             ? nullptr
             : (uint16_t*)malloc(align32(width * height * sizeof(uint16_t)));
#else
  data = (uint16_t*)malloc(align32(width * height * sizeof(uint16_t)));
#endif
  if (!data) {
    dataAllocated = false;
    _width = 0;
    _height = 0;
    xmax = 0;
    ymax = 0;
    data_end = nullptr;
    return;
  }

  data_end = data + (width * height);

#if !defined(BOOT)
  // Assume we need a canvas here
  canvas = createBitmapBufferCanvas(nullptr);
  if (canvas)
    lv_canvas_set_buffer(canvas, data, width, height, LV_COLOR_FORMAT_RGB565);
#endif
}

BitmapBuffer::BitmapBuffer(uint8_t format, uint16_t width, uint16_t height,
                           uint16_t* data) :
    format(format),
    _width(width),
    _height(height),
    xmax(width),
    ymax(height),
    data(data),
    data_end(data + (width * height)),
    dataAllocated(false)
#if defined(DEBUG)
    ,
    leakReported(false)
#endif
{
}

BitmapBuffer::~BitmapBuffer()
{
  DMAWait();
  if (dataAllocated) {
#if !defined(BOOT)
    if (canvas) lv_obj_del(canvas);
#endif
    free(data);
  }
}

void BitmapBuffer::setData(uint16_t* d)
{
  if (!dataAllocated) {
    data = d;
    data_end = d + (_width * _height);
  }
}

void BitmapBuffer::clear(LcdFlags flags)
{
  drawSolidFilledRect(0, 0, _width - offsetX, _height - offsetY, flags);
}

void BitmapBuffer::clearClippingRect()
{
  xmin = 0;
  xmax = _width;
  ymin = 0;
  ymax = _height;
}

void BitmapBuffer::setClippingRect(coord_t xmin, coord_t xmax, coord_t ymin,
                                   coord_t ymax)
{
  this->xmin = xmin;
  this->xmax = xmax;
  this->ymin = ymin;
  this->ymax = ymax;
}

void BitmapBuffer::getClippingRect(coord_t& xmin, coord_t& xmax, coord_t& ymin,
                                   coord_t& ymax)
{
  xmin = this->xmin;
  xmax = this->xmax;
  ymin = this->ymin;
  ymax = this->ymax;
}

bool BitmapBuffer::applyClippingRect(coord_t& x, coord_t& y, coord_t& w,
                                     coord_t& h) const
{
  if (h < 0) {
    y += h;
    h = -h;
  }

  if (w < 0) {
    x += w;
    w = -w;
  }

  if (x >= xmax || y >= ymax) return false;

  if (y < ymin) {
    h += y - ymin;
    y = ymin;
  }

  if (x < xmin) {
    w += x - xmin;
    x = xmin;
  }

  if (y + h > ymax) h = ymax - y;

  if (x + w > xmax) w = xmax - x;

  return data && h > 0 && w > 0;
}

void BitmapBuffer::setOffset(coord_t offsetX, coord_t offsetY)
{
  this->offsetX = offsetX;
  this->offsetY = offsetY;
}

void BitmapBuffer::drawBitmap(coord_t x, coord_t y, const BitmapBuffer* bmp,
                              coord_t srcx, coord_t srcy, coord_t srcw,
                              coord_t srch, float scale)
{
  if (!data || !bmp) return;
  APPLY_OFFSET();
  if (x >= xmax || y >= ymax) return;

  coord_t bmpw = bmp->width();
  coord_t bmph = bmp->height();

  if (srcw == 0) srcw = bmpw;
  if (srch == 0) srch = bmph;
  if (srcx + srcw > bmpw) srcw = bmpw - srcx;
  if (srcy + srch > bmph) srch = bmph - srcy;

  if (scale == 0) {
    if (x < xmin) {
      srcw += x - xmin;
      srcx -= x - xmin;
      x = xmin;
    }
    if (y < ymin) {
      srch += y - ymin;
      srcy -= y - ymin;
      y = ymin;
    }
    if (x + srcw > xmax) {
      srcw = xmax - x;
    }
    if (y + srch > ymax) {
      srch = ymax - y;
    }
  } else {
    if (x < xmin) {
      srcw += (x - xmin) / scale;
      srcx -= (x - xmin) / scale;
      x = xmin;
    }
    if (y < ymin) {
      srch += (y - ymin) / scale;
      srcy -= (y - ymin) / scale;
      y = ymin;
    }
    if (x + srcw * scale > xmax) {
      srcw = (xmax - x) / scale;
    }
    if (y + srch * scale > ymax) {
      srch = (ymax - y) / scale;
    }
  }

  if (srcw <= 0 || srch <= 0) {
    return;
  }

  if (scale == 0) {
    if (bmp->format == BMP_ARGB4444) {
      DMACopyAlphaBitmap(data, _width, _height, x, y, bmp->getData(), bmpw,
                         bmph, srcx, srcy, srcw, srch);
    } else {
      DMACopyBitmap(data, _width, _height, x, y, bmp->getData(), bmpw, bmph,
                    srcx, srcy, srcw, srch);
    }
    DMAWait();

#if __CORTEX_M >= 0x07
    SCB_CleanInvalidateDCache();
#endif
  } else {
    int scaledw = srcw * scale;
    int scaledh = srch * scale;

    if (x + scaledw > _width) scaledw = _width - x;
    if (y + scaledh > _height) scaledh = _height - y;

    if (format == BMP_ARGB4444) {
      for (int i = 0; i < scaledh; i++) {
        pixel_t* p = getPixelPtrAbs(x, y + i);
        const pixel_t* qstart =
            bmp->getPixelPtrAbs(srcx, srcy + int(i / scale));

        for (int j = 0; j < scaledw; j++) {
          const pixel_t* q = qstart;
          MOVE_PIXEL_RIGHT(q, int(j / scale));

          if (bmp->format == BMP_RGB565) {
            RGB_SPLIT(*q, r, g, b);
            drawPixel(p, ARGB_JOIN(0xF, r >> 1, g >> 2, b >> 1));

          } else {  // bmp->format == BMP_ARGB4444
            drawPixel(p, *q);
          }
          MOVE_TO_NEXT_RIGHT_PIXEL(p);
        }
      }
    } else {  // format == BM_RGB565

      for (int i = 0; i < scaledh; i++) {
        pixel_t* p = getPixelPtrAbs(x, y + i);
        const pixel_t* qstart =
            bmp->getPixelPtrAbs(srcx, srcy + int(i / scale));

        for (int j = 0; j < scaledw; j++) {
          const pixel_t* q = qstart;
          MOVE_PIXEL_RIGHT(q, int(j / scale));

          if (bmp->format == BMP_RGB565) {
            drawPixel(p, *q);
          } else {  // bmp->format == BMP_ARGB4444
            ARGB_SPLIT(*q, a, r, g, b);
            drawAlphaPixel(p, a, RGB_JOIN(r << 1, g << 2, b << 1));
          }
          MOVE_TO_NEXT_RIGHT_PIXEL(p);
        }  // for j
      }  // for i
    }  // if format
  }  //  else (scale != 0) {
}

void BitmapBuffer::drawAlphaPixel(pixel_t* p, uint8_t opacity, uint16_t color)
{
  // TRACE("BitmapBuffer::drawAlphaPixel()");
  if (opacity == OPACITY_MAX) {
    drawPixel(p, color);
  } else if (opacity != 0) {
    uint8_t bgWeight = OPACITY_MAX - opacity;
    RGB_SPLIT(color, red, green, blue);
    RGB_SPLIT(*p, bgRed, bgGreen, bgBlue);
    uint16_t r = (bgRed * bgWeight + red * opacity) / OPACITY_MAX;
    uint16_t g = (bgGreen * bgWeight + green * opacity) / OPACITY_MAX;
    uint16_t b = (bgBlue * bgWeight + blue * opacity) / OPACITY_MAX;
    drawPixel(p, RGB_JOIN(r, g, b));
  }
}

void BitmapBuffer::drawHorizontalLine(coord_t x, coord_t y, coord_t w,
                                      uint8_t pat, LcdFlags flags,
                                      uint8_t opacity)
{
  if (opacity == OPACITY_MAX) return;

  APPLY_OFFSET();

  coord_t h = 1;
  if (!applyClippingRect(x, y, w, h)) return;

  drawHorizontalLineAbs(x, y, w, pat, flags, opacity);
}

void BitmapBuffer::drawHorizontalLineAbs(coord_t x, coord_t y, coord_t w,
                                         uint8_t pat, LcdFlags flags,
                                         uint8_t opacity)
{
  if (opacity == OPACITY_MAX) return;

  if (!draw_ctx) {
    pixel_t* p = getPixelPtrAbs(x, y);
    for (coord_t col = 0; col < w; col++) {
      if (pat == SOLID || ((col & 1) == 0)) {
        if (opacity == 0) {
          drawPixel(p, COLOR_VAL(flags));
        } else {
          drawAlphaPixel(p, OPACITY_MAX - opacity, COLOR_VAL(flags));
        }
      }
      MOVE_TO_NEXT_RIGHT_PIXEL(p);
    }
    return;
  }

#if !defined(BOOT)
  x += draw_ctx->buf_area.x1;
  y += draw_ctx->buf_area.y1;

  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);

  line_dsc.width = 1;
  line_dsc.opa = ((OPACITY_MAX - opacity) * LV_OPA_COVER) / OPACITY_MAX;
  line_dsc.color = makeLvColor(flags);

  if (pat == DOTTED) {
    line_dsc.dash_gap = 1;
    line_dsc.dash_width = 1;
  }

  line_dsc.p1.x = x;
  line_dsc.p1.y = y;
  line_dsc.p2.x = x + w;
  line_dsc.p2.y = y;
  lv_draw_line(draw_ctx, &line_dsc);
#endif
}

void BitmapBuffer::drawVerticalLine(coord_t x, coord_t y, coord_t h,
                                    uint8_t pat, LcdFlags flags,
                                    uint8_t opacity)
{
  if (opacity == OPACITY_MAX) return;

  APPLY_OFFSET();

  coord_t w = 1;
  if (!applyClippingRect(x, y, w, h)) return;

  if (!draw_ctx) {
    pixel_t* p = getPixelPtrAbs(x, y);
    for (coord_t line = 0; line < h; line++) {
      if (pat == SOLID || ((line & 1) == 0)) {
        if (opacity == 0) {
          drawPixel(p, COLOR_VAL(flags));
        } else {
          drawAlphaPixel(p, OPACITY_MAX - opacity, COLOR_VAL(flags));
        }
      }
      p += _width;
    }
    return;
  }

#if !defined(BOOT)
  x += draw_ctx->buf_area.x1;
  y += draw_ctx->buf_area.y1;

  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);

  line_dsc.width = 1;
  line_dsc.opa = ((OPACITY_MAX - opacity) * LV_OPA_COVER) / OPACITY_MAX;
  line_dsc.color = makeLvColor(flags);

  if (pat == DOTTED) {
    line_dsc.dash_gap = 1;
    line_dsc.dash_width = 1;
  }

  line_dsc.p1.x = x;
  line_dsc.p1.y = y;
  line_dsc.p2.x = x;
  line_dsc.p2.y = y + h;
  lv_draw_line(draw_ctx, &line_dsc);
#endif
}

void BitmapBuffer::drawRect(coord_t x, coord_t y, coord_t w, coord_t h,
                            uint8_t thickness, uint8_t pat, LcdFlags flags,
                            uint8_t opacity)
{
  drawFilledRect(x, y, thickness, h, pat, flags, opacity);
  drawFilledRect(x + w - thickness, y, thickness, h, pat, flags, opacity);
  drawFilledRect(x, y, w, thickness, pat, flags, opacity);
  drawFilledRect(x, y + h - thickness, w, thickness, pat, flags, opacity);
}

void BitmapBuffer::drawSolidRect(coord_t x, coord_t y, coord_t w, coord_t h,
                                 uint8_t thickness, LcdFlags flags)
{
  drawRect(x, y, w, h, thickness, SOLID, flags, 0);
}

void BitmapBuffer::drawSolidFilledRect(coord_t x, coord_t y, coord_t w,
                                       coord_t h, LcdFlags flags)
{
  drawFilledRect(x, y, w, h, SOLID, flags, 0);
}

void BitmapBuffer::drawFilledRect(coord_t x, coord_t y, coord_t w, coord_t h,
                                  uint8_t pat, LcdFlags flags, uint8_t opacity)
{
  if (opacity == OPACITY_MAX) return;

  APPLY_OFFSET();
  if (!applyClippingRect(x, y, w, h)) return;

  if (SOLID != pat) {
    // If we have a pattern, draw line by line
    for (coord_t i = y; i < y + h; i++) {
      drawHorizontalLineAbs(x, i, w, pat, flags, opacity);
    }
    return;
  }

  if (!draw_ctx) {
    if (opacity == 0) {
      for (coord_t line = y; line < y + h; line++) {
        pixel_t* p = getPixelPtrAbs(x, line);
        for (coord_t col = 0; col < w; col++) {
          drawPixel(p, COLOR_VAL(flags));
          MOVE_TO_NEXT_RIGHT_PIXEL(p);
        }
      }
    } else {
      for (coord_t line = y; line < y + h; line++) {
        pixel_t* p = getPixelPtrAbs(x, line);
        for (coord_t col = 0; col < w; col++) {
          drawAlphaPixel(p, OPACITY_MAX - opacity, COLOR_VAL(flags));
          MOVE_TO_NEXT_RIGHT_PIXEL(p);
        }
      }
    }
    return;
  }

#if !defined(BOOT)
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = ((OPACITY_MAX - opacity) * LV_OPA_COVER) / OPACITY_MAX;
  rect_dsc.bg_color = makeLvColor(flags);
  rect_dsc.shadow_opa = LV_OPA_TRANSP;
  rect_dsc.outline_opa = LV_OPA_TRANSP;
  rect_dsc.border_opa = LV_OPA_TRANSP;

  if (draw_ctx) {
    x += draw_ctx->buf_area.x1;
    y += draw_ctx->buf_area.y1;
  }

  lv_area_t coords = {
      (lv_coord_t)x,
      (lv_coord_t)y,
      (lv_coord_t)(x + w - 1),
      (lv_coord_t)(y + h - 1),
  };

  if (draw_ctx) {
    lv_draw_rect(draw_ctx, &rect_dsc, &coords);
  } else if (canvas) {
    lv_layer_t _layer;
    lv_canvas_init_layer(canvas, &_layer);
    lv_draw_rect(&_layer, &rect_dsc, &coords);
    lv_canvas_finish_layer(canvas, &_layer);
  }
#endif
}

coord_t BitmapBuffer::drawSizedText(coord_t x, coord_t y, const char* s,
                                    uint8_t len, LcdFlags flags)
{
  if (!s) return x;
  MOVE_OFFSET();

  // LVGL does not handle non-null terminated strings
  static char buffer[256];
  strncpy(buffer, s, len);
  buffer[len] = '\0';

  coord_t pos = x;
  const coord_t orig_pos = pos;

  const lv_font_t* font = getFont(flags);

  if (draw_ctx) {
    x += draw_ctx->buf_area.x1;
    y += draw_ctx->buf_area.y1;
  }

  lv_point_t p;
  lv_text_get_size(&p, buffer, font, 0, 0, LV_COORD_MAX, (lv_text_flag_t)0);

  lv_coord_t lv_x = (lv_coord_t)x;
  lv_coord_t lv_y = (lv_coord_t)y;

  lv_area_t coords = {
      lv_x,
      lv_y,
      lv_x,
      lv_y,
  };

  coords.x2 += p.x - 1;
  coords.y2 += p.y - 1;

#if !defined(BOOT)
  lv_draw_label_dsc_t label_draw_dsc;
  lv_draw_label_dsc_init(&label_draw_dsc);

  label_draw_dsc.font = font;
  auto color = COLOR_VAL(flags);
  label_draw_dsc.color =
      lv_color_make(GET_RED(color), GET_GREEN(color), GET_BLUE(color));
  label_draw_dsc.text = buffer;
  label_draw_dsc.text_length = len;
  label_draw_dsc.text_local = true;

  if (flags & RIGHT) {
    label_draw_dsc.align = LV_TEXT_ALIGN_RIGHT;
    coords.x1 -= p.x;
    coords.x2 -= p.x;
  } else if (flags & CENTERED) {
    label_draw_dsc.align = LV_TEXT_ALIGN_CENTER;
    coords.x1 -= p.x / 2;
    coords.x2 -= p.x / 2;
  }
#else
  if (flags & RIGHT) {
    coords.x1 -= p.x;
    coords.x2 -= p.x;
  } else if (flags & CENTERED) {
    coords.x1 -= p.x / 2;
    coords.x2 -= p.x / 2;
  }
#endif

#if !defined(BOOT)
  if (draw_ctx) {
    lv_draw_label(draw_ctx, &label_draw_dsc, &coords);
  } else if (canvas) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_label(&layer, &label_draw_dsc, &coords);
    lv_canvas_finish_layer(canvas, &layer);
  }
#else
  {
    coord_t pen_x = coords.x1;
    uint32_t i = 0;
    while (buffer[i] != '\0') {
      uint32_t i_next = i;
      uint32_t letter = lv_text_encoded_next(buffer, &i_next);
      uint32_t letter_next = lv_text_encoded_next(&buffer[i_next], nullptr);

      lv_font_glyph_dsc_t glyph;
      if (lv_font_get_glyph_dsc(font, &glyph, letter, letter_next)) {
        glyph.req_raw_bitmap = 1;
        const uint8_t* bitmap = static_cast<const uint8_t*>(
            glyph.resolved_font->get_glyph_bitmap(&glyph, nullptr));

        if (bitmap && glyph.format == LV_FONT_GLYPH_FORMAT_A1) {
          coord_t glyph_x = pen_x + glyph.ofs_x;
          coord_t glyph_y = y + font->line_height - font->base_line -
                            glyph.box_h - glyph.ofs_y;

          for (uint16_t row = 0; row < glyph.box_h; row++) {
            for (uint16_t col = 0; col < glyph.box_w; col++) {
              uint32_t bit_index = row * glyph.box_w + col;
              if (bitmap[bit_index / 8] & (0x80 >> (bit_index % 8))) {
                drawPixel(glyph_x + col, glyph_y + row, COLOR_VAL(flags));
              }
            }
          }
        }

        if (glyph.resolved_font->release_glyph) {
          glyph.resolved_font->release_glyph(glyph.resolved_font, &glyph);
        }
        pen_x += glyph.adv_w;
      }

      i = i_next;
    }
  }
#endif

  RESTORE_OFFSET();

  pos += p.x;

  return ((flags & RIGHT) ? orig_pos : pos) - offsetX;
}

coord_t BitmapBuffer::drawText(coord_t x, coord_t y, const char* s,
                               LcdFlags flags)
{
  if (!s) return x;
  return drawSizedText(x, y, s, strlen(s), flags);
}

// Resize and convert to LVGL9's planar RGB565A8 image data format.
void BitmapBuffer::resizeToLVGL(coord_t w, coord_t h)
{
  auto invalidateSize = [this]() {
    _width = 0;
    _height = 0;
    xmax = 0;
    ymax = 0;
    data_end = data;
  };

  if (!data || width() == 0 || height() == 0 || w <= 0 || h <= 0) {
    invalidateSize();
    return;
  }

  // Scale values from ARGB4444 to RGB565 with alpha
  static uint8_t rbcnv[16] = {0,  2,  4,  6,  8,  10, 12, 14,
                              17, 19, 21, 23, 25, 27, 29, 31};
  static uint8_t gcnv[16] = {0,  4,  8,  13, 17, 21, 25, 29,
                             34, 38, 42, 46, 50, 55, 59, 63};
  static uint8_t alpha[16] = {0,   17,  34,  51,  68,  85,  102, 119,
                              136, 153, 170, 187, 204, 221, 238, 255};

  float vscale = float(h) / (float)height();
  float hscale = float(w) / (float)width();
  float scale;
  coord_t scaledw, scaledh;
  if (vscale < hscale) {
    scale = vscale;
    scaledw = width() * scale;
    scaledh = h;
  } else {
    scale = hscale;
    scaledw = w;
    scaledh = height() * scale;
  }
  if (scaledw <= 0 || scaledh <= 0) {
    invalidateSize();
    return;
  }

#if defined(SIMU)
  const uint32_t stride =
      lv_draw_buf_width_to_stride(scaledw, LV_COLOR_FORMAT_RGB565A8);
  const size_t lvglByteCount = stride * scaledh + (stride / 2) * scaledh;

  pixel_t* ndata = forceBitmapBufferResizeMallocFailure
                       ? nullptr
                       : (pixel_t*)malloc(align32(lvglByteCount));
#else
  const uint32_t stride =
      lv_draw_buf_width_to_stride(scaledw, LV_COLOR_FORMAT_RGB565A8);
  const size_t lvglByteCount = stride * scaledh + (stride / 2) * scaledh;
  pixel_t* ndata = (pixel_t*)malloc(align32(lvglByteCount));
#endif

  if (!ndata) {
    invalidateSize();
    return;
  }

  uint8_t* dst = (uint8_t*)ndata;
  uint8_t* alphaDst = dst + stride * scaledh;
  for (int i = 0; i < scaledh; i += 1) {
    pixel_t* src = &data[(coord_t)(i / scale) * width()];
    uint8_t* colorLine = dst + i * stride;
    uint8_t* alphaLine = alphaDst + i * (stride / 2);
    for (int j = 0; j < scaledw; j += 1) {
      ARGB_SPLIT(src[(coord_t)(j / scale)], a, r, g, b);
      auto c = RGB_JOIN(rbcnv[r], gcnv[g], rbcnv[b]);
      colorLine[j * 2] = c & 0xFF;
      colorLine[j * 2 + 1] = c >> 8;
      alphaLine[j] = alpha[a];
    }
  }

  free(data);
  data = ndata;
  _width = scaledw;
  _height = scaledh;
  xmax = scaledw;
  ymax = scaledh;
  data_end = data + ((lvglByteCount + 1) / 2);
}

#if defined(SIMU)
bool bitmapBufferCanvasCreateFailureKeepsDataForTest()
{
  bitmapBufferForceCanvasCreateFailureForTest(true);
  BitmapBuffer bitmap(BMP_RGB565, 8, 8);
  bitmapBufferForceCanvasCreateFailureForTest(false);

  return bitmap.getData() != nullptr && bitmap.width() == 8 &&
         bitmap.height() == 8;
}

bool bitmapBufferResizeAllocationFailurePreventsLvglOverreadForTest()
{
  BitmapBuffer bitmap(BMP_ARGB4444, 512, 512);
  if (!bitmap.getData()) return false;

  bitmapBufferForceResizeMallocFailureForTest(true);
  bitmap.resizeToLVGL(256, 256);
  bitmapBufferForceResizeMallocFailureForTest(false);

  if (bitmap.width() == 0 || bitmap.height() == 0) return true;

  auto stride =
      lv_draw_buf_width_to_stride(bitmap.width(), LV_COLOR_FORMAT_RGB565A8);
  auto lvglByteCount =
      stride * bitmap.height() + (stride / 2) * bitmap.height();
  volatile auto lastByte =
      reinterpret_cast<const uint8_t*>(bitmap.getData())[lvglByteCount - 1];
  (void)lastByte;
  return false;
}
#endif
