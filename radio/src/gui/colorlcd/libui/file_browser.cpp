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

#include "file_browser.h"
#include "lib_file.h"
#include "fonts.h"

#include <cstring>
#include <list>
#include <string>

#define CELL_CTRL_DIR  LV_TABLE_CELL_CTRL_CUSTOM_1
#define CELL_CTRL_FILE LV_TABLE_CELL_CTRL_CUSTOM_2

static const char* getFullPath(const char* filename)
{
  static char full_path[FF_MAX_LFN + 1];
  if (f_getcwd((TCHAR*)full_path, FF_MAX_LFN) != FR_OK) {
    full_path[0] = '\0';
  }
  full_path[FF_MAX_LFN] = '\0';

  size_t path_len = strlen(full_path);
  if (path_len < FF_MAX_LFN) {
    full_path[path_len++] = '/';
    full_path[path_len] = '\0';
  }

  if (filename && path_len < FF_MAX_LFN) {
    strncat(full_path, filename, FF_MAX_LFN - path_len);
    full_path[FF_MAX_LFN] = '\0';
  }
  return full_path;
}

static const char* getCurrentPath()
{
  static char path[FF_MAX_LFN + 1];
  f_getcwd((TCHAR*)path, FF_MAX_LFN);
  return path;
}

static int strnatcasecmp(char const *s1, char const *s2)
{
  int i1, i2;
  char c1, c2;

  i1 = i2 = 0;
  while (true) {
    c1 = s1[i1]; c2 = s2[i2];

    if (c1 == 0 && c2 == 0) {
      return 0;
    }

    if (isdigit(c1) && isdigit(c2)) {
      int num_cmp = 0;
      while (true) {
        if (!num_cmp) {
          if (c1 < c2) {
            num_cmp = -1;
          } else if (c1 > c2) {
            num_cmp = 1;
          }
        }
        i1 += 1; i2 += 1;
        c1 = s1[i1]; c2 = s2[i2];
        if (!isdigit(c1) && !isdigit(c2))
          break;
        if (!isdigit(c1)) {
          num_cmp = -1;
          break;
        }
        if (!isdigit(c2)) {
          num_cmp = 1;
          break;
        }
      }
      if (num_cmp)
        return num_cmp;
    }

    c1 = toupper(c1);
    c2 = toupper(c2);

    if (c1 < c2)
      return -1;

    if (c1 > c2)
      return +1;

    i1 += 1; i2 += 1;
  }
}

// natural comparison, not case sensitive.
static bool natural_compare_nocase(const std::string & first, const std::string & second)
{
  return strnatcasecmp(first.c_str(), second.c_str()) < 0;
}

static int scan_files(std::list<std::string>& files,
                      std::list<std::string>& directories)
{
  FILINFO fno;
  DIR dir;

  FRESULT res = f_opendir(&dir, "."); // Open the directory
  if (res != FR_OK) return -1;

  // read all entries
  bool firstTime = true;
  for (;;) {
    res = sdReadDir(&dir, &fno, firstTime);

    if (res != FR_OK || fno.fname[0] == 0)
      break; // Break on error or end of dir
    // if (strlen((const char*)fno.fname) > SD_SCREEN_FILE_LENGTH)
    //   continue;
    if (fno.fattrib & (AM_HID|AM_SYS)) continue;     /* Ignore hidden and system files */
    if (fno.fname[0] == '.' && fno.fname[1] != '.') continue; // Ignore hidden files under UNIX, but not ..

    if (fno.fattrib & AM_DIR) {
      directories.push_back((char*)fno.fname);
    } else {
      files.push_back((char*)fno.fname);
    }
  }

  directories.sort(natural_compare_nocase);
  files.sort(natural_compare_nocase);

  return 0;
}

FileBrowser::FileBrowser(Window* parent, const rect_t& rect, const char* dir) :
    TableField(parent, rect)
{
  f_chdir(dir);

  setAutoEdit();

  setLongPressHandler([=]() {
    int row = getSelected();
    withLive([&](LiveWindow& live) {
      auto obj = live.lvobj();
      bool is_dir = lv_table_has_cell_ctrl(obj, row, 0, CELL_CTRL_DIR);
      onPressLong(lv_table_get_cell_value(obj, row, 0), is_dir);
    });
  });
}

void FileBrowser::setFileAction(FileAction fct) { fileAction = std::move(fct); }
void FileBrowser::setFileSelected(FileAction fct) { fileSelected = std::move(fct); }

