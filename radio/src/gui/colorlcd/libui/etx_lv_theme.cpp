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

/*********************
 *      INCLUDES
 *********************/
#include "etx_lv_theme.h"

#include "window.h"
#include "../colors.h"
#include "fonts.h"

#include <iterator>
#include <new>

extern lv_color_t makeLvColor(uint32_t colorFlags);

/**********************
 *   Filter Callbacks
 **********************/

static lv_color_t dark_color_filter_cb(const lv_color_filter_dsc_t* f,
                                       lv_color_t c, lv_opa_t opa)
{
  LV_UNUSED(f);
  return lv_color_darken(c, opa);
}

static lv_color_t grey_filter_cb(const lv_color_filter_dsc_t* f,
                                 lv_color_t color, lv_opa_t opa)
{
  LV_UNUSED(f);
  return lv_color_mix(lv_palette_lighten(LV_PALETTE_GREY, 2), color, opa);
}

static lv_color_t qm_disabled_filter_cb(const lv_color_filter_dsc_t* f,
                                 lv_color_t color, lv_opa_t opa)
{
  LV_UNUSED(f);
  return lv_color_mix(makeLvColor(COLOR_THEME_QM_FG_INDEX), color, opa);
}

static lv_color_filter_dsc_t dark_filter;
static lv_color_filter_dsc_t grey_filter;
static lv_color_filter_dsc_t qm_disabled_filter;

/**********************
 *   Variable Styles
 **********************/

