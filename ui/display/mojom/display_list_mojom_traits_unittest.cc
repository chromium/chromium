// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_list_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_list.h"
#include "ui/display/mojom/display_list.mojom.h"

namespace {

using mojo::test::SerializeAndDeserialize;

TEST(StructTraitsTest, DisplayListRoundtripEmpty) {
  display::DisplayList i, o;
  ASSERT_TRUE(SerializeAndDeserialize<display::mojom::DisplayList>(i, o));
  EXPECT_EQ(i, o);
}

TEST(StructTraitsTest, DisplayListRoundtripValid) {
  display::DisplayList i, o;
  i = display::DisplayList({display::Display(1)}, /*primary_id=*/1,
                           /*current_id=*/1);
  EXPECT_EQ(i.primary_id(), i.displays()[0].id());
  EXPECT_EQ(i.current_id(), i.displays()[0].id());
  ASSERT_TRUE(SerializeAndDeserialize<display::mojom::DisplayList>(i, o));
  EXPECT_EQ(i, o);
}

// This macro helps test for DEATH via DCHECKs if supported.
#if DCHECK_IS_ON()
#define EXPECT_DCHECK(statement) EXPECT_DEATH_IF_SUPPORTED(statement, "")
#else
#define EXPECT_DCHECK(statement) GTEST_EXECUTE_STATEMENT_(statement, "")
#endif  // DCHECK_IS_ON() for EXPECT_DCHECK

TEST(StructTraitsTest, DisplayListPrimaryMustBeInvalidWhenEmpty) {
  // `primary_id` must be kInvalidDisplayId if `displays` is empty.
  display::mojom::DisplayListPtr i = display::mojom::DisplayList::New();
  display::DisplayList o;
  i->displays = {};
  i->primary_id = 1;
  i->current_id = display::kInvalidDisplayId;
  EXPECT_DCHECK({
    SerializeAndDeserialize<display::mojom::DisplayList>(i, o);
    EXPECT_FALSE(o.IsValid());
  });
}

TEST(StructTraitsTest, DisplayListCurrentMustBeInvalidWhenEmpty) {
  // `current_id` must be kInvalidDisplayId if `displays` is empty.
  display::mojom::DisplayListPtr i = display::mojom::DisplayList::New();
  display::DisplayList o;
  i->displays = {};
  i->primary_id = display::kInvalidDisplayId;
  i->current_id = 1;
  EXPECT_DCHECK({
    SerializeAndDeserialize<display::mojom::DisplayList>(i, o);
    EXPECT_FALSE(o.IsValid());
  });
}

TEST(StructTraitsTest, DisplayListPrimaryIdMustBePresent) {
  // `primary_id` must match an element of `displays`.
  display::mojom::DisplayListPtr i = display::mojom::DisplayList::New();
  display::DisplayList o;
  i->displays = {display::Display(1)};
  i->primary_id = 2;
  i->current_id = display::kInvalidDisplayId;
  EXPECT_NE(i->primary_id, i->displays[0].id());
  EXPECT_DCHECK({
    SerializeAndDeserialize<display::mojom::DisplayList>(i, o);
    EXPECT_FALSE(o.IsValid());
  });
}

TEST(StructTraitsTest, DisplayListCurrentIdMustBePresent) {
  // `current_id` must match an element of `displays`.
  display::mojom::DisplayListPtr i = display::mojom::DisplayList::New();
  display::DisplayList o;
  i->displays = {display::Display(1)};
  i->primary_id = 1;
  i->current_id = 2;
  EXPECT_NE(i->current_id, i->displays[0].id());
  EXPECT_DCHECK({
    SerializeAndDeserialize<display::mojom::DisplayList>(i, o);
    EXPECT_FALSE(o.IsValid());
  });
}

TEST(StructTraitsTest, DisplayListDisplaysIdsMustBeUnique) {
  // `displays` must use unique id values.
  display::mojom::DisplayListPtr i = display::mojom::DisplayList::New();
  display::DisplayList o;
  i->displays = {display::Display(1), display::Display(1)};
  i->primary_id = 1;
  i->current_id = 1;
  EXPECT_EQ(i->displays[0].id(), i->displays[1].id());
  EXPECT_DCHECK({
    SerializeAndDeserialize<display::mojom::DisplayList>(i, o);
    EXPECT_FALSE(o.IsValid());
  });
}

}  // namespace
