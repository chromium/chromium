// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// TODO(huangdarwin): Investigate creating a new clipboard_format_type_x11 as a
// wrapper around an X11 ::Atom. This wasn't possible in the past, because unit
// tests spawned a new X11 server for each test, meaning Atom numeric values
// didn't persist across tests.
ClipboardFormatType::ClipboardFormatType() = default;

ClipboardFormatType::~ClipboardFormatType() = default;

ClipboardFormatType::ClipboardFormatType(const std::string& native_format)
    : data_(native_format) {}

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

// TODO(crbug.com/106449): Support custom formats.
ClipboardFormatType ClipboardFormatType::GetCustomPlatformType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// TODO(crbug.com/106449): Support custom formats.
std::string ClipboardFormatType::GetCustomPlatformName() const {
  return Serialize();
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::FilenamesType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeURIList);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeMozillaURL);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeText);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeHTML);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::SvgType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeSvg);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeRTF);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypePNG);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  return ClipboardFormatType::PngType();
}

// static
const ClipboardFormatType& ClipboardFormatType::WebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypeWebkitSmartPaste);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeWebCustomData);
  return *type;
}

}  // namespace ui
