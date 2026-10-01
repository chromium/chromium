// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test_internal.h"

#include <cstdlib>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <string_view>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#elif BUILDFLAG(IS_ANDROID)
#include "ui/android/window_android.h"
#elif USE_AURA
#include "ui/aura/window.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "ui/native_window_tracker/native_window_tracker.h"
#endif

namespace ui::test::internal {

namespace {

// Basic implementation of the framework for dumping generic elements.
class InteractiveTestPrivateFrameworkImpl
    : public InteractiveTestPrivateFrameworkBase {
 public:
  DECLARE_SAFE_CAST_TARGET()

  explicit InteractiveTestPrivateFrameworkImpl(
      InteractiveTestPrivate& test_impl)
      : InteractiveTestPrivateFrameworkBase(test_impl) {}
  ~InteractiveTestPrivateFrameworkImpl() override = default;

  std::vector<DebugTreeNode> DebugDumpElements(
      std::set<const ui::TrackedElement*>& elements) const override {
    std::vector<DebugTreeNode> nodes;
    for (auto* el : elements) {
      if (el->identifier() == kInteractiveTestPivotElementId) {
        nodes.insert(nodes.begin(),
                     DebugTreeNode("Pivot element (part of test automation)"));
      } else {
        nodes.emplace_back(
            base::StringPrintf("%s - %s at %s", el->GetSafeCastableClassName(),
                               el->identifier().GetName().c_str(),
                               DebugDumpBounds(el->GetScreenBounds())));
      }
    }
    elements.clear();
    return nodes;
  }

  std::string DebugDescribeContext(ui::ElementContext context) const override {
    std::ostringstream oss;
    oss << context;
    return oss.str();
  }

  gfx::NativeWindow GetNativeWindowFromElement(
      const TrackedElement* el) const override {
#if BUILDFLAG(IS_MAC)
    return InteractionTestUtilMac::GetNativeWindowFor(el);
#elif BUILDFLAG(IS_ANDROID)
    const auto view = el->GetNativeView();
    return view ? view->GetWindowAndroid() : gfx::NativeWindow();
#elif USE_AURA
    const auto view = el->GetNativeView();
    return view ? view->GetToplevelWindow() : gfx::NativeWindow();
#else
    return gfx::NativeWindow();
#endif
  }
};

DEFINE_SAFE_CAST_TARGET(InteractiveTestPrivateFrameworkImpl)

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);
DEFINE_STATE_IDENTIFIER_VALUE(PollingStateObserver<bool>,
                              kInteractiveTestPollUntilState);

// Caches the last-known native window associated with a context.
// Useful for executing ClickMouse() and ReleaseMouse() commands, as no target
// element is provided for those commands. A NativeWindowTracker is used to
// prevent using a cached value after the native window has been destroyed.
class InteractiveTestPrivate::NativeWindowReference {
 public:
  NativeWindowReference() = default;
  ~NativeWindowReference() = default;
  NativeWindowReference(NativeWindowReference&& other) = default;
  NativeWindowReference& operator=(NativeWindowReference&& other) = default;

  bool IsValid() const {
#if BUILDFLAG(IS_IOS)
    // iOS uses a weak reference already.
    return static_cast<bool>(window_);
#else
    return window_ && tracker_ && !tracker_->WasNativeWindowDestroyed();
#endif
  }

  gfx::NativeWindow GetWindow() const {
    return IsValid() ? window_ : gfx::NativeWindow();
  }

  void SetWindow(gfx::NativeWindow window) {
    if (window_ == window) {
      return;
    }
    window_ = window;
#if !BUILDFLAG(IS_IOS)
    tracker_ = window ? ui::NativeWindowTracker::Create(window) : nullptr;
#endif
  }

