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

class COMPONENT_EXPORT(BASE_CLIPBOARD) ClipboardMac : public Clipboard {
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
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          std::vector<base::string16>* types,
                          bool* contains_filenames) const override;
  void ReadText(ClipboardBuffer buffer, base::string16* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadRTF(ClipboardBuffer buffer, std::string* result) const override;
  SkBitmap ReadImage(ClipboardBuffer buffer, NSPasteboard* pb) const;
  SkBitmap ReadImage(ClipboardBuffer buffer) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const base::string16& type,
                      base::string16* result) const override;
  void ReadBookmark(base::string16* title, std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                std::string* result) const override;
  void WritePortableRepresentations(ClipboardBuffer buffer,
                                    const ObjectMap& objects) override;
  void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations)
      override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
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

  DISALLOW_COPY_AND_ASSIGN(ClipboardMac);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_
