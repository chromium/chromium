// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_

#include <memory>

#include "base/scoped_observation.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace gfx {
class Rect;
class Point;
}  // namespace gfx

namespace ui {
class Layer;

// A magnifier which shows the text caret or selection endpoint during a touch
// selection session.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionMagnifierAura
    : public NativeThemeObserver {
 public:
  TouchSelectionMagnifierAura();

  TouchSelectionMagnifierAura(const TouchSelectionMagnifierAura&) = delete;
  TouchSelectionMagnifierAura& operator=(const TouchSelectionMagnifierAura&) =
      delete;

  ~TouchSelectionMagnifierAura() override;

  // Shows the magnifier at the focus bound. Roughly, this is a line segment
  // representing a caret position or selection endpoint and is generally
  // vertical or horizontal (depending on text orientation). E.g. for a caret in
  // horizontal text, `focus_start` is the top of the caret and `focus_end` is
  // the bottom of the caret. These are specified in coordinates of the
  // `parent` layer which the magnifier should be attached to.
  void ShowFocusBound(Layer* parent,
                      const gfx::Point& focus_start,
                      const gfx::Point& focus_end);

  // NativeThemeObserver:
  void OnNativeThemeUpdated(NativeTheme* observed_theme) override;

  // Returns the bounds of the zoomed contents in coordinates of the magnifier's
  // parent layer. This is the bounding box of the source pixels that will be
  // scaled and offset to fill the magnifier layer.
  gfx::Rect GetZoomedContentsBoundsForTesting() const;

  // Returns the bounds of the magnifier (i.e. where the zoomed content is drawn
  // to), ignoring border and style padding.
  gfx::Rect GetMagnifierBoundsForTesting() const;

  const Layer* GetMagnifierParentForTesting() const;

 private:
  class BorderRenderer;

  void CreateMagnifierLayer();

  // The magnifier layer is the parent of the zoom layer and border layer. The
  // layer bounds should be updated when selection updates occur.
  std::unique_ptr<Layer> magnifier_layer_;

  // Draws the zoomed contents, i.e. the background with a zoom and offset
  // filter applied.
  std::unique_ptr<Layer> zoom_layer_;

  // Draws the magnifier border and shadows. `border_layer_` must be ordered
  // after `border_renderer_` so that it is destroyed before `border_renderer_`.
  // Otherwise `border_layer_` will have a pointer to a deleted delegate.
  std::unique_ptr<BorderRenderer> border_renderer_;
  std::unique_ptr<Layer> border_layer_;

  base::ScopedObservation<NativeTheme, NativeThemeObserver> theme_observation_{
      this};
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_AURA_H_
