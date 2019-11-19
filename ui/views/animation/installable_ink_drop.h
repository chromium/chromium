// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_event_handler.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/installable_ink_drop_animator.h"
#include "ui/views/animation/installable_ink_drop_config.h"
#include "ui/views/animation/installable_ink_drop_painter.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace gfx {
class AnimationContainer;
class Size;
}  // namespace gfx

namespace ui {
class Layer;
class PaintContext;
}  // namespace ui

namespace views {

class InkDropHostView;
class View;

extern const VIEWS_EXPORT base::Feature kInstallableInkDropFeature;

// Stub for future InkDrop implementation that will be installable on any View
// without needing InkDropHostView. This is currently non-functional and fails
// on some method calls. TODO(crbug.com/931964): implement the necessary parts
// of the API and remove the rest from the InkDrop interface.
class VIEWS_EXPORT InstallableInkDrop : public InkDrop,
                                        public InkDropEventHandler::Delegate,
                                        public ui::LayerDelegate,
                                        public ViewObserver {
 public:
  // Create ink drop for |view|. Note that |view| must live longer than us.
  explicit InstallableInkDrop(View* view);

  // Overload for working within the InkDropHostView hierarchy. Similar to
  // above, |ink_drop_host_view| must outlive us.
  //
  // TODO(crbug.com/931964): Remove this.
  explicit InstallableInkDrop(InkDropHostView* ink_drop_host_view);

  InstallableInkDrop(const InstallableInkDrop&) = delete;
  InstallableInkDrop(InstallableInkDrop&&) = delete;

  ~InstallableInkDrop() override;

  void SetConfig(InstallableInkDropConfig config);
  InstallableInkDropConfig config() const { return config_; }

  // Should only be used for inspecting properties of the layer in tests.
  const ui::Layer* layer_for_testing() const { return layer_.get(); }

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SetHoverHighlightFadeDuration(base::TimeDelta duration) override;
  void UseDefaultHoverHighlightFadeDuration() override;
  void SnapToActivated() override;
  void SnapToHidden() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_focused) override;
  bool IsHighlightFadingInOrVisible() const override;
  void SetShowHighlightOnHover(bool show_highlight_on_hover) override;
  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override;

  // InkDropEventHandler::Delegate:
  InkDrop* GetInkDrop() override;
  bool HasInkDrop() const override;
  bool SupportsGestureEvents() const override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  void SchedulePaint();
  void UpdateAnimatorHighlight();

  // The view this ink drop is showing for. |layer_| is added to the layer
  // hierarchy that |view_| belongs to. We track events on |view_| to update our
  // visual state.
  View* const view_;

  // If we were installed on an InkDropHostView, this will be non-null. We store
  // this to to remove our InkDropEventHandler override.
  InkDropHostView* ink_drop_host_view_ = nullptr;

  // Contains the colors and opacities used to paint.
  InstallableInkDropConfig config_;

  // The layer we paint to.
  std::unique_ptr<ui::Layer> layer_;

  // Observes |view_| and updates our visual state accordingly.
  InkDropEventHandler event_handler_;

  // Completely describes the current visual state of the ink drop, including
  // progress of animations.
  InstallableInkDropPainter::State visual_state_;

  // Handles painting |visual_state_| on request.
  InstallableInkDropPainter painter_;

  // Used to synchronize the hover and activation animations within this ink
  // drop. Since we use |views::CompositorAnimationRunner|, this also
  // synchronizes them with compositor frames.
  scoped_refptr<gfx::AnimationContainer> animation_container_;

  // Manages our animations and maniuplates |visual_state_| for us.
  InstallableInkDropAnimator animator_;

  bool is_hovered_ = false;
  bool is_focused_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
