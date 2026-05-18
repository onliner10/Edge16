/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ui_events.h"

#include "LvglWrapper.h"
#include "os/time.h"
#include "timers.h"

#include <array>

namespace {

constexpr auto TOPIC_COUNT = static_cast<size_t>(UiTopic::Count);

std::array<UiSignal<uint32_t>, TOPIC_COUNT> topicSignals;
std::array<bool, TOPIC_COUNT> dirtyTopics = {};
std::array<bool, TOPIC_COUNT> deferredTopics = {};
std::array<uint32_t, TOPIC_COUNT> topicPayloads = {};
std::array<uint32_t, TOPIC_COUNT> deferredPayloads = {};

bool flushing = false;
uint8_t liveValueConsumers = 0;
uint32_t nextLiveValueTick = 0;

size_t topicIndex(UiTopic topic)
{
  return static_cast<size_t>(topic);
}

}  // namespace

UiLiveSubscription::~UiLiveSubscription()
{
  reset();
}

UiLiveSubscription::UiLiveSubscription(UiLiveSubscription&& other) noexcept :
    active(other.active)
{
  other.active = false;
}

UiLiveSubscription& UiLiveSubscription::operator=(
    UiLiveSubscription&& other) noexcept
{
  if (this != &other) {
    reset();
    active = other.active;
    other.active = false;
  }
  return *this;
}

void UiLiveSubscription::reset()
{
  if (!active) return;
  active = false;
  if (liveValueConsumers > 0) liveValueConsumers--;
  if (liveValueConsumers == 0) nextLiveValueTick = 0;
}

UiScopedConnection UiEventHub::subscribe(UiTopic topic, Callback callback)
{
  return topicSignals[topicIndex(topic)].connect(std::move(callback));
}

void UiEventHub::publish(UiTopic topic, uint32_t data)
{
  const auto index = topicIndex(topic);
  if (flushing) {
    deferredTopics[index] = true;
    deferredPayloads[index] = data;
  } else {
    dirtyTopics[index] = true;
    topicPayloads[index] = data;
  }
}

void UiEventHub::emitNow(UiTopic topic, uint32_t data)
{
  topicSignals[topicIndex(topic)].emit(data);
}

void UiEventHub::flush()
{
  flushing = true;

  for (size_t i = 0; i < TOPIC_COUNT; i++) {
    if (!dirtyTopics[i]) continue;
    dirtyTopics[i] = false;
    topicSignals[i].emit(topicPayloads[i]);
  }

  flushing = false;

  for (size_t i = 0; i < TOPIC_COUNT; i++) {
    if (!deferredTopics[i]) continue;
    deferredTopics[i] = false;
    dirtyTopics[i] = true;
    topicPayloads[i] = deferredPayloads[i];
  }
}

void UiEventHub::tickLiveValues()
{
  if (liveValueConsumers == 0) return;

  const uint32_t now = time_get_ms();
  if (nextLiveValueTick == 0 ||
      static_cast<int32_t>(now - nextLiveValueTick) >= 0) {
    publish(UiTopic::LiveChannelValues);
    nextLiveValueTick = now + lvglLiveValueRefreshPeriod();
  }
}

UiLiveSubscription UiEventHub::registerLiveValueConsumer()
{
  if (liveValueConsumers >= UINT8_MAX) return UiLiveSubscription(false);

  liveValueConsumers++;
  return UiLiveSubscription(true);
}

#if defined(UNIT_TEST) || defined(SIMU)
uint8_t UiEventHub::liveValueConsumerCountForTest()
{
  return liveValueConsumers;
}
#endif