EdgeTxStyles::EdgeTxStyles()
{
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1) {
    lv_style_init(&bg_color[i]);
    lv_style_init(&txt_color[i]);
    lv_style_init(&img_color[i]);
    lv_style_set_img_recolor_opa(&img_color[i], LV_OPA_COVER);
    lv_style_init(&border_color[i]);
    lv_style_init(&arc_color[i]);
    lv_style_init(&line_color[i]);
  }
  lv_style_init(&outline_color_light);
  lv_style_init(&outline_color_normal);
  lv_style_init(&outline_color_focus);
  lv_style_init(&outline_color_edit);
  lv_style_init(&graph_border);
  lv_style_init(&graph_dashed);
  lv_style_init(&graph_line);
  lv_style_init(&graph_position_line);
  lv_style_init(&div_line);
  lv_style_init(&div_line_edit);
  lv_style_init(&div_line_warn);
  lv_style_init(&div_line_black);
  lv_style_init(&div_line_white);

  lv_style_set_line_width(&graph_border, 1);
  lv_style_set_line_opa(&graph_border, LV_OPA_COVER);

  lv_style_set_line_width(&graph_dashed, 1);
  lv_style_set_line_opa(&graph_dashed, LV_OPA_COVER);
  lv_style_set_line_dash_width(&graph_dashed, 2);
  lv_style_set_line_dash_gap(&graph_dashed, 2);

  lv_style_set_line_width(&graph_line, 3);
  lv_style_set_line_opa(&graph_line, LV_OPA_COVER);
  lv_style_set_line_rounded(&graph_line, true);

  lv_style_set_line_width(&graph_position_line, 1);
  lv_style_set_line_opa(&graph_position_line, LV_OPA_COVER);

  lv_style_set_line_width(&div_line, 1);
  lv_style_set_line_opa(&div_line, LV_OPA_COVER);

  lv_style_set_line_width(&div_line_edit, 1);
  lv_style_set_line_opa(&div_line_edit, LV_OPA_COVER);

  lv_style_set_line_width(&div_line_warn, 1);
  lv_style_set_line_opa(&div_line_warn, LV_OPA_COVER);

  lv_style_set_line_width(&div_line_black, 1);
  lv_style_set_line_opa(&div_line_black, LV_OPA_COVER);

  lv_style_set_line_width(&div_line_white, 1);
  lv_style_set_line_opa(&div_line_white, LV_OPA_COVER);

  // Init styles that were previously const-initialized
  lv_style_init(&pad_zero);
  lv_style_set_pad_top(&pad_zero, 0);
  lv_style_set_pad_bottom(&pad_zero, 0);
  lv_style_set_pad_left(&pad_zero, 0);
  lv_style_set_pad_right(&pad_zero, 0);
  lv_style_set_pad_row(&pad_zero, 0);
  lv_style_set_pad_column(&pad_zero, 0);

  lv_style_init(&pad_tiny);
  lv_style_set_pad_top(&pad_tiny, PAD_TINY);
  lv_style_set_pad_bottom(&pad_tiny, PAD_TINY);
  lv_style_set_pad_left(&pad_tiny, PAD_TINY);
  lv_style_set_pad_right(&pad_tiny, PAD_TINY);
  lv_style_set_pad_row(&pad_tiny, PAD_TINY);
  lv_style_set_pad_column(&pad_tiny, PAD_TINY);

  lv_style_init(&pad_small);
  lv_style_set_pad_top(&pad_small, PAD_SMALL);
  lv_style_set_pad_bottom(&pad_small, PAD_SMALL);
  lv_style_set_pad_left(&pad_small, PAD_SMALL);
  lv_style_set_pad_right(&pad_small, PAD_SMALL);
  lv_style_set_pad_row(&pad_small, PAD_SMALL);
  lv_style_set_pad_column(&pad_small, PAD_SMALL);

  lv_style_init(&pad_medium);
  lv_style_set_pad_top(&pad_medium, PAD_MEDIUM);
  lv_style_set_pad_bottom(&pad_medium, PAD_MEDIUM);
  lv_style_set_pad_left(&pad_medium, PAD_MEDIUM);
  lv_style_set_pad_right(&pad_medium, PAD_MEDIUM);
  lv_style_set_pad_row(&pad_medium, PAD_SMALL);
  lv_style_set_pad_column(&pad_medium, PAD_SMALL);

  lv_style_init(&pad_large);
  lv_style_set_pad_top(&pad_large, PAD_LARGE);
  lv_style_set_pad_bottom(&pad_large, PAD_LARGE);
  lv_style_set_pad_left(&pad_large, PAD_LARGE);
  lv_style_set_pad_right(&pad_large, PAD_LARGE);
  lv_style_set_pad_row(&pad_large, PAD_SMALL);
  lv_style_set_pad_column(&pad_large, PAD_SMALL);

  lv_style_init(&pad_button);
  lv_style_set_pad_top(&pad_button, PAD_TINY);
  lv_style_set_pad_bottom(&pad_button, PAD_TINY);
  lv_style_set_pad_left(&pad_button, PAD_MEDIUM);
  lv_style_set_pad_right(&pad_button, PAD_MEDIUM);
  lv_style_set_pad_row(&pad_button, PAD_TINY);
  lv_style_set_pad_column(&pad_button, PAD_TINY);

  lv_style_init(&pad_textarea);
  lv_style_set_pad_top(&pad_textarea, PAD_SMALL);
  lv_style_set_pad_bottom(&pad_textarea, PAD_SMALL - 1);
  lv_style_set_pad_left(&pad_textarea, PAD_MEDIUM);
  lv_style_set_pad_right(&pad_textarea, PAD_SMALL);

  lv_style_init(&pad_left_2);
  lv_style_set_pad_left(&pad_left_2, 2);

  lv_style_init(&text_align_left);
  lv_style_set_text_align(&text_align_left, LV_TEXT_ALIGN_LEFT);

  lv_style_init(&text_align_right);
  lv_style_set_text_align(&text_align_right, LV_TEXT_ALIGN_RIGHT);

  lv_style_init(&text_align_center);
  lv_style_set_text_align(&text_align_center, LV_TEXT_ALIGN_CENTER);

  lv_style_init(&bg_opacity_transparent);
  lv_style_set_bg_opa(&bg_opacity_transparent, LV_OPA_TRANSP);

  lv_style_init(&bg_opacity_20);
  lv_style_set_bg_opa(&bg_opacity_20, LV_OPA_20);

  lv_style_init(&bg_opacity_50);
  lv_style_set_bg_opa(&bg_opacity_50, LV_OPA_50);

  lv_style_init(&bg_opacity_75);
  lv_style_set_bg_opa(&bg_opacity_75, 187);

  lv_style_init(&bg_opacity_90);
  lv_style_set_bg_opa(&bg_opacity_90, 230);

  lv_style_init(&bg_opacity_cover);
  lv_style_set_bg_opa(&bg_opacity_cover, LV_OPA_COVER);

  lv_style_init(&fg_opacity_transparent);
  lv_style_set_opa(&fg_opacity_transparent, LV_OPA_TRANSP);

  lv_style_init(&fg_opacity_cover);
  lv_style_set_opa(&fg_opacity_cover, LV_OPA_COVER);

  lv_style_init(&rounded);
  lv_style_set_radius(&rounded, 6);

  lv_style_init(&circle);
  lv_style_set_radius(&circle, LV_RADIUS_CIRCLE);

  lv_style_init(&scrollbar);
  lv_style_set_bg_opa(&scrollbar, LV_OPA_50);
  lv_style_set_pad_top(&scrollbar, PAD_SCROLL);
  lv_style_set_pad_bottom(&scrollbar, PAD_SCROLL);
  lv_style_set_pad_left(&scrollbar, PAD_SCROLL);
  lv_style_set_pad_right(&scrollbar, PAD_SCROLL);
  lv_style_set_width(&scrollbar, PAD_SMALL);

  lv_style_init(&border);
  lv_style_set_border_opa(&border, LV_OPA_COVER);
  lv_style_set_border_width(&border, PAD_BORDER);

  lv_style_init(&border_transparent);
  lv_style_set_border_opa(&border_transparent, LV_OPA_TRANSP);
  lv_style_set_border_width(&border_transparent, PAD_BORDER);

  lv_style_init(&border_thin);
  lv_style_set_border_opa(&border_thin, LV_OPA_COVER);
  lv_style_set_border_width(&border_thin, 1);

  lv_style_init(&outline);
  lv_style_set_outline_width(&outline, PAD_OUTLINE + 1);
  lv_style_set_outline_opa(&outline, LV_OPA_COVER);
  lv_style_set_outline_pad(&outline, PAD_TINY);

  lv_style_init(&state_focus_frame);
  lv_style_set_border_width(&state_focus_frame, PAD_BORDER + 2);
  lv_style_set_border_opa(&state_focus_frame, LV_OPA_COVER);

  lv_style_init(&state_edit_frame);
  lv_style_set_border_width(&state_edit_frame, PAD_BORDER + 2);
  lv_style_set_border_opa(&state_edit_frame, LV_OPA_COVER);

  // Init color filter dsc objects and filter styles
  lv_color_filter_dsc_init(&dark_filter, dark_color_filter_cb);
  lv_style_init(&pressed);
  lv_style_set_color_filter_dsc(&pressed, &dark_filter);
  lv_style_set_color_filter_opa(&pressed, 35);

  lv_color_filter_dsc_init(&grey_filter, grey_filter_cb);
  lv_style_init(&disabled);
  lv_style_set_color_filter_dsc(&disabled, &grey_filter);
  lv_style_set_color_filter_opa(&disabled, LV_OPA_50);

  lv_color_filter_dsc_init(&qm_disabled_filter, qm_disabled_filter_cb);
  lv_style_init(&qmdisabled);
  lv_style_set_color_filter_dsc(&qmdisabled, &qm_disabled_filter);
  lv_style_set_color_filter_opa(&qmdisabled, LV_OPA_60);

  // Fonts
  for (int i = FONT_STD_INDEX; i < FONTS_COUNT; i += 1) {
    lv_style_init(&font[i]);
  }

  applyColors();
}

