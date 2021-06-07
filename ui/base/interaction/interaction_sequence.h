// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
#define UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_

#include <list>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

// Follows an expected sequence of user-UI interactions and provides callbacks
// at each step. Useful for creating interaction tests and user tutorials.
//
// An interaction sequence consists of an ordered series of steps, each of which
// refers to an interface element tagged with a ElementIdentifier and each of
// which represents that element being either shown, activated, or hidden. Other
// unrelated events such as element hover or focus are ignored (but could be
// supported in the future).
//
// Each step has an optional callback that is triggered when the expected
// interaction happens, and an optional callback that is triggered when the step
// ends - either because the next step has started or because the user has
// aborted the sequence (typically by dismissing UI such as a dialog or menu,
// resulting in the element from the current step being hidden/destroyed). Once
// the first callback is called/the step starts, the second callback will always
// be called.
//
// Furthermore, when the last step in the sequence completes, in addition to its
// end callback, an optional sequence-completed callback will be called. If the
// user aborts the sequence or if this object is destroyed, then an optional
// sequence-aborted callback is called instead.
//
// To use a InteractionSequence, start with a builder:
//
//  sequence_ = InteractionSequence::Builder()
//      .SetCompletedCallback(base::BindOnce(...))
//      .AddStep(InteractionSequence::WithInitialElement(initial_element))
//      .AddStep(InteractionSequence::StepBuilder()
//          .SetElementID(kDialogElementID)
//          .SetType(StepType::kShown)
//          .SetStartCallback(...)
//          .Build())
//      .AddStep(...)
//      .Build();
//  sequence_->Start();
//
// For more detailed instructions on using the ui/base/interaction library, see
// README.md in this folder.
//
class COMPONENT_EXPORT(UI_BASE) InteractionSequence {
 public:
  // The type of event that is expected to happen next in the sequence.
  enum class StepType {
    // Represents the element with the specified ID becoming visible to the
    // user.
    kShown,
    // Represents an element with the specified ID becoming activated by the
    // user (for buttons or menu items, being clicked).
    kActivated,
    // Represents an element with the specified ID becoming hidden or destroyed.
    kHidden
  };

  // Callback when a step happens in the sequence, or when a step ends. If
  // |element| is no longer available, it will be null.
  using StepCallback = base::OnceCallback<void(TrackedElement* element,
                                               ElementIdentifier element_id,
                                               StepType step_type)>;

  // Callback for when the user aborts the sequence by failing to follow the
  // sequence of steps, or if this object is deleted after the sequence starts.
  // The most recent event is described by the parameters; if the target element
  // is no longer available it will be null.
  using AbortedCallback = base::OnceCallback<void(TrackedElement* last_element,
                                                  ElementIdentifier last_id,
                                                  StepType last_step_type)>;

  using CompletedCallback = base::OnceClosure;

  struct Configuration;
  class StepBuilder;

  struct COMPONENT_EXPORT(UI_BASE) Step {
    Step();
    Step(const Step& other) = delete;
    void operator=(const Step& other) = delete;
    ~Step();

    StepType type = StepType::kShown;
    ElementIdentifier id;
    ElementContext context;

    // These will always have values when the sequence is built, but can be
    // unspecified during construction. If unspecified, they will be set to
    // appropriate defaults for `type`.
    absl::optional<bool> must_be_visible;
    absl::optional<bool> must_remain_visible;

    StepCallback start_callback;
    StepCallback end_callback;
    ElementTracker::Subscription subscription;

    // Tracks the element associated with the step, if known. We could use a
    // SafeElementReference here, but there are cases where we want to do
    // additional processing if this element goes away, so we'll add the
    // listeners manually instead.
    TrackedElement* element = nullptr;
  };

  // Use a Builder to specify parameters when creating an InteractionSequence.
  class COMPONENT_EXPORT(UI_BASE) Builder {
   public:
    Builder();
    Builder(const Builder& other) = delete;
    void operator=(const Builder& other) = delete;
    ~Builder();

    // Sets the callback if the user exits the sequence early.
    Builder& SetAbortedCallback(AbortedCallback callback);

    // Sets the callback if the user completes the sequence.
    // Convenience method so that the last step's end callback doesn't need to
    // have special logic in it.
    Builder& SetCompletedCallback(CompletedCallback callback);

    // Adds an expected step in the sequence. All sequences must have at least
    // one step.
    Builder& AddStep(std::unique_ptr<Step> step);

    // Sets the context for this sequence. Must be called if no step is added
    // by element or has had SetContext() called. Typically the initial step of
    // a sequence will use WithInitialElement() so it won't be necessary to call
    // this method.
    Builder& SetContext(ElementContext context);

