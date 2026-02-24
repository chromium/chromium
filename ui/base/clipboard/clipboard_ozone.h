// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

// Clipboard implementation for Ozone-based ports. It delegates the platform
// specifics to the PlatformClipboard instance provided by the Ozone platform.
// Currently, used on Linux Desktop, i.e: X11 and Wayland platforms.
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
                          const std::optional<DataTransferEndpoint>& data_dst,
                          ReadAvailableTypesCallback callback) const override;
  void ReadText(ClipboardBuffer buffer,
                const std::optional<DataTransferEndpoint>& data_dst,
                ReadTextCallback callback) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const std::optional<DataTransferEndpoint>& data_dst,
                     ReadAsciiTextCallback callback) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const std::optional<DataTransferEndpoint>& data_dst,
                ReadHtmlCallback callback) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadSvgCallback callback) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadRTFCallback callback) const override;
  void ReadPng(ClipboardBuffer buffer,
               const std::optional<DataTransferEndpoint>& data_dst,
               ReadPngCallback callback) const override;
  void ReadDataTransferCustomData(
      ClipboardBuffer buffer,
      const std::u16string& type,
      const std::optional<DataTransferEndpoint>& data_dst,
      ReadDataTransferCustomDataCallback callback) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const std::optional<DataTransferEndpoint>& data_dst,
                     ReadFilenamesCallback callback) const override;
  void ReadBookmark(const std::optional<DataTransferEndpoint>& data_dst,
                    ReadBookmarkCallback callback) const override;
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

  // Used to put a source URL in the clipboard on other Ozone platforms.
  void AddSourceToClipboard(const ClipboardBuffer buffer,
                            std::unique_ptr<DataTransferEndpoint> data_src);

  void OnReadAvailableTypes(
      ClipboardBuffer buffer,
      ReadAvailableTypesCallback callback,
      const std::vector<std::string>& available_types) const;
  void OnReadCustomData(std::vector<std::u16string> types,
                        ReadAvailableTypesCallback callback,
                        const PlatformClipboard::Data& data) const;

  template <typename Callback, typename ProcessCallback>
  void ReadAsync(ClipboardBuffer buffer,
                 const std::string& mime_type,
                 const std::optional<DataTransferEndpoint>& data_dst,
                 ClipboardFormatMetric metric,
                 Callback callback,
                 ProcessCallback process_cb) const;

  template <typename Callback, typename ProcessCallback>
  void OnReadAsync(ClipboardBuffer buffer,
                   const std::optional<DataTransferEndpoint>& data_dst,
                   ClipboardFormatMetric metric,
                   Callback callback,
                   ProcessCallback process_cb,
                   const PlatformClipboard::Data& data) const;

  template <typename Callback, typename ProcessCallback>
  void OnReadAsyncSource(const std::optional<DataTransferEndpoint>& data_dst,
                         ClipboardFormatMetric metric,
                         Callback callback,
                         ProcessCallback process_cb,
                         const PlatformClipboard::Data& data,
                         std::optional<DataTransferEndpoint> data_src) const;

  class AsyncClipboardOzone;

  std::unique_ptr<AsyncClipboardOzone> async_clipboard_ozone_;

  mutable base::WeakPtrFactory<ClipboardOzone> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_OZONE_H_
