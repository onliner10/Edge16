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

#include "radio_theme.h"

#include "color_editor.h"
#include "color_list.h"
#include "dialog.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "file_carosell.h"
#include "menu.h"
#include "page.h"
#include "preview_window.h"
#include "textedit.h"

class ThemeColorPreview : public Window
{
 public:
  ThemeColorPreview(Window *parent, const rect_t &rect,
                    std::vector<ColorEntry> colorList) :
      Window(parent, rect), colorList(colorList)
  {
    setWindowFlag(NO_FOCUS);

    padAll(PAD_ZERO);  // ds-allow: theme editor — zero padding on the color-swatch strip so swatches abut; preview pane, not a DS list/form.
#if LANDSCAPE
    setFlexLayout(LV_FLEX_FLOW_COLUMN, BOX_MARGIN);
#else
    setFlexLayout(LV_FLEX_FLOW_ROW, BOX_MARGIN);
#endif
    build();
  }

  void build()
  {
    clear();
    setBoxWidth();
    int size = (boxWidth + BOX_MARGIN) * colorList.size() - BOX_MARGIN;
#if LANDSCAPE
    padTop((height() - size) / 2);  // ds-allow: theme editor — landscape vertical centering of the swatch column within the preview strip; not a DS list/form.
#else
    padLeft((width() - size) / 2);  // ds-allow: theme editor — portrait horizontal centering of the swatch row within the preview strip; not a DS list/form.
#endif
    for (auto color : colorList) {
      new ColorSwatch(this, {0, 0, boxWidth, boxWidth}, color.colorValue);
    }
  }

  void setColorList(std::vector<ColorEntry> colorList)
  {
    this->colorList = colorList;
    build();
  }

  static LAYOUT_VAL_SCALED(MAX_BOX_WIDTH, 15)  // ds-allow: theme editor — max swatch-box size in the color-swatch preview strip; not a DS list/form.
  static constexpr int BOX_MARGIN = 2;

 protected:
  std::vector<ColorEntry> colorList;
  int boxWidth = MAX_BOX_WIDTH;

  void setBoxWidth()
  {
#if LANDSCAPE
    boxWidth =
        (height() - (colorList.size() - 1) * BOX_MARGIN) / colorList.size();
#else
    boxWidth =
        (width() - (colorList.size() - 1) * BOX_MARGIN) / colorList.size();
#endif
    boxWidth = min(boxWidth, MAX_BOX_WIDTH);
  }
};

static const lv_coord_t d_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3),
                                       LV_GRID_TEMPLATE_LAST};
