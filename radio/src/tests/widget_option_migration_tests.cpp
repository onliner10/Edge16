/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Proves the widget-option migration is positionally safe: retiring a stock
 * widget's Color option (now a hidden WidgetOption::Deprecated placeholder)
 * MUST NOT shift the remaining options for models already saved before the
 * change. An old positional record with a Color value at its index must load
 * with shadow/align intact and the legacy colour dropped.
 */

#include "gtests.h"

#if defined(COLORLCD)

#include "mainview/widget.h"

// Reconciles a persistent-data record against a factory's option definitions
// exactly like WidgetFactory::create() does on model load (init=false).
static void reconcileOnLoad(WidgetPersistentData& data, const WidgetOption* opts)
{
  int i = 0;
  for (const WidgetOption* o = opts; o->name; o++, i++) data.setDefault(i, o, false);
}

TEST(WidgetOptionMigration, valueWidgetKeepsShadowAndAlignAcrossColorRemoval)
{
  const WidgetFactory* factory = WidgetFactory::getWidgetFactory("Value");
  ASSERT_NE(factory, nullptr);
  const WidgetOption* opts = factory->getDefaultOptions();

  // Confirm the retired Color slot is now a hidden Deprecated placeholder at
  // its ORIGINAL index (1), so positions 2..4 are preserved.
  ASSERT_EQ(opts[1].type, WidgetOption::Deprecated);
  EXPECT_STREQ(opts[1].name, "");

  // Craft an OLD-format saved Value widget (pre-change positional layout):
  //   [0] Source, [1] Color, [2] Bool(shadow), [3] Align(label), [4] Align(value)
  WidgetPersistentData data;
  data.setType(0, WOV_Source);   data.setUnsignedValue(0, 42 /*a source*/);
  data.setType(1, WOV_Color);    data.setUnsignedValue(1, 0x8000000E /*COLIDX14*/);
  data.setType(2, WOV_Bool);     data.setBoolValue(2, true /*shadow ON*/);
  data.setType(3, WOV_Unsigned); data.setUnsignedValue(3, 2 /*ALIGN_RIGHT*/);
  data.setType(4, WOV_Unsigned); data.setUnsignedValue(4, 1 /*ALIGN_CENTER*/);

  reconcileOnLoad(data, opts);

  // Source is untouched.
  EXPECT_EQ(data.getType(0), WOV_Source);
  EXPECT_EQ(data.getUnsignedValue(0), 42u);

  // The Color slot is retyped to the Deprecated placeholder (Unsigned) and its
  // legacy colour value is dropped/reset — it is ignored on next save.
  EXPECT_EQ(data.getType(1), WOV_Unsigned);
  EXPECT_EQ(data.getUnsignedValue(1), 0u);

  // CRITICAL: shadow + both alignments keep their positions and values.
  EXPECT_EQ(data.getType(2), WOV_Bool);
  EXPECT_TRUE(data.getBoolValue(2));
  EXPECT_EQ(data.getType(3), WOV_Unsigned);
  EXPECT_EQ(data.getUnsignedValue(3), 2u);
  EXPECT_EQ(data.getType(4), WOV_Unsigned);
  EXPECT_EQ(data.getUnsignedValue(4), 1u);
}

TEST(WidgetOptionMigration, textWidgetKeepsTrailingOptionsAcrossColorRemoval)
{
  const WidgetFactory* factory = WidgetFactory::getWidgetFactory("Text");
  ASSERT_NE(factory, nullptr);
  const WidgetOption* opts = factory->getDefaultOptions();
  ASSERT_EQ(opts[1].type, WidgetOption::Deprecated);  // retired Color slot

  // Old layout: [0] String, [1] Color, [2] TextSize, [3] Bool(shadow), [4] Align
  WidgetPersistentData data;
  data.setType(0, WOV_String);   data.setString(0, "HELLO");
  data.setType(1, WOV_Color);    data.setUnsignedValue(1, 0x8000000E);
  data.setType(2, WOV_Unsigned); data.setUnsignedValue(2, 3 /*font size*/);
  data.setType(3, WOV_Bool);     data.setBoolValue(3, true /*shadow*/);
  data.setType(4, WOV_Unsigned); data.setUnsignedValue(4, 2 /*align*/);

  reconcileOnLoad(data, opts);

  EXPECT_EQ(data.getType(0), WOV_String);
  EXPECT_EQ(data.getString(0), std::string("HELLO"));
  EXPECT_EQ(data.getType(1), WOV_Unsigned);  // colour dropped
  EXPECT_EQ(data.getUnsignedValue(2), 3u);   // font size intact
  EXPECT_TRUE(data.getBoolValue(3));         // shadow intact
  EXPECT_EQ(data.getUnsignedValue(4), 2u);   // align intact
}

