// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include <AppKit/AppKit.h>
#include <CoreServices/CoreServices.h>                      // pre-macOS 11
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>  // macOS 11

#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

namespace {

// Reads the "WebKitWebURLsWithTitles" type put onto the clipboard by Safari and
// returns the urls/titles found within. Returns true if this was successful, or
// false if it was not.
bool ReadWebURLsWithTitlesPboardType(NSPasteboard* pboard,
                                     NSArray<NSString*>** urls,
                                     NSArray<NSString*>** titles) {
  NSArray* bookmark_pairs = base::mac::ObjCCast<NSArray>(
      [pboard propertyListForType:kUTTypeWebKitWebURLsWithTitles]);
  if (!bookmark_pairs) {
    return false;
  }
  if (bookmark_pairs.count != 2) {
    return false;
  }

  NSArray<NSString*>* urls_array =
      base::mac::ObjCCast<NSArray>(bookmark_pairs[0]);
  NSArray<NSString*>* titles_array =
      base::mac::ObjCCast<NSArray>(bookmark_pairs[1]);

  if (!urls_array || !titles_array) {
    return false;
  }
  if (urls_array.count < 1) {
    return false;
  }
  if (urls_array.count != titles_array.count) {
    return false;
  }
  for (id obj in urls_array) {
    if (![obj isKindOfClass:[NSString class]]) {
      return false;
    }
  }

  for (id obj in titles_array) {
    if (![obj isKindOfClass:[NSString class]]) {
      return false;
    }
  }

  *urls = urls_array;
  *titles = titles_array;
  return true;
}

// If the given pasteboard item is a drag of an internet location file, return
// the title that should be used for for the URL. Returns nil if it is not a
// file or if any error occurs.
NSString* ExtractTitleFromFileURL(NSPasteboardItem* item) {
  NSURL* file_url =
      [NSURL URLWithString:[item stringForType:NSPasteboardTypeFileURL]];
  if (!file_url) {
    return nil;
  }

  // The UTType for .webloc files is com.apple.web-internet-location, but
  // there is no official constant for that. However, that type does conform
  // to the generic "internet location" type (aka .inetloc), so check for
  // that.
  NSDictionary* resource_values;
  if (@available(macOS 11, *)) {
    resource_values = [file_url resourceValuesForKeys:@[
      NSURLContentTypeKey, NSURLLocalizedNameKey, NSURLHasHiddenExtensionKey
    ]
                                                error:nil];
    if (!resource_values) {
      return nil;
    }

    UTType* type = resource_values[NSURLContentTypeKey];
    if (!type || ![type conformsToType:UTTypeInternetLocation]) {
      return nil;
    }
  } else {
    resource_values = [file_url resourceValuesForKeys:@[
      NSURLTypeIdentifierKey, NSURLLocalizedNameKey, NSURLHasHiddenExtensionKey
    ]
                                                error:nil];
    if (!resource_values) {
      return nil;
    }

    NSString* type = resource_values[NSURLTypeIdentifierKey];
    if (!type ||
        ![NSWorkspace.sharedWorkspace
                      type:type
            conformsToType:base::mac::CFToNSCast(kUTTypeInternetLocation)]) {
      return nil;
    }
  }

  NSString* title = resource_values[NSURLLocalizedNameKey];
  if (!title) {
    return nil;
  }

  NSNumber* has_hidden_extension = resource_values[NSURLHasHiddenExtensionKey];
  if (!has_hidden_extension || has_hidden_extension.boolValue) {
    // If it's already hidden, or it's unknown if it's hidden, return it
    // unaltered.
    return title;
  }

  return [title stringByDeletingPathExtension];
}

// Reads the given pasteboard, and returns urls/titles found on it. Returns true
// if at least one url was found on the pasteboard, and false if none were.
bool ReadURLItemsWithTitles(NSPasteboard* pboard,
                            NSArray<NSString*>** urls,
                            NSArray<NSString*>** titles) {
  NSMutableArray<NSString*>* urls_array = [NSMutableArray array];
  NSMutableArray<NSString*>* titles_array = [NSMutableArray array];

  NSArray<NSPasteboardItem*>* items = pboard.pasteboardItems;
  for (NSPasteboardItem* item : items) {
    NSString* url = [item stringForType:NSPasteboardTypeURL];
    NSString* title = [item stringForType:kUTTypeURLName];

    if (!url) {
      continue;
    }

    if (!title) {
      // If there is no title on the drag, check to see if it's a URL drag
      // reconstituted from a Finder .webloc. If so, use the name of the file as
      // the title.
      title = ExtractTitleFromFileURL(item);
    }

    if (url) {
      [urls_array addObject:url];
      if (title) {
        [titles_array addObject:title];
      } else {
        [titles_array addObject:@""];
      }
    }
  }

  if (urls_array.count) {
    *urls = urls_array;
    *titles = titles_array;
    return true;
  } else {
    return false;
  }
}

}  // namespace

