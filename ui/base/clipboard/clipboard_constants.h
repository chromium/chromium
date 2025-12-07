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
#endif
#endif  // BUILDFLAG(IS_APPLE)

namespace ui {

// ----- PLATFORM NEUTRAL MIME TYPES -----

inline constexpr char kMimeTypePlainText[] = "text/plain";
inline constexpr char16_t kMimeTypePlainText16[] = u"text/plain";
inline constexpr char kMimeTypeUtf8PlainText[] = "text/plain;charset=utf-8";
inline constexpr char kMimeTypeUriList[] = "text/uri-list";
inline constexpr char16_t kMimeTypeUriList16[] = u"text/uri-list";
// Non-standard type for downloading files after drop events. Only works on
// Windows. See https://crbug.com/41399675 and https://crbug.com/40390016.
inline constexpr char kMimeTypeDownloadUrl[] = "downloadurl";
inline constexpr char kMimeTypeMozillaUrl[] = "text/x-moz-url";
inline constexpr char16_t kMimeTypeMozillaUrl16[] = u"text/x-moz-url";
inline constexpr char kMimeTypeHtml[] = "text/html";
inline constexpr char16_t kMimeTypeHtml16[] = u"text/html";
inline constexpr char kMimeTypeUtf8Html[] = "text/html;charset=utf-8";
inline constexpr char kMimeTypeSvg[] = "image/svg+xml";
inline constexpr char16_t kMimeTypeSvg16[] = u"image/svg+xml";
inline constexpr char kMimeTypeRtf[] = "text/rtf";
inline constexpr char16_t kMimeTypeRtf16[] = u"text/rtf";
inline constexpr char kMimeTypePng[] = "image/png";
inline constexpr char16_t kMimeTypePng16[] = u"image/png";
// Used for image drag & drop on X11 and Wayland.
inline constexpr char kMimeTypeOctetStream[] = "application/octet-stream";
// Used for window dragging on some platforms.
inline constexpr char kMimeTypeWindowDrag[] = "chromium/x-window-drag";

// ----- LINUX & CHROMEOS & FUCHSIA MIME TYPES -----

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
inline constexpr char kMimeTypeLinuxUtf8String[] = "UTF8_STRING";
inline constexpr char kMimeTypeLinuxString[] = "STRING";
inline constexpr char kMimeTypeLinuxText[] = "TEXT";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_ANDROID)
inline constexpr char kMimeTypeSourceUrl[] = "chromium/x-source-url";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)

// ----- EVERYTHING EXCEPT FOR APPLE MIME TYPES -----

#if !BUILDFLAG(IS_APPLE)

// TODO(dcheng): This name is temporary. See https://crbug.com/40123727.
inline constexpr char kMimeTypeDataTransferCustomData[] =
    "chromium/x-web-custom-data";
inline constexpr char16_t kMimeTypeDataTransferCustomData16[] =
    u"chromium/x-web-custom-data";
inline constexpr char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";

#else

// ----- APPLE UNIFORM TYPES -----

#ifdef __OBJC__

// Mail.app and TextEdit accept drags that have both HTML and image types on
// them, but don't process them correctly <https://crbug.com/40445637>.
// Therefore, if there is an image type, don't put the HTML data on as HTML, but
// rather put it on as this Chrome-only type. External apps won't see HTML but
// Chrome will know enough to read it as HTML.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumImageAndHtml;

// Data type always placed on dragging pasteboards. There is never any data
// associated with this type; it's only used to ensure that Chromium supports
// any drag initiated inside of Chromium, whether or not data has been
// associated with it.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumInitiatedDrag;

// Data type placed on dragging pasteboards when the drag is initiated from a
// renderer that is privileged. There is never any data associated with this
// type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumPrivilegedInitiatedDrag;

// Data type placed on dragging pasteboards when the drag is initiated from a
// renderer. If the initiator has a tuple origin (e.g. https://example.com),
// the data is a string representation (i.e. the result of calling
// `url::Origin::Serialize()`). Otherwise, the initiator has an opaque origin
// and the data is the empty string.
//
// This format is intentionally chosen for safer backwards compatibility with
// previous versions of Chrome, which always set an empty string for the data.
// When newer versions of Chrome attempt to interpret this data as an origin,
// they will safely treat it as a unique opaque origin.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumRendererInitiatedDrag;

// A type specifying DataTransfer custom data. The data is pickled.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumDataTransferCustomData;

// It is the common convention on the Mac and on iOS that password managers tag
// confidential data with this type. There's no data associated with this
// type. See http://nspasteboard.org/ for more info.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeConfidentialData;

// A publicly-used type for the name of a URL.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern NSString* const kUTTypeUrlName;

// A type specifying that WebKit or a WebKit-derived browser engine like Blink
// was the last to write to the pasteboard. There's no data associated with this
// type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeWebKitWebSmartPaste;

// A type used by WebKit to add an array of URLs with titles to the clipboard.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeWebKitWebUrlsWithTitles;

// A type used to track the source URL of data put in the clipboard.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumSourceUrl;

#endif  //  __OBJC__

#endif  // BUILDFLAG(IS_APPLE)

// ----- ANDROID MIME TYPES -----

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
inline constexpr char kMimeTypeImageUri[] = "image-uri";
inline constexpr char16_t kMimeTypeImageUri16[] = u"image-uri";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// ----- OTHER RELATED CONSTANTS -----

// Max number of custom formats which can be registered per write operation.
// Windows / X11 clipboards enter an unrecoverable state after registering
// some amount of unique formats, and there's no way to un-register these
// formats. For these clipboards, we use a conservative limit to avoid
// registering too many formats.
inline constexpr int kMaxRegisteredClipboardFormats = 100;

// Web prefix for web custom format types.
inline constexpr char kWebClipboardFormatPrefix[] = "web ";
inline constexpr char16_t kWebClipboardFormatPrefix16[] = u"web ";

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
