// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// A publicly-used UTI for the name of a URL. It really should be in a system
// header but isn't.
UI_BASE_EXPORT extern NSString* const kUTTypeURLName;

class UI_BASE_EXPORT UniquePasteboard
    : public base::RefCounted<UniquePasteboard> {
 public:
  UniquePasteboard();

  NSPasteboard* get() { return pasteboard_; }

 private:
  friend class base::RefCounted<UniquePasteboard>;
  ~UniquePasteboard();
  base::scoped_nsobject<NSPasteboard> pasteboard_;
};

class UI_BASE_EXPORT ClipboardUtil {
 public:
  // Returns an NSPasteboardItem that represents the given |url|.
  // |url| must not be nil.
  // If |title| is nil, |url| is used in its place.
  static base::scoped_nsobject<NSPasteboardItem> PasteboardItemFromUrl(
      NSString* url,
      NSString* title);

  // Returns an NSPasteboardItem that represents the given |urls| and |titles|.
  static base::scoped_nsobject<NSPasteboardItem> PasteboardItemFromUrls(
      NSArray* urls,
      NSArray* titles);

  // Returns an NSPasteboardItem that represents the given string.
  // |string| must not be nil.
  static base::scoped_nsobject<NSPasteboardItem> PasteboardItemFromString(
      NSString* string);

  // Returns the title or url associated with a NSPasteboard which contains an
  // url NSPasteboardItem.
  static NSString* GetTitleFromPasteboardURL(NSPasteboard* pboard);
  static NSString* GetURLFromPasteboardURL(NSPasteboard* pboard);

  // Returns the UTI of a pasteboard type.
  static NSString* UTIForPasteboardType(NSString* type);
  static NSString* UTIForWebURLsAndTitles();

  // For each pasteboard type in |item| that is not in |pboard|, add the type
  // and its associated data.
  static void AddDataToPasteboard(NSPasteboard* pboard, NSPasteboardItem* item);

  // Returns whether the operation was succesful. On success, the two arrays are
  // guaranteed to be equal length, and are populated with strings of |urls| and
  // |titles|.
  static bool URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                          NSArray** urls,
                                          NSArray** titles);

  // Gets the NSPasteboard specified from the clipboard type.
  static NSPasteboard* PasteboardFromType(ui::ClipboardType type);
};
}

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
