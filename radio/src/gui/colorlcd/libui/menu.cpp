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

#include "menu.h"

#include <new>

#include "lvgl/src/widgets/table/lv_table_private.h"
#include "lvgl/src/core/lv_obj_private.h"
#include "lvgl/src/core/lv_obj_class_private.h"

#include "bitmaps.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "keyboard_base.h"
#include "mainwindow.h"
#include "menutoolbar.h"
#include "static.h"
#include "table.h"

#if defined(SIMU)
static bool forceMenuIconCanvasCreateFailure = false;

void menuForceIconCanvasCreateFailureForTest(bool force)
{
  forceMenuIconCanvasCreateFailure = force;
}
#endif

static lv_obj_t* createMenuIconCanvas(lv_obj_t* parent)
{
#if defined(SIMU)
  if (forceMenuIconCanvasCreateFailure) return nullptr;
#endif
  return lv_canvas_create(parent);
}

//-----------------------------------------------------------------------------

class MenuBody : public TableField
{
  enum MENU_DIRECTION { DIRECTION_UP = 1, DIRECTION_DOWN = -1 };

  class MenuLine
  {
    friend class MenuBody;

   public:
    MenuLine(const std::string& text, std::function<void()> onPress,
             std::function<bool()> isChecked, lv_obj_t* icon) :
        text(text),
        onPress(std::move(onPress)),
        isChecked(std::move(isChecked)),
        icon(icon)
    {
    }

    ~MenuLine()
    {
      if (icon) lv_obj_del(icon);
    }

    lv_obj_t* getIcon() const { return icon; }

   protected:
    std::string text;
    std::function<void()> onPress;
    std::function<bool()> isChecked;
    lv_obj_t* icon;
  };

 public:
  MenuBody(Window* parent, const rect_t& rect) : TableField(parent, rect)
  {
    // Allow encoder acceleration
    addFlag(LV_OBJ_FLAG_ENCODER_ACCEL);

    setColumnWidth(0, rect.w);

    setAutoEdit();

    setLongPressHandler([=]() { getParentMenu()->handleLongPress(); });
  }

