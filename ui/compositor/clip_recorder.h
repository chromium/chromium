// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLIP_RECORDER_H_
#define UI_COMPOSITOR_CLIP_RECORDER_H_

#include "base/memory/raw_ref.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

class SkPath;

namespace cc {
class DisplayItemList;
}

namespace ui {
class PaintContext;

// A class to provide scoped clips of painting to a DisplayItemList. The clip
// provided will be applied to any DisplayItems added to the DisplayItemList
// while this object is alive. In other words, any nested recorders will be
// clipped.
class COMPOSITOR_EXPORT ClipRecorder {
 public:
  explicit ClipRecorder(const PaintContext& context);

  ClipRecorder(const ClipRecorder&) = delete;
  ClipRecorder& operator=(const ClipRecorder&) = delete;

  ~ClipRecorder();

  void ClipRect(const gfx::Rect& clip_rect);
  void ClipPath(const SkPath& clip_path);
  void ClipPathWithAntiAliasing(const SkPath& clip_path);

 private:
  const raw_ref<const PaintContext> context_;
  int num_closers_ = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_RECORDER_H_
