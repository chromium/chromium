// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
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
  const ClipboardData* GetClipboardData(
      DataTransferEndpoint* data_dst,
      ClipboardBuffer buffer = ClipboardBuffer::kCopyPaste) const;

  // Writes the current ClipboardData and returns the previous data.
  // The data source is expected to be set in `data`.
  std::unique_ptr<ClipboardData> WriteClipboardData(
      std::unique_ptr<ClipboardData> data,
      ClipboardBuffer buffer = ClipboardBuffer::kCopyPaste);

  // Clipboard overrides:
  std::optional<DataTransferEndpoint> GetSource(
      ClipboardBuffer buffer) const override;
  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const override;

  int NumImagesEncodedForTesting(
      ClipboardBuffer buffer = ClipboardBuffer::kCopyPaste) const;

 private:
  friend class Clipboard;
  friend class ClipboardNonBackedTestBase;
  friend class headless::HeadlessClipboard;
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, PlainText);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, BookmarkURL);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, TextURIList);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, ImageEncoding);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, EncodeImageOnce);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, EncodeMultipleImages);
  FRIEND_TEST_ALL_PREFIXES(ClipboardNonBackedTest, ClipboardBufferTypes);
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
  void ReadDataTransferCustomData(ClipboardBuffer buffer,
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
      std::unique_ptr<DataTransferEndpoint> data_src,
      uint32_t privacy_types) override;
  void WriteText(std::string_view text) override;
  void WriteHTML(std::string_view markup,
                 std::optional<std::string_view> source_url) override;
  void WriteSvg(std::string_view markup) override;
  void WriteRTF(std::string_view rtf) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(std::string_view title, std::string_view url) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 base::span<const uint8_t> data) override;
  void WriteClipboardHistory() override;
  void WriteUploadCloudClipboard() override;
  void WriteConfidentialDataForPassword() override;

  const ClipboardInternal& GetInternalClipboard(ClipboardBuffer buffer) const;
  ClipboardInternal& GetInternalClipboard(ClipboardBuffer buffer);

  base::flat_map<ClipboardBuffer, std::unique_ptr<ClipboardInternal>>
      internal_clipboards_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
