// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test.h"

#include "build/build_config.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_tracker.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace views::test {

using ui::test::internal::SpecifyElement;

namespace {

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseGestureCompleteEvent);

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
    base::StringPiece name,
    AbsoluteViewSpecifier spec) {
  return NameViewRelative(kInteractiveTestPivotElementId, name,
                          GetFindViewCallback(std::move(spec)));
}

// static
ui::InteractionSequence::StepBuilder InteractiveViewsTestApi::NameChildView(
    ElementSpecifier parent,
    base::StringPiece name,
    ChildViewSpecifier spec) {
  return NameViewRelative(parent, name, GetFindViewCallback(std::move(spec)));
}

// static
ui::InteractionSequence::StepBuilder
InteractiveViewsTestApi::NameDescendantView(ElementSpecifier parent,
                                            base::StringPiece name,
                                            ViewMatcher matcher) {
  return NameViewRelative(
      parent, name,
      base::BindOnce(
          [](ViewMatcher matcher, View* ancestor) -> View* {
            auto* const result =
                FindMatchingView(ancestor, matcher, /* recursive =*/true);
            if (!result)
              LOG(ERROR)
                  << "NameDescendantView(): No descendant matches matcher.";
            return result;
          },
          matcher));
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::MoveMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position) {
  StepBuilder step;
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, RelativePositionCallback pos_callback,
         ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveViewsTestApi* test, bool success) {
                  if (!success)
                    test->test_impl().mouse_error_message_ =
                        "MoreMouseTo() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->test_impl().pivot_element(),
                      kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            InteractionTestUtilMouse::MoveTo(std::move(pos_callback).Run(el)));
      },
      base::Unretained(this), GetPositionCallback(std::move(position))));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::MoveMouseTo(
    AbsolutePositionSpecifier position) {
  return MoveMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)));
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::ClickMouse(
    ui_controls::MouseButton button,
    bool release) {
  StepBuilder step;
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, ui_controls::MouseButton button,
         bool release) {
        test->test_impl().mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveViewsTestApi* test, bool success) {
                  if (!success)
                    test->test_impl().mouse_error_message_ =
                        "ClickMouse() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->test_impl().pivot_element(),
                      kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            release ? InteractionTestUtilMouse::Click(button)
                    : InteractionTestUtilMouse::MouseGestures{
                          InteractionTestUtilMouse::MouseDown(button)});
      },
      base::Unretained(this), button, release));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::DragMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position,
    bool release) {
  StepBuilder step;
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, RelativePositionCallback pos_callback,
         bool release, ui::TrackedElement* el) {
        test->test_impl().mouse_error_message_.clear();
        const gfx::Point target = std::move(pos_callback).Run(el);
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveViewsTestApi* test, bool success) {
                  if (!success)
                    test->test_impl().mouse_error_message_ =
                        "DragMouseTo() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->test_impl().pivot_element(),
                      kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            release ? InteractionTestUtilMouse::DragAndRelease(target)
                    : InteractionTestUtilMouse::DragAndHold(target));
      },
      base::Unretained(this), GetPositionCallback(std::move(position)),
      release));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::DragMouseTo(
    AbsolutePositionSpecifier position,
    bool release) {
  return DragMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)), release);
}

InteractiveViewsTestApi::MultiStep InteractiveViewsTestApi::ReleaseMouse(
    ui_controls::MouseButton button) {
  StepBuilder step;
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveViewsTestApi* test, ui_controls::MouseButton button) {
        test->test_impl().mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveViewsTestApi* test, bool success) {
                  if (!success)
                    test->test_impl().mouse_error_message_ =
                        "ReleaseMouse() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->test_impl().pivot_element(),
                      kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            InteractionTestUtilMouse::MouseUp(button));
      },
      base::Unretained(this), button));
  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

