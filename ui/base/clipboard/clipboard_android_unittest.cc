// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace ui {

class ClipboardAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    clipboard_ = Clipboard::GetForCurrentThread();
    ASSERT_TRUE(clipboard_);
  }

  raw_ptr<Clipboard> clipboard_;
};

TEST_F(ClipboardAndroidTest, EraseCustomDataTest) {
  std::string test_data = "test_custom_data";
  std::string encoded_data = base::Base64Encode(test_data);
  SetCustomClipDataForTesting(encoded_data);
  std::u16string type = u"text/custom";

  clipboard_->ReadDataTransferCustomData(ClipboardBuffer::kCopyPaste, type,
                                         /*data_dst=*/std::nullopt,
                                         base::DoNothing());

  // Verify the clipboard map now contains the custom data type.
  base::flat_set<ClipboardFormatType> formats;
  clipboard_->GetAllAvailableFormats(
      ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
      base::BindOnce(
          [](base::flat_set<ClipboardFormatType>* out,
             base::flat_set<ClipboardFormatType> f) { *out = std::move(f); },
          &formats));
  EXPECT_TRUE(formats.contains(ClipboardFormatType::DataTransferCustomType()));

  // Clear the custom clip data and notify the clipboard of the change.
  SetCustomClipDataForTesting(std::nullopt);
  static_cast<ClipboardAndroid*>(clipboard_)->OnPrimaryClipChanged(nullptr);

  clipboard_->ReadDataTransferCustomData(ClipboardBuffer::kCopyPaste, type,
                                         /*data_dst=*/std::nullopt,
                                         base::DoNothing());

  formats.clear();
  clipboard_->GetAllAvailableFormats(
      ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
      base::BindOnce(
          [](base::flat_set<ClipboardFormatType>* out,
             base::flat_set<ClipboardFormatType> f) { *out = std::move(f); },
          &formats));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::DataTransferCustomType()));
}

}  // namespace ui
