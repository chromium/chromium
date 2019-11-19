// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

const char kMimeTypeText[] = "text/plain";
const char kMimeTypeTextUtf8[] = "text/plain;charset=utf-8";
const char kMimeTypeURIList[] = "text/uri-list";
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
// Unstandardized format for downloading files  after drop events. Now only
// works in Windows, but used to also work in Linux and MacOS.
// See https://crbug.com/860557 and https://crbug.com/425170.
const char kMimeTypeDownloadURL[] = "downloadurl";
const char kMimeTypeHTML[] = "text/html";
const char kMimeTypeRTF[] = "text/rtf";
const char kMimeTypePNG[] = "image/png";
#if !defined(OS_MACOSX)
// TODO(dcheng): This name is temporary. See crbug.com/106449.
const char kMimeTypeWebCustomData[] = "chromium/x-web-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
#endif  // defined(OS_MACOSX)
}  // namespace ui
