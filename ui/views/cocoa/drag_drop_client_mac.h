// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_
#define UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views_bridge_mac/drag_drop_client.h"

// This class acts as a bridge between NSPasteboardItem and OSExchangeData by
// implementing NSPasteboardItemDataProvider and writing data from
// OSExchangeData into the pasteboard.
VIEWS_EXPORT
@interface CocoaDragDropDataProvider : NSObject<NSPasteboardItemDataProvider>
- (instancetype)initWithData:(const ui::OSExchangeData&)data;
@end

namespace gfx {
class Point;
}

namespace views {
namespace test {
class DragDropClientMacTest;
}

class BridgedNativeWidgetImpl;
class View;

// Implements drag and drop on MacViews. This class acts as a bridge between
// the Views and native system's drag and drop. This class mimics
// DesktopDragDropClientAuraX11.
class VIEWS_EXPORT DragDropClientMac : public views_bridge_mac::DragDropClient {
 public:
  DragDropClientMac(BridgedNativeWidgetImpl* bridge, View* root_view);
  ~DragDropClientMac() override;

  // Initiates a drag and drop session. Returns the drag operation that was
  // applied at the end of the drag drop session.
  void StartDragAndDrop(View* view,
                        const ui::OSExchangeData& data,
                        int operation,
                        ui::DragDropTypes::DragEventSource source);

  DropHelper* drop_helper() { return &drop_helper_; }

  // views_bridge_mac::DragDropClient:
  NSDragOperation DragUpdate(id<NSDraggingInfo>) override;
  NSDragOperation Drop(id<NSDraggingInfo> sender) override;
  void EndDrag() override;
  void DragExit() override;

 private:
  friend class test::DragDropClientMacTest;

  // Converts the given NSPoint to the coordinate system in Views.
  gfx::Point LocationInView(NSPoint point) const;

  // Provides the data for the drag and drop session.
  base::scoped_nsobject<CocoaDragDropDataProvider> data_source_;

  // Used to handle drag and drop with Views.
  DropHelper drop_helper_;

  // The drag and drop operation.
  int operation_;

  // The bridge between the content view and the drag drop client.
  BridgedNativeWidgetImpl* bridge_;  // Weak. Owns |this|.

  // The closure for the drag and drop's run loop.
  base::Closure quit_closure_;

  // Whether |this| is the source of current dragging session.
  bool is_drag_source_;

  DISALLOW_COPY_AND_ASSIGN(DragDropClientMac);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_
