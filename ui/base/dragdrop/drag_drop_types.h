// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DRAG_DROP_TYPES_H_
#define UI_BASE_DRAGDROP_DRAG_DROP_TYPES_H_

#include <stdint.h>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_DRAGDROP_TYPES) DragDropTypes {
 public:
  // These constants match their equivalents in NSDragOperation and
  // should not be renumbered.
  // TODO(crbug.com/40135065): replace this enum with
  // ui::mojom::DragOperation.
  enum DragOperation {
    DRAG_NONE = 0,
    DRAG_COPY = 1,
    DRAG_LINK = 2,
    DRAG_MOVE = 16,
  };

#if BUILDFLAG(IS_WIN)
  static uint32_t DragOperationToDropEffect(int drag_operation);
  static int DropEffectToDragOperation(uint32_t effect);
#endif

#if BUILDFLAG(IS_APPLE)
  static uint64_t DragOperationToNSDragOperation(int drag_operation);
  static int NSDragOperationToDragOperation(uint64_t ns_drag_operation);
#endif
};

COMPONENT_EXPORT(UI_BASE_DRAGDROP_TYPES)
ui::mojom::DragOperation PreferredDragOperation(int operations);

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DRAG_DROP_TYPES_H_
