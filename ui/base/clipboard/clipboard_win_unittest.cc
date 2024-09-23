// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_win.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
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
  ASSERT_EQ(data_changed_count(), 1);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(u"html", "https://source.com/");
    writer.WriteSvg(u"svg");
  }
  ASSERT_EQ(data_changed_count(), 2);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteRTF("rtf");
  }
  ASSERT_EQ(data_changed_count(), 3);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteImage(gfx::test::CreateBitmap(2, 3));
  }
  ASSERT_EQ(data_changed_count(), 4);

  Clipboard::GetForCurrentThread()->Clear(ClipboardBuffer::kCopyPaste);
  ASSERT_EQ(data_changed_count(), 5);
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

}  // namespace ui
