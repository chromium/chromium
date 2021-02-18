// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_

#include <stddef.h>
#include <stdint.h>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/clipboard/clipboard.h"

@class NSPasteboard;

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardMac : public Clipboard {
 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardMacTest, ReadImageRetina);
  FRIEND_TEST_ALL_PREFIXES(ClipboardMacTest, ReadImageNonRetina);
  FRIEND_TEST_ALL_PREFIXES(ClipboardMacTest, EmptyImage);
  FRIEND_TEST_ALL_PREFIXES(ClipboardMacTest, PDFImage);
  friend class Clipboard;

  ClipboardMac();
  ~ClipboardMac() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  DataTransferEndpoint* GetSource(ClipboardBuffer buffer) const override;
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst) const override;
  bool IsMarkedByOriginatorAsConfidential() const override;
  void MarkAsConfidential() override;
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

  SkBitmap ReadImageInternal(ClipboardBuffer buffer,
                             NSPasteboard* pasteboard) const;

  DISALLOW_COPY_AND_ASSIGN(ClipboardMac);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_
