// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include <shlobj.h>
#include <urlmon.h>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

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

// static
ClipboardFormatType ClipboardFormatType::GetCustomPlatformType(
    const std::string& format_string) {
  // For unsanitized custom formats, we add `Web ` prefix and capitalize the
  // first letter of each word in the format. e.g. `text/custom` format would be
  // converted to `Web Text Custom`. Similarly for `text/html` or any other
  // standard formats, the pickled version would be prefixed with `Web ` and
  // first letter capitalized. e.g. text/html would be converted to `Web Text
  // Html`.
  // For security reasons we also check for valid ascii codepoints.
  constexpr int kMinNameSize = 3;  // Formats need to have at least 3 chars.
  if (!base::IsStringASCII(format_string) ||
      format_string.size() < kMinNameSize) {
    return ClipboardFormatType();
  }

  size_t index = format_string.find('/');
  if (index == std::string::npos || index == 0 ||
      index == format_string.size() - 1)
    return ClipboardFormatType();
  base::StringPiece first_part =
      base::StringPiece(format_string).substr(0, index);
  base::StringPiece second_part =
      base::StringPiece(format_string).substr(index + 1);
  std::string web_custom_format_string = base::StrCat(
      {"Web ", base::ToUpperASCII(first_part.substr(0, 1)),
       first_part.substr(1), " ", base::ToUpperASCII(second_part.substr(0, 1)),
       second_part.substr(1)});
  return ClipboardFormatType(::RegisterClipboardFormat(
      base::ASCIIToWide(web_custom_format_string).c_str()));
}

// static
std::string ClipboardFormatType::GetCustomPlatformName() const {
  constexpr size_t kMaxFormatSize = 1024;
  static base::NoDestructor<std::vector<wchar_t>> name_buffer(kMaxFormatSize);
  int name_size = GetClipboardFormatName(data_.cfFormat, name_buffer->data(),
                                         kMaxFormatSize);
  // Custom formats should have at least 7 characters. e.g. "Web x y"
  constexpr int kMinNameSize = 7;
  if (!name_size || name_size < kMinNameSize) {
    // Input format doesn't exist or is predefined.
    return std::string();
  }

  std::string format_name_in_clipboard =
      base::WideToASCII(std::wstring(name_buffer->data(), name_size));

  // For security reasons we also check for valid ascii codepoints.
  if (!base::IsStringASCII(format_name_in_clipboard))
    return std::string();
  // For unsanitized custom formats (prefixed with Web) we extract the strings
  // and convert it into standard representation. e.g. `Web Text Custom` format
  // would be converted to `text/custom`.
  if (format_name_in_clipboard.substr(0, 4) == "Web ") {
    base::StringPiece format_name =
        base::StringPiece(format_name_in_clipboard).substr(4);
    size_t space_index = format_name.find(" ");

    if (space_index == std::string::npos)
      return std::string();
    if (format_name.size() < (space_index + 2))
      return std::string();

    return base::StrCat(
        {base::ToLowerASCII(format_name.substr(0, space_index)), "/",
         base::ToLowerASCII(format_name.substr(space_index + 1))});
  }
  return format_name_in_clipboard;
}

std::string ClipboardFormatType::GetName() const {
  if (ClipboardFormatType::GetPlainTextAType().ToFormatEtc().cfFormat ==
      data_.cfFormat) {
    return kMimeTypeText;
  }
  if (ClipboardFormatType::GetHtmlType().ToFormatEtc().cfFormat ==
      data_.cfFormat) {
    return kMimeTypeHTML;
  }
  if (ClipboardFormatType::GetRtfType().ToFormatEtc().cfFormat ==
      data_.cfFormat) {
    return kMimeTypeRTF;
  }
  if (CF_DIB == data_.cfFormat)
    return kMimeTypePNG;
  if (ClipboardFormatType::GetCFHDropType().ToFormatEtc().cfFormat ==
          data_.cfFormat ||
      ClipboardFormatType::GetFilenameType().ToFormatEtc().cfFormat ==
          data_.cfFormat ||
      ClipboardFormatType::GetFilenameAType().ToFormatEtc().cfFormat ==
          data_.cfFormat) {
    return kMimeTypeURIList;
  }
  // Not a standard format type.
  return std::string();
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return data_.cfFormat < other.data_.cfFormat;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return data_.cfFormat == other.data_.cfFormat;
}

// Predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType(
      ::RegisterClipboardFormat(base::ASCIIToWide(format_string).c_str()));
}

// The following formats can be referenced by ClipboardUtilWin::GetPlainText.
// Clipboard formats are initialized in a thread-safe manner, using static
// initialization. COM requires this thread-safe initialization.
// TODO(dcheng): We probably need to make static initialization of "known"
// ClipboardFormatTypes thread-safe on all platforms.

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenamesType() {
  return GetFilenameType();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_INETURLW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_UNICODETEXT);
  return *format;
}

// MS HTML Format

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"HTML Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetSvgType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_MIME_SVG_XML));
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
const ClipboardFormatType& ClipboardFormatType::GetPngType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"PNG"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_DIBV5);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_INETURLA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextAType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_TEXT);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILENAMEA));
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
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileContentZeroType() {
  // This uses a storage media type of TYMED_HGLOBAL, which is not commonly
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
const ClipboardFormatType& ClipboardFormatType::GetFilenameType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_FILENAMEW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetIDListType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(CFSTR_SHELLIDLIST));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetMozUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      ::RegisterClipboardFormat(L"text/x-moz-url"));
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

}  // namespace ui
