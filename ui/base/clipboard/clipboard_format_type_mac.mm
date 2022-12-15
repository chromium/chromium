// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>  // pre-macOS 11
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h> // macOS 11

#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

ClipboardFormatType::ClipboardFormatType() : uttype_(nil) {}

ClipboardFormatType::ClipboardFormatType(NSString* uttype)
    : uttype_([uttype copy]) {}

ClipboardFormatType::ClipboardFormatType(const ClipboardFormatType& other)
    : uttype_([other.uttype_ copy]) {}

ClipboardFormatType& ClipboardFormatType::operator=(
    const ClipboardFormatType& other) {
  if (this != &other) {
    [uttype_ release];
    uttype_ = [other.uttype_ copy];
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

// static
std::string ClipboardFormatType::WebCustomFormatName(int index) {
  return base::StrCat({"com.web.custom.format", base::NumberToString(index)});
}

// static
std::string ClipboardFormatType::WebCustomFormatMapName() {
  return "com.web.custom.format.map";
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomFormatMap() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(ClipboardFormatType::WebCustomFormatMapName()));
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
  // This is an awkward mismatch between macOS which has a "multiple items on a
  // pasteboard approach" and Chromium which has a "one item containing multiple
  // items" concept. This works well enough, though, as `NSPasteboard.types` is
  // a union of all types, and thus will find individual items of this type.
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeFileURL);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeURL);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeString);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeHTML);
  return *type;
}

const ClipboardFormatType& ClipboardFormatType::SvgType() {
  if (@available(macOS 11, *)) {
    static base::NoDestructor<ClipboardFormatType> type(UTTypeSVG.identifier);
    return *type;
  } else {
    static base::NoDestructor<ClipboardFormatType> type(
        base::mac::CFToNSCast(kUTTypeScalableVectorGraphics));
    return *type;
  }
}

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeRTF);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypePNG);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeTIFF);
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
