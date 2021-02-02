// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

const char kMimeTypeText[] = "text/plain";
const char kMimeTypeTextUtf8[] = "text/plain;charset=utf-8";
// Used for file:// URLs.
const char kMimeTypeURIList[] = "text/uri-list";
// Used for site URL bookmarks.
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypeDownloadURL[] = "downloadurl";
const char kMimeTypeHTML[] = "text/html";
const char kMimeTypeSvg[] = "image/svg+xml";
const char kMimeTypeRTF[] = "text/rtf";
const char kMimeTypePNG[] = "image/png";

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
const char kMimeTypeLinuxUtf8String[] = "UTF8_STRING";
const char kMimeTypeLinuxString[] = "STRING";
const char kMimeTypeLinuxText[] = "TEXT";
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

#if !defined(OS_APPLE)
const char kMimeTypeWebCustomData[] = "chromium/x-web-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
#endif  // defined(OS_APPLE)

#if defined(OS_ANDROID)
const char kMimeTypeImageURI[] = "image-uri";
#endif  // defined(OS_ANDROID)
}  // namespace ui
