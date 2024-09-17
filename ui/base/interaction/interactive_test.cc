// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"

namespace ui::test {

using internal::kInteractiveTestPivotElementId;

InteractiveTestApi::InteractiveTestApi(
    std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl)
    : private_test_impl_(std::move(private_test_impl)) {}
InteractiveTestApi::~InteractiveTestApi() = default;

InteractionSequence::StepBuilder InteractiveTestApi::PressButton(
    ElementSpecifier button,
    InputType input_type) {
  StepBuilder builder;
  builder.SetDescription("PressButton()");
  internal::SpecifyElement(builder, button);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "PressButton",
            test->test_util().PressButton(el, input_type));
      },
      input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectMenuItem(
    ElementSpecifier menu_item,
    InputType input_type) {
  RequireInteractiveTest();
  StepBuilder builder;
  builder.SetDescription("SelectMenuItem()");
  internal::SpecifyElement(builder, menu_item);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "SelectMenuItem",
            test->test_util().SelectMenuItem(el, input_type));
      },
      input_type, base::Unretained(this)));

  // TODO(https://crbug.com/359252812): On Linux, sometimes a SelectMenuItem is
  // interrupted by a spurious focus change, even on tests designed to run
  // single-thread, single-process. Once the culprit has been tracked down and
  // eliminated, remove this #if.
#if BUILDFLAG(IS_LINUX)
  builder.SetStepStartMode(InteractionSequence::StepStartMode::kImmediate);
#endif

  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::DoDefaultAction(
    ElementSpecifier element,
    InputType input_type) {
  StepBuilder builder;
  builder.SetDescription("DoDefaultAction()");
  internal::SpecifyElement(builder, element);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "DoDefaultAction",
            test->test_util().DoDefaultAction(el, input_type));
      },
      input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectTab(
    ElementSpecifier tab_collection,
    size_t tab_index,
    InputType input_type) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf("SelectTab( %zu )", tab_index));
  internal::SpecifyElement(builder, tab_collection);
  builder.SetStartCallback(base::BindOnce(
      [](size_t index, InputType input_type, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "SelectTab",
            test->test_util().SelectTab(el, index, input_type));
      },
      tab_index, input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectDropdownItem(
    ElementSpecifier collection,
    size_t item,
    InputType input_type) {
  // "Don't care" option directly sets the value; the other actually require
  // popping out the dropdown menu and selecting an item which is not reliable
  // in non-interactive tests.
  if (input_type != InputType::kDontCare) {
    RequireInteractiveTest();
  }

  StepBuilder builder;
  builder.SetDescription(base::StringPrintf("SelectDropdownItem( %zu )", item));
  internal::SpecifyElement(builder, collection);
  builder.SetStartCallback(base::BindOnce(
      [](size_t item, InputType input_type, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "SelectDropdownItem",
            test->test_util().SelectDropdownItem(el, item, input_type));
      },
      item, input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::EnterText(
    ElementSpecifier element,
    std::u16string text,
    TextEntryMode mode) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf("EnterText( \"%s\" )",
                                            base::UTF16ToUTF8(text).c_str()));
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](std::u16string text, TextEntryMode mode, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "EnterText",
            test->test_util().EnterText(el, std::move(text), mode));
      },
      std::move(text), mode, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::ActivateSurface(
    ElementSpecifier element) {
  RequireInteractiveTest();
  StepBuilder builder;
  builder.SetDescription("ActivateSurface()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](InteractiveTestApi* test, InteractionSequence* seq,
         TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "ActivateSurface", test->test_util().ActivateSurface(el));
      },
      base::Unretained(this)));
  return builder;
}

#if !BUILDFLAG(IS_IOS)
InteractionSequence::StepBuilder InteractiveTestApi::SendAccelerator(
    ElementSpecifier element,
    Accelerator accelerator) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf(
      "SendAccelerator( %s )",
      base::UTF16ToUTF8(accelerator.GetShortcutText()).c_str()));
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](Accelerator accelerator, InteractiveTestApi* test,
         InteractionSequence* seq, TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "SendAccelerator",
            test->test_util().SendAccelerator(el, accelerator));
      },
      accelerator, base::Unretained(this)));
  return builder;
}
#endif  // !BUILDFLAG(IS_IOS)

