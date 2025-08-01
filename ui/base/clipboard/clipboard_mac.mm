// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/pasteboard_changed_observation.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/filename_util.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_mac.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "url/gurl.h"

@interface NSPasteboard (SPI)
// Sets the pasteboard to expire at the specified date. Tested to exist and work
// from macOS 12+. This must be called immediately after clearing the pasteboard
// or else it will not work and will log an error. This is used by WebKit; see
// https://github.com/WebKit/WebKit/commit/978bbafb7a3ab151703ba7e9694b78e2d63c3fe1
- (BOOL)_setExpirationDate:(NSDate*)date;
@end

namespace ui {

namespace {

NSPasteboard* GetPasteboard() {
  // The pasteboard can always be nil, since there is a finite amount of storage
  // that must be shared between all pasteboards.
  NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
  return pasteboard;
}

NSImage* GetNSImage(NSPasteboard* pasteboard) {
  // If the pasteboard's image data is not to its liking, the guts of NSImage
  // may throw, and that exception will leak. Prevent a crash in that case;
  // a blank image is better.
  NSImage* image;
  @try {
    if (pasteboard) {
      image = [[NSImage alloc] initWithPasteboard:pasteboard];
    }
  } @catch (id exception) {
  }
  if (!image) {
    return nil;
  }
  if (image.representations.count == 0u) {
    return nil;
  }
  return image;
}

// Read raw PNG bytes from the clipboard.
std::vector<uint8_t> GetPngFromPasteboard(NSPasteboard* pasteboard) {
  if (!pasteboard) {
    return {};
  }

  NSData* data = [pasteboard dataForType:NSPasteboardTypePNG];
  auto span = base::apple::NSDataToSpan(data);
  return {span.begin(), span.end()};
}

std::vector<uint8_t> EncodeGfxImageToPng(gfx::Image image) {
  base::AssertLongCPUWorkAllowed();

  if (image.IsEmpty()) {
    return {};
  }

  scoped_refptr<base::RefCountedMemory> mem = image.As1xPNGBytes();
  std::vector<uint8_t> image_data(mem->data(), mem->data() + mem->size());
  return image_data;
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
  if (base::FeatureList::IsEnabled(features::kPlatformClipboardMonitor)) {
    ClipboardMonitor::GetInstance()->SetNotifier(this);
  }
}

ClipboardMac::~ClipboardMac() {
  DCHECK(CalledOnValidThread());
  if (ClipboardMonitor::GetInstance()->GetNotifier() == this) {
    ClipboardMonitor::GetInstance()->SetNotifier(nullptr);
  }
  if (clipboard_change_subscription_) {
    StopNotifying();
  }
}

void ClipboardMac::StartNotifying() {
  if (!clipboard_change_subscription_) {
    // Unretained is safe because the subscription's lifetime is scoped to the
    // lifetime of this object.
    clipboard_change_subscription_ =
        base::RegisterPasteboardChangedCallback(base::BindRepeating(
            &ClipboardMac::ClipboardChanged, base::Unretained(this)));
  }
}

void ClipboardMac::StopNotifying() {
  if (clipboard_change_subscription_) {
    clipboard_change_subscription_ = base::CallbackListSubscription();
  }
}

void ClipboardMac::OnPreShutdown() {}

std::optional<DataTransferEndpoint> ClipboardMac::GetSource(
    ClipboardBuffer buffer) const {
  return GetSourceInternal(buffer, GetPasteboard());
}

std::optional<DataTransferEndpoint> ClipboardMac::GetSourceInternal(
    ClipboardBuffer buffer,
    NSPasteboard* pasteboard) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSString* source_url = [pasteboard stringForType:kUTTypeChromiumSourceUrl];

  if (!source_url) {
    return std::nullopt;
  }

  GURL gurl(base::SysNSStringToUTF8(source_url));
  if (!gurl.is_valid()) {
    return std::nullopt;
  }

  return DataTransferEndpoint(std::move(gurl));
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