static const lv_coord_t b_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                       LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class ThemeDetailsDialog : public BaseDialog
{
 public:
  ThemeDetailsDialog(
      ThemeFile theme,
      std::function<bool(ThemeFile theme)> saveHandler = nullptr) :
      BaseDialog(STR_EDIT_THEME_DETAILS, false, DIALOG_DEFAULT_WIDTH,
                 LV_SIZE_CONTENT),
      theme(theme),
      saveHandler(saveHandler)
  {
    FlexGridLayout grid(d_col_dsc, row_dsc, PAD_TINY);  // ds-allow: theme editor — column gap for the theme-details dialog's custom name/author grid; not a single DS FormRow control.

    strAppend(name, this->theme.getName().c_str(), SELECTED_THEME_NAME_LEN);
    strAppend(author, this->theme.getAuthor().c_str(), ThemeFile::AUTHOR_LENGTH);
    strAppend(info, this->theme.getInfo().c_str(), ThemeFile::INFO_LENGTH);

    form.with([&](Window& formWindow) {
      auto line = formWindow.newLine(grid);
      line->padAll(PAD_TINY);  // ds-allow: theme editor — per-line padding on the name line of the theme-details dialog; not a single DS FormRow control.

      new StaticText(line, rect_t{}, STR_NAME);
      auto te = new TextEdit(line, rect_t{}, name, SELECTED_THEME_NAME_LEN);
      te->setGridCell(LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

      line = formWindow.newLine(grid);
      line->padAll(PAD_TINY);  // ds-allow: theme editor — per-line padding on the author line of the theme-details dialog; not a single DS FormRow control.

      new StaticText(line, rect_t{}, STR_AUTHOR);
      te = new TextEdit(line, rect_t{}, author, ThemeFile::AUTHOR_LENGTH);
      te->setGridCell(LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

      FlexGridLayout grid2(b_col_dsc, row_dsc, PAD_TINY);  // ds-allow: theme editor — column gap for the description/buttons sub-grid in the theme-details dialog; not a single DS FormRow control.

      line = formWindow.newLine(grid2);
      line->padAll(PAD_TINY);  // ds-allow: theme editor — per-line padding on the description label line of the theme-details dialog; not a single DS FormRow control.

      new StaticText(line, rect_t{}, STR_DESCRIPTION);
      line = formWindow.newLine(grid2);
      line->padAll(PAD_TINY);  // ds-allow: theme editor — per-line padding on the description text-edit line of the theme-details dialog; not a single DS FormRow control.
      te = new TextEdit(line, rect_t{}, info, ThemeFile::INFO_LENGTH);
      te->setGridCell(LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);

      line = formWindow.newLine(grid2);
      line->padAll(PAD_TINY);  // ds-allow: theme editor — per-line padding on the cancel/save button row of the theme-details dialog; not a single DS FormRow control.
      line->padTop(10);  // ds-allow: theme editor — extra top gap separating the button row from the description field; not a single DS FormRow control.

      auto button =
          new TextButton(line, rect_t{0, 0, lv_pct(30), 0}, STR_CANCEL, [=]() {
            deleteLater();
            return 0;
          });
      button->setGridCell(LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

      button =
          new TextButton(line, rect_t{0, 0, lv_pct(30), 0}, STR_SAVE, [=]() {
            if (saveHandler != nullptr) {
              this->theme.setName(name);
              this->theme.setAuthor(author);
              this->theme.setInfo(info);
              if (!saveHandler(this->theme)) return 0;
            }
            deleteLater();
            return 0;
          });
      button->setGridCell(LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    });
  }

 protected:
  ThemeFile theme;
  char name[SELECTED_THEME_NAME_LEN + 1];
  char author[ThemeFile::AUTHOR_LENGTH + 1];
  char info[ThemeFile::INFO_LENGTH + 1];
  std::function<bool(ThemeFile theme)> saveHandler = nullptr;
};

class ColorEditPage : public Page
{
 public:
  ColorEditPage(ThemeFile *theme, LcdColorIndex indexOfColor,
                std::function<void()> updateHandler = nullptr) :
      Page(ICON_RADIO_EDIT_THEME, Route{}, PAD_SMALL),  // ds-allow: theme editor — color-edit page built with small padding for a fixed list/editor/preview multi-pane split; not a DS list/form.
      _updateHandler(std::move(updateHandler)),
      _indexOfColor(indexOfColor),
      _theme(theme)
  {
    buildHead(header);
    buildBody(body);
  }

  void setActiveColorBar(int activeTab)
  {
    if (activeTab >= 0 && _activeTab <= (int)_tabs.size()) {
      _activeTab = activeTab;
      for (auto i = 0; i < (int)_tabs.size(); i++)
        _tabs[i]->check(i == _activeTab);
      _colorEditor->setColorEditorType(_activeTab == 1 ? HSV_COLOR_EDITOR
                                                       : RGB_COLOR_EDITOR);
    }
  }

  static LAYOUT_SIZE_SCALED(COLOR_BOX_WIDTH, 45, 55)  // ds-allow: theme editor — color-swatch box width in the fixed swatch+hex row; not a DS list/form.
  static LAYOUT_VAL_SCALED(COLOR_BOX_HEIGHT, 30)  // ds-allow: theme editor — color-swatch box height in the fixed swatch+hex row; not a DS list/form.
  static LAYOUT_VAL_SCALED(HEX_STR_W, 95)  // ds-allow: theme editor — hex-string field width beside the color swatch; not a DS list/form.
  static LAYOUT_SIZE_SCALED(BUTTON_WIDTH, 75, 65)  // ds-allow: theme editor — save/cancel button width in the color-edit page; not a DS list/form.
  static LAYOUT_SIZE(COLOR_LIST_SIZE, (LCD_W * 3) / 10, LCD_H / 2 - (PAD_LARGE * 3))  // ds-allow: theme editor — color-list pane size as viewport fraction in the fixed list/preview split; not a DS list/form.

 protected:
  std::function<void()> _updateHandler;
  LcdColorIndex _indexOfColor;
  ThemeFile *_theme;
  TextButton *_cancelButton;
  ColorEditor *_colorEditor;
  PreviewWindow *_previewWindow = nullptr;
  std::vector<ButtonBase *> _tabs;
  int _activeTab = 0;
  ColorSwatch *_colorSquare = nullptr;
  StaticText *_hexBox = nullptr;

  void onDelete() override
  {
    if (_updateHandler != nullptr) _updateHandler();
  }

  void setHexStr(uint32_t rgb)
  {
    if (_hexBox) {
      auto r = GET_RED(rgb), g = GET_GREEN(rgb), b = GET_BLUE(rgb);
      char hexstr[8];
      snprintf(hexstr, sizeof(hexstr), "%02X%02X%02X", (uint16_t)r, (uint16_t)g,
               (uint16_t)b);
      _hexBox->setText(hexstr);
    }
  }

  void buildBody(Window *form)
  {
    form->padAll(PAD_SMALL);  // ds-allow: theme editor — color-edit body padding around the fixed list/editor/preview panes; not a DS list/form.
#if LANDSCAPE
    form->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL);  // ds-allow: theme editor — landscape row split placing the color list beside the editor/preview; not a DS list/form.
    rect_t r = {0, 0, COLOR_LIST_SIZE, form->height() - 2 * PAD_SMALL};  // ds-allow: theme editor — landscape color-list pane rect sized as a viewport fraction; not a DS list/form.
#else
    form->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL);  // ds-allow: theme editor — portrait column split stacking the color list above the editor/preview; not a DS list/form.
    rect_t r = {0, 0, form->width() - 2 * PAD_SMALL, COLOR_LIST_SIZE};  // ds-allow: theme editor — portrait color-list pane rect sized as a viewport fraction; not a DS list/form.
#endif

    Window *colForm = new Window(form, r);
    colForm->padAll(PAD_ZERO);  // ds-allow: theme editor — zero padding on the color-editor column that stacks the swatch row and editor pane; not a DS list/form.
    colForm->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL, r.w);  // ds-allow: theme editor — column layout for the swatch row + color-editor pane; not a DS list/form.

#if LANDSCAPE
    r.w = form->width() - COLOR_LIST_SIZE - 3 * PAD_SMALL;  // ds-allow: theme editor — landscape preview-pane width = viewport remaining after the color list; not a DS list/form.
#else
    r.h = form->height() - COLOR_LIST_SIZE - 3 * PAD_SMALL;  // ds-allow: theme editor — portrait preview-pane height = viewport remaining after the color list; not a DS list/form.
#endif
    _previewWindow = new PreviewWindow(form, r, _theme->getColorList());

    r.w = colForm->width();
    r.h = COLOR_BOX_HEIGHT;

    Window *colBoxForm = new Window(colForm, r);
    colBoxForm->padAll(PAD_ZERO);  // ds-allow: theme editor — zero padding on the swatch+hex row so swatch and hex box abut; not a DS list/form.
    colBoxForm->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_TINY);  // ds-allow: theme editor — row layout packing the color swatch beside the hex box; not a DS list/form.

    r.h = colForm->height() - COLOR_BOX_HEIGHT - PAD_SMALL;  // ds-allow: theme editor — color-editor pane height = column remaining after the swatch row; not a DS list/form.

    auto colorEntry = _theme->getColorEntryByIndex(_indexOfColor);
    uint32_t color = colorEntry ? colorEntry->colorValue : 0;

    _colorEditor = new ColorEditor(
        colForm, r, COLOR2FLAGS(color) | RGB_FLAG,
        [=](uint32_t rgb) {
          rgb = COLOR_VAL(rgb);
          _theme->setColor(_indexOfColor, rgb);
          if (_colorSquare != nullptr) {
            _colorSquare->setColor(rgb);
          }
          if (_previewWindow != nullptr) {
            _previewWindow->setColorList(_theme->getColorList());
          }
          setHexStr(rgb);
        });
    _activeTab = 1;

    r.w = COLOR_BOX_WIDTH;
    r.h = COLOR_BOX_HEIGHT;
    _colorSquare = new ColorSwatch(
        colBoxForm, r, color);

    // hexBox
    r.w = HEX_STR_W;
    _hexBox = new StaticText(colBoxForm, r, "", COLOR_THEME_PRIMARY1_INDEX, FONT(L) | RIGHT);
    setHexStr(color);
  }

  void buildHead(PageHeader *window)
  {
    // page title
    header->setTitle(STR_EDIT_COLOR);
    auto t2 =
        header->setTitle2(ThemePersistance::getColorNames()[(int)_indexOfColor]);
#if NARROW_LAYOUT
    t2->font(FONT_XS_INDEX);
#else
    LV_UNUSED(t2);
#endif

    // page tabs
    rect_t r = {LCD_W - 2 * (BUTTON_WIDTH + PAD_SMALL + 1) - EdgeTxStyles::MENU_HEADER_HEIGHT, PAD_MEDIUM, BUTTON_WIDTH, 0};  // ds-allow: theme editor — RGB/HSV tab buttons positioned absolutely at the top-right of the color-edit header; not a DS list/form.
    _tabs.emplace_back(new TextButton(window, r, "RGB", [=]() {
      setActiveColorBar(0);
      return 1;
    }));
    r.x += (BUTTON_WIDTH + 5);
    _tabs.emplace_back(new TextButton(window, r, "HSV", [=]() {
      setActiveColorBar(1);
      return 1;
    }));
    _tabs[1]->check(true);
  }
};

