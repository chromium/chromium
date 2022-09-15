// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data.h"

#include <memory>
#include <ostream>
#include <vector>

#include "base/threading/thread_restrictions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"

namespace ui {

// static
std::vector<uint8_t> ClipboardData::EncodeBitmapData(const SkBitmap& bitmap) {
  // Encoding a PNG can be a long CPU operation.
  base::AssertLongCPUWorkAllowed();

  std::vector<uint8_t> data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                    &data);
  return data;
}

ClipboardData::ClipboardData() = default;

ClipboardData::ClipboardData(const ClipboardData& other) {
  sequence_number_token_ = other.sequence_number_token_;
  format_ = other.format_;
  text_ = other.text_;
  markup_data_ = other.markup_data_;
  url_ = other.url_;
  rtf_data_ = other.rtf_data_;
  maybe_png_ = other.maybe_png_;
  maybe_bitmap_ = other.maybe_bitmap_;
  bookmark_title_ = other.bookmark_title_;
  bookmark_url_ = other.bookmark_url_;
  custom_data_format_ = other.custom_data_format_;
  custom_data_data_ = other.custom_data_data_;
  web_smart_paste_ = other.web_smart_paste_;
  svg_data_ = other.svg_data_;
  filenames_ = other.filenames_;
  src_ = other.src_ ? std::make_unique<DataTransferEndpoint>(*other.src_.get())
                    : nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  commit_time_ = other.commit_time_;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

ClipboardData::ClipboardData(ClipboardData&&) = default;

ClipboardData& ClipboardData::operator=(ClipboardData&& rhs) = default;

ClipboardData::~ClipboardData() = default;

bool ClipboardData::operator==(const ClipboardData& that) const {
  // Two `ClipboardData` instances are equal if they have the same contents.
  // Their sequence number tokens need not be the same, but if they are, we
  // can be sure the data contents will be the same as well.
  if (sequence_number_token_ == that.sequence_number_token_)
    return true;

  bool equal_except_images =
      format_ == that.format() && text_ == that.text() &&
      markup_data_ == that.markup_data() && url_ == that.url() &&
      rtf_data_ == that.rtf_data() &&
      bookmark_title_ == that.bookmark_title() &&
      bookmark_url_ == that.bookmark_url() &&
      custom_data_format_ == that.custom_data_format() &&
      custom_data_data_ == that.custom_data_data() &&
      web_smart_paste_ == that.web_smart_paste() &&
      svg_data_ == that.svg_data() && filenames_ == that.filenames() &&
      (src_.get() ? (that.source() && *src_.get() == *that.source())
                  : !that.source());
  if (!equal_except_images)
    return false;

  // Both instances have encoded PNGs. Compare these.
  if (maybe_png_.has_value() && that.maybe_png_.has_value())
    return maybe_png_ == that.maybe_png_;

  // If only one of the these instances has a bitmap which has not yet been
  // encoded as a PNG, we can't be sure that the images are equal without
  // encoding the bitamp here, on the UI thread. To avoid this, just return
  // false. This means that in the below scenario, a != b.
  //
  //   ClipboardData a;
  //   a.SetBitmapData(image);
  //
  //   ClipboardData b;
  //   b.SetPngData(EncodeBitmapData(image));
  //
  // Avoid this scenario if possible.
  if (maybe_bitmap_.has_value() != that.maybe_bitmap_.has_value())
    return false;

  // Both or neither instances have a bitmap. Compare the bitmaps to determine
  // equality without encoding.
  return !maybe_bitmap_.has_value() ||
         gfx::BitmapsAreEqual(maybe_bitmap_.value(),
                              that.maybe_bitmap_.value());
}

bool ClipboardData::operator!=(const ClipboardData& that) const {
  return !(*this == that);
}

absl::optional<size_t> ClipboardData::size(
    const absl::optional<ClipboardInternalFormat>& format) const {
  size_t total_size = 0;
  if (format_ & static_cast<int>(ClipboardInternalFormat::kText)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kText)
      return text_.size();
    total_size += text_.size();
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kHtml)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kHtml)
      return markup_data_.size() + url_.size();
    total_size += markup_data_.size() + url_.size();
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kSvg)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kSvg)
      return svg_data_.size();
    total_size += svg_data_.size();
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kRtf)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kRtf)
      return rtf_data_.size();
    total_size += rtf_data_.size();
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kBookmark)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kBookmark)
      return bookmark_title_.size() + bookmark_url_.size();
    total_size += bookmark_title_.size() + bookmark_url_.size();
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kPng)) {
    // If there is an unencoded image, use the bitmap's size. This will be
    // inaccurate by a few bytes.
    size_t image_size;
    if (maybe_png_.has_value()) {
      image_size = maybe_png_.value().size();
    } else {
      DCHECK(maybe_bitmap_.has_value());
      image_size = maybe_bitmap_.value().computeByteSize();
    }
    if (format.has_value() && *format == ClipboardInternalFormat::kPng)
      return image_size;
    total_size += image_size;
  }
  if (format_ & static_cast<int>(ClipboardInternalFormat::kCustom)) {
    if (format.has_value() && *format == ClipboardInternalFormat::kCustom)
      return custom_data_data_.size();
    total_size += custom_data_data_.size();
  }

  if (format.has_value() ||
      (format_ & static_cast<int>(ClipboardInternalFormat::kFilenames))) {
    return absl::nullopt;
  }

  return total_size;
}

void ClipboardData::SetPngData(std::vector<uint8_t> png) {
  maybe_bitmap_ = absl::nullopt;
  maybe_png_ = std::move(png);
  format_ |= static_cast<int>(ClipboardInternalFormat::kPng);
}

void ClipboardData::SetPngDataAfterEncoding(std::vector<uint8_t> png) const {
  // Bitmap data can be kept around since we know it corresponds to this PNG.
  // The bitmap can be used to compare two ClipboardData instances even if only
  // one had been encoded to a PNG.
  DCHECK(maybe_bitmap_.has_value());
  DCHECK(format_ & static_cast<int>(ClipboardInternalFormat::kPng));
  maybe_png_ = std::move(png);
}

void ClipboardData::SetBitmapData(const SkBitmap& bitmap) {
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);
  maybe_bitmap_ = bitmap;
  maybe_png_ = absl::nullopt;
  format_ |= static_cast<int>(ClipboardInternalFormat::kPng);
}

absl::optional<SkBitmap> ClipboardData::GetBitmapIfPngNotEncoded() const {
  return maybe_png_.has_value() ? absl::nullopt : maybe_bitmap_;
}

void ClipboardData::SetCustomData(const std::string& data_format,
                                  const std::string& data_data) {
  if (data_data.size() == 0) {
    custom_data_data_.clear();
    custom_data_format_.clear();
    return;
  }
  custom_data_data_ = data_data;
  custom_data_format_ = data_format;
  format_ |= static_cast<int>(ClipboardInternalFormat::kCustom);
}

}  // namespace ui
