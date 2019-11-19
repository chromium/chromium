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

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // defined(OS_MACOSX)

namespace ui {

// Platform neutral holder for native data representation of a clipboard type.
// Copyable and assignable, since this is an opaque value type.
struct COMPONENT_EXPORT(BASE_CLIPBOARD_TYPES) ClipboardFormatType {
  ClipboardFormatType();
  ~ClipboardFormatType();

  // Serializes and deserializes a ClipboardFormatType for use in IPC messages.
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
  static const ClipboardFormatType& GetUrlWType();
  static const ClipboardFormatType& GetMozUrlType();
  static const ClipboardFormatType& GetPlainTextType();
  static const ClipboardFormatType& GetPlainTextWType();
  static const ClipboardFormatType& GetFilenameType();
  static const ClipboardFormatType& GetFilenameWType();
  static const ClipboardFormatType& GetWebKitSmartPasteType();
  // Win: MS HTML Format, Other: Generic HTML format
  static const ClipboardFormatType& GetHtmlType();
  static const ClipboardFormatType& GetRtfType();
  static const ClipboardFormatType& GetBitmapType();
  // TODO(raymes): Unify web custom data and pepper custom data:
  // crbug.com/158399.
  static const ClipboardFormatType& GetWebCustomDataType();
  static const ClipboardFormatType& GetPepperCustomDataType();

#if defined(OS_WIN)
  // Firefox text/html
  static const ClipboardFormatType& GetTextHtmlType();
  static const ClipboardFormatType& GetCFHDropType();
  static const ClipboardFormatType& GetFileDescriptorType();
  static const ClipboardFormatType& GetFileDescriptorWType();
  static const ClipboardFormatType& GetFileContentZeroType();
  static const ClipboardFormatType& GetFileContentAtIndexType(LONG index);
  static const ClipboardFormatType& GetIDListType();
#endif

#if defined(OS_ANDROID)
  static const ClipboardFormatType& GetBookmarkType();
#endif

  // ClipboardFormatType can be used in a set on some platforms.
  bool operator<(const ClipboardFormatType& other) const;

#if defined(OS_WIN)
  const FORMATETC& ToFormatEtc() const { return data_; }
#elif defined(USE_AURA) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
  const std::string& ToString() const { return data_; }
#elif defined(OS_MACOSX)
  NSString* ToNSString() const { return data_; }
  // Custom copy and assignment constructor to handle NSString.
  ClipboardFormatType(const ClipboardFormatType& other);
  ClipboardFormatType& operator=(const ClipboardFormatType& other);
#endif

  bool Equals(const ClipboardFormatType& other) const;

 private:
  friend class base::NoDestructor<ClipboardFormatType>;
  friend struct ClipboardFormatType;

  // Platform-specific glue used internally by the ClipboardFormatType struct.
  // Each platform should define at least one of each of the following:
  // 1. A constructor that wraps that native clipboard format descriptor.
  // 2. An accessor to retrieve the wrapped descriptor.
  // 3. A data member to hold the wrapped descriptor.
  //
  // Note that in some cases, the accessor for the wrapped descriptor may be
  // public, as these format types can be used by drag and drop code as well.
#if defined(OS_WIN)
  explicit ClipboardFormatType(UINT native_format);
  ClipboardFormatType(UINT native_format, LONG index);
  ClipboardFormatType(UINT native_format, LONG index, DWORD tymed);

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
#elif defined(OS_MACOSX)
  explicit ClipboardFormatType(NSString* native_format);
  NSString* data_;
#else
#error No ClipboardFormatType definition.
#endif
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_FORMAT_TYPE_H_
