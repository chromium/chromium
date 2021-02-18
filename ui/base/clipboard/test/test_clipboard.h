// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ui {

// Platform-neutral ui::Clipboard mock used for tests.
class TestClipboard : public Clipboard {
 public:
  TestClipboard();
  ~TestClipboard() override;

  // Creates and associates a TestClipboard with the current thread. When no
  // longer needed, the returned clipboard must be freed by calling
  // Clipboard::DestroyClipboardForCurrentThread() on the same thread.
  static TestClipboard* CreateForCurrentThread();

  // Sets the time to be returned by GetLastModifiedTime();
  void SetLastModifiedTime(const base::Time& time);

  // Clipboard overrides.
  void OnPreShutdown() override;
  DataTransferEndpoint* GetSource(ClipboardBuffer buffer) const override;
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst,
                          std::vector<base::string16>* types) const override;
  std::vector<base::string16> ReadAvailablePlatformSpecificFormatNames(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const override;
  void ReadText(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                base::string16* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               base::string16* result) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::string* result) const override;
  void ReadImage(ClipboardBuffer buffer,
                 const DataTransferEndpoint* data_dst,
                 ReadImageCallback callback) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const base::string16& type,
                      const DataTransferEndpoint* data_dst,
                      base::string16* result) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::vector<ui::FileInfo>* result) const override;
  void ReadBookmark(const DataTransferEndpoint* data_dst,
                    base::string16* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const DataTransferEndpoint* data_dst,
                std::string* result) const override;
  base::Time GetLastModifiedTime() const override;
  void ClearLastModifiedTime() override;
#if defined(USE_OZONE)
  bool IsSelectionBufferAvailable() const override;
#endif  // defined(USE_OZONE)
  void WritePortableRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
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

 private:
  struct DataStore {
    DataStore();
    DataStore(const DataStore& other);
    DataStore& operator=(const DataStore& other);
    ~DataStore();
    void Clear();
    void SetDataSource(std::unique_ptr<DataTransferEndpoint> data_src);
    DataTransferEndpoint* GetDataSource() const;
    uint64_t sequence_number = 0;
    base::flat_map<ClipboardFormatType, std::string> data;
    std::string url_title;
    std::string html_src_url;
    SkBitmap image;
    std::vector<ui::FileInfo> filenames;
    std::unique_ptr<DataTransferEndpoint> data_src = nullptr;
  };

  // The non-const versions increment the sequence number as a side effect.
  const DataStore& GetStore(ClipboardBuffer buffer) const;
  const DataStore& GetDefaultStore() const;
  DataStore& GetStore(ClipboardBuffer buffer);
  DataStore& GetDefaultStore();

  ClipboardBuffer default_store_buffer_;
  mutable base::flat_map<ClipboardBuffer, DataStore> stores_;
  base::Time last_modified_time_;

  DISALLOW_COPY_AND_ASSIGN(TestClipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
