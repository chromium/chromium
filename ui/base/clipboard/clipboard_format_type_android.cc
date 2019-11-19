// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

namespace {

// Various formats we support.
const char kURLFormat[] = "url";
const char kPlainTextFormat[] = "text";
const char kHTMLFormat[] = "html";
const char kRTFFormat[] = "rtf";
const char kBitmapFormat[] = "bitmap";
const char kWebKitSmartPasteFormat[] = "webkit_smart";
const char kBookmarkFormat[] = "bookmark";

}  // namespace

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

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return data_ < other.data_;
}

bool ClipboardFormatType::Equals(const ClipboardFormatType& other) const {
  return data_ == other.data_;
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlWType() {
  static base::NoDestructor<ClipboardFormatType> type(kURLFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(kPlainTextFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextWType() {
  static base::NoDestructor<ClipboardFormatType> type(kPlainTextFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(kWebKitSmartPasteFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(kHTMLFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetRtfType() {
  static base::NoDestructor<ClipboardFormatType> type(kRTFFormat);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(kBitmapFormat);
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

// static
const ClipboardFormatType& ClipboardFormatType::GetBookmarkType() {
  static base::NoDestructor<ClipboardFormatType> type(kBookmarkFormat);
  return *type;
}

}  // namespace ui