  // https://crbug.com/1016740#c21: The pasteboard types array may end up going
  // away; make a copy.
  NSArray* types = [GetPasteboard().types copy];

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

void ClipboardMac::Clear(ClipboardBuffer buffer) {
  ClearInternal(buffer, GetPasteboard());
}

void ClipboardMac::ClearInternal(ClipboardBuffer buffer,
                                 NSPasteboard* pasteboard) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  [pasteboard clearContents];
}

std::vector<std::u16string> ClipboardMac::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  NSPasteboard* pb = GetPasteboard();
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer,
                        data_dst)) {
    types.push_back(kMimeTypePlainText16);
  }
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst)) {
    types.push_back(kMimeTypeHtml16);
  }
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst)) {
    types.push_back(kMimeTypeSvg16);
  }
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst)) {
    types.push_back(kMimeTypeRtf16);
  }
  if (IsFormatAvailable(ClipboardFormatType::FilenamesType(), buffer,
                        data_dst)) {
    types.push_back(kMimeTypeUriList16);
  } else if (pb && [NSImage canInitWithPasteboard:pb]) {
    // Finder Cmd+C places both file and icon onto the clipboard
    // (http://crbug.com/553686), so ignore images if we have detected files.
    // This means that if an image is present with file content, we will always
    // ignore the image, but this matches observable Safari behavior.
    types.push_back(kMimeTypePng16);
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

  if ([pb.types containsObject:kUTTypeChromiumDataTransferCustomData]) {
    NSData* data = [pb dataForType:kUTTypeChromiumDataTransferCustomData];
    if ([data length]) {
      ReadCustomDataTypes(base::apple::NSDataToSpan(data), types);
    }
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
  ReadPngInternal(buffer, GetPasteboard(), std::move(callback));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardMac::ReadDataTransferCustomData(
    ClipboardBuffer buffer,
    const std::u16string& type,
    const DataTransferEndpoint* data_dst,
    std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kCustomData);

  NSPasteboard* pb = GetPasteboard();
  if ([[pb types] containsObject:kUTTypeChromiumDataTransferCustomData]) {
    NSData* data = [pb dataForType:kUTTypeChromiumDataTransferCustomData];
    if ([data length]) {
      if (std::optional<std::u16string> maybe_result =
              ReadCustomDataForType(base::apple::NSDataToSpan(data), type);
          maybe_result) {
        *result = std::move(*maybe_result);
      }
    }
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
  std::ranges::move(files, std::back_inserter(*result));
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
    NSString* contents = [pb stringForType:kUTTypeUrlName];
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
    *result = as_string_view(base::apple::NSDataToSpan(data));
}

void ClipboardMac::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    const std::vector<RawData>& raw_objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src,
    uint32_t privacy_types) {
  WritePortableAndPlatformRepresentationsInternal(
      buffer, objects, std::move(raw_objects),
      std::move(platform_representations), std::move(data_src), GetPasteboard(),
      privacy_types);
}

void ClipboardMac::WritePortableAndPlatformRepresentationsInternal(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    const std::vector<RawData>& raw_objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src,
    NSPasteboard* pasteboard,
    uint32_t privacy_types) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  NSPasteboardContentsOptions pasteboard_options = 0;
  if (privacy_types & Clipboard::PrivacyTypes::kNoCloudClipboard) {
    pasteboard_options |= NSPasteboardContentsCurrentHostOnly;
  }
  [pasteboard prepareForNewContentsWithOptions:pasteboard_options];

  if (privacy_types & Clipboard::PrivacyTypes::kNoLocalClipboardHistory) {
    // One way to not add the pasteboard to the history is to give it an
    // expiration. For data that has "no history" specified, give it a
    // restricted lifetime so that the user has time to take action with it.
    //
    // WebKit uses this mechanism to implement privacy for data copied from
    // their ephemeral sessions. They use a lifetime of 8 minutes, derived from
    // their private browser lock time. The use of 5 minutes here is arbitrary,
    // but inspired by the rough lifetime that they chose.
    //
    // Note that another possible approach is also SPI; in macOS 26+ there
    // exists a method -[NSPasteboard _setShouldExcludeFromHistory:] which is
    // more explicitly an "exclude from history" method than the "set an
    // expiration" method used here. However, it is not available on all macOS
    // releases that Chromium supports. When Chromium requires macOS 26+, it
    // might be worthwhile looking into whether that is a better choice.
    [GetPasteboard()
        _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:5 * 60]];
  }

  if (privacy_types & Clipboard::PrivacyTypes::kNoDisplay) {
    [GetPasteboard() setData:nil forType:kUTTypeConfidentialData];
  }

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects) {
    DispatchPortableRepresentation(object.second);
  }
  for (const auto& raw_object : raw_objects) {
    DispatchPortableRepresentation(raw_object);
  }

  if (data_src && data_src->IsUrlType()) {
    [pasteboard setString:base::SysUTF8ToNSString(data_src->GetURL()->spec())
                  forType:kUTTypeChromiumSourceUrl];
  }

  // If not actively monitoring, notify immediately. Otherwise, when monitoring,
  // the change to the pasteboard's `changeCount` will be detected by
  // `CheckClipboardForChanges` (called by `clipboard_polling_timer_`),
  // which will then update `last_known_sequence_number_` and
  // `clipboard_sequence_`, and subsequently call
  // `NotifyClipboardDataChanged`. This avoids redundant notifications.
  if (!clipboard_change_subscription_) {
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }
}

void ClipboardMac::WriteText(std::string_view text) {
  [GetPasteboard() setString:base::SysUTF8ToNSString(text)
                     forType:NSPasteboardTypeString];
}

