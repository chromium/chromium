// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "base/types/is_instantiation.h"
#include "base/types/pass_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/framework_specific_registration_list.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/gfx/geometry/rect.h"

class ChromeOSTestLauncherDelegate;
class InteractiveUITestSuite;

namespace ui::test {

class InteractiveTestApi;
class InteractiveTestTest;

namespace internal {

// Element that is present during interactive tests that actions can bounce
// events off of.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

// Used by `PollUntil()`.
DECLARE_STATE_IDENTIFIER_VALUE(PollingStateObserver<bool>,
                               kInteractiveTestPollUntilState);

inline constexpr char kInteractiveTestFailedMessagePrefix[] =
    "Interactive test failed ";
inline constexpr char kNoCheckDescriptionSpecified[] =
    "[no description specified]";

class InteractiveTestPrivate;
class StateObserverElement;

// Represents a private test implementation for a particular framework or
// platform.
class InteractiveTestPrivateFrameworkBase
    : public FrameworkSpecificImplementation {
 public:
  explicit InteractiveTestPrivateFrameworkBase(
      InteractiveTestPrivate& test_impl);
  ~InteractiveTestPrivateFrameworkBase() override;

  // Represents a node in a debug tree of UI elements that can be pretty-
  // printed.
  struct DebugTreeNode {
    DebugTreeNode();
    explicit DebugTreeNode(std::string initial_text);
    DebugTreeNode(DebugTreeNode&& other) noexcept;
    DebugTreeNode& operator=(DebugTreeNode&& other) noexcept;
    ~DebugTreeNode();

    std::string text;
    std::vector<DebugTreeNode> children;

    void PrintTo(std::ostream& stream) const;
  };

  // Gets a verbose string representation of a set of `bounds` for debug
  // purposes.
  static std::string DebugDumpBounds(const gfx::Rect& bounds);

  // Called to populate any simulators required for this platform.
  virtual void PopulateSimulators(InteractionTestUtil& test_util) {}

  // Call this method during test SetUp(), or SetUpOnMainThread() for browser
  // tests.
  virtual void DoTestSetUp() {}

  // Call this method during test TearDown(), or TearDownOnMainThread() for
  // browser tests.
  virtual void DoTestTearDown() {}

  // Called when the sequence ends, but before we break out of the run loop
  // in RunTestSequenceImpl().
  virtual void OnSequenceComplete() {}
  virtual void OnSequenceAborted(const InteractionSequence::AbortedData& data) {
  }

  // Retrieves the native window from `el`. If this particular implementation
  // does not know how to do this, or there is no window, returns a null/falsy
  // value.
  virtual gfx::NativeWindow GetNativeWindowFromElement(
      const TrackedElement* el) const;

  // Retrieves the native window from `context`. If this particular
  // implementation does not know how to do this, or there is no window, returns
  // a null/falsy value.
  virtual gfx::NativeWindow GetNativeWindowFromContext(
      ElementContext context) const;

  // Convert some or all of `elements` to debug tree nodes; removing elements
  // that are processed from the set.
  virtual std::vector<DebugTreeNode> DebugDumpElements(
      std::set<const ui::TrackedElement*>& elements) const;

  // Provides the top-level description for a context, or null if none.
  virtual std::string DebugDescribeContext(ui::ElementContext context) const;

 protected:
  InteractiveTestPrivate& test_impl() { return test_impl_.get(); }

 private:
  const raw_ref<InteractiveTestPrivate> test_impl_;
};

// Class that implements functionality for InteractiveTest* that should be
// hidden from tests that inherit the API.
class InteractiveTestPrivate {
 public:
  using MultiStep = internal::MultiStep;

  // Describes what should happen when an action isn't compatible with the
  // current build, platform, or environment. For example, not all tests are set
  // up to handle screenshots, and some Linux window managers cannot bring a
  // background window to the front.
  //
  // See chrome/test/interaction/README.md for best practices.
  enum class OnIncompatibleAction {
    // The test should fail. This is the default, and should be used in almost
    // all cases.
    kFailTest,
    // The sequence should abort immediately and the test should be skipped.
    // Use this when the remainder of the test would depend on the result of the
    // incompatible step. Good for smoke/regression tests that have known
    // incompatibilities but still need to be run in as many environments as
    // possible.
    kSkipTest,
    // As `kSkipTest`, but instead of marking the test as skipped, just stops
    // the test sequence. This is useful when the test cannot continue past the
    // problematic step, but you also want to preserve any non-fatal errors that
    // may have occurred up to that point (or check any conditions after the
    // test stops).
    kHaltTest,
    // The failure should be ignored and the test should continue.
    // Use this when the step does not affect the outcome of the test, such as
    // taking an incidental screenshot in a test job that doesn't support
    // screenshots.
    kIgnoreAndContinue,
  };

