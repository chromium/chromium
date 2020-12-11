// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
#define UI_AURA_TEST_TEST_FOCUS_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace test {

class TestFocusClient : public client::FocusClient,
                        public WindowObserver {
 public:
  explicit TestFocusClient(Window* root_window);
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

  Window* root_window_;
  Window* focused_window_ = nullptr;
  ScopedObserver<Window, WindowObserver> observer_manager_{this};
  base::ObserverList<aura::client::FocusChangeObserver>::Unchecked
      focus_observers_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_FOCUS_CLIENT_H_
