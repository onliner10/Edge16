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

#if defined(COLORLCD)

#include "mru_list.h"

// Item 2: most-recently-used ordering for long pickers.

TEST(MRUList, StartsEmpty)
{
  MRUList mru;
  EXPECT_EQ(mru.size(), 0);
  EXPECT_FALSE(mru.contains(5));
}

TEST(MRUList, MostRecentGoesToFront)
{
  MRUList mru;
  mru.touch(10);
  mru.touch(20);
  mru.touch(30);

  ASSERT_EQ(mru.size(), 3);
  EXPECT_EQ(mru[0], 30);
  EXPECT_EQ(mru[1], 20);
  EXPECT_EQ(mru[2], 10);
}

TEST(MRUList, ReTouchingMovesToFrontWithoutDuplicating)
{
  MRUList mru;
  mru.touch(10);
  mru.touch(20);
  mru.touch(30);
  mru.touch(10);  // re-select an existing value

  ASSERT_EQ(mru.size(), 3);
  EXPECT_EQ(mru[0], 10);
  EXPECT_EQ(mru[1], 30);
  EXPECT_EQ(mru[2], 20);
  EXPECT_TRUE(mru.contains(10));
  EXPECT_TRUE(mru.contains(20));
  EXPECT_TRUE(mru.contains(30));
}

TEST(MRUList, IsCappedAtCapacityEvictingOldest)
{
  MRUList mru;
  mru.touch(1);
  mru.touch(2);
  mru.touch(3);
  mru.touch(4);  // should evict the oldest (1)

  ASSERT_EQ(mru.size(), MRUList::CAPACITY);
  EXPECT_EQ(mru.size(), 3);
  EXPECT_EQ(mru[0], 4);
  EXPECT_EQ(mru[1], 3);
  EXPECT_EQ(mru[2], 2);
  EXPECT_FALSE(mru.contains(1));  // evicted
}

TEST(MRUList, ReTouchingFrontIsStable)
{
  MRUList mru;
  mru.touch(7);
  mru.touch(7);

  ASSERT_EQ(mru.size(), 1);
  EXPECT_EQ(mru[0], 7);
}

TEST(MRUList, ClearEmptiesList)
{
  MRUList mru;
  mru.touch(1);
  mru.touch(2);
  mru.clear();

  EXPECT_EQ(mru.size(), 0);
  EXPECT_FALSE(mru.contains(1));
  EXPECT_FALSE(mru.contains(2));
}

#endif  // COLORLCD
