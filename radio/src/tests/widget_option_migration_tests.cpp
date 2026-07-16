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

#endif  // COLORLCD
