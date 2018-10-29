// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {
class Layer;
class LocatedEvent;
}  // namespace ui

namespace views {

class InkDrop;
class InkDropHighlight;
class InkDropImpl;
class InkDropMask;
class InkDropRipple;
enum class InkDropState;

namespace test {
class InkDropHostViewTestApi;
}  // namespace test

// A view that provides InkDropHost functionality.
class VIEWS_EXPORT InkDropHostView : public View {
 public:
  // Used in SetInkDropMode() to specify whether the ink drop effect is enabled
  // or not for the view. In case of having an ink drop, it also specifies
  // whether the default gesture event handler for the ink drop should be
  // installed or the subclass will handle gesture events itself.
  enum class InkDropMode {
    OFF,
    ON,
    ON_NO_GESTURE_HANDLER,
  };

  InkDropHostView();
  ~InkDropHostView() override;

  // Adds the |ink_drop_layer| in to a visible layer tree.
  virtual void AddInkDropLayer(ui::Layer* ink_drop_layer);

  // Removes |ink_drop_layer| from the layer tree.
  virtual void RemoveInkDropLayer(ui::Layer* ink_drop_layer);

  // Returns a configured InkDrop. In general subclasses will return an
  // InkDropImpl instance that will use the CreateInkDropRipple() and
  // CreateInkDropHighlight() methods to create the visual effects.
  //
  // Subclasses should override this if they need to configure any properties
  // specific to the InkDrop instance. e.g. the AutoHighlightMode of an
  // InkDropImpl instance.
  virtual std::unique_ptr<InkDrop> CreateInkDrop();

  // Creates and returns the visual effect used for press. Used by InkDropImpl
  // instances.
  virtual std::unique_ptr<InkDropRipple> CreateInkDropRipple() const;

  // Creates and returns the visual effect used for hover and focus. Used by
  // InkDropImpl instances.
  virtual std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const;

  // Subclasses can override to return a mask for the ink drop. By default,
  // returns nullptr (i.e no mask).
  // TODO(bruthig): InkDropMasks do not currently work on Windows. See
  // https://crbug.com/713359.
  virtual std::unique_ptr<views::InkDropMask> CreateInkDropMask() const;

  // Returns the base color for the ink drop.
  virtual SkColor GetInkDropBaseColor() const;

  // Toggle to enable/disable an InkDrop on this View.  Descendants can override
  // CreateInkDropHighlight() and CreateInkDropRipple() to change the look/feel
  // of the InkDrop.
  //
  // TODO(bruthig): Add an easier mechanism than overriding functions to allow
  // subclasses/clients to specify the flavor of ink drop.
  void SetInkDropMode(InkDropMode ink_drop_mode);

  void set_ink_drop_visible_opacity(float visible_opacity) {
    ink_drop_visible_opacity_ = visible_opacity;
  }
  float ink_drop_visible_opacity() const { return ink_drop_visible_opacity_; }

  void set_ink_drop_corner_radii(int small_radius, int large_radius) {
    ink_drop_small_corner_radius_ = small_radius;
    ink_drop_large_corner_radius_ = large_radius;
  }
  int ink_drop_small_corner_radius() const {
    return ink_drop_small_corner_radius_;
  }
  int ink_drop_large_corner_radius() const {
    return ink_drop_large_corner_radius_;
  }

  // Animates |ink_drop_| to the desired |ink_drop_state|. Caches |event| as the
  // last_ripple_triggering_event().
  //
  // *** NOTE ***: |event| has been plumbed through on a best effort basis for
  // the purposes of centering ink drop ripples on located Events.  Thus nullptr
  // has been used by clients who do not have an Event instance available to
  // them.
  void AnimateInkDrop(InkDropState state, const ui::LocatedEvent* event);

 protected:
  // Size used for the default SquareInkDropRipple.
  static constexpr int kDefaultInkDropSize = 24;

  // Called after a new InkDrop instance is created.
  virtual void OnInkDropCreated() {}

  // View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Returns an InkDropImpl with default configuration. The base implementation
  // of CreateInkDrop() delegates to this function.
  std::unique_ptr<InkDropImpl> CreateDefaultInkDropImpl();

  // Returns an InkDropImpl configured to work well with a
  // flood-fill ink drop ripple.
  std::unique_ptr<InkDropImpl> CreateDefaultFloodFillInkDropImpl();

  // Returns the default InkDropRipple centered on |center_point|.
  std::unique_ptr<InkDropRipple> CreateDefaultInkDropRipple(
      const gfx::Point& center_point,
      const gfx::Size& size = gfx::Size(kDefaultInkDropSize,
                                        kDefaultInkDropSize)) const;

  // Returns the default InkDropHighlight centered on |center_point|.
  std::unique_ptr<InkDropHighlight> CreateDefaultInkDropHighlight(
      const gfx::PointF& center_point,
      const gfx::Size& size = gfx::Size(kDefaultInkDropSize,
                                        kDefaultInkDropSize)) const;

  // Returns true if an ink drop instance has been created.
  bool HasInkDrop() const;

  // Provides access to |ink_drop_|. Implements lazy initialization of
  // |ink_drop_| so as to avoid virtual method calls during construction since
  // subclasses should be able to call SetInkDropMode() during construction.
  InkDrop* GetInkDrop();

  // Returns the point of the |last_ripple_triggering_event_| if it was a
  // LocatedEvent, otherwise the center point of the local bounds is returned.
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Initializes and sets a mask on |ink_drop_layer|. No-op if
  // CreateInkDropMask() returns null.
  void InstallInkDropMask(ui::Layer* ink_drop_layer);

  void ResetInkDropMask();

  // Updates the ink drop mask layer size to |new_size|. It does nothing if
  // |ink_drop_mask_| is null.
  void UpdateInkDropMaskLayerSize(const gfx::Size& new_size);

  // Returns a large ink drop size based on the |small_size| that works well
  // with the SquareInkDropRipple animation durations.
  static gfx::Size CalculateLargeInkDropSize(const gfx::Size& small_size);

 private:
  class InkDropGestureHandler;
  friend class InkDropGestureHandler;
  friend class test::InkDropHostViewTestApi;

  // The last user Event to trigger an ink drop ripple animation.
  std::unique_ptr<ui::LocatedEvent> last_ripple_triggering_event_;

  // Defines what type of |ink_drop_| to create.
  InkDropMode ink_drop_mode_ = InkDropMode::OFF;

  // Should not be accessed directly. Use GetInkDrop() instead.
  std::unique_ptr<InkDrop> ink_drop_;

  // Intentionally declared after |ink_drop_| so that it doesn't access a
  // destroyed |ink_drop_| during destruction.
  std::unique_ptr<InkDropGestureHandler> gesture_handler_;

  float ink_drop_visible_opacity_ = 0.175f;

  // Radii used for the SquareInkDropRipple.
  int ink_drop_small_corner_radius_ = 2;
  int ink_drop_large_corner_radius_ = 4;

  // Determines whether the view was already painting to layer before adding ink
  // drop layer.
  bool old_paint_to_layer_ = false;

  bool destroying_ = false;

  std::unique_ptr<views::InkDropMask> ink_drop_mask_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostView);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
