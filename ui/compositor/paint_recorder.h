// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_RECORDER_H_
#define UI_COMPOSITOR_PAINT_RECORDER_H_

#include <memory>

#include "base/macros.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Canvas;
}

namespace ui {
class PaintCache;
class PaintContext;

// A class to hide the complexity behind setting up a recording into a
// DisplayItem. This is meant to be short-lived within the scope of recording
// taking place, the DisplayItem should be removed from the PaintRecorder once
// recording is complete and can be cached.
class COMPOSITOR_EXPORT PaintRecorder {
 public:
  // The |cache| is owned by the caller and must be kept alive while
  // PaintRecorder is in use. Canvas is bounded by |recording_size|.
  PaintRecorder(const PaintContext& context,
                const gfx::Size& recording_size,
                float recording_scale_x,
                float recording_scale_y,
                PaintCache* cache);
  PaintRecorder(const PaintContext& context, const gfx::Size& recording_size);
  ~PaintRecorder();

  // Gets a gfx::Canvas for painting into.
  gfx::Canvas* canvas() { return &canvas_; }

 private:
  const PaintContext& context_;
  scoped_refptr<cc::DisplayItemList> local_list_;
  cc::RecordPaintCanvas record_canvas_;
  gfx::Canvas canvas_;
  PaintCache* cache_;
  gfx::Size recording_size_;

  DISALLOW_COPY_AND_ASSIGN(PaintRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_RECORDER_H_
