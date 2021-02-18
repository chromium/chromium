// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() : data_(nil) {}

ClipboardFormatType::ClipboardFormatType(NSString* native_format)
    : data_([native_format retain]) {}

ClipboardFormatType::ClipboardFormatType(const ClipboardFormatType& other)
    : data_([other.data_ retain]) {}

ClipboardFormatType& ClipboardFormatType::operator=(
    const ClipboardFormatType& other) {
  if (this != &other) {
    [data_ release];
    data_ = [other.data_ retain];
  }
  return *this;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return [data_ isEqualToString:other.data_];
}

ClipboardFormatType::~ClipboardFormatType() {
  [data_ release];
}

std::string ClipboardFormatType::Serialize() const {
  return base::SysNSStringToUTF8(data_);
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    const std::string& serialization) {
  return ClipboardFormatType(base::SysUTF8ToNSString(serialization));
}

std::string ClipboardFormatType::GetName() const {
  return Serialize();
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return [data_ compare:other.data_] == NSOrderedAscending;
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenamesType() {
  static base::NoDestructor<ClipboardFormatType> type(NSFilenamesPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSURLPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeString);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSHTMLPboardType);
  return *type;
}

const ClipboardFormatType& ClipboardFormatType::GetSvgType() {
  static base::NoDestructor<ClipboardFormatType> type(kImageSvg);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetRtfType() {
  static base::NoDestructor<ClipboardFormatType> type(NSRTFPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(NSTIFFPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(kWebSmartPastePboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(kWebCustomDataPboardType);
  return *type;
}

}  // namespace ui