void EdgeTxStyles::init()
{
  if (!initDone) {
    initDone = true;

    // Fonts
    for (int i = FONT_STD_INDEX; i < FONTS_COUNT; i += 1) {
      lv_style_set_text_font(&font[i], getFont(i << 8));
    }
  }

  applyColors();
}

void EdgeTxStyles::setFonts()
{
  // Fonts
  for (int i = FONT_STD_INDEX; i < FONTS_COUNT; i += 1) {
    lv_style_set_text_font(&font[i], getFont(i << 8));
  }
}

void EdgeTxStyles::applyColors()
{
  // Always update colors in case theme changes

  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1) {
    auto c = makeLvColor(COLOR(i));
    lv_style_set_bg_color(&bg_color[i], c);
    lv_style_set_text_color(&txt_color[i], c);
    lv_style_set_img_recolor(&img_color[i], c);
    lv_style_set_border_color(&border_color[i], c);
    lv_style_set_arc_color(&arc_color[i], c);
    lv_style_set_line_color(&line_color[i], c);
  }

  lv_style_set_line_color(&graph_border, makeLvColor(COLOR_THEME_SECONDARY2));
  lv_style_set_line_color(&graph_dashed, makeLvColor(COLOR_THEME_SECONDARY2));
  lv_style_set_line_color(&graph_line, makeLvColor(COLOR_THEME_SECONDARY1));
  lv_style_set_line_color(&graph_position_line,
                          makeLvColor(COLOR_THEME_ACTIVE));
  lv_style_set_line_color(&div_line, makeLvColor(COLOR_THEME_SECONDARY1));
  lv_style_set_line_color(&div_line_edit, makeLvColor(COLOR_THEME_EDIT));
  lv_style_set_line_color(&div_line_warn, makeLvColor(COLOR_THEME_WARNING));
  lv_style_set_line_color(&div_line_black, makeLvColor(COLOR_THEME_PRIMARY1));
  lv_style_set_line_color(&div_line_white, makeLvColor(COLOR_THEME_PRIMARY2));

  lv_style_set_outline_color(&outline_color_light,
                             makeLvColor(COLOR_THEME_SECONDARY3));
  lv_style_set_outline_color(&outline_color_normal,
                             makeLvColor(COLOR_THEME_SECONDARY2));
  lv_style_set_outline_color(&outline_color_focus,
                             makeLvColor(COLOR_THEME_PRIMARY1));
  lv_style_set_outline_color(&outline_color_edit,
                             makeLvColor(COLOR_THEME_EDIT));
}