class ThemeEditPage : public Page
{
 public:
  explicit ThemeEditPage(
      ThemeFile *theme,
      std::function<void(ThemeFile &theme)> saveHandler = nullptr) :
      Page(ICON_RADIO_EDIT_THEME, Route{}, PAD_SMALL),  // ds-allow: theme editor — theme-edit page built with small padding for a fixed color-list beside preview-pane split; not a DS list/form.
      _theme(*theme),
      page(this),
      saveHandler(std::move(saveHandler))
  {
    buildHeader(header);
    buildBody(body);
  }

  void onCancel() override
  {
    if (_dirty) {
      new ConfirmDialog(STR_SAVE_THEME, _theme.getName().c_str(),
          [=]() {
            if (saveHandler != nullptr) {
              saveHandler(_theme);
            }
            deleteLater();
          },
          [=]() { deleteLater(); });
    } else {
      deleteLater();
    }
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (!started && _themeName) {
      started = true;
      _themeName->setText(_theme.getName());
    }
    Page::onLiveCheckEvents(live);
  }

  void editColorPage()
  {
    auto colorEntry = _cList->getSelectedColor();
    new ColorEditPage(&_theme, colorEntry.colorNumber, [=]() {
      _dirty = true;
      _cList->setColorList(_theme.getColorList());
      _previewWindow->setColorList(_theme.getColorList());
    });
  }

