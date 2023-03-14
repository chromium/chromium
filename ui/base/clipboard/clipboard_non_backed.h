// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_

#include <stddef.h>
#include <stdint.h>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"

namespace headless {
class HeadlessClipboard;
}

namespace ui {

class ClipboardData;
class ClipboardInternal;

// In-memory clipboard implementation not backed by an underlying platform.
// This clipboard can be used where there's no need to sync the clipboard with
// an underlying platform, and can substitute platform clipboards like
// ClipboardWin on Windows or ClipboardMac on MacOS. As this isn't backed by an
// underlying platform, the clipboard data isn't persisted after an instance
// goes away.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardNonBacked
    : public Clipboard {
 public:
  // Returns the in-memory clipboard for the current thread. This
  // method must *only* be used when the caller is sure that the clipboard for
  // the current thread is in fact an instance of ClipboardNonBacked.
  static ClipboardNonBacked* GetForCurrentThread();

  ClipboardNonBacked(const ClipboardNonBacked&) = delete;
  ClipboardNonBacked& operator=(const ClipboardNonBacked&) = delete;

  // Returns the current ClipboardData.
  const ClipboardData* GetClipboardData(DataTransferEndpoint* data_dst) const;

  // Writes the current ClipboardData and returns the previous data.
  // The data source is expected to be set in `data`.
  std::unique_ptr<ClipboardData> WriteClipboardData(
      std::unique_ptr<ClipboardData> data);

  // Clipboard overrides:
  DataTransferEndpoint* GetSource(ClipboardBuffer buffer) const override;
  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const override;

  int NumImagesEncodedForTesting() const;

 private:
  friend class Clipboard;
  friend class ClipboardNonBackedTestBase;
  friend class headless::HeadlessClipboard;
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, TextURIList);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, ImageEncoding);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, EncodeImageOnce);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, EncodeMultipleImages);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedMockTimeTest,
                           RecordsTimeIntervalBetweenCommitAndRead);
  ClipboardNonBacked();
  ~ClipboardNonBacked() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  std::vector<std::u16string> GetStandardFormats(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst,
                          std::vector<std::u16string>* types) const override;
  void ReadText(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::u16string* result) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::string* result) const override;
  void ReadPng(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               ReadPngCallback callback) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const std::u16string& type,
                      const DataTransferEndpoint* data_dst,
                      std::u16string* result) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::vector<ui::FileInfo>* result) const override;
  void ReadBookmark(const DataTransferEndpoint* data_dst,
                    std::u16string* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const DataTransferEndpoint* data_dst,
                std::string* result) const override;
#if BUILDFLAG(IS_OZONE)
  bool IsSelectionBufferAvailable() const override;
#endif  // BUILDFLAG(IS_OZONE)
  void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteUnsanitizedHTML(const char* markup_data,
                            size_t markup_len,
                            const char* url_data,
                            size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;

  const std::unique_ptr<ClipboardInternal> clipboard_internal_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
