// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_win.h"

#include <windows.h>

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_hglobal.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ui {

namespace {

class ClipboardWinTest : public PlatformTest, public ClipboardObserver {
 public:
  ClipboardWinTest() { ClipboardMonitor::GetInstance()->AddObserver(this); }

  ~ClipboardWinTest() override {
    ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void TearDown() override { Clipboard::DestroyClipboardForCurrentThread(); }

  void OnClipboardDataChanged() override { ++data_changed_count_; }

  int data_changed_count() const { return data_changed_count_; }

  // Helper method to wait for data_changed_count to reach expected value
  void WaitForDataChangedCount(int expected_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return data_changed_count() == expected_count;
    })) << "Timeout waiting for data_changed_count to reach "
        << expected_count << ", actual count: " << data_changed_count();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  int data_changed_count_ = 0;
};

}  // namespace

TEST_F(ClipboardWinTest, DataChangedNotificationOnWrite) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text");
  }
  WaitForDataChangedCount(1);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(u"html", "https://source.com/");
    writer.WriteSvg(u"svg");
  }
  WaitForDataChangedCount(2);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteRTF("rtf");
  }
  WaitForDataChangedCount(3);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteImage(gfx::test::CreateBitmap(2, 3));
  }
  WaitForDataChangedCount(4);

  Clipboard::GetForCurrentThread()->Clear(ClipboardBuffer::kCopyPaste);
  WaitForDataChangedCount(5);
}

TEST_F(ClipboardWinTest, NoDataChangedNotificationOnRead) {
  auto* clipboard = Clipboard::GetForCurrentThread();

  std::vector<std::u16string> types;
  clipboard->ReadAvailableTypes(ClipboardBuffer::kCopyPaste, nullptr, &types);
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<std::vector<std::u16string>> types_future;
  clipboard->ReadAvailableTypes(ClipboardBuffer::kCopyPaste, std::nullopt,
                                types_future.GetCallback());
  ASSERT_TRUE(types_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  std::u16string text_result;
  clipboard->ReadText(ClipboardBuffer::kCopyPaste, nullptr, &text_result);
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<std::u16string> text_future;
  clipboard->ReadText(ClipboardBuffer::kCopyPaste, std::nullopt,
                      text_future.GetCallback());
  ASSERT_TRUE(text_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  std::string ascii_text_result;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste, nullptr,
                           &ascii_text_result);
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<std::string> ascii_text_future;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste, std::nullopt,
                           ascii_text_future.GetCallback());
  ASSERT_TRUE(ascii_text_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<std::u16string, GURL, uint32_t, uint32_t> html_future;
  clipboard->ReadHTML(ClipboardBuffer::kCopyPaste, std::nullopt,
                      html_future.GetCallback());
  ASSERT_TRUE(html_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  std::u16string html;
  std::string src_url;
  uint32_t start;
  uint32_t end;
  clipboard->ReadHTML(ClipboardBuffer::kCopyPaste, nullptr, &html, &src_url,
                      &start, &end);
  ASSERT_EQ(data_changed_count(), 0);

  std::u16string svg;
  clipboard->ReadSvg(ClipboardBuffer::kCopyPaste, nullptr, &svg);
  ASSERT_EQ(data_changed_count(), 0);

  std::string rtf;
  clipboard->ReadRTF(ClipboardBuffer::kCopyPaste, nullptr, &rtf);
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<const std::vector<uint8_t>&> png_future;
  clipboard->ReadPng(ClipboardBuffer::kCopyPaste, std::nullopt,
                     png_future.GetCallback());
  ASSERT_TRUE(png_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  std::u16string custom_data_result;
  clipboard->ReadDataTransferCustomData(
      ClipboardBuffer::kCopyPaste, u"text/plain", nullptr, &custom_data_result);
  ASSERT_EQ(data_changed_count(), 0);

  std::vector<FileInfo> file_infos;
  clipboard->ReadFilenames(ClipboardBuffer::kCopyPaste, nullptr, &file_infos);
  ASSERT_EQ(data_changed_count(), 0);

  base::test::TestFuture<std::vector<FileInfo>> filenames_future;
  clipboard->ReadFilenames(ClipboardBuffer::kCopyPaste, std::nullopt,
                           filenames_future.GetCallback());
  ASSERT_TRUE(filenames_future.Wait());
  ASSERT_EQ(data_changed_count(), 0);

  std::u16string title;
  std::string bookmark_url;
  clipboard->ReadBookmark(nullptr, &title, &bookmark_url);
  ASSERT_EQ(data_changed_count(), 0);

  std::string data_result;
  clipboard->ReadData(ClipboardFormatType::PlainTextType(), nullptr,
                      &data_result);
  ASSERT_EQ(data_changed_count(), 0);
}

// Test that the ClipboardMonitor sends a notification when data is written to
// the clipboard when platform clipboard monitoring is enabled. With the API
// enabled, the ClipboardMonitor gets notified of clipboard changes via the OS's
// clipboard change notification mechanism. (On Windows, this is done via the
// WM_CLIPBOARDUPDATE message.)
TEST_F(ClipboardWinTest, DataChangedNotificationOnWriteWithClipboardChangeAPI) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text");
  }
  // Since the WM_CLIPBOARDUPDATE message is sent on the same thread, we
  // need to wait for the thread to process the message.
  WaitForDataChangedCount(1);
}

TEST_F(ClipboardWinTest, InvalidBitmapDoesNotCrash) {
  const int kWidth = 1;
  const int kHeight = 1;
  const size_t kHeaderSize = sizeof(BITMAPINFOHEADER);
  const size_t kPixelBytes = 4;

  BITMAPINFOHEADER hdr = {};
  hdr.biSize = sizeof(BITMAPINFOHEADER);
  hdr.biWidth = kWidth;
  hdr.biHeight = kHeight;
  hdr.biPlanes = 1;
  hdr.biBitCount = 0;  // Abnormal value under test.
  hdr.biCompression = BI_RGB;

  std::vector<uint8_t> invalid_bitmap_data(kHeaderSize + kPixelBytes, 0);
  base::as_writable_byte_span(invalid_bitmap_data)
      .first(sizeof(BITMAPINFOHEADER))
      .copy_from(base::byte_span_from_ref(hdr));

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    // Writing an invalid bitmap to the clipboard.
    writer.WriteRawDataForTest(ClipboardFormatType(CF_DIB),
                               std::move(invalid_bitmap_data));
  }

  // Reading PNG should not crash.
  base::test::TestFuture<const std::vector<uint8_t>&> png_future;
  Clipboard::GetForCurrentThread()->ReadPng(
      ClipboardBuffer::kCopyPaste, std::nullopt, png_future.GetCallback());
  ASSERT_TRUE(png_future.Wait());
  const auto& png = png_future.Get();
  ASSERT_GE(png.size(), 0u);

  // Clear invalid data in clipboard.
  Clipboard::GetForCurrentThread()->Clear(ClipboardBuffer::kCopyPaste);
}

