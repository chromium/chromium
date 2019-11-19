// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"

namespace ui {

NSString* const kUTTypeURLName = @"public.url-name";

namespace {

NSString* const kWebURLsWithTitlesPboardType = @"WebURLsWithTitlesPboardType";

// It's much more convenient to return an NSString than a
// base::ScopedCFTypeRef<CFStringRef>, since the methods on NSPasteboardItem
// require an NSString*.
NSString* UTIFromPboardType(NSString* type) {
  return [base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassNSPboardType, base::mac::NSToCFCast(type), kUTTypeData))
      autorelease];
}

bool ReadWebURLsWithTitlesPboardType(NSPasteboard* pboard,
                                     NSArray** urls,
                                     NSArray** titles) {
  NSArray* bookmarkPairs = base::mac::ObjCCast<NSArray>([pboard
      propertyListForType:UTIFromPboardType(kWebURLsWithTitlesPboardType)]);
  if (!bookmarkPairs)
    return false;

  if ([bookmarkPairs count] != 2)
    return false;

  NSArray* urlsArr = base::mac::ObjCCast<NSArray>(bookmarkPairs[0]);
  NSArray* titlesArr = base::mac::ObjCCast<NSArray>(bookmarkPairs[1]);

  if (!urlsArr || !titlesArr)
    return false;
  if ([urlsArr count] < 1)
    return false;
  if ([urlsArr count] != [titlesArr count])
    return false;

  for (id obj in urlsArr) {
    if (![obj isKindOfClass:[NSString class]])
      return false;
  }

  for (id obj in titlesArr) {
    if (![obj isKindOfClass:[NSString class]])
      return false;
  }

  *urls = urlsArr;
  *titles = titlesArr;
  return true;
}

bool ReadURLItemsWithTitles(NSPasteboard* pboard,
                            NSArray** urls,
                            NSArray** titles) {
  NSMutableArray* urlsArr = [NSMutableArray array];
  NSMutableArray* titlesArr = [NSMutableArray array];

  NSArray* items = [pboard pasteboardItems];
  for (NSPasteboardItem* item : items) {
    NSString* url = [item stringForType:base::mac::CFToNSCast(kUTTypeURL)];
    NSString* title = [item stringForType:kUTTypeURLName];

    if (url) {
      [urlsArr addObject:url];
      if (title)
        [titlesArr addObject:title];
      else
        [titlesArr addObject:@""];
    }
  }

  if ([urlsArr count]) {
    *urls = urlsArr;
    *titles = titlesArr;
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

  if (base::mac::IsOS10_12()) {
    // On 10.12, move ownership to the autorelease pool rather than possibly
    // triggering -[NSPasteboard dealloc] here. This is a speculative workaround
    // for https://crbug.com/877979 where a call to __CFPasteboardDeallocate
    // from here is triggering "Semaphore object deallocated while in use".
    pasteboard_.autorelease();
  }
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromUrl(
    NSString* urlString,
    NSString* title) {
  DCHECK(urlString);
  if (!title)
    title = urlString;

  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);

  NSURL* url = [NSURL URLWithString:urlString];
  if ([url isFileURL] &&
      [[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
    [item setPropertyList:@[ [url path] ]
                  forType:UTIFromPboardType(NSFilenamesPboardType)];
  }

  // Set Safari's URL + title arrays Pboard type.
  NSArray* urlsAndTitles = @[ @[ urlString ], @[ title ] ];
  [item setPropertyList:urlsAndTitles
                forType:UTIFromPboardType(kWebURLsWithTitlesPboardType)];

  // Set NSURLPboardType. The format of the property list is divined from
  // Webkit's function PlatformPasteboard::setStringForType.
  // https://github.com/WebKit/webkit/blob/master/Source/WebCore/platform/mac/PlatformPasteboardMac.mm
  NSURL* base = [url baseURL];
  if (base) {
    [item setPropertyList:@[ [url relativeString], [base absoluteString] ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  } else if (url) {
    [item setPropertyList:@[ [url absoluteString], @"" ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  }

  [item setString:urlString forType:NSPasteboardTypeString];
  [item setString:urlString forType:base::mac::CFToNSCast(kUTTypeURL)];
  [item setString:title forType:kUTTypeURLName];
  return item;
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromUrls(
    NSArray* urls,
    NSArray* titles) {
  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);

  // Set Safari's URL + title arrays Pboard type.
  NSArray* urlsAndTitles = @[ urls, titles ];
  [item setPropertyList:urlsAndTitles
                forType:UTIFromPboardType(kWebURLsWithTitlesPboardType)];

  return item;
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromString(
    NSString* string) {
  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);
  [item setString:string forType:NSPasteboardTypeString];
  return item;
}

//static
NSString* ClipboardUtil::GetTitleFromPasteboardURL(NSPasteboard* pboard) {
  return [pboard stringForType:kUTTypeURLName];
}

//static
NSString* ClipboardUtil::GetURLFromPasteboardURL(NSPasteboard* pboard) {
  return [pboard stringForType:base::mac::CFToNSCast(kUTTypeURL)];
}

// static
NSString* ClipboardUtil::UTIForPasteboardType(NSString* type) {
  return UTIFromPboardType(type);
}

// static
NSString* ClipboardUtil::UTIForWebURLsAndTitles() {
  return UTIFromPboardType(kWebURLsWithTitlesPboardType);
}

// static
void ClipboardUtil::AddDataToPasteboard(NSPasteboard* pboard,
                                        NSPasteboardItem* item) {
  NSSet* oldTypes = [NSSet setWithArray:[pboard types]];
  NSMutableSet* newTypes = [NSMutableSet setWithArray:[item types]];
  [newTypes minusSet:oldTypes];

  [pboard addTypes:[newTypes allObjects] owner:nil];
  for (NSString* type in newTypes) {
    // Technically, the object associated with |type| might be an NSString or a
    // property list. It doesn't matter though, since the type gets pulled from
    // and shoved into an NSDictionary.
    [pboard setData:[item dataForType:type] forType:type];
  }
}

// static
bool ClipboardUtil::URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                                NSArray** urls,
                                                NSArray** titles) {
  return ReadWebURLsWithTitlesPboardType(pboard, urls, titles) ||
         ReadURLItemsWithTitles(pboard, urls, titles);
}

// static
NSPasteboard* ClipboardUtil::PasteboardFromBuffer(ui::ClipboardBuffer buffer) {
  NSString* buffer_type = nil;
  switch (buffer) {
    case ui::ClipboardBuffer::kCopyPaste:
      buffer_type = NSGeneralPboard;
      break;
    case ui::ClipboardBuffer::kDrag:
      buffer_type = NSDragPboard;
      break;
    case ui::ClipboardBuffer::kSelection:
      NOTREACHED();
      break;
  }

  return [NSPasteboard pasteboardWithName:buffer_type];
}

// static
NSString* ClipboardUtil::GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard) {
  NSData* rtfData = [pboard dataForType:NSRTFPboardType];
  if (!rtfData)
    return nil;

  NSAttributedString* attributed =
      [[[NSAttributedString alloc] initWithRTF:rtfData
                            documentAttributes:nil] autorelease];
  NSData* htmlData =
      [attributed dataFromRange:NSMakeRange(0, [attributed length])
             documentAttributes:@{
               NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
             }
                          error:nil];

  // According to the docs, NSHTMLTextDocumentType is UTF8.
  return [[[NSString alloc] initWithData:htmlData
                                encoding:NSUTF8StringEncoding] autorelease];
}

}  // namespace ui
