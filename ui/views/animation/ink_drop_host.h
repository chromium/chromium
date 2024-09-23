// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
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
class InkDropHostTestApi;
}  // namespace test

// TODO(crbug.com/40613900): Rename this type and move this header. Also
// consider if InkDropHost should be what implements the InkDrop interface and
// have that be the public interface. The current division of labor is roughly
// as follows:
// * InkDropHost manages an InkDrop and is responsible for a lot of its
//   configuration and creating the parts of the InkDrop.
// * InkDrop manages the parts of the ink-drop effect once it's up and running.
// * InkDropRipple is a ripple effect that usually triggers as a result of
//   clicking or activating the button / similar which hosts this.
// * InkDropHighlight manages the hover/focus highlight layer.
// TODO(pbos): See if this can be the only externally visible surface for an
// ink-drop effect, and rename this InkDrop, or consolidate with InkDrop.
class VIEWS_EXPORT InkDropHost {
 public:
  // Used in SetMode() to specify whether the ink drop effect is enabled
  // or not for the view. In case of having an ink drop, it also specifies
  // whether the default event handler for the ink drop should be installed or
  // the subclass will handle ink drop events itself.
  enum class InkDropMode {
    OFF,
    ON,
    ON_NO_GESTURE_HANDLER,
    ON_NO_ANIMATE,
  };

  explicit InkDropHost(View* host);
  InkDropHost(const InkDropHost&) = delete;
  InkDropHost& operator=(const InkDropHost&) = delete;
  virtual ~InkDropHost();

  // Returns a configured InkDrop. To override default behavior call
  // SetCreateInkDropCallback().
  std::unique_ptr<InkDrop> CreateInkDrop();

  // Replace CreateInkDrop() behavior.
  void SetCreateInkDropCallback(
      base::RepeatingCallback<std::unique_ptr<InkDrop>()> callback);

  // Creates and returns the visual effect used for press. Used by InkDropImpl
  // instances.
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const;