void FileBrowser::refresh()
{
  std::list<std::string> files;
  std::list<std::string> directories;
  if (scan_files(files, directories) < 0) return;

  setRowCount(files.size() + directories.size());

  uint16_t row = 0;
  for (const auto& name: directories) {

    // LV_SYMBOL_DIRECTORY
    withLive([&](LiveWindow& live) {
      auto obj = live.lvobj();
      lv_table_set_cell_value(obj, row, 0, name.c_str());
      lv_table_set_cell_ctrl(obj, row, 0, CELL_CTRL_DIR);
    });
    row++;
  }

  for (const auto& name: files) {
    // LV_SYMBOL_FILE
    withLive([&](LiveWindow& live) {
      auto obj = live.lvobj();
      lv_table_set_cell_value(obj, row, 0, name.c_str());
      lv_table_clear_cell_ctrl(obj, row, 0, CELL_CTRL_DIR);
    });
    row++;
  }

  select(0, 0);
}

void FileBrowser::adjustWidth()
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_obj_update_layout(obj);
    setColumnWidth(0, lv_obj_get_width(obj));
  });
}

void FileBrowser::onSelected(uint16_t row, uint16_t col)
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    bool is_dir = lv_table_has_cell_ctrl(obj, row, col, CELL_CTRL_DIR);
    onSelected(lv_table_get_cell_value(obj, row, col), is_dir);
  });
}

void FileBrowser::onPress(uint16_t row, uint16_t col)
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    bool is_dir = lv_table_has_cell_ctrl(obj, row, col, CELL_CTRL_DIR);
    onPress(lv_table_get_cell_value(obj, row, col), is_dir);
  });
}

void FileBrowser::onDrawBegin(uint16_t row, uint16_t col, lv_area_t* cell_area,
                               lv_layer_t*)
{
  // onDrawBegin was used to pre-modify dsc->label_dsc->ofs_x
  // In LVGL9, pre-draw modification is no longer possible.
  // The icon is drawn in onDrawEnd.
}

void FileBrowser::onDrawEnd(uint16_t row, uint16_t col, lv_area_t* cell_area,
                             lv_layer_t* layer)
{
  const char* sym = nullptr;
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    if (lv_table_has_cell_ctrl(obj, row, 0, CELL_CTRL_DIR)) {
      const char* dir = lv_table_get_cell_value(obj, row, 0);
      if (dir[0] == '.')
        sym = LV_SYMBOL_LEFT;
      else
        sym = LV_SYMBOL_DIRECTORY;
    } else {
      sym = LV_SYMBOL_FILE;
    }
  });

  lv_area_t coords;
  lv_coord_t area_h = lv_area_get_height(cell_area);

  lv_coord_t cell_left = 0;
  withLive([&](LiveWindow& live) {
    cell_left = lv_obj_get_style_pad_left(live.lvobj(), LV_PART_ITEMS);
  });
  lv_coord_t font_h = getFontHeight(FONT(STD));

  coords.x1 = cell_area->x1 + cell_left;
  coords.x2 = coords.x1 + font_h - 1;
  coords.y1 = cell_area->y1 + (area_h - font_h) / 2;
  coords.y2 = coords.y1 + font_h - 1;

  lv_draw_label_dsc_t label_dsc;
  lv_draw_label_dsc_init(&label_dsc);
  label_dsc.text = sym;
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    label_dsc.color = lv_obj_get_style_text_color(obj, LV_PART_ITEMS);
    label_dsc.font = lv_obj_get_style_text_font(obj, LV_PART_ITEMS);
  });
  lv_draw_label(layer, &label_dsc, &coords);
}

void FileBrowser::onSelected(const char* name, bool is_dir)
{
  if (is_dir) {
    if (fileSelected) fileSelected(nullptr, nullptr, nullptr, is_dir);
    selected = nullptr;
    return;
  }

  const char* path = getCurrentPath();
  const char* fullpath = getFullPath(name);
  if (fileSelected) fileSelected(path, name, fullpath, is_dir);
  selected = name;
}

void FileBrowser::onPress(const char* name, bool is_dir)
{
  const char* path = getCurrentPath();
  const char* fullpath = getFullPath(name);
  if (is_dir) {
    f_chdir(fullpath);
    if (fileSelected) fileSelected(nullptr, nullptr, nullptr, is_dir);
    selected = nullptr;
    refresh();
    return;
  }

  if (!selected || (selected != name)) {
    onSelected(name, is_dir);
    return;
  }

  if (fileAction){
    fileAction(path, name, fullpath, is_dir);
  }
}

void FileBrowser::onPressLong(const char* name, bool is_dir)
{
  const char* path = getCurrentPath();
  const char* fullpath = getFullPath(name);

  if (!selected || (selected != name)) {
    onSelected(name, is_dir);
  }

  if (fileAction){
    fileAction(path, name, fullpath, is_dir);
  }
}
