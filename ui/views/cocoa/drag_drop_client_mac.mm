// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
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

DragDropClientMac::~DragDropClientMac() = default;

void DragDropClientMac::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    int operation,
    ui::mojom::DragEventSource source) {
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
  NSEvent* event =
      [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
                         location:window.mouseLocationOutsideOfEventStream
                    modifierFlags:0
                        timestamp:NSApp.currentEvent.timestamp
                     windowNumber:window.windowNumber
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:1.0];

  NSImage* image = gfx::NSImageFromImageSkia(provider_mac.GetDragImage());

  DCHECK(!NSEqualSizes(image.size, NSZeroSize));
  NSArray<NSDraggingItem*>* drag_items = provider_mac.GetDraggingItems();
  if (!drag_items) {
    // The source of this data is ultimately the OS, and while this shouldn't be
    // nil, it is documented as possibly being so if things are broken. If so,
    // fail early rather than try to start a drag of a nil item list and making
    // things worse.
    return;
  }

  // At this point the mismatch between the Views drag API, which assumes a
  // single drag item, and the macOS drag API, which allows for multiple items,
  // is encountered. For now, set the dragging frame and image on the first
  // item, and set nil images for the remaining items. In the future, when Views
  // becomes more capable in the area of the clipboard, revisit this.

  // Create the frame to cause the mouse to be centered over the image, with the
  // image slightly above the mouse pointer for visibility.
  NSRect dragging_frame =
      NSMakeRect(event.locationInWindow.x - image.size.width / 2,
                 event.locationInWindow.y - image.size.height / 4,
                 image.size.width, image.size.height);
  for (NSUInteger i = 0; i < drag_items.count; ++i) {
    if (i == 0) {
      [drag_items[i] setDraggingFrame:dragging_frame contents:image];
    } else {
      [drag_items[i] setDraggingFrame:NSMakeRect(0, 0, 1, 1) contents:nil];
    }
  }

  [bridge_->ns_view() beginDraggingSessionWithItems:drag_items
                                              event:event
                                             source:bridge_->ns_view()];

  // Since Drag and drop is asynchronous on the Mac, spin a nested run loop for
  // consistency with other platforms.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

NSDragOperation DragDropClientMac::DragUpdate(id<NSDraggingInfo> sender) {
  if (!exchange_data_) {
    exchange_data_ = std::make_unique<OSExchangeData>(
        ui::OSExchangeDataProviderMac::CreateProviderWrappingPasteboard(
            sender.draggingPasteboard));
    source_operation_ = ui::DragDropTypes::NSDragOperationToDragOperation(
        sender.draggingSourceOperationMask);
  }
  last_operation_ =
      drop_helper_.OnDragOver(*exchange_data_,
                              LocationInView(sender.draggingLocation,
                                             sender.draggingDestinationWindow),
                              source_operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(last_operation_);
}

NSDragOperation DragDropClientMac::Drop(id<NSDraggingInfo> sender) {
  // OnDrop may delete |this|, so clear |exchange_data_| first.
  std::unique_ptr<ui::OSExchangeData> exchange_data = std::move(exchange_data_);

  ui::mojom::DragOperation drag_operation = drop_helper_.OnDrop(
      *exchange_data, LocationInView([sender draggingLocation]),
      last_operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(
      static_cast<int>(drag_operation));
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
  NSRect content_rect =
      [bridge_->ns_window() contentRectForFrameRect:bridge_->ns_window().frame];
  return gfx::Point(point.x, NSHeight(content_rect) - point.y);
}

// In immersive fullscreen, the `NSToolbarFullScreenWindow` hosts both the tab
// strip and toolbar. Convert the point to the corresponding hosted view's
// coordinates.
gfx::Point DragDropClientMac::LocationInView(
    NSPoint point,
    NSWindow* destination_window) const {
  if (remote_cocoa::IsNSToolbarFullScreenWindow(destination_window)) {
    NSView* overlay_view = [destination_window.contentView hitTest:point];
    point = [overlay_view convertPoint:point fromView:nil];
  }
  return LocationInView(point);
}
}  // namespace views
