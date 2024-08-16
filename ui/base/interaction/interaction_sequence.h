// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
#define UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
    // user (for buttons or menu items, being clicked). If the element goes away
    // before the step start callback can be called, null will be passed to the
    // step start callback (you can avoid this by setting step start callback
    // mode to immediate).
    kActivated,
    // Represents an element with the specified ID becoming hidden or destroyed,
    // or no elements with the specified ID being visible. If there is no
    // matching element or the element disappears before the start callback can
    // be called, null will be passed to the start callback.
    kHidden,
    // Represents a custom event with a specific custom event type. You may
    // further specify a required element name or ID to filter down which
    // events you actually want to step on vs. ignore.
    kCustomEvent,
    // Represents one or more nested, conditional subsequences. An element may
    // be provided for use in `SubsequenceCondition` checks. See
    // `SubsequenceMode` for more information on how subsequences work.
    //
    // Note that while a subsequence step can have an element name or ID, it is
    // not required. Furthermore, the element will be located at the start of
    // the step and if it is not present, null will be passed to the
    // SubsequenceCondition (unless must_be_visible is true, in which case the
    // step will fail). This allows subsequences to be conditional on the
    // presence of an element.
    //
    // Known limitations:
    // - If the triggering condition for the step following this one occurs
    //   during execution of one of the subsequences, it may be missed/lost.
    // - If there is no element specified, or the element does not exist, then
    //   the following step will not be able to effectively use
    //   ContextMode::kFromPreviousStep.
    kSubsequence,
    kMaxValue = kSubsequence
  };

  // Describes how the subsequences in a `StepType::kSubsequence` step are
  // executed.
  enum class SubsequenceMode {
    // The first subsequence whose condition is met is executed, and the step
    // finishes if the subsequence completes. If no subsequences run, the step
    // succeeds.
    kAtMostOne,
    // The first subsequence whose condition is met is executed, and the step
    // finishes if the subsequence completes. If no subsequences run, the step
    // fails.
    kExactlyOne,
    // All subsequences whose conditions are met are executed, and the step
    // finishes if any of the subsequences completes successfully. The others
    // may fail, and are destroyed immediately as soon as the first succeeds. If
    // no sequences run, the step fails.
    kAtLeastOne,
    // All subsequences whose conditions are met are executed, and the step
    // finishes if all of the subsequences complete. If no sequences run, the
    // step succeeds.
    //
    // This is the default behavior.
    kAll,
    kMaxValue = kAll
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
    // An element was hidden during an asynchronous step between trigger and
    // step start.
    kElementHiddenBetweenTriggerAndStepStart,
    // One or more subsequences were expected to run, but none could due to
    // failed preconditions.
    kNoSubsequenceRun,
    // One or more subsequences needed to succeed, but one or more unexpectedly
    // failed. Details will be the failed step from the first subsequence that
    // should have completed but did not.
    kSubsequenceFailed,
    // The sequence was explicitly failed as part of a test.
    kFailedForTesting,
    // A timeout was reached during execution. This might not be fatal, but the
    // current state can be dumped regardless.
    kSequenceTimedOut,
    // A discrete triggering condition for the following step happened before
    // the start callback for the current callback was even run.
    kSubsequentStepTriggeredTooEarly,
    // A triggering condition for the following step happened while waiting for
    // the previous step to complete, but then the state changed in such a way
    // that the trigger became invalid.
    kSubsequentStepTriggerInvalidated,
    // Update this if values are added to the enumeration.
    kMaxValue = kSubsequentStepTriggerInvalidated
  };

  // Specifies how the context for a step is determined.
  enum class ContextMode {
    // Use the initial context for the sequence.
    kInitial,
    // Search for the element in any context.
    //
    // Note that these events are sent after events for specific contexts, if
    // you have two steps in succession that are both `StepType::kActivated`
    // with the second one having `ContextMode::kAny`, then the same activation
    // could conceivably trigger both steps. Fortunately, this sort of thing
    // doesn't happen in any of the real-world use cases; if it does it will be
    // fixed with special case code.
    kAny,
    // Inherits the context from the previous step. Cannot be used on the first
    // step in a sequence.
    kFromPreviousStep
  };

  // Determines how each step's start callback executes - and by extension, when
  // the following step is staged for execution.
  enum class StepStartMode {
    // The start callback will be posted and execute on a fresh call stack,
    // after all immediately-pending tasks. Even if there is no start callback,
    // staging of the following step will be done on that fresh call stack,
    // preventing multiple steps from cascading on the same triggering callback.
    // This is the default.
    kAsynchronous,
    // The start callback will execute immediately as soon as the step trigger
    // is detected. This could be in the middle of an operation, such as a set
    // of UI elements being shown together. Use this only when waiting even a
    // small amount of time would cause a problem (which is almost never).
    kImmediate
  };

  // Determines whether a subsequence will run. `seq` is the parent sequence,
  // and `el` is the reference element, and may be null if the element is not
  // specified or if there is no matching element. This is unlike other steps
  // where an element is typically required to be present before the step can
  // proceed.
  using SubsequenceCondition =
      base::OnceCallback<bool(const InteractionSequence* seq,
                              const TrackedElement* el)>;

  // Returns a callback that causes the subsequence to always run.
  static SubsequenceCondition AlwaysRun();

  // A step context is either an explicit context or a ContextMode.
  using StepContext = std::variant<ElementContext, ContextMode>;

  // Callback when a step in the sequence starts. If |element| is no longer
  // available, it will be null.
  using StepStartCallback =
      base::OnceCallback<void(InteractionSequence* sequence,
                              TrackedElement* element)>;

  // Callback when a step in the sequence ends. If |element| is no longer
  // available, it will be null.
  using StepEndCallback = base::OnceCallback<void(TrackedElement* element)>;

  // Information passed when a sequence fails or is aborted.
  struct COMPONENT_EXPORT(UI_BASE) AbortedData {
    AbortedData();
    ~AbortedData();
    AbortedData(const AbortedData& other);
    AbortedData& operator=(const AbortedData& other);

    // The index of the step where the failure occurred. 0 before the sequence
    // starts, and is incremented on each step transition after the previous
    // step's end callback is called, or if the next step's precondition fails
    // (so that it refers to the correct step).
    int step_index = 0;

    // The description of the failed step.
    std::string step_description;

    // The step type of the failed step.
    StepType step_type = StepType::kShown;

    // A reference to the element used by the failed step. This is a weak
    // reference and may be null if the element was hidden or destroyed.
    SafeElementReference element;

    // The identifier of the element used by the failed step.
    ElementIdentifier element_id;

    // The reason the step failed/the sequence was aborted.
    AbortedReason aborted_reason = AbortedReason::kSequenceDestroyed;

    // If this failure was due to a subsequence failing, the failure information
    // for the subsequences will be stored here.
    //
    // This also stores the next step when a step fails due to e.g. an element
    // losing visibility.
    std::vector<std::optional<AbortedData>> subsequence_failures;
  };

  // Callback for when the user aborts the sequence by failing to follow the
  // sequence of steps, or if this object is deleted after the sequence starts,
  // or when the sequence fails for some other reason.
  //
  // The most recent step is described by the `AbortedData` block.
  using AbortedCallback = base::OnceCallback<void(const AbortedData&)>;

  using CompletedCallback = base::OnceClosure;

  struct Configuration;
  class StepBuilder;
  struct SubsequenceData;

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

    // This is used for testing; while `context` can be updated as part of
    // sequence execution, this will never change.
    bool in_any_context = false;

    // These will always have values when the sequence is built, but can be
    // unspecified during construction. If unspecified, they will be set to
    // appropriate defaults for `type`.
    std::optional<bool> must_be_visible;
    std::optional<bool> must_remain_visible;
    bool transition_only_on_event = false;

    StepStartCallback start_callback;
    std::optional<StepStartMode> step_start_mode;
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

    // These only apply if the type of the step is kSubsequence.
    SubsequenceMode subsequence_mode = SubsequenceMode::kAll;
    std::vector<SubsequenceData> subsequence_data;
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

    // Sets the default step start mode for this sequence. This will acquire
    // a default value if not set; for subsequences, the value of the parent
    // sequence is inherited instead if no value is set.
    Builder& SetDefaultStepStartMode(StepStartMode step_start_mode);

    // Creates the InteractionSequence. You must call Start() to initiate the
    // sequence; sequences cannot be re-used, and a Builder is no longer valid
    // after Build() is called.
    std::unique_ptr<InteractionSequence> Build();

   private:
    friend class InteractionSequence;

    std::unique_ptr<InteractionSequence> BuildSubsequence(
        const Configuration* owner_config,
        const Step* owning_step);

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
    StepBuilder& SetElementName(std::string_view name);

    // Sets the context for the step; useful for setting up the initial
    // element of the sequence if you do not know the context ahead of time, or
    // to specify that a step should not use the default context.
    StepBuilder& SetContext(StepContext context);

    // Sets the type of step. Required. You must set `event_type` if and only
    // if `step_type` is kCustomEvent.
    StepBuilder& SetType(
        StepType step_type,
        CustomElementEventType event_type = CustomElementEventType());

    // Changes the subsequence mode from the default. See `SubsequenceMode` for
    // details. Implicitly sets the step type to kSubsequence.
    StepBuilder& SetSubsequenceMode(SubsequenceMode subsequence_mode);

    // Adds a subsequence to the step. The subsequence will run if `condition`
    // returns true. Implicitly changes the step type to kSubsequence.
    //
    // The subsequence will not actually be built until it is needed. It will
    // inherit the named elements of its parent unless otherwise specified.
    StepBuilder& AddSubsequence(Builder subsequence,
                                SubsequenceCondition condition = AlwaysRun());

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

    // Sets the step start mode for this step. If not set, inherits the default
    // mode from its sequence.
    StepBuilder& SetStepStartMode(StepStartMode step_start_mode);

    // Sets the callback called at the end of the step. Guaranteed to be called
    // if the start callback is called, before the start callback of the next
    // step or the sequence aborted or completed callback. Also called if this
    // object is destroyed while the step is still in-process.
    StepBuilder& SetEndCallback(StepEndCallback end_callback);

    // Sets the callback called at the end of the step. Convenience method if
    // you don't need the parameter.
    StepBuilder& SetEndCallback(base::OnceClosure end_callback);

    // Sets the description of the step.
    StepBuilder& SetDescription(std::string_view description);

    // Formats the existing description into a new string; allows for adding
    // modifiers to an existing description. `format_string` should contain
    // exactly one "%s".
    StepBuilder& FormatDescription(std::string_view format_string);

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

  // Returns whether the current step uses ContextMode::kAny.
  bool IsCurrentStepInAnyContextForTesting() const;

  // Explicitly fails the sequence.
  void FailForTesting();

  // Assigns an element to a given name. The name is local to this interaction
  // sequence. It is valid for `element` to be null; in this case, we are
  // explicitly saying "there is no element with this name [yet]".
  //
  // It is safe to call this method from a step start callback, but not a step
  // end or aborted callback, as in the latter case the sequence might be in
  // the process of being destructed.
  void NameElement(TrackedElement* element, std::string_view name);

  // Retrieves a named element, which may be null if we specified "no element"
  // or if the element has gone away.
  //
  // It is safe to call this method from a step start callback, but not a step
  // end or aborted callback, as in the latter case the sequence might be in
  // the process of being destructed.
  TrackedElement* GetNamedElement(const std::string& name);
  const TrackedElement* GetNamedElement(const std::string& name) const;

  // Builds aborted data for the current step and the given reason.
  AbortedData BuildAbortedData(AbortedReason reason) const;

  // Gets a weak pointer to this object.
  base::WeakPtr<InteractionSequence> AsWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(InteractionSequenceSubsequenceTest, NamedElements);

  // Describes the state of the sequence.
  enum State {
    // [Sub]sequence waiting to be started.
    kNotStarted,
    // No transition is currently in progress.
    kIdle,
    // The end callback of the previous step is running.
    kInEndCallback,
    // The next step has been loaded, and the sequence is preparing to run the
    // new step's start callback.
    kWaitingForStartCallback,
    // The start callback for the new step is running.
    kInStartCallback,
  };

  explicit InteractionSequence(std::unique_ptr<Configuration> configuration,
                               const Step* reference_step);

  // Callbacks from the ElementTracker.
  void OnElementShown(TrackedElement* element);
  void OnElementActivated(TrackedElement* element);
  void OnElementHidden(TrackedElement* element);
  void OnCustomEvent(TrackedElement* element);

  // Callbacks used only during step transitions to cache certain events.
  void OnTriggerDuringStepTransition(TrackedElement* element);
  void OnElementHiddenDuringStepTransition(TrackedElement* element);
  void OnElementHiddenWaitingForEvent(TrackedElement* element);

  // While we're transitioning steps or staging a subsequence, it's possible for
  // an activation that would trigger the following step to come in. This method
  // adds a callback that's valid only during the step transition to watch for
  // this event.
  void MaybeWatchForEarlyTrigger(const Step* current_step);

  // A note on the next three methods - DoStepTransition(), StageNextStep(), and
  // Abort(): To prevent re-entrancy issues, they must always be the final call
  // in any method before it returns. This greatly simplifies the consistency
  // checks and safeguards that need to be put into place to make sure we aren't
  // making contradictory changes to state or calling callbacks in the wrong
  // order.

  // Start the transition from the current step to the next step.
  void StartStepTransition(TrackedElement* element);

  // Finish the transition from the current step to the next step.
  void CompleteStepTransition();

  // Looks at the next step to determine what needs to be done. Called at the
  // start of the sequence and after each subsequent step starts.
  void StageNextStep();

  // Cancels the sequence and cleans up.
  void Abort(AbortedReason reason);

  // Returns true if `name` is non-empty and `element` matches the element
  // with the specified name, or if `name` is empty (indicating we don't care
  // about it being a named element). Otherwise returns false.
  bool MatchesNameIfSpecified(const TrackedElement* element,
                              const std::string& name) const;

  // Returns the next step, or null if none.
  Step* next_step();
  const Step* next_step() const;

  // Returns the context for the current sequence.
  ElementContext context() const;

  // Updates the next step context from the current based on its StepContext.
  // Returns an element context if one is determined; null context if the step
  // allows any context.
  // Do not call for named elements.
  ElementContext UpdateNextStepContext(const Step* current_step);

  // Callbacks for when subsequences terminate.
  using SubsequenceHandle = const void*;
  void OnSubsequenceCompleted(SubsequenceHandle subsequence);
  void OnSubsequenceAborted(SubsequenceHandle subsequence,
                            const AbortedData& aborted_data);
  void BuildSubsequences(const Step* current_step);
  SubsequenceData* FindSubsequenceData(SubsequenceHandle subsequence);

  State state_ = State::kNotStarted;
  int active_step_index_ = 0;
  bool missing_first_element_ = false;
  bool trigger_during_callback_ = false;
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
extern void PrintTo(InteractionSequence::SubsequenceMode mode,
                    std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(InteractionSequence::StepStartMode mode, std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(const InteractionSequence::AbortedData& aborted_data,
                    std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::StepType step_type);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::AbortedReason reason);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::StepStartMode mode);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::SubsequenceMode mode);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(
    std::ostream& os,
    const InteractionSequence::AbortedData& aborted_data);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_INTERACTION_SEQUENCE_H_
