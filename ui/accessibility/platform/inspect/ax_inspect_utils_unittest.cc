// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"

#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXInspectUtilsTest, FormatString) {
  base::Value value("hey");
  EXPECT_EQ(AXFormatValue(value), std::string("'hey'"));
}

TEST(AXInspectUtilsTest, FormatConst) {
  base::Value value(AXMakeConst("hey"));
  EXPECT_EQ(AXFormatValue(value), std::string("hey"));
}

TEST(AXInspectUtilsTest, FormatInteger) {
  base::Value value(3);
  EXPECT_EQ(AXFormatValue(value), std::string("3"));
}

TEST(AXInspectUtilsTest, FormatDouble) {
  base::Value value(3.3);
  EXPECT_EQ(AXFormatValue(value), std::string("3.3"));
}

TEST(AXInspectUtilsTest, FormatList) {
  base::Value list(base::Value::Type::LIST);
  list.Append("item1");
  list.Append("item2");
  EXPECT_EQ(AXFormatValue(list), std::string("['item1', 'item2']"));
}

TEST(AXInspectUtilsTest, FormatDict) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringPath("anchor", AXMakeConst("textbox"));
  dict.SetIntPath("offset", 2);
  dict.SetStringPath("affinity", AXMakeConst("down"));
  EXPECT_EQ(AXFormatValue(dict),
            std::string("{affinity: down, anchor: textbox, offset: 2}"));
}

TEST(AXInspectUtilsTest, FormatSet) {
  base::Value set(base::Value::Type::DICTIONARY);
  set.SetStringPath(AXMakeSetKey("index1_anchor"), AXMakeConst(":1"));
  set.SetIntPath(AXMakeSetKey("index2_offset"), 2);
  set.SetStringPath(AXMakeSetKey("index3_affinity"), AXMakeConst("down"));
  EXPECT_EQ(AXFormatValue(set), std::string("{:1, 2, down}"));
}

TEST(AXInspectUtilsTest, FormatOrderedDict) {
  base::Value ordered_dict(base::Value::Type::DICTIONARY);
  ordered_dict.SetIntPath(AXMakeOrderedKey("w", 0), 40);
  ordered_dict.SetIntPath(AXMakeOrderedKey("h", 1), 30);
  EXPECT_EQ(AXFormatValue(ordered_dict), std::string("{w: 40, h: 30}"));
}

}  // namespace ui
