// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

#import <Cocoa/Cocoa.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "url/gurl.h"

@interface CrPasteboardItemWrapper : NSObject <NSPasteboardWriting>
- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem;
@end

@implementation CrPasteboardItemWrapper {
  NSPasteboardItem* __strong _pasteboardItem;
}

- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem {
  if ((self = [super init])) {
    _pasteboardItem = pasteboardItem;
  }

  return self;
}

- (NSArray<NSString*>*)writableTypesForPasteboard:(NSPasteboard*)pasteboard {
  // If the NSPasteboardItem hasn't been added to an NSPasteboard, then the
  // -[NSPasteboardItem writableTypesForPasteboard:] will return -types. But if
  // it has been added to a pasteboard, it will return nil. This pasteboard item
  // was added implicitly by adding flavors to the owned pasteboard of
  // OwningProvider, so call -types to actually get data.
  //
  // Merge in the ui::kUTTypeChromiumInitiatedDrag type, so that all of Chromium
  // is marked to receive the drags. TODO(avi): Wire up MacViews so that
  // BridgedContentView properly registers the result of View::GetDropFormats()
  // rather than OSExchangeDataProviderMac::SupportedPasteboardTypes().
  return [_pasteboardItem.types
      arrayByAddingObject:ui::kUTTypeChromiumInitiatedDrag];
}

- (NSPasteboardWritingOptions)writingOptionsForType:(NSString*)type
                                         pasteboard:(NSPasteboard*)pasteboard {
  // It is critical to return 0 here. If any flavors are promised, then when the
  // app quits, AppKit will call in the promises, and the backing pasteboard
  // will likely be long-deallocated. Yes, AppKit will call in promises for
  // *all* promised flavors on *all* pasteboards, not just those pasteboards
  // used for copy/paste.
  return 0;
}

- (id)pasteboardPropertyListForType:(NSString*)type {
  if ([type isEqual:ui::kUTTypeChromiumInitiatedDrag])
    return [NSData data];

  // Like above, an NSPasteboardItem added to a pasteboard will return nil from
  // -pasteboardPropertyListForType:, so call -dataForType: instead.
  return [_pasteboardItem dataForType:type];
}

@end

namespace ui {

namespace {

class OwningProvider : public OSExchangeDataProviderMac {
 public:
  OwningProvider() : owned_pasteboard_(new UniquePasteboard) {}
  OwningProvider(const OwningProvider& provider) = default;

  std::unique_ptr<OSExchangeDataProvider> Clone() const override {
    return std::make_unique<OwningProvider>(*this);
  }

  NSPasteboard* GetPasteboard() const override {
    return owned_pasteboard_->get();
  }

 private:
  scoped_refptr<UniquePasteboard> owned_pasteboard_;
};

class WrappingProvider : public OSExchangeDataProviderMac {
 public:
  explicit WrappingProvider(NSPasteboard* pasteboard)
      : wrapped_pasteboard_(pasteboard) {}
  WrappingProvider(const WrappingProvider& provider) = default;

  std::unique_ptr<OSExchangeDataProvider> Clone() const override {
    return std::make_unique<WrappingProvider>(*this);
  }

  NSPasteboard* GetPasteboard() const override { return wrapped_pasteboard_; }