static EdgeTxStyles* mainStyles = nullptr;
static EdgeTxStyles* previewStyles = nullptr;
EdgeTxStyles* styles = nullptr;

#if defined(ALL_LANGS)
void setAllFonts()
{
  if (mainStyles) mainStyles->setFonts();
  if (previewStyles) previewStyles->setFonts();
}
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void usePreviewStyle()
{
  if (!previewStyles) {
    previewStyles = new (std::nothrow) EdgeTxStyles();
    if (previewStyles) {
      previewStyles->init();
    }
  }
  if (previewStyles) {
    styles = previewStyles;
    styles->applyColors();
  }
}

void useMainStyle()
{
  if (!mainStyles) {
    mainStyles = new (std::nothrow) EdgeTxStyles();
    if (mainStyles) {
      mainStyles->init();
    }
  }
  if (mainStyles) {
    styles = mainStyles;
  }
}

/**********************
 *   Custom object creation
 **********************/

// Object constructor helpers

#if defined(SIMU)
static bool forceEtxLabelAllocationFailureForTest = false;
#endif

lv_obj_t* etx_label_create(lv_obj_t* parent, FontIndex fontIdx)
{
  lv_obj_t* lvobj =
#if defined(SIMU)
      forceEtxLabelAllocationFailureForTest ? nullptr :
#endif
      lv_label_create(parent);
  if (!lvobj) return nullptr;

  etx_font(lvobj, fontIdx);
  lv_obj_clear_flag(lvobj,
                    static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE |
                                               LV_OBJ_FLAG_SCROLL_ON_FOCUS |
                                               LV_OBJ_FLAG_SCROLL_ELASTIC));
  return lvobj;
}

lv_obj_t* etx_label_create(Window* parent, FontIndex fontIdx)
{
  if (!parent) return nullptr;
  lv_obj_t* label = nullptr;
  parent->withLive([&](Window::LiveWindow& live) {
    label = etx_label_create(live.lvobj(), fontIdx);
  });
  return label;
}

#if defined(SIMU)
bool etxLabelAllocationFailureReturnsNullForTest()
{
  forceEtxLabelAllocationFailureForTest = true;
  lv_obj_t* obj = etx_label_create((lv_obj_t*)nullptr);
  forceEtxLabelAllocationFailureForTest = false;
  return obj == nullptr;
}

void etx_std_settings(lv_obj_t* obj, lv_style_selector_t selector);

