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

#pragma once

// BaseDialog lives in its own header so the design-system layer (ds_core.h)
// can build ds::Dialog / ds::PickerOverlay on top of it WITHOUT pulling in the
// concrete dialog subclasses (ConfirmDialog / MessageDialog / ...), which in
// turn are being migrated to inherit from ds::Dialog. Splitting the base out
// breaks what would otherwise be a circular include (ds_core.h -> dialog.h ->
// ds_core.h). See dialog.h.

#include "form.h"
#include "modal_window.h"

class StaticText;

//-----------------------------------------------------------------------------

#define DIALOG_DEFAULT_WIDTH ((coord_t)(LCD_W * 0.8))
#define DIALOG_DEFAULT_HEIGHT ((coord_t)(LCD_H * 0.8))

//-----------------------------------------------------------------------------

class BaseDialog : public ModalWindow
{
 public:
  BaseDialog(const char* title, bool closeIfClickedOutside,
             lv_coord_t width = DIALOG_DEFAULT_WIDTH,
             lv_coord_t maxHeight = DIALOG_DEFAULT_HEIGHT,
             bool flexLayout = true);

  void setTitle(const char* title);

 protected:
  RequiredWindow<Window> form;
  StaticText* header = nullptr;

  void onCancel() override { deleteLater(); }
  void onLiveEvent(LiveWindow& live, event_t event) override {}
};
