// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include <AppKit/AppKit.h>
#include <CoreServices/CoreServices.h>                      // pre-macOS 11
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>  // macOS 11

#include <string>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/url_file_parser.h"
#include "url/gurl.h"

namespace ui {

namespace {

// Reads the "WebKitWebURLsWithTitles" type put onto the pasteboard by Safari
// and returns the URLs/titles found within. Returns true if this was
// successful, or false if it was not.
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

// Returns the user-visible name of the file, without any extension. If there
// is any error, returns nil.
NSString* ExtractTitleFromFilename(NSURL* file_url) {
  NSDictionary* resource_values;
  resource_values = [file_url resourceValuesForKeys:@[
    NSURLLocalizedNameKey, NSURLHasHiddenExtensionKey
  ]
                                              error:nil];
  if (!resource_values) {
    return nil;
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

// A simple pair of URL with title. Valid if the `url` field is not null.
struct URLAndTitle {
  NSString* url = nil;
  NSString* title = nil;
};

// Returns a URL and title if standard URL and URL title types are present on
// the pasteboard item. Because the Finder and/or the core macOS drag code
// automatically turn .webloc file drags into standard URL types, .webloc file
// drags are also handled by this function.
URLAndTitle ExtractStandardURLAndTitle(NSPasteboardItem* item) {
  NSString* url = [item stringForType:NSPasteboardTypeURL];
  if (!url) {
    return {};
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
      if (@available(macOS 11, *)) {
        UTType* type;
        if (![file_url getResourceValue:&type
                                 forKey:NSURLContentTypeKey
                                  error:nil]) {
          return {};
        }
        if (![type conformsToType:UTTypeInternetLocation]) {
          return {};
        }
      } else {
        NSString* type;
        if (![file_url getResourceValue:&type
                                 forKey:NSURLTypeIdentifierKey
                                  error:nil]) {
          return {};
        }
        if (![NSWorkspace.sharedWorkspace type:type
                                conformsToType:base::mac::CFToNSCast(
                                                   kUTTypeInternetLocation)]) {
          return {};
        }
      }

      title = ExtractTitleFromFilename(file_url);
      if (!title) {
        // If there was a file URL but the filename could not be extracted, use
        // the last bit of the URL as the title.
        title = file_url.lastPathComponent;
      }
    }
  }

  if (!title) {
    // If still no title, use the hostname as the last resort.
    title = [NSURL URLWithString:url].host;
  }

  return {.url = url, .title = title};
}

// Returns a URL and title if the pasteboard item is of a standard Microsoft
// Windows IShellLink-style .url file.
URLAndTitle ExtractURLFromURLFile(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return {};
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  if (@available(macOS 11, *)) {
    UTType* type;
    if (![file_url getResourceValue:&type
                             forKey:NSURLContentTypeKey
                              error:nil]) {
      return {};
    }
    if (![type conformsToType:UTTypeInternetShortcut]) {
      return {};
    }
  } else {
    NSString* type;
    if (![file_url getResourceValue:&type
                             forKey:NSURLTypeIdentifierKey
                              error:nil]) {
      return {};
    }
    NSString* const kUTTypeInternetShortcut =
        @"com.microsoft.internet-shortcut";
    if (![NSWorkspace.sharedWorkspace type:type
                            conformsToType:kUTTypeInternetShortcut]) {
      return {};
    }
  }

  // Windows codepage 1252 (aka WinLatin1) is the best guess.
  NSString* contents =
      [NSString stringWithContentsOfURL:file_url
                               encoding:NSWindowsCP1252StringEncoding
                                  error:nil];
  if (!contents) {
    return {};
  }

  std::string found_url =
      ClipboardUtil::internal::ExtractURLFromURLFileContents(
          base::SysNSStringToUTF8(contents));
  if (found_url.empty()) {
    return {};
  }

  NSString* title = ExtractTitleFromFilename(file_url);
  if (!title) {
    // Fall back to the last path component of the .url file URL if there's no
    // better option.
    title = file_url.lastPathComponent;
  }

  return {.url = base::SysUTF8ToNSString(found_url), .title = title};
}

// Returns a URL and title if a string on the pasteboard item is formatted as a
// URL but doesn't actually have the URL type.
URLAndTitle ExtractURLFromStringValue(NSPasteboardItem* item) {
  NSString* string = [item stringForType:NSPasteboardTypeString];
  if (!string) {
    return {};
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
  if (url.is_valid()) {
    // The hostname is the best that can be done for the title.
    return {.url = string, .title = base::SysUTF8ToNSString(url.host())};
  }

  return {};
}

// If there is a file URL on the pasteboard, returns that file as the URL and
// returns the file's name as the title.
URLAndTitle ExtractFileURL(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return {};
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  NSString* filename;
  BOOL success = [file_url getResourceValue:&filename
                                     forKey:NSURLLocalizedNameKey
                                      error:nil];

  if (success) {
    return {.url = file_url.absoluteString, .title = filename};
  } else {
    return {.url = file_url.absoluteString,
            .title = file_url.lastPathComponent};
  }
}

// Reads the given pasteboard, and returns URLs/titles found on it. If
// `include_files` is set, then any file references on the pasteboard will be
// returned as file URLs. Returns true if at least one URL was found on the
// pasteboard, and false if none were.
bool ReadURLItemsWithTitles(NSPasteboard* pboard,
                            bool include_files,
                            NSArray<NSString*>** urls,
                            NSArray<NSString*>** titles) {
  NSMutableArray<NSString*>* urls_array = [NSMutableArray array];
  NSMutableArray<NSString*>* titles_array = [NSMutableArray array];

  for (NSPasteboardItem* item in pboard.pasteboardItems) {
    // Try each of several ways of getting URLs from the pasteboard item and
    // stop with the first one that works.

    URLAndTitle url_and_title = ExtractStandardURLAndTitle(item);

    if (!url_and_title.url) {
      url_and_title = ExtractURLFromURLFile(item);
    }

    if (!url_and_title.url) {
      url_and_title = ExtractURLFromStringValue(item);
    }

    if (!url_and_title.url && include_files) {
      url_and_title = ExtractFileURL(item);
    }

    if (url_and_title.url) {
      [urls_array addObject:url_and_title.url];
      [titles_array addObject:url_and_title.title];
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

namespace ClipboardUtil {

NSArray<NSPasteboardItem*>* PasteboardItemsFromUrls(
    NSArray<NSString*>* urls,
    NSArray<NSString*>* titles) {
  DCHECK_EQ(urls.count, titles.count);

  NSMutableArray<NSPasteboardItem*>* items = [NSMutableArray array];

  for (NSUInteger i = 0; i < urls.count; ++i) {
    NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];

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

bool URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                 bool include_files,
                                 NSArray<NSString*>** urls,
                                 NSArray<NSString*>** titles) {
  return ReadWebURLsWithTitlesPboardType(pboard, urls, titles) ||
         ReadURLItemsWithTitles(pboard, include_files, urls, titles);
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
        base::mac::NSURLToFilePath(file_url),
        base::mac::NSStringToFilePath(file_url.lastPathComponent));
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
    NSURL* url = base::mac::FilePathToNSURL(file.path);
    NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];
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
      break;
  }

  return [NSPasteboard pasteboardWithName:buffer_type];
}

NSString* GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard) {
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

}  // namespace ClipboardUtil

}  // namespace ui
