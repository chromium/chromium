// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_interactive_test.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/input_protection/input_protection_specification.h"
#include "ui/views/input_protection/widget_stationarity_monitor.h"
#include "ui/views/metrics.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views::test {

namespace {

// Helper class to wait for a widget bounds change.
class WidgetBoundsWaiter : public WidgetObserver {
 public:
  WidgetBoundsWaiter(Widget* widget, const gfx::Rect& target_bounds)
      : target_bounds_(target_bounds) {
    observation_.Observe(widget);
    if (widget->GetWindowBoundsInScreen() == target_bounds_) {
      finished_ = true;
    }
  }
  WidgetBoundsWaiter(const WidgetBoundsWaiter&) = delete;
  WidgetBoundsWaiter& operator=(const WidgetBoundsWaiter&) = delete;
  ~WidgetBoundsWaiter() override = default;

  void Wait() {
    if (!finished_) {
      run_loop_.Run();
    }
  }

 private:
  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect& bounds) override {
    if (widget->GetWindowBoundsInScreen() == target_bounds_) {
      finished_ = true;
      if (run_loop_.running()) {
        run_loop_.Quit();
      }
    }
  }

  const gfx::Rect target_bounds_;
  bool finished_ = false;
  base::RunLoop run_loop_;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

std::unique_ptr<Widget> CreateAotWidget(View* target_view,
                                        const gfx::Rect& screen_bounds) {
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  if (target_view && target_view->GetWidget()) {
    params.context = target_view->GetWidget()->GetNativeWindow();
  }
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.bounds = screen_bounds;
  auto widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  return widget;
}

// Returns `click_point` (or the center point of `view` if omitted) converted to
// the `RootView` coordinate space of the target widget.
gfx::Point GetPointInRootView(View* view,
                              const std::optional<gfx::Point>& click_point) {
  gfx::Point point = click_point.value_or(view->GetLocalBounds().CenterPoint());
  View::ConvertPointToTarget(view, view->GetWidget()->GetRootView(), &point);
  return point;
}

// Event dispatch helpers below use `ui::EventTimeForNow()` so the event
// timestamp is generated from the same mock clock that tracks input protection
// cooldowns.
void DispatchMousePressOnly(
    View* view,
    const std::optional<gfx::Point>& click_point = std::nullopt) {
  gfx::Point point = GetPointInRootView(view, click_point);
  ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&press);
}

void DispatchMouseReleaseOnly(
    View* view,
    const std::optional<gfx::Point>& click_point = std::nullopt) {
  gfx::Point point = GetPointInRootView(view, click_point);
  ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&release);
}

void DispatchClick(View* view, const std::optional<gfx::Point>& click_point) {
  DispatchMousePressOnly(view, click_point);
  DispatchMouseReleaseOnly(view, click_point);
}

void DispatchKeyPressOnly(View* view, ui::KeyboardCode key, int flags) {
  ui::KeyEvent press(ui::EventType::kKeyPressed, key, flags,
                     ui::EventTimeForNow());
  view->GetWidget()->OnKeyEvent(&press);
}

void DispatchKeyReleaseOnly(View* view, ui::KeyboardCode key, int flags) {
  ui::KeyEvent release(ui::EventType::kKeyReleased, key, flags,
                       ui::EventTimeForNow());
  view->GetWidget()->OnKeyEvent(&release);
}

void DispatchKeyPressAndRelease(View* view, ui::KeyboardCode key, int flags) {
  DispatchKeyPressOnly(view, key, flags);
  DispatchKeyReleaseOnly(view, key, flags);
}

void DispatchTouchTap(View* view, const std::optional<gfx::Point>& tap_point) {
  gfx::Point root_point = GetPointInRootView(view, tap_point);
  ui::GestureEventDetails tap_details(ui::EventType::kGestureTap);
  ui::GestureEvent tap(root_point.x(), root_point.y(), 0, ui::EventTimeForNow(),
                       tap_details);
  view->GetWidget()->OnGestureEvent(&tap);
}

