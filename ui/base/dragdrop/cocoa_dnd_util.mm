// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/dragdrop/cocoa_dnd_util.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "url/gurl.h"

namespace ui {

NSString* const kChromeDragDummyPboardType = @"org.chromium.drag-dummy-type";

NSString* const kChromeDragImageHTMLPboardType = @"org.chromium.image-html";

BOOL PopulateURLAndTitleFromPasteboard(GURL* url,
                                       base::string16* title,
                                       NSPasteboard* pboard,
                                       BOOL convert_filenames) {
  CHECK(url);

  // Bail out early if there's no URL data.
  if (![pboard containsURLDataConvertingTextToURL:YES])
    return NO;

  // -getURLs:andTitles:convertingFilenames: will already validate URIs so we
  // don't need to again. The arrays returned are both of NSStrings.
  NSArray* url_array = nil;
  NSArray* title_array = nil;
  [pboard getURLs:&url_array
                andTitles:&title_array
      convertingFilenames:convert_filenames
      convertingTextToURL:YES];
  DCHECK_EQ([url_array count], [title_array count]);
  // It's possible that no URLs were actually provided!
  if (![url_array count])
    return NO;
  NSString* url_string = url_array[0];
  if ([url_string length]) {
    // Check again just to make sure to not assign NULL into a std::string,
    // which throws an exception.
    const char* utf8_url = [url_string UTF8String];
    if (utf8_url) {
      *url = GURL(utf8_url);
      // Extra paranoia check.
      if (title && [title_array count])
        *title = base::SysNSStringToUTF16(title_array[0]);
    }
  }
  return YES;
}

}  // namespace ui
