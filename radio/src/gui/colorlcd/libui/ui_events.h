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

#include "ui_signal.h"

#include <functional>
#include <stdint.h>

enum class UiTopic : uint8_t
{
  GlobalRefresh = 0,
  ModelOutputsChanged,
  ModelFlightModesChanged,
  ModelGVarsChanged,
  SpecialFunctionsChanged,
  LiveChannelValues,
  PageVisibilityChanged,
  Count,
};

class UiLiveSubscription
{
 public:
  UiLiveSubscription() = default;
  ~UiLiveSubscription();

  UiLiveSubscription(const UiLiveSubscription&) = delete;
  UiLiveSubscription& operator=(const UiLiveSubscription&) = delete;

  UiLiveSubscription(UiLiveSubscription&& other) noexcept;
  UiLiveSubscription& operator=(UiLiveSubscription&& other) noexcept;

  void reset();

 private:
  friend class UiEventHub;
  explicit UiLiveSubscription(bool active) : active(active) {}

  bool active = false;
};

class UiEventHub
{
 public:
  using Callback = std::function<void(uint32_t)>;

  static UiScopedConnection subscribe(UiTopic topic, Callback callback);
  static void publish(UiTopic topic, uint32_t data = 0);
  static void emitNow(UiTopic topic, uint32_t data = 0);
  static void flush();
  static void tickLiveValues();
  static UiLiveSubscription registerLiveValueConsumer();

#if defined(UNIT_TEST) || defined(SIMU)
  static uint8_t liveValueConsumerCountForTest();
#endif
};
