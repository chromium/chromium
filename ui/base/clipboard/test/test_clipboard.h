// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ui {

// Platform-neutral ui::Clipboard mock used for tests.
class TestClipboard : public Clipboard {
 public:
  TestClipboard();
  TestClipboard(const TestClipboard&) = delete;
  TestClipboard& operator=(const TestClipboard&) = delete;
  ~TestClipboard() override;

  // Creates and associates a TestClipboard with the current thread. When no
  // longer needed, the returned clipboard must be freed by calling
  // Clipboard::DestroyClipboardForCurrentThread() on the same thread.
  static TestClipboard* CreateForCurrentThread();

  // Sets the time to be returned by GetLastModifiedTime();
  void SetLastModifiedTime(const base::Time& time);

  // Clipboard overrides.
  void OnPreShutdown() override;
  std::optional<DataTransferEndpoint> GetSource(
      ClipboardBuffer buffer) const override;
  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const override;
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
  base::Time GetLastModifiedTime() const override;
  void ClearLastModifiedTime() override;
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

 private:
  struct DataStore {
    DataStore();
    DataStore(const DataStore& other);
    DataStore& operator=(const DataStore& other);
    ~DataStore();
    void Clear();
    void SetDataSource(std::optional<DataTransferEndpoint> new_data_src);
    std::optional<DataTransferEndpoint> GetDataSource() const;
    ClipboardSequenceNumberToken sequence_number;
    base::flat_map<ClipboardFormatType, std::string> data;
    std::string url_title;
    std::string html_src_url;
    std::vector<uint8_t> png;
    std::vector<ui::FileInfo> filenames;
    std::optional<DataTransferEndpoint> data_src;
  };

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Used for syncing clipboard sources between Lacros and Ash in ChromeOS.
  void AddClipboardSourceToDataOffer(const ClipboardBuffer buffer);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // In Lacros, retrieves and parses the clipboard source DataTransferEndpoint
  // from the DTE MIME type if no source is provided. In all cases,
  // IsReadAllowed() is called and returned.
  bool MaybeRetrieveSyncedSourceAndCheckIfReadIsAllowed(
      ClipboardBuffer buffer,
      base::optional_ref<const DataTransferEndpoint> data_src,
      const DataTransferEndpoint* data_dst) const;

  // The non-const versions update the sequence number as a side effect.
  const DataStore& GetStore(ClipboardBuffer buffer) const;
  const DataStore& GetDefaultStore() const;
  DataStore& GetStore(ClipboardBuffer buffer);
  DataStore& GetDefaultStore();

  ClipboardBuffer default_store_buffer_;
  mutable base::flat_map<ClipboardBuffer, DataStore> stores_;
  base::Time last_modified_time_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
