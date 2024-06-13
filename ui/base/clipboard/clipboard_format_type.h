// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_FORMAT_TYPE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_FORMAT_TYPE_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_APPLE)
#ifdef __OBJC__
@class NSString;
#endif
#endif  // BUILDFLAG(IS_APPLE)

namespace ui {

// Platform neutral holder for native data representation of a clipboard type.
// Copyable and assignable, since this is an opaque value type.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) ClipboardFormatType {
 public:
  ClipboardFormatType();
  ~ClipboardFormatType();

#if BUILDFLAG(IS_WIN)
  explicit ClipboardFormatType(UINT native_format);
  ClipboardFormatType(UINT native_format, LONG index);
  ClipboardFormatType(UINT native_format, LONG index, DWORD tymed);
#endif

  // Serializes and deserializes a ClipboardFormatType for use in IPC messages.
  // The serialized string may not be human-readable.
  std::string Serialize() const;
  static ClipboardFormatType Deserialize(const std::string& serialization);

  // Gets the ClipboardFormatType corresponding to the standard formats.
  static ClipboardFormatType GetType(const std::string& format_string);

  // Get format identifiers for various types.
  static const ClipboardFormatType& FilenamesType();
  static const ClipboardFormatType& UrlType();
  static const ClipboardFormatType& PlainTextType();
  static const ClipboardFormatType& WebKitSmartPasteType();
  // Win: MS HTML Format, Other: Generic HTML format
  static const ClipboardFormatType& HtmlType();
  static const ClipboardFormatType& SvgType();
  static const ClipboardFormatType& RtfType();
  static const ClipboardFormatType& PngType();
  // TODO(crbug.com/40178509): Remove this type.
  static const ClipboardFormatType& BitmapType();
  // Chromium-only type for custom formats copied via DataTransfer API.
  // See https://w3c.github.io/clipboard-apis/#clipboard-events-and-interfaces.
  static const ClipboardFormatType& DataTransferCustomType();

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS custom type to sync clipboard source metadata between Ash and
  // Lacros.
  static const ClipboardFormatType& DataTransferEndpointDataType();
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  // ANSI formats. Only Windows differentiates between ANSI and UNICODE formats
  // in ClipboardFormatType. Reference:
  // https://docs.microsoft.com/en-us/windows/win32/learnwin32/working-with-strings
  static const ClipboardFormatType& UrlAType();
  static const ClipboardFormatType& PlainTextAType();
  static const ClipboardFormatType& FilenameAType();

  // Firefox text/html
  static const ClipboardFormatType& TextHtmlType();
  static const ClipboardFormatType& CFHDropType();
  static const ClipboardFormatType& FileDescriptorAType();
  static const ClipboardFormatType& FileDescriptorType();
  static const ClipboardFormatType& FileContentZeroType();
  static const ClipboardFormatType& FileContentAtIndexType(LONG index);
  static const ClipboardFormatType& FilenameType();
  static const ClipboardFormatType& IDListType();
  static const ClipboardFormatType& MozUrlType();

  // Type only used by Chromium to track the source URL of clipboard data.
  static const ClipboardFormatType& InternalSourceUrlType();

  // Prevents clipboard data from being included in the clipboard history.
  static const ClipboardFormatType& ClipboardHistoryType();
  // Prevents clipboard data from being included in the cloud clipboard.
  static const ClipboardFormatType& UploadCloudClipboardType();
#endif

  // For custom formats, individual types are added to the clipboard with a type
  // consisting of a prefix + index, and a map type that maps the custom format
  // type to the type used on the clipboard. On Windows/Linux, this is done
  // because there is a limited amount of system resources available to hold
  // format types, so they must be conserved. On the Mac, there is no limit, but
  // types must be named using strict alphanumerics, and MIME types do not
  // conform to that naming restriction, so types must be mapped, and therefore
  // the same mapping scheme is reused.

  // Returns the format identifier used for web custom format data on the
  // clipboard. Derived from the provided `index` value.
  static std::string WebCustomFormatName(int index);

  // Returns the ClipboardFormatType used for web custom format data,
  // registering it with the system if needed. Pass in a value obtained from
  // `WebCustomFormatName` above.
  static ClipboardFormatType CustomPlatformType(
      const std::string& format_string);

  // Returns the ClipboardFormatType used for the web custom format map that has
  // the mapping of MIME types to custom format names.
  static const ClipboardFormatType& WebCustomFormatMap();

  // ClipboardFormatType can be used in a set on some platforms.
  bool operator<(const ClipboardFormatType& other) const;

  // Returns a human-readable format name, or an empty string as an error value
  // if the format isn't found.
  std::string GetName() const;
#if BUILDFLAG(IS_WIN)
  const FORMATETC& ToFormatEtc() const { return *ChromeToWindowsType(&data_); }
#elif BUILDFLAG(IS_APPLE)
#if __OBJC__
  NSString* ToNSString() const;
#endif  // __OBJC__
  // Custom copy and assignment constructor to handle NSString.
  ClipboardFormatType(const ClipboardFormatType& other);
  ClipboardFormatType& operator=(const ClipboardFormatType& other);
#endif

  bool operator==(const ClipboardFormatType& other) const;

 private:
  friend class base::NoDestructor<ClipboardFormatType>;
  friend class Clipboard;

  // Platform-specific glue used internally by the ClipboardFormatType struct.
  // Each platform should define at least one of each of the following:
  // 1. A constructor that wraps that native clipboard format descriptor.
  // 2. An accessor to retrieve the wrapped descriptor.
  // 3. A data member to hold the wrapped descriptor.
  //
  // In some cases, the accessor for the wrapped descriptor may be public, as
  // these format types can be used by drag and drop code as well.
  //
  // In all platforms, format names may be ASCII or UTF8/16.
  // TODO(huangdarwin): Convert interfaces to std::u16string.
#if BUILDFLAG(IS_WIN)
  // When there are multiple files in the data store and they are described
  // using a file group descriptor, the file contents are retrieved by
  // requesting the CFSTR_FILECONTENTS clipboard format type and also providing
  // an index into the data (the first file corresponds to index 0). This
  // function returns a map of index to CFSTR_FILECONTENTS clipboard format
  // type.
  static std::map<LONG, ClipboardFormatType>& FileContentTypeMap();

  // FORMATETC:
  // https://docs.microsoft.com/en-us/windows/desktop/com/the-formatetc-structure
  CHROME_FORMATETC data_;
#elif defined(USE_AURA) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  explicit ClipboardFormatType(const std::string& native_format);
  std::string data_;
#elif BUILDFLAG(IS_APPLE)
#if __OBJC__
  explicit ClipboardFormatType(NSString* uttype);
#endif  // __OBJC__
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
#else
#error No ClipboardFormatType definition.
#endif
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_FORMAT_TYPE_H_
