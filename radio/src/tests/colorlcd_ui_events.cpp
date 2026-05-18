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

#include "gtests.h"

#if defined(COLORLCD) && defined(SIMU) && !GTEST_OS_WINDOWS

#include "ui_events.h"
#include "ui_signal.h"

#include <vector>

namespace
{

void drainUiEventHub()
{
  UiEventHub::flush();
  UiEventHub::flush();
}

}  // namespace

TEST(ColorUiEvents, ScopedConnectionDisconnectsOnDestruction)
{
  UiSignal<uint32_t> signal;
  uint32_t calls = 0;

  {
    auto connection = signal.connect([&](uint32_t) { calls++; });
    ASSERT_TRUE(connection.connected());
    signal.emit(1);
  }

  signal.emit(2);
  EXPECT_EQ(calls, 1u);
}

TEST(ColorUiEvents, ScopedConnectionDisconnectsOnReset)
{
  UiSignal<uint32_t> signal;
  uint32_t calls = 0;

  auto connection = signal.connect([&](uint32_t) { calls++; });
  ASSERT_TRUE(connection.connected());

  connection = UiScopedConnection();
  EXPECT_FALSE(connection.connected());

  signal.emit(1);
  EXPECT_EQ(calls, 0u);
}

TEST(ColorUiEvents, EventHubCoalescesRepeatedPublishesIntoOneFlushCallback)
{
  drainUiEventHub();

  uint32_t calls = 0;
  uint32_t lastData = 0;
  auto connection =
      UiEventHub::subscribe(UiTopic::ModelOutputsChanged, [&](uint32_t data) {
        calls++;
        lastData = data;
      });
  ASSERT_TRUE(connection.connected());

  UiEventHub::publish(UiTopic::ModelOutputsChanged, 10);
  UiEventHub::publish(UiTopic::ModelOutputsChanged, 20);
  UiEventHub::publish(UiTopic::ModelOutputsChanged, 30);
  UiEventHub::flush();

  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(lastData, 30u);
}

TEST(ColorUiEvents, EventHubDefersPublishDuringFlushUntilNextFlush)
{
  drainUiEventHub();

  uint32_t calls = 0;
  uint32_t lastData = 0;
  auto connection = UiEventHub::subscribe(
      UiTopic::ModelFlightModesChanged, [&](uint32_t data) {
        calls++;
        lastData = data;
        if (data == 10) {
          UiEventHub::publish(UiTopic::ModelFlightModesChanged, 20);
        }
      });
  ASSERT_TRUE(connection.connected());

  UiEventHub::publish(UiTopic::ModelFlightModesChanged, 10);
  UiEventHub::flush();
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(lastData, 10u);

  UiEventHub::flush();
  EXPECT_EQ(calls, 2u);
  EXPECT_EQ(lastData, 20u);
}

TEST(ColorUiEvents, LiveSubscriptionUpdatesLiveConsumerCount)
{
  const uint8_t initialCount = UiEventHub::liveValueConsumerCountForTest();
  ASSERT_LT(initialCount, UINT8_MAX);

  {
    auto subscription = UiEventHub::registerLiveValueConsumer();
    EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(),
              static_cast<uint8_t>(initialCount + 1));

    subscription.reset();
    EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), initialCount);
  }

  EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), initialCount);
}

TEST(ColorUiEvents, LiveSubscriptionSaturationDoesNotUndercountOnReset)
{
  const uint8_t initialCount = UiEventHub::liveValueConsumerCountForTest();
  ASSERT_LT(initialCount, UINT8_MAX);

  std::vector<UiLiveSubscription> subscriptions;
  subscriptions.reserve(UINT8_MAX - initialCount + 1);

  for (uint16_t i = initialCount; i < UINT8_MAX; i++) {
    subscriptions.emplace_back(UiEventHub::registerLiveValueConsumer());
  }
  EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), UINT8_MAX);

  auto saturatedSubscription = UiEventHub::registerLiveValueConsumer();
  EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), UINT8_MAX);

  saturatedSubscription.reset();
  EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), UINT8_MAX);

  subscriptions.clear();
  EXPECT_EQ(UiEventHub::liveValueConsumerCountForTest(), initialCount);
}

#endif
