// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_event_handler.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

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
  METADATA_HEADER(InkDropHostView);

  // Used in SetInkDropMode() to specify whether the ink drop effect is enabled
  // or not for the view. In case of having an ink drop, it also specifies
  // whether the default event handler for the ink drop should be installed or
  // the subclass will handle ink drop events itself.
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
  // this generates a mask based on HighlightPathGenerator.
  // TODO(pbos): Replace overrides with HighlightPathGenerator usage and remove
  // this function.
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

  void SetInkDropVisibleOpacity(float visible_opacity);
  float GetInkDropVisibleOpacity() const;

  void SetInkDropHighlightOpacity(base::Optional<float> opacity);
  base::Optional<float> GetInkDropHighlightOpacity() const;

  void SetInkDropSmallCornerRadius(int small_radius);
  int GetInkDropSmallCornerRadius() const;

  void SetInkDropLargeCornerRadius(int large_radius);
  int GetInkDropLargeCornerRadius() const;

  // Allows InstallableInkDrop to override our InkDropEventHandler
  // instance.
  //
  // TODO(crbug.com/931964): Remove this, either by finishing refactor or by
  // giving up.
  void set_ink_drop_event_handler_override(
      InkDropEventHandler* ink_drop_event_handler_override) {
    ink_drop_event_handler_override_ = ink_drop_event_handler_override;
  }

  // Animates |ink_drop_| to the desired |ink_drop_state|. Caches |event| as the
  // last_ripple_triggering_event().
  //
  // *** NOTE ***: |event| has been plumbed through on a best effort basis for
  // the purposes of centering ink drop ripples on located Events.  Thus nullptr
  // has been used by clients who do not have an Event instance available to
  // them.
  void AnimateInkDrop(InkDropState state, const ui::LocatedEvent* event);

  // Provides public access to |ink_drop_| so that factory methods can configure
  // the inkdrop. Implements lazy initialization of |ink_drop_| so as to avoid
  // virtual method calls during construction since subclasses should be able to
  // call SetInkDropMode() during construction.
  //
  // WARNING: please don't override this; this is only virtual for the
  // InstallableInkDrop refactor. TODO(crbug.com/931964): make non-virtual when
  // this isn't necessary anymore.
  virtual InkDrop* GetInkDrop();

  // Returns whether the ink drop should be considered "highlighted" (in or
  // animating into "highlight visible" steady state).
  bool GetHighlighted() const;

  PropertyChangedSubscription AddHighlightedChangedCallback(
      PropertyChangedCallback callback);

  // Should be called by InkDrop implementations when their highlight state
  // changes, to trigger the corresponding property change notification here.
  void OnInkDropHighlightedChanged();

 protected:
  // Size used for the default SquareInkDropRipple.
  static constexpr gfx::Size kDefaultInkDropSize = gfx::Size(24, 24);

  // Called after a new InkDrop instance is created.
  virtual void OnInkDropCreated() {}

  // Returns an InkDropImpl suitable for use with a square ink drop.
  // TODO(pbos): Rename to CreateDefaultSquareInkDropImpl.
  std::unique_ptr<InkDropImpl> CreateDefaultInkDropImpl();

  // Returns an InkDropImpl configured to work well with a flood-fill ink drop
  // ripple.
  std::unique_ptr<InkDropImpl> CreateDefaultFloodFillInkDropImpl();

  // TODO(pbos): Migrate uses to CreateSquareInkDropRipple which this calls
  // directly.
  std::unique_ptr<InkDropRipple> CreateDefaultInkDropRipple(
      const gfx::Point& center_point,
      const gfx::Size& size = kDefaultInkDropSize) const;

  // Creates a SquareInkDropRipple centered on |center_point|.
  std::unique_ptr<InkDropRipple> CreateSquareInkDropRipple(
      const gfx::Point& center_point,
      const gfx::Size& size) const;

  // Returns true if an ink drop instance has been created.
  bool HasInkDrop() const;

  // Returns the point of the |last_ripple_triggering_event_| if it was a
  // LocatedEvent, otherwise the center point of the local bounds is returned.
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Initializes and sets a mask on |ink_drop_layer|. No-op if
  // CreateInkDropMask() returns null. This will not run if |AddInkDropClip()|
  // succeeds in the default implementation of |AddInkDropLayer()|.
  void InstallInkDropMask(ui::Layer* ink_drop_layer);

  void ResetInkDropMask();

  // Adds a clip rect on the root layer of the ink drop impl. This is a more
  // performant alternative to using circles or rectangle mask layers. Returns
  // true if a clip was added.
  bool AddInkDropClip(ui::Layer* ink_drop_layer);

  // Returns a large ink drop size based on the |small_size| that works well
  // with the SquareInkDropRipple animation durations.
  static gfx::Size CalculateLargeInkDropSize(const gfx::Size& small_size);

  // View:
  void OnLayerTransformed(const gfx::Transform& old_transform,
                          ui::PropertyChangeReason reason) override;

 private:
  friend class test::InkDropHostViewTestApi;

  class InkDropHostViewEventHandlerDelegate
      : public InkDropEventHandler::Delegate {
   public:
    explicit InkDropHostViewEventHandlerDelegate(InkDropHostView* host_view);

    // InkDropEventHandler:
    InkDrop* GetInkDrop() override;
    bool HasInkDrop() const override;

    bool SupportsGestureEvents() const override;

   private:
    // The host view.
    InkDropHostView* const host_view_;
  };

  const InkDropEventHandler* GetEventHandler() const;
  InkDropEventHandler* GetEventHandler();

  // Defines what type of |ink_drop_| to create.
  InkDropMode ink_drop_mode_ = InkDropMode::OFF;

  // Should not be accessed directly. Use GetInkDrop() instead.
  std::unique_ptr<InkDrop> ink_drop_;

  // Intentionally declared after |ink_drop_| so that it doesn't access a
  // destroyed |ink_drop_| during destruction.
  InkDropHostViewEventHandlerDelegate ink_drop_event_handler_delegate_;
  InkDropEventHandler ink_drop_event_handler_;

  InkDropEventHandler* ink_drop_event_handler_override_ = nullptr;

  float ink_drop_visible_opacity_ = 0.175f;

  // TODO(pbos): Audit call sites to make sure highlight opacity is either
  // always set or using the default value. Then make this a non-optional float.
  base::Optional<float> ink_drop_highlight_opacity_;

  // Radii used for the SquareInkDropRipple.
  int ink_drop_small_corner_radius_ = 2;
  int ink_drop_large_corner_radius_ = 4;

  bool destroying_ = false;

  std::unique_ptr<views::InkDropMask> ink_drop_mask_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostView);
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, InkDropHostView, View)
VIEW_BUILDER_PROPERTY(base::Optional<float>, InkDropHighlightOpacity)
VIEW_BUILDER_PROPERTY(int, InkDropLargeCornerRadius)
VIEW_BUILDER_PROPERTY(InkDropHostView::InkDropMode, InkDropMode)
VIEW_BUILDER_PROPERTY(int, InkDropSmallCornerRadius)
VIEW_BUILDER_PROPERTY(float, InkDropVisibleOpacity)
END_VIEW_BUILDER(VIEWS_EXPORT, InkDropHostView)

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
