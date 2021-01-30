// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#include "url/gurl.h"

@interface CrPasteboardItemWrapper : NSObject <NSPasteboardWriting>
- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem;
@end

@implementation CrPasteboardItemWrapper {
  base::scoped_nsobject<NSPasteboardItem> _pasteboardItem;
}

- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem {
  if ((self = [super init])) {
    _pasteboardItem.reset([pasteboardItem retain]);
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
  // Merge in the ui::kChromeDragDummyPboardType type, so that all of Chromium
  // is marked to receive the drags. TODO(avi): Wire up MacViews so that
  // BridgedContentView properly registers the result of View::GetDropFormats()
  // rather than OSExchangeDataProviderMac::SupportedPasteboardTypes().
  return [[_pasteboardItem types]
      arrayByAddingObject:ui::kChromeDragDummyPboardType];
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
  if ([type isEqual:ui::kChromeDragDummyPboardType])
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
  OwningProvider()
      : OSExchangeDataProviderMac(), owned_pasteboard_(new UniquePasteboard) {}
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
  WrappingProvider(NSPasteboard* pasteboard)
      : OSExchangeDataProviderMac(), wrapped_pasteboard_([pasteboard retain]) {}
  WrappingProvider(const WrappingProvider& provider) = default;

  std::unique_ptr<OSExchangeDataProvider> Clone() const override {
    return std::make_unique<WrappingProvider>(*this);
  }

  NSPasteboard* GetPasteboard() const override { return wrapped_pasteboard_; }

 private:
  base::scoped_nsobject<NSPasteboard> wrapped_pasteboard_;
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

void OSExchangeDataProviderMac::MarkOriginatedFromRenderer() {
  NOTIMPLEMENTED();
}

bool OSExchangeDataProviderMac::DidOriginateFromRenderer() const {
  NOTIMPLEMENTED();
  return false;
}

void OSExchangeDataProviderMac::SetString(const base::string16& string) {
  [GetPasteboard() setString:base::SysUTF16ToNSString(string)
                     forType:NSPasteboardTypeString];
}

void OSExchangeDataProviderMac::SetURL(const GURL& url,
                                       const base::string16& title) {
  base::scoped_nsobject<NSPasteboardItem> item =
      ClipboardUtil::PasteboardItemFromUrl(base::SysUTF8ToNSString(url.spec()),
                                           base::SysUTF16ToNSString(title));
  ClipboardUtil::AddDataToPasteboard(GetPasteboard(), item);
}

void OSExchangeDataProviderMac::SetFilename(const base::FilePath& path) {
  [GetPasteboard() setPropertyList:@[ base::SysUTF8ToNSString(path.value()) ]
                           forType:NSFilenamesPboardType];
}

void OSExchangeDataProviderMac::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  if (filenames.empty())
    return;

  NSMutableArray* paths = [NSMutableArray arrayWithCapacity:filenames.size()];

  for (const auto& filename : filenames) {
    NSString* path = base::SysUTF8ToNSString(filename.path.value());
    [paths addObject:path];
  }
  [GetPasteboard() setPropertyList:paths forType:NSFilenamesPboardType];
}

void OSExchangeDataProviderMac::SetPickledData(
    const ClipboardFormatType& format,
    const base::Pickle& data) {
  NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
  [GetPasteboard() setData:ns_data forType:format.ToNSString()];
}

bool OSExchangeDataProviderMac::GetString(base::string16* data) const {
  DCHECK(data);
  NSString* item = [GetPasteboard() stringForType:NSPasteboardTypeString];
  if (item) {
    *data = base::SysNSStringToUTF16(item);
    return true;
  }

  // There was no NSString, check for an NSURL.
  GURL url;
  base::string16 title;
  bool result = GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES,
                               &url, &title);
  if (result)
    *data = base::UTF8ToUTF16(url.spec());

  return result;
}