TEST_F(ClipboardWinTest, NormalizeRtfStringToUTF8) {
  std::u16string expectedString(u"€");
  std::string encodedString;
  base::UTF16ToCodepage(expectedString, "windows-1252",
                        base::OnStringConversionError::FAIL, &encodedString);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteRTF(encodedString);
  }

  std::string readString;
  Clipboard::GetForCurrentThread()->ReadRTF(ClipboardBuffer::kCopyPaste,
                                            nullptr, &readString);

  std::u16string resultString;
  base::CodepageToUTF16(readString, "UTF-8",
                        base::OnStringConversionError::FAIL, &resultString);

  ASSERT_EQ(resultString, expectedString);

  // Clear data in clipboard.
  Clipboard::GetForCurrentThread()->Clear(ClipboardBuffer::kCopyPaste);
}

TEST_F(ClipboardWinTest, ReadHTMLAsyncReturnsWrittenData) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(u"html_test", "https://source.com/");
  }

  base::test::TestFuture<std::u16string, GURL, uint32_t, uint32_t> html_future;
  clipboard->ReadHTML(ClipboardBuffer::kCopyPaste, std::nullopt,
                      html_future.GetCallback());
  ASSERT_TRUE(html_future.Wait());
  const std::u16string html = html_future.Get<0>();
  EXPECT_EQ(html_future.Get<1>(), GURL("https://source.com/"));
  EXPECT_EQ(html.substr(html_future.Get<2>(),
                        html_future.Get<3>() - html_future.Get<2>()),
            u"html_test");
}

