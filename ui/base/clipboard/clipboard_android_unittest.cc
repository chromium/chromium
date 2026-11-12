// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

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

TEST_F(ClipboardAndroidTest, GetAvailableFormats) {
  // Clear the clipboard to ensure a clean state.
  clipboard_->Clear(ClipboardBuffer::kCopyPaste);

  // Write some text to the clipboard.
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"test text");
  }

  std::vector<ClipboardFormatType> requested_formats = {
      ClipboardFormatType::PlainTextType(),
      ClipboardFormatType::UrlType(),
      ClipboardFormatType::HtmlType(),
  };

  base::flat_set<ClipboardFormatType> formats;
  clipboard_->GetAvailableFormats(
      ClipboardBuffer::kCopyPaste, requested_formats, /*data_dst=*/std::nullopt,
      base::BindOnce(
          [](base::flat_set<ClipboardFormatType>* out,
             base::flat_set<ClipboardFormatType> f) { *out = std::move(f); },
          &formats));

  // Verify that GetAvailableFormats correctly filters down to just the
  // requested formats that are actually present.
  EXPECT_TRUE(formats.contains(ClipboardFormatType::PlainTextType()));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::UrlType()));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::HtmlType()));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::FilenamesType()));
}

TEST_F(ClipboardAndroidTest, StaleImageFormatsClearedOnPrimaryClipChanged) {
  clipboard_->Clear(ClipboardBuffer::kCopyPaste);
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    writer.WriteImage(bitmap);
  }

  auto get_all_formats = [&]() {
    base::flat_set<ClipboardFormatType> formats;
    clipboard_->GetAllAvailableFormats(
        ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
        base::BindOnce(
            [](base::flat_set<ClipboardFormatType>* out,
               base::flat_set<ClipboardFormatType> f) { *out = std::move(f); },
            &formats));
    return formats;
  };

  base::flat_set<ClipboardFormatType> formats = get_all_formats();
  EXPECT_TRUE(formats.contains(ClipboardFormatType::PngType()));
  EXPECT_TRUE(formats.contains(ClipboardFormatType::BitmapType()));

  static_cast<ClipboardAndroid*>(clipboard_)->OnPrimaryClipChanged(nullptr);

  // Stale PngType/BitmapType must not be reported while map_ is out of date.
  formats = get_all_formats();
  EXPECT_FALSE(formats.contains(ClipboardFormatType::PngType()));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::BitmapType()));

  // Trigger UpdateFromAndroidClipboard() and verify PngType was erased.
  clipboard_->ReadText(ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
                       base::DoNothing());
  formats = get_all_formats();
  EXPECT_FALSE(formats.contains(ClipboardFormatType::PngType()));
  EXPECT_FALSE(formats.contains(ClipboardFormatType::BitmapType()));
}

TEST_F(ClipboardAndroidTest, ReentrantReadInsideGetSourceCallback) {
  const ClipboardFormatType test_format =
      ClipboardFormatType::CustomPlatformType("chromium/x-test-token");

  // First, verify when clipboard is cleared.
  clipboard_->Clear(ClipboardBuffer::kCopyPaste);
  bool first_callback_ran = false;
  clipboard_->GetSource(
      ClipboardBuffer::kCopyPaste,
      base::BindLambdaForTesting(
          [&](std::optional<DataTransferEndpoint> source) {
            first_callback_ran = true;
            EXPECT_FALSE(source.has_value());

            std::string read_data = "initial";
            clipboard_->ReadData(
                test_format, /*data_dst=*/std::nullopt,
                base::BindLambdaForTesting(
                    [&](std::string data) { read_data = std::move(data); }));
            EXPECT_TRUE(read_data.empty());

            std::u16string read_text = u"initial";
            clipboard_->ReadText(
                ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
                base::BindLambdaForTesting(
                    [&](std::u16string text) { read_text = std::move(text); }));
            EXPECT_TRUE(read_text.empty());
          }));
  EXPECT_TRUE(first_callback_ran);

  // Second, write text, pickled custom data, and a URL DataTransferEndpoint.
  base::Pickle pickle;
  pickle.WriteString("test_payload");
  std::string expected_pickle_bytes(pickle.AsStringView());

  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(GURL("https://example.com")));
    writer.WriteText(u"hello");
    writer.WritePickledData(pickle, test_format);
  }

  bool second_callback_ran = false;
  clipboard_->GetSource(
      ClipboardBuffer::kCopyPaste,
      base::BindLambdaForTesting(
          [&](std::optional<DataTransferEndpoint> source) {
            second_callback_ran = true;
            ASSERT_TRUE(source.has_value());
            EXPECT_TRUE(source->IsUrlType());
            ASSERT_TRUE(source->GetURL());
            EXPECT_EQ(*source->GetURL(), GURL("https://example.com"));

            std::string read_data;
            clipboard_->ReadData(
                test_format, /*data_dst=*/std::nullopt,
                base::BindLambdaForTesting(
                    [&](std::string data) { read_data = std::move(data); }));
            EXPECT_EQ(read_data, expected_pickle_bytes);

            std::u16string read_text;
            clipboard_->ReadText(
                ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
                base::BindLambdaForTesting(
                    [&](std::u16string text) { read_text = std::move(text); }));
            EXPECT_EQ(read_text, u"hello");
          }));
  EXPECT_TRUE(second_callback_ran);
}

TEST_F(ClipboardAndroidTest, GetSourcePreservesOffTheRecord) {
  auto get_source = [&]() {
    std::optional<DataTransferEndpoint> result;
    clipboard_->GetSource(ClipboardBuffer::kCopyPaste,
                          base::BindLambdaForTesting(
                              [&](std::optional<DataTransferEndpoint> src) {
                                result = std::move(src);
                              }));
    return result;
  };

  clipboard_->Clear(ClipboardBuffer::kCopyPaste);
  EXPECT_FALSE(get_source().has_value());

  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(
            GURL("https://incognito.example.com"),
            DataTransferEndpointOptions{.off_the_record = true}));
    writer.WriteText(u"secret");
  }
  std::optional<DataTransferEndpoint> otr_source = get_source();
  ASSERT_TRUE(otr_source.has_value());
  ASSERT_TRUE(otr_source->IsUrlType());
  ASSERT_TRUE(otr_source->GetURL());
  EXPECT_EQ(*otr_source->GetURL(), GURL("https://incognito.example.com"));
  EXPECT_TRUE(otr_source->off_the_record());

  // Subsequent regular write must reset `off_the_record` to false.
  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(
            GURL("https://regular.example.com"),
            DataTransferEndpointOptions{.off_the_record = false}));
    writer.WriteText(u"public");
  }
  std::optional<DataTransferEndpoint> regular_source = get_source();
  ASSERT_TRUE(regular_source.has_value());
  ASSERT_TRUE(regular_source->IsUrlType());
  ASSERT_TRUE(regular_source->GetURL());
  EXPECT_EQ(*regular_source->GetURL(), GURL("https://regular.example.com"));
  EXPECT_FALSE(regular_source->off_the_record());

  // Write without a data source must clear the stored source metadata.
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"no source");
  }
  EXPECT_FALSE(get_source().has_value());

  // Clear() after an off-the-record write must also clear source metadata.
  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(
            GURL("https://incognito2.example.com"),
            DataTransferEndpointOptions{.off_the_record = true}));
    writer.WriteText(u"secret 2");
  }
  EXPECT_TRUE(get_source().has_value());
  clipboard_->Clear(ClipboardBuffer::kCopyPaste);
  EXPECT_FALSE(get_source().has_value());
}

}  // namespace ui