  ~MenuBody() { clearLines(); }

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "MenuBody"; }
#endif

  void setIndex(int index)
  {
    if (index >= (int)lines.size()) return;
    if (index == selectedIndex) return;
    selectedIndex = index;

    withLive([&](LiveWindow& live) {
      auto obj = live.lvobj();
      lv_obj_invalidate(obj);
      lv_table_t* table = (lv_table_t*)obj;

      if (index < 0) {
        table->row_act = LV_TABLE_CELL_NONE;
        table->col_act = LV_TABLE_CELL_NONE;
        return;
      }

      table->row_act = index;
      table->col_act = 0;

      lv_coord_t h_before = 0;
      for (uint16_t i = 0; i < table->row_act; i++) h_before += table->row_h[i];

      lv_coord_t row_h = table->row_h[table->row_act];
      lv_coord_t scroll_y = lv_obj_get_scroll_y(obj);

      lv_obj_update_layout(obj);
      lv_coord_t h = lv_obj_get_height(obj);

      lv_coord_t diff_y = 0;
      if (h_before < scroll_y) {
        diff_y = scroll_y - h_before;
      } else if (scroll_y + h < h_before + row_h) {
        diff_y = scroll_y + h - h_before - row_h;
      } else {
        return;
      }

      lv_obj_scroll_by_bounded(obj, 0, diff_y, LV_ANIM_OFF);
    });
  }

  int selection() const { return selectedIndex; }

  int count() const { return lines.size(); }

  void addLine(const MaskBitmap* icon_mask, const std::string& text,
               std::function<void()> onPress, std::function<bool()> isChecked,
               bool update = true)
  {
    lv_obj_t* canvas = nullptr;
    if (icon_mask) {
      canvas = createMenuIconCanvas(nullptr);
      if (canvas) {
        lv_coord_t w = icon_mask->width;
        lv_coord_t h = icon_mask->height;
        lv_canvas_set_buffer(canvas, (void*)&icon_mask->data[0], w, h,
                             LV_COLOR_FORMAT_A8);
      }
    }

    auto l = new (std::nothrow) MenuLine(text, onPress, isChecked, canvas);
    if (!l) return;
    lines.push_back(l);

    if (update) {
      auto idx = lines.size() - 1;
      withLive([&](LiveWindow& live) {
        lv_table_set_cell_value(live.lvobj(), idx, 0, text.c_str());
      });
    }
  }

  void updateLines()
  {
    setRowCount(lines.size());
    for (unsigned int idx = 0; idx < lines.size(); idx++) {
      withLive([&](LiveWindow& live) {
        lv_table_set_cell_value(live.lvobj(), idx, 0,
                                lines[idx]->text.c_str());
      });
    }
  }

  void clearLines()
  {
    for (auto itr = lines.begin(); itr != lines.end();) {
      auto l = *itr;
      itr = lines.erase(itr);
      delete l;
    }
    lines.clear();
  }

  void removeLines()
  {
    clearLines();
    setRowCount(0);
    selectedIndex = 0;

    // reset vertical scroll
    withLive([](LiveWindow& live) {
      lv_obj_scroll_to_y(live.lvobj(), 0, LV_ANIM_OFF);
    });
  }

  void onPress(uint16_t row, uint16_t col) override
  {
    Menu* menu = getParentMenu();
    if (row < lines.size()) {
      if (menu->isMultiple()) {
        if (selectedIndex == (int)row)
          lines[row]->onPress();
        else {
          setIndex(row);
          lines[row]->onPress();
        }
      } else {
        auto onPress = lines[row]->onPress;
        // delete menu first to avoid
        // focus issues with onPress()
        menu->deleteLater();
        if (onPress) onPress();
      }
    }
  }

  void onDrawBegin(uint16_t row, uint16_t col,
                   lv_area_t* cell_area,
                   lv_layer_t* layer) override
  {
    // onDrawBegin was used to modify pre-draw state (dsc->label_dsc->ofs_x).
    // In LVGL9, pre-draw modification is no longer possible.
    // The icon will be drawn in onDrawEnd instead.
  }

  void onDrawEnd(uint16_t row, uint16_t col,
                 lv_area_t* cell_area,
                 lv_layer_t* layer) override
  {
    if (row >= lines.size()) return;

    lv_obj_t* icon = lines[row]->getIcon();
    if (icon) {
      lv_draw_image_dsc_t img_dsc;
      lv_draw_image_dsc_init(&img_dsc);

      lv_img_dsc_t* img = lv_canvas_get_image(icon);
      lv_area_t coords;

      lv_coord_t area_h = lv_area_get_height(cell_area);

      lv_coord_t cell_left = 0;
      withLive([&](LiveWindow& live) {
        cell_left = lv_obj_get_style_pad_left(live.lvobj(), LV_PART_ITEMS);
      });
      coords.x1 = cell_area->x1 + cell_left;
      coords.x2 = coords.x1 + img->header.w - 1;
      coords.y1 = cell_area->y1 + (area_h - img->header.h) / 2;
      coords.y2 = coords.y1 + img->header.h - 1;

      img_dsc.src = img;
      lv_draw_image(layer, &img_dsc, &coords);
    }

    if (lines[row]->isChecked != nullptr && lines[row]->isChecked()) {
      lv_area_t coords;
      lv_coord_t area_h = lv_area_get_height(cell_area);
      lv_coord_t cell_right = 0;
      withLive([&](LiveWindow& live) {
        cell_right = lv_obj_get_style_pad_right(live.lvobj(), LV_PART_ITEMS);
      });
      lv_coord_t font_h = getFontHeight(FONT(STD));
      coords.x1 = cell_area->x2 - cell_right - font_h;
      coords.x2 = coords.x1 + font_h;
      coords.y1 = cell_area->y1 + (area_h - font_h) / 2;
      coords.y2 = coords.y1 + font_h - 1;

      // Create local label descriptor (replaces dsc->label_dsc)
      lv_draw_label_dsc_t label_dsc;
      lv_draw_label_dsc_init(&label_dsc);
      withLive([&](LiveWindow& live) {
        label_dsc.color = lv_obj_get_style_text_color(live.lvobj(), LV_PART_ITEMS);
        label_dsc.font = lv_obj_get_style_text_font(live.lvobj(), LV_PART_ITEMS);
      });
      label_dsc.text = LV_SYMBOL_OK;
      lv_draw_label(layer, &label_dsc, &coords);
    }
  }

  void onSelected(uint16_t row, uint16_t col) override { selectedIndex = row; }

 protected:
  std::vector<MenuLine*> lines;
  int selectedIndex = 0;

  Menu* getParentMenu() { return static_cast<Menu*>(getParent()->getParent()); }
};

#if defined(SIMU)
void etxCreateForceObjectAllocationFailureForTest(bool force);

