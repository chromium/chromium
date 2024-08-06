// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include <AppKit/AppKit.h>
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/url_file_parser.h"
#include "url/gurl.h"

@interface URLAndTitle ()

@property(copy) NSString* URL;
@property(copy) NSString* title;

+ (instancetype)URLAndTitleWithURL:(NSString*)url title:(NSString*)title;

@end

@implementation URLAndTitle

@synthesize URL = _url;
@synthesize title = _title;

+ (instancetype)URLAndTitleWithURL:(NSString*)url title:(NSString*)title {
  URLAndTitle* result = [[URLAndTitle alloc] init];
  result.URL = url;
  result.title = title;
  return result;
}

@end

namespace ui {

namespace {

// Reads the "WebKitWebURLsWithTitles" type put onto the pasteboard by Safari
// and returns the URLs/titles found within.
NSArray<URLAndTitle*>* ReadWebURLsWithTitlesPboardType(NSPasteboard* pboard) {
  NSArray* bookmark_pairs = base::apple::ObjCCast<NSArray>(
      [pboard propertyListForType:kUTTypeWebKitWebURLsWithTitles]);
  if (!bookmark_pairs) {
    return [NSArray array];
  }
  if (bookmark_pairs.count != 2) {
    return [NSArray array];
  }

  NSArray<NSString*>* urls_array =
      base::apple::ObjCCast<NSArray>(bookmark_pairs[0]);
  NSArray<NSString*>* titles_array =
      base::apple::ObjCCast<NSArray>(bookmark_pairs[1]);

  if (!urls_array || !titles_array) {
    return [NSArray array];
  }
  if (urls_array.count < 1) {
    return [NSArray array];
  }
  if (urls_array.count != titles_array.count) {
    return [NSArray array];
  }
  for (id obj in urls_array) {
    if (![obj isKindOfClass:[NSString class]]) {
      return [NSArray array];
    }
  }

  for (id obj in titles_array) {
    if (![obj isKindOfClass:[NSString class]]) {
      return [NSArray array];
    }
  }

  NSMutableArray<URLAndTitle*>* result = [NSMutableArray array];
  for (NSUInteger i = 0; i < urls_array.count; ++i) {
    [result addObject:[URLAndTitle URLAndTitleWithURL:urls_array[i]
                                                title:titles_array[i]]];
  }

  return result;
}

// Returns the user-visible name of the file, optionally without any extension.
// If given a non-empty `file_url`, will always return a title.
NSString* DeriveTitleFromFilename(NSURL* file_url, bool strip_extension) {
  NSString* localized_name = nil;
  BOOL success = [file_url getResourceValue:&localized_name
                                     forKey:NSURLLocalizedNameKey
                                      error:nil];
  if (!success || !localized_name) {
    // For the case where the actual display name of a file cannot be obtained,
    // derive a quick-and-dirty version by swapping in "/" for ":", as that's
    // the most common difference between the last path component of a file and
    // how that file is presented to the user. See -[NSFileManager
    // displayNameAtPath:] for an example of macOS doing this. Also, given that
    // this is a failure case, don't bother trying to figure out the extension
    // situation.
    NSString* last_path_component = file_url.lastPathComponent;
    return [last_path_component stringByReplacingOccurrencesOfString:@":"
                                                          withString:@"/"];
  }

  if (!strip_extension) {
    return localized_name;
  }

  NSNumber* has_hidden_extension = nil;
  success = [file_url getResourceValue:&has_hidden_extension
                                forKey:NSURLHasHiddenExtensionKey
                                 error:nil];
  if (!success || !has_hidden_extension || has_hidden_extension.boolValue) {
    // If it's unknown if the extension is hidden, or if the extension is
    // already hidden, return the filename unaltered.
    return localized_name;
  }

  return [localized_name stringByDeletingPathExtension];
}

// Returns a URL and title if standard URL and URL title types are present on
// the pasteboard item. Because the Finder and/or the core macOS drag code
// automatically turn .webloc file drags into standard URL types, .webloc file
// drags are also handled by this function.
URLAndTitle* ExtractStandardURLAndTitle(NSPasteboardItem* item) {
  NSString* url = [item stringForType:NSPasteboardTypeURL];
  if (!url) {
    return nil;
  }

  NSString* title = [item stringForType:kUTTypeURLName];

  if (!title) {
    // If there is no title on the drag, check to see if it's a URL drag
    // reconstituted from a Finder .webloc. If so, use the name of the file as
    // the title.
    NSString* file = [item stringForType:NSPasteboardTypeFileURL];
    if (file) {
      NSURL* file_url = [NSURL URLWithString:file].filePathURL;

      // The UTType for .webloc files is com.apple.web-internet-location, but
      // there is no official constant for that. However, that type does conform
      // to the generic "internet location" type (aka .inetloc), so check for
      // that.
      UTType* type;
      if (![file_url getResourceValue:&type
                               forKey:NSURLContentTypeKey
                                error:nil]) {
        return nil;
      }
      if (![type conformsToType:UTTypeInternetLocation]) {
        return nil;
      }

      title = DeriveTitleFromFilename(file_url, /*strip_extension=*/true);
    }
  }

  if (!title) {
    // If still no title, use the hostname as the last resort.
    title = [NSURL URLWithString:url].host;
  }

  return [URLAndTitle URLAndTitleWithURL:url title:title];
}

// Returns a URL and title if the pasteboard item is of a standard Microsoft
// Windows IShellLink-style .url file.
URLAndTitle* ExtractURLFromURLFile(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return nil;
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  NSDictionary* resource_values;
  resource_values =
      [file_url resourceValuesForKeys:@[ NSURLFileSizeKey, NSURLContentTypeKey ]
                                error:nil];
  if (!resource_values) {
    return nil;
  }

  NSNumber* file_size = resource_values[NSURLFileSizeKey];
  if (file_size.unsignedLongValue >
      clipboard_util::internal::kMaximumParsableFileSize) {
    return nil;
  }

  UTType* type = resource_values[NSURLContentTypeKey];
  if (![type conformsToType:UTTypeInternetShortcut]) {
    return nil;
  }

  // Windows codepage 1252 (aka WinLatin1) is the best guess.
  NSString* contents =
      [NSString stringWithContentsOfURL:file_url
                               encoding:NSWindowsCP1252StringEncoding
                                  error:nil];
  if (!contents) {
    return nil;
  }

  std::string found_url =
      clipboard_util::internal::ExtractURLFromURLFileContents(
          base::SysNSStringToUTF8(contents));
  if (found_url.empty()) {
    return nil;
  }

  NSString* title = DeriveTitleFromFilename(file_url, /*strip_extension=*/true);

  return [URLAndTitle URLAndTitleWithURL:base::SysUTF8ToNSString(found_url)
                                   title:title];
}

// Returns a URL and title if a string on the pasteboard item is formatted as a
// URL but doesn't actually have the URL type.
URLAndTitle* ExtractURLFromStringValue(NSPasteboardItem* item) {
  NSString* string = [item stringForType:NSPasteboardTypeString];
  if (!string) {
    return nil;
  }

  string = [string
      stringByTrimmingCharactersInSet:NSCharacterSet
                                          .whitespaceAndNewlineCharacterSet];

  // Check to see if this string is a valid URL; use GURL to do so. NSURL was
  // found in 2010 to not be strict enough; see https://crbug.com/43100. It's
  // unknown if things have changed since then, but there's no reason to revert.
  // FYI earlier code also allowed all "javascript:" and "data:" URLs as
  // "loosely validated". TODO(avi): If that "loosely validated" escape hatch
  // needed? If significant time goes by and no one complains, remove this TODO
  // and don't put that back in.
  GURL url(base::SysNSStringToUTF8(string));
  if (!url.is_valid()) {
    return nil;
  }

  // The hostname is the best that can be done for the title.
  return [URLAndTitle URLAndTitleWithURL:string
                                   title:base::SysUTF8ToNSString(url.host())];
}

// If there is a file URL on the pasteboard, returns that file as the URL. For
// compatibility with other platforms, return no title.
URLAndTitle* ExtractFileURL(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return nil;
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  return [URLAndTitle URLAndTitleWithURL:file_url.absoluteString title:@""];
}

// Reads the given pasteboard, and returns URLs/titles found on it. If
// `include_files` is set, then any file references on the pasteboard will be
// returned as file URLs. Returns true if at least one URL was found on the
// pasteboard, and false if none were.
NSArray<URLAndTitle*>* ReadURLItemsWithTitles(NSPasteboard* pboard,
                                              bool include_files) {
  NSMutableArray<URLAndTitle*>* result = [NSMutableArray array];

  for (NSPasteboardItem* item in pboard.pasteboardItems) {
    // Try each of several ways of getting URLs from the pasteboard item and
    // stop with the first one that works.

    URLAndTitle* url_and_title = ExtractStandardURLAndTitle(item);

    if (!url_and_title) {
      url_and_title = ExtractURLFromURLFile(item);
    }

    if (!url_and_title) {
      url_and_title = ExtractURLFromStringValue(item);
    }

    if (!url_and_title && include_files) {
      url_and_title = ExtractFileURL(item);
    }

    if (url_and_title) {
      [result addObject:url_and_title];
    }
  }

  return result;
}

}  // namespace

UniquePasteboard::UniquePasteboard()
    : pasteboard_([NSPasteboard pasteboardWithUniqueName]) {}

UniquePasteboard::~UniquePasteboard() {
  [pasteboard_ releaseGlobally];
}

namespace clipboard_util {

NSArray<NSPasteboardItem*>* PasteboardItemsFromUrls(
    NSArray<NSString*>* urls,
    NSArray<NSString*>* titles) {
  DCHECK_EQ(urls.count, titles.count);

  NSMutableArray<NSPasteboardItem*>* items = [NSMutableArray array];

  for (NSUInteger i = 0; i < urls.count; ++i) {
    NSPasteboardItem* item = [[NSPasteboardItem alloc] init];

    NSString* url_string = urls[i];
    NSString* title = titles[i];

    NSURL* url = [NSURL URLWithString:url_string];
    if (url.isFileURL && [url checkResourceIsReachableAndReturnError:nil]) {
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

void AddDataToPasteboard(NSPasteboard* pboard, NSPasteboardItem* item) {
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

NSArray<URLAndTitle*>* URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                                   bool include_files) {
  NSArray<URLAndTitle*>* result = ReadWebURLsWithTitlesPboardType(pboard);
  if (result.count) {
    return result;
  }

  return ReadURLItemsWithTitles(pboard, include_files);
}

std::vector<FileInfo> FilesFromPasteboard(NSPasteboard* pboard) {
  std::vector<FileInfo> results;
  for (NSPasteboardItem* item in pboard.pasteboardItems) {
    NSString* file_url_string = [item stringForType:NSPasteboardTypeFileURL];
    if (!file_url_string) {
      continue;
    }
    NSURL* file_url = [NSURL URLWithString:file_url_string].filePathURL;

    // Despite the second value being the "display name", it must be the full
    // filename because deep in Blink it's used to determine the file's type.
    // See https://crbug.com/1412205.
    results.emplace_back(
        base::apple::NSURLToFilePath(file_url),
        base::apple::NSStringToFilePath(file_url.lastPathComponent));
  }

  return results;
}

void WriteFilesToPasteboard(NSPasteboard* pboard,
                            const std::vector<FileInfo>& files) {
  if (files.empty()) {
    return;
  }

  NSMutableArray<NSPasteboardItem*>* items =
      [NSMutableArray arrayWithCapacity:files.size()];
  for (const auto& file : files) {
    NSURL* url = base::apple::FilePathToNSURL(file.path);
    NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
    [item setString:url.absoluteString forType:NSPasteboardTypeFileURL];
    [items addObject:item];
  }

  [pboard writeObjects:items];
}

NSPasteboard* PasteboardFromBuffer(ClipboardBuffer buffer) {
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
  }

  return [NSPasteboard pasteboardWithName:buffer_type];
}

NSString* GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard) {
  NSData* rtf_data = [pboard dataForType:NSPasteboardTypeRTF];
  if (!rtf_data)
    return nil;

  NSAttributedString* attributed =
      [[NSAttributedString alloc] initWithRTF:rtf_data documentAttributes:nil];
  NSData* html_data =
      [attributed dataFromRange:NSMakeRange(0, attributed.length)
             documentAttributes:@{
               NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
             }
                          error:nil];

  // According to the docs, NSHTMLTextDocumentType is UTF-8.
  return [[NSString alloc] initWithData:html_data
                               encoding:NSUTF8StringEncoding];
}

}  // namespace clipboard_util

}  // namespace ui