UniquePasteboard::UniquePasteboard()
    : pasteboard_([[NSPasteboard pasteboardWithUniqueName] retain]) {}

UniquePasteboard::~UniquePasteboard() {
  [pasteboard_ releaseGlobally];
}

NSArray<NSPasteboardItem*>* ClipboardUtil::PasteboardItemsFromUrls(
    NSArray<NSString*>* urls,
    NSArray<NSString*>* titles) {
  DCHECK_EQ(urls.count, titles.count);

  NSMutableArray<NSPasteboardItem*>* items = [NSMutableArray array];

  for (NSUInteger i = 0; i < urls.count; ++i) {
    NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];

    NSString* url_string = urls[i];
    NSString* title = titles[i];

    NSURL* url = [NSURL URLWithString:url_string];
    if (url.isFileURL &&
        [NSFileManager.defaultManager fileExistsAtPath:url.path]) {
      [item setString:url_string forType:NSPasteboardTypeFileURL];
    }

    [item setString:url_string forType:NSPasteboardTypeString];
    [item setString:url_string forType:NSPasteboardTypeURL];
    if (title.length) {
      [item setString:title forType:kUTTypeURLName];
    }

    // Safari puts the "Web URLs and Titles" pasteboard type onto the first
    // pasteboard item.
    if (i == 0) {
      [item setPropertyList:@[ urls, titles ]
                    forType:kUTTypeWebKitWebURLsWithTitles];
    }

    [items addObject:item];
  }

  return items;
}

void ClipboardUtil::AddDataToPasteboard(NSPasteboard* pboard,
                                        NSPasteboardItem* item) {
  NSSet* old_types = [NSSet setWithArray:[pboard types]];
  NSMutableSet* new_types = [NSMutableSet setWithArray:[item types]];
  [new_types minusSet:old_types];

  [pboard addTypes:[new_types allObjects] owner:nil];
  for (NSString* type in new_types) {
    // Technically, the object associated with |type| might be an NSString or a
    // property list. It doesn't matter though, since the type gets pulled from
    // and shoved into an NSDictionary.
    [pboard setData:[item dataForType:type] forType:type];
  }
}

bool ClipboardUtil::URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                                NSArray<NSString*>** urls,
                                                NSArray<NSString*>** titles) {
  return ReadWebURLsWithTitlesPboardType(pboard, urls, titles) ||
         ReadURLItemsWithTitles(pboard, urls, titles);
}

NSPasteboard* ClipboardUtil::PasteboardFromBuffer(ClipboardBuffer buffer) {
  NSString* buffer_type = nil;
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste:
      buffer_type = NSPasteboardNameGeneral;
      break;
    case ClipboardBuffer::kDrag:
      buffer_type = NSPasteboardNameDrag;
      break;
    case ClipboardBuffer::kSelection:
      NOTREACHED();
      break;
  }

  return [NSPasteboard pasteboardWithName:buffer_type];
}

NSString* ClipboardUtil::GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard) {
  NSData* rtf_data = [pboard dataForType:NSPasteboardTypeRTF];
  if (!rtf_data)
    return nil;

  NSAttributedString* attributed =
      [[[NSAttributedString alloc] initWithRTF:rtf_data
                            documentAttributes:nil] autorelease];
  NSData* html_data =
      [attributed dataFromRange:NSMakeRange(0, attributed.length)
             documentAttributes:@{
               NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
             }
                          error:nil];

  // According to the docs, NSHTMLTextDocumentType is UTF-8.
  return [[[NSString alloc] initWithData:html_data
                                encoding:NSUTF8StringEncoding] autorelease];
}

}  // namespace ui
