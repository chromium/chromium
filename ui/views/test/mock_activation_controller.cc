// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/mock_activation_controller.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/views/buildflags.h"
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
  auto* native_widget = GetNativeWidget(widget);
  CHECK(native_widget);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  native_widget->HandleActivationChanged(active);
#else
  native_widget->OnWindowKeyStatusChanged(active, active);
#endif
}

}  // namespace

MockActivationController::MockActivationController() = default;

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
      auto iter = std::ranges::find(widgets_, widget);
      CHECK(iter != widgets_.end());
      widgets_.erase(iter);

      auto riter = FindActivatableWidget();
      if (riter != widgets_.rend()) {
        next_active = (*riter).get();
        // reverse iterator
        riter++;
        widgets_.insert(riter.base(), widget);
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

void MockActivationController::OnWidgetDestroying(Widget* widget) {
  widget->RemoveObserver(this);
  auto iter = std::ranges::find(widgets_, widget);
  CHECK(iter != widgets_.end());
  widgets_.erase(iter);

  if (active_widget_ == widget) {
    active_widget_ = nullptr;

    if (!widgets_.empty()) {
      auto riter = FindActivatableWidget();
      if (riter != widgets_.rend()) {
        active_widget_ = (*riter).get();
      }
    }
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

MockActivationController::WidgetList::reverse_iterator
MockActivationController::FindActivatableWidget() {
  for (auto iter = widgets_.rbegin(); iter != widgets_.rend(); iter++) {
    if ((*iter)->IsVisible() && (*iter)->CanActivate()) {
      return iter;
    }
  }
  return widgets_.rend();
}

}  // namespace views::test
