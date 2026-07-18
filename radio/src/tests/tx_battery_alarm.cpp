/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Locks the TX-battery alarm threshold/precedence decision that keeps the
 * audible/haptic low-battery alert in agreement with the state-aware
 * Value/TX-battery widget colour: Warning at vBatWarn, Critical (RED) at the
 * radio-level vBatCrit setting. Critical takes precedence over warning so the
 * RED state is never silent, and the two never sound together.
 */

#include "gtests.h"

extern uint8_t g_vbat100mV;

namespace {

// Restore the touched globals so this fixture never leaks state into other
// tests that read g_vbat100mV / g_eeGeneral voltages.
struct TxBattAlarmGuard {
  uint8_t vbat, warn, crit;
  TxBattAlarmGuard()
      : vbat(g_vbat100mV),
        warn(g_eeGeneral.vBatWarn),
        crit(g_eeGeneral.vBatCrit) {}
  ~TxBattAlarmGuard() {
    g_vbat100mV = vbat;
    g_eeGeneral.vBatWarn = warn;
    g_eeGeneral.vBatCrit = crit;
  }
};

}  // namespace

TEST(TxBatteryAlarm, thresholdBands)
{
  TxBattAlarmGuard guard;
  g_eeGeneral.vBatWarn = 68;  // 6.8V
  g_eeGeneral.vBatCrit = 65;  // 6.5V

  // Above the warning voltage -> silent.
  g_vbat100mV = 70;
  EXPECT_FALSE(IS_TXBATT_WARNING());
  EXPECT_FALSE(IS_TXBATT_CRITICAL());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_NONE);

  // Exactly at the warning voltage -> warning.
  g_vbat100mV = 68;
  EXPECT_TRUE(IS_TXBATT_WARNING());
  EXPECT_FALSE(IS_TXBATT_CRITICAL());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_WARNING);

  // Between critical and warning -> warning only.
  g_vbat100mV = 66;
  EXPECT_TRUE(IS_TXBATT_WARNING());
  EXPECT_FALSE(IS_TXBATT_CRITICAL());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_WARNING);
}

TEST(TxBatteryAlarm, criticalTakesPrecedence)
{
  TxBattAlarmGuard guard;
  g_eeGeneral.vBatWarn = 68;
  g_eeGeneral.vBatCrit = 65;

  // Exactly at the critical voltage -> critical replaces warning.
  g_vbat100mV = 65;
  EXPECT_TRUE(IS_TXBATT_WARNING());   // still inside the warning band
  EXPECT_TRUE(IS_TXBATT_CRITICAL());  // but critical wins
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_CRITICAL);

  // Deep below critical -> still critical, never both.
  g_vbat100mV = 50;
  EXPECT_TRUE(IS_TXBATT_CRITICAL());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_CRITICAL);
}

TEST(TxBatteryAlarm, criticalDisabledFallsBackToWarning)
{
  TxBattAlarmGuard guard;
  g_eeGeneral.vBatWarn = 68;
  g_eeGeneral.vBatCrit = 0;  // disabled

  // Well below the warning voltage but critical is disabled -> warning only,
  // never a critical alert.
  g_vbat100mV = 40;
  EXPECT_TRUE(IS_TXBATT_WARNING());
  EXPECT_FALSE(IS_TXBATT_CRITICAL());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_WARNING);
}

TEST(TxBatteryAlarm, noReadingSuppressesCritical)
{
  TxBattAlarmGuard guard;
  g_eeGeneral.vBatWarn = 68;
  g_eeGeneral.vBatCrit = 65;

  // No usable reading yet (0): the critical alert must be a no-op so a missing
  // reading can never false-alarm RED. Warning stays exactly as it was before
  // (it has always fired from a 0 reading).
  g_vbat100mV = 0;
  EXPECT_FALSE(IS_TXBATT_CRITICAL());
  EXPECT_TRUE(IS_TXBATT_WARNING());
  EXPECT_EQ(getTxBatteryAlarm(), TXBATT_ALARM_WARNING);
}
