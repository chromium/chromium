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
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace {

const base::flat_map<UINT, std::string>& PredefinedFormatToNameMap() {
  // These formats are described in winuser.h and
  // https://docs.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats
  static const base::NoDestructor<base::flat_map<UINT, std::string>>
      format_to_name({
          {CF_TEXT, "CF_TEXT"},
          {CF_DIF, "CF_DIF"},
          {CF_TIFF, "CF_TIFF"},
          {CF_OEMTEXT, "CF_OEMTEXT"},
          {CF_DIB, "CF_DIB"},
          {CF_PENDATA, "CF_PENDATA"},
          {CF_RIFF, "CF_RIFF"},
          {CF_WAVE, "CF_WAVE"},
          {CF_UNICODETEXT, "CF_UNICODETEXT"},
          {CF_DIBV5, "CF_DIBV5"},
          {CF_OWNERDISPLAY, "CF_OWNERDISPLAY"},

          // These formats are predefined but explicitly blocked from use,
          // either due to passing along handles, or concerns regarding exposing
          // private information.
          // {CF_MAX, "CF_MAX"},
          // {CF_PRIVATEFIRST, "CF_PRIVATEFIRST"},
          // {CF_PRIVATELAST, "CF_PRIVATELAST"},
          // {CF_GDIOBJFIRST, "CF_GDIOBJFIRST"},
          // {CF_GDIOBJLAST, "CF_GDIOBJLAST"},
          // {CF_BITMAP, "CF_BITMAP"},
          // {CF_SYLK, "CF_SYLK"},
          // {CF_METAFILEPICT, "CF_METAFILEPICT"},
          // {CF_DSPTEXT, "CF_DSPTEXT"},
          // {CF_DSPBITMAP, "CF_DSPBITMAP"},
          // {CF_DSPMETAFILEPICT, "CF_DSPMETAFILEPICT"},
          // {CF_DSPENHMETAFILE, "CF_DSPENHMETAFILE"},
          // {CF_ENHMETAFILE, "CF_ENHMETAFILE"},
          // {CF_HDROP, "CF_HDROP"},
          // {CF_LOCALE, "CF_LOCALE"},
          // {CF_PALETTE, "CF_PALETTE"},

      });
  return *format_to_name;
}

const base::flat_map<std::string, UINT>& PredefinedNameToFormatMap() {
  // Use lambda constructor for thread-safe initialization of name_to_format.
  static const base::NoDestructor<base::flat_map<std::string, UINT>>
      name_to_format([] {
        base::flat_map<std::string, UINT> new_name_to_format;
        const auto& format_to_name = PredefinedFormatToNameMap();
        new_name_to_format.reserve(format_to_name.size());
        for (const auto& it : format_to_name)
          new_name_to_format.emplace(it.second, it.first);
        return new_name_to_format;
      }());
  return *name_to_format;
}

}  // namespace

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

std::string ClipboardFormatType::GetName() const {
  const auto& predefined_format_to_name = PredefinedFormatToNameMap();
  const auto it = predefined_format_to_name.find(data_.cfFormat);
  if (it != predefined_format_to_name.end())
    return it->second;

  constexpr size_t kMaxFormatSize = 1024;
  static base::NoDestructor<std::vector<wchar_t>> name_buffer(kMaxFormatSize);
  int name_size = GetClipboardFormatName(data_.cfFormat, name_buffer->data(),
                                         kMaxFormatSize);
  if (!name_size) {
    // Input format doesn't exist or is predefined.
    return std::string();
  }

  return base::WideToUTF8(std::wstring(name_buffer->data(), name_size));
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
  const auto& predefined_name_to_format = PredefinedNameToFormatMap();
  const auto it = predefined_name_to_format.find(format_string);
  if (it != predefined_name_to_format.end())
    return ClipboardFormatType(it->second);

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
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  static base::NoDestructor<ClipboardFormatType> format(CF_BITMAP);
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
