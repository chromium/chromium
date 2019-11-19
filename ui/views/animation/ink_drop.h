// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INK_DROP_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class InkDropObserver;

// Base class that manages the lifetime and state of an ink drop ripple as
// well as visual hover state feedback.
class VIEWS_EXPORT InkDrop {
 public:
  virtual ~InkDrop();

  // Called by ink drop hosts when their size is changed.
  virtual void HostSizeChanged(const gfx::Size& new_size) = 0;

  // Gets the target state of the ink drop.
  virtual InkDropState GetTargetInkDropState() const = 0;

  // Animates from the current InkDropState to |ink_drop_state|.
  virtual void AnimateToState(InkDropState ink_drop_state) = 0;

  // Sets hover highlight fade animations to last for |duration|.
  virtual void SetHoverHighlightFadeDuration(base::TimeDelta duration) = 0;

  // Clears any set hover highlight fade durations and uses the default
  // durations instead.
  virtual void UseDefaultHoverHighlightFadeDuration() = 0;

  // Immediately snaps the InkDropState to ACTIVATED and HIDDEN specifically.
  // These are more specific implementations of the non-existent
  // SnapToState(InkDropState) function are the only ones available because they
  // were the only InkDropState that clients needed to skip animations for.
  virtual void SnapToActivated() = 0;
  virtual void SnapToHidden() = 0;

  // Enables or disables the hover state.
  virtual void SetHovered(bool is_hovered) = 0;

  // Enables or disables the focus state.
  virtual void SetFocused(bool is_focused) = 0;

  // Returns true if the highlight animation is in the process of fading in or
  // is visible.
  virtual bool IsHighlightFadingInOrVisible() const = 0;

  // Enables or disables the highlight when the target is hovered.
  virtual void SetShowHighlightOnHover(bool show_highlight_on_hover) = 0;

  // Enables or disables the highlight when the target is focused.
  virtual void SetShowHighlightOnFocus(bool show_highlight_on_focus) = 0;

  // Methods to add/remove observers for this object.
  void AddObserver(InkDropObserver* observer);
  void RemoveObserver(InkDropObserver* observer);

 protected:
  InkDrop();

  // Notifes all of the observers that the animation has started.
  void NotifyInkDropAnimationStarted();

  // Notifies all of the observers that an animation to a state has ended.
  void NotifyInkDropRippleAnimationEnded(InkDropState state);

 private:
  base::ObserverList<InkDropObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(InkDrop);
};

// A View which can be used to parent ink drop layers. Typically this is used
// as a non-ancestor view to labels so that the labels can paint on an opaque
// canvas. This is used to avoid ugly text renderings when labels with subpixel
// rendering enabled are painted onto a non-opaque canvas.
class VIEWS_EXPORT InkDropContainerView : public views::View {
 public:
  InkDropContainerView();

  void AddInkDropLayer(ui::Layer* ink_drop_layer);
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer);

  // View:
  bool CanProcessEventsWithinSubtree() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropContainerView);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_H_