  void buildHeader(Window *window)
  {
    // page title
    header->setTitle(STR_EDIT_THEME);
    _themeName = header->setTitle2(_theme.getName());
#if NARROW_LAYOUT
    _themeName->font(FONT_XS_INDEX);
#endif

    // save and cancel
    rect_t r = {LCD_W - (ColorEditPage::BUTTON_WIDTH + PAD_SMALL + 1) - EdgeTxStyles::MENU_HEADER_HEIGHT, PAD_MEDIUM, ColorEditPage::BUTTON_WIDTH, 0};  // ds-allow: theme editor — details button positioned absolutely at the top-right of the theme-edit header; not a DS list/form.
    new TextButton(window, r, STR_DETAILS, [=]() {
      new ThemeDetailsDialog(_theme, [=](ThemeFile t) {
        _theme.setAuthor(t.getAuthor());
        _theme.setInfo(t.getInfo());
        _theme.setName(t.getName());

        // update the theme name
        _themeName->setText(_theme.getName());
        _dirty = true;
        return true;
      });
      return 0;
    });
  }

  void buildBody(Window *form)
  {
    form->padAll(PAD_SMALL);  // ds-allow: theme editor — theme-edit body padding around the fixed color-list and preview panes; not a DS list/form.
#if LANDSCAPE
    form->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL);  // ds-allow: theme editor — landscape row split placing the color list beside the preview pane; not a DS list/form.
    rect_t r = {0, 0, ColorEditPage::COLOR_LIST_SIZE, form->height() - 2 * PAD_SMALL};  // ds-allow: theme editor — landscape color-list rect sized as a viewport fraction; not a DS list/form.
#else
    form->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL);  // ds-allow: theme editor — portrait column split stacking the color list above the preview pane; not a DS list/form.
    rect_t r = {0, 0, form->width() - 2 * PAD_SMALL, ColorEditPage::COLOR_LIST_SIZE};  // ds-allow: theme editor — portrait color-list rect sized as a viewport fraction; not a DS list/form.
