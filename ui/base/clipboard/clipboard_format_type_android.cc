// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() = default;

ClipboardFormatType::ClipboardFormatType(std::string_view native_format)
    : data_(native_format) {}

ClipboardFormatType::~ClipboardFormatType() = default;

std::string ClipboardFormatType::Serialize() const {
  return data_;
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    std::string_view serialization) {
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

// static
std::string ClipboardFormatType::WebCustomFormatName(int index) {
  return base::StrCat({"application/web;type=\"custom/format",
                       base::NumberToString(index), "\""});
}

// static
ClipboardFormatType ClipboardFormatType::CustomPlatformType(
    std::string_view format_string) {
  CHECK(base::IsStringASCII(format_string));
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomFormatMap() {
  static base::NoDestructor<ClipboardFormatType> type(
      "application/web;type=\"custom/formatmap\"");
  return *type;
}

// Various predefined ClipboardFormatTypes.

// static
const ClipboardFormatType& ClipboardFormatType::FilenamesType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeUriList);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeMozillaUrl);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypePlainText);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypeWebkitSmartPaste);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeHtml);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::SvgType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeSvg);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeRtf);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypePng);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeImageUri);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::DataTransferCustomType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypeDataTransferCustomData);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::InternalSourceUrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeSourceUrl);
  return *type;
}

}  // namespace ui
