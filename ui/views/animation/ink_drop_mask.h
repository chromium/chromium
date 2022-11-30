// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_MASK_H_
#define UI_VIEWS_ANIMATION_INK_DROP_MASK_H_

#include "base/gtest_prod_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/views_export.h"

class SkPath;

namespace views {

// Base class for different ink drop masks. It is responsible for creating the
// ui::Layer that can be set as the mask layer for ink drop layer. Note that the
// mask's layer size (passed in the constructor) should always match size of the
// layer it is masking.
class VIEWS_EXPORT InkDropMask : public ui::LayerDelegate {
 public:
  InkDropMask(const InkDropMask&) = delete;
  InkDropMask& operator=(const InkDropMask&) = delete;

  ~InkDropMask() override;

  ui::Layer* layer() { return &layer_; }

 protected:
  explicit InkDropMask(const gfx::Size& layer_size);

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  ui::Layer layer_;
};

// An ink-drop mask that paints a specified path.
class VIEWS_EXPORT PathInkDropMask : public InkDropMask {
 public:
  PathInkDropMask(const gfx::Size& layer_size, const SkPath& path);

  PathInkDropMask(const PathInkDropMask&) = delete;
  PathInkDropMask& operator=(const PathInkDropMask&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(InkDropMaskTest, PathInkDropMaskPaintsTriangle);

  // InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  SkPath path_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_MASK_H_