    // Creates the InteractionSequence. You must call Start() to initiate the
    // sequence; sequences cannot be re-used, and a Builder is no longer valid
    // after Build() is called.
    std::unique_ptr<InteractionSequence> Build();

   private:
    std::unique_ptr<Configuration> configuration_;
  };

  // Used inline in calls to Builder::AddStep to specify step parameters.
  class COMPONENT_EXPORT(UI_BASE) StepBuilder {
   public:
    StepBuilder();
    ~StepBuilder();
    StepBuilder(const StepBuilder& other) = delete;
    void operator=(StepBuilder& other) = delete;

    // Sets the unique identifier for this step. Required.
    StepBuilder& SetElementID(ElementIdentifier element_id);

    // Sets the context for the element; useful for setting up the initial
    // element of the sequence if you do not know the context ahead of time.
    // Prefer to use Builder::SetContext() if possible.
    StepBuilder& SetContext(ElementContext context);

    // Sets the type of step. Required.
    StepBuilder& SetType(StepType step_type);

    // Indicates that the specified element must be visible at the start of the
    // step. Defaults to true for StepType::kActivated, false otherwise. Failure
    // To meet this condition will abort the sequence.
    StepBuilder& SetMustBeVisibleAtStart(bool must_be_visible);

    // Indicates that the specified element must remain visible throughout the
    // step once it has been shown. Defaults to true for StepType::kShown, false
    // otherwise (and incompatible with StepType::kHidden). Failure to meet this
    // condition will abort the sequence.
    StepBuilder& SetMustRemainVisible(bool must_remain_visible);

    // Sets the callback called at the start of the step.
    StepBuilder& SetStartCallback(StepCallback start_callback);

    // Sets the callback called at the end of the step. Guaranteed to be called
    // if the start callback is called, before the start callback of the next
    // step or the sequence aborted or completed callback. Also called if this
    // object is destroyed while the step is still in-process.
    StepBuilder& SetEndCallback(StepCallback end_callback);

    // Builds the step. The builder will not be valid after calling Build().
    std::unique_ptr<Step> Build();

   private:
    friend class InteractionSequence;
    std::unique_ptr<Step> step_;
  };

  // Returns a step with the following values already set, typically used as the
  // first step in a sequence (because the first element is usually present):
  //   ElementID: element->identifier()
  //   MustBeVisibleAtStart: true
  //   MustRemainVisible: true
  //
  // This is a convenience method and also removes the need to call
  // Builder::SetContext(). Specific framework implementations may provide
  // wrappers around this method that allow direct conversion from framework UI
  // elements (e.g. a views::View) to the target element.
  static std::unique_ptr<Step> WithInitialElement(
      TrackedElement* element,
      StepCallback start_callback = StepCallback(),
      StepCallback end_callback = StepCallback());

  ~InteractionSequence();

  // Starts the sequence. All of the elements in the sequence must belong to the
  // same top-level application window (which includes menus, bubbles, etc.
  // associated with that window).
  void Start();

 private:
  explicit InteractionSequence(std::unique_ptr<Configuration> configuration);

  // Callbacks from the ElementTracker.
  void OnElementShown(TrackedElement* element);
  void OnElementActivated(TrackedElement* element);
  void OnElementHidden(TrackedElement* element);

  // Callbacks used only during step transitions to cache certain events.
  void OnElementActivatedDuringStepTransition(TrackedElement* element);
  void OnElementHiddenDuringStepTransition(TrackedElement* element);

  // A note on the next three methods - DoStepTransition(), StageNextStep(), and
  // Abort(): To prevent re-entrancy issues, they must always be the final call
  // in any method before it returns. This greatly simplifies the consistency
  // checks and safeguards that need to be put into place to make sure we aren't
  // making contradictory changes to state or calling callbacks in the wrong
  // order.

  // Perform the transition from the current step to the next step.
  void DoStepTransition(TrackedElement* element);

  // Looks at the next step to determine what needs to be done. Called at the
  // start of the sequence and after each subsequent step starts.
  void StageNextStep();

  // Cancels the sequence and cleans up.
  void Abort();

  // Returns true (and does some sanity checking) if the sequence was aborted
  // during the most recent callback.
  bool AbortedDuringCallback() const;

  // The following would be inline if not for the fact that the data that holds
  // the values is an implementation detail.

  // Returns the next step, or null if none.
  Step* next_step();

  // Returns the context for the current sequence.
  ElementContext context() const;

  bool missing_first_element_ = false;
  bool started_ = false;
  bool activated_during_callback_ = false;
  bool processing_step_ = false;
  std::unique_ptr<Step> current_step_;
  std::unique_ptr<Configuration> configuration_;

  // This is necessary because this object could be deleted during any callback,
  // and we don't want to risk a UAF if that happens.
  base::WeakPtrFactory<InteractionSequence> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
