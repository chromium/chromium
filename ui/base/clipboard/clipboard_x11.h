// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_X11_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_X11_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

class ClipboardX11 : public Clipboard {
 private:
  friend class Clipboard;

  ClipboardX11();
  ~ClipboardX11() override;

  // Clipboard overrides:
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

  SkBitmap ReadImageInternal(ClipboardBuffer buffer) const;

  // TODO(dcheng): Is this still needed now that each platform clipboard has its
  // own class derived from Clipboard?
  class X11Details;
  std::unique_ptr<X11Details> x11_details_;
  base::flat_map<ClipboardBuffer, std::unique_ptr<DataTransferEndpoint>>
      data_src_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardX11);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_X11_H_