bool etxStyleHelpersIgnoreNullObjectForTest()
{
  etx_obj_add_style(nullptr, styles->rounded, LV_PART_MAIN);
  etx_font(nullptr, FONT_STD_INDEX);
  etx_solid_bg(nullptr);
  etx_bg_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_bg_color_from_flags(nullptr, COLOR_THEME_SECONDARY1);
  etx_txt_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_txt_color_from_flags(nullptr, COLOR_THEME_SECONDARY1);
  etx_border_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_arc_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_arc_color_from_flags(nullptr, COLOR_THEME_SECONDARY1);
  etx_line_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_line_color_from_flags(nullptr, COLOR_THEME_SECONDARY1);
  etx_img_color(nullptr, COLOR_THEME_SECONDARY1_INDEX);
  etx_std_ctrl_colors(nullptr);
  etx_keyboard_key_colors(nullptr);
  etx_std_settings(nullptr, LV_PART_MAIN);
  etx_btn_style(nullptr);
  etx_padding(nullptr, PAD_ZERO);
  etx_std_style(nullptr);
  etx_scrollbar(nullptr);
  return true;
}
#endif

void etx_solid_bg(lv_obj_t* obj, LcdColorIndex bg_color,
                  lv_style_selector_t selector)
{
  if (!obj) return;
  etx_bg_color(obj, bg_color, selector);
  etx_obj_add_style(obj, styles->bg_opacity_cover, selector);
}

void etx_font(lv_obj_t* obj, FontIndex fontIdx, lv_style_selector_t selector)
{
  if (!obj) return;

  const auto fontCount = static_cast<int>(std::size(styles->font));
  if (fontIdx < FONT_STD_INDEX || fontIdx >= fontCount) {
    fontIdx = FONT_STD_INDEX;
  }

  // Remove old style first
  for (int i = FONT_STD_INDEX; i < fontCount; i += 1)
    lv_obj_remove_style(obj, &styles->font[i], selector);
  etx_obj_add_style(obj, styles->font[fontIdx], selector);
}

void etx_remove_bg_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->bg_color[i], selector);
}

void etx_bg_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                  lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_bg_color(obj, selector);
  etx_obj_add_style(obj, styles->bg_color[colorIdx], selector);
}

void etx_bg_color_from_flags(lv_obj_t* obj, LcdFlags colorFlags,
                             lv_style_selector_t selector)
{
  if (!obj) return;

  if (colorFlags & RGB_FLAG) {
    etx_remove_bg_color(obj, selector);
    lv_obj_set_style_bg_color(obj,
                              makeLvColor(colorFlags), selector);
  } else {
    lv_obj_remove_local_style_prop(obj, LV_STYLE_BG_COLOR, selector);
    etx_bg_color(obj, (LcdColorIndex)COLOR_VAL(colorFlags), selector);
  }
}

void etx_remove_txt_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->txt_color[i], selector);
}

void etx_txt_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                   lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_txt_color(obj, selector);
  etx_obj_add_style(obj, styles->txt_color[colorIdx], selector);
}

void etx_txt_color_from_flags(lv_obj_t* obj, LcdFlags colorFlags,
                              lv_style_selector_t selector)
{
  if (!obj) return;

  if (colorFlags & RGB_FLAG) {
    etx_remove_txt_color(obj, selector);
    lv_obj_set_style_text_color(obj,
                                makeLvColor(colorFlags), selector);
  } else {
    lv_obj_remove_local_style_prop(obj, LV_STYLE_TEXT_COLOR, selector);
    etx_txt_color(obj, (LcdColorIndex)COLOR_VAL(colorFlags), selector);
  }
}

void etx_remove_border_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->border_color[i], selector);
}

void etx_border_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                   lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_border_color(obj, selector);
  etx_obj_add_style(obj, styles->border_color[colorIdx], selector);
}

void etx_remove_arc_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->arc_color[i], selector);
}

void etx_arc_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                  lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_arc_color(obj, selector);
  etx_obj_add_style(obj, styles->arc_color[colorIdx], selector);
}

void etx_arc_color_from_flags(lv_obj_t* obj, LcdFlags colorFlags,
                             lv_style_selector_t selector)
{
  if (!obj) return;

  if (colorFlags & RGB_FLAG) {
    etx_remove_arc_color(obj, selector);
    lv_obj_set_style_arc_color(obj,
                              makeLvColor(colorFlags), selector);
  } else {
    lv_obj_remove_local_style_prop(obj, LV_STYLE_ARC_COLOR, selector);
    etx_arc_color(obj, (LcdColorIndex)COLOR_VAL(colorFlags), selector);
  }
}

