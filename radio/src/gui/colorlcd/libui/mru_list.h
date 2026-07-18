/*
 * Copyright (C) EdgeTX
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

#include <cstdint>

// Text used to visually separate the pinned most-recently-used entries at the
// top of a long picker from the full list below. Plain ASCII hyphens so it
// renders on every bundled font.
#define MRU_DIVIDER_TEXT "----------------"

// Small, fixed-size most-recently-used list for long pickers.
//
// A picker "value" is whatever integer identifies a choice for that picker
// (e.g. a Special-Function sort index, or a widget-catalog index). The most
// recently chosen values are kept at the front, capped at CAPACITY entries.
//
// Storage is session-only (a plain radio-global static per picker type). It is
// deliberately NOT persisted to radio settings: an MRU accelerator is pure
// convenience, and keeping it volatile avoids touching the RadioData layout,
// its YAML serialisation and any settings-version migration - i.e. it is both
// the cheapest option and trivially migration-safe (there is nothing to
// migrate). The list simply rebuilds itself from use after each power cycle.
class MRUList
{
 public:
  static constexpr uint8_t CAPACITY = 3;

  // Record a chosen value, moving it to the front (most-recent).
  void touch(int value)
  {
    int found = -1;
    for (uint8_t i = 0; i < count_; ++i) {
      if (values_[i] == value) {
        found = i;
        break;
      }
    }
    if (found < 0) {
      if (count_ < CAPACITY) ++count_;
      found = count_ - 1;
    }
    for (int i = found; i > 0; --i) values_[i] = values_[i - 1];
    values_[0] = value;
  }

  uint8_t size() const { return count_; }

  int operator[](uint8_t index) const { return values_[index]; }

  bool contains(int value) const
  {
    for (uint8_t i = 0; i < count_; ++i)
      if (values_[i] == value) return true;
    return false;
  }

  void clear() { count_ = 0; }

 private:
  int values_[CAPACITY] = {0};
  uint8_t count_ = 0;
};