bool OSExchangeDataProviderMac::GetURLAndTitle(FilenameToURLPolicy policy,
                                               GURL* url,
                                               base::string16* title) const {
  DCHECK(url);
  DCHECK(title);

  if (PopulateURLAndTitleFromPasteboard(url, title, GetPasteboard(), false)) {
    return true;
  }

  // If there are no URLs, try to convert a filename to a URL if the policy
  // allows it. The title remains blank.
  //
  // This could be done in the call to PopulateURLAndTitleFromPasteboard above
  // if |true| were passed in as the last parameter, but that function strips
  // the trailing slashes off of paths and always returns the last path element
  // as the title whereas no path conversion nor title is wanted.
  base::FilePath path;
  if (policy != FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES &&
      GetFilename(&path)) {
    NSURL* fileUrl =
        [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
    *url =
        GURL([[fileUrl absoluteString] stringByStandardizingPath].UTF8String);
    return true;
  }

  return false;
}

bool OSExchangeDataProviderMac::GetFilename(base::FilePath* path) const {
  NSArray* paths = [GetPasteboard() propertyListForType:NSFilenamesPboardType];
  if ([paths count] == 0)
    return false;

  *path = base::FilePath(base::SysNSStringToUTF8(paths[0]));
  return true;
}

bool OSExchangeDataProviderMac::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  NSArray* paths = [GetPasteboard() propertyListForType:NSFilenamesPboardType];
  if ([paths count] == 0)
    return false;

  for (NSString* path in paths)
    filenames->push_back(
        {base::FilePath(base::SysNSStringToUTF8(path)), base::FilePath()});

  return true;
}

bool OSExchangeDataProviderMac::GetPickledData(
    const ClipboardFormatType& format,
    base::Pickle* data) const {
  DCHECK(data);
  NSData* ns_data = [GetPasteboard() dataForType:format.ToNSString()];
  if (!ns_data)
    return false;

  *data =
      base::Pickle(static_cast<const char*>([ns_data bytes]), [ns_data length]);
  return true;
}

bool OSExchangeDataProviderMac::HasString() const {
  base::string16 string;
  return GetString(&string);
}

bool OSExchangeDataProviderMac::HasURL(FilenameToURLPolicy policy) const {
  GURL url;
  base::string16 title;
  return GetURLAndTitle(policy, &url, &title);
}

bool OSExchangeDataProviderMac::HasFile() const {
  return [[GetPasteboard() types] containsObject:NSFilenamesPboardType];
}

bool OSExchangeDataProviderMac::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return [[GetPasteboard() types] containsObject:format.ToNSString()];
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

NSDraggingItem* OSExchangeDataProviderMac::GetDraggingItem() const {
  // What's going on here is that initiating a drag (-[NSView
  // beginDraggingSessionWithItems...]) requires a dragging item. Even though
  // pasteboard items are NSPasteboardWriters, they are locked to their
  // pasteboard and cannot be used to initiate a drag with another pasteboard
  // (hello https://crbug.com/928684). Therefore, wrap them.
  //
  // OSExchangeDataProviderMac was written to the old NSPasteboard APIs that
  // didn't account for more than one item. This kinda matches Views which also
  // assumes that only one drag item can exist at a time. TODO(avi): Fix all of
  // Views to be able to handle drags of more than one item. Then rewrite
  // OSExchangeDataProviderMac to the new NSPasteboard item API.

  NSArray* pasteboardItems = [GetPasteboard() pasteboardItems];
  DCHECK(pasteboardItems);
  DCHECK_EQ(1u, [pasteboardItems count]);

  CrPasteboardItemWrapper* wrapper = [[[CrPasteboardItemWrapper alloc]
      initWithPasteboardItem:[pasteboardItems firstObject]] autorelease];

  NSDraggingItem* drag_item =
      [[[NSDraggingItem alloc] initWithPasteboardWriter:wrapper] autorelease];

  return drag_item;
}

// static
NSArray* OSExchangeDataProviderMac::SupportedPasteboardTypes() {
  return @[
    kWebCustomDataPboardType, ClipboardUtil::UTIForWebURLsAndTitles(),
    NSURLPboardType, NSFilenamesPboardType, kChromeDragDummyPboardType,
    NSStringPboardType, NSHTMLPboardType, NSRTFPboardType,
    NSFilenamesPboardType, kWebCustomDataPboardType, NSPasteboardTypeString
  ];
}

void OSExchangeDataProviderMac::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {}

DataTransferEndpoint* OSExchangeDataProviderMac::GetSource() const {
  return nullptr;
}

}  // namespace ui
