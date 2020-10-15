// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_metadata_test_utils.h"

#include "base/strings/string16.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/metadata/metadata_types.h"

namespace views {
namespace test {

void TestViewMetadata(View* view) {
  metadata::ClassMetaData* meta_data = view->GetClassMetaData();
  EXPECT_NE(meta_data, nullptr);
  for (auto* property : *meta_data) {
    base::string16 value = property->GetValueAsString(view);
    metadata::PropertyFlags flags = property->GetPropertyFlags();
    if (!(flags & metadata::PropertyFlags::kReadOnly) &&
        !!(flags & metadata::PropertyFlags::kSerializable)) {
      property->SetValueAsString(view, value);
    }
  }
}

}  // namespace test
}  // namespace views
