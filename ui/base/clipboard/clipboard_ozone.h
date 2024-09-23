// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

// Clipboard implementation for Ozone-based ports. It delegates the platform
// specifics to the PlatformClipboard instance provided by the Ozone platform.
// Currently, used on Linux Desktop, i.e: X11 and Wayland, and Lacros platforms.
class ClipboardOzone : public Clipboard {
 public:
  ClipboardOzone(const ClipboardOzone&) = delete;
  ClipboardOzone& operator=(const ClipboardOzone&) = delete;

 private:
  friend class Clipboard;

  ClipboardOzone();
  ~ClipboardOzone() override;

  // Clipboard overrides:
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
  bool IsSelectionBufferAvailable() const override;
  void WritePortableTextRepresentation(ClipboardBuffer buffer,
                                       const ObjectMap& objects);
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

  // Used for syncing clipboard sources between Lacros and Ash in ChromeOS, or
  // to put a source URL in the clipboard on other Ozone platforms.
  void AddSourceToClipboard(const ClipboardBuffer buffer,
                            std::unique_ptr<DataTransferEndpoint> data_src);

  class AsyncClipboardOzone;

  std::unique_ptr<AsyncClipboardOzone> async_clipboard_ozone_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
