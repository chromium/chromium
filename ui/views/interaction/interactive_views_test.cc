// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test.h"

#include <functional>
#include <optional>
#include <string_view>
#include <variant>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/interaction/accelerator_observer.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test_internal.h"
#include "ui/views/view_tracker.h"

namespace views::test {

using ui::test::internal::kInteractiveTestPivotElementId;

InteractiveViewsTestApi::InteractiveViewsTestApi()
    : test_impl_(private_test_impl()
                     .MaybeRegisterFrameworkImpl<
                         internal::InteractiveViewsTestPrivate>()) {}

InteractiveViewsTestApi::~InteractiveViewsTestApi() = default;

ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::NameView(
    std::string_view name,
    AbsoluteViewSpecifier spec) {
  return NameViewRelative(kInteractiveTestPivotElementId, name,
                          GetFindViewCallback(std::move(spec)));
}

// static
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::NameChildView(
    ElementSpecifier parent,
    std::string_view name,
    ChildViewSpecifier spec) {
  return std::move(
      NameViewRelative(parent, name, GetFindViewCallback(std::move(spec)))
          .SetDescription(
              base::StringPrintf("NameChildView( \"%s\" )", name.data())));
}

// static
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameDescendantView(ElementSpecifier parent,
                                            std::string_view name,
                                            ViewMatcher matcher) {
  return std::move(
      NameViewRelative(
          parent, name,
          base::BindOnce(
              [](ViewMatcher matcher, View* ancestor) -> View* {
                auto* const result =
                    FindMatchingView(ancestor, matcher, /* recursive =*/true);
                if (!result) {
                  LOG(ERROR)
                      << "NameDescendantView(): No descendant matches matcher.";
                }
                return result;
              },
              matcher))
          .SetDescription(
              base::StringPrintf("NameDescendantView( \"%s\" )", name.data())));
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::ScrollIntoView(
    ElementSpecifier view) {
  return std::move(WithView(view, [](View* v) {
                     v->ScrollViewToVisible();
                   }).SetDescription("ScrollIntoView()"));
}

InteractiveViewsTestApi::StepBuilder
InteractiveViewsTestApi::MaybeEnterInteractiveMode(
    ElementSpecifier target,
    ui::Accelerator exit_accelerator) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(AcceleratorObserver,
                                      kExitInteractiveModeObserver);
  return If(
      [] {
        return base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestLauncherInteractive);
      },
      Then(Log("\n-----------------------------------"
               "\nEntering interactive mode. Press ",
               exit_accelerator.GetShortcutText(),
               " to exit."
               "\n-----------------------------------"),
           CheckElement(
               target,
               [=, this](ui::TrackedElement* el) {
                 return private_test_impl().AddStateObserver(
                     kExitInteractiveModeObserver.identifier(), el->context(),
                     std::make_unique<AcceleratorObserver>(
                         el, private_test_impl().GetNativeWindowFor(el),
                         exit_accelerator));
               }),
           WaitForState(kExitInteractiveModeObserver,
                        testing::Ne(AcceleratorObserverState::kWaiting)),
           StopObservingState(kExitInteractiveModeObserver)));
}

// static
InteractiveViewsTestApi::FindViewCallback
InteractiveViewsTestApi::GetFindViewCallback(AbsoluteViewSpecifier spec) {
  return std::visit(
      absl::Overload{
          [](View* view) {
            CHECK(view) << "NameView(View*): view must be set.";
            return base::BindOnce(
                [](const std::unique_ptr<ViewTracker>& ref, View*) {
                  LOG_IF(ERROR, !ref->view())
                      << "NameView(View*): view ceased to be "
                         "valid before step was executed.";
                  return ref->view();
                },
                std::make_unique<ViewTracker>(view));
          },
          [](std::reference_wrapper<View*> ref) {
            return base::BindOnce(
                [](std::reference_wrapper<View*> ref, View*) {
                  LOG_IF(ERROR, !ref.get())
                      << "NameView(View*): view ceased to be "
                         "valid before step was executed.";
                  return ref.get();
                },
                ref);
          },
          [](base::OnceCallback<View*()>& callback) {
            return base::RectifyCallback<FindViewCallback>(std::move(callback));
          }},
      spec);
}

// static
InteractiveViewsTestApi::FindViewCallback
InteractiveViewsTestApi::GetFindViewCallback(ChildViewSpecifier spec) {
  return std::visit(
      absl::Overload{
          [](size_t index) {
            return base::BindOnce(
                [](size_t index, View* parent) -> View* {
                  if (index >= parent->children().size()) {
                    LOG(ERROR)
                        << "NameChildView(int): Child index out of bounds; got "
                        << index << " but only " << parent->children().size()
                        << " children.";
                    return nullptr;
                  }
                  return parent->children()[index];
                },
                index);
          },
          [](ViewMatcher& matcher) {
            return base::BindOnce(
                [](ViewMatcher matcher, View* parent) -> View* {
                  auto* const result =
                      FindMatchingView(parent, matcher, /*recursive =*/false);
                  LOG_IF(ERROR, !result) << "NameChildView(ViewMatcher): No "
                                            "child matches matcher.";
                  return result;
                },
                std::move(matcher));
          }},
      spec);
}

// static
View* InteractiveViewsTestApi::FindMatchingView(const View* from,
                                                ViewMatcher& matcher,
                                                bool recursive) {
  for (views::View* const child : from->children()) {
    if (matcher.Run(child)) {
      return child;
    }
    if (recursive) {
      auto* const result = FindMatchingView(child, matcher, true);
      if (result) {
        return result;
      }
    }
  }
  return nullptr;
}

void InteractiveViewsTestApi::SetContextWidget(Widget* widget) {
  CHECK(!widget || !private_test_impl().default_context())
      << "Changing the context widget during a test is not supported.";
  if (widget) {
    context_widget_ = widget->GetWeakPtr();
    private_test_impl().SetDefaultContext(
        ElementTrackerViews::GetContextForWidget(widget),
        widget->GetNativeWindow());
  } else {
    context_widget_.reset();
    private_test_impl().SetDefaultContext(ui::ElementContext(),
                                          gfx::NativeWindow());
  }
}

}  // namespace views::test