  // Provides a copyable handle to some test state that can be output in the
  // event of a test failure. The context will persist until `End()` is called
  // or the test ends.
  //
  // Example:
  // ```
  //   auto MyVerb() {
  //     AdditionalContext context = CreateAdditionalContext();
  //     return Steps(
  //
  //       // Set the context. Note the use of the `mutable` keyword:
  //       AfterShow(..., [context]() mutable {
  //         context.Set(...);
  //       }),
  //
  //       // Context is still valid here, even if it's not modified.
  //       WithElement(..., [](ui::TrackedElement*) {
  //         ...
  //       }),
  //
  //       Do([context]() { context.End(); })
  //
  //       // Since no more steps reference `context` it is no longer valid
  //       // here; if the test were to fail, no additional information would
  //       // be printed.
  //       PressButton(...));
  //   }
  // ```
  class AdditionalContext {
   public:
    AdditionalContext();
    AdditionalContext(const AdditionalContext& other);
    AdditionalContext& operator=(const AdditionalContext& other);
    ~AdditionalContext();

    // Adds or replaces the existing value with `additional_context`. Until this
    // is called, nothing will be stored or output.
    void Set(const std::string_view& additional_context);

    // Fetches the current value of the context.
    std::string Get() const;

    // Removes the context.
    void Clear();

   private:
    friend InteractiveTestPrivate;

    // Creates a new context with the given `owner` and `handle`.
    AdditionalContext(InteractiveTestPrivate& owner, intptr_t handle);

    base::WeakPtr<InteractiveTestPrivate> owner_;
    intptr_t handle_ = 0;
  };

  InteractiveTestPrivate();
  virtual ~InteractiveTestPrivate();
  InteractiveTestPrivate(const InteractiveTestPrivate&) = delete;
  void operator=(const InteractiveTestPrivate&) = delete;

  InteractionTestUtil& test_util() { return test_util_; }

  OnIncompatibleAction on_incompatible_action() const {
    return on_incompatible_action_;
  }

  bool sequence_skipped() const { return sequence_skipped_; }

  base::WeakPtr<InteractiveTestPrivate> GetAsWeakPtr();

  void set_default_context(ElementContext default_context) {
    default_context_ = default_context;
  }
  ElementContext default_context() const { return default_context_; }

  // Fetch the native window for the given element.
  gfx::NativeWindow GetNativeWindowFor(const ui::TrackedElement* el) const;

  // Possibly fails or skips a sequence based on the result of an action
  // simulation.
  void HandleActionResult(InteractionSequence* seq,
                          const TrackedElement* el,
                          const std::string& operation_name,
                          ActionResult result);

  // Gets the pivot element for the specified context, which must exist.
  TrackedElement* GetPivotElement(ElementContext context) const;

  // Adds `state_observer` and associates it with an element with identifier
  // `id` and context `context`. Must be unique in its context.
  // Returns true on success.
  template <typename Observer, typename V = Observer::ValueType>
  bool AddStateObserver(ElementIdentifier id,
                        ElementContext context,
                        std::unique_ptr<Observer> state_observer);

  // Removes `StateObserver` with identifier `id` in `context`; if the context
  // is null, assumes there is exactly one matching observer in some context.
  // Returns true on success.
  bool RemoveStateObserver(ElementIdentifier id, ElementContext context);

  // Creates an additional context that will persist as long as copies of the
  // context exist.
  [[nodiscard]] AdditionalContext CreateAdditionalContext();

  // Gets a string representation of the current additional context for this
  // test.
  std::vector<std::string> GetAdditionalContext() const;

  // Call this method during test SetUp(), or SetUpOnMainThread() for browser
  // tests.
  virtual void DoTestSetUp();

  // Call this method during test TearDown(), or TearDownOnMainThread() for
  // browser tests.
  virtual void DoTestTearDown();

  // Called when the sequence ends, but before we break out of the run loop
  // in RunTestSequenceImpl().
  virtual void OnSequenceComplete();
  virtual void OnSequenceAborted(const InteractionSequence::AbortedData& data);

  // Sets a callback that is called if the test sequence fails instead of
  // failing the current test. Should only be called in tests that are testing
  // InteractiveTestApi or descendant classes.
  void set_aborted_callback_for_testing(
      InteractionSequence::AbortedCallback aborted_callback_for_testing) {
    aborted_callback_for_testing_ = std::move(aborted_callback_for_testing);
  }