void etx_remove_line_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->line_color[i], selector);
}

void etx_line_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                  lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_line_color(obj, selector);
  etx_obj_add_style(obj, styles->line_color[colorIdx], selector);
}

void etx_line_color_from_flags(lv_obj_t* obj, LcdFlags colorFlags,
                             lv_style_selector_t selector)
{
  if (!obj) return;

  if (colorFlags & RGB_FLAG) {
    etx_remove_line_color(obj, selector);
    lv_obj_set_style_line_color(obj,
                              makeLvColor(colorFlags), selector);
  } else {
    lv_obj_remove_local_style_prop(obj, LV_STYLE_LINE_COLOR, selector);
    etx_line_color(obj, (LcdColorIndex)COLOR_VAL(colorFlags), selector);
  }
}

void etx_remove_img_color(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove styles
  for (int i = 0; i < TOTAL_COLOR_COUNT; i += 1)
    lv_obj_remove_style(obj, &styles->img_color[i], selector);
}

void etx_img_color(lv_obj_t* obj, LcdColorIndex colorIdx,
                   lv_style_selector_t selector)
{
  if (!obj) return;

  // Remove old style first
  etx_remove_img_color(obj, selector);
  etx_obj_add_style(obj, styles->img_color[colorIdx], selector);
}

void etx_std_ctrl_colors(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX, selector);
  etx_txt_color(obj, COLOR_THEME_SECONDARY1_INDEX, selector);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX, selector | LV_STATE_FOCUSED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, selector | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY1_INDEX],
                    selector | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->state_focus_frame,
                    selector | LV_STATE_FOCUSED);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX, selector | LV_STATE_EDITED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, selector | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY1_INDEX],
                    selector | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->state_edit_frame,
                    selector | LV_STATE_EDITED);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX,
               selector | LV_STATE_FOCUSED | LV_STATE_EDITED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                selector | LV_STATE_FOCUSED | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY1_INDEX],
                    selector | LV_STATE_FOCUSED | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->state_edit_frame,
                    selector | LV_STATE_FOCUSED | LV_STATE_EDITED);

  etx_bg_color(obj, COLOR_THEME_ACTIVE_INDEX, selector | LV_STATE_CHECKED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, selector | LV_STATE_CHECKED);
}

void etx_keyboard_key_colors(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  etx_std_style(obj, selector, PAD_SMALL);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, selector);

  etx_solid_bg(obj, COLOR_THEME_SECONDARY3_INDEX,
               selector | LV_STATE_CHECKED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                selector | LV_STATE_CHECKED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_SECONDARY2_INDEX],
                    selector | LV_STATE_CHECKED);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY1_INDEX,
               selector | LV_STATE_FOCUSED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX,
                selector | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY2_INDEX],
                    selector | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->state_edit_frame,
                    selector | LV_STATE_FOCUSED);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY1_INDEX,
               selector | LV_STATE_CHECKED | LV_STATE_FOCUSED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX,
                selector | LV_STATE_CHECKED | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY2_INDEX],
                    selector | LV_STATE_CHECKED | LV_STATE_FOCUSED);
  etx_obj_add_style(obj, styles->state_edit_frame,
                    selector | LV_STATE_CHECKED | LV_STATE_FOCUSED);

  etx_bg_color(obj, COLOR_THEME_PRIMARY2_INDEX, selector | LV_STATE_EDITED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, selector | LV_STATE_EDITED);
}

void etx_std_settings(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  etx_obj_add_style(obj, styles->border, selector);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_SECONDARY2_INDEX], selector);
  etx_obj_add_style(obj, styles->rounded, selector);

  etx_obj_add_style(obj, styles->disabled, selector | LV_STATE_DISABLED);
  etx_obj_add_style(obj, styles->pressed, selector | LV_STATE_PRESSED);
}

void etx_btn_style(lv_obj_t* obj, lv_style_selector_t selector)
{
  if (!obj) return;

  etx_std_settings(obj, selector);
  etx_std_ctrl_colors(obj, selector);
  etx_obj_add_style(obj, styles->pad_button, selector);
}

