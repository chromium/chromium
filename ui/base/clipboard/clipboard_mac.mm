// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <limits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace ui {

namespace {

NSPasteboard* GetPasteboard() {
  // The pasteboard can always be nil, since there is a finite amount of storage
  // that must be shared between all pasteboards.
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  return pasteboard;
}

}  // namespace

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardMac;
}

// ClipboardMac implementation.
ClipboardMac::ClipboardMac() {
  DCHECK(CalledOnValidThread());
}

ClipboardMac::~ClipboardMac() {
  DCHECK(CalledOnValidThread());
}

void ClipboardMac::OnPreShutdown() {}

uint64_t ClipboardMac::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  return [pb changeCount];
}

bool ClipboardMac::IsFormatAvailable(const ClipboardFormatType& format,
                                     ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  NSArray* types = [pb types];

  // Safari only places RTF on the pasteboard, never HTML. We can convert RTF
  // to HTML, so the presence of either indicates success when looking for HTML.
  if ([format.ToNSString() isEqualToString:NSHTMLPboardType]) {
    return [types containsObject:NSHTMLPboardType] ||
           [types containsObject:NSRTFPboardType];
  }
  return [types containsObject:format.ToNSString()];
}

void ClipboardMac::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:@[] owner:nil];
}

void ClipboardMac::ReadAvailableTypes(ClipboardBuffer buffer,
                                      std::vector<base::string16>* types,
                                      bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  types->clear();
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));

  NSPasteboard* pb = GetPasteboard();
  if (pb && [NSImage canInitWithPasteboard:pb])
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;

  if ([[pb types] containsObject:kWebCustomDataPboardType]) {
    NSData* data = [pb dataForType:kWebCustomDataPboardType];
    if ([data length])
      ReadCustomDataTypes([data bytes], [data length], types);
  }
}

void ClipboardMac::ReadText(ClipboardBuffer buffer,
                            base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSPasteboardTypeString];

  *result = base::SysNSStringToUTF16(contents);
}

void ClipboardMac::ReadAsciiText(ClipboardBuffer buffer,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSPasteboardTypeString];

  if (!contents)
    result->clear();
  else
    result->assign([contents UTF8String]);
}

void ClipboardMac::ReadHTML(ClipboardBuffer buffer,
                            base::string16* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  // TODO(avi): src_url?
  markup->clear();
  if (src_url)
    src_url->clear();

  NSPasteboard* pb = GetPasteboard();
  NSArray* supportedTypes =
      @[ NSHTMLPboardType, NSRTFPboardType, NSPasteboardTypeString ];
  NSString* bestType = [pb availableTypeFromArray:supportedTypes];
  if (bestType) {
    NSString* contents;
    if ([bestType isEqualToString:NSRTFPboardType])
      contents = ClipboardUtil::GetHTMLFromRTFOnPasteboard(pb);
    else
      contents = [pb stringForType:bestType];
    *markup = base::SysNSStringToUTF16(contents);
  }

  *fragment_start = 0;
  DCHECK(markup->length() <= std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardMac::ReadRTF(ClipboardBuffer buffer, std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  return ReadData(ClipboardFormatType::GetRtfType(), result);
}

SkBitmap ClipboardMac::ReadImage(ClipboardBuffer buffer,
                                 NSPasteboard* pb) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  // If the pasteboard's image data is not to its liking, the guts of NSImage
  // may throw, and that exception will leak. Prevent a crash in that case;
  // a blank image is better.
  base::scoped_nsobject<NSImage> image;
  @try {
    if ([[pb types] containsObject:NSFilenamesPboardType]) {
      // -[NSImage initWithPasteboard:] gets confused with copies of a single
      // file from the Finder, so extract the path ourselves.
      // http://crbug.com/553686
      NSArray* paths = [pb propertyListForType:NSFilenamesPboardType];
      if ([paths count]) {
        // If N number of files are selected from finder, choose the last one.
        image.reset([[NSImage alloc]
            initWithContentsOfURL:[NSURL fileURLWithPath:[paths lastObject]]]);
      }
    } else {
      if (pb)
        image.reset([[NSImage alloc] initWithPasteboard:pb]);
    }
  } @catch (id exception) {
  }

  if (!image)
    return SkBitmap();
  if ([[image representations] count] == 0u)
    return SkBitmap();

  // This logic prevents loss of pixels from retina images, where size != pixel
  // size. In an ideal world, the concept of "retina-ness" would be plumbed all
  // the way through to the web, but the clipboard API doesn't support the
  // additional metainformation.
  if ([[image representations] count] == 1u) {
    NSImageRep* rep = [image representations][0];
    NSInteger width = [rep pixelsWide];
    NSInteger height = [rep pixelsHigh];
    if (width != 0 && height != 0) {
      return skia::NSImageRepToSkBitmapWithColorSpace(
          rep, NSMakeSize(width, height), /*is_opaque=*/false,
          base::mac::GetSystemColorSpace());
    }
  }
  return skia::NSImageToSkBitmapWithColorSpace(
      image.get(), /*is_opaque=*/false, base::mac::GetSystemColorSpace());
}

SkBitmap ClipboardMac::ReadImage(ClipboardBuffer buffer) const {
  return ReadImage(buffer, GetPasteboard());
}

void ClipboardMac::ReadCustomData(ClipboardBuffer buffer,
                                  const base::string16& type,
                                  base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  if ([[pb types] containsObject:kWebCustomDataPboardType]) {
    NSData* data = [pb dataForType:kWebCustomDataPboardType];
    if ([data length])
      ReadCustomDataForType([data bytes], [data length], type, result);
  }
}

void ClipboardMac::ReadBookmark(base::string16* title, std::string* url) const {
  DCHECK(CalledOnValidThread());
  NSPasteboard* pb = GetPasteboard();

  if (title) {
    NSString* contents = ClipboardUtil::GetTitleFromPasteboardURL(pb);
    *title = base::SysNSStringToUTF16(contents);
  }

  if (url) {
    NSString* url_string = ClipboardUtil::GetURLFromPasteboardURL(pb);
    if (!url_string)
      url->clear();
    else
      url->assign([url_string UTF8String]);
  }
}

void ClipboardMac::ReadData(const ClipboardFormatType& format,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  NSPasteboard* pb = GetPasteboard();
  NSData* data = [pb dataForType:format.ToNSString()];
  if ([data length])
    result->assign(static_cast<const char*>([data bytes]), [data length]);
}

void ClipboardMac::WritePortableRepresentations(ClipboardBuffer buffer,
                                                const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:@[] owner:nil];

  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
}

