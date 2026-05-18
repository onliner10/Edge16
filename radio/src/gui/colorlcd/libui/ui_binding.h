/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include "ui_events.h"
#include "window.h"

#include <functional>
#include <utility>

template <typename T, typename Equal = std::equal_to<T>>
class UiBinding
{
 public:
  using Reader = std::function<T()>;
  using Renderer = std::function<void(const T&)>;

  UiBinding(Window& owner, UiTopic topic, Reader reader, Renderer renderer,
            Equal equal = Equal()) :
      reader(std::move(reader)),
      renderer(std::move(renderer)),
      equal(std::move(equal))
  {
    owner.trackUiConnection(UiEventHub::subscribe(
        topic, [this](uint32_t) { update(); }));
    update();
  }

  UiBinding(const UiBinding&) = delete;
  UiBinding& operator=(const UiBinding&) = delete;

  void pause() { paused = true; }

  void resume()
  {
    paused = false;
    update();
  }

  void update()
  {
    if (paused) return;

    T value = reader();
    if (hasValue && equal(lastValue, value)) return;

    lastValue = value;
    hasValue = true;
    renderer(lastValue);
  }

 private:
  Reader reader;
  Renderer renderer;
  Equal equal;
  T lastValue = {};
  bool hasValue = false;
  bool paused = false;
};
