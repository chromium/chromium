// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ios.h"

#import <UIKit/UIKit.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/containers/span.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

UIPasteboard* GetPasteboard() {
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  return pasteboard;
}

NSData* GetDataWithTypeFromPasteboard(UIPasteboard* pasteboard,
                                      NSString* type) {
  DCHECK(pasteboard);
  auto items = [pasteboard dataForPasteboardType:type inItemSet:nil];
  if (!items) {
    return nullptr;
  }
  return [items firstObject];
}

}  // namespace

Clipboard* Clipboard::Create() {
  return new ClipboardIOS;
}

// ClipboardIOS implementation.
ClipboardIOS::ClipboardIOS() {
  DCHECK(CalledOnValidThread());
}

ClipboardIOS::~ClipboardIOS() {
  DCHECK(CalledOnValidThread());
}

void ClipboardIOS::OnPreShutdown() {}

// DataTransferEndpoint is not used on this platform.
std::optional<DataTransferEndpoint> ClipboardIOS::GetSource(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return std::nullopt;
}

const ClipboardSequenceNumberToken& ClipboardIOS::GetSequenceNumber(
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
std::vector<std::u16string> ClipboardIOS::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeText));
  }
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  }
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeSvg));
  }
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  }
  if (IsFormatAvailable(ClipboardFormatType::FilenamesType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeURIList));
  }
  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardIOS::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  return [GetPasteboard() containsPasteboardTypes:@[ format.ToNSString() ]
                                        inItemSet:nil];
}

void ClipboardIOS::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  [GetPasteboard() setItems:@[]];
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  types->clear();
  *types = GetStandardFormats(buffer, data_dst);

  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), (NSString*)kUTTypeChromiumDataTransferCustomData);
  if (data) {
    ReadCustomDataTypes(base::apple::NSDataToSpan(data), types);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadText(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);

  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::PlainTextType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result->assign(base::SysNSStringToUTF16(contents));
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadAsciiText(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);

  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::PlainTextType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result->assign(base::SysNSStringToUTF8(contents));
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadHTML(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);

  NSString* best_type = nil;
  for (NSString* type in @[
         ClipboardFormatType::HtmlType().ToNSString(),
         ClipboardFormatType::RtfType().ToNSString(),
         ClipboardFormatType::PlainTextType().ToNSString()
       ]) {
    if ([GetPasteboard() containsPasteboardTypes:@[ type ]]) {
      best_type = type;
      break;
    }
  }

  NSData* data = GetDataWithTypeFromPasteboard(GetPasteboard(), best_type);
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    markup->assign(base::SysNSStringToUTF16(contents));
  }

  *fragment_start = 0;
  DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());

  // TODO: src_url
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadSvg(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kSvg);

  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::SvgType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result->assign(base::SysNSStringToUTF16(contents));
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadRTF(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kRtf);

  return ReadData(ClipboardFormatType::RtfType(), data_dst, result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadPng(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kPng);

  std::move(callback).Run(ReadPngInternal(buffer, GetPasteboard()));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadDataTransferCustomData(
    ClipboardBuffer buffer,
    const std::u16string& type,
    const DataTransferEndpoint* data_dst,
    std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kCustomData);

  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), (NSString*)kUTTypeChromiumDataTransferCustomData);
  if (data) {
    if (std::optional<std::u16string> maybe_result =
            ReadCustomDataForType(base::apple::NSDataToSpan(data), type);
        maybe_result) {
      *result = std::move(*maybe_result);
    }
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadFilenames(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kFilenames);

  auto items = [GetPasteboard()
      dataForPasteboardType:ClipboardFormatType::FilenamesType().ToNSString()
                  inItemSet:nil];
  if (!items) {
    return;
  }

  std::vector<ui::FileInfo> files;
  for (NSData* item : items) {
    if (item) {
      NSString* file_str = [[NSString alloc] initWithData:item
                                                 encoding:NSUTF8StringEncoding];
      NSURL* file_url = [NSURL URLWithString:file_str];
      files.emplace_back(
          base::apple::NSURLToFilePath(file_url),
          base::apple::NSStringToFilePath(file_url.lastPathComponent));
    }
  }
  base::ranges::move(files, std::back_inserter(*result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadBookmark(const DataTransferEndpoint* data_dst,
                                std::u16string* title,
                                std::string* url) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kBookmark);

  NSData* url_data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::UrlType().ToNSString());
  if (url_data) {
    NSString* contents = [[NSString alloc] initWithData:url_data
                                               encoding:NSUTF8StringEncoding];
    url->assign(base::SysNSStringToUTF8(contents));
  }

  NSData* title_data =
      GetDataWithTypeFromPasteboard(GetPasteboard(), kUTTypeURLName);
  if (title_data) {
    NSString* contents = [[NSString alloc] initWithData:title_data
                                               encoding:NSUTF8StringEncoding];
    title->assign(base::SysNSStringToUTF16(contents));
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadData(const ClipboardFormatType& format,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);

  NSData* data =
      GetDataWithTypeFromPasteboard(GetPasteboard(), format.ToNSString());
  if (data) {
    result->assign(base::as_string_view(base::apple::NSDataToSpan(data)));
  }
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src,
    uint32_t privacy_types) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  [GetPasteboard() setItems:@[]];

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects) {
    DispatchPortableRepresentation(object.second);
  }
}

