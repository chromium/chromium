// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include <shlobj.h>
#include <urlmon.h>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

namespace {

unsigned int RegisterClipboardFormatChecked(const wchar_t* format) {
  // On Windows, there is a limit of 16k ATOMs that can be created. After
  // reaching this limit, the system will be in an unstable state. The function
  // RegisterClipboardFormat(...) is used to register format. These formats
  // can't be released.
  //
  // A common cause of ATOMs exhaustion seems related to running Chrome under
  // automation (https://crbug.com/1470483).
  UINT result = ::RegisterClipboardFormat(format);
  PCHECK(result);
  return result;
}

}  // namespace

// ClipboardFormatType implementation.
// Windows formats are backed by "Clipboard Formats", documented here:
// https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats
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
            /* .lindex */ index, /* .tymed*/ tymed} {}

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
  CHECK(base::StringToInt(serialization, &clipboard_format));
  return ClipboardFormatType(clipboard_format);
}

// static
std::string ClipboardFormatType::WebCustomFormatName(int index) {
  return base::StrCat({"Web Custom Format", base::NumberToString(index)});
}

// static
ClipboardFormatType ClipboardFormatType::CustomPlatformType(
    const std::string& format_string) {
  // Once these formats are registered, `RegisterClipboardFormat` just returns
  // the `cfFormat` associated with it and doesn't register a new format.
  DCHECK(base::IsStringASCII(format_string));
  return ClipboardFormatType(
      RegisterClipboardFormatChecked(base::ASCIIToWide(format_string).c_str()));
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomFormatMap() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"Web Custom Format Map"));
  return *format;
}

std::string ClipboardFormatType::GetName() const {
  return Serialize();
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
      RegisterClipboardFormatChecked(base::ASCIIToWide(format_string).c_str()));
}

// The following formats can be referenced by clipboard_util::GetPlainText.
// Clipboard formats are initialized in a thread-safe manner, using static
// initialization. COM requires this thread-safe initialization.
// TODO(dcheng): We probably need to make static initialization of "known"
// ClipboardFormatTypes thread-safe on all platforms.

// static
const ClipboardFormatType& ClipboardFormatType::FilenamesType() {
  return FilenameType();
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_INETURLW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_UNICODETEXT);
  return *format;
}

// MS HTML Format

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"HTML Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::SvgType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_MIME_SVG_XML));
  return *format;
}

// MS RTF Format

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"Rich Text Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"PNG"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_DIBV5);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_INETURLA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextAType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_TEXT);
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::FilenameAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_FILENAMEA));
  return *format;
}

// Firefox text/html
// static
const ClipboardFormatType& ClipboardFormatType::TextHtmlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_MIME_HTML));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::CFHDropType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_HDROP);
  return *format;
}

// Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
// ANSI format (e.g., it could be that it doesn't support Unicode). So need to
// register both the ANSI and Unicode file group descriptors.
// static
const ClipboardFormatType& ClipboardFormatType::FileDescriptorAType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_FILEDESCRIPTORA));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::FileDescriptorType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_FILEDESCRIPTORW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::FileContentZeroType() {
  // This uses a storage media type of TYMED_HGLOBAL, which is not commonly
  // used with CFSTR_FILECONTENTS (but used in Chromium--see
  // OSExchangeDataProviderWin::SetFileContents). Use FileContentAtIndexType
  // if TYMED_ISTREAM and TYMED_ISTORAGE are needed.
  // TODO(crbug.com/41451800): Should TYMED_ISTREAM / TYMED_ISTORAGE be
  // used instead of TYMED_HGLOBAL in
  // OSExchangeDataProviderWin::SetFileContents.
  // The 0 constructor argument is used with CFSTR_FILECONTENTS to specify file
  // content.
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_FILECONTENTS), 0);
  return *format;
}

// static
std::map<LONG, ClipboardFormatType>& ClipboardFormatType::FileContentTypeMap() {
  static base::NoDestructor<std::map<LONG, ClipboardFormatType>>
      index_to_type_map;
  return *index_to_type_map;
}

// static
const ClipboardFormatType& ClipboardFormatType::FileContentAtIndexType(
    LONG index) {
  auto& index_to_type_map = FileContentTypeMap();

  auto insert_or_assign_result = index_to_type_map.insert(
      {index, ClipboardFormatType(
                  RegisterClipboardFormatChecked(CFSTR_FILECONTENTS), index,
                  TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE)});
  return insert_or_assign_result.first->second;
}

// static
const ClipboardFormatType& ClipboardFormatType::FilenameType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_FILENAMEW));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::IDListType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(CFSTR_SHELLIDLIST));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::MozUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"text/x-moz-url"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"WebKit Smart Paste Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::DataTransferCustomType() {
  // TODO(http://crbug.com/106449): Standardize this name.
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"Chromium Web Custom MIME Data Format"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::InternalSourceUrlType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"Chromium internal source URL"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::ClipboardHistoryType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"CanIncludeInClipboardHistory"));
  return *format;
}

// static
const ClipboardFormatType& ClipboardFormatType::UploadCloudClipboardType() {
  static base::NoDestructor<ClipboardFormatType> format(
      RegisterClipboardFormatChecked(L"CanUploadToCloudClipboard"));
  return *format;
}

}  // namespace ui
