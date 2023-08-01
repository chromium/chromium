// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_drop_types.h"

#import <AppKit/AppKit.h>

namespace ui {

uint64_t DragDropTypes::DragOperationToNSDragOperation(int drag_operation) {
  NSUInteger ns_drag_operation = NSDragOperationNone;

  if (drag_operation & DRAG_LINK)
    ns_drag_operation |= NSDragOperationLink;
  if (drag_operation & DRAG_COPY)
    ns_drag_operation |= NSDragOperationCopy;
  if (drag_operation & DRAG_MOVE)
    ns_drag_operation |= NSDragOperationMove;

  return ns_drag_operation;
}

int DragDropTypes::NSDragOperationToDragOperation(uint64_t ns_drag_operation) {
  NSUInteger drag_operation = DRAG_NONE;

  if (ns_drag_operation & NSDragOperationLink)
    drag_operation |= DRAG_LINK;
  if (ns_drag_operation & NSDragOperationCopy)
    drag_operation |= DRAG_COPY;
  if (ns_drag_operation & NSDragOperationMove)
    drag_operation |= DRAG_MOVE;

  return drag_operation;
}

}  // namespace ui
