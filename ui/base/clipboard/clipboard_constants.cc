// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#include "build/build_config.h"

namespace ui {

const char kMimeTypeText[] = "text/plain";
const char kMimeTypeTextUtf8[] = "text/plain;charset=utf-8";
// Used for file:// URLs.
const char kMimeTypeURIList[] = "text/uri-list";
// Used for site URL bookmarks.
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypeDownloadURL[] = "downloadurl";
const char kMimeTypeHTML[] = "text/html";
const char kMimeTypeHTMLUtf8[] = "text/html;charset=utf-8";
const char kMimeTypeSvg[] = "image/svg+xml";
const char kMimeTypeRTF[] = "text/rtf";
const char kMimeTypePNG[] = "image/png";
// Used for image drag & drop from LaCrOS.
const char kMimeTypeOctetStream[] = "application/octet-stream";
// Used for window dragging on some platforms.
const char kMimeTypeWindowDrag[] = "chromium/x-window-drag";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
const char kMimeTypeLinuxUtf8String[] = "UTF8_STRING";
const char kMimeTypeLinuxString[] = "STRING";
const char kMimeTypeLinuxText[] = "TEXT";
const char kMimeTypeLinuxSourceUrl[] = "chromium/x-source-url";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
// Used to sync clipboard source metadata between Ash and LaCrOS.
const char kMimeTypeDataTransferEndpoint[] =
    "chromium/x-data-transfer-endpoint";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_APPLE)
const char kMimeTypeDataTransferCustomData[] = "chromium/x-web-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const char kMimeTypeImageURI[] = "image-uri";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

const int kMaxRegisteredClipboardFormats = 100;
const char kWebClipboardFormatPrefix[] = "web ";

}  // namespace ui
