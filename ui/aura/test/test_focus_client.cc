// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_focus_client.h"

#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window.h"

namespace aura {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestFocusClient, public:

TestFocusClient::TestFocusClient(Window* root_window)
    : root_window_(root_window) {
  DCHECK(root_window_);
  client::SetFocusClient(root_window_, this);
}

TestFocusClient::~TestFocusClient() {
  client::SetFocusClient(root_window_, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// TestFocusClient, client::FocusClient implementation:

void TestFocusClient::AddObserver(client::FocusChangeObserver* observer) {
  focus_observers_.AddObserver(observer);
}

void TestFocusClient::RemoveObserver(client::FocusChangeObserver* observer) {
  focus_observers_.RemoveObserver(observer);
}

void TestFocusClient::FocusWindow(Window* window) {
  if (window && !window->CanFocus())
    return;

  if (focused_window_) {
    DCHECK(observation_manager_.IsObservingSource(focused_window_.get()));
    observation_manager_.Reset();
  }
  aura::Window* old_focused_window = focused_window_;
  focused_window_ = window;
  if (focused_window_)
    observation_manager_.Observe(focused_window_.get());

  focus_observers_.Notify(&aura::client::FocusChangeObserver::OnWindowFocused,
                          focused_window_, old_focused_window);

  client::FocusChangeObserver* observer =
      client::GetFocusChangeObserver(old_focused_window);
  if (observer)
    observer->OnWindowFocused(focused_window_, old_focused_window);
  observer = client::GetFocusChangeObserver(focused_window_);
  if (observer)
    observer->OnWindowFocused(focused_window_, old_focused_window);
}

void TestFocusClient::ResetFocusWithinActiveWindow(Window* window) {
  if (!window->Contains(focused_window_))
    FocusWindow(window);
}

Window* TestFocusClient::GetFocusedWindow() {
  return focused_window_;
}

////////////////////////////////////////////////////////////////////////////////
// TestFocusClient, WindowObserver implementation:

void TestFocusClient::OnWindowDestroying(Window* window) {
  DCHECK_EQ(window, focused_window_);
  FocusWindow(NULL);
}

}  // namespace test
}  // namespace aura
