// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INK_DROP_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class InkDropHost;
class InkDropObserver;
class View;

// Base class that manages the lifetime and state of an ink drop ripple as
// well as visual hover state feedback.
class VIEWS_EXPORT InkDrop {
 public:
  InkDrop(const InkDrop&) = delete;
  InkDrop& operator=(const InkDrop&) = delete;
  virtual ~InkDrop();

  // TODO(pbos): Make sure what's installed here implements InkDrop so that can
  // be used as type instead of InkDropHost.
  static InkDropHost* Install(View* host,
                              std::unique_ptr<InkDropHost> ink_drop);

  // Removes the InkDrop from `host`.
  static void Remove(View* host);

  // TODO(pbos): Make sure what's installed here implements InkDrop so that can
  // be used as type instead of InkDropHost.
  static const InkDropHost* Get(const View* host);
  static InkDropHost* Get(View* host) {
    return const_cast<InkDropHost*>(Get(const_cast<const View*>(host)));
  }

  // Create an InkDrop appropriate for the "square" InkDropRipple effect. This
  // InkDrop hides when the ripple effect is active instead of layering
  // underneath it.
  static std::unique_ptr<InkDrop> CreateInkDropForSquareRipple(
      InkDropHost* host,
      bool highlight_on_hover = true,
      bool highlight_on_focus = false,
      bool show_highlight_on_ripple = false);

  // Configure `host` to use CreateInkDropForSquareRipple().
  static void UseInkDropForSquareRipple(InkDropHost* host,
                                        bool highlight_on_hover = true,
                                        bool highlight_on_focus = false,
                                        bool show_highlight_on_ripple = false);

  // Create an InkDrop appropriate for the "flood-fill" InkDropRipple effect.
  // This InkDrop shows as a response to the ripple effect.
  static std::unique_ptr<InkDrop> CreateInkDropForFloodFillRipple(
      InkDropHost* host,
      bool highlight_on_hover = true,
      bool highlight_on_focus = false,
      bool show_highlight_on_ripple = true);

  // Configure `host` to use CreateInkDropForFloodFillRipple().
  static void UseInkDropForFloodFillRipple(
      InkDropHost* host,
      bool highlight_on_hover = true,
      bool highlight_on_focus = false,
      bool show_highlight_on_ripple = true);

  // Create an InkDrop whose highlight does not react to its ripple.
  static std::unique_ptr<InkDrop> CreateInkDropWithoutAutoHighlight(
      InkDropHost* host,
      bool highlight_on_hover = true,
      bool highlight_on_focus = false);

  // Configure `host` to use CreateInkDropWithoutAutoHighlight().
  static void UseInkDropWithoutAutoHighlight(InkDropHost* host,
                                             bool highlight_on_hover = true,
                                             bool highlight_on_focus = false);

  // Called by ink drop hosts when their size is changed.
  virtual void HostSizeChanged(const gfx::Size& new_size) = 0;

  // Called by ink drop hosts when their theme is changed.
  virtual void HostViewThemeChanged() = 0;

  // Called by ink drop hosts when their transform is changed.
  virtual void HostTransformChanged(const gfx::Transform& new_transform) = 0;

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
  // SnapToState(InkDropState) function are the only ones available because
  // they were the only InkDropState that clients needed to skip animations
  // for.
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
};

// A View which can be used to parent ink drop layers. Typically this is used
// as a non-ancestor view to labels so that the labels can paint on an opaque
// canvas. This is used to avoid ugly text renderings when labels with subpixel
// rendering enabled are painted onto a non-opaque canvas.
// TODO(pbos): Replace with a function that returns unique_ptr<View>, this only
// calls SetProcessEventsWithinSubtree(false) right now.
class VIEWS_EXPORT InkDropContainerView : public View, public ViewObserver {
  METADATA_HEADER(InkDropContainerView, View)

 public:
  InkDropContainerView();
  ~InkDropContainerView() override;
  InkDropContainerView(const InkDropContainerView&) = delete;
  InkDropContainerView& operator=(const InkDropContainerView&) = delete;

  bool GetAutoMatchParentBounds() const;
  void SetAutoMatchParentBounds(bool auto_match_parent_bounds);

 private:
  // View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

  // TODO (crbug.com/345627615): Make this value true by default or remove
  // entirely, making this behavior intrinsic.
  bool auto_match_parent_bounds_ = false;
  base::ScopedObservation<View, ViewObserver> observer_{this};
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, InkDropContainerView, View)
VIEW_BUILDER_PROPERTY(bool, AutoMatchParentBounds)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, InkDropContainerView)

#endif  // UI_VIEWS_ANIMATION_INK_DROP_H_
