// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#import <Foundation/Foundation.h>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() : uttype_(nil) {}

ClipboardFormatType::ClipboardFormatType(NSString* native_format)
    : uttype_([native_format retain]) {}

ClipboardFormatType::ClipboardFormatType(const ClipboardFormatType& other)
    : uttype_([other.uttype_ retain]) {}

ClipboardFormatType& ClipboardFormatType::operator=(
    const ClipboardFormatType& other) {
  if (this != &other) {
    [uttype_ release];
    uttype_ = [other.uttype_ retain];
  }
  return *this;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return [uttype_ isEqualToString:other.uttype_];
}

ClipboardFormatType::~ClipboardFormatType() {
  [uttype_ release];
}

std::string ClipboardFormatType::Serialize() const {
  return base::SysNSStringToUTF8(uttype_);
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
  return [uttype_ compare:other.uttype_] == NSOrderedAscending;
}

std::string ClipboardFormatType::WebCustomFormatName(int index) {
  return base::StrCat(
      {"org.w3.web-custom-format.type-", base::NumberToString(index)});
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomFormatMap() {
  static base::NoDestructor<ClipboardFormatType> type(
      @"org.w3.web-custom-format.map");
  return *type;
}

// static
ClipboardFormatType ClipboardFormatType::CustomPlatformType(
    const std::string& format_string) {
  DCHECK(base::IsStringASCII(format_string));
  return ClipboardFormatType::Deserialize(format_string);
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::FilenamesType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeURIList));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeMozillaURL));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeText));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeHTML));
  return *type;
}

const ClipboardFormatType& ClipboardFormatType::SvgType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeSvg));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeRTF));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypePNG));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(kMimeTypeImageURI));
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kUTTypeWebKitWebSmartPaste);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kUTTypeChromiumWebCustomData);
  return *type;
}

}  // namespace ui
