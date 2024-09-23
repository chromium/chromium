// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

struct ClipboardFormatType::ObjCStorage {
  // A Uniform Type identifier string.
  NSString* uttype;
};

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType()
    : objc_storage_(std::make_unique<ObjCStorage>()) {}

ClipboardFormatType::ClipboardFormatType(NSString* native_format)
    : ClipboardFormatType() {
  objc_storage_->uttype = native_format;
}

ClipboardFormatType::ClipboardFormatType(const ClipboardFormatType& other)
    : ClipboardFormatType() {
  objc_storage_->uttype = other.objc_storage_->uttype;
}

ClipboardFormatType& ClipboardFormatType::operator=(
    const ClipboardFormatType& other) {
  if (this != &other) {
    objc_storage_->uttype = other.objc_storage_->uttype;
  }
  return *this;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return [objc_storage_->uttype isEqualToString:other.objc_storage_->uttype];
}

ClipboardFormatType::~ClipboardFormatType() = default;

std::string ClipboardFormatType::Serialize() const {
  return base::SysNSStringToUTF8(objc_storage_->uttype);
}

NSString* ClipboardFormatType::ToNSString() const {
  return objc_storage_->uttype;
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
  return [objc_storage_->uttype compare:other.objc_storage_->uttype] ==
         NSOrderedAscending;
}

// static
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
  static base::NoDestructor<ClipboardFormatType> type(UTTypeSVG.identifier);
  return *type;
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
const ClipboardFormatType& ClipboardFormatType::DataTransferCustomType() {
  static base::NoDestructor<ClipboardFormatType> type(
      kUTTypeChromiumDataTransferCustomData);
  return *type;
}

}  // namespace ui
