// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Dummy data type that Chrome places in dragging pasteboards. There is never
// any data associated with this type; it's only used to ensure that Chrome
// supports any drag initiated inside of Chrome, whether or not data has been
// associated with it.
COMPONENT_EXPORT(UI_BASE) extern NSString* const kChromeDragDummyPboardType;

// Mail.app and TextEdit accept drags that have both HTML and image flavors on
// them, but don't process them correctly <http://crbug.com/55879>. Therefore,
// if there is an image flavor, don't put the HTML data on as HTML, but rather
// put it on as this Chrome-only flavor. External apps won't see HTML but
// Chrome will know enough to read it as HTML. <http://crbug.com/55879>
COMPONENT_EXPORT(UI_BASE) extern NSString* const kChromeDragImageHTMLPboardType;

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