void etx_padding(lv_obj_t* obj, PaddingSize padding,
                 lv_style_selector_t selector)
{
  if (!obj) return;

  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_tiny, selector);
  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_small, selector);
  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_medium, selector);
  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_large, selector);
  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_zero, selector);
  lv_obj_remove_style(obj, (lv_style_t*)&styles->pad_button, selector);
  switch (padding) {
    case PAD_TINY:
      etx_obj_add_style(obj, styles->pad_tiny, selector);
      break;
    case PAD_SMALL:
      etx_obj_add_style(obj, styles->pad_small, selector);
      break;
    case PAD_MEDIUM:
      etx_obj_add_style(obj, styles->pad_medium, selector);
      break;
    case PAD_LARGE:
      etx_obj_add_style(obj, styles->pad_large, selector);
      break;
    default:
      etx_obj_add_style(obj, styles->pad_zero, selector);
      break;
  };
}

void etx_std_style(lv_obj_t* obj, lv_style_selector_t selector,
                   PaddingSize padding)
{
  if (!obj) return;

  etx_std_settings(obj, selector);
  etx_std_ctrl_colors(obj, selector);
  etx_padding(obj, padding, selector);
}

void etx_scrollbar(lv_obj_t* obj)
{
  if (!obj) return;

  etx_obj_add_style(obj, styles->scrollbar, LV_PART_SCROLLBAR);
  etx_obj_add_style(obj, styles->bg_color[COLOR_GREY_INDEX], LV_PART_SCROLLBAR);
  etx_obj_add_style(obj, styles->bg_opacity_cover,
                    LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
}

// Object creators

#if defined(SIMU)
static bool forceEtxObjectAllocationFailureForTest = false;

void etxCreateForceObjectAllocationFailureForTest(bool force)
{
  forceEtxObjectAllocationFailureForTest = force;
}
#endif

lv_obj_t* etx_create(const lv_obj_class_t* class_p, lv_obj_t* parent)
{
  lv_obj_t* obj =
#if defined(SIMU)
      forceEtxObjectAllocationFailureForTest ? nullptr :
#endif
      lv_obj_class_create_obj(class_p, parent);
  if (!obj) return nullptr;

  lv_obj_class_init_obj(obj);

  return obj;
}

#if defined(SIMU)
bool etxCreateObjectAllocationFailureReturnsNullForTest()
{
  etxCreateForceObjectAllocationFailureForTest(true);
  lv_obj_t* obj = etx_create(&lv_obj_class, nullptr);
  etxCreateForceObjectAllocationFailureForTest(false);
  return obj == nullptr;
}
#endif

static void textarea_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
  etx_obj_add_style(obj, styles->border, LV_PART_MAIN);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_SECONDARY2_INDEX], LV_PART_MAIN);
  etx_obj_add_style(obj, styles->rounded, LV_PART_MAIN);
  etx_std_ctrl_colors(obj, LV_PART_MAIN);
  etx_obj_add_style(obj, styles->pad_textarea, LV_PART_MAIN);

  etx_bg_color(obj, COLOR_THEME_PRIMARY2_INDEX, LV_PART_MAIN | LV_STATE_EDITED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                LV_PART_MAIN | LV_STATE_EDITED);

  etx_bg_color(obj, COLOR_THEME_PRIMARY1_INDEX,
               LV_PART_CURSOR | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->bg_opacity_cover,
                    LV_PART_CURSOR | LV_STATE_EDITED);

  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  lv_textarea_set_password_mode(obj, false);
  lv_textarea_set_one_line(obj, true);

  lv_textarea_t* ta = (lv_textarea_t*)obj;
  lv_obj_set_height(ta->label, EdgeTxStyles::UI_ELEMENT_HEIGHT * 21 / 32);
}

static const lv_obj_class_t textarea_class = {
    .base_class = &lv_textarea_class,
    .constructor_cb = textarea_constructor,
    .destructor_cb = nullptr,
    .event_cb = nullptr,
    .user_data = nullptr,
    .width_def = 0,
    .height_def = EdgeTxStyles::UI_ELEMENT_HEIGHT,
    .editable = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_INHERIT,
    .instance_size = sizeof(lv_textarea_t),
};

lv_obj_t* etx_textarea_create(lv_obj_t* parent)
{
  return etx_create(&textarea_class, parent);
}
