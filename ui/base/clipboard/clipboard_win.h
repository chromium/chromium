// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_change_notifier.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace base {
namespace win {
class MessageWindow;
}
}

namespace ui {

class ClipboardChangeNotifier;

// Documentation on the underlying Win32 API this ultimately abstracts is
// available at
// https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard.
class ClipboardWin : public Clipboard, public ClipboardChangeNotifier {
 public:
  ClipboardWin(const ClipboardWin&) = delete;
  ClipboardWin& operator=(const ClipboardWin&) = delete;

  // ClipboardChangeNotifier overrides:
  void StartNotifying() override;
  void StopNotifying() override;

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
  void ReadText(ClipboardBuffer buffer,
                const std::optional<DataTransferEndpoint>& data_dst,
                ReadTextCallback callback) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const std::optional<DataTransferEndpoint>& data_dst,
                     ReadAsciiTextCallback callback) const override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const std::optional<DataTransferEndpoint>& data_dst,
                          ReadAvailableTypesCallback callback) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const std::optional<DataTransferEndpoint>& data_dst,
                ReadHtmlCallback callback) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadSvgCallback callback) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadRTFCallback callback) const override;
  void ReadDataTransferCustomData(
      ClipboardBuffer buffer,
      const std::u16string& type,
      const std::optional<DataTransferEndpoint>& data_dst,
      ReadDataTransferCustomDataCallback callback) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const std::optional<DataTransferEndpoint>& data_dst,
                     ReadFilenamesCallback callback) const override;
  void ReadData(const ClipboardFormatType& format,
                const std::optional<DataTransferEndpoint>& data_dst,
                ReadDataCallback callback) const override;

  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst,
                          std::vector<std::u16string>* types) const override;
  void ReadText(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadPng(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadPngCallback callback) const override;
  void ReadBookmark(const DataTransferEndpoint* data_dst,
                    std::u16string* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const DataTransferEndpoint* data_dst,
                std::string* result) const override;
  void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      const std::vector<RawData>& raw_objects,
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

  void WriteClipboardHistory();
  void WriteUploadCloudClipboard();
  void WriteConfidentialDataForPassword();

  // If kNonBlockingOsClipboardReads is enabled, runs `read_func` on
  // `worker_task_runner_` (passing owner_window = nullptr) and runs
  // `reply_func` on the caller sequence with the result. Otherwise runs both
  // callbacks synchronously on the caller thread, and `read_func` is passed
  // owner_window = GetClipboardWindow().
  template <typename Result>
  void ReadAsync(base::OnceCallback<Result(HWND)> read_func,
                 base::OnceCallback<void(Result)> reply_func) const;
  static std::u16string ReadTextInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::string ReadAsciiTextInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::vector<std::u16string> ReadAvailableTypesInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::vector<std::u16string> GetStandardFormatsInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst);
  static bool IsFormatAvailableInternal(
      const ClipboardFormatType& format,
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst);
  struct ReadHTMLResult {
    std::u16string markup;
    std::string src_url;
    uint32_t fragment_start = 0;
    uint32_t fragment_end = 0;
  };
  // TODO(crbug.com/458194647): Return ReadHTMLResult instead of using
  // out-params.
  static void ReadHTMLInternal(
      HWND owner_window,
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      std::u16string* markup,
      std::string* src_url,
      uint32_t* fragment_start,
      uint32_t* fragment_end);
  static std::u16string ReadSvgInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::string ReadRTFInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::u16string ReadDataTransferCustomDataInternal(
      ClipboardBuffer buffer,
      const std::u16string& type,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::string ReadDataInternal(
      const ClipboardFormatType& format,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::vector<ui::FileInfo> ReadFilenamesInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  // first: PNG bytes (if available), second: bitmap fallback.
  using ReadPngResult = std::pair<std::vector<uint8_t>, SkBitmap>;
  static ReadPngResult ReadPngInternal(
      ClipboardBuffer buffer,
      const std::optional<DataTransferEndpoint>& data_dst,
      HWND owner_window);
  static std::vector<uint8_t> ReadPngTypeDataInternal(ClipboardBuffer buffer,
                                                      HWND owner_window);
  static SkBitmap ReadBitmapInternal(ClipboardBuffer buffer, HWND owner_window);

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

  // Whether the clipboard is being monitored for changes.
  bool monitoring_clipboard_changes_ = false;

  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_WIN_H_
