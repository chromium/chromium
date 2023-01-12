// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
#define UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_

#include <map>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
    // user, or already being visible when the step starts.
    kShown,
    // Represents an element with the specified ID becoming activated by the
    // user (for buttons or menu items, being clicked).
    kActivated,
    // Represents an element with the specified ID becoming hidden or
    // destroyed, or no elements with the specified ID being visible.
    kHidden,
    // Represents a custom event with a specific custom event type. You may
    // further specify a required element name or ID to filter down which
    // events you actually want to step on vs. ignore.
    kCustomEvent,
    // Update this if values are added to the enumeration.
    kMaxValue = kCustomEvent
  };

  // Details why a sequence was aborted.
  enum class AbortedReason {
    // External code destructed this object before the sequence could complete.
    kSequenceDestroyed,
    // The starting element was hidden before the sequence started.
    kElementHiddenBeforeSequenceStart,
    // An element should have been visible at the start of a step but was not.
    kElementNotVisibleAtStartOfStep,
    // An element should have remained visible during a step but did not.
    kElementHiddenDuringStep,
    // The sequence was explicitly failed as part of a test.
    kFailedForTesting,
    // Update this if values are added to the enumeration.
    kMaxValue = kFailedForTesting
  };

  // Specifies how the context for a step is determined.
  enum class ContextMode {
    // Use the initial context for the sequence.
    kInitial,
    // Search for the element in any context. Currently can only apply to kShown
    // steps.
    kAny,
    // Inherits the context from the previous step. Cannot be used on the first
    // step in a sequence.
    kFromPreviousStep
  };

  // A step context is either an explicit context or a ContextMode.
  using StepContext = absl::variant<ElementContext, ContextMode>;

  // Callback when a step in the sequence starts. If |element| is no longer
  // available, it will be null.
  using StepStartCallback =
      base::OnceCallback<void(InteractionSequence* sequence,
                              TrackedElement* element)>;

  // Callback when a step in the sequence ends. If |element| is no longer
  // available, it will be null.
  using StepEndCallback = base::OnceCallback<void(TrackedElement* element)>;

  // Callback for when the user aborts the sequence by failing to follow the
  // sequence of steps, or if this object is deleted after the sequence starts.
  // The most recent event is described by the parameters; if the target element
  // is no longer available it will be null.
  //
  // The active step will be 0 before the sequence starts, and is incremented on
  // each step transition after the previous step's end callback is called, or
  // if the next step's precondition fails (so that it refers to the correct
  // step).
  using AbortedCallback = base::OnceCallback<void(int active_step,
                                                  TrackedElement* last_element,
                                                  ElementIdentifier last_id,
                                                  StepType last_step_type,
                                                  AbortedReason aborted_reason,
                                                  std::string description)>;

  using CompletedCallback = base::OnceClosure;

  struct Configuration;
  class StepBuilder;

  struct COMPONENT_EXPORT(UI_BASE) Step {
    Step();
    Step(const Step& other) = delete;
    void operator=(const Step& other) = delete;
    ~Step();

    bool uses_named_element() const { return !element_name.empty(); }

    StepType type = StepType::kShown;
    ElementIdentifier id;
    CustomElementEventType custom_event_type;
    std::string element_name;
    StepContext context = ContextMode::kInitial;

    // These will always have values when the sequence is built, but can be
    // unspecified during construction. If unspecified, they will be set to
    // appropriate defaults for `type`.
    absl::optional<bool> must_be_visible;
    absl::optional<bool> must_remain_visible;
    bool transition_only_on_event = false;

    StepStartCallback start_callback;
    StepEndCallback end_callback;
    ElementTracker::Subscription subscription;

    // Tracks the element associated with the step, if known. We could use a
    // SafeElementReference here, but there are cases where we want to do
    // additional processing if this element goes away, so we'll add the
    // listeners manually instead.
    raw_ptr<TrackedElement, DanglingUntriaged> element = nullptr;

    // Provides a useful description for debugging that can be read or passed
    // to the abort callback on failure.
    std::string description;
  };

  // Use a Builder to specify parameters when creating an InteractionSequence.
  class COMPONENT_EXPORT(UI_BASE) Builder {
   public:
    Builder();
    Builder(Builder&& other);
    Builder& operator=(Builder&& other);
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

    // Convenience methods to add a step when using a StepBuilder.
    Builder& AddStep(StepBuilder& step_builder);

    // Convenience method for cases where we don't have an lvalue.
    Builder& AddStep(StepBuilder&& step_builder);

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
    StepBuilder(StepBuilder&& other);
    StepBuilder& operator=(StepBuilder&& other);

    // Sets the unique identifier for this step. Either this or
    // SetElementName() is required for all step types except kCustomEvent.
    StepBuilder& SetElementID(ElementIdentifier element_id);

    // Sets the step to refer to a named element instead of an
    // ElementIdentifier. Either this or SetElementID() is required for all
    // step types other than kCustomEvent.
    StepBuilder& SetElementName(const base::StringPiece& name);

    // Sets the context for the step; useful for setting up the initial
    // element of the sequence if you do not know the context ahead of time, or
    // to specify that a step should not use the default context.
    StepBuilder& SetContext(StepContext context);

    // Sets the type of step. Required. You must set `event_type` if and only
    // if `step_type` is kCustomEvent.
    StepBuilder& SetType(
        StepType step_type,
        CustomElementEventType event_type = CustomElementEventType());

    // Indicates that the specified element must be visible at the start of the
    // step. Defaults to true for StepType::kActivated, false otherwise. Failure
    // To meet this condition will abort the sequence.
    StepBuilder& SetMustBeVisibleAtStart(bool must_be_visible);

    // Indicates that the specified element must remain visible throughout the
    // step once it has been shown. Defaults to true for StepType::kShown, false
    // otherwise (and incompatible with StepType::kHidden). Failure to meet this
    // condition will abort the sequence.
    StepBuilder& SetMustRemainVisible(bool must_remain_visible);

    // For kShown and kHidden events, if set to true, only allows a step
    // transition to happen when a "shown" or "hidden" event is received, and
    // not if an element is already visible (in the case of kShown steps) or no
    // elements are visible (in the case of kHidden steps).
    //
    // Default is false. Has no effect on kActiated events which are discrete
    // rather than stateful.
    //
    // Note: Does not track events fired during previous step's start callback,
    // so should not be used in automated interaction testing. The default
    // behavior should be fine for these cases.
    //
    // Note: Be careful when setting this value to true, as it increases the
    // likelihood of ending up in a state where a failure cannot be detected;
    // that is, waiting for an element to appear and then it... never does. In
    // this case, you will need an external way to terminate the sequence (a
    // timeout, user interaction, etc.)
    StepBuilder& SetTransitionOnlyOnEvent(bool transition_only_on_event);

    // Sets the callback called at the start of the step.
    StepBuilder& SetStartCallback(StepStartCallback start_callback);

    // Sets the callback called at the start of the step. Convenience method
    // that eliminates the InteractionSequence argument if you do not need it.
    StepBuilder& SetStartCallback(
        base::OnceCallback<void(TrackedElement*)> start_callback);

    // Sets the callback called at the start of the step. Convenience method
    // that eliminates both arguments if you do not need them.
    StepBuilder& SetStartCallback(base::OnceClosure start_callback);

    // Sets the callback called at the end of the step. Guaranteed to be called
    // if the start callback is called, before the start callback of the next
    // step or the sequence aborted or completed callback. Also called if this
    // object is destroyed while the step is still in-process.
    StepBuilder& SetEndCallback(StepEndCallback end_callback);

    // Sets the callback called at the end of the step. Convenience method if
    // you don't need the parameter.
    StepBuilder& SetEndCallback(base::OnceClosure end_callback);

    // Sets the description of the step.
    StepBuilder& SetDescription(const base::StringPiece& description);

    // Formats the existing description into a new string; allows for adding
    // modifiers to an existing description. `format_string` should contain
    // exactly one "%s".
    StepBuilder& FormatDescription(const base::StringPiece& format_string);

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
      StepStartCallback start_callback = StepStartCallback(),
      StepEndCallback end_callback = StepEndCallback());

  ~InteractionSequence();

  // Starts the sequence. All of the elements in the sequence must belong to the
  // same top-level application window (which includes menus, bubbles, etc.
  // associated with that window).
  void Start();

  // Starts the sequence and does not return until the sequence either
  // completes or aborts. Events on the current thread continue to be processed
  // while the method is waiting, so this will not e.g. block the browser UI
  // thread from handling inputs.
  //
  // This is a test-only method since production code applications should
  // always run asynchronously.
  void RunSynchronouslyForTesting();

  // Explicitly fails the sequence.
  void FailForTesting();

  // Assigns an element to a given name. The name is local to this interaction
  // sequence. It is valid for `element` to be null; in this case, we are
  // explicitly saying "there is no element with this name [yet]".
  //
  // It is safe to call this method from a step start callback, but not a step
  // end or aborted callback, as in the latter case the sequence might be in
  // the process of being destructed.
  void NameElement(TrackedElement* element, const base::StringPiece& name);

  // Retrieves a named element, which may be null if we specified "no element"
  // or if the element has gone away.
  //
  // It is safe to call this method from a step start callback, but not a step
  // end or aborted callback, as in the latter case the sequence might be in
  // the process of being destructed.
  TrackedElement* GetNamedElement(const base::StringPiece& name);
  const TrackedElement* GetNamedElement(const base::StringPiece& name) const;

 private:
  explicit InteractionSequence(std::unique_ptr<Configuration> configuration);

  // Callbacks from the ElementTracker.
  void OnElementShown(TrackedElement* element);
  void OnElementActivated(TrackedElement* element);
  void OnElementHidden(TrackedElement* element);
  void OnCustomEvent(TrackedElement* element);

  // Callbacks used only during step transitions to cache certain events.
  void OnTriggerDuringStepTransition(TrackedElement* element);
  void OnElementHiddenDuringStepTransition(TrackedElement* element);
  void OnElementHiddenWaitingForActivate(TrackedElement* element);

  // While we're transitioning steps, it's possible for an activation that
  // would trigger the following step to come in. This method adds a callback
  // that's valid only during the step transition to watch for this event.
  void MaybeWatchForTriggerDuringStepTransition();

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
  void Abort(AbortedReason reason);

  // Returns true (and does some sanity checking) if the sequence was aborted
  // during the most recent callback.
  bool AbortedDuringCallback() const;

  // Returns true if `name` is non-empty and `element` matches the element
  // with the specified name, or if `name` is empty (indicating we don't care
  // about it being a named element). Otherwise returns false.
  bool MatchesNameIfSpecified(const TrackedElement* element,
                              const base::StringPiece& name) const;

  // The following would be inline if not for the fact that the data that holds
  // the values is an implementation detail.

  // Returns the next step, or null if none.
  Step* next_step();

  // Returns the context for the current sequence.
  ElementContext context() const;

  // Updates the next step context from the current based on its StepContext.
  // Returns an element context if one is determined; null context if the step
  // allows any context.
  // Do not call for named elements.
  ElementContext UpdateNextStepContext();

  int active_step_index_ = 0;
  bool missing_first_element_ = false;
  bool started_ = false;
  bool trigger_during_callback_ = false;
  bool processing_step_ = false;
  std::unique_ptr<Step> current_step_;
  ElementTracker::Subscription next_step_hidden_subscription_;
  std::unique_ptr<Configuration> configuration_;
  std::map<std::string, SafeElementReference> named_elements_;
  base::OnceClosure quit_run_loop_closure_for_testing_;

  // This is necessary because this object could be deleted during any callback,
  // and we don't want to risk a UAF if that happens.
  base::WeakPtrFactory<InteractionSequence> weak_factory_{this};
};

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(InteractionSequence::StepType step_type, std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(InteractionSequence::AbortedReason reason,
                    std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::StepType step_type);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::AbortedReason reason);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
