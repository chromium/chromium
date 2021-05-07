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
  InkDropHostView(const InkDropHostView&) = delete;
  InkDropHostView& operator=(const InkDropHostView&) = delete;
  ~InkDropHostView() override;

  // TODO(pbos): Re-think this API, we may want to expose adding a Layer beneath
  // a child view to add the effect in the middle of the layer stack. See
  // ToggleButton.
  //
  // Adds a callback for attaching |ink_drop_layer| in to a visible layer tree.
  //
  // Do not call from new code. Most uses for this API should be overriding
  // View::AddLayerBeneathView instead. New ones should re-think the API.
  void SetAddInkDropLayerCallback(
      base::RepeatingCallback<void(ui::Layer*)> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<void(ui::Layer*)>& GetAddInkDropLayerCallback()
      const;

  // TODO(pbos): Re-think this API, we may want to expose adding a Layer beneath
  // a child view to add the effect in the middle of the layer stack. See
  // ToggleButton.
  //
  // Adds a callback for removing |ink_drop_layer| from the layer tree.
  //
  // Do not call from new code. Most uses for this API should be overriding
  // View::AddLayerBeneathView instead. New ones should re-think the API.
  void SetRemoveInkDropLayerCallback(
      base::RepeatingCallback<void(ui::Layer*)> callback);
  // Only here to support metadata.
  const base::RepeatingCallback<void(ui::Layer*)>&
  GetRemoveInkDropLayerCallback() const;

  // Returns a configured InkDrop. To override default behavior call
  // SetCreateInkDropCallback().
  std::unique_ptr<InkDrop> CreateInkDrop();

  // Replace CreateInkDrop() behavior.
  void SetCreateInkDropCallback(
      base::RepeatingCallback<std::unique_ptr<InkDrop>()> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<std::unique_ptr<InkDrop>()>&
  GetCreateInkDropCallback() const;

  // Creates and returns the visual effect used for press. Used by InkDropImpl
  // instances.
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const;

  // Replaces CreateInkDropRipple() behavior.
  void SetCreateInkDropRippleCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropRipple>()> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<std::unique_ptr<InkDropRipple>()>&
  GetCreateInkDropRippleCallback() const;

  // Returns the point of the |last_ripple_triggering_event_| if it was a
  // LocatedEvent, otherwise the center point of the local bounds is returned.
  // This is nominally used by the InkDropRipple.
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Creates and returns the visual effect used for hover and focus. Used by
  // InkDropImpl instances. To override behavior call
  // SetCreateInkDropHighlightCallback().
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const;

  // Replaces CreateInkDropHighlight() behavior.
  void SetCreateInkDropHighlightCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()>&
  GetCreateInkDropHighlightCallback() const;

  // Callback replacement of CreateInkDropMask().
  // TODO(pbos): Investigate removing this. It currently is only used by
  // ToolbarButton.
  void SetCreateInkDropMaskCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropMask>()> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<std::unique_ptr<InkDropMask>()>&
  GetCreateInkDropMaskCallback() const;

  // Returns the base color for the ink drop.
  SkColor GetInkDropBaseColor() const;

  // Sets the base color for the ink drop.
  void SetInkDropBaseColor(SkColor color);

  // Callback version of GetInkDropBaseColor(). If possible, prefer using
  // SetInkDropBaseColor(). If a callback has been set by previous configuration
  // and you want to use the base version of GetInkDropBaseColor() that's
  // reading SetInkDropBaseColor(), you need to reset the callback by calling
  // SetInkDropBaseColorCallback({}).
  void SetInkDropBaseColorCallback(base::RepeatingCallback<SkColor()> callback);

  // Only here to support metadata.
  const base::RepeatingCallback<SkColor()>& GetInkDropBaseColorCallback() const;

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

  // Returns true if an ink drop instance has been created.
  bool HasInkDrop() const;

  // Provides public access to |ink_drop_| so that factory methods can configure
  // the inkdrop. Implements lazy initialization of |ink_drop_| so as to avoid
  // virtual method calls during construction since subclasses should be able to
  // call SetInkDropMode() during construction.
  InkDrop* GetInkDrop();

  // Returns whether the ink drop should be considered "highlighted" (in or
  // animating into "highlight visible" steady state).
  bool GetHighlighted() const;

  base::CallbackListSubscription AddHighlightedChangedCallback(
      PropertyChangedCallback callback);

  // Should be called by InkDrop implementations when their highlight state
  // changes, to trigger the corresponding property change notification here.
  void OnInkDropHighlightedChanged();

  // Methods called by InkDrop for attaching its layer.
  // TODO(pbos): Investigate using direct calls on View::AddLayerBeneathView.
  void AddInkDropLayer(ui::Layer* ink_drop_layer);
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer);

  // Size used by default for the SquareInkDropRipple.
  static constexpr gfx::Size kDefaultSquareInkDropSize = gfx::Size(24, 24);

  // Creates a SquareInkDropRipple centered on |center_point|.
  std::unique_ptr<InkDropRipple> CreateSquareInkDropRipple(
      const gfx::Point& center_point,
      const gfx::Size& size = kDefaultSquareInkDropSize) const;

 private:
  friend class test::InkDropHostViewTestApi;

  class ViewLayerTransformObserver : public ViewObserver {
   public:
    explicit ViewLayerTransformObserver(InkDropHostView* host);
    ~ViewLayerTransformObserver() override;

    void OnViewLayerTransformed(View* observed_view) override;

   private:
    base::ScopedObservation<View, ViewObserver> observation_{this};
    InkDropHostView* const host_;
  };

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

  // This generates a mask for the InkDrop.
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const;

  // Adds a clip rect on the root layer of the ink drop impl. This is a more
  // performant alternative to using circles or rectangle mask layers. Returns
  // true if a clip was added.
  bool AddInkDropClip(ui::Layer* ink_drop_layer);

  // Initializes and sets a mask on `ink_drop_layer`. This will not run if
  // AddInkDropClip() succeeds in the default implementation of
  // AddInkDropLayer().
  void InstallInkDropMask(ui::Layer* ink_drop_layer);

  // Defines what type of |ink_drop_| to create.
  InkDropMode ink_drop_mode_ = InkDropMode::OFF;

  // Used to observe View and inform the InkDrop of host-transform changes.
  ViewLayerTransformObserver host_view_transform_observer_;

  // Should not be accessed directly. Use GetInkDrop() instead.
  std::unique_ptr<InkDrop> ink_drop_;

  // Intentionally declared after |ink_drop_| so that it doesn't access a
  // destroyed |ink_drop_| during destruction.
  InkDropHostViewEventHandlerDelegate ink_drop_event_handler_delegate_;
  InkDropEventHandler ink_drop_event_handler_;

  InkDropEventHandler* ink_drop_event_handler_override_ = nullptr;

  float ink_drop_visible_opacity_ = 0.175f;

  // The color of the ripple and hover.
  base::Optional<SkColor> ink_drop_base_color_;

  // TODO(pbos): Audit call sites to make sure highlight opacity is either
  // always set or using the default value. Then make this a non-optional float.
  base::Optional<float> ink_drop_highlight_opacity_;

  // Radii used for the SquareInkDropRipple.
  int ink_drop_small_corner_radius_ = 2;
  int ink_drop_large_corner_radius_ = 4;

  bool destroying_ = false;

  std::unique_ptr<views::InkDropMask> ink_drop_mask_;

  base::RepeatingCallback<void(ui::Layer*)> add_ink_drop_layer_callback_;
  base::RepeatingCallback<void(ui::Layer*)> remove_ink_drop_layer_callback_;
  base::RepeatingCallback<std::unique_ptr<InkDrop>()> create_ink_drop_callback_;
  base::RepeatingCallback<std::unique_ptr<InkDropRipple>()>
      create_ink_drop_ripple_callback_;
  base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()>
      create_ink_drop_highlight_callback_;

  base::RepeatingCallback<std::unique_ptr<InkDropMask>()>
      create_ink_drop_mask_callback_;
  base::RepeatingCallback<SkColor()> ink_drop_base_color_callback_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, InkDropHostView, View)
VIEW_BUILDER_PROPERTY(base::Optional<float>, InkDropHighlightOpacity)
VIEW_BUILDER_PROPERTY(int, InkDropLargeCornerRadius)
VIEW_BUILDER_PROPERTY(InkDropHostView::InkDropMode, InkDropMode)
VIEW_BUILDER_PROPERTY(int, InkDropSmallCornerRadius)
VIEW_BUILDER_PROPERTY(SkColor, InkDropBaseColor)
VIEW_BUILDER_PROPERTY(float, InkDropVisibleOpacity)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, InkDropHostView)

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
