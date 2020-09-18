// Copyright 2019 The Chromium Authors. All rights reserved.
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

#if defined(OS_WIN)
#include <objidl.h>
#endif

#if defined(OS_APPLE)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // defined(OS_APPLE)

namespace ui {

// Platform neutral holder for native data representation of a clipboard type.
// Copyable and assignable, since this is an opaque value type.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) ClipboardFormatType {
 public:
  ClipboardFormatType();
  ~ClipboardFormatType();

#if defined(OS_WIN)
  explicit ClipboardFormatType(UINT native_format);
  ClipboardFormatType(UINT native_format, LONG index);
  ClipboardFormatType(UINT native_format, LONG index, DWORD tymed);
#endif

  // Serializes and deserializes a ClipboardFormatType for use in IPC messages.
  // The serialized string may not be human-readable.
  std::string Serialize() const;
  static ClipboardFormatType Deserialize(const std::string& serialization);

  // Gets the ClipboardFormatType corresponding to an arbitrary format string,
  // registering it with the system if needed. Due to Windows/Linux
  // limitations, please place limits on the amount of GetType calls with unique
  // |format_string| arguments, when ingesting |format_string| from
  // untrusted sources, such as renderer processes. In Windows, a failure will
  // return an invalid format with Deserialize()'ed value of "0".
  static ClipboardFormatType GetType(const std::string& format_string);

  // Get format identifiers for various types.
  static const ClipboardFormatType& GetUrlType();
  static const ClipboardFormatType& GetPlainTextType();
  static const ClipboardFormatType& GetWebKitSmartPasteType();
  // Win: MS HTML Format, Other: Generic HTML format
  static const ClipboardFormatType& GetHtmlType();
  static const ClipboardFormatType& GetSvgType();
  static const ClipboardFormatType& GetRtfType();
  static const ClipboardFormatType& GetBitmapType();
  // TODO(raymes): Unify web custom data and pepper custom data:
  // crbug.com/158399.
  static const ClipboardFormatType& GetWebCustomDataType();
  static const ClipboardFormatType& GetPepperCustomDataType();

#if defined(OS_WIN)
  // ANSI formats. Only Windows differentiates between ANSI and UNICODE formats
  // in ClipboardFormatType. Reference:
  // https://docs.microsoft.com/en-us/windows/win32/learnwin32/working-with-strings
  static const ClipboardFormatType& GetUrlAType();
  static const ClipboardFormatType& GetPlainTextAType();
  static const ClipboardFormatType& GetFilenameAType();

  // Firefox text/html
  static const ClipboardFormatType& GetTextHtmlType();
  static const ClipboardFormatType& GetCFHDropType();
  static const ClipboardFormatType& GetFileDescriptorAType();
  static const ClipboardFormatType& GetFileDescriptorType();
  static const ClipboardFormatType& GetFileContentZeroType();
  static const ClipboardFormatType& GetFileContentAtIndexType(LONG index);
  static const ClipboardFormatType& GetFilenameType();
  static const ClipboardFormatType& GetIDListType();
  static const ClipboardFormatType& GetMozUrlType();
#endif

  // ClipboardFormatType can be used in a set on some platforms.
  bool operator<(const ClipboardFormatType& other) const;

  // Returns a human-readable format name, or an empty string as an error value
  // if the format isn't found.
  std::string GetName() const;
#if defined(OS_WIN)
  const FORMATETC& ToFormatEtc() const { return data_; }
#elif defined(OS_APPLE)
  NSString* ToNSString() const { return data_; }
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
  // TODO(huangdarwin): Convert interfaces to base::string16.
#if defined(OS_WIN)
  // When there are multiple files in the data store and they are described
  // using a file group descriptor, the file contents are retrieved by
  // requesting the CFSTR_FILECONTENTS clipboard format type and also providing
  // an index into the data (the first file corresponds to index 0). This
  // function returns a map of index to CFSTR_FILECONTENTS clipboard format
  // type.
  static std::map<LONG, ClipboardFormatType>& GetFileContentTypeMap();

  // FORMATETC:
  // https://docs.microsoft.com/en-us/windows/desktop/com/the-formatetc-structure
  FORMATETC data_;
#elif defined(USE_AURA) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
  explicit ClipboardFormatType(const std::string& native_format);
  std::string data_;
#elif defined(OS_APPLE)
  explicit ClipboardFormatType(NSString* native_format);
  NSString* data_;
#else
#error No ClipboardFormatType definition.
#endif
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_FORMAT_TYPE_H_
