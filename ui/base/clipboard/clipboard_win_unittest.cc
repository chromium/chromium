// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_win.h"

#include <windows.h>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/win/scoped_hglobal.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
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

  std::u16string text_result;
  clipboard->ReadText(ClipboardBuffer::kCopyPaste, nullptr, &text_result);
  ASSERT_EQ(data_changed_count(), 0);

  std::string ascii_text_result;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste, nullptr,
                           &ascii_text_result);
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
  clipboard->ReadPng(ClipboardBuffer::kCopyPaste, nullptr,
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
  Clipboard::GetForCurrentThread()->ReadPng(ClipboardBuffer::kCopyPaste,
                                            nullptr, png_future.GetCallback());
  ASSERT_TRUE(png_future.Wait());
  const auto& png = png_future.Get();
  ASSERT_GE(png.size(), 0u);

  // Clear invalid data in clipboard.
  Clipboard::GetForCurrentThread()->Clear(ClipboardBuffer::kCopyPaste);
}

}  // namespace ui
