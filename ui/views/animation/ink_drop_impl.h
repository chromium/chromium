// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight_observer.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/views_export.h"

namespace views {
namespace test {
class InkDropImplTestApi;
}  // namespace test

class InkDropRipple;
class InkDropHost;
class InkDropHighlight;

// A functional implementation of an InkDrop.
class VIEWS_EXPORT InkDropImpl : public InkDrop,
                                 public InkDropRippleObserver,
                                 public InkDropHighlightObserver {
 public:
  // The different auto highlight behaviors.
  enum class AutoHighlightMode {
    // No auto-highlighting is done. The highlight will only be shown/hidden as
    // per the hover/focus settings.
    NONE,
    // The highlight will be hidden when a ripple becomes visible. After the
    // ripple is hidden the highlight will be made visible again if the
    // hover/focus settings deem it should be.
    HIDE_ON_RIPPLE,
    // The highlight is made visible when the ripple becomes visible. After the
    // ripple is hidden the highlight will be hidden again if the hover/focus
    // settings deem it should be.
    SHOW_ON_RIPPLE,
  };

  // Constructs an ink drop that will attach the ink drop to the given
  // |ink_drop_host|. |host_size| is used to set the size of the ink drop layer.
  //
  // By default the highlight will be made visible while |this| is hovered but
  // not focused.
  InkDropImpl(InkDropHost* ink_drop_host,
              const gfx::Size& host_size,
              AutoHighlightMode auto_highlight_mode);

  InkDropImpl(const InkDropImpl&) = delete;
  InkDropImpl& operator=(const InkDropImpl&) = delete;

  ~InkDropImpl() override;

  const std::optional<base::TimeDelta>& hover_highlight_fade_duration() const {
    return hover_highlight_fade_duration_;
  }

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  void HostViewThemeChanged() override;
  void HostTransformChanged(const gfx::Transform& new_transform) override;
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

 private:
  friend class InkDropImplTest;
  friend class test::InkDropImplTestApi;

  // Forward declaration for use by the HighlightState class definition.
  class HighlightStateFactory;

  // Base HighlightState defines functions to handle all state changes that may
  // affect the highlight state.
  //
  // Subclasses are expected to handle state changes and transition the
  // InkDropImpl::highlight_state_ to new states as desired via the
  // InkDropImpl::SetHighlightState() method.
  //
  // New states should be created via the HighlightStateFactory and not
  // directly. This makes it possible for highlighting strategies to extend the
  // behavior of existing states and re-use existing state behavior.
  //
  // Subclasses are also expected to trigger the appropriate highlight
  // animations (e.g. fade in/out) via GetInkDrop()->SetHighlight(). Typically
  // this is done in the Enter()/Exit() functions. Triggering animations
  // anywhere else may be a sign that a new state should exist.
  class HighlightState {
   public:
    HighlightState(const HighlightState&) = delete;
    HighlightState& operator=(const HighlightState&) = delete;

    virtual ~HighlightState() = default;

    // Called when |this| becomes the current state. Allows subclasses to
    // perform any work that should not be done in the constructor. It is ok for
    // subclass implementations to trigger state changes from within Enter().
    virtual void Enter() {}

    // Called just before |this| is removed as the current state. Allows
    // subclasses to perform any work that should not be done in the destructor
    // but is required before exiting |this| state (e.g. releasing resources).
    //
    // Subclass implementations should NOT do any work that may trigger another
    // state change since a state change is already in progress. They must also
    // avoid triggering any animations since Exit() will be called during
    // InkDropImpl destruction.
    virtual void Exit() {}

    // Input state change handlers.

    // Called when the value of InkDropImpl::show_highlight_on_hover_ changes.
    virtual void ShowOnHoverChanged() = 0;

    // Called when the value of InkDropImpl::is_hovered_ changes.
    virtual void OnHoverChanged() = 0;

    // Called when the value of InkDropImpl::show_highlight_on_focus_ changes.
    virtual void ShowOnFocusChanged() = 0;

    // Called when the value of InkDropImpl::is_focused_ changes.
    virtual void OnFocusChanged() = 0;

    // Called when an ink drop ripple animation is started.
    virtual void AnimationStarted(InkDropState ink_drop_state) = 0;

    // Called when an ink drop ripple animation has ended.
    virtual void AnimationEnded(InkDropState ink_drop_state,
                                InkDropAnimationEndedReason reason) = 0;

   protected:
    explicit HighlightState(HighlightStateFactory* state_factory)
        : state_factory_(state_factory) {}

    HighlightStateFactory* state_factory() { return state_factory_; }

    // Returns the ink drop that has |this| as the current state.
    InkDropImpl* GetInkDrop();

   private:
    // Used by |this| to create the new states to transition to.
    const raw_ptr<HighlightStateFactory> state_factory_;
  };

  // Creates the different HighlightStates instances. A factory is used to make
  // it easier for states to extend and re-use existing state logic.
  class HighlightStateFactory {
   public:
    HighlightStateFactory(AutoHighlightMode highlight_mode,
                          InkDropImpl* ink_drop);

    HighlightStateFactory(const HighlightStateFactory&) = delete;
    HighlightStateFactory& operator=(const HighlightStateFactory&) = delete;

    // Returns the initial state.
    std::unique_ptr<HighlightState> CreateStartState();

    std::unique_ptr<HighlightState> CreateHiddenState(
        base::TimeDelta animation_duration);

    std::unique_ptr<HighlightState> CreateVisibleState(
        base::TimeDelta animation_duration);

    InkDropImpl* ink_drop() { return ink_drop_; }

   private:
    // Defines which concrete state types to create.
    AutoHighlightMode highlight_mode_;

    // The ink drop to invoke highlight changes on.
    raw_ptr<InkDropImpl> ink_drop_;
  };

  class DestroyingHighlightState;

  // AutoHighlightMode::NONE
  class NoAutoHighlightHiddenState;
  class NoAutoHighlightVisibleState;

  // AutoHighlightMode::HIDE_ON_RIPPLE
  class HideHighlightOnRippleHiddenState;
  class HideHighlightOnRippleVisibleState;

  // AutoHighlightMode::SHOW_ON_RIPPLE states
  class ShowHighlightOnRippleHiddenState;
  class ShowHighlightOnRippleVisibleState;

  // Destroys |ink_drop_ripple_| if it's targeted to the HIDDEN state.
  void DestroyHiddenTargetedAnimations();

  // Creates a new InkDropRipple and sets it to |ink_drop_ripple_|. If
  // |ink_drop_ripple_| wasn't null then it will be destroyed using
  // DestroyInkDropRipple().
  void CreateInkDropRipple();

  // Destroys the current |ink_drop_ripple_|.
  void DestroyInkDropRipple();

  // Creates a new InkDropHighlight and assigns it to |highlight_|. If
  // |highlight_| wasn't null then it will be destroyed using
  // DestroyInkDropHighlight().
  void CreateInkDropHighlight();

  // Destroys the current |highlight_|.
  void DestroyInkDropHighlight();

  // Adds the |root_layer_| to the |ink_drop_host_| if it hasn't already been
  // added.
  void AddRootLayerToHostIfNeeded();

  // Removes the |root_layer_| from the |ink_drop_host_| if no ink drop ripple
  // or highlight is active.
  void RemoveRootLayerFromHostIfNeeded();

  // views::InkDropRippleObserver:
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

  // views::InkDropHighlightObserver:
  void AnimationStarted(
      InkDropHighlight::AnimationType animation_type) override;
  void AnimationEnded(InkDropHighlight::AnimationType animation_type,
                      InkDropAnimationEndedReason reason) override;

  // Enables or disables the highlight state based on |should_highlight| and if
  // an animation is triggered it will be scheduled to have the given
  // |animation_duration|.
  void SetHighlight(bool should_highlight, base::TimeDelta animation_duration);

  // Returns true if |this| the highlight should be visible based on the
  // hover/focus status.
  bool ShouldHighlight() const;

  // Returns true if |this| the hilight should be visible based on the focus
  // status.
  bool ShouldHighlightBasedOnFocus() const;

  // Updates the current |highlight_state_|. Calls Exit()/Enter() on the
  // previous/new state to notify them of the transition.
  //
  // Uses ExitHighlightState() to exit the current state.
  void SetHighlightState(std::unique_ptr<HighlightState> highlight_state);

  // Exits the current |highlight_state_| and sets it to null. Ensures state
  // transitions are not triggered during HighlightStatae::Exit() calls on debug
  // builds.
  void ExitHighlightState();

  // Recreate the ripple and highlight.
  void RecreateRippleAndHighlight();

  // The host of the ink drop. Used to create the ripples and highlights, and to
  // add/remove the root layer to/from it.
  const raw_ptr<InkDropHost> ink_drop_host_;

  // Used by |this| to initialize the starting |highlight_state_| and by the
  // current |highlight_state_| to create the next state.
  HighlightStateFactory highlight_state_factory_;

  // The root Layer that parents the InkDropRipple layers and the
  // InkDropHighlight layers. The |root_layer_| is the one that is added and
  // removed from the |ink_drop_host_|.
  std::unique_ptr<ui::Layer> root_layer_;

  // True when the |root_layer_| has been added to the |ink_drop_host_|.
  bool root_layer_added_to_host_ = false;

  // The current InkDropHighlight. Lazily created using
  // CreateInkDropHighlight();
  std::unique_ptr<InkDropHighlight> highlight_;

  // True denotes the highlight should be shown when |this| is hovered.
  bool show_highlight_on_hover_ = true;

  // True denotes the highlight should be shown when |this| is focused.
  bool show_highlight_on_focus_ = false;

  // Tracks the logical hovered state of |this| as manipulated by the public
  // SetHovered() function.
  bool is_hovered_ = false;

  // Tracks the logical focused state of |this| as manipulated by the public
  // SetFocused() function.
  bool is_focused_ = false;

  // The current InkDropRipple. Created on demand using CreateInkDropRipple().
  std::unique_ptr<InkDropRipple> ink_drop_ripple_;

  // The current state object that handles all inputs that affect the visibility
  // of the |highlight_|.
  std::unique_ptr<HighlightState> highlight_state_;

  // Overrides the default hover highlight fade durations when set.
  std::optional<base::TimeDelta> hover_highlight_fade_duration_;

  // Used to ensure highlight state transitions are not triggered when exiting
  // the current state.
  bool exiting_highlight_state_ = false;

  // Used to fail DCHECKS to catch unexpected behavior during tear down.
  bool destroying_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_
