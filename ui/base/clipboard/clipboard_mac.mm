// Copyright 2012 The Chromium Authors
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
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "url/gurl.h"

namespace ui {

namespace {

NSPasteboard* GetPasteboard() {
  // The pasteboard can always be nil, since there is a finite amount of storage
  // that must be shared between all pasteboards.
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  return pasteboard;
}

base::scoped_nsobject<NSImage> GetNSImage(NSPasteboard* pasteboard) {
  // If the pasteboard's image data is not to its liking, the guts of NSImage
  // may throw, and that exception will leak. Prevent a crash in that case;
  // a blank image is better.
  base::scoped_nsobject<NSImage> image;
  @try {
    if (pasteboard)
      image.reset([[NSImage alloc] initWithPasteboard:pasteboard]);
  } @catch (id exception) {
  }
  if (!image)
    return base::scoped_nsobject<NSImage>();
  if ([[image representations] count] == 0u)
    return base::scoped_nsobject<NSImage>();
  return image;
}

// Read raw PNG bytes from the clipboard.
std::vector<uint8_t> GetPngFromPasteboard(NSPasteboard* pasteboard) {
  if (!pasteboard)
    return std::vector<uint8_t>();

  NSData* data = [pasteboard dataForType:NSPasteboardTypePNG];
  if (!data)
    return std::vector<uint8_t>();

  const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> png(bytes, bytes + data.length);
  return png;
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

// DataTransferEndpoint is not used on this platform.
DataTransferEndpoint* ClipboardMac::GetSource(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return nullptr;
}

const ClipboardSequenceNumberToken& ClipboardMac::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSInteger sequence_number = [GetPasteboard() changeCount];
  if (sequence_number != clipboard_sequence_.sequence_number) {
    // Generate a unique token associated with the current sequence number.
    clipboard_sequence_ = {sequence_number, ClipboardSequenceNumberToken()};
  }
  return clipboard_sequence_.token;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardMac::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  // https://crbug.com/1016740#c21
  base::scoped_nsobject<NSArray> types([[GetPasteboard() types] retain]);

  // Safari only places RTF on the pasteboard, never HTML. We can convert RTF
  // to HTML, so the presence of either indicates success when looking for HTML.
  if (format == ClipboardFormatType::HtmlType()) {
    return [types containsObject:NSPasteboardTypeHTML] ||
           [types containsObject:NSPasteboardTypeRTF];
  }
  // Chrome can retrieve an image from the clipboard as either a bitmap or PNG.
  if (format == ClipboardFormatType::PngType() ||
      format == ClipboardFormatType::BitmapType()) {
    return [types containsObject:NSPasteboardTypePNG] ||
           [types containsObject:NSPasteboardTypeTIFF];
  }
  return [types containsObject:format.ToNSString()];
}

bool ClipboardMac::IsMarkedByOriginatorAsConfidential() const {
  DCHECK(CalledOnValidThread());

  NSPasteboardType type =
      [GetPasteboard() availableTypeFromArray:@[ kUTTypeConfidentialData ]];

  if (type)
    return true;

  return false;
}

void ClipboardMac::MarkAsConfidential() {
  DCHECK(CalledOnValidThread());

  [GetPasteboard() setData:nil forType:kUTTypeConfidentialData];
}

void ClipboardMac::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  [GetPasteboard() declareTypes:@[] owner:nil];
}

std::vector<std::u16string> ClipboardMac::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  NSPasteboard* pb = GetPasteboard();
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeSvg));
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(ClipboardFormatType::FilenamesType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeURIList));
  } else if (pb && [NSImage canInitWithPasteboard:pb]) {
    // Finder Cmd+C places both file and icon onto the clipboard
    // (http://crbug.com/553686), so ignore images if we have detected files.
    // This means that if an image is present with file content, we will always
    // ignore the image, but this matches observable Safari behavior.
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  }
  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  NSPasteboard* pb = GetPasteboard();
  types->clear();
  *types = GetStandardFormats(buffer, data_dst);

  if ([[pb types] containsObject:kUTTypeChromiumWebCustomData]) {
    NSData* data = [pb dataForType:kUTTypeChromiumWebCustomData];
    if ([data length])
      ReadCustomDataTypes([data bytes], [data length], types);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadText(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  NSString* contents = [GetPasteboard() stringForType:NSPasteboardTypeString];

  *result = base::SysNSStringToUTF16(contents);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadAsciiText(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  NSString* contents = [GetPasteboard() stringForType:NSPasteboardTypeString];

  if (!contents)
    result->clear();
  else
    result->assign(base::SysNSStringToUTF8(contents));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadHTML(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);

  // TODO(avi): src_url?
  markup->clear();
  if (src_url)
    src_url->clear();

  NSPasteboard* pb = GetPasteboard();
  NSArray* supportedTypes =
      @[ NSPasteboardTypeHTML, NSPasteboardTypeRTF, NSPasteboardTypeString ];
  NSString* bestType = [pb availableTypeFromArray:supportedTypes];
  if (bestType) {
    NSString* contents;
    if ([bestType isEqualToString:NSPasteboardTypeRTF]) {
      contents = clipboard_util::GetHTMLFromRTFOnPasteboard(pb);
    } else {
      contents = [pb stringForType:bestType];
    }
    *markup = base::SysNSStringToUTF16(contents);
  }

  *fragment_start = 0;
  DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardMac::ReadSvg(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kSvg);
  NSString* contents = [GetPasteboard()
      stringForType:ClipboardFormatType::SvgType().ToNSString()];

  *result = base::SysNSStringToUTF16(contents);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadRTF(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kRtf);

  return ReadData(ClipboardFormatType::RtfType(), data_dst, result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadPng(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           ReadPngCallback callback) const {
  RecordRead(ClipboardFormatMetric::kPng);
  std::move(callback).Run(ReadPngInternal(buffer, GetPasteboard()));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadCustomData(ClipboardBuffer buffer,
                                  const std::u16string& type,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kCustomData);

  NSPasteboard* pb = GetPasteboard();
  if ([[pb types] containsObject:kUTTypeChromiumWebCustomData]) {
    NSData* data = [pb dataForType:kUTTypeChromiumWebCustomData];
    if ([data length])
      ReadCustomDataForType([data bytes], [data length], type, result);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadFilenames(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kFilenames);

  std::vector<ui::FileInfo> files =
      clipboard_util::FilesFromPasteboard(GetPasteboard());
  base::ranges::move(files, std::back_inserter(*result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadBookmark(const DataTransferEndpoint* data_dst,
                                std::u16string* title,
                                std::string* url) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kBookmark);
  NSPasteboard* pb = GetPasteboard();

  if (title) {
    NSString* contents = [pb stringForType:kUTTypeURLName];
    *title = base::SysNSStringToUTF16(contents);
  }

  if (url) {
    NSString* url_string = [pb stringForType:NSPasteboardTypeURL];
    if (!url_string)
      url->clear();
    else
      url->assign(base::SysNSStringToUTF8(url_string));
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadData(const ClipboardFormatType& format,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);
  NSData* data = [GetPasteboard() dataForType:format.ToNSString()];
  if ([data length])
    result->assign(static_cast<const char*>([data bytes]), [data length]);
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  [GetPasteboard() declareTypes:@[] owner:nil];

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
}

void ClipboardMac::WriteText(const char* text_data, size_t text_len) {
  std::string text_str(text_data, text_len);
  NSString* text = base::SysUTF8ToNSString(text_str);
  [GetPasteboard() setString:text forType:NSPasteboardTypeString];
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
  [GetPasteboard() setString:html_fragment forType:NSPasteboardTypeHTML];
}

void ClipboardMac::WriteUnsanitizedHTML(const char* markup_data,
                                        size_t markup_len,
                                        const char* url_data,
                                        size_t url_len) {
  WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardMac::WriteSvg(const char* markup_data, size_t markup_len) {
  std::string svg_str(markup_data, markup_len);
  NSString* svg = base::SysUTF8ToNSString(svg_str);
  [GetPasteboard() setString:svg
                     forType:ClipboardFormatType::SvgType().ToNSString()];
}

void ClipboardMac::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::RtfType(), rtf_data, data_len);
}

void ClipboardMac::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void ClipboardMac::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  std::string title_str(title_data, title_len);
  NSString* title = base::SysUTF8ToNSString(title_str);
  std::string url_str(url_data, url_len);
  NSString* url = base::SysUTF8ToNSString(url_str);

  NSArray<NSPasteboardItem*>* items =
      clipboard_util::PasteboardItemsFromUrls(@[ url ], @[ title ]);
  clipboard_util::AddDataToPasteboard(GetPasteboard(), items.firstObject);
}

void ClipboardMac::WriteBitmap(const SkBitmap& bitmap) {
  // The bitmap type is sanitized to be N32 before we get here. The conversion
  // to an NSImage would not explode if we got this wrong, so this is not a
  // security CHECK.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  NSImage* image = skia::SkBitmapToNSImageWithColorSpace(
      bitmap, base::mac::GetSystemColorSpace());
  if (!image) {
    NOTREACHED() << "SkBitmapToNSImageWithColorSpace failed";
    return;
  }
  [GetPasteboard() writeObjects:@[ image ]];
}

void ClipboardMac::WriteData(const ClipboardFormatType& format,
                             const char* data_data,
                             size_t data_len) {
  [GetPasteboard() setData:[NSData dataWithBytes:data_data length:data_len]
                   forType:format.ToNSString()];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardMac::WriteWebSmartPaste() {
  NSString* format = ClipboardFormatType::WebKitSmartPasteType().ToNSString();
  [GetPasteboard() setData:nil forType:format];
}

std::vector<uint8_t> ClipboardMac::ReadPngInternal(
    ClipboardBuffer buffer,
    NSPasteboard* pasteboard) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  std::vector<uint8_t> png = GetPngFromPasteboard(pasteboard);
  if (!png.empty())
    return png;

  // If we can’t read a PNG, try reading for an NSImage, and if successful,
  // transcode it to PNG.
  base::scoped_nsobject<NSImage> image = GetNSImage(pasteboard);
  if (!image)
    return std::vector<uint8_t>();

  auto gfx_image = gfx::Image(image);
  if (gfx_image.IsEmpty())
    return std::vector<uint8_t>();

  scoped_refptr<base::RefCountedMemory> mem = gfx_image.As1xPNGBytes();
  std::vector<uint8_t> image_data(mem->data(), mem->data() + mem->size());
  return image_data;
}

}  // namespace ui
