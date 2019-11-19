// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/views/drag_utils.h"
#include "ui/views/widget/native_widget_mac.h"

namespace views {

DragDropClientMac::DragDropClientMac(
    remote_cocoa::NativeWidgetNSWindowBridge* bridge,
    View* root_view)
    : drop_helper_(root_view), bridge_(bridge) {
  DCHECK(bridge);
}

DragDropClientMac::~DragDropClientMac() {}

void DragDropClientMac::StartDragAndDrop(
    View* view,
    std::unique_ptr<ui::OSExchangeData> data,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  exchange_data_ = std::move(data);
  source_operation_ = operation;
  is_drag_source_ = true;

  const ui::OSExchangeDataProviderMac& provider_mac =
      static_cast<const ui::OSExchangeDataProviderMac&>(
          exchange_data_->provider());

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
      provider_mac.GetDragImage(), base::mac::GetSRGBColorSpace());

  DCHECK(!NSEqualSizes([image size], NSZeroSize));
  NSDraggingItem* drag_item = provider_mac.GetDraggingItem();

  // Subtract the image's height from the y location so that the mouse will be
  // at the upper left corner of the image.
  NSRect dragging_frame =
      NSMakeRect([event locationInWindow].x,
                 [event locationInWindow].y - [image size].height,
                 [image size].width, [image size].height);
  [drag_item setDraggingFrame:dragging_frame contents:image];

  [bridge_->ns_view() beginDraggingSessionWithItems:@[ drag_item ]
                                              event:event
                                             source:bridge_->ns_view()];

  // Since Drag and drop is asynchronous on Mac, we need to spin a nested run
  // loop for consistency with other platforms.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

NSDragOperation DragDropClientMac::DragUpdate(id<NSDraggingInfo> sender) {
  if (!exchange_data_) {
    exchange_data_ = std::make_unique<OSExchangeData>(
        ui::OSExchangeDataProviderMac::CreateProviderWrappingPasteboard(
            [sender draggingPasteboard]));
    source_operation_ = ui::DragDropTypes::NSDragOperationToDragOperation(
        [sender draggingSourceOperationMask]);
  }

  last_operation_ = drop_helper_.OnDragOver(
      *exchange_data_, LocationInView([sender draggingLocation]),
      source_operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(last_operation_);
}

NSDragOperation DragDropClientMac::Drop(id<NSDraggingInfo> sender) {
  // OnDrop may delete |this|, so clear |exchange_data_| first.
  std::unique_ptr<ui::OSExchangeData> exchange_data = std::move(exchange_data_);

  int drag_operation = drop_helper_.OnDrop(
      *exchange_data, LocationInView([sender draggingLocation]),
      last_operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(drag_operation);
}

void DragDropClientMac::EndDrag() {
  exchange_data_.reset();
  is_drag_source_ = false;

  // Allow a test to invoke EndDrag() without spinning the nested run loop.
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

void DragDropClientMac::DragExit() {
  drop_helper_.OnDragExit();
  if (!is_drag_source_)
    exchange_data_.reset();
}

gfx::Point DragDropClientMac::LocationInView(NSPoint point) const {
  NSRect content_rect = [bridge_->ns_window()
      contentRectForFrameRect:[bridge_->ns_window() frame]];
  return gfx::Point(point.x, NSHeight(content_rect) - point.y);
}

}  // namespace views