  // The following are the classes allowed to set the "allow interactive test
  // verbs" flag.
  template <typename T>
    requires std::same_as<T, ui::test::InteractiveTestTest> ||
             std::same_as<T, ChromeOSTestLauncherDelegate> ||
             std::same_as<T, InteractiveUITestSuite>
  static void set_interactive_test_verbs_allowed(base::PassKey<T>) {
    allow_interactive_test_verbs_ = true;
  }

  using DebugTreeNode = InteractiveTestPrivateFrameworkBase::DebugTreeNode;

  template <typename T, typename... Args>
    requires std::derived_from<T, InteractiveTestPrivateFrameworkBase>
  T* MaybeRegisterFrameworkImpl(Args&&... args) {
    T* const result = framework_implementations_.MaybeRegister<T>(
        *this, std::forward<Args>(args)...);
    if (result) {
      result->PopulateSimulators(test_util_);
    }
    return result;
  }

 protected:
  // Dumps the entire tree of named elements. Default implementation organizes
  // all elements by context. This is the entry point when printing test failure
  // information. The `current_context` is the current context in the test, if
  // known.
  DebugTreeNode DebugDumpElements(ui::ElementContext current_context) const;

  // Dumps the contents of a particular context.
  virtual DebugTreeNode DebugDumpContext(
      const ui::ElementContext context) const;

 private:
  friend class ui::test::InteractiveTestTest;
  friend class ui::test::InteractiveTestApi;

  // Prepare for a sequence to start.
  void Init(ElementContext initial_context);

  // Clean up after a sequence.
  void Cleanup();

  // Note when a new element appears; we may update the context list.
  void OnElementAdded(TrackedElement* el);

  // Maybe adds a pivot element for the given context.
  void MaybeAddPivotElement(ElementContext context);

  // Tracks whether a sequence succeeded or failed.
  bool success_ = false;

  // Specifies how an incompatible action should be handled.
  OnIncompatibleAction on_incompatible_action_ =
      OnIncompatibleAction::kFailTest;
  std::string on_incompatible_action_reason_;

  // Tracks whether a sequence is skipped. Will only be set if
  // `skip_on_unsupported_operation` is true.
  bool sequence_skipped_ = false;

  // Used to simulate input to UI elements.
  InteractionTestUtil test_util_;

  // The default context for running test sequences.
  ElementContext default_context_;

  // Used to keep track of valid contexts.
  base::CallbackListSubscription context_subscription_;

  // Used to track state observers and their associated elements.
  std::vector<std::unique_ptr<StateObserverElement>> state_observer_elements_;

  // Used to relay events to trigger follow-up steps.
  std::map<ElementContext, std::unique_ptr<TrackedElement>> pivot_elements_;

  // Overrides the default test failure behavior to test the API itself.
  InteractionSequence::AbortedCallback aborted_callback_for_testing_;

  intptr_t next_additional_context_handle_ = 1U;
  std::map<intptr_t, std::string> additional_context_data_;

  FrameworkSpecificRegistrationList<InteractiveTestPrivateFrameworkBase>
      framework_implementations_;

  base::WeakPtrFactory<InteractiveTestPrivate> weak_ptr_factory_{this};

  // Whether interactive test verbs are allowed. See
  // `InteractiveTestApi::RequireInteractiveTest()` for more info.
  static bool allow_interactive_test_verbs_;
};

class StateObserverElement : public TestElementBase {
 public:
  StateObserverElement(ElementIdentifier id, ElementContext context);
  ~StateObserverElement() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

// Implements an element that is shown when an observed state matches a desired
// value or pattern, and hidden when it does not.
template <typename T>
class StateObserverElementT : public StateObserverElement {
 public:
  // A lookup table is provided per value of `T`.
  using LookupTable = std::map<std::pair<ElementIdentifier, ElementContext>,
                               StateObserverElementT<T>*>;
  using TestContext = InteractiveTestPrivate::AdditionalContext;