#endif

    _cList = new ColorList(form, r, _theme.getColorList());
    _cList->setLongPressHandler([=]() { editColorPage(); });
    _cList->setPressHandler([=]() { editColorPage(); });

#if LANDSCAPE
    r.w = form->width() - ColorEditPage::COLOR_LIST_SIZE - 3 * PAD_SMALL;  // ds-allow: theme editor — landscape preview-pane width = viewport remaining after the color list; not a DS list/form.
#else
    r.h = form->height() - ColorEditPage::COLOR_LIST_SIZE - 3 * PAD_SMALL;  // ds-allow: theme editor — portrait preview-pane height = viewport remaining after the color list; not a DS list/form.
#endif
    _previewWindow = new PreviewWindow(form, r, _theme.getColorList());
  }

 protected:
  bool _dirty = false;
  bool started = false;
  ThemeFile _theme;
  Page *page;
  std::function<void(ThemeFile &theme)> saveHandler = nullptr;
  PreviewWindow *_previewWindow = nullptr;
  ColorList *_cList = nullptr;
  StaticText *_themeName = nullptr;
};

ThemeSetupPage::ThemeSetupPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

void ThemeSetupPage::setAuthor(ThemeFile *theme)
{
  std::string s("");
  if (theme && !theme->getAuthor().empty()) {
    s = s + "By: " + theme->getAuthor();
  }
  authorText->setText(s);
}

void ThemeSetupPage::setName(ThemeFile *theme)
{
  if (theme && !theme->getName().empty()) {
    nameText->setText(theme->getName());
  } else {
    nameText->setText("");
  }
}

void ThemeSetupPage::checkEvents()
{
  PageGroupItem::checkEvents();

  if (fileCarosell) fileCarosell->pause(!isVisible());
}