bool menuBodyObjectAllocationFailureFailsClosedForTest()
{
  class TestMenuBody : public MenuBody
  {
   public:
    explicit TestMenuBody(Window* parent) : MenuBody(parent, {0, 0, 120, 80}) {}
  };

  etxCreateForceObjectAllocationFailureForTest(true);
  auto body = new (std::nothrow) TestMenuBody(MainWindow::instance());
  etxCreateForceObjectAllocationFailureForTest(false);

  if (!body) return false;

  bool ok = !body->isAvailable() && !body->isVisible() &&
            !body->automationClickable();
  delete body;
  return ok;
}
#endif

//-----------------------------------------------------------------------------

static void menu_content_constructor(const lv_obj_class_t* class_p,
                                     lv_obj_t* obj)
{
  etx_solid_bg(obj);
  etx_obj_add_style(obj, styles->outline, LV_PART_MAIN);
  etx_obj_add_style(obj, styles->outline_color_normal, LV_PART_MAIN);
}

static const lv_obj_class_t menu_content_class = {
    .base_class = &window_base_class,
    .constructor_cb = menu_content_constructor,
    .destructor_cb = nullptr,
    .event_cb = nullptr,
    .user_data = nullptr,
    .width_def = 0,
    .height_def = 0,
    .editable = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .group_def = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .instance_size = sizeof(lv_obj_t)};

static lv_obj_t* menu_content_create(lv_obj_t* parent)
{
  return etx_create(&menu_content_class, parent);
}

class MenuContent
{
 public:
  virtual ~MenuContent() = default;

  virtual void setTitle(std::string text) {}
  virtual void addLine(const MaskBitmap* icon_mask, const std::string& text,
                       std::function<void()> onPress,
                       std::function<bool()> isChecked, bool update)
  {
  }
  virtual void updateLines() {}
  virtual void removeLines() {}
  virtual unsigned count() const { return 0; }
  virtual int selection() const { return -1; }
  virtual void setIndex(int index) {}
  virtual void updatePosition(MenuToolbar* toolbar) {}
};

class MissingMenuContent final : public MenuContent
{
};

static MissingMenuContent missingMenuContent;

class MenuWindowContent : public NavWindow, public MenuContent
{
 public:
  explicit MenuWindowContent(Menu* parent, coord_t popupWidth) :
      NavWindow(parent, rect_t{}, menu_content_create)
  {
    setWindowFlag(OPAQUE);

    coord_t w = (popupWidth > 0) ? popupWidth : MENUS_WIDTH;

    withLive([](LiveWindow& live) { lv_obj_center(live.lvobj()); });
    setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, w, LV_SIZE_CONTENT);

    if (!initRequiredWindow(header, this, rect_t{0, 0, LV_PCT(100), 0}, "",
                            COLOR_THEME_PRIMARY2_INDEX))
      return;
    header->withLive([](Window::LiveWindow& liveHeader) {
      etx_solid_bg(liveHeader.lvobj(), COLOR_THEME_SECONDARY1_INDEX);
    });
    header->padAll(PAD_SMALL);
    header->hide();

    if (!initRequiredWindow(body, this, rect_t{0, 0, w, LV_SIZE_CONTENT}))
      return;
    body->withLive([](Window::LiveWindow& liveBody) {
      lv_obj_set_style_max_height(liveBody.lvobj(), LCD_H * 0.8, LV_PART_MAIN);
    });
  }

  void setTitle(std::string text) override
  {
    header->setText(text);
    header->show();
  }

  void onLiveClicked(LiveWindow&) override { Keyboard::hide(false); }

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "MenuWindowContent"; }
#endif

  void updateLines() override { body->updateLines(); }
  void removeLines() override { body->removeLines(); }
  unsigned count() const override { return body->count(); }
  int selection() const override { return body->selection(); }
  void setIndex(int index) override { body->setIndex(index); }

  void addLine(const MaskBitmap* icon_mask, const std::string& text,
               std::function<void()> onPress, std::function<bool()> isChecked,
               bool update) override
  {
    body->addLine(icon_mask, text, std::move(onPress), std::move(isChecked),
                  update);
  }

  void updatePosition(MenuToolbar* toolbar) override
  {
    if (!toolbar) return;
    withLive([&](Window::LiveWindow& liveContent) {
      toolbar->withLive([&](Window::LiveWindow& liveToolbar) {
        auto contentObj = liveContent.lvobj();
        auto toolbarObj = liveToolbar.lvobj();
        coord_t cw = lv_obj_get_width(contentObj);
        coord_t ch = lv_obj_get_height(contentObj);
        coord_t tw = lv_obj_get_width(toolbarObj);
        coord_t th = lv_obj_get_height(toolbarObj);

        lv_obj_align(toolbarObj, LV_ALIGN_CENTER, -cw / 2, 0);
        lv_obj_align(contentObj, LV_ALIGN_CENTER, tw / 2, 0);

        toolbar->setHeight(max(ch, th));
        setHeight(max(ch, th));
      });
    });
  }

