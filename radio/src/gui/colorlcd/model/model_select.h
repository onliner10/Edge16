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

#pragma once

#include "listbox.h"
#include "storage/modelslist.h"
#include "pagegroup.h"
#include "page.h"
#include "etx_lv_theme.h"

class ModelsPageBody;
class ModelLayoutButton;

class ModelLabelsWindow : public Page
{
 public:
  ModelLabelsWindow();

  static LAYOUT_VAL_SCALED(NEW_BTN_W, 60) // ds-allow: model-select header 'New' button width for absolute header placement; two-panel browser geometry, not a DS list metric
  static constexpr coord_t LAYOUT_BTN_XO = NEW_BTN_W + PAD_LARGE * 2 + EdgeTxStyles::UI_ELEMENT_HEIGHT; // ds-allow: model-select layout-cycle button x-offset for absolute header placement; two-panel browser geometry, not a DS list metric
  static LAYOUT_VAL_SCALED(LAYOUT_BTN_YO, 6) // ds-allow: model-select layout-cycle button y-offset for absolute header placement; two-panel browser geometry, not a DS list metric
  static constexpr coord_t MDLS_Y = PAD_BORDER; // ds-allow: models-grid panel y origin in the two-panel browser viewport; absolute panel geometry, not a DS list metric
  static constexpr coord_t LABELS_X = PAD_BORDER; // ds-allow: labels panel x origin in the two-panel browser viewport; absolute panel geometry, not a DS list metric
  static LAYOUT_ORIENTATION(MDLS_H, LCD_H - EdgeTxStyles::MENU_HEADER_HEIGHT - PAD_SMALL * 2, LAYOUT_SCALE(219)) // ds-allow: models-grid panel height sized to the viewport minus header; two-panel browser geometry, not a DS list metric
  static LAYOUT_ORIENTATION(LABELS_Y, PAD_SMALL, MDLS_Y + MDLS_H + PAD_SMALL) // ds-allow: labels panel y origin in the two-panel browser viewport; absolute panel geometry, not a DS list metric
  static LAYOUT_ORIENTATION(LABELS_WIDTH, LCD_W * 131 / 480, LCD_W - PAD_SMALL * 2) // ds-allow: labels panel width as a viewport fraction in the two-panel browser; absolute panel geometry, not a DS list metric
  static LAYOUT_ORIENTATION(LABELS_HEIGHT, LCD_H - EdgeTxStyles::MENU_HEADER_HEIGHT - EdgeTxStyles::UI_ELEMENT_HEIGHT - PAD_SMALL * 2 - PAD_MEDIUM, LAYOUT_SCALE(166)) // ds-allow: labels panel height sized to the viewport minus header/sort row; two-panel browser geometry, not a DS list metric
  static LAYOUT_ORIENTATION(SORT_BUTTON_W, LABELS_WIDTH, LAYOUT_SCALE(120)) // ds-allow: sort button width matched to the labels panel width; two-panel browser geometry, not a DS list metric
  static LAYOUT_ORIENTATION(MDLS_X, LABELS_WIDTH + PAD_BORDER, PAD_SMALL) // ds-allow: models-grid panel x origin placed right of the labels panel; absolute panel geometry, not a DS list metric
  static LAYOUT_ORIENTATION(MDLS_W, LCD_W - LABELS_WIDTH - PAD_BORDER, LCD_W - PAD_MEDIUM) // ds-allow: models-grid panel width filling the viewport beside the labels panel; absolute panel geometry, not a DS list metric

 protected:
  ModelsSortBy sort = DEFAULT_MODEL_SORT;
  char tmpLabel[LABEL_LENGTH + 1] = "\0";
  ListBox *lblselector;
  ModelsPageBody *mdlselector;
  ModelLayoutButton *mdlLayout;
  std::string currentLabel;

  LabelsVector getLabels()
  {
    auto labels = modelslabels.getLabels();
    if (modelslabels.getUnlabeledModels().size() > 0)
      labels.emplace_back(STR_UNLABELEDMODEL);
    return labels;
  }

  void newModel();
  void newLabel();
  void buildHead(Window *window);
  void buildBody(Window *window);
  void updateFilteredLabels(std::set<uint32_t> selected, bool setdirty = true);
  void labelRefreshRequest();
  void setTitle();

  void moveLabel(int selected, int direction);

#if defined(HARDWARE_KEYS)
  // Long MDL is intentionally disabled on the model select screen.
  void onLongPressMDL() override {}
  void onPressPG(bool isNext);
  void onPressPGUP() override;
  void onPressPGDN() override;
#endif
};
