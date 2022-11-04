// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_metadata_test_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_types.h"

namespace views::test {

void TestViewMetadata(View* view) {
  ui::metadata::ClassMetaData* meta_data = view->GetClassMetaData();
  EXPECT_NE(meta_data, nullptr);
  for (auto* property : *meta_data) {
    std::u16string value = property->GetValueAsString(view);
    ui::metadata::PropertyFlags flags = property->GetPropertyFlags();
    if (!(flags & ui::metadata::PropertyFlags::kReadOnly) &&
        !!(flags & ui::metadata::PropertyFlags::kSerializable)) {
      property->SetValueAsString(view, value);
    }
  }
}

}  // namespace views::test
