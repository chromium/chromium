// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <memory>
#include <string>

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace ui {

// Regression test for https://crbug.com/1284996.
TEST(WaylandExchangeDataProviderTest, ExtractPickledData) {
  WaylandExchangeDataProvider provider;
  std::string extracted;

  EXPECT_FALSE(provider.ExtractData(kMimeTypeText, &extracted));
  EXPECT_FALSE(provider.ExtractData(kMimeTypeWebCustomData, &extracted));

  extracted.clear();
  provider.SetString(u"dnd-string");
  EXPECT_TRUE(provider.ExtractData(kMimeTypeText, &extracted));
  EXPECT_EQ("dnd-string", extracted);

  extracted.clear();
  base::Pickle pickle;
  pickle.WriteString("pickled-str");
  provider.SetPickledData(ClipboardFormatType::WebCustomDataType(), pickle);
  EXPECT_TRUE(provider.ExtractData(kMimeTypeWebCustomData, &extracted));

  // Ensure Pickle "reconstruction" works as expected.
  std::string read_pickled_str;
  base::Pickle read_pickle(reinterpret_cast<const char*>(extracted.data()),
                           extracted.size());
  base::PickleIterator iter(read_pickle);
  ASSERT_TRUE(read_pickle.data());
  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadString(&read_pickled_str));
  EXPECT_EQ("pickled-str", read_pickled_str);
}

}  // namespace ui
