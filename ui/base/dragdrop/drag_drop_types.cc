// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_drop_types.h"

#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace ui {

using mojom::DragOperation;

// Ensure that the DragDropTypes::DragOperation enum values stay in sync with
// mojom::DragOperation.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "enum mismatch: " #a)
STATIC_ASSERT_ENUM(DragDropTypes::DRAG_NONE, DragOperation::kNone);
STATIC_ASSERT_ENUM(DragDropTypes::DRAG_COPY, DragOperation::kCopy);
STATIC_ASSERT_ENUM(DragDropTypes::DRAG_LINK, DragOperation::kLink);
STATIC_ASSERT_ENUM(DragDropTypes::DRAG_MOVE, DragOperation::kMove);

DragOperation PreferredDragOperation(int operations) {
  if (operations & DragDropTypes::DRAG_COPY)
    return DragOperation::kCopy;
  if (operations & DragDropTypes::DRAG_MOVE)
    return DragOperation::kMove;
  if (operations & DragDropTypes::DRAG_LINK)
    return DragOperation::kLink;
  return DragOperation::kNone;
}

}  // namespace ui