void ClipboardMac::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:@[] owner:nil];

  DispatchPlatformRepresentations(std::move(platform_representations));
}

void ClipboardMac::WriteText(const char* text_data, size_t text_len) {
  std::string text_str(text_data, text_len);
  NSString* text = base::SysUTF8ToNSString(text_str);
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:@[ NSPasteboardTypeString ] owner:nil];
  [pb setString:text forType:NSPasteboardTypeString];
}

void ClipboardMac::WriteHTML(const char* markup_data,
                             size_t markup_len,
                             const char* url_data,
                             size_t url_len) {
  // We need to mark it as utf-8. (see crbug.com/11957)
  std::string html_fragment_str("<meta charset='utf-8'>");
  html_fragment_str.append(markup_data, markup_len);
  NSString* html_fragment = base::SysUTF8ToNSString(html_fragment_str);

  // TODO(avi): url_data?
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:@[ NSHTMLPboardType ] owner:nil];
  [pb setString:html_fragment forType:NSHTMLPboardType];
}

void ClipboardMac::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::GetRtfType(), rtf_data, data_len);
}

void ClipboardMac::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  std::string title_str(title_data, title_len);
  NSString* title = base::SysUTF8ToNSString(title_str);
  std::string url_str(url_data, url_len);
  NSString* url = base::SysUTF8ToNSString(url_str);

  base::scoped_nsobject<NSPasteboardItem> item(
      ClipboardUtil::PasteboardItemFromUrl(url, title));
  NSPasteboard* pb = GetPasteboard();
  ui::ClipboardUtil::AddDataToPasteboard(pb, item);
}

void ClipboardMac::WriteBitmap(const SkBitmap& bitmap) {
  SkBitmap out_bitmap;
  if (!skia::SkBitmapToN32OpaqueOrPremul(bitmap, &out_bitmap)) {
    NOTREACHED() << "Unable to convert bitmap for clipboard";
    return;
  }

  NSImage* image = skia::SkBitmapToNSImageWithColorSpace(
      out_bitmap, base::mac::GetSystemColorSpace());
  if (!image) {
    NOTREACHED() << "SkBitmapToNSImageWithColorSpace failed";
    return;
  }
  // TODO (https://crbug.com/971916): Write NSImage directly to clipboard.
  // An API to ask the NSImage to write itself to the clipboard comes in 10.6 :(
  // For now, spit out the image as a TIFF.
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:@[ NSTIFFPboardType ] owner:nil];
  NSData* tiff_data = [image TIFFRepresentation];
  LOG_IF(ERROR, tiff_data == nullptr)
      << "Failed to allocate image for clipboard";
  if (tiff_data) {
    [pb setData:tiff_data forType:NSTIFFPboardType];
  }
}

void ClipboardMac::WriteData(const ClipboardFormatType& format,
                             const char* data_data,
                             size_t data_len) {
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:@[ format.ToNSString() ] owner:nil];
  [pb setData:[NSData dataWithBytes:data_data length:data_len]
      forType:format.ToNSString()];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardMac::WriteWebSmartPaste() {
  NSPasteboard* pb = GetPasteboard();
  NSString* format =
      ClipboardFormatType::GetWebKitSmartPasteType().ToNSString();
  [pb addTypes:@[ format ] owner:nil];
  [pb setData:nil forType:format];
}

}  // namespace ui