 private:
  __strong NSPasteboard* wrapped_pasteboard_;
};

}  // namespace

OSExchangeDataProviderMac::OSExchangeDataProviderMac() = default;
OSExchangeDataProviderMac::OSExchangeDataProviderMac(
    const OSExchangeDataProviderMac&) = default;
OSExchangeDataProviderMac& OSExchangeDataProviderMac::operator=(
    const OSExchangeDataProviderMac&) = default;

OSExchangeDataProviderMac::~OSExchangeDataProviderMac() = default;

// static
std::unique_ptr<OSExchangeDataProviderMac>
OSExchangeDataProviderMac::CreateProvider() {
  return std::make_unique<OwningProvider>();
}

// static
std::unique_ptr<OSExchangeDataProviderMac>
OSExchangeDataProviderMac::CreateProviderWrappingPasteboard(
    NSPasteboard* pasteboard) {
  return std::make_unique<WrappingProvider>(pasteboard);
}

void OSExchangeDataProviderMac::MarkRendererTaintedFromOrigin(
    const url::Origin& origin) {
  NSString* string = origin.opaque()
                         ? [NSString string]
                         : base::SysUTF8ToNSString(origin.Serialize());
  [GetPasteboard() setString:string
                     forType:kUTTypeChromiumRendererInitiatedDrag];
}

bool OSExchangeDataProviderMac::IsRendererTainted() const {
  return [GetPasteboard().types
      containsObject:kUTTypeChromiumRendererInitiatedDrag];
}

std::optional<url::Origin> OSExchangeDataProviderMac::GetRendererTaintedOrigin()
    const {
  NSString* item =
      [GetPasteboard() stringForType:kUTTypeChromiumRendererInitiatedDrag];
  if (!item) {
    return std::nullopt;
  }

  if (0 == [item length]) {
    return url::Origin();
  }

  return url::Origin::Create(GURL(base::SysNSStringToUTF8(item)));
}

void OSExchangeDataProviderMac::MarkAsFromPrivileged() {
  [GetPasteboard() setData:[NSData data]
                   forType:kUTTypeChromiumPrivilegedInitiatedDrag];
}

bool OSExchangeDataProviderMac::IsFromPrivileged() const {
  return [GetPasteboard().types
      containsObject:kUTTypeChromiumPrivilegedInitiatedDrag];
}

void OSExchangeDataProviderMac::SetString(const std::u16string& string) {
  [GetPasteboard() setString:base::SysUTF16ToNSString(string)
                     forType:NSPasteboardTypeString];
}

void OSExchangeDataProviderMac::SetURL(const GURL& url,
                                       const std::u16string& title) {
  NSArray<NSPasteboardItem*>* items = clipboard_util::PasteboardItemsFromUrls(
      @[ base::SysUTF8ToNSString(url.spec()) ],
      @[ base::SysUTF16ToNSString(title) ]);
  clipboard_util::AddDataToPasteboard(GetPasteboard(), items.firstObject);
}

void OSExchangeDataProviderMac::SetFilename(const base::FilePath& path) {
  std::vector<FileInfo> filenames(1, FileInfo(path, base::FilePath()));
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void OSExchangeDataProviderMac::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void OSExchangeDataProviderMac::SetPickledData(
    const ClipboardFormatType& format,
    const base::Pickle& data) {
  NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
  [GetPasteboard() setData:ns_data forType:format.ToNSString()];
}

std::optional<std::u16string> OSExchangeDataProviderMac::GetString() const {
  NSString* item = [GetPasteboard() stringForType:NSPasteboardTypeString];
  if (item) {
    return base::SysNSStringToUTF16(item);
  }

  // There was no NSString, check for an NSURL.
  if (std::optional<UrlInfo> url_info =
          GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
      url_info.has_value()) {
    return base::UTF8ToUTF16(url_info->url.spec());
  }

  return std::nullopt;
}

std::optional<OSExchangeDataProvider::UrlInfo>
OSExchangeDataProviderMac::GetURLAndTitle(FilenameToURLPolicy policy) const {
  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(
          GetPasteboard(), policy == FilenameToURLPolicy::CONVERT_FILENAMES);
  if (!urls_and_titles.count) {
    return std::nullopt;
  }

  GURL url(base::SysNSStringToUTF8(urls_and_titles.firstObject.URL));
  return UrlInfo{std::move(url),
                 base::SysNSStringToUTF16(urls_and_titles.firstObject.title)};
}

std::optional<std::vector<GURL>> OSExchangeDataProviderMac::GetURLs(
    FilenameToURLPolicy policy) const {
  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(
          GetPasteboard(), policy == FilenameToURLPolicy::CONVERT_FILENAMES);
  if (!urls_and_titles.count) {
    return std::nullopt;
  }

  std::vector<GURL> local_urls;
  for (URLAndTitle* url_and_title in urls_and_titles) {
    local_urls.emplace_back(base::SysNSStringToUTF8(url_and_title.URL));
  }
  return local_urls;
}

std::optional<std::vector<FileInfo>> OSExchangeDataProviderMac::GetFilenames()
    const {
  std::vector<FileInfo> files =
      clipboard_util::FilesFromPasteboard(GetPasteboard());
  if (files.empty()) {
    return std::nullopt;
  }

  return files;
}

std::optional<base::Pickle> OSExchangeDataProviderMac::GetPickledData(
    const ClipboardFormatType& format) const {
  NSData* ns_data = [GetPasteboard() dataForType:format.ToNSString()];
  if (!ns_data) {
    return std::nullopt;
  }

  return base::Pickle::WithData(base::apple::NSDataToSpan(ns_data));
}

bool OSExchangeDataProviderMac::HasString() const {
  return GetString().has_value();
}

bool OSExchangeDataProviderMac::HasURL(FilenameToURLPolicy policy) const {
  return GetURLAndTitle(policy).has_value();
}

bool OSExchangeDataProviderMac::HasFile() const {
  return [GetPasteboard().types containsObject:NSPasteboardTypeFileURL];
}

bool OSExchangeDataProviderMac::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return [GetPasteboard().types containsObject:format.ToNSString()];
}

void OSExchangeDataProviderMac::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  NOTIMPLEMENTED();
}