  // Replaces CreateInkDropRipple() behavior.
  void SetCreateRippleCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropRipple>()> callback);

  // Returns the point of the |last_ripple_triggering_event_| if it was a
  // LocatedEvent, otherwise the center point of the local bounds is returned.
  // This is nominally used by the InkDropRipple.
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Creates and returns the visual effect used for hover and focus. Used by
  // InkDropImpl instances. To override behavior call
  // SetCreateHighlightCallback().
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const;

  // Replaces CreateInkDropHighlight() behavior.
  void SetCreateHighlightCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()> callback);

  // Callback replacement of CreateInkDropMask().
  // TODO(pbos): Investigate removing this. It currently is only used by
  // PieMenuView.
  void SetCreateMaskCallback(
      base::RepeatingCallback<std::unique_ptr<InkDropMask>()> callback);

  // Toggles ink drop attention state on/off. If set on, a pulsing highlight
  // is shown, prompting users to interact with `host_view_`.
  // Called by components that want to call into user's attention, e.g. IPH.
  void ToggleAttentionState(bool attention_on);

  // Returns the base color for the ink drop.
  SkColor GetBaseColor() const;

  // Sets the base color of the ink drop. If `SetBaseColor` is called, the
  // effect of previous calls to `SetBaseColorId` and `SetBaseColorCallback` is
  // overwritten and vice versa.
  // TODO(crbug.com/40230665): Replace SetBaseColor with SetBaseColorId.
  void SetBaseColor(SkColor color);
  void SetBaseColorId(ui::ColorId color_id);
  // Callback version of `GetBaseColor`. If possible, prefer using
  // `SetBaseColor` or `SetBaseColorId`.
  void SetBaseColorCallback(base::RepeatingCallback<SkColor()> callback);

  // Toggle to enable/disable an InkDrop on this View.  Descendants can override
  // CreateInkDropHighlight() and CreateInkDropRipple() to change the look/feel
  // of the InkDrop.
  //
  // TODO(bruthig): Add an easier mechanism than overriding functions to allow
  // subclasses/clients to specify the flavor of ink drop.
  void SetMode(InkDropMode ink_drop_mode);
  InkDropMode GetMode() const;

  // Set whether the ink drop layers should be placed into the region above or
  // below the view layer. The default is kBelow;
  void SetLayerRegion(LayerRegion region);
  LayerRegion GetLayerRegion() const;

  void SetVisibleOpacity(float visible_opacity);
  float GetVisibleOpacity() const;

  void SetHighlightOpacity(std::optional<float> opacity);

  void SetSmallCornerRadius(int small_radius);
  int GetSmallCornerRadius() const;

  void SetLargeCornerRadius(int large_radius);
  int GetLargeCornerRadius() const;

  // Animates |ink_drop_| to the desired |ink_drop_state|. Caches |event| as the
  // last_ripple_triggering_event().
  //
  // *** NOTE ***: |event| has been plumbed through on a best effort basis for
  // the purposes of centering ink drop ripples on located Events.  Thus nullptr
  // has been used by clients who do not have an Event instance available to
  // them.
  void AnimateToState(InkDropState state, const ui::LocatedEvent* event);

  // Returns true if an ink drop instance has been created.
  bool HasInkDrop() const;

  // Provides public access to |ink_drop_| so that factory methods can configure
  // the inkdrop. Implements lazy initialization of |ink_drop_| so as to avoid
  // virtual method calls during construction since subclasses should be able to
  // call SetMode() during construction.
  InkDrop* GetInkDrop();

  // Returns whether the ink drop should be considered "highlighted" (in or
  // animating into "highlight visible" steady state).
  bool GetHighlighted() const;

  base::CallbackListSubscription AddHighlightedChangedCallback(
      base::RepeatingClosure callback);

  // Should be called by InkDrop implementations when their highlight state
  // changes, to trigger the corresponding property change notification here.
  void OnInkDropHighlightedChanged();

  // Methods called by InkDrop for attaching its layer.
  // TODO(pbos): Investigate using direct calls on View::AddLayerToRegion.
  void AddInkDropLayer(ui::Layer* ink_drop_layer);
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer);

  // Size used by default for the SquareInkDropRipple.
  static constexpr gfx::Size kDefaultSquareInkDropSize = gfx::Size(24, 24);

  // Returns a large scaled size used by SquareInkDropRipple and Highlight.
  static gfx::Size GetLargeSize(gfx::Size small_size);

  // Creates a SquareInkDropRipple centered on |center_point|.
  std::unique_ptr<InkDropRipple> CreateSquareRipple(
      const gfx::Point& center_point,
      const gfx::Size& size = kDefaultSquareInkDropSize) const;

  View* host_view() { return host_view_; }
  const View* host_view() const { return host_view_; }

 private:
  friend class test::InkDropHostTestApi;

  class ViewLayerTransformObserver : public ViewObserver {
   public:
    ViewLayerTransformObserver(InkDropHost* ink_drop_host, View* host);
    ~ViewLayerTransformObserver() override;

    void OnViewLayerTransformed(View* observed_view) override;

   private:
    base::ScopedObservation<View, ViewObserver> observation_{this};
    const raw_ptr<InkDropHost> ink_drop_host_;
  };

  class InkDropHostEventHandlerDelegate : public InkDropEventHandler::Delegate {
   public:
    explicit InkDropHostEventHandlerDelegate(InkDropHost* host);

    // InkDropEventHandler::Delegate:
    InkDrop* GetInkDrop() override;
    bool HasInkDrop() const override;

    bool SupportsGestureEvents() const override;

   private:
    // The host.
    const raw_ptr<InkDropHost> ink_drop_host_;
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

  const raw_ptr<View> host_view_;

  // Defines what type of |ink_drop_| to create.
  InkDropMode ink_drop_mode_ = views::InkDropHost::InkDropMode::OFF;

  // Into which region should the ink drop layers be placed.
  LayerRegion layer_region_ = LayerRegion::kBelow;

  // Used to observe View and inform the InkDrop of host-transform changes.
  ViewLayerTransformObserver host_view_transform_observer_;

  // Should not be accessed directly. Use GetInkDrop() instead.
  std::unique_ptr<InkDrop> ink_drop_;

  // Intentionally declared after |ink_drop_| so that it doesn't access a
  // destroyed |ink_drop_| during destruction.
  InkDropHostEventHandlerDelegate ink_drop_event_handler_delegate_;
  InkDropEventHandler ink_drop_event_handler_;

  float ink_drop_visible_opacity_ = 0.175f;

  // The color of the ripple and hover.
  absl::variant<SkColor, ui::ColorId, base::RepeatingCallback<SkColor()>>
      ink_drop_base_color_ = gfx::kPlaceholderColor;

  // TODO(pbos): Audit call sites to make sure highlight opacity is either
  // always set or using the default value. Then make this a non-optional float.
  std::optional<float> ink_drop_highlight_opacity_;

  // Radii used for the SquareInkDropRipple.
  int ink_drop_small_corner_radius_ = 2;
  int ink_drop_large_corner_radius_ = 4;

  std::unique_ptr<views::InkDropMask> ink_drop_mask_;

  base::RepeatingCallback<std::unique_ptr<InkDrop>()> create_ink_drop_callback_;
  base::RepeatingCallback<std::unique_ptr<InkDropRipple>()>
      create_ink_drop_ripple_callback_;
  base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()>
      create_ink_drop_highlight_callback_;

  base::RepeatingCallback<std::unique_ptr<InkDropMask>()>
      create_ink_drop_mask_callback_;

  base::RepeatingClosureList highlighted_changed_callbacks_;

  // Attention is a state we apply on Buttons' ink drop when we want to draw
  // users' attention to this button and prompt users' interaction.
  // It consists of two visual effects: a default light blue color and a pulsing
  // effect. Current use case is IPH. Go to chrome://internals/user-education
  // and press e.g. IPH_TabSearch to see the effects.
  bool in_attention_state_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_H_
