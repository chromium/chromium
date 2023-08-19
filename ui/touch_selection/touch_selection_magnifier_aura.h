// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_

#include <memory>

#include "ui/touch_selection/ui_touch_selection_export.h"

namespace gfx {
class Rect;
class Point;
}  // namespace gfx

namespace ui {
class Layer;

// A magnifier which shows the text caret or selection endpoint during a touch
// selection session.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionMagnifierAura {
 public:
  TouchSelectionMagnifierAura();

  TouchSelectionMagnifierAura(const TouchSelectionMagnifierAura&) = delete;
  TouchSelectionMagnifierAura& operator=(const TouchSelectionMagnifierAura&) =
      delete;

  ~TouchSelectionMagnifierAura();

  // Shows the magnifier at the focus bound. Roughly, this is a line segment
  // representing a caret position or selection endpoint and is generally
  // vertical or horizontal (depending on text orientation). E.g. for a caret in
  // horizontal text, `focus_start` is the top of the caret and `focus_end` is
  // the bottom of the caret. These are specified in coordinates of the
  // `parent` layer which the magnifier should be attached to.
  void ShowFocusBound(Layer* parent,
                      const gfx::Point& focus_start,
                      const gfx::Point& focus_end);

  // Returns the bounds of the magnified area, in coordinates of the magnifier's
  // parent layer.
  gfx::Rect GetMagnifiedAreaBoundsForTesting() const;

  const Layer* GetMagnifierParentForTesting() const;

 private:
  class BorderRenderer;

  void CreateMagnifierLayer();

  // The magnifier layer is the parent of the zoom layer and border layer. The
  // layer bounds should be updated when selection updates occur.
  std::unique_ptr<Layer> magnifier_layer_;

  // Draws the magnified area, i.e. the background with a zoom and offset filter
  // applied.
  std::unique_ptr<Layer> zoom_layer_;

  // Draws the magnifier border and shadows. `border_layer_` must be ordered
  // after `border_renderer_` so that it is destroyed before `border_renderer_`.
  // Otherwise `border_layer_` will have a pointer to a deleted delegate.
  std::unique_ptr<BorderRenderer> border_renderer_;
  std::unique_ptr<Layer> border_layer_;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_
