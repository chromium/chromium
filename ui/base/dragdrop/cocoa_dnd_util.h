// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_COCOA_DND_UTIL_H_
#define UI_BASE_DRAGDROP_COCOA_DND_UTIL_H_

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/component_export.h"

class GURL;

namespace ui {

// Populates the |url| and |title| with URL data in |pboard|. There may be more
// than one, but we only handle dropping the first. |url| must not be |NULL|;
// |title| is an optional parameter. Returns |YES| if URL data was obtained from
// the pasteboard, |NO| otherwise. If |convert_filenames| is |YES|, the function
// will also attempt to convert filenames in |pboard| to file URLs.
COMPONENT_EXPORT(UI_BASE)
BOOL PopulateURLAndTitleFromPasteboard(GURL* url,
                                       std::u16string* title,
                                       NSPasteboard* pboard,
                                       BOOL convert_filenames);

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_COCOA_DND_UTIL_H_
