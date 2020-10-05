// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

// ClipboardOzone is not yet shipped in production. It is a work in progress
// for desktop Linux Wayland support.
class ClipboardOzone : public Clipboard {
 private:
  friend class Clipboard;

  ClipboardOzone();
  ~ClipboardOzone() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const ClipboardDataEndpoint* data_dst) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const ClipboardDataEndpoint* data_dst,
                          std::vector<base::string16>* types) const override;
  std::vector<base::string16> ReadAvailablePlatformSpecificFormatNames(
      ClipboardBuffer buffer,
      const ClipboardDataEndpoint* data_dst) const override;
  void ReadText(ClipboardBuffer buffer,
                const ClipboardDataEndpoint* data_dst,
                base::string16* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const ClipboardDataEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const ClipboardDataEndpoint* data_dst,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const ClipboardDataEndpoint* data_dst,
               base::string16* result) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const ClipboardDataEndpoint* data_dst,
               std::string* result) const override;
  void ReadImage(ClipboardBuffer buffer,
                 const ClipboardDataEndpoint* data_dst,
                 ReadImageCallback callback) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const base::string16& type,
                      const ClipboardDataEndpoint* data_dst,
                      base::string16* result) const override;
  void ReadBookmark(const ClipboardDataEndpoint* data_dst,
                    base::string16* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const ClipboardDataEndpoint* data_dst,
                std::string* result) const override;
  bool IsSelectionBufferAvailable() const override;
  void WritePortableRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::unique_ptr<ClipboardDataEndpoint> data_src) override;
  void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<ClipboardDataEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
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

  class AsyncClipboardOzone;

  std::unique_ptr<AsyncClipboardOzone> async_clipboard_ozone_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardOzone);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
