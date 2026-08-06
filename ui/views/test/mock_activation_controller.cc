// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/mock_activation_controller.h"

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_activation_delegate.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#endif

namespace views::test {
namespace {

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
DesktopNativeWidgetAura* GetNativeWidget(Widget* widget) {
  CHECK(widget->GetIsDesktopWidget());
  auto* native_widget_private = widget->native_widget_private();
  return static_cast<DesktopNativeWidgetAura*>(native_widget_private);
}

#elif BUILDFLAG(IS_MAC)
NativeWidgetMac* GetNativeWidget(Widget* widget) {
  auto* native_widget_private = widget->native_widget_private();
  return static_cast<NativeWidgetMac*>(native_widget_private);
}
#endif

void SetActivationState(Widget* widget, bool active) {
  CHECK(widget);
  if (widget->is_destroying()) {
    CHECK(!active);
    return;
  }
  auto* native_widget = GetNativeWidget(widget);
  CHECK(native_widget);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  native_widget->HandleActivationChanged(active);
#else
  native_widget->OnWindowKeyStatusChanged(active, active);
#endif
}

}  // namespace

MockActivationController::MockActivationController(
    bool allow_in_interactive_ui_tests) {
  CHECK(allow_in_interactive_ui_tests || !ui_controls::IsUIControlsEnabled());
  const auto widgets = WidgetTest::GetAllWidgets();
  CHECK(widgets.empty())
      << "MockActivationController created when widgets already exist: "
      << base::JoinString(base::ToVector(widgets, &Widget::GetName), ", ");
}

MockActivationController::~MockActivationController() {
  for (auto widget : widgets_) {
    widget->RemoveObserver(this);
  }
}

void MockActivationController::MaybeActivate(Widget* widget, bool activate) {
  CHECK(widget);
  CHECK(widget->is_top_level());
  auto current_active = active_widget_;

  // Do not change the activation even if activate == false.
  if (current_active == widget) {
    return;
  }

  auto iter = std::ranges::find(widgets_, widget);
  if (iter == widgets_.end()) {
    widget->AddObserver(this);
    widgets_.push_back(widget);
  } else if (activate) {
    // Move the widget to the top.
    widgets_.erase(iter);
    widgets_.push_back(widget);
  }

  if (!activate) {
    // no need to update active_widget_;
    return;
  }
  CHECK(widget->CanActivate());

  active_widget_ = widget;
  if (current_active) {
    SetActivationState(current_active, false);
  }
  SetActivationState(widget, true);
}

void MockActivationController::Deactivate(Widget* widget) {
  CHECK(widget);
  CHECK(widget->is_top_level());
  if (active_widget_ == widget) {
    Widget* next_active = nullptr;
    if (widgets_.size() > 1) {
      next_active = FindActivatableWidget(widget);
      if (next_active) {
        auto iter = std::ranges::find(widgets_, widget);
        CHECK(iter != widgets_.end());
        widgets_.erase(iter);

        auto next_active_iter = std::ranges::find(widgets_, next_active);
        CHECK(next_active_iter != widgets_.end());
        widgets_.insert(next_active_iter, widget);
      }
    }

    active_widget_ = next_active;
    SetActivationState(widget, false);

    if (next_active) {
      SetActivationState(next_active, true);
    }
  }
}

bool MockActivationController::IsActive(const Widget* widget) {
  return widget == active_widget_;
}

bool MockActivationController::IsTrackedForTesting(const Widget* widget) const {
  return std::ranges::find(widgets_, widget) != widgets_.end();
}

void MockActivationController::OnWidgetDestroying(Widget* widget) {
  widget->RemoveObserver(this);
  auto iter = std::ranges::find(widgets_, widget);
  CHECK(iter != widgets_.end());
  widgets_.erase(iter);

  if (active_widget_ == widget) {
    active_widget_ = FindActivatableWidget();
    if (active_widget_) {
      SetActivationState(active_widget_, true);
    }
  }
}

void MockActivationController::OnWidgetVisibilityChanged(Widget* widget,
                                                         bool visible) {
  auto iter = std::ranges::find(widgets_, widget);
  if (iter == widgets_.end()) {
    widgets_.push_back(widget);
  }
  if (!visible && IsActive(widget)) {
    Deactivate(widget);
  }
}

Widget* MockActivationController::FindActivatableWidget(Widget* skip_widget) {
  for (auto iter = widgets_.rbegin(); iter != widgets_.rend(); iter++) {
    if (*iter != skip_widget && (*iter)->IsVisible() &&
        (*iter)->CanActivate()) {
      return *iter;
    }
  }
  return nullptr;
}

}  // namespace views::test
