// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() = default;

ClipboardFormatType::ClipboardFormatType(const std::string& native_format)
    : data_(native_format) {}

ClipboardFormatType::~ClipboardFormatType() = default;

std::string ClipboardFormatType::Serialize() const {
  return data_;
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    const std::string& serialization) {
  return ClipboardFormatType(serialization);
}

std::string ClipboardFormatType::GetName() const {
  return Serialize();
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return data_ < other.data_;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return data_ == other.data_;
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeURIList);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeText);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypeWebkitSmartPaste);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeHTML);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetSvgType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeSvg);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetRtfType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeRTF);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypePNG);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeWebCustomData);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPepperCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypePepperCustomData);
  return *type;
}

}  // namespace ui