InteractionSequence::StepBuilder InteractiveTestApi::Confirm(
    ElementSpecifier element) {
  StepBuilder builder;
  builder.SetDescription("Confirm()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](InteractiveTestApi* test, InteractionSequence* seq,
         TrackedElement* el) {
        test->private_test_impl().HandleActionResult(
            seq, el, "Confirm", test->test_util().Confirm(el));
      },
      base::Unretained(this)));
  return builder;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForShow(
    ElementSpecifier element,
    bool transition_only_on_event) {
  StepBuilder step;
  step.SetDescription("WaitForShow()");
  internal::SpecifyElement(step, element);
  step.SetTransitionOnlyOnEvent(transition_only_on_event);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForHide(
    ElementSpecifier element,
    bool transition_only_on_event) {
  StepBuilder step;
  step.SetDescription("WaitForHide()");
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kHidden);
  step.SetTransitionOnlyOnEvent(transition_only_on_event);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForActivate(
    ElementSpecifier element) {
  StepBuilder step;
  step.SetDescription("WaitForActivate()");
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kActivated);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForEvent(
    ElementSpecifier element,
    CustomElementEventType event) {
  StepBuilder step;
  step.SetDescription(
      base::StringPrintf("WaitForEvent( %s )", event.GetName().c_str()));
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kCustomEvent, event);
  return step;
}

// static
InteractiveTestApi::StepBuilder InteractiveTestApi::EnsureNotPresent(
    ElementIdentifier element_to_check) {
  return std::move(
      WithElement(kInteractiveTestPivotElementId,
                  [element_to_check](InteractionSequence* seq,
                                     TrackedElement* reference) {
                    auto* const tracker = ElementTracker::GetElementTracker();
                    auto* const element =
                        seq->IsCurrentStepInAnyContextForTesting()
                            ? tracker->GetElementInAnyContext(element_to_check)
                            : tracker->GetFirstMatchingElement(
                                  element_to_check, reference->context());
                    if (element) {
                      LOG(ERROR) << "Expected element " << element->ToString()
                                 << " not to be present but it was present.";
                      seq->FailForTesting();
                    }
                  })
          .SetDescription(base::StringPrintf(
              "EnsureNotPresent( %s )", element_to_check.GetName().c_str())));
}

// static
InteractiveTestApi::StepBuilder InteractiveTestApi::EnsurePresent(
    ElementSpecifier element_to_check) {
  return std::move(
      WithElement(element_to_check, base::DoNothing())
          .SetDescription(base::StringPrintf(
              "EnsurePresent( %s )",
              internal::DescribeElement(element_to_check).c_str())));
}

