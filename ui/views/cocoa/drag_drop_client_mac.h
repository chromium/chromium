// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_
#define UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/remote_cocoa/app_shim/drag_drop_client.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/drop_helper.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

namespace views {
namespace test {
class DragDropClientMacTest;
}  // namespace test

class View;

// Implements drag and drop on MacViews. This class acts as a bridge between
// the Views and native system's drag and drop. This class mimics
// DesktopDragDropClientAuraX11.
class VIEWS_EXPORT DragDropClientMac : public remote_cocoa::DragDropClient {
 public:
  DragDropClientMac(remote_cocoa::NativeWidgetNSWindowBridge* bridge,
                    View* root_view);

  DragDropClientMac(const DragDropClientMac&) = delete;
  DragDropClientMac& operator=(const DragDropClientMac&) = delete;

  ~DragDropClientMac() override;

  // Initiates a drag and drop session.
  void StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                        int operation,
                        ui::mojom::DragEventSource source);

  DropHelper* drop_helper() { return &drop_helper_; }

  // remote_cocoa::DragDropClient:
  NSDragOperation DragUpdate(id<NSDraggingInfo>) override;
  NSDragOperation Drop(id<NSDraggingInfo> sender) override;
  void EndDrag() override;
  void DragExit() override;

 private:
  friend class test::DragDropClientMacTest;

  // Converts the given NSPoint to the coordinate system in Views.
  gfx::Point LocationInView(NSPoint point) const;
  gfx::Point LocationInView(NSPoint point, NSWindow* destination_window) const;

  // Provides the data for the drag and drop session.
  std::unique_ptr<ui::OSExchangeData> exchange_data_;

  // Used to handle drag and drop with Views.
  DropHelper drop_helper_;

  // The drag and drop operation.
  int source_operation_ = 0;
  int last_operation_ = 0;

  // The bridge between the content view and the drag drop client.
  raw_ptr<remote_cocoa::NativeWidgetNSWindowBridge, DanglingUntriaged>
      bridge_;  // Weak. Owns |this|.

  // The closure for the drag and drop's run loop.
  base::OnceClosure quit_closure_;

  // Whether |this| is the source of current dragging session.
  bool is_drag_source_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_DRAG_DROP_CLIENT_MAC_H_
