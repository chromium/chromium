// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"

class SkBitmap;

namespace ui {

// Clipboard data format used by ClipboardInternal.
enum class ClipboardInternalFormat {
  kText = 1 << 0,
  kHtml = 1 << 1,
  kSvg = 1 << 2,
  kRtf = 1 << 3,
  kBookmark = 1 << 4,
  kBitmap = 1 << 5,
  kCustom = 1 << 6,
  kWeb = 1 << 7,
};

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardData {
 public:
  ClipboardData();
  explicit ClipboardData(const ClipboardData&);
  ClipboardData(ClipboardData&&);
  ClipboardData& operator=(const ClipboardData&) = delete;
  ~ClipboardData();

  bool operator==(const ClipboardData& that) const;
  bool operator!=(const ClipboardData& that) const;

  // Bitmask of ClipboardInternalFormat types.
  int format() const { return format_; }

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) {
    text_ = text;
    format_ |= static_cast<int>(ClipboardInternalFormat::kText);
  }

  const std::string& markup_data() const { return markup_data_; }
  void set_markup_data(const std::string& markup_data) {
    markup_data_ = markup_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kHtml);
  }

  const std::string& svg_data() const { return svg_data_; }
  void set_svg_data(const std::string& svg_data) {
    svg_data_ = svg_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kSvg);
  }

  const std::string& rtf_data() const { return rtf_data_; }
  void SetRTFData(const std::string& rtf_data) {
    rtf_data_ = rtf_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kRtf);
  }

  const std::string& url() const { return url_; }
  void set_url(const std::string& url) {
    url_ = url;
    format_ |= static_cast<int>(ClipboardInternalFormat::kHtml);
  }

  const std::string& bookmark_title() const { return bookmark_title_; }
  void set_bookmark_title(const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
    format_ |= static_cast<int>(ClipboardInternalFormat::kBookmark);
  }

  const std::string& bookmark_url() const { return bookmark_url_; }
  void set_bookmark_url(const std::string& bookmark_url) {
    bookmark_url_ = bookmark_url;
    format_ |= static_cast<int>(ClipboardInternalFormat::kBookmark);
  }

  const SkBitmap& bitmap() const { return bitmap_; }
  void SetBitmapData(const SkBitmap& bitmap);

  const std::string& custom_data_format() const { return custom_data_format_; }
  const std::string& custom_data_data() const { return custom_data_data_; }
  void SetCustomData(const std::string& data_format,
                     const std::string& data_data);

  bool web_smart_paste() const { return web_smart_paste_; }
  void set_web_smart_paste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    format_ |= static_cast<int>(ClipboardInternalFormat::kWeb);
  }

  ClipboardDataEndpoint* source() const { return src_.get(); }

  void set_source(std::unique_ptr<ClipboardDataEndpoint> src) {
    src_ = std::move(src);
  }

 private:
  // Plain text in UTF8 format.
  std::string text_;

  // HTML markup data in UTF8 format.
  std::string markup_data_;
  std::string url_;

  // RTF data.
  std::string rtf_data_;

  // Bookmark title in UTF8 format.
  std::string bookmark_title_;
  std::string bookmark_url_;

  // Bitmap images.
  SkBitmap bitmap_;

  // Data with custom format.
  std::string custom_data_format_;
  std::string custom_data_data_;

  // WebKit smart paste data.
  bool web_smart_paste_;

  // Svg data.
  std::string svg_data_;

  int format_;

  // The source of the data.
  std::unique_ptr<ClipboardDataEndpoint> src_ = nullptr;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_
