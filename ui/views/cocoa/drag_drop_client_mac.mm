// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/views/drag_utils.h"
#include "ui/views/widget/native_widget_mac.h"
#import "ui/views_bridge_mac/bridged_content_view.h"
#import "ui/views_bridge_mac/bridged_native_widget_impl.h"

@interface CocoaDragDropDataProvider ()
- (instancetype)initWithData:(const ui::OSExchangeData&)data;
- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard;
@end

@implementation CocoaDragDropDataProvider {
  std::unique_ptr<ui::OSExchangeData> data_;
}

- (instancetype)initWithData:(const ui::OSExchangeData&)data {
  if ((self = [super init])) {
    data_.reset(new OSExchangeData(
        std::unique_ptr<OSExchangeData::Provider>(data.provider().Clone())));
  }
  return self;
}

- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard {
  if ((self = [super init])) {
    data_ = ui::OSExchangeDataProviderMac::CreateDataFromPasteboard(pasteboard);
  }
  return self;
}

- (ui::OSExchangeData*)data {
  return data_.get();
}

// NSPasteboardItemDataProvider protocol implementation.

- (void)pasteboard:(NSPasteboard*)sender
                  item:(NSPasteboardItem*)item
    provideDataForType:(NSString*)type {
  const ui::OSExchangeDataProviderMac& provider =
      static_cast<const ui::OSExchangeDataProviderMac&>(data_->provider());
  NSData* ns_data = provider.GetNSDataForType(type);
  [sender setData:ns_data forType:type];
}

@end

namespace views {

DragDropClientMac::DragDropClientMac(BridgedNativeWidgetImpl* bridge,
                                     View* root_view)
    : drop_helper_(root_view),
      operation_(0),
      bridge_(bridge),
      quit_closure_(base::Closure()),
      is_drag_source_(false) {
  DCHECK(bridge);
}

DragDropClientMac::~DragDropClientMac() {}

void DragDropClientMac::StartDragAndDrop(
    View* view,
    const ui::OSExchangeData& data,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  data_source_.reset([[CocoaDragDropDataProvider alloc] initWithData:data]);
  operation_ = operation;
  is_drag_source_ = true;

  const ui::OSExchangeDataProviderMac& provider =
      static_cast<const ui::OSExchangeDataProviderMac&>(data.provider());

  // Release capture before beginning the dragging session. Capture may have
  // been acquired on the mouseDown, but capture is not required during the
  // dragging session and the mouseUp that would release it will be suppressed.
  bridge_->ReleaseCapture();

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = bridge_->ns_window();
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval event_time = [[NSApp currentEvent] timestamp];
  NSEvent* event = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                      location:position
                                 modifierFlags:NSLeftMouseDraggedMask
                                     timestamp:event_time
                                  windowNumber:[window windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:1.0];

  NSImage* image = gfx::NSImageFromImageSkiaWithColorSpace(
      provider.GetDragImage(), base::mac::GetSRGBColorSpace());

  // TODO(crbug/876201): This shouldn't happen. When a repro for this
  // is identified and the bug is fixed, change the early return to
  // a DCHECK.
  if (!image || NSEqualSizes([image size], NSZeroSize))
    return;

  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);
  [item setDataProvider:data_source_.get()
               forTypes:provider.GetAvailableTypes()];

  base::scoped_nsobject<NSDraggingItem> drag_item(
      [[NSDraggingItem alloc] initWithPasteboardWriter:item.get()]);

  // Subtract the image's height from the y location so that the mouse will be
  // at the upper left corner of the image.
  NSRect dragging_frame =
      NSMakeRect([event locationInWindow].x,
                 [event locationInWindow].y - [image size].height,
                 [image size].width, [image size].height);
  [drag_item setDraggingFrame:dragging_frame contents:image];

  [bridge_->ns_view() beginDraggingSessionWithItems:@[ drag_item.get() ]
                                              event:event
                                             source:bridge_->ns_view()];

  // Since Drag and drop is asynchronous on Mac, we need to spin a nested run
  // loop for consistency with other platforms.
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

NSDragOperation DragDropClientMac::DragUpdate(id<NSDraggingInfo> sender) {
  if (!data_source_.get()) {
    data_source_.reset([[CocoaDragDropDataProvider alloc]
        initWithPasteboard:[sender draggingPasteboard]]);
    operation_ = ui::DragDropTypes::NSDragOperationToDragOperation(
        [sender draggingSourceOperationMask]);
  }

  int drag_operation = drop_helper_.OnDragOver(
      *[data_source_ data], LocationInView([sender draggingLocation]),
      operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(drag_operation);
}

NSDragOperation DragDropClientMac::Drop(id<NSDraggingInfo> sender) {
  // OnDrop may delete |this|, so clear |data_source_| first.
  base::scoped_nsobject<CocoaDragDropDataProvider> data_source(
      std::move(data_source_));

  int drag_operation = drop_helper_.OnDrop(
      *[data_source data], LocationInView([sender draggingLocation]),
      operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(drag_operation);
}

void DragDropClientMac::EndDrag() {
  data_source_.reset();
  is_drag_source_ = false;

  // Allow a test to invoke EndDrag() without spinning the nested run loop.
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
    quit_closure_.Reset();
  }
}

void DragDropClientMac::DragExit() {
  drop_helper_.OnDragExit();
  if (!is_drag_source_)
    data_source_.reset();
}

gfx::Point DragDropClientMac::LocationInView(NSPoint point) const {
  return gfx::Point(point.x, NSHeight([bridge_->ns_window() frame]) - point.y);
}

}  // namespace views
