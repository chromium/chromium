// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_

#include <stddef.h>
#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/clipboard/clipboard.h"

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
  // Returns the in-memory clipboard for the current thread. Note that this
  // method must *only* be used when the caller is sure that the clipboard for
  // the current thread is in fact an instance of ClipboardNonBacked.
  static ClipboardNonBacked* GetForCurrentThread();

  // Returns the current ClipboardData.
  const ClipboardData* GetClipboardData(ClipboardDataEndpoint* data_dst) const;

  // Writes the current ClipboardData and returns the previous data.
  // The data source is expected to be set in `data`.
  std::unique_ptr<ClipboardData> WriteClipboardData(
      std::unique_ptr<ClipboardData> data);

 private:
  friend class Clipboard;
  friend class ClipboardNonBackedTest;
  ClipboardNonBacked();
  ~ClipboardNonBacked() override;

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

  const std::unique_ptr<ClipboardInternal> clipboard_internal_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardNonBacked);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_NON_BACKED_H_