void ThemeSetupPage::displayThemeMenu(Window *window, ThemePersistance *tp)
{
  auto menu = new Menu(false);

  // you can't activate the active theme
  if (listBox->getSelected() != tp->getThemeIndex()) {
    menu->addLine(STR_ACTIVATE, [=]() {
      auto idx = listBox->getSelected();
      tp->applyTheme(idx);
      tp->setDefaultTheme(idx);
      listBox->setActiveItem(idx);
    });
  }

  // you can't edit the default theme
  if (listBox->getSelected() != 0) {
    menu->addLine(STR_EDIT, [=]() {
      auto themeIdx = listBox->getSelected();
      if (themeIdx < 0) return;

      auto theme = tp->getThemeByIndex(themeIdx);
      if (theme == nullptr) return;

      new ThemeEditPage(theme, [=](ThemeFile &editedTheme) {
        // update cached theme data
        *theme = editedTheme;

        theme->serialize();

        // if the theme info currently displayed
        // were changed, update the UI
        if (themeIdx == currentTheme) {
          setAuthor(theme);
          setName(theme);
          listBox->setName(currentTheme, theme->getName());
          themeColorPreview->setColorList(theme->getColorList());
        }

        // if the active theme changed, re-apply it
        if (themeIdx == tp->getThemeIndex()) {
          // Update saved them name in case it was changed.
          tp->setDefaultTheme(themeIdx);
          theme->applyTheme();
        }
      });
    });
  }

  menu->addLine(STR_DUPLICATE, [=]() {
    ThemeFile newTheme;

    new ThemeDetailsDialog(newTheme, [=](ThemeFile theme) {
      if (!theme.getName().empty()) {
        char name[SELECTED_THEME_NAME_LEN + 1];
        int n = 0;
        for (size_t i = 0; i < theme.getName().size(); i += 1) {
          if (!isspace(theme.getName()[i])) {
            name[n] = theme.getName()[i];
            n += 1;
          }
        }
        name[n] = 0;

        // use the selected themes color list to make the new theme
        auto themeIdx = listBox->getSelected();
        if (themeIdx < 0) return true;

        auto selTheme = tp->getThemeByIndex(themeIdx);
        if (selTheme == nullptr) return true;

        for (auto color : selTheme->getColorList())
          theme.setColor(color.colorNumber, color.colorValue);

        if (!tp->createNewTheme(name, theme))
          return false;

        listBox->setNames(tp->getNames());
        listBox->setSelected(currentTheme);
      }
      return true;
    });
  });

  // you can't delete the default theme or the currently active theme
  auto selectedThemeIndex = listBox->getSelected();
  auto selectedTheme = tp->getThemeByIndex(selectedThemeIndex);
  if (selectedTheme && selectedThemeIndex != 0 &&
      selectedThemeIndex != tp->getThemeIndex()) {
    menu->addLine(STR_DELETE, [=]() {
      auto theme = tp->getThemeByIndex(listBox->getSelected());
      if (!theme) return;

      new ConfirmDialog(STR_DELETE_THEME, theme->getName().c_str(), [=] {
            auto deleteIndex = listBox->getSelected();
            if (!tp->getThemeByIndex(deleteIndex)) return;
            tp->deleteThemeByIndex(deleteIndex);
            listBox->setNames(tp->getNames());
            currentTheme = min<int>(currentTheme, tp->getNames().size() - 1);
            listBox->setSelected(currentTheme);
          });
    });
  }
}

void ThemeSetupPage::setSelected(ThemePersistance *tp)
{
  auto value = listBox->getSelected();
  if (currentTheme != value && themeColorPreview && authorText && nameText && fileCarosell) {
    ThemeFile *theme = tp->getThemeByIndex(value);
    if (theme) {
      themeColorPreview->setColorList(theme->getColorList());
      setAuthor(theme);
      setName(theme);
      fileCarosell->setFileNames(theme->getThemeImageFileNames());
    }
    currentTheme = value;
  }
}

void ThemeSetupPage::setupListbox(Window *window, rect_t r,
                                  ThemePersistance *tp)
{
  listBox = new ListBox(window, r, tp->getNames());
  listBox->scrollbar();
  listBox->setAutoEdit();
  listBox->setSelected(currentTheme);
  listBox->setActiveItem(tp->getThemeIndex());
  listBox->setLongPressHandler([=]() {
    setSelected(tp);
    displayThemeMenu(window, tp);
  });
  listBox->setPressHandler([=]() { setSelected(tp); });
}

