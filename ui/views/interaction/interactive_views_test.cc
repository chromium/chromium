// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test.h"

#include <functional>
#include <optional>
#include <string_view>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_tracker.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace views::test {

using ui::test::internal::SpecifyElement;

namespace {

auto CreateTestUtil() {
  auto test_util = std::make_unique<ui::test::InteractionTestUtil>();
  test_util->AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
  test_util->AddSimulator(
      std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
  return test_util;
}

}  // namespace

using ui::test::internal::kInteractiveTestPivotElementId;

InteractiveViewsTestApi::InteractiveViewsTestApi()
    : InteractiveViewsTestApi(
          std::make_unique<internal::InteractiveViewsTestPrivate>(
              CreateTestUtil())) {}

InteractiveViewsTestApi::InteractiveViewsTestApi(
    std::unique_ptr<internal::InteractiveViewsTestPrivate> private_test_impl)
    : InteractiveTestApi(std::move(private_test_impl)) {}
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

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::MoveMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("MoveMouseTo()");
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, RelativePositionCallback pos_callback,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl().GetWindowHintFor(el),
                InteractionTestUtilMouse::MoveTo(
                    std::move(pos_callback).Run(el)))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), GetPositionCallback(std::move(position))));
  return step;
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::MoveMouseTo(
    AbsolutePositionSpecifier position) {
  return MoveMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)));
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::ClickMouse(
    ui_controls::MouseButton button,
    bool release) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("ClickMouse()");
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, ui_controls::MouseButton button,
         bool release, ui::InteractionSequence* seq, ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl().GetWindowHintFor(el),
                release ? InteractionTestUtilMouse::Click(button)
                        : InteractionTestUtilMouse::MouseGestures{
                              InteractionTestUtilMouse::MouseDown(button)})) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), button, release));
  step.SetMustRemainVisible(false);
  return step;
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::DragMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position,
    bool release) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("DragMouseTo()");
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, RelativePositionCallback pos_callback,
         bool release, ui::InteractionSequence* seq, ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        const gfx::Point target = std::move(pos_callback).Run(el);
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl().GetWindowHintFor(el),
                release ? InteractionTestUtilMouse::DragAndRelease(target)
                        : InteractionTestUtilMouse::DragAndHold(target))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), GetPositionCallback(std::move(position)),
      release));
  return step;
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::DragMouseTo(
    AbsolutePositionSpecifier position,
    bool release) {
  return DragMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)), release);
}

InteractiveViewsTestApi::StepBuilder InteractiveViewsTestApi::ReleaseMouse(
    ui_controls::MouseButton button) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("ReleaseMouse()");
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, ui_controls::MouseButton button,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl().GetWindowHintFor(el),
                InteractionTestUtilMouse::MouseUp(button))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), button));
  step.SetMustRemainVisible(false);
  return step;
}

// static
InteractiveViewsTestApi::FindViewCallback
InteractiveViewsTestApi::GetFindViewCallback(AbsoluteViewSpecifier spec) {
  return std::visit(
      base::Overloaded{
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
      base::Overloaded{
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
    if (matcher.Run(child))
      return child;
    if (recursive) {
      auto* const result = FindMatchingView(child, matcher, true);
      if (result)
        return result;
    }
  }
  return nullptr;
}

void InteractiveViewsTestApi::SetContextWidget(Widget* widget) {
  context_widget_ = widget ? widget->GetWeakPtr() : nullptr;
  if (widget) {
    CHECK(!test_impl().mouse_util_)
        << "Changing the context widget during a test is not supported.";
    test_impl().mouse_util_ =
        std::make_unique<InteractionTestUtilMouse>(widget);
  } else {
    test_impl().mouse_util_.reset();
  }
}

// static
InteractiveViewsTestApi::RelativePositionCallback
InteractiveViewsTestApi::GetPositionCallback(AbsolutePositionSpecifier spec) {
  return std::visit(
      base::Overloaded{
          [](const gfx::Point& point) {
            return base::BindOnce(
                [](gfx::Point p, ui::TrackedElement*) { return p; }, point);
          },
          [](std::reference_wrapper<gfx::Point> point) {
            return base::BindOnce([](std::reference_wrapper<gfx::Point> p,
                                     ui::TrackedElement*) { return p.get(); },
                                  point);
          },
          [](AbsolutePositionCallback& callback) {
            return base::RectifyCallback<RelativePositionCallback>(
                std::move(callback));
          }},
      spec);
}

// static
InteractiveViewsTestApi::RelativePositionCallback
InteractiveViewsTestApi::GetPositionCallback(RelativePositionSpecifier spec) {
  return std::visit(
      base::Overloaded{[](RelativePositionCallback& callback) {
                         return std::move(callback);
                       },
                       [](CenterPoint) {
                         return base::BindOnce([](ui::TrackedElement* el) {
                           CHECK(el->IsA<views::TrackedElementViews>());
                           return el->AsA<views::TrackedElementViews>()
                               ->view()
                               ->GetBoundsInScreen()
                               .CenterPoint();
                         });
                       }},
      spec);
}

}  // namespace views::test
