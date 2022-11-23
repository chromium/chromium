// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
#define UI_AURA_TEST_TEST_FOCUS_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace test {

class TestFocusClient : public client::FocusClient,
                        public WindowObserver {
 public:
  explicit TestFocusClient(Window* root_window);

  TestFocusClient(const TestFocusClient&) = delete;
  TestFocusClient& operator=(const TestFocusClient&) = delete;

  ~TestFocusClient() override;

 private:
  // Overridden from client::FocusClient:
  void AddObserver(client::FocusChangeObserver* observer) override;
  void RemoveObserver(client::FocusChangeObserver* observer) override;
  void FocusWindow(Window* window) override;
  void ResetFocusWithinActiveWindow(Window* window) override;
  Window* GetFocusedWindow() override;

  // Overridden from WindowObserver:
  void OnWindowDestroying(Window* window) override;

  raw_ptr<Window> root_window_;
  raw_ptr<Window> focused_window_ = nullptr;
  base::ScopedObservation<Window, WindowObserver> observation_manager_{this};
  base::ObserverList<aura::client::FocusChangeObserver> focus_observers_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
