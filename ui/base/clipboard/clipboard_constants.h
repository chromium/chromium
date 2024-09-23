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

// ----- PLATFORM NEUTRAL MIME TYPES -----

COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeText[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeTextUtf8[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeURIList[];
// Non-standard type for downloading files after drop events. Only works on
// Windows. See https://crbug.com/860557 and https://crbug.com/425170.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDownloadURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeMozillaURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeHTML[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeHTMLUtf8[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeSvg[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeRTF[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypePNG[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeOctetStream[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWindowDrag[];

// ----- CHROMEOS MIME TYPES -----

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDataTransferEndpoint[];
#endif  // BUILDFLAG(IS_CHROMEOS)

// ----- LINUX & CHROMEOS & FUCHSIA MIME TYPES -----

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxUtf8String[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxString[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxText[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeLinuxSourceUrl[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

// ----- EVERYTHING EXCEPT FOR APPLE MIME TYPES -----

#if !BUILDFLAG(IS_APPLE)
// TODO(dcheng): This name is temporary. See crbug.com/106449.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDataTransferCustomData[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWebkitSmartPaste[];
#else

// ----- APPLE UNIFORM TYPES -----

// Mail.app and TextEdit accept drags that have both HTML and image types on
// them, but don't process them correctly <http://crbug.com/55879>. Therefore,
// if there is an image type, don't put the HTML data on as HTML, but rather
// put it on as this Chrome-only type. External apps won't see HTML but
// Chrome will know enough to read it as HTML. <http://crbug.com/55879>
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumImageAndHTML;

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
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern NSString* const kUTTypeURLName;

// A type specifying that WebKit or a WebKit-derived browser engine like Blink
// was the last to write to the pasteboard. There's no data associated with this
// type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeWebKitWebSmartPaste;

// A type used by WebKit to add an array of URLs with titles to the clipboard.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeWebKitWebURLsWithTitles;

// A type used to track the source URL of data put in the clipboard.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kUTTypeChromiumSourceURL;

#endif  // BUILDFLAG(IS_APPLE)

// ----- ANDROID MIME TYPES -----

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeImageURI[];
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// ----- OTHER RELATED CONSTANTS -----

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
