// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_

#import <AppKit/AppKit.h>

#include <vector>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) UniquePasteboard
    : public base::RefCounted<UniquePasteboard> {
 public:
  UniquePasteboard();

  NSPasteboard* get() { return pasteboard_; }

 private:
  friend class base::RefCounted<UniquePasteboard>;
  ~UniquePasteboard();
  base::scoped_nsobject<NSPasteboard> pasteboard_;
};

namespace ClipboardUtil {

// Returns an array of NSPasteboardItems that represent the given `urls` and
// `titles`.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
NSArray<NSPasteboardItem*>* PasteboardItemsFromUrls(NSArray<NSString*>* urls,
                                                    NSArray<NSString*>* titles);

// For each pasteboard type in `item` that is not in `pboard`, add the type
// and its associated data.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void AddDataToPasteboard(NSPasteboard* pboard, NSPasteboardItem* item);

// For a given pasteboard, reads and extracts the URLs to be found on it. If
// `include_files` is set, then any file references on the pasteboard will be
// returned as file URLs. The two out-parameter arrays are guaranteed to be the
// same length when this function completes. Returns true if at least one URL
// was successfully read, and false otherwise.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                 bool include_files,
                                 NSArray<NSString*>** urls,
                                 NSArray<NSString*>** titles);

// For a given pasteboard, extracts the files found on it.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
std::vector<FileInfo> FilesFromPasteboard(NSPasteboard* pboard);

// Writes files to a given pasteboard.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void WriteFilesToPasteboard(NSPasteboard* pboard,
                            const std::vector<FileInfo>& files);

// Gets the NSPasteboard specified from the clipboard buffer.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
NSPasteboard* PasteboardFromBuffer(ClipboardBuffer buffer);

// If there is RTF data on the pasteboard, returns an HTML version of it.
// Otherwise returns nil.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
NSString* GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard);

}  // namespace ClipboardUtil

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
