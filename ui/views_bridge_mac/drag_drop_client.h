// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_DRAG_DROP_CLIENT_H_
#define UI_VIEWS_BRIDGE_MAC_DRAG_DROP_CLIENT_H_

#import <Cocoa/Cocoa.h>

#include "ui/views_bridge_mac/views_bridge_mac_export.h"

namespace views_bridge_mac {

// Interface between the content view of a BridgedNativeWidgetImpl and a
// DragDropClientMac in the browser process. This interface should eventually
// become mojo-ified, but at the moment only passes raw pointers (consequently,
// drag-drop behavior does not work in RemoteMacViews).
class VIEWS_BRIDGE_MAC_EXPORT DragDropClient {
 public:
  virtual ~DragDropClient() {}

  // Called when mouse is dragged during a drag and drop.
  virtual NSDragOperation DragUpdate(id<NSDraggingInfo>) = 0;

  // Called when mouse is released during a drag and drop.
  virtual NSDragOperation Drop(id<NSDraggingInfo> sender) = 0;

  // Called when the drag and drop session has ended.
  virtual void EndDrag() = 0;

  // Called when mouse leaves the drop area.
  virtual void DragExit() = 0;
};

}  // namespace views_bridge_mac

#endif  // UI_VIEWS_BRIDGE_MAC_DRAG_DROP_CLIENT_H_
