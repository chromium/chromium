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
  static const char kViewClassName[];

  using ViewPredicate = std::function<bool(View* view)>;

  ~FocusRing() override;

  // Create a FocusRing and adds it to |parent|. The returned focus ring is
  // owned by the client (the code calling FocusRing::Install), *not* by
  // |parent|.
  static std::unique_ptr<FocusRing> Install(View* parent);

  // Returns whether this class can draw a focus ring from |path|. Not all paths
  // are useable since not all paths can be easily outset. If a FocusRing is
  // configured to use an unuseable path, it will fall back to the default focus
  // ring path.
  static bool IsPathUseable(const SkPath& path);

  // Sets the path to draw this FocusRing around. This path is in the parent
  // view's coordinate system, *not* in the FocusRing's coordinate system. Note
  // that this path will not be mirrored in RTL, so your View's computation of
  // it should take RTL into account.
  // Note: This method should only be used if the focus ring needs to differ
  // from the highlight shape used for inkdrops. Otherwise set kHighlightPathKey
  // on the parent and FocusRing will use it as well.
  void SetPath(const SkPath& path);

  // Sets whether the FocusRing should show an invalid state for the View it
  // encloses.
  void SetInvalid(bool invalid);

  // Sets the predicate function used to tell when the parent has focus. The
  // parent is passed into this predicate; it should return whether the parent
  // should be treated as focused. This is useful when, for example, the parent
  // wraps an inner view and the inner view is the one that actually receives
  // focus, but the FocusRing sits on the parent instead of the inner view.
  void SetHasFocusPredicate(const ViewPredicate& predicate);

  // View:
  const char* GetClassName() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ViewObserver:
  void OnViewFocused(View* view) override;
  void OnViewBlurred(View* view) override;

 private:
  explicit FocusRing(View* parent);

  // Translates the provided SkRect or SkRRect, which is in the parent's
  // coordinate system, into this view's coordinate system, then insets it
  // appropriately to produce the focus ring "halo" effect. If the supplied rect
  // is an SkRect, it will have the default focus ring corner radius applied as
  // well.
  SkRRect RingRectFromPathRect(const SkRect& rect) const;
  SkRRect RingRectFromPathRect(const SkRRect& rect) const;

  // The View this focus ring is installed on.
  View* view_ = nullptr;

  // The path to draw this focus ring around. IsPathUseable(path_) is always
  // true.
  SkPath path_;

  // Whether the enclosed View is in an invalid state, which controls whether
  // the focus ring shows an invalid appearance (usually a different color).
  bool invalid_ = false;

  // The predicate used to determine whether the parent has focus.
  ViewPredicate has_focus_predicate_;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

}  // views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