template <typename Factory>
void EnableProtectionOnWidget(Widget* widget,
                              const std::vector<Factory>& factories,
                              View* view = nullptr) {
  if (!widget) {
    return;
  }
  if (factories.empty()) {
    widget->EnableInputEventActivationProtection();
    return;
  }
  auto run_factory = [&](const auto& factory) {
    if constexpr (std::is_same_v<
                      Factory, InputProtectionTestApi::PolicyFactory<View&>>) {
      CHECK(view);
      return factory.Run(*view);
    } else if constexpr (std::is_same_v<
                             Factory,
                             InputProtectionTestApi::PolicyFactory<Widget*>>) {
      return factory.Run(widget);
    } else {
      return factory.Run();
    }
  };
  auto protector = std::make_unique<InputEventActivationProtector>(
      run_factory(factories[0]));
  for (size_t i = 1; i < factories.size(); ++i) {
    protector->AddPolicy(run_factory(factories[i]));
  }
  widget->EnableInputEventActivationProtection(std::move(protector));
}

}  // namespace

InputProtectionTestApi::InputProtectionTestApi() = default;
InputProtectionTestApi::~InputProtectionTestApi() = default;

template <typename Factory>
ui::InteractionSequence::StepBuilder
InputProtectionTestApi::EnableInputEventActivationProtection(
    ui::ElementIdentifier element_id,
    std::vector<Factory> policy_factories) {
  if (element_id) {
    auto step = WithView(
        element_id, [factories = std::move(policy_factories)](View* view) {
          EnableProtectionOnWidget(view->GetWidget(), factories, view);
        });
    step.SetDescription("EnableInputEventActivationProtection()");
    return step;
  }
  auto step = Do([this, factories = std::move(policy_factories)]() {
    EnableProtectionOnWidget(context_widget(), factories);
  });
  step.SetDescription("EnableInputEventActivationProtection()");
  return step;
}

template ui::InteractionSequence::StepBuilder
    InputProtectionTestApi::EnableInputEventActivationProtection<
        InputProtectionTestApi::PolicyFactory<View&>>(
        ui::ElementIdentifier,
        std::vector<PolicyFactory<View&>>);
template ui::InteractionSequence::StepBuilder
    InputProtectionTestApi::EnableInputEventActivationProtection<
        InputProtectionTestApi::PolicyFactory<Widget*>>(
        ui::ElementIdentifier,
        std::vector<PolicyFactory<Widget*>>);
template ui::InteractionSequence::StepBuilder
    InputProtectionTestApi::EnableInputEventActivationProtection<
        InputProtectionTestApi::PolicyFactory<>>(ui::ElementIdentifier,
                                                 std::vector<PolicyFactory<>>);

