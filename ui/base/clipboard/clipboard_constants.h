// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_


#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // BUILDFLAG(IS_APPLE)

namespace ui {

// Platform-Neutral MIME type constants.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeText[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeTextUtf8[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeURIList[];
// Unstandardized format for downloading files after drop events. Now only
// works in Windows, but used to also work in Linux and MacOS.
// See https://crbug.com/860557 and https://crbug.com/425170.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDownloadURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeMozillaURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeHTML[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeSvg[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeRTF[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypePNG[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeOctetStream[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWindowDrag[];

// Chrome OS-specific MIME type constants.
#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDataTransferEndpoint[];
#endif  // BUILDFLAG(IS_CHROMEOS)

// Linux-specific MIME type constants (also used in Fuchsia).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxUtf8String[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxString[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxText[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

#if !BUILDFLAG(IS_APPLE)
// TODO(dcheng): This name is temporary. See crbug.com/106449.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWebCustomData[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWebkitSmartPaste[];
#else
// MacOS-specific Uniform Type Identifiers.

// SVG images.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kImageSvg;

// Pickled data.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kWebCustomDataPboardType;

// Tells us if WebKit was the last to write to the pasteboard. There's no
// actual data associated with this type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kWebSmartPastePboardType;

// Data format used to tag the current data as confidential.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeConfidentialData;

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeImageURI[];
#endif  // BUILDFLAG(IS_ANDROID)

// Max number of custom formats which can be registered per write operation.
// Windows / X11 clipboards enter an unrecoverable state after registering
// some amount of unique formats, and there's no way to un-register these
// formats. For these clipboards, we use a conservative limit to avoid
// registering too many formats.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const int kMaxRegisteredClipboardFormats;

// Web prefix for web custom format types.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kWebClipboardFormatPrefix[];

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