#if defined(HARDWARE_KEYS)
  void onPressPGUP() override
  {
    Messaging::send(Messaging::MENU_CHANGE_FILTER, -1);
  }

  void onPressPGDN() override
  {
    Messaging::send(Messaging::MENU_CHANGE_FILTER, 1);
  }
#endif

 static LAYOUT_VAL_SCALED(MENUS_WIDTH, 200)

     protected : StaticText* header = nullptr;
  MenuBody* body = nullptr;
};

#if defined(SIMU)
bool menuWindowContentObjectAllocationFailureFailsClosedForTest()
{
  etxCreateForceObjectAllocationFailureForTest(true);
  auto menu = new (std::nothrow) Menu();
  etxCreateForceObjectAllocationFailureForTest(false);

  if (!menu) return false;
  menu->setTitle("Title");
  menu->addLine(nullptr, "A", []() {}, nullptr);
  menu->updateLines();
  menu->removeLines();
  menu->select(0);

  bool ok = !menu->isAvailable() && !menu->isVisible() && menu->count() == 0 &&
            menu->selection() == -1;
  delete menu;
  return ok;
}
#endif

//-----------------------------------------------------------------------------

MenuContent& Menu::createContent(coord_t popupWidth)
{
  auto liveContent = Window::makeLive<MenuWindowContent>(this, popupWidth);
  if (liveContent) return *liveContent;
  failClosed();
  return missingMenuContent;
}

Menu::Menu(bool multiple, coord_t popupWidth) :
    ModalWindow(true), multiple(multiple), content(createContent(popupWidth))
{
}

void Menu::setToolbar(MenuToolbar* window)
{
  toolbar = window;
  updatePosition();
}

void Menu::updatePosition() { content.updatePosition(toolbar); }

void Menu::setTitle(std::string text)
{
  content.setTitle(std::move(text));
  updatePosition();
}

void Menu::addLine(const MaskBitmap* icon_mask, const std::string& text,
                   std::function<void()> onPress,
                   std::function<bool()> isChecked)
{
  content.addLine(icon_mask, text, std::move(onPress), std::move(isChecked),
                  true);
  updatePosition();
}

void Menu::addLine(const std::string& text, std::function<void()> onPress,
                   std::function<bool()> isChecked)
{
  addLine(nullptr, text, onPress, isChecked);
}

void Menu::addLineBuffered(const std::string& text,
                           std::function<void()> onPress,
                           std::function<bool()> isChecked)
{
  content.addLine(nullptr, text, std::move(onPress), std::move(isChecked),
                  false);
}

void Menu::updateLines()
{
  content.updateLines();
  updatePosition();
}

void Menu::removeLines()
{
  content.removeLines();
  updatePosition();
}

void Menu::onCancel()
{
  if (cancelHandler) cancelHandler();
  deleteLater();
}

void Menu::setCancelHandler(std::function<void()> handler)
{
  cancelHandler = std::move(handler);
}

void Menu::setWaitHandler(std::function<void()> handler)
{
  waitHandler = std::move(handler);
}

void Menu::setLongPressHandler(std::function<void()> handler)
{
  longPressHandler = std::move(handler);
}

void Menu::handleLongPress()
{
  if (longPressHandler) longPressHandler();
}

unsigned Menu::count() const { return content.count(); }

int Menu::selection() const { return content.selection(); }

void Menu::select(int index) { content.setIndex(index); }

void Menu::onLiveCheckEvents(Window::LiveWindow& live)
{
  ModalWindow::onLiveCheckEvents(live);
  if (waitHandler) waitHandler();
}

#if defined(SIMU)
bool menuIconCanvasCreateFailureStillAddsLineForTest()
{
  auto menu = new Menu();
  menuForceIconCanvasCreateFailureForTest(true);
  menu->addLine(getBuiltinIcon(ICON_BTN_NEXT), "Next", []() {});
  menuForceIconCanvasCreateFailureForTest(false);

  return menu->count() == 1;
}
#endif
