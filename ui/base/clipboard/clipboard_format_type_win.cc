// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include <shlobj.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() = default;

ClipboardFormatType::ClipboardFormatType(UINT native_format)
    : ClipboardFormatType(native_format, -1) {}

ClipboardFormatType::ClipboardFormatType(UINT native_format, LONG index)
    : ClipboardFormatType(native_format, index, TYMED_HGLOBAL) {}

// In C++ 20, we can use designated initializers.
ClipboardFormatType::ClipboardFormatType(UINT native_format,
                                         LONG index,
                                         DWORD tymed)
    : data_{/* .cfFormat */ static_cast<CLIPFORMAT>(native_format),
            /* .ptd */ nullptr, /* .dwAspect */ DVASPECT_CONTENT,
            /* .lindex */ index, /* .tymed*/ tymed} {
  // Log the frequency of invalid formats being input into the constructor.
  if (!native_format) {
    static int error_count = 0;
    ++error_count;
    // TODO(https://crbug.com/1000919): Evaluate and remove UMA metrics after
    // enough data is gathered.
    base::UmaHistogramCounts100("Clipboard.RegisterClipboardFormatFailure",
                                error_count);
  }
}

ClipboardFormatType::~ClipboardFormatType() = default;

std::string ClipboardFormatType::Serialize() const {
  return base::NumberToString(data_.cfFormat);
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    const std::string& serialization) {
  int clipboard_format = -1;
  // |serialization| is expected to be a string representing the Windows
  // data_.cfFormat (format number) returned by GetType.
  if (!base::StringToInt(serialization, &clipboard_format)) {
    NOTREACHED();
    return ClipboardFormatType();
  }
  return ClipboardFormatType(clipboard_format);
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return data_.cfFormat < other.data_.cfFormat;
}

bool ClipboardFormatType::Equals(const ClipboardFormatType& other) const {
  return data_.cfFormat == other.data_.cfFormat;
}

// Predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType(
      ::RegisterClipboardFormat(base::ASCIIToUTF16(format_string).c_str()));
}

// The following formats can be referenced by ClipboardUtilWin::GetPlainText.
// Clipboard formats are initialized in a thread-safe manner, using static
// initialization. COM requires this thread-safe initialization.
// TODO(dcheng): We probably need to make static initialization of "known"
// ClipboardFormatTypes thread-safe on all platforms.

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_INETURLA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlWType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_INETURLW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetMozUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"text/x-moz-url"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_TEXT);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextWType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_UNICODETEXT);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILENAMEA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameWType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILENAMEW));
  return *format;
}

// MS HTML Format

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"HTML Format"));
  return *format;
}

// MS RTF Format

// static
const ClipboardFormatType& ClipboardFormatType::GetRtfType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"Rich Text Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_BITMAP);
  return *format;
}

// Firefox text/html

// static
const ClipboardFormatType& ClipboardFormatType::GetTextHtmlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"text/html"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetCFHDropType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_HDROP);
  return *format;
}

// Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
// ANSI format (e.g., it could be that it doesn't support Unicode). So need to
// register both the ANSI and Unicode file group descriptors.
// static
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorWType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileContentZeroType() {
  // Note this uses a storage media type of TYMED_HGLOBAL, which is not commonly
  // used with CFSTR_FILECONTENTS (but used in Chromium--see
  // OSExchangeDataProviderWin::SetFileContents). Use GetFileContentAtIndexType
  // if TYMED_ISTREAM and TYMED_ISTORAGE are needed.
  // TODO(https://crbug.com/950756): Should TYMED_ISTREAM / TYMED_ISTORAGE be
  // used instead of TYMED_HGLOBAL in
  // OSExchangeDataProviderWin::SetFileContents.
  // The 0 constructor argument is used with CFSTR_FILECONTENTS to specify file
  // content.
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILECONTENTS), 0);
  return *format;
}

// static
std::map<LONG, ClipboardFormatType>&
ClipboardFormatType::GetFileContentTypeMap() {
  static base::NoDestructor<std::map<LONG, ClipboardFormatType>>
      index_to_type_map;
  return *index_to_type_map;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileContentAtIndexType(
    LONG index) {
  auto& index_to_type_map = GetFileContentTypeMap();

  auto insert_or_assign_result = index_to_type_map.insert(
      {index,
       ClipboardFormatType(::RegisterClipboardFormat(CFSTR_FILECONTENTS), index,
                           TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE)});
  return insert_or_assign_result.first->second;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetIDListType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_SHELLIDLIST));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"WebKit Smart Paste Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebCustomDataType() {
  // TODO(http://crbug.com/106449): Standardize this name.
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"Chromium Web Custom MIME Data Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPepperCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"Chromium Pepper MIME Data Format"));
  return *format;
}

}  // namespace ui
