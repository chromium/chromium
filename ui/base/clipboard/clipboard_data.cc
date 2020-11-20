// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data.h"

#include <memory>
#include <ostream>

#include "base/notreached.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/gfx/skia_util.h"

namespace ui {

ClipboardData::ClipboardData() : web_smart_paste_(false), format_(0) {}

ClipboardData::ClipboardData(const ClipboardData& other) {
  format_ = other.format_;
  text_ = other.text_;
  markup_data_ = other.markup_data_;
  url_ = other.url_;
  rtf_data_ = other.rtf_data_;
  bitmap_ = other.bitmap();
  bookmark_title_ = other.bookmark_title_;
  bookmark_url_ = other.bookmark_url_;
  custom_data_format_ = other.custom_data_format_;
  custom_data_data_ = other.custom_data_data_;
  web_smart_paste_ = other.web_smart_paste_;
  svg_data_ = other.svg_data_;
  src_ = other.src_ ? std::make_unique<ClipboardDataEndpoint>(*other.src_.get())
                    : nullptr;
}

ClipboardData::~ClipboardData() = default;

ClipboardData::ClipboardData(ClipboardData&&) = default;

bool ClipboardData::operator==(const ClipboardData& that) const {
  return format_ == that.format() && text_ == that.text() &&
         markup_data_ == that.markup_data() && url_ == that.url() &&
         rtf_data_ == that.rtf_data() &&
         bookmark_title_ == that.bookmark_title() &&
         bookmark_url_ == that.bookmark_url() &&
         custom_data_format_ == that.custom_data_format() &&
         custom_data_data_ == that.custom_data_data() &&
         web_smart_paste_ == that.web_smart_paste() &&
         svg_data_ == that.svg_data() &&
         gfx::BitmapsAreEqual(bitmap_, that.bitmap()) &&
         (src_.get() ? (that.source() && *src_.get() == *that.source())
                     : !that.source());
}

bool ClipboardData::operator!=(const ClipboardData& that) const {
  return !(*this == that);
}

void ClipboardData::SetBitmapData(const SkBitmap& bitmap) {
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);
  bitmap_ = bitmap;
  format_ |= static_cast<int>(ClipboardInternalFormat::kBitmap);
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