TEST_F(ClipboardWinTest, ReadHTMLAsyncEmptyClipboard) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  clipboard->Clear(ClipboardBuffer::kCopyPaste);

  base::test::TestFuture<std::u16string, GURL, uint32_t, uint32_t> html_future;
  clipboard->ReadHTML(ClipboardBuffer::kCopyPaste, std::nullopt,
                      html_future.GetCallback());
  ASSERT_TRUE(html_future.Wait());
  EXPECT_TRUE(html_future.Get<0>().empty());
  EXPECT_EQ(html_future.Get<1>(), GURL());
  EXPECT_EQ(html_future.Get<2>(), 0u);
  EXPECT_EQ(html_future.Get<3>(), 0u);
}

TEST_F(ClipboardWinTest, ReadFilenamesAsyncReturnsWrittenData) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file));

  auto* clipboard = Clipboard::GetForCurrentThread();
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames(
        ui::FileInfosToURIList({ui::FileInfo(file, base::FilePath())}));
  }

  base::test::TestFuture<std::vector<ui::FileInfo>> filenames_future;
  clipboard->ReadFilenames(ClipboardBuffer::kCopyPaste, std::nullopt,
                           filenames_future.GetCallback());
  ASSERT_TRUE(filenames_future.Wait());
  const auto& filenames = filenames_future.Get();
  ASSERT_EQ(1u, filenames.size());
  EXPECT_EQ(file, filenames[0].path);
}

TEST_F(ClipboardWinTest, ReadFilenamesAsyncEmptyClipboard) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  clipboard->Clear(ClipboardBuffer::kCopyPaste);

  base::test::TestFuture<std::vector<ui::FileInfo>> filenames_future;
  clipboard->ReadFilenames(ClipboardBuffer::kCopyPaste, std::nullopt,
                           filenames_future.GetCallback());
  ASSERT_TRUE(filenames_future.Wait());
  EXPECT_TRUE(filenames_future.Get().empty());
}

TEST_F(ClipboardWinTest, ReadTextAsyncReturnsWrittenData) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text_test");
  }

  base::test::TestFuture<std::u16string> text_future;
  clipboard->ReadText(ClipboardBuffer::kCopyPaste, std::nullopt,
                      text_future.GetCallback());
  ASSERT_TRUE(text_future.Wait());
  EXPECT_EQ(text_future.Get(), u"text_test");
}

TEST_F(ClipboardWinTest, ReadTextAsyncEmptyClipboard) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  clipboard->Clear(ClipboardBuffer::kCopyPaste);

  base::test::TestFuture<std::u16string> text_future;
  clipboard->ReadText(ClipboardBuffer::kCopyPaste, std::nullopt,
                      text_future.GetCallback());
  ASSERT_TRUE(text_future.Wait());
  EXPECT_TRUE(text_future.Get().empty());
}

TEST_F(ClipboardWinTest, ReadAsciiTextAsyncReturnsWrittenData) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text_test");
  }

  base::test::TestFuture<std::string> text_future;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste, std::nullopt,
                           text_future.GetCallback());
  ASSERT_TRUE(text_future.Wait());
  EXPECT_EQ(text_future.Get(), "text_test");
}

TEST_F(ClipboardWinTest, ReadAsciiTextAsyncEmptyClipboard) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  clipboard->Clear(ClipboardBuffer::kCopyPaste);

  base::test::TestFuture<std::string> text_future;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste, std::nullopt,
                           text_future.GetCallback());
  ASSERT_TRUE(text_future.Wait());
  EXPECT_TRUE(text_future.Get().empty());
}

TEST_F(ClipboardWinTest, ReadAvailableTypesAsyncReturnsWrittenData) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text_test");
  }

  base::test::TestFuture<std::vector<std::u16string>> types_future;
  clipboard->ReadAvailableTypes(ClipboardBuffer::kCopyPaste, std::nullopt,
                                types_future.GetCallback());
  ASSERT_TRUE(types_future.Wait());
  const auto& types = types_future.Get();
  EXPECT_NE(std::find(types.begin(), types.end(), kMimeTypePlainText16),
            types.end());
}

TEST_F(ClipboardWinTest, ReadAvailableTypesAsyncEmptyClipboard) {
  auto* clipboard = Clipboard::GetForCurrentThread();
  clipboard->Clear(ClipboardBuffer::kCopyPaste);

  base::test::TestFuture<std::vector<std::u16string>> types_future;
  clipboard->ReadAvailableTypes(ClipboardBuffer::kCopyPaste, std::nullopt,
                                types_future.GetCallback());
  ASSERT_TRUE(types_future.Wait());
  EXPECT_TRUE(types_future.Get().empty());
}

}  // namespace ui
