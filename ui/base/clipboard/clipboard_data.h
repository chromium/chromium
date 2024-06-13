// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/time/time.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class SkBitmap;

namespace ui {

// Clipboard data format used by ClipboardInternal.
enum class ClipboardInternalFormat {
  kText = 1 << 0,
  kHtml = 1 << 1,
  kSvg = 1 << 2,
  kRtf = 1 << 3,
  kBookmark = 1 << 4,
  kPng = 1 << 5,
  kCustom = 1 << 6,
  kWeb = 1 << 7,
  kFilenames = 1 << 8,
};

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardData {
 public:
  ClipboardData();
  ClipboardData(const ClipboardData&);
  ClipboardData(ClipboardData&&);
  ClipboardData& operator=(const ClipboardData&) = delete;
  ClipboardData& operator=(ClipboardData&&);
  ~ClipboardData();

  bool operator==(const ClipboardData& that) const;
  bool operator!=(const ClipboardData& that) const;

  const ClipboardSequenceNumberToken& sequence_number_token() const {
    return sequence_number_token_;
  }

  // Bitmask of ClipboardInternalFormat types.
  int format() const { return format_; }

  // Returns the size of the data in clipboard of `format`, or total size of the
  // clipboard data if `format` is empty. When `format` is kCustom,
  // `custom_data_format` identifies which custom data item to calculate the
  // size of. Returns std::nullopt if size can't be determined, such as when
  // `format` is kFilenames.
  std::optional<size_t> CalculateSize(
      const std::optional<ClipboardInternalFormat>& format,
      const std::optional<ClipboardFormatType>& custom_data_format) const;

  const std::string& text() const { return text_; }
  void set_text(std::string_view text) {
    text_ = text;
    format_ |= static_cast<int>(ClipboardInternalFormat::kText);
  }

  const std::string& markup_data() const { return markup_data_; }
  void set_markup_data(std::string_view markup_data) {
    markup_data_ = markup_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kHtml);
  }

  const std::string& svg_data() const { return svg_data_; }
  void set_svg_data(std::string_view svg_data) {
    svg_data_ = svg_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kSvg);
  }

  const std::string& rtf_data() const { return rtf_data_; }
  void SetRTFData(std::string_view rtf_data) {
    rtf_data_ = rtf_data;
    format_ |= static_cast<int>(ClipboardInternalFormat::kRtf);
  }

  const std::string& url() const { return url_; }
  void set_url(std::string_view url) {
    url_ = url;
    format_ |= static_cast<int>(ClipboardInternalFormat::kHtml);
  }

  const std::string& bookmark_title() const { return bookmark_title_; }
  void set_bookmark_title(std::string_view bookmark_title) {
    bookmark_title_ = bookmark_title;
    format_ |= static_cast<int>(ClipboardInternalFormat::kBookmark);
  }

  const std::string& bookmark_url() const { return bookmark_url_; }
  void set_bookmark_url(std::string_view bookmark_url) {
    bookmark_url_ = bookmark_url;
    format_ |= static_cast<int>(ClipboardInternalFormat::kBookmark);
  }

  // Returns an encoded PNG, or std::nullopt if either there is no image on the
  // clipboard or there is an image which has not yet been encoded to a PNG.
  // `GetBitmapIfPngNotEncoded()` will return a value in the latter case.
  const std::optional<std::vector<uint8_t>>& maybe_png() const {
    return maybe_png_;
  }
  // Set PNG data. If an existing image is already on the clipboard, its
  // contents will be overwritten. If setting the PNG after encoding the bitmap
  // already on the clipboard, use `SetPngDataAfterEncoding()`.
  void SetPngData(std::vector<uint8_t> png);

  // Use this method if the PNG being set was encoded from the bitmap which is
  // already on the clipboard. This allows the operator== method to check
  // equality of two clipboard instances if only one has been encoded to a PNG.
  // It is invalid to call this method unless a bitmap is already on the
  // clipboard. This method is marked const to allow setting this member on
  // const ClipboardData instances.
  void SetPngDataAfterEncoding(std::vector<uint8_t> png) const;

  // Prefer to use `SetPngData()` where possible. Images can be written to the
  // clipboard as bitmaps, but must be read out as an encoded PNG. Callers are
  // responsible for ensuring that the bitmap eventually gets encoded as a PNG.
  // See GetBitmapIfPngNotEncoded() below.
  void SetBitmapData(const SkBitmap& bitmap);
  // Use this method to obtain the bitmap to be encoded to a PNG. It is only
  // recommended to call this method after checking that `maybe_png()` returns
  // no value. If this returns a value, use `EncodeBitmapToPng()` to encode the
  // bitmap to a PNG on a background thread.
  std::optional<SkBitmap> GetBitmapIfPngNotEncoded() const;

  // Returns true if `format` such as
  // ClipboardFormatType::DataTransferCustomType(), or
  // `application/web;type="custom/format0"` exists.
  bool HasCustomDataFormat(const ClipboardFormatType& format) const;
  std::string GetCustomData(const ClipboardFormatType& data_format) const;
  // Returns the ClipboardFormatType::DataTransferCustomType() pickle.
  std::string GetDataTransferCustomData() const;
  void SetCustomData(const ClipboardFormatType& format,
                     const std::string& data);

  bool web_smart_paste() const { return web_smart_paste_; }
  void set_web_smart_paste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    format_ |= static_cast<int>(ClipboardInternalFormat::kWeb);
  }

  const std::vector<ui::FileInfo>& filenames() const { return filenames_; }
  void set_filenames(std::vector<ui::FileInfo> filenames) {
    filenames_ = std::move(filenames);
    if (!filenames_.empty())
      format_ |= static_cast<int>(ClipboardInternalFormat::kFilenames);
  }

  const std::optional<DataTransferEndpoint>& source() const { return src_; }

  void set_source(std::optional<DataTransferEndpoint> src) {
    src_ = std::move(src);
  }

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<base::Time> commit_time() const { return commit_time_; }
  void set_commit_time(std::optional<base::Time> commit_time) {
    commit_time_ = commit_time;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Unique identifier for the clipboard state at the time of data creation.
  ClipboardSequenceNumberToken sequence_number_token_;

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

  // Image data can take the form of PNGs or bitmaps. Strongly prefer PNGs where
  // possible, since images can only be read out of this interface as PNGs. This
  // field is marked as mutable so it can be set after a bitmap is encoded to a
  // PNG on a const instance. The contents of the clipboard are not changing,
  // merely the format.
  mutable std::optional<std::vector<uint8_t>> maybe_png_ = std::nullopt;
  // This member contains a value only in the following cases:
  // 1) SetBitmapData() wrote a bitmap to the clipboard, but it has not yet been
  //    encoded into a PNG.
  // 2) SetBitmapData() wrote a bitmap to the clipboard, then this image was
  //    encoded to PNG. SetPngDataAfterEncoding() was called to indicate that
  //    this member is the decoded version of `maybe_png_`.
  std::optional<SkBitmap> maybe_bitmap_ = std::nullopt;

  // Custom Data keyed by format.
  std::map<ClipboardFormatType, std::string> custom_data_;

  // WebKit smart paste data.
  bool web_smart_paste_ = false;

  // Svg data.
  std::string svg_data_;

  // text/uri-list filenames data.
  std::vector<ui::FileInfo> filenames_;

  int format_ = 0;

  // The source of the data.
  std::optional<DataTransferEndpoint> src_;

#if BUILDFLAG(IS_CHROMEOS)
  // If present, the time at which this data was committed to the clipboard.
  std::optional<base::Time> commit_time_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DATA_H_
