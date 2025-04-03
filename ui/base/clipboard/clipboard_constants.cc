// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#include "build/build_config.h"

namespace ui {

const char kMimeTypePlainText[] = "text/plain";
const char kMimeTypeUtf8PlainText[] = "text/plain;charset=utf-8";
// Used for file:// URLs.
const char kMimeTypeUriList[] = "text/uri-list";
// Used for site URL bookmarks.
const char kMimeTypeMozillaUrl[] = "text/x-moz-url";
const char kMimeTypeDownloadUrl[] = "downloadurl";
const char kMimeTypeHtml[] = "text/html";
const char kMimeTypeUtf8Html[] = "text/html;charset=utf-8";
const char kMimeTypeSvg[] = "image/svg+xml";
const char kMimeTypeRtf[] = "text/rtf";
const char kMimeTypePng[] = "image/png";
// Used for image drag & drop on X11 and Wayland.
const char kMimeTypeOctetStream[] = "application/octet-stream";
// Used for window dragging on some platforms.
const char kMimeTypeWindowDrag[] = "chromium/x-window-drag";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
const char kMimeTypeLinuxUtf8String[] = "UTF8_STRING";
const char kMimeTypeLinuxString[] = "STRING";
const char kMimeTypeLinuxText[] = "TEXT";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_ANDROID)
const char kMimeTypeSourceUrl[] = "chromium/x-source-url";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_APPLE)
const char kMimeTypeDataTransferCustomData[] = "chromium/x-web-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const char kMimeTypeImageUri[] = "image-uri";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

const int kMaxRegisteredClipboardFormats = 100;
const char kWebClipboardFormatPrefix[] = "web ";

}  // namespace ui
