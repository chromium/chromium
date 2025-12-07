// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_MOCK_ACTIVATION_CHANGE_OBSERVER_H_
#define UI_WM_PUBLIC_MOCK_ACTIVATION_CHANGE_OBSERVER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

// A mock activation change observer for testing.
class MockActivationChangeObserver : public wm::ActivationChangeObserver {
 public:
  MockActivationChangeObserver();
  MockActivationChangeObserver(const MockActivationChangeObserver&);
  MockActivationChangeObserver& operator=(const MockActivationChangeObserver&);
  ~MockActivationChangeObserver() override;

  // wm::ActivationChangeObserver:
  MOCK_METHOD(void,
              OnAttemptToReactivateWindow,
              (aura::Window * gained_active, aura::Window* lost_active),
              (override));
  MOCK_METHOD(void,
              OnWindowActivated,
              (wm::ActivationChangeObserver::ActivationReason reason,
               aura::Window* gained_active,
               aura::Window* lost_active),
              (override));
  MOCK_METHOD(void,
              OnWindowActivating,
              (wm::ActivationChangeObserver::ActivationReason reason,
               aura::Window* gained_active,
               aura::Window* lost_active),
              (override));
};

#endif  // UI_WM_PUBLIC_MOCK_ACTIVATION_CHANGE_OBSERVER_H_