InteractionSequence::StepBuilder InteractiveTestApi::NameElement(
    std::string_view name,
    AbsoluteElementSpecifier spec) {
  return NameElementRelative(kInteractiveTestPivotElementId, name,
                             GetFindElementCallback(std::move(spec)));
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::InAnyContext(
    MultiStep steps) {
  for (auto& step : steps) {
    step.SetContext(InteractionSequence::ContextMode::kAny)
        .FormatDescription("InAnyContext( %s )");
  }
  return steps;
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::InSameContext(
    MultiStep steps) {
  for (auto& step : steps) {
    step.SetContext(InteractionSequence::ContextMode::kFromPreviousStep)
        .FormatDescription("InSameContext( %s )");
  }
  return steps;
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::InContext(
    ElementContext context,
    MultiStep steps) {
  // This context may not yet exist, but we want the pivot element to exist.
  private_test_impl_->MaybeAddPivotElement(context);
  const auto fmt = base::StringPrintf("InContext( %p, %%s )",
                                      static_cast<const void*>(context));
  for (auto& step : steps) {
    step.SetContext(context).FormatDescription(fmt);
  }

  return steps;
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::WithoutDelay(
    MultiStep steps) {
  for (auto& step : steps) {
    step.SetStepStartMode(InteractionSequence::StepStartMode::kImmediate)
        .FormatDescription("WithoutDelay( %s )");
  }
  return steps;
}

InteractiveTestApi::StepBuilder InteractiveTestApi::SetOnIncompatibleAction(
    OnIncompatibleAction action,
    const char* reason) {
  return Do(base::BindOnce(
      [](InteractiveTestApi* test, OnIncompatibleAction action,
         std::string reason) {
        DCHECK(action == OnIncompatibleAction::kFailTest || !reason.empty());
        test->private_test_impl().on_incompatible_action_ = action;
        test->private_test_impl().on_incompatible_action_reason_ = reason;
      },
      base::Unretained(this), action, std::string(reason)));
}

constexpr char kInteractiveErrorMessage[] = R"(

The test verb you are trying to use requires an interactive test environment.

This is one in which the test can safely control things like mouse movement and
window activation, without having to worry about other processes making changes
that can cause flakiness.

Q: But I was just selecting a menu item!
A: In tests where process exclusivity is not guaranteed, if the test application
   loses focus, the menu could unexpectedly close, leading to flakiness. We want
   to preemptively avoid these flakes.

Solutions:
 - Use PressButton() instead of MoveMouseTo() + ClickMouse()
 - Move your browser test from browser_tests to interactive_ui_tests
 - Move your Ash test into chromeos_integration_tests

How to make a browser test interactive:
 - Rename your test file from *_browsertest.cc to *_interactive_uitest.cc
    * e.g. my_system_browsertest.cc -> my_system_interactive_uitest.cc
 - Rename your test class from *Browsertest to *UiTest
    * e.g. MySystemBrowserTest -> MySystemUiTest
 - Edit chrome/test/BUILD.gn to move your test from the "browser_tests" target
   to the "interactive_ui_tests" target.
    * Ensure that if your test only ran in certain configurations, in
      browser_tests, it is gated by the same conditions in interactive_ui_tests.
)";

void InteractiveTestApi::RequireInteractiveTest() {
  CHECK(internal::InteractiveTestPrivate::allow_interactive_test_verbs_)
      << kInteractiveErrorMessage;
}

bool InteractiveTestApi::RunTestSequenceImpl(
    ElementContext context,
    InteractionSequence::Builder builder) {
  builder.SetContext(context);

  private_test_impl_->Init(context);

  builder.SetCompletedCallback(
      base::BindOnce(&internal::InteractiveTestPrivate::OnSequenceComplete,
                     base::Unretained(private_test_impl_.get())));
  builder.SetAbortedCallback(
      base::BindOnce(&internal::InteractiveTestPrivate::OnSequenceAborted,
                     base::Unretained(private_test_impl_.get())));
  auto sequence = builder.Build();

  {
    base::test::ScopedRunLoopTimeout timeout(
        FROM_HERE, std::nullopt,
        base::BindRepeating(
            [](base::WeakPtr<InteractionSequence> sequence) {
              std::ostringstream oss;
              if (sequence) {
                oss << internal::kInteractiveTestFailedMessagePrefix
                    << sequence->BuildAbortedData(
                           InteractionSequence::AbortedReason::
                               kSequenceTimedOut);
              } else {
                oss << "Interactive test: timeout after test sequence "
                       "destroyed; a failure message may already have been "
                       "logged.";
              }
              return oss.str();
            },
            sequence->AsWeakPtr()));
    sequence->RunSynchronouslyForTesting();
  }

  private_test_impl_->Cleanup();

  return private_test_impl_->success_;
}

// static
InteractiveTestApi::FindElementCallback
InteractiveTestApi::GetFindElementCallback(AbsoluteElementSpecifier spec) {
  using ContextCallback = base::OnceCallback<TrackedElement*(ElementContext)>;
  return std::visit(
      base::Overloaded{
          [](TrackedElement* el) {
            CHECK(el) << "NameView(TrackedElement*): view must be set.";
            return base::BindOnce(
                [](const SafeElementReference& ref, TrackedElement*) {
                  LOG_IF(ERROR, !ref.get()) << "NameElement(TrackedElement*): "
                                               "element ceased to be valid "
                                               "before step was executed.";
                  return ref.get();
                },
                SafeElementReference(el));
          },
          [](std::reference_wrapper<TrackedElement*> ref) {
            return base::BindOnce(
                [](std::reference_wrapper<TrackedElement*> ref,
                   TrackedElement*) {
                  LOG_IF(ERROR, !ref.get()) << "NameElement(TrackedElement*): "
                                               "element ceased to be valid "
                                               "before step was executed.";
                  return ref.get();
                },
                ref);
          },
          [](ContextCallback& callback) {
            return base::BindOnce(
                [](ContextCallback callback, TrackedElement* relative_to) {
                  return std::move(callback).Run(relative_to->context());
                },
                std::move(callback));
          },
          [](base::OnceCallback<TrackedElement*()>& callback) {
            return base::RectifyCallback<FindElementCallback>(
                std::move(callback));
          }},
      spec);
}

// static
void InteractiveTestApi::AddStep(InteractionSequence::Builder& builder,
                                 MultiStep multi_step) {
  for (auto& step : multi_step)
    builder.AddStep(step);
}

// static
void InteractiveTestApi::AddStep(MultiStep& dest, StepBuilder src) {
  dest.emplace_back(std::move(src));
}

// static
void InteractiveTestApi::AddStep(MultiStep& dest, MultiStep src) {
  for (auto& step : src)
    dest.emplace_back(std::move(step));
}

// static
void InteractiveTestApi::AddDescription(MultiStep& steps,
                                        std::string_view format) {
  for (auto& step : steps) {
    step.FormatDescription(format);
  }
}

}  // namespace ui::test