std::optional<OSExchangeDataProvider::FileContentsInfo>
OSExchangeDataProviderMac::GetFileContents() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

bool OSExchangeDataProviderMac::HasFileContents() const {
  NOTIMPLEMENTED();
  return false;
}

void OSExchangeDataProviderMac::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  cursor_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderMac::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderMac::GetDragImageOffset() const {
  return cursor_offset_;
}

NSArray<NSDraggingItem*>* OSExchangeDataProviderMac::GetDraggingItems() const {
  // What's going on here is that initiating a drag (-[NSView
  // beginDraggingSessionWithItems...]) requires a dragging item. Even though
  // pasteboard items are NSPasteboardWriters, they are locked to their
  // pasteboard and cannot be used to initiate a drag with another pasteboard
  // (hello https://crbug.com/928684). Therefore, wrap them.

  NSArray<NSPasteboardItem*>* pasteboard_items =
      GetPasteboard().pasteboardItems;
  if (!pasteboard_items) {
    return nil;
  }

  NSMutableArray<NSDraggingItem*>* drag_items = [NSMutableArray array];
  for (NSPasteboardItem* item in pasteboard_items) {
    CrPasteboardItemWrapper* wrapper =
        [[CrPasteboardItemWrapper alloc] initWithPasteboardItem:item];
    NSDraggingItem* drag_item =
        [[NSDraggingItem alloc] initWithPasteboardWriter:wrapper];

    [drag_items addObject:drag_item];
  }

  return drag_items;
}

// static
NSArray* OSExchangeDataProviderMac::SupportedPasteboardTypes() {
  return @[
    kUTTypeChromiumInitiatedDrag, kUTTypeChromiumPrivilegedInitiatedDrag,
    kUTTypeChromiumRendererInitiatedDrag, kUTTypeChromiumDataTransferCustomData,
    kUTTypeWebKitWebURLsWithTitles, kUTTypeChromiumSourceURL,
    NSPasteboardTypeFileURL, NSPasteboardTypeHTML, NSPasteboardTypeRTF,
    NSPasteboardTypeString, NSPasteboardTypeURL
  ];
}

void OSExchangeDataProviderMac::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {}

DataTransferEndpoint* OSExchangeDataProviderMac::GetSource() const {
  return nullptr;
}

}  // namespace ui
