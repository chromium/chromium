// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TRANSFORM_RECORDER_H_
#define UI_COMPOSITOR_TRANSFORM_RECORDER_H_

#include "base/memory/raw_ref.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class DisplayItemList;
}

namespace gfx {
class Transform;
}

namespace ui {
class PaintContext;

// A class to provide scoped transforms of painting to a DisplayItemList. The
// transform provided will be applied to any DisplayItems added to the
// DisplayItemList while this object is alive. In other words, any nested
// recorders will be transformed.
class COMPOSITOR_EXPORT TransformRecorder {
 public:
  explicit TransformRecorder(const PaintContext& context);

  TransformRecorder(const TransformRecorder&) = delete;
  TransformRecorder& operator=(const TransformRecorder&) = delete;

  ~TransformRecorder();

  void Transform(const gfx::Transform& transform);

 private:
  const raw_ref<const PaintContext> context_;
  bool transformed_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TRANSFORM_RECORDER_H_
