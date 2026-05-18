/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
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

#include "lcd.h"

#include <lvgl/lvgl.h>

#if LV_USE_GPU_STM32_DMA2D
#include <lvgl/src/draw/stm32_dma2d/lv_gpu_stm32_dma2d.h>
#else
#include "dma2d.h"
#endif

#include "bitmapbuffer.h"
#include "board.h"
#include "etx_lv_theme.h"

#include <cstring>

pixel_t LCD_FIRST_FRAME_BUFFER[DISPLAY_BUFFER_SIZE] __SDRAM;
pixel_t LCD_SECOND_FRAME_BUFFER[DISPLAY_BUFFER_SIZE] __SDRAM;

BitmapBuffer lcdBuffer1(BMP_RGB565, LCD_W, LCD_H,
                        (uint16_t*)LCD_FIRST_FRAME_BUFFER);
BitmapBuffer lcdBuffer2(BMP_RGB565, LCD_W, LCD_H,
                        (uint16_t*)LCD_SECOND_FRAME_BUFFER);

BitmapBuffer* lcdFront = &lcdBuffer1;
BitmapBuffer* lcd = &lcdBuffer2;

// v9 flush callback takes lv_display_t* + uint8_t*
static void (*lcd_flush_cb)(lv_display_t*, uint16_t* buffer,
                            const rect_t& area) = nullptr;

void lcdSetFlushCb(void (*cb)(lv_display_t*, uint16_t*, const rect_t&))
{
  lcd_flush_cb = cb;
}

static void flushLcd(lv_display_t* disp_drv, const lv_area_t* area,
                     uint8_t* color_p)
{
  (void)disp_drv;

  if (lcd_flush_cb) {
    rect_t copy_area = {area->x1, area->y1, area->x2 - area->x1 + 1,
                        area->y2 - area->y1 + 1};

    auto* pixels = reinterpret_cast<uint16_t*>(static_cast<void*>(color_p));
    lcd_flush_cb(disp_drv, pixels, copy_area);
  }

}

static void clear_frame_buffers()
{
  memset(LCD_FIRST_FRAME_BUFFER, 0, sizeof(LCD_FIRST_FRAME_BUFFER));
  memset(LCD_SECOND_FRAME_BUFFER, 0, sizeof(LCD_SECOND_FRAME_BUFFER));
}

void lcdInitDisplayDriver()
{
  static bool lcdDriverStarted = false;
  // we already have a display: exit
  if (lcdDriverStarted) return;
  lcdDriverStarted = true;

#if LV_USE_GPU_STM32_DMA2D
  // Init only DMA2D
  lv_draw_stm32_dma2d_init();
#else
  DMAInit();
#endif

  // Clear buffers first
  clear_frame_buffers();
  lcdSetInitalFrameBuffer(lcdFront->getData());

  // Init hardware LCD driver
  lcdInit();
  backlightInit();

}

void lcdClear() { lcd->clear(); }

// Direct drawing - used by boot loader
// In v9, direct frame-buffer access is done through the BitmapBuffer API.
// No LVGL draw_ctx is needed since we can write directly to the buffer
// and then trigger a refresh.
void lcdInitDirectDrawing()
{
  // BitmapBuffer already points to the correct draw buffer
  // (LCD_SECOND_FRAME_BUFFER via &lcdBuffer2).
  // No LVGL internal draw context setup needed in v9.
  lcd->setData((pixel_t*)lcd->getData());
  lcd->reset();
}

void lcdRefresh()
{
  lv_area_t full_screen = {0, 0, LCD_W - 1, LCD_H - 1};
  flushLcd(nullptr, &full_screen, reinterpret_cast<uint8_t*>(lcd->getData()));
}
