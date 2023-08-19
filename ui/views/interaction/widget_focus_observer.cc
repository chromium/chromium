// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/widget_focus_observer.h"

#include "ui/base/interaction/state_observer.h"

namespace views::test {

WidgetFocusObserver::WidgetFocusObserver()
    : ObservationStateObserver(views::WidgetFocusManager::GetInstance()) {}
WidgetFocusObserver::~WidgetFocusObserver() = default;

void WidgetFocusObserver::OnNativeFocusChanged(gfx::NativeView focused_now) {
  OnStateObserverStateChanged(focused_now);
}

DEFINE_STATE_IDENTIFIER_VALUE(kCurrentWidgetFocus);

}  // namespace views::test
