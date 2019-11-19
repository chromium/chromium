// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

namespace {
constexpr char kMimeTypeFilename[] = "chromium/filename";
}

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
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeURIList);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlWType() {
  return ClipboardFormatType::GetUrlType();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetMozUrlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeMozillaURL);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeText);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextWType() {
  return ClipboardFormatType::GetPlainTextType();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeFilename);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameWType() {
  return ClipboardFormatType::GetFilenameType();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(kMimeTypeHTML);
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
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kMimeTypeWebkitSmartPaste);
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
