// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUS_RING_H_
#define UI_VIEWS_CONTROLS_FOCUS_RING_H_

#include "base/scoped_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class HighlightPathGenerator;

// FocusRing is a View that is designed to act as an indicator of focus for its
// parent. It is a stand-alone view that paints to a layer which extends beyond
// the bounds of its parent view.
//
// Using FocusRing looks something like this:
//
//   class MyView : public View {
//     ...
//    private:
//     std::unique_ptr<FocusRing> focus_ring_;
//   };
//
//   MyView::MyView() {
//     focus_ring_ = FocusRing::Install(this);
//     ...
//   }
//
// If MyView should show a rounded rectangular focus ring when it has focus and
// hide the ring when it loses focus, no other configuration is necessary. In
// other cases, it might be necessary to use the Set*() functions on FocusRing;
// these take care of repainting it when the state changes.
class VIEWS_EXPORT FocusRing : public View, public ViewObserver {
 public:
  METADATA_HEADER(FocusRing);

  using ViewPredicate = std::function<bool(View* view)>;

  ~FocusRing() override;

  // Create a FocusRing and adds it to |parent|. The returned focus ring is
  // owned by the client (the code calling FocusRing::Install), *not* by
  // |parent|.
  static std::unique_ptr<FocusRing> Install(View* parent);

  // Sets the HighlightPathGenerator to draw this FocusRing around.
  // Note: This method should only be used if the focus ring needs to differ
  // from the highlight shape used for InkDrops.
  // Otherwise install a HighlightPathGenerator on the parent and FocusRing will
  // use it as well.
  void SetPathGenerator(std::unique_ptr<HighlightPathGenerator> generator);

  // Sets whether the FocusRing should show an invalid state for the View it
  // encloses.
  void SetInvalid(bool invalid);

  // Sets the predicate function used to tell when the parent has focus. The
  // parent is passed into this predicate; it should return whether the parent
  // should be treated as focused. This is useful when, for example, the parent
  // wraps an inner view and the inner view is the one that actually receives
  // focus, but the FocusRing sits on the parent instead of the inner view.
  void SetHasFocusPredicate(const ViewPredicate& predicate);

  void SetColor(base::Optional<SkColor> color);

  // View:
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ViewObserver:
  void OnViewFocused(View* view) override;
  void OnViewBlurred(View* view) override;

 private:
  FocusRing();

  void RefreshLayer();

  // Translates the provided SkRect or SkRRect, which is in the parent's
  // coordinate system, into this view's coordinate system, then insets it
  // appropriately to produce the focus ring "halo" effect. If the supplied rect
  // is an SkRect, it will have the default focus ring corner radius applied as
  // well.
  SkRRect RingRectFromPathRect(const SkRect& rect) const;
  SkRRect RingRectFromPathRect(const SkRRect& rect) const;

  // The path generator used to draw this focus ring.
  std::unique_ptr<HighlightPathGenerator> path_generator_;

  // Whether the enclosed View is in an invalid state, which controls whether
  // the focus ring shows an invalid appearance (usually a different color).
  bool invalid_ = false;

  // Overriding color for the focus ring.
  base::Optional<SkColor> color_;

  // The predicate used to determine whether the parent has focus.
  base::Optional<ViewPredicate> has_focus_predicate_;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

VIEWS_EXPORT SkPath GetHighlightPath(const View* view);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