void ClipboardMac::WriteHTML(std::string_view markup,
                             std::optional<std::string_view> /* source_url */) {
  // We need to mark it as utf-8. (see crbug.com/40759159)
  std::string html_fragment_str("<meta charset='utf-8'>");
  html_fragment_str.append(markup);
  NSString* html_fragment = base::SysUTF8ToNSString(html_fragment_str);

  [GetPasteboard() setString:html_fragment forType:NSPasteboardTypeHTML];
}

void ClipboardMac::WriteSvg(std::string_view markup) {
  [GetPasteboard() setString:base::SysUTF8ToNSString(markup)
                     forType:ClipboardFormatType::SvgType().ToNSString()];
}

void ClipboardMac::WriteRTF(std::string_view rtf) {
  WriteData(ClipboardFormatType::RtfType(), base::as_byte_span(rtf));
}

void ClipboardMac::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void ClipboardMac::WriteBookmark(std::string_view title, std::string_view url) {
  NSArray<NSPasteboardItem*>* items = clipboard_util::PasteboardItemsFromUrls(
      @[ base::SysUTF8ToNSString(url) ], @[ base::SysUTF8ToNSString(title) ]);
  clipboard_util::AddDataToPasteboard(GetPasteboard(), items.firstObject);
}

void ClipboardMac::WriteBitmap(const SkBitmap& bitmap) {
  // The bitmap type is sanitized to be N32 before we get here. The conversion
  // to an NSImage would not explode if we got this wrong, so this is not a
  // security CHECK.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  WriteBitmapInternal(bitmap, GetPasteboard());
}

void ClipboardMac::WriteData(const ClipboardFormatType& format,
                             base::span<const uint8_t> data) {
  [GetPasteboard() setData:[NSData dataWithBytes:data.data() length:data.size()]
                   forType:format.ToNSString()];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardMac::WriteWebSmartPaste() {
  NSString* format = ClipboardFormatType::WebKitSmartPasteType().ToNSString();
  [GetPasteboard() setData:nil forType:format];
}

void ClipboardMac::ReadPngInternal(ClipboardBuffer buffer,
                                   NSPasteboard* pasteboard,
                                   ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  std::vector<uint8_t> png = GetPngFromPasteboard(pasteboard);
  if (!png.empty()) {
    std::move(callback).Run(std::move(png));
    return;
  }

  // If we can’t read a PNG, try reading for an NSImage, and if successful,
  // transcode it to PNG.
  NSImage* image = GetNSImage(pasteboard);
  if (!image) {
    std::move(callback).Run({});
    return;
  }

  auto gfx_image = gfx::Image(image);
  if (gfx_image.IsEmpty()) {
    std::move(callback).Run({});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&EncodeGfxImageToPng, std::move(gfx_image)),
      std::move(callback));
}

void ClipboardMac::WriteBitmapInternal(const SkBitmap& bitmap,
                                       NSPasteboard* pasteboard) {
  // The bitmap type is sanitized to be N32 before we get here. The conversion
  // to an NSImage would not explode if we got this wrong, so this is not a
  // security CHECK.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  NSBitmapImageRep* image_rep = skia::SkBitmapToNSBitmapImageRep(bitmap);
  CHECK(image_rep) << "SkBitmapToNSBitmapImageRep failed";
  // Attempt to format the image representation as a PNG, and write it directly
  // to the clipboard if this succeeds. This will write both a PNG and a TIFF.
  NSData* data = [image_rep representationUsingType:NSBitmapImageFileTypePNG
                                         properties:@{}];
  if (data) {
    NSPasteboardItem* pasteboard_item = [[NSPasteboardItem alloc] init];
    [pasteboard_item setData:data forType:NSPasteboardTypePNG];
    if ([pasteboard writeObjects:@[ pasteboard_item ]]) {
      return;
    }
  }

  // Otherwise, fall back to writing the NSImage directly to the clipboard,
  // which will write only a TIFF.
  NSImage* image = [[NSImage alloc] init];
  [image addRepresentation:image_rep];
  [image setSize:NSMakeSize(bitmap.width(), bitmap.height())];
  [pasteboard writeObjects:@[ image ]];
}

void ClipboardMac::ClipboardChanged() {
  NSInteger current_sequence_number = GetPasteboard().changeCount;
  if (current_sequence_number != last_known_sequence_number_) {
    last_known_sequence_number_ = current_sequence_number;
    // Update clipboard_sequence_ to reflect the new number and generate a new
    // token, so subsequent calls to GetSequenceNumber() return the latest
    // state.
    clipboard_sequence_ = {current_sequence_number,
                           ClipboardSequenceNumberToken()};
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }
}

}  // namespace ui
