// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUS_RING_H_
#define UI_VIEWS_CONTROLS_FOCUS_RING_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class HighlightPathGenerator;

// FocusRing is a View that is designed to act as an indicator of focus for its
// parent. It is a view that paints to a layer which extends beyond the bounds
// of its parent view.
// If MyView should show a rounded rectangular focus ring when it has focus and
// hide the ring when it loses focus, no other configuration is necessary. In
// other cases, it might be necessary to use the Set*() functions on FocusRing;
// these take care of repainting it when the state changes.
// TODO(tluk): FocusRing should not be a view but instead a new concept which
// only participates in view painting ( https://crbug.com/840796 ).
class VIEWS_EXPORT FocusRing : public View, public ViewObserver {
  METADATA_HEADER(FocusRing, View)

 public:
  static constexpr float kDefaultCornerRadiusDp = 2.0f;

  using ViewPredicate = base::RepeatingCallback<bool(const View* view)>;

  // The default thickness and inset amount of focus ring halos. If you need
  // the thickness of a specific focus ring, call halo_thickness() instead.
  static constexpr float kDefaultHaloThickness = 2.0f;

  // The default inset for the focus ring. Moves the ring slightly out from the
  // edge of the host view, so that the halo doesn't significantly overlap the
  // host view's contents. If you need a value for a specific focus ring, call
  // halo_inset() instead.
  static constexpr float kDefaultHaloInset = kDefaultHaloThickness * -0.5f;

  // Creates a FocusRing and adds it to `host`.
  static void Install(View* host);

  // Gets the FocusRing, if present, from `host`.
  static FocusRing* Get(View* host);
  static const FocusRing* Get(const View* host);

  // Removes the FocusRing, if present, from `host`.
  static void Remove(View* host);

  FocusRing(const FocusRing&) = delete;
  FocusRing& operator=(const FocusRing&) = delete;

  ~FocusRing() override;

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

  std::optional<ui::ColorId> GetColorId() const;
  void SetColorId(std::optional<ui::ColorId> color_id);

  float GetHaloThickness() const;
  float GetHaloInset() const;
  void SetHaloThickness(float halo_thickness);
  void SetHaloInset(float halo_inset);

  // Explicitly disable using style of focus ring that is drawn with a 2dp gap
  // between the focus ring and component.
  void SetOutsetFocusRingDisabled(bool disable);
  bool GetOutsetFocusRingDisabled() const;

  bool ShouldPaintForTesting();

  // View:
  void Layout(PassKey) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // ViewObserver:
  void OnViewFocused(View* view) override;
  void OnViewBlurred(View* view) override;
  void OnViewLayoutInvalidated(View* view) override;

 private:
  FocusRing();

  // Outset the input bounds if conditions are met.
  void AdjustBounds(SkRect& rect) const;
  void AdjustBounds(SkRRect& rect) const;

  SkPath GetPath() const;
  SkRRect GetRingRoundRect() const;

  void RefreshLayer();

  // Returns whether we should outset by `kFocusRingOutset` dp before drawing
  // the focus ring.
  bool ShouldSetOutsetFocusRing() const;

  bool ShouldPaint();

  // Translates the provided SkRect or SkRRect, which is in the parent's
  // coordinate system, into this view's coordinate system, then insets it
  // appropriately to produce the focus ring "halo" effect. If the supplied rect
  // is an SkRect, it will have the default focus ring corner radius applied as
  // well.
  SkRRect RingRectFromPathRect(const SkRect& rect) const;
  SkRRect RingRectFromPathRect(const SkRRect& rect) const;

  // The path generator used to draw this focus ring.
  std::unique_ptr<HighlightPathGenerator> path_generator_;

  bool outset_focus_ring_disabled_ = false;

  // Whether the enclosed View is in an invalid state, which controls whether
  // the focus ring shows an invalid appearance (usually a different color).
  bool invalid_ = false;

  // Overriding color_id for the focus ring.
  std::optional<ui::ColorId> color_id_;

  // The predicate used to determine whether the parent has focus.
  ViewPredicate has_focus_predicate_;

  // The thickness of the focus ring halo, in DIP.
  float halo_thickness_ = kDefaultHaloThickness;

  // The adjustment from the visible border of the host view to render the
  // focus ring.
  //
  // At -0.5 * halo_thickness_ (the default), the inner edge of the focus
  // ring will align with the outer edge of the default inkdrop. For very thin
  // focus rings, a zero value may provide better visual results.
  float halo_inset_ = kDefaultHaloInset;

  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

VIEWS_EXPORT SkPath
GetHighlightPath(const View* view,
                 float halo_thickness = FocusRing::kDefaultHaloThickness);

// Set this on the FocusRing host to have the FocusRing paint an outline around
// itself. This ensures that the FocusRing has sufficient contrast with its
// surroundings (this is used for prominent MdTextButtons because they are blue,
// while the background is light/dark, and the FocusRing doesn't contrast well
// with both the interior and exterior of the button). This may need some polish
// (such as blur?) in order to be expandable to all controls. For now it solves
// color contrast on prominent buttons which is an a11y issue. See
// https://crbug.com/1197631.
// TODO(pbos): Consider polishing this well enough that this can be
// unconditional. This may require rolling out `kCascadingBackgroundColor` to
// more surfaces to have an accurate background color.
VIEWS_EXPORT extern const ui::ClassProperty<bool>* const
    kDrawFocusRingBackgroundOutline;

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