// static
InteractiveViewsTestApi::FindViewCallback<View>
InteractiveViewsTestApi::GetFindViewCallback(AbsoluteViewSpecifier spec) {
  if (View** view = absl::get_if<View*>(&spec)) {
    CHECK(*view) << "NameView(View*): view must be set.";
    return base::BindOnce(
        [](const std::unique_ptr<ViewTracker>& ref, View*) {
          LOG_IF(ERROR, !ref->view()) << "NameView(View*): view ceased to be "
                                         "valid before step was executed.";
          return ref->view();
        },
        std::make_unique<ViewTracker>(*view));
  }

  if (View*** view = absl::get_if<View**>(&spec)) {
    CHECK(*view) << "NameView(View**): view pointer is null.";
    return base::BindOnce(
        [](View** view, View*) {
          LOG_IF(ERROR, !*view) << "NameView(View**): view pointer is null.";
          return *view;
        },
        base::Unretained(*view));
  }

  return base::RectifyCallback<FindViewCallback<View>>(
      std::move(absl::get<base::OnceCallback<View*()>>(spec)));
}

// static
InteractiveViewsTestApi::FindViewCallback<View>
InteractiveViewsTestApi::GetFindViewCallback(ChildViewSpecifier spec) {
  if (size_t* index = absl::get_if<size_t>(&spec)) {
    return base::BindOnce(
        [](size_t index, View* parent) -> View* {
          if (index >= parent->children().size()) {
            LOG(ERROR) << "NameChildView(int): Child index out of bounds; got "
                       << index << " but only " << parent->children().size()
                       << " children.";
            return nullptr;
          }
          return parent->children()[index];
        },
        *index);
  }

  return base::BindOnce(
      [](ViewMatcher matcher, View* parent) -> View* {
        auto* const result =
            FindMatchingView(parent, matcher, /*recursive =*/false);
        LOG_IF(ERROR, !result)
            << "NameChildView(ViewMatcher): No child matches matcher.";
        return result;
      },
      absl::get<ViewMatcher>(spec));
}

// static
View* InteractiveViewsTestApi::FindMatchingView(const View* from,
                                                ViewMatcher& matcher,
                                                bool recursive) {
  for (auto* const child : from->children()) {
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
  context_widget_ = widget;
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
  if (auto* point = absl::get_if<gfx::Point>(&spec)) {
    return base::BindOnce([](gfx::Point p, ui::TrackedElement*) { return p; },
                          *point);
  }

  if (auto** point = absl::get_if<gfx::Point*>(&spec)) {
    return base::BindOnce([](gfx::Point* p, ui::TrackedElement*) { return *p; },
                          base::Unretained(*point));
  }

  CHECK(absl::holds_alternative<AbsolutePositionCallback>(spec));
  return base::RectifyCallback<RelativePositionCallback>(
      std::move(absl::get<AbsolutePositionCallback>(spec)));
}

// static
InteractiveViewsTestApi::RelativePositionCallback
InteractiveViewsTestApi::GetPositionCallback(RelativePositionSpecifier spec) {
  if (auto* cb = absl::get_if<RelativePositionCallback>(&spec)) {
    return std::move(*cb);
  }

  CHECK(absl::holds_alternative<CenterPoint>(spec));
  return base::BindOnce([](ui::TrackedElement* el) {
    return el->AsA<views::TrackedElementViews>()
        ->view()
        ->GetBoundsInScreen()
        .CenterPoint();
  });
}

InteractiveViewsTestApi::StepBuilder
InteractiveViewsTestApi::CreateMouseFollowUpStep() {
  return std::move(
      StepBuilder()
          .SetElementID(kInteractiveTestPivotElementId)
          .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                   kMouseGestureCompleteEvent)
          .SetStartCallback(base::BindOnce(
              [](InteractiveViewsTestApi* test, ui::InteractionSequence* seq,
                 ui::TrackedElement*) {
                if (!test->test_impl().mouse_error_message_.empty()) {
                  LOG(ERROR) << test->test_impl().mouse_error_message_;
                  seq->FailForTesting();
                }
              },
              base::Unretained(this))));
}

InteractiveViewsTest::InteractiveViewsTest(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : ViewsTestBase(std::move(task_environment)) {}

InteractiveViewsTest::~InteractiveViewsTest() = default;

void InteractiveViewsTest::SetUp() {
  ViewsTestBase::SetUp();
  private_test_impl().DoTestSetUp();
}

void InteractiveViewsTest::TearDown() {
  private_test_impl().DoTestTearDown();
  ViewsTestBase::TearDown();
}

}  // namespace views::test
