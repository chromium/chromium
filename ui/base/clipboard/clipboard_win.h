// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace base {
namespace win {
class MessageWindow;
}
}

namespace ui {

// Documentation on the underlying Win32 API this ultimately abstracts is
// available at
// https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard.
class ClipboardWin : public Clipboard {
 public:
  ClipboardWin(const ClipboardWin&) = delete;
  ClipboardWin& operator=(const ClipboardWin&) = delete;

 private:
  friend class Clipboard;

  ClipboardWin();
  ~ClipboardWin() override;

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
  std::vector<uint8_t> ReadPngInternal(ClipboardBuffer buffer) const;
  SkBitmap ReadBitmapInternal(ClipboardBuffer buffer) const;

  // Safely write to system clipboard. Free |handle| on failure.
  // This function takes ownership of the given handle's memory.
  void WriteToClipboard(ClipboardFormatType format, HANDLE handle);

  // Return the window that should be the clipboard owner, creating it
  // if necessary.  Marked const for lazily initialization by const methods.
  HWND GetClipboardWindow() const;

  // Mark this as mutable so const methods can still do lazy initialization.
  mutable std::unique_ptr<base::win::MessageWindow> clipboard_owner_;

  // Mapping of OS-provided sequence number to a unique token.
  mutable struct {
    DWORD sequence_number;
    ClipboardSequenceNumberToken token;
  } clipboard_sequence_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_
