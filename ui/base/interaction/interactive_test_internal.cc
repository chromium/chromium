// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test_internal.h"

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
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/native_ui_types.h"

namespace ui::test::internal {

namespace {

// Basic implementation of the framework for dumping generic elements.
class InteractiveTestPrivateFrameworkImpl
    : public InteractiveTestPrivateFrameworkBase {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

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
            base::StringPrintf("%s - %s at %s", el->GetImplementationName(),
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
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(InteractiveTestPrivateFrameworkImpl)

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);
DEFINE_STATE_IDENTIFIER_VALUE(PollingStateObserver<bool>,
                              kInteractiveTestPollUntilState);

StateObserverElement::StateObserverElement(ElementIdentifier id,
                                           ElementContext context)
    : TestElementBase(id, context) {}

StateObserverElement::~StateObserverElement() = default;

DEFINE_FRAMEWORK_SPECIFIC_METADATA(StateObserverElement)

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
  if (!base::Contains(pivot_elements_, context)) {
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

gfx::NativeWindow InteractiveTestPrivate::GetNativeWindowFor(
    const ui::TrackedElement* el) const {
  for (auto& framework : framework_implementations_) {
    if (auto result = framework.GetNativeWindowFromElement(el)) {
      return result;
    }
  }
  for (auto& framework : framework_implementations_) {
    if (auto result = framework.GetNativeWindowFromContext(el->context())) {
      return result;
    }
  }
  return gfx::NativeWindow();
}

void InteractiveTestPrivate::HandleActionResult(
    InteractionSequence* seq,
    const TrackedElement* el,
    const std::string& operation_name,
    ActionResult result) {
  switch (result) {
    case ActionResult::kSucceeded:
      break;
    case ActionResult::kFailed:
      LOG(ERROR) << operation_name << " failed for " << *el;
      seq->FailForTesting();
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

bool InteractiveTestPrivate::RemoveStateObserver(ElementIdentifier id,
                                                 ElementContext context) {
  using It = decltype(state_observer_elements_.begin());
  It found = state_observer_elements_.end();
  for (It it = state_observer_elements_.begin();
       it != state_observer_elements_.end(); ++it) {
    auto& entry = **it;
    if (entry.identifier() == id && (!context || entry.context() == context)) {
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
  for (auto& framework : framework_implementations_) {
    framework.DoTestSetUp();
  }
}
void InteractiveTestPrivate::DoTestTearDown() {
  for (auto& framework : base::Reversed(framework_implementations_)) {
    framework.DoTestTearDown();
  }
  state_observer_elements_.clear();
}

void InteractiveTestPrivate::OnSequenceComplete() {
  for (auto& framework : base::Reversed(framework_implementations_)) {
    framework.OnSequenceComplete();
  }
  success_ = true;
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
    GTEST_FAIL() << "Interactive test failed " << data
                 << additional_message.str();
  }
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