void ThemeSetupPage::build(Window *window)
{
  window->padAll(PAD_SMALL);  // ds-allow: theme editor — theme-setup body padding around the fixed list/color-preview/carousel panes; not a DS list/form.
  pageWindow = window;

#if LANDSCAPE
  window->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL);  // ds-allow: theme editor — landscape row split placing theme list, color-preview strip and carousel side by side; not a DS list/form.
#else
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL);  // ds-allow: theme editor — portrait column split stacking theme list, color-preview strip and carousel; not a DS list/form.
#endif

  auto tp = ThemePersistance::instance();
  auto theme = tp->getCurrentTheme();
  currentTheme = tp->getThemeIndex();

  themeColorPreview = nullptr;
  listBox = nullptr;
  fileCarosell = nullptr;
  nameText = nullptr;
  authorText = nullptr;

  // create listbox and setup menus
#if LANDSCAPE
  rect_t r = {0, 0, LIST_SIZE, window->height() - 2 * PAD_SMALL};  // ds-allow: theme editor — landscape theme-list pane rect sized as a viewport fraction; not a DS list/form.
#else
  rect_t r = {0, 0, window->width() - 2 * PAD_SMALL, LIST_SIZE};  // ds-allow: theme editor — portrait theme-list pane rect sized as a viewport fraction; not a DS list/form.
#endif
  setupListbox(window, r, tp);

#if LANDSCAPE
  r.w = COLOR_PREVIEW_SIZE;
#else
  r.h = COLOR_PREVIEW_SIZE;
#endif

  // setup ThemeColorPreview()
  auto colorList =
      theme != nullptr ? theme->getColorList() : std::vector<ColorEntry>();
  themeColorPreview = new ThemeColorPreview(window, r, colorList);
  themeColorPreview->setWidth(r.w);

#if LANDSCAPE
  r.w = window->width() - LIST_SIZE - COLOR_PREVIEW_SIZE - 4 * PAD_SMALL;  // ds-allow: theme editor — landscape carousel/detail pane width = viewport remaining after the list and color-preview strip; not a DS list/form.
  r.h = window->height() - 2 * PAD_SMALL;  // ds-allow: theme editor — landscape carousel/detail pane height spanning the page; not a DS list/form.
#else
  r.w = window->width() - 2 * PAD_SMALL;  // ds-allow: theme editor — portrait carousel/detail pane width spanning the page; not a DS list/form.
  r.h = window->height() - LIST_SIZE - COLOR_PREVIEW_SIZE - 4 * PAD_SMALL;  // ds-allow: theme editor — portrait carousel/detail pane height = viewport remaining after the list and color-preview strip; not a DS list/form.
#endif

  auto rw = new Window(window, r);
  rw->padAll(PAD_ZERO);  // ds-allow: theme editor — zero padding on the right-hand detail pane holding the carousel and name/author text; not a DS list/form.
  rw->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, r.w);  // ds-allow: theme editor — column layout stacking the carousel and name/author text in the detail pane; not a DS list/form.

  r.h -= 2 * EdgeTxStyles::STD_FONT_HEIGHT;

  // setup FileCarosell()
  auto fileNames = theme != nullptr ? theme->getThemeImageFileNames()
                                    : std::vector<std::string>();
  fileCarosell = new FileCarosell(rw, r, fileNames);

  r.h = EdgeTxStyles::STD_FONT_HEIGHT;

  // author and name of theme on right side of screen
  nameText = new StaticText(rw, r, "");
  nameText->setLongMode(LV_LABEL_LONG_DOT);
  authorText = new StaticText(rw, r, "");
  authorText->setLongMode(LV_LABEL_LONG_DOT);

  setName(theme);
  setAuthor(theme);
}