// ---------------------------------------------------------------------------
// Outputs widget: legacy 5-option layout migration (insertion, not just
// removal). Before the 'last channel' option existed, the positional layout
// was:
//   [0] firstChan, [1] fillBackground (Bool), [2] bgColor, [3] textColor,
//   [4] color (the latter three are now retired Deprecated placeholders).
// The current layout inserts 'lastChan' (Signed) at index 1, which requires
// SHIFTING every entry from the old index 1 onward up by one slot rather
// than just retyping in place. OutputsWidgetFactory::checkOptions() does
// this shift; a real user model saved under semver 2.12.0 with exactly this
// 5-entry shape (options[1].type == Bool) previously triggered an
// out-of-bounds vector write (options[5] with no bounds check that the
// vector was ever grown to hold 6 entries) that SIGSEGV'd the radio on
// model load.
//
// checkOptions() reads its persistent-data record via a WidgetLocation,
// which is only backed by g_model / g_eeGeneral, so these tests exercise it
// against a scratch main-view zone (screen 0, zone 0) rather than a
// stack-local WidgetPersistentData.
TEST(WidgetOptionMigration, outputsWidgetShiftsLegacyFiveOptionLayout)
{
  const WidgetFactory* factory = WidgetFactory::getWidgetFactory("Outputs");
  ASSERT_NE(factory, nullptr);

  WidgetLocation loc{MainViewWidgetLocation{0, 0}};
  WidgetPersistentData* data = loc.persistentData();
  ASSERT_NE(data, nullptr);
  data->clear();

  // Old 5-entry positional layout, exactly as a pre-'last channel' model
  // would have saved it (firstChan is WidgetOption::Integer -> WOV_Signed,
  // matching real 2.12.0-era YAML: "type: Signed, value: signedValue: N").
  data->setType(0, WOV_Signed);    data->setSignedValue(0, 3);              // firstChan = 3
  data->setType(1, WOV_Bool);      data->setBoolValue(1, true);             // fillBackground = ON
  data->setType(2, WOV_Color);     data->setUnsignedValue(2, 0x8000000A);   // bgColor (retired)
  data->setType(3, WOV_Color);     data->setUnsignedValue(3, 0x8000000B);   // textColor (retired)
  data->setType(4, WOV_Color);     data->setUnsignedValue(4, 0x8000000C);   // color (retired)

  factory->checkOptions(loc);

  ASSERT_EQ(data->options.size(), 6u);

  // firstChan is untouched.
  EXPECT_EQ(data->getType(0), WOV_Signed);
  EXPECT_EQ(data->getSignedValue(0), 3);

  // The new 'last channel' slot lands at index 1 with its default value.
  EXPECT_EQ(data->getType(1), WOV_Signed);
  EXPECT_EQ(data->getSignedValue(1), MAX_OUTPUT_CHANNELS);

  // fillBackground's value migrates from index 1 to index 2, unchanged.
  EXPECT_EQ(data->getType(2), WOV_Bool);
  EXPECT_TRUE(data->getBoolValue(2));

  // The three retired colour options shift from [2,3,4] to [3,4,5],
  // keeping their original (now-ignored) values rather than being lost.
  EXPECT_EQ(data->getType(3), WOV_Color);
  EXPECT_EQ(data->getUnsignedValue(3), 0x8000000Au);
  EXPECT_EQ(data->getType(4), WOV_Color);
  EXPECT_EQ(data->getUnsignedValue(4), 0x8000000Bu);
  EXPECT_EQ(data->getType(5), WOV_Color);
  EXPECT_EQ(data->getUnsignedValue(5), 0x8000000Cu);

  // A subsequent reconcile pass (as WidgetFactory::create() runs after
  // checkOptions()) must land on the canonical 6-option layout: firstChan,
  // lastChan, fillBackground kept, retired slots reset to the Deprecated
  // placeholder default.
  const WidgetOption* opts = factory->getDefaultOptions();
  int i = 0;
  for (const WidgetOption* o = opts; o->name; o++, i++) data->setDefault(i, o, false);

  EXPECT_EQ(data->getSignedValue(0), 3);
  EXPECT_EQ(data->getSignedValue(1), MAX_OUTPUT_CHANNELS);
  EXPECT_TRUE(data->getBoolValue(2));

  data->clear();
}

TEST(WidgetOptionMigration, outputsWidgetMigratesUndersizedLegacyLayoutWithoutCrashing)
{
  const WidgetFactory* factory = WidgetFactory::getWidgetFactory("Outputs");
  ASSERT_NE(factory, nullptr);

  WidgetLocation loc{MainViewWidgetLocation{0, 0}};
  WidgetPersistentData* data = loc.persistentData();
  ASSERT_NE(data, nullptr);
  data->clear();

  // A shorter-than-full legacy record (only firstChan + fillBackground;
  // trailing default-valued retired options trimmed on save). The
  // pre-fix code's `options.size() >= 4` guard let this size (2) skip the
  // migration entirely and would also have OOB-read/written on a
  // size-4 vector; this must neither crash nor silently drop
  // fillBackground.
  data->setType(0, WOV_Signed);    data->setSignedValue(0, 1);      // firstChan = 1
  data->setType(1, WOV_Bool);      data->setBoolValue(1, true);     // fillBackground = ON

  factory->checkOptions(loc);

  // firstChan untouched, lastChan default inserted, fillBackground shifted
  // to index 2 with its value preserved.
  EXPECT_EQ(data->getSignedValue(0), 1);
  EXPECT_EQ(data->getType(1), WOV_Signed);
  EXPECT_EQ(data->getSignedValue(1), MAX_OUTPUT_CHANNELS);
  EXPECT_EQ(data->getType(2), WOV_Bool);
  EXPECT_TRUE(data->getBoolValue(2));

  data->clear();
}

#endif  // COLORLCD