 private:
  gfx::NativeWindow window_ = gfx::NativeWindow();
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<ui::NativeWindowTracker> tracker_;
#endif
};

StateObserverElement::StateObserverElement(ElementIdentifier id,
                                           ElementContext context)
    : TestElementBase(id, context) {}

StateObserverElement::~StateObserverElement() = default;

DEFINE_SAFE_CAST_TARGET(StateObserverElement)

// static
bool InteractiveTestPrivate::allow_interactive_test_verbs_ = false;

InteractiveTestPrivate::AdditionalContext::AdditionalContext() = default;

InteractiveTestPrivate::AdditionalContext::AdditionalContext(
    InteractiveTestPrivate& owner,
    intptr_t handle)
    : owner_(owner.GetAsWeakPtr()), handle_(handle) {
  CHECK(handle);
}

InteractiveTestPrivate::AdditionalContext::AdditionalContext(
    const AdditionalContext& other) = default;

InteractiveTestPrivate::AdditionalContext&
InteractiveTestPrivate::AdditionalContext::operator=(
    const AdditionalContext& other) = default;

InteractiveTestPrivate::AdditionalContext::~AdditionalContext() = default;

void InteractiveTestPrivate::AdditionalContext::Set(
    const std::string_view& additional_context) {
  auto* const owner = owner_.get();
  CHECK(owner) << "Set() should never be executed after destruction of the "
                  "owning sequence.";
  CHECK(handle_)
      << "Set() should never be executed on a default-constructed object.";
  owner_->additional_context_data_[handle_] = additional_context;
}

std::string InteractiveTestPrivate::AdditionalContext::Get() const {
  auto* const owner = owner_.get();
  CHECK(owner) << "Set() should never be executed after destruction of the "
                  "owning sequence.";
  CHECK(handle_)
      << "Set() should never be executed on a default-constructed object.";
  const auto it = owner_->additional_context_data_.find(handle_);
  return (it != owner_->additional_context_data_.end()) ? it->second
                                                        : std::string();
}

void InteractiveTestPrivate::AdditionalContext::Clear() {
  auto* const owner = owner_.get();
  CHECK(owner) << "Clear() should never be executed after destruction of the "
                  "owning sequence.";
  CHECK(handle_)
      << "Clear() should never be executed on a default-constructed object.";
  owner_->additional_context_data_.erase(handle_);
}

InteractiveTestPrivate::InteractiveTestPrivate() {
  MaybeRegisterFrameworkImpl<InteractiveTestPrivateFrameworkImpl>();
}

InteractiveTestPrivate::~InteractiveTestPrivate() = default;

void InteractiveTestPrivate::Init(ElementContext initial_context) {
  success_ = false;
  sequence_skipped_ = false;
  MaybeAddPivotElement(initial_context);
  for (ElementContext context :
       ElementTracker::GetElementTracker()->GetAllContextsForTesting()) {
    MaybeAddPivotElement(context);
  }
  context_subscription_ =
      ElementTracker::GetElementTracker()->AddAnyElementShownCallbackForTesting(
          base::BindRepeating(&InteractiveTestPrivate::OnElementAdded,
                              base::Unretained(this)));
}

void InteractiveTestPrivate::Cleanup() {
  context_subscription_ = base::CallbackListSubscription();
  pivot_elements_.clear();
}

void InteractiveTestPrivate::OnElementAdded(TrackedElement* el) {
  if (el->identifier() == kInteractiveTestPivotElementId)
    return;
  MaybeAddPivotElement(el->context());
}

void InteractiveTestPrivate::MaybeAddPivotElement(ElementContext context) {
  CHECK(context) << "Attempted to run steps in an invalid (null) context.";
  if (!pivot_elements_.contains(context)) {
    auto pivot =
        std::make_unique<TestElement>(kInteractiveTestPivotElementId, context);
    auto* const el = pivot.get();
    pivot_elements_.emplace(context, std::move(pivot));
    el->Show();
  }
}

base::WeakPtr<InteractiveTestPrivate> InteractiveTestPrivate::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void InteractiveTestPrivate::SetDefaultContext(
    ElementContext context,
    gfx::NativeWindow default_context_window) {
  default_context_ = context;
  if (default_context_window) {
    if (!default_context_window_) {
      default_context_window_ = std::make_unique<NativeWindowReference>();
    }
    default_context_window_->SetWindow(default_context_window);
  } else {
    default_context_window_.reset();
  }
  for (auto& framework : framework_implementations_) {
    framework.OnDefaultContextSet();
  }
}

gfx::NativeWindow InteractiveTestPrivate::GetDefaultContextWindow() const {
  return default_context_window_ ? default_context_window_->GetWindow()
                                 : gfx::NativeWindow();
}

gfx::NativeWindow InteractiveTestPrivate::GetNativeWindowFor(
    const ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();

  for (auto& framework : base::Reversed(framework_implementations_)) {
    window = framework.GetNativeWindowFromElement(el);
    if (window) {
      break;
    }
  }

  if (window) {
    // Want to remember the last window hit within each context so that verbs
    // which do not target a real element use the same window as the previous
    // action.
    const auto emplace_result = most_recent_windows_.try_emplace(
        el->context(), NativeWindowReference());
    emplace_result.first->second.SetWindow(window);
  } else {
    // If the element does not correspond to a specific native window (because
    // it is a pivot, test element, or for some other reason), fall back to the
    // most recent window for that context.
    if (auto* const entry =
            base::FindOrNull(most_recent_windows_, el->context())) {
      window = entry->GetWindow();
    }
  }

  // If we still don't know what window to use, use the default one for the
  // current context. We don't check this one before the most recent windows
  // cache because it will always return the primary window for the context, but
  // the mouse could be over a child window.
  if (!window) {
    for (auto& framework : base::Reversed(framework_implementations_)) {
      window = framework.GetNativeWindowFromContext(el->context());
      if (window) {
        break;
      }
    }
  }

  return window;
}

void InteractiveTestPrivate::HandleActionResult(
    InteractionSequence* seq,
    const TrackedElement* el,
    const std::string& operation_name,
    ActionResult result,
    bool defer_failure) {
  switch (result) {
    case ActionResult::kSucceeded:
      break;
    case ActionResult::kFailed:
      if (defer_failure) {
        ReportDeferredFailure(operation_name, el->context());
      } else {
        LOG(ERROR) << operation_name << " failed for " << *el;
        seq->FailForTesting();
      }
      break;
    case ActionResult::kNotAttempted:
      LOG(ERROR) << operation_name << " could not be applied to " << *el;
      seq->FailForTesting();
      break;
    case ActionResult::kKnownIncompatible:
      LOG(WARNING) << operation_name
                   << " failed because it is unsupported on this platform for "
                   << *el;
      if (!on_incompatible_action_reason_.empty()) {
        LOG(WARNING) << "Unsupported action was expected: "
                     << on_incompatible_action_reason_;
      } else {
        LOG(ERROR) << "Unsupported action was unexpected. "
                      "Did you forget to call SetOnIncompatibleAction()?";
      }
      switch (on_incompatible_action_) {
        case OnIncompatibleAction::kFailTest:
          seq->FailForTesting();
          break;
        case OnIncompatibleAction::kSkipTest:
        case OnIncompatibleAction::kHaltTest:
          sequence_skipped_ = true;
          seq->FailForTesting();
          break;
        case OnIncompatibleAction::kIgnoreAndContinue:
          break;
      }
      break;
  }
}

TrackedElement* InteractiveTestPrivate::GetPivotElement(
    ElementContext context) const {
  const auto it = pivot_elements_.find(context);
  CHECK(it != pivot_elements_.end())
      << "Tried to reference non-existent context.";
  return it->second.get();
}

bool InteractiveTestPrivate::RemoveStateObserver(UntypedStateIdentifier id,
                                                 ElementContext context) {
  using It = decltype(state_observer_elements_.begin());
  It found = state_observer_elements_.end();
  const auto element_id = StateToElementId(id);
  for (It it = state_observer_elements_.begin();
       it != state_observer_elements_.end(); ++it) {
    auto& entry = **it;
    if (entry.identifier() == element_id &&
        (!context || entry.context() == context)) {
      CHECK(found == state_observer_elements_.end())
          << "RemoveStateObserver: Duplicate entries found for " << id;
      found = it;
    }
  }
  if (found == state_observer_elements_.end()) {
    LOG(ERROR) << "RemoveStateObserver: Entry not found for " << id;
    return false;
  }

  state_observer_elements_.erase(found);
  return true;
}

InteractiveTestPrivate::AdditionalContext
InteractiveTestPrivate::CreateAdditionalContext() {
  return AdditionalContext(*this, next_additional_context_handle_++);
}

std::vector<std::string> InteractiveTestPrivate::GetAdditionalContext() const {
  std::vector<std::string> entries;
  std::transform(additional_context_data_.begin(),
                 additional_context_data_.end(), std::back_inserter(entries),
                 [](const auto& entry) { return entry.second; });
  return entries;
}

void InteractiveTestPrivate::DoTestSetUp() {
  temporary_storage_.emplace();
  for (auto& framework : framework_implementations_) {
    framework.DoTestSetUp();
  }
}
void InteractiveTestPrivate::DoTestTearDown() {
  for (auto& framework : base::Reversed(framework_implementations_)) {
    framework.DoTestTearDown();
  }
  state_observer_elements_.clear();
  temporary_storage_.reset();
}

void InteractiveTestPrivate::OnSequenceComplete() {
  for (auto& framework : base::Reversed(framework_implementations_)) {
    framework.OnSequenceComplete();
  }

  if (deferred_failures_.empty()) {
    success_ = true;
  } else {
    std::ostringstream full_error_message;
    full_error_message
        << "Interactive test failed.\n"
           "Some steps reported errors (see test log for more details):";
    for (const auto& failure : deferred_failures_) {
      full_error_message << "\n" << failure;
    }
    if (aborted_callback_for_testing_) {
      InteractionSequence::AbortedData data;
      data.aborted_reason =
          InteractionSequence::AbortedReason::kFailedForTesting;
      data.context = default_context();
      data.step_description = full_error_message.str();
      std::move(aborted_callback_for_testing_).Run(data);
      return;
    }
    GTEST_FAIL() << full_error_message.str();
  }
}

void InteractiveTestPrivate::OnSequenceAborted(
    const InteractionSequence::AbortedData& data) {
  for (auto& framework : base::Reversed(framework_implementations_)) {
    framework.OnSequenceAborted(data);
  }
  if (aborted_callback_for_testing_) {
    std::move(aborted_callback_for_testing_).Run(data);
    return;
  }
  if (sequence_skipped_) {
    LOG(WARNING) << kInteractiveTestFailedMessagePrefix << data;
    if (on_incompatible_action_ == OnIncompatibleAction::kSkipTest) {
      GTEST_SKIP();
    } else {
      DCHECK_EQ(OnIncompatibleAction::kHaltTest, on_incompatible_action_);
    }
  } else {
    std::ostringstream additional_message;
    if (data.aborted_reason == InteractionSequence::AbortedReason::
                                   kElementHiddenBetweenTriggerAndStepStart) {
      additional_message
          << "\nNOTE: Please check for one of the following common mistakes:\n"
             " - A RunLoop whose type is not set to kNestableTasksAllowed. "
             "Change the type and try again.\n"
             " - A check being performed on an element that has been hidden. "
             "Wrap waiting for the hide and subsequent checks in a "
             "WithoutDelay() to avoid possible access-after-delete.";
    }
    const auto additional_context = GetAdditionalContext();
    if (!additional_context.empty()) {
      additional_message << "\nAdditional test context:";
      for (const auto& ctx : additional_context) {
        additional_message << "\n * " << ctx;
      }
    }
    DebugDumpElements(data.context).PrintTo(additional_message);
    if (!deferred_failures_.empty()) {
      additional_message << "\n" << "Some prior steps also failed:";
      for (const auto& failure : deferred_failures_) {
        additional_message << "\n" << failure;
      }
    }
    GTEST_FAIL() << "Interactive test failed " << data
                 << additional_message.str();
  }
}

void InteractiveTestPrivate::ReportDeferredFailure(
    std::string_view error_message,
    ElementContext current_context) {
  std::ostringstream full_error_message;
  full_error_message << error_message << "\n";
  DebugDumpContext(current_context).PrintTo(full_error_message);
  deferred_failures_.push_back(full_error_message.str());
}

InteractiveTestPrivateFrameworkBase::InteractiveTestPrivateFrameworkBase(
    InteractiveTestPrivate& test_impl)
    : test_impl_(test_impl) {}
InteractiveTestPrivateFrameworkBase::~InteractiveTestPrivateFrameworkBase() =
    default;
InteractiveTestPrivateFrameworkBase::DebugTreeNode::DebugTreeNode() = default;
InteractiveTestPrivateFrameworkBase::DebugTreeNode::DebugTreeNode(
    std::string initial_text)
    : text(initial_text) {}
InteractiveTestPrivateFrameworkBase::DebugTreeNode::DebugTreeNode(
    DebugTreeNode&&) noexcept = default;
InteractiveTestPrivateFrameworkBase::DebugTreeNode&
InteractiveTestPrivateFrameworkBase::DebugTreeNode::operator=(
    DebugTreeNode&&) noexcept = default;
InteractiveTestPrivateFrameworkBase::DebugTreeNode::~DebugTreeNode() = default;

std::vector<InteractiveTestPrivateFrameworkBase::DebugTreeNode>
InteractiveTestPrivateFrameworkBase::DebugDumpElements(
    std::set<const ui::TrackedElement*>& el) const {
  return {};
}
std::string InteractiveTestPrivateFrameworkBase::DebugDescribeContext(
    ui::ElementContext context) const {
  return std::string();
}

// static
std::string InteractiveTestPrivateFrameworkBase::DebugDumpBounds(
    const gfx::Rect& bounds) {
  return base::StringPrintf("x:%d-%d y:%d-%d (%dx%d)", bounds.x(),
                            bounds.right(), bounds.y(), bounds.bottom(),
                            bounds.width(), bounds.height());
}

gfx::NativeWindow
InteractiveTestPrivateFrameworkBase::GetNativeWindowFromElement(
    const TrackedElement* el) const {
  // Default answer is "no window is associated with this element".
  // Other framework implementations will provide ways to convert specific
  // types of elements to their native windows.
  return gfx::NativeWindow();
}

gfx::NativeWindow
InteractiveTestPrivateFrameworkBase::GetNativeWindowFromContext(
    ElementContext) const {
  // Default answer is "no window is associated with this context".
  // Other framework implementations will provide ways to convert specific
  // contexts to their native windows.
  return gfx::NativeWindow();
}

namespace {
void PrintDebugTree(std::ostream& stream,
                    const InteractiveTestPrivate::DebugTreeNode& node,
                    std::string prefix,
                    bool last) {
  stream << prefix;
  if (prefix.empty()) {
    stream << "\n";
    prefix += "  ";
  } else {
    if (last) {
      stream << "╰─";
      prefix += "   ";
    } else {
      stream << "├─";
      prefix += "│  ";
    }
  }
  stream << node.text << '\n';
  for (size_t i = 0; i < node.children.size(); ++i) {
    const bool last_child = (i == node.children.size() - 1);
    PrintDebugTree(stream, node.children[i], prefix, last_child);
  }
}
}  // namespace

void InteractiveTestPrivate::DebugTreeNode::PrintTo(
    std::ostream& stream) const {
  PrintDebugTree(stream, *this, "", true);
}

// static
ElementIdentifier InteractiveTestPrivate::StateToElementId(
    UntypedStateIdentifier id) {
  return ElementIdentifier::FromRawValue(
      id.GetRawValue(base::PassKey<InteractiveTestPrivate>()));
}

InteractiveTestPrivate::DebugTreeNode InteractiveTestPrivate::DebugDumpElements(
    ui::ElementContext current_context) const {
  DebugTreeNode node("UI Elements");
  const auto* const tracker = ui::ElementTracker::GetElementTracker();
  for (const auto ctx : tracker->GetAllContextsForTesting()) {
    DebugTreeNode ctx_node = DebugDumpContext(ctx);
    if (ctx == current_context) {
      ctx_node.text = "[CURRENT CONTEXT] " + ctx_node.text;
    }
    node.children.emplace_back(std::move(ctx_node));
  }
  return node;
}

InteractiveTestPrivate::DebugTreeNode InteractiveTestPrivate::DebugDumpContext(
    ui::ElementContext context) const {
  std::string context_description;
  for (auto& framework_implementation :
       base::Reversed(framework_implementations_)) {
    context_description =
        framework_implementation.DebugDescribeContext(context);
    if (!context_description.empty()) {
      break;
    }
  }
  CHECK(!context_description.empty());
  DebugTreeNode node(context_description);
  auto element_list =
      ui::ElementTracker::GetElementTracker()->GetAllElementsForTesting(
          context);
  std::set<const ui::TrackedElement*> elements(element_list.begin(),
                                               element_list.end());
  std::vector<std::vector<DebugTreeNode>> temp;
  for (auto& framework_implementation :
       base::Reversed(framework_implementations_)) {
    temp.push_back(framework_implementation.DebugDumpElements(elements));
  }
  CHECK(elements.empty());
  for (auto& nodes : base::Reversed(temp)) {
    std::move(nodes.begin(), nodes.end(), std::back_inserter(node.children));
  }
  return node;
}
std::string DescribeElement(ElementSpecifier element) {
  std::ostringstream oss;
  oss << element;
  return oss.str();
}

InteractionSequence::Builder BuildSubsequence(
    InteractiveTestPrivate::MultiStep steps) {
  InteractionSequence::Builder builder;
  for (auto& step : steps) {
    builder.AddStep(std::move(step));
  }
  return builder;
}

}  // namespace ui::test::internal
