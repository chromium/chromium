// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/clipboard/clipboard_ios.h"

#import <UIKit/UIKit.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
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
void ClipboardIOS::GetSource(ClipboardBuffer buffer,
                             GetSourceCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::move(callback).Run(std::nullopt);
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

void ClipboardIOS::GetAllAvailableFormats(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    base::OnceCallback<void(base::flat_set<ClipboardFormatType>)> callback)
    const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSArray* types = GetPasteboard().pasteboardTypes;
  base::flat_set<ClipboardFormatType> formats;
  for (NSString* type in types) {
    std::string type_utf8 = base::SysNSStringToUTF8(type);
    if (base::IsStringASCII(type_utf8)) {
      formats.insert(ClipboardFormatType::Deserialize(type_utf8));
    }
  }
  std::move(callback).Run(std::move(formats));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::GetStandardFormats(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    GetStandardFormatsCallback callback) const {
  auto get_standard_formats =
      [](GetStandardFormatsCallback callback,
         base::flat_set<ClipboardFormatType> available_formats) {
        std::vector<std::u16string> types;
        if (available_formats.contains(ClipboardFormatType::PlainTextType())) {
          types.push_back(kMimeTypePlainText16);
        }
        if (available_formats.contains(ClipboardFormatType::HtmlType())) {
          types.push_back(kMimeTypeHtml16);
        }
        if (available_formats.contains(ClipboardFormatType::SvgType())) {
          types.push_back(kMimeTypeSvg16);
        }
        if (available_formats.contains(ClipboardFormatType::RtfType())) {
          types.push_back(kMimeTypeRtf16);
        }
        if (available_formats.contains(ClipboardFormatType::FilenamesType())) {
          types.push_back(kMimeTypeUriList16);
        }
        std::move(callback).Run(std::move(types));
      };

  GetAllAvailableFormats(
      buffer, data_dst,
      base::BindOnce(get_standard_formats, std::move(callback)));
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
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadAvailableTypesCallback callback) const {
  DCHECK(CalledOnValidThread());
  GetStandardFormats(
      buffer, data_dst,
      base::BindOnce(
          [](ReadAvailableTypesCallback callback,
             std::vector<std::u16string> types) {
            NSData* data = GetDataWithTypeFromPasteboard(
                GetPasteboard(),
                (NSString*)kUTTypeChromiumDataTransferCustomData);
            if (data) {
              ReadCustomDataTypes(base::apple::NSDataToSpan(data), &types);
            }
            std::move(callback).Run(std::move(types));
          },
          std::move(callback)));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadText(ClipboardBuffer buffer,
                            const std::optional<DataTransferEndpoint>& data_dst,
                            ReadTextCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);

  std::u16string result;
  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::PlainTextType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result.assign(base::SysNSStringToUTF16(contents));
  }
  std::move(callback).Run(std::move(result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadAsciiText(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadAsciiTextCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);

  std::string result;
  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::PlainTextType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result.assign(base::SysNSStringToUTF8(contents));
  }
  std::move(callback).Run(std::move(result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadHTML(ClipboardBuffer buffer,
                            const std::optional<DataTransferEndpoint>& data_dst,
                            ReadHtmlCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);

  std::u16string markup;
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
    markup = base::SysNSStringToUTF16(contents);
  }

  uint32_t fragment_start = 0;
  DCHECK_LE(markup.length(), std::numeric_limits<uint32_t>::max());
  uint32_t fragment_end = static_cast<uint32_t>(markup.length());

  // TODO: src_url
  std::move(callback).Run(std::move(markup), GURL(), fragment_start,
                          fragment_end);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadSvg(ClipboardBuffer buffer,
                           const std::optional<DataTransferEndpoint>& data_dst,
                           ReadSvgCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kSvg);

  std::u16string result;
  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::SvgType().ToNSString());
  if (data) {
    NSString* contents = [[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding];
    result = base::SysNSStringToUTF16(contents);
  }
  std::move(callback).Run(std::move(result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadRTF(ClipboardBuffer buffer,
                           const std::optional<DataTransferEndpoint>& data_dst,
                           ReadRTFCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kRtf);

  ReadData(ClipboardFormatType::RtfType(), data_dst, std::move(callback));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadPng(ClipboardBuffer buffer,
                           const std::optional<DataTransferEndpoint>& data_dst,
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
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadDataTransferCustomDataCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kCustomData);

  std::u16string result;
  NSData* data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), (NSString*)kUTTypeChromiumDataTransferCustomData);
  if (data) {
    if (std::optional<std::u16string> maybe_result =
            ReadCustomDataForType(base::apple::NSDataToSpan(data), type);
        maybe_result) {
      result = std::move(*maybe_result);
    }
  }
  std::move(callback).Run(std::move(result));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadFilenames(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadFilenamesCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kFilenames);

  auto items = [GetPasteboard()
      dataForPasteboardType:ClipboardFormatType::FilenamesType().ToNSString()
                  inItemSet:nil];
  if (!items) {
    std::move(callback).Run({});
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
  std::move(callback).Run(std::move(files));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadURL(const std::optional<DataTransferEndpoint>& data_dst,
                           ReadUrlCallback callback) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kUrl);

  std::string url;
  NSData* url_data = GetDataWithTypeFromPasteboard(
      GetPasteboard(), ClipboardFormatType::UrlType().ToNSString());
  if (url_data) {
    NSString* contents = [[NSString alloc] initWithData:url_data
                                               encoding:NSUTF8StringEncoding];
    url.assign(base::SysNSStringToUTF8(contents));
  }

  std::u16string title;
  NSData* title_data =
      GetDataWithTypeFromPasteboard(GetPasteboard(), kUTTypeUrlName);
  if (title_data) {
    NSString* contents = [[NSString alloc] initWithData:title_data
                                               encoding:NSUTF8StringEncoding];
    title.assign(base::SysNSStringToUTF16(contents));
  }
  ClipboardUrlInfo url_info;
  url_info.url = GURL(url);
  url_info.title = std::move(title);
  std::move(callback).Run(std::move(url_info));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::ReadData(const ClipboardFormatType& format,
                            const std::optional<DataTransferEndpoint>& data_dst,
                            ReadDataCallback callback) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);

  std::string result;
  NSData* data =
      GetDataWithTypeFromPasteboard(GetPasteboard(), format.ToNSString());
  if (data) {
    result.assign(base::as_string_view(base::apple::NSDataToSpan(data)));
  }
  std::move(callback).Run(std::move(result));
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardIOS::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    const std::vector<RawData>& raw_objects,
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
  for (const auto& raw_object : raw_objects) {
    DispatchPortableRepresentation(raw_object);
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
  WriteData(ClipboardFormatType::RtfType(), base::as_byte_span(rtf));
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

void ClipboardIOS::WriteURL(const ClipboardUrlInfo& url_info) {
  NSDictionary<NSString*, id>* bookmarkItem = @{
    ClipboardFormatType::UrlType().ToNSString() :
        base::SysUTF8ToNSString(url_info.url.spec()),
    kUTTypeUrlName : base::SysUTF16ToNSString(url_info.title),
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