InputProtectionTestApi::MultiStep InputProtectionTestApi::TriggerShowCooldown(
    ui::ElementIdentifier element_id) {
  auto steps = Steps(WithView(element_id,
                              [](View* view) {
                                if (view->GetWidget()) {
                                  view->GetWidget()->Hide();
                                  view->GetWidget()->Show();
                                }
                              })
                         .SetMustRemainVisible(false),
                     WaitForShow(element_id), ActivateSurface(element_id));
  AddDescriptionPrefix(steps, "TriggerShowCooldown()");
  return steps;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::TriggerViewVisibilityCooldown(
    ui::ElementIdentifier element_id) {
  auto step = WithView(element_id, [](View* view) {
    view->SetVisible(false);
    view->SetVisible(true);
  });
  step.SetMustRemainVisible(false);
  step.SetDescription("TriggerViewVisibilityCooldown()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::MoveViewBy(
    ui::ElementIdentifier element_id,
    const gfx::Vector2d& offset) {
  auto step = WithView(element_id, [offset](View* view) {
    view->SetPosition(view->origin() + offset);
  });
  step.SetDescription("MoveViewBy()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::ResizeViewBy(
    ui::ElementIdentifier element_id,
    const gfx::Vector2d& size_delta) {
  auto step = WithView(element_id, [size_delta](View* view) {
    view->SetSize(gfx::Size(view->width() + size_delta.x(),
                            view->height() + size_delta.y()));
  });
  step.SetDescription("ResizeViewBy()");
  return step;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::MousePress(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchMousePressOnly(view, click_point);
  }));
  AddDescriptionPrefix(steps, "MousePress()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::MouseRelease(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchMouseReleaseOnly(view, click_point);
  }));
  AddDescriptionPrefix(steps, "MouseRelease()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::Click(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchClick(view, click_point);
  }));
  AddDescriptionPrefix(steps, "Click()");
  return steps;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::ClickExpectingBlocked(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    std::optional<gfx::Point> click_point) {
  auto step = WithView(element_id, [&action_counter, click_point](View* view) {
    const int initial_count = action_counter;
    DispatchClick(view, click_point);
    EXPECT_EQ(action_counter, initial_count);
  });
  step.SetDescription("ClickExpectingBlocked()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::ClickExpectingAllowed(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    std::optional<gfx::Point> click_point) {
  auto step = WithView(element_id, [&action_counter, click_point](View* view) {
    const int initial_count = action_counter;
    DispatchClick(view, click_point);
    EXPECT_EQ(action_counter, initial_count + 1);
  });
  step.SetDescription("ClickExpectingAllowed()");
  return step;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyPress(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyPressOnly(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyPress()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyRelease(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyReleaseOnly(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyRelease()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyPressAndRelease(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyPressAndRelease(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyPressAndRelease()");
  return steps;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::KeyPressAndReleaseExpectingBlocked(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    const int& action_counter,
    int flags) {
  auto step = WithView(element_id, [&action_counter, key, flags](View* view) {
    const int initial_count = action_counter;
    DispatchKeyPressAndRelease(view, key, flags);
    EXPECT_EQ(action_counter, initial_count);
  });
  step.SetDescription("KeyPressAndReleaseExpectingBlocked()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::KeyPressAndReleaseExpectingAllowed(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    const int& action_counter,
    int flags) {
  auto step = WithView(element_id, [&action_counter, key, flags](View* view) {
    const int initial_count = action_counter;
    DispatchKeyPressAndRelease(view, key, flags);
    EXPECT_EQ(action_counter, initial_count + 1);
  });
  step.SetDescription("KeyPressAndReleaseExpectingAllowed()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::TouchTap(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> tap_point) {
  auto step = WithView(element_id, [tap_point](View* view) {
    DispatchTouchTap(view, tap_point);
  });
  step.SetDescription("TouchTap()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::TouchTapExpectingBlocked(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    std::optional<gfx::Point> tap_point) {
  auto step = WithView(element_id, [&action_counter, tap_point](View* view) {
    const int initial_count = action_counter;
    DispatchTouchTap(view, tap_point);
    EXPECT_EQ(action_counter, initial_count);
  });
  step.SetDescription("TouchTapExpectingBlocked()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::TouchTapExpectingAllowed(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    std::optional<gfx::Point> tap_point) {
  auto step = WithView(element_id, [&action_counter, tap_point](View* view) {
    const int initial_count = action_counter;
    DispatchTouchTap(view, tap_point);
    EXPECT_EQ(action_counter, initial_count + 1);
  });
  step.SetDescription("TouchTapExpectingAllowed()");
  return step;
}

void InputProtectionTestApi::FastForwardMockClock(base::TimeDelta delta) {
  NOTREACHED()
      << "`FastForwardMockClock` must be overridden by a derived test fixture "
         "or mixin (such as `InputProtectionInteractiveTestMixin`) to advance "
         "mock time.";
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::AdvanceClockBy(
    base::TimeDelta delta) {
  auto step = Do([this, delta]() { FastForwardMockClock(delta); });
  step.SetDescription(
      base::StrCat({"AdvanceClockBy(",
                    base::NumberToString(delta.InMilliseconds()), " ms)"}));
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::AdvancePastInputProtectionInterval() {
  auto step =
      AdvanceClockBy(views::GetDoubleClickInterval() + base::Milliseconds(50));
  step.SetDescription("AdvancePastInputProtectionInterval()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::OccludeElementWithAotWindow(
    ui::ElementIdentifier element_id,
    ui::ElementIdentifier button_id,
    base::RepeatingClosure on_button_clicked) {
  auto step = WithView(
      element_id,
      [this, button_id,
       on_button_clicked = std::move(on_button_clicked)](View* view) {
        constexpr int kAotOutsetStep = 20;
        const int margin =
            kAotOutsetStep * (static_cast<int>(aot_widgets_.size()) + 1);
        gfx::Rect screen_bounds = view->GetBoundsInScreen();
        if (button_id) {
          screen_bounds.Outset(margin);
        }
        auto widget = CreateAotWidget(view, screen_bounds);
        if (button_id) {
          constexpr gfx::Rect kButtonBounds{10, 10, 80, 30};
          auto contents = std::make_unique<View>();
          auto button =
              std::make_unique<LabelButton>(on_button_clicked, u"AOT Button");
          button->SetProperty(kElementIdentifierKey, button_id);
          button->SetBoundsRect(kButtonBounds);
          contents->AddChildView(std::move(button));
          widget->SetContentsView(std::move(contents));
        }
        widget->Show();
        WidgetVisibleWaiter(widget.get()).Wait();
        aot_widgets_.push_back(std::move(widget));
      });
  step.SetDescription("OccludeElementWithAotWindow()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::OccludeRectWithAotWindow(
    ui::ElementIdentifier element_id,
    const gfx::Rect& local_bounds) {
  auto step = WithView(element_id, [this, local_bounds](View* view) {
    gfx::Rect screen_bounds = local_bounds;
    View::ConvertRectToScreen(view, &screen_bounds);
    auto widget = CreateAotWidget(view, screen_bounds);
    widget->Show();
    WidgetVisibleWaiter(widget.get()).Wait();
    aot_widgets_.push_back(std::move(widget));
  });
  step.SetDescription("OccludeRectWithAotWindow()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::HideAotWindows() {
  auto step = Do([this]() {
    CHECK(!aot_widgets_.empty());
    for (auto& widget : aot_widgets_) {
      if (widget) {
        widget->Hide();
      }
    }
  });
  step.SetDescription("HideAotWindows()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::HideAotWindow(
    ui::ElementIdentifier element_id) {
  auto step = WithView(element_id, [](View* view) {
    if (auto* widget = view->GetWidget()) {
      widget->Hide();
    }
  });
  step.SetContext(ui::InteractionSequence::ContextMode::kAny);
  step.SetMustRemainVisible(false);
  step.SetDescription("HideAotWindow()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::MoveAotWindowsToUnocclude(
    ui::ElementIdentifier element_id) {
  auto step = WithView(element_id, [this](View* view) {
    CHECK(!aot_widgets_.empty());
    const gfx::Rect element_bounds = view->GetBoundsInScreen();
    const gfx::Point new_origin =
        element_bounds.top_right() + gfx::Vector2d(50, 0);
    for (auto& widget : aot_widgets_) {
      if (!widget) {
        continue;
      }
      const gfx::Rect aot_bounds = widget->GetWindowBoundsInScreen();
      const gfx::Rect target_bounds(new_origin, aot_bounds.size());
      WidgetBoundsWaiter waiter(widget.get(), target_bounds);
      widget->SetBounds(target_bounds);
      waiter.Wait();
    }
  });
  step.SetDescription("MoveAotWindowsToUnocclude()");
  return step;
}

InputProtectionTestApi::MultiStep
InputProtectionTestApi::TriggerAotPopAwayAttack(
    ui::ElementIdentifier element_id) {
  auto steps = Steps(OccludeElementWithAotWindow(element_id), HideAotWindows());
  AddDescriptionPrefix(steps, "TriggerAotPopAwayAttack()");
  return steps;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::ShowBubbleDialog(
    ui::ElementIdentifier anchor_element_id,
    ui::ElementIdentifier button_id,
    base::RepeatingClosure on_button_clicked,
    std::vector<PolicyFactory<Widget*>> policy_factories) {
  auto step = WithView(
      anchor_element_id,
      [this, button_id, on_button_clicked = std::move(on_button_clicked),
       factories = std::move(policy_factories)](View* anchor_view) {
        bubble_delegate_ = std::make_unique<BubbleDialogDelegate>(
            anchor_view, BubbleBorder::TOP_LEFT);
        bubble_delegate_->set_close_on_deactivate(false);
        auto button =
            std::make_unique<LabelButton>(on_button_clicked, u"Bubble Button");
        button->SetProperty(kElementIdentifierKey, button_id);
        bubble_delegate_->SetContentsView(std::move(button));
        bubble_widget_ =
            BubbleDialogDelegate::CreateBubble(bubble_delegate_.get());
        EnableProtectionOnWidget(bubble_widget_.get(), factories);
        bubble_widget_->Show();
        WidgetVisibleWaiter(bubble_widget_.get()).Wait();
      });
  step.SetDescription("ShowBubbleDialog()");
  return step;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::HideWindow(
    ui::ElementIdentifier element_id) {
  auto steps = InAnyContext(WithView(element_id,
                                     [this, element_id](View* view) {
                                       if (auto* widget = view->GetWidget()) {
                                         hidden_widgets_[element_id] = widget;
                                         widget->Hide();
                                       }
                                     }),
                            WaitForHide(element_id));
  AddDescriptionPrefix(steps, "HideWindow()");
  return steps;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::ShowAuxiliaryWindow(ui::ElementIdentifier button_id) {
  auto step = Do([this, button_id]() {
    Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                              Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    if (context_widget()) {
      params.context = context_widget()->GetNativeWindow();
    }
    params.bounds = gfx::Rect(500, 100, 200, 100);
    auto widget = std::make_unique<Widget>();
    widget->Init(std::move(params));
    auto contents = std::make_unique<View>();
    auto button =
        std::make_unique<LabelButton>(Button::PressedCallback(), u"Aux Button");
    button->SetProperty(kElementIdentifierKey, button_id);
    button->SetBoundsRect(gfx::Rect(10, 10, 80, 30));
    contents->AddChildView(std::move(button));
    widget->SetContentsView(std::move(contents));
    widget->Show();
    WidgetVisibleWaiter(widget.get()).Wait();
    auxiliary_widgets_.push_back(std::move(widget));
  });
  step.SetDescription("ShowAuxiliaryWindow()");
  return step;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::ShowWindow(
    ui::ElementIdentifier element_id) {
  auto steps = InAnyContext(Do([this, element_id]() {
                              auto it = hidden_widgets_.find(element_id);
                              CHECK(it != hidden_widgets_.end());
                              if (it->second) {
                                it->second->Show();
                              }
                              hidden_widgets_.erase(it);
                            }),
                            WaitForShow(element_id));
  AddDescriptionPrefix(steps, "ShowWindow()");
  return steps;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::TrackWidgetStationarity(
    ui::ElementIdentifier element_id) {
  auto step = WithView(element_id, [](View* view) {
    Widget* widget = view->GetWidget();
    CHECK(widget);
    WidgetStationarityMonitor::GetInstance().TrackWidget(*widget);
  });
  step.SetDescription("TrackWidgetStationarity()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::MoveWidgetBy(
    ui::ElementIdentifier element_id,
    const gfx::Vector2d& offset) {
  auto step = WithView(element_id, [offset](View* view) {
    Widget* widget = view->GetWidget();
    CHECK(widget);
    const gfx::Rect target_bounds = widget->GetWindowBoundsInScreen() + offset;
    WidgetBoundsWaiter waiter(widget, target_bounds);
    widget->SetBounds(target_bounds);
    waiter.Wait();
  });
  step.SetDescription("MoveWidgetBy()");
  return step;
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::ResizeWidgetBy(
    ui::ElementIdentifier element_id,
    const gfx::Vector2d& size_delta) {
  auto step = WithView(element_id, [size_delta](View* view) {
    Widget* widget = view->GetWidget();
    CHECK(widget);
    gfx::Rect target_bounds = widget->GetWindowBoundsInScreen();
    target_bounds.set_size(gfx::Size(target_bounds.width() + size_delta.x(),
                                     target_bounds.height() + size_delta.y()));
    WidgetBoundsWaiter waiter(widget, target_bounds);
    widget->SetBounds(target_bounds);
    waiter.Wait();
  });
  step.SetDescription("ResizeWidgetBy()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::InstallInputProtectionSpecification(
    ui::ElementIdentifier element_id,
    InputProtectionSpecification::GetBoundsCallback<View> callback) {
  auto step = WithView(
      element_id, [callback = std::move(callback)](View* view) mutable {
        InputProtectionSpecification::Install(*view, std::move(callback));
      });
  step.SetDescription("InstallInputProtectionSpecification()");
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::AdvanceHalfwayThroughInputProtectionInterval() {
  auto step = AdvanceClockBy(views::GetDoubleClickInterval() / 2);
  step.SetDescription("AdvanceHalfwayThroughInputProtectionInterval()");
  return step;
}

}  // namespace views::test