void ClipboardIOS::WriteText(std::string_view text) {
  NSDictionary<NSString*, id>* text_item = @{
    ClipboardFormatType::PlainTextType().ToNSString() :
        base::SysUTF8ToNSString(text)
  };
  [GetPasteboard() addItems:@[ text_item ]];
}

void ClipboardIOS::WriteHTML(std::string_view markup,
                             std::optional<std::string_view> /* source_url */) {
  // We need to mark it as utf-8. (see crbug.com/40759159)
  std::string html_fragment_str("<meta charset='utf-8'>");
  html_fragment_str += markup;
  NSString* html = base::SysUTF8ToNSString(html_fragment_str);

  NSDictionary<NSString*, id>* html_item =
      @{ClipboardFormatType::HtmlType().ToNSString() : html};
  [GetPasteboard() addItems:@[ html_item ]];
}

void ClipboardIOS::WriteSvg(std::string_view markup) {
  NSDictionary<NSString*, id>* svg_item = @{
    ClipboardFormatType::SvgType().ToNSString() :
        base::SysUTF8ToNSString(markup)
  };
  [GetPasteboard() addItems:@[ svg_item ]];
}

void ClipboardIOS::WriteRTF(std::string_view rtf) {
  WriteData(ClipboardFormatType::RtfType(),
            base::as_bytes(base::make_span(rtf)));
}

void ClipboardIOS::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  if (filenames.empty()) {
    return;
  }

  NSMutableArray<NSDictionary<NSString*, id>*>* items =
      [NSMutableArray arrayWithCapacity:filenames.size()];
  for (const auto& file : filenames) {
    NSURL* url = base::apple::FilePathToNSURL(file.path);
    NSString* fileURLType = ClipboardFormatType::FilenamesType().ToNSString();
    NSDictionary<NSString*, id>* item = @{fileURLType : url.absoluteString};
    [items addObject:item];
  }

  [GetPasteboard() addItems:items];
}

void ClipboardIOS::WriteBookmark(std::string_view title, std::string_view url) {
  NSDictionary<NSString*, id>* bookmarkItem = @{
    ClipboardFormatType::UrlType().ToNSString() : base::SysUTF8ToNSString(url),
    kUTTypeURLName : base::SysUTF8ToNSString(title),
  };

  [GetPasteboard() addItems:@[ bookmarkItem ]];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardIOS::WriteWebSmartPaste() {
  NSDictionary<NSString*, id>* item = @{
    ClipboardFormatType::WebKitSmartPasteType().ToNSString() : [NSData data]
  };
  [GetPasteboard() addItems:@[ item ]];
}

void ClipboardIOS::WriteBitmap(const SkBitmap& bitmap) {
  // The bitmap type is sanitized to be N32 before we get here. The conversion
  // to a UIImage would not explode if we got this wrong, so this is not a
  // security CHECK.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* image =
      skia::SkBitmapToUIImageWithColorSpace(bitmap, 1.0f, color_space.get());
  CHECK(image) << "SkBitmapToUIImageWithColorSpace failed";

  [GetPasteboard() setImage:image];
}

void ClipboardIOS::WriteData(const ClipboardFormatType& format,
                             base::span<const uint8_t> data) {
  NSDictionary<NSString*, id>* data_item = @{
    format.ToNSString() : [NSData dataWithBytes:data.data() length:data.size()]
  };
  [GetPasteboard() addItems:@[ data_item ]];
}

void ClipboardIOS::WriteClipboardHistory() {
  // TODO(crbug.com/40945200): Add support for this.
}

void ClipboardIOS::WriteUploadCloudClipboard() {
  // TODO(crbug.com/40945200): Add support for this.
}

void ClipboardIOS::WriteConfidentialDataForPassword() {
  // TODO(crbug.com/40945200): Add support for this.
}

std::vector<uint8_t> ClipboardIOS::ReadPngInternal(
    ClipboardBuffer buffer,
    UIPasteboard* pasteboard) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  DCHECK(pasteboard);

  UIImage* image = pasteboard.image;
  if (!image) {
    return std::vector<uint8_t>();
  }

  NSData* png_data = UIImagePNGRepresentation(image);
  if (!png_data) {
    return std::vector<uint8_t>();
  }

  auto png_span = base::apple::NSDataToSpan(png_data);
  return std::vector<uint8_t>(png_span.begin(), png_span.end());
}

}  // namespace ui
