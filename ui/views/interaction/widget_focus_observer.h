// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
#define UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_

#include "ui/base/interaction/state_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/focus/widget_focus_manager.h"

namespace views::test {

// Tracks focus as a StateObserver. Use ObserveState and WaitForState.
class WidgetFocusObserver
    : public ui::test::ObservationStateObserver<gfx::NativeView,
                                                WidgetFocusManager,
                                                WidgetFocusChangeListener> {
 public:
  WidgetFocusObserver();
  ~WidgetFocusObserver() override;

  // WidgetFocusChangeListener:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;
};

// Since there is only one WidgetFocusManager, there only ever needs to be one
// WidgetFocusObserver.
DECLARE_STATE_IDENTIFIER_VALUE(WidgetFocusObserver, kCurrentWidgetFocus);

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