  // Specify the `id` and `context` of the element to be created, as well as the
  // associated `observer` which will be linked to this element.
  StateObserverElementT(ElementIdentifier id,
                        ElementContext context,
                        std::unique_ptr<StateObserver<T>> observer,
                        TestContext test_context)
      : StateObserverElement(id, context),
        test_context_(test_context),
        current_value_(observer->GetStateObserverInitialState()),
        observer_(std::move(observer)) {
    auto& table = GetLookupTable();
    CHECK(!base::Contains(table, std::make_pair(id, context)))
        << "Duplicate ID + context for StateObserver not allowed: " << id
        << ", " << context;
    table.emplace(std::make_pair(id, context), this);
    observer_->SetStateObserverStateChangedCallback(base::BindRepeating(
        &StateObserverElementT::OnStateChanged, base::Unretained(this)));
    OnStateChanged(current_value_);
  }
  ~StateObserverElementT() override {
    CHECK(GetLookupTable().erase(std::make_pair(identifier(), context())));
  }

  void SetTarget(testing::Matcher<T> target) {
    target_value_ = std::move(target);
    UpdateVisibility();
  }

  // Helper method that looks up an element based on `id`, `context`, and
  // whether `seq` allows all contexts to be searched. Fails the sequence if the
  // element is not found.
  static StateObserverElementT<T>* LookupElement(ElementIdentifier id,
                                                 ElementContext context,
                                                 bool search_all_contexts) {
    const auto& lookup_table = GetLookupTable();
    const auto it = lookup_table.find(std::make_pair(id, context));
    if (it != lookup_table.end()) {
      return it->second;
    }

    if (search_all_contexts) {
      for (const auto& [key, ptr] : lookup_table) {
        if (key.first == id) {
          return ptr;
        }
      }
    }

    return nullptr;
  }

  const T& current_value() const { return current_value_; }

 private:
  void OnStateChanged(T new_state) {
    current_value_ = new_state;
    UpdateVisibility();
  }

  void UpdateVisibility() {
    testing::StringMatchResultListener listener;
    if (target_value_ &&
        target_value_->MatchAndExplain(current_value_, &listener)) {
      test_context_.Clear();
      Show();
    } else {
      std::ostringstream oss;
      oss << "Waiting for state " << identifier() << " " << listener.str();
      test_context_.Set(oss.str());
      Hide();
    }
  }

  // Fetch the lookup table associated with a value type/template instantiation.
  //
  // This table does not own the instances, just tracks them as long as they are
  // alive and allows them to be retrieved. There is one static table per
  // template instantiation due to the use of `base::NoDestructor`,
  static LookupTable& GetLookupTable() {
    static base::NoDestructor<LookupTable> lookup_table;
    return *lookup_table;
  }

 private:
  // Since the context can be updated on observer shutdown and needs access to
  // the current value, it needs to be destructed last.
  TestContext test_context_;
  T current_value_;
  std::optional<testing::Matcher<T>> target_value_;
  std::unique_ptr<StateObserver<T>> observer_;
};

// Applies `matcher` to `value` and returns the result; on failure a useful
// error message is printed using `test_name`, `value`, and `matcher`.
//
// Steps which use this method will fail if it returns false, printing out the
// details of the step in the usual way.
template <typename T, typename V = std::decay_t<T>>
bool MatchAndExplain(std::string_view test_name,
                     const testing::Matcher<V>& matcher,
                     const T& value) {
  testing::StringMatchResultListener listener;
  if (matcher.MatchAndExplain(value, &listener)) {
    return true;
  }
  std::ostringstream oss;
  oss << test_name << " failed.\nExpected: ";
  matcher.DescribeTo(&oss);
  oss << "\nActual: " << testing::PrintToString(value);
  if (!listener.str().empty()) {
    oss << "\n" << listener.str();
  }
  LOG(ERROR) << oss.str();
  return false;
}

template <typename Observer, typename V>
bool InteractiveTestPrivate::AddStateObserver(
    ElementIdentifier id,
    ElementContext context,
    std::unique_ptr<Observer> state_observer) {
  CHECK(id);
  CHECK(context);
  for (const auto& existing : state_observer_elements_) {
    if (existing->identifier() == id && existing->context() == context) {
      LOG(ERROR) << "AddStateObserver: Duplicate observer added for " << id;
      return false;
    }
  }
  state_observer_elements_.emplace_back(
      std::make_unique<StateObserverElementT<V>>(
          id, context, std::move(state_observer), CreateAdditionalContext()));
  return true;
}

}  // namespace internal

}  // namespace ui::test

inline ui::test::internal::MultiStep& operator+=(
    ui::test::internal::MultiStep& steps,
    ui::InteractionSequence::StepBuilder&& step) {
  steps.push_back(std::move(step));
  return steps;
}

inline ui::test::internal::MultiStep& operator+=(
    ui::test::internal::MultiStep& steps,
    ui::test::internal::MultiStep&& other) {
  std::ranges::move(other, std::back_inserter(steps));
  return steps;
}

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
