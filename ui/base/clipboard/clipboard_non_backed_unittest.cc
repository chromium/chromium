// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/ui_base_features.h"

namespace ui {
namespace {

std::vector<std::string> UTF8Types(std::vector<base::string16> types) {
  std::vector<std::string> result;
  for (const base::string16& type : types)
    result.push_back(base::UTF16ToUTF8(type));
  return result;
}

}  // namespace

class ClipboardNonBackedTest : public testing::Test {
 public:
  ClipboardNonBackedTest() = default;
  ClipboardNonBackedTest(const ClipboardNonBackedTest&) = delete;
  ClipboardNonBackedTest& operator=(const ClipboardNonBackedTest&) = delete;
  ~ClipboardNonBackedTest() override = default;

  ClipboardNonBacked* clipboard() { return &clipboard_; }

 private:
  ClipboardNonBacked clipboard_;
};

// Verifies that GetClipboardData() returns the same instance of ClipboardData
// as was written via WriteClipboardData().
TEST_F(ClipboardNonBackedTest, WriteAndGetClipboardData) {
  auto clipboard_data = std::make_unique<ClipboardData>();

  auto* expected_clipboard_data_ptr = clipboard_data.get();
  clipboard()->WriteClipboardData(std::move(clipboard_data));
  auto* actual_clipboard_data_ptr = clipboard()->GetClipboardData(nullptr);

  EXPECT_EQ(expected_clipboard_data_ptr, actual_clipboard_data_ptr);
}

// Verifies that WriteClipboardData() writes a ClipboardData instance to the
// clipboard and returns the previous instance.
TEST_F(ClipboardNonBackedTest, WriteClipboardData) {
  auto first_data = std::make_unique<ClipboardData>();
  auto second_data = std::make_unique<ClipboardData>();

  auto* first_data_ptr = first_data.get();
  auto* second_data_ptr = second_data.get();

  auto previous_data = clipboard()->WriteClipboardData(std::move(first_data));
  EXPECT_EQ(previous_data.get(), nullptr);

  previous_data = clipboard()->WriteClipboardData(std::move(second_data));

  EXPECT_EQ(first_data_ptr, previous_data.get());
  EXPECT_EQ(second_data_ptr, clipboard()->GetClipboardData(nullptr));
}

// Verifies that directly writing to ClipboardInternal does not result in
// histograms being logged. This is used by ClipboardHistoryController to
// manipulate the clipboard in order to facilitate pasting from clipboard
// history.
TEST_F(ClipboardNonBackedTest, AdminWriteDoesNotRecordHistograms) {
  base::HistogramTester histogram_tester;
  auto data = std::make_unique<ClipboardData>();
  data->set_text("test");

  auto* data_ptr = data.get();

  // Write the data to the clipboard, no histograms should be recorded.
  clipboard()->WriteClipboardData(std::move(data));
  EXPECT_EQ(data_ptr, clipboard()->GetClipboardData(/*data_dst=*/nullptr));

  histogram_tester.ExpectTotalCount("Clipboard.Read", 0);
  histogram_tester.ExpectTotalCount("Clipboard.Write", 0);
}

// Tests that site bookmark URLs are accessed as text, and
// IsFormatAvailable('text/uri-list') is only true for files.
TEST_F(ClipboardNonBackedTest, TextURIList) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kClipboardFilenames}, {});
  EXPECT_EQ("text/uri-list", ClipboardFormatType::GetFilenamesType().GetName());

  auto data = std::make_unique<ClipboardData>();
  data->set_bookmark_url("http://example.com");
  clipboard()->WriteClipboardData(std::move(data));
  std::vector<base::string16> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);

  // With bookmark data, available types should be only 'text/plain'.
  EXPECT_EQ(std::vector<std::string>({"text/plain"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(ClipboardFormatType::GetUrlType(),
                                             ClipboardBuffer::kCopyPaste,
                                             /*data_dst=*/nullptr));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::GetFilenamesType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  // With filenames data, available types should be 'text/uri-list'.
  data = std::make_unique<ClipboardData>();
  data->set_filenames({FileInfo(base::FilePath("/path"), base::FilePath())});
  clipboard()->WriteClipboardData(std::move(data));
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"text/uri-list"}), UTF8Types(types));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(ClipboardFormatType::GetUrlType(),
                                              ClipboardBuffer::kCopyPaste,
                                              /*data_dst=*/nullptr));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::GetFilenamesType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
}

}  // namespace ui
