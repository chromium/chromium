// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/capture_controller.h"

#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"

namespace wm {

// static
CaptureController* CaptureController::instance_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// CaptureController, public:

CaptureController::CaptureController()
    : capture_window_(nullptr), capture_delegate_(nullptr) {
  DCHECK(!instance_);
  instance_ = this;
}

CaptureController::~CaptureController() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void CaptureController::Attach(aura::Window* root) {
  DCHECK_EQ(0u, delegates_.count(root));
  delegates_[root] = root->GetHost()->dispatcher();
  aura::client::SetCaptureClient(root, this);
}

void CaptureController::Detach(aura::Window* root) {
  delegates_.erase(root);
  aura::client::SetCaptureClient(root, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// CaptureController, aura::client::CaptureClient implementation:

void CaptureController::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;

  // Make sure window has a root window.
  DCHECK(!new_capture_window || new_capture_window->GetRootWindow());
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;
  aura::client::CaptureDelegate* old_capture_delegate = capture_delegate_;

  // Copy the map in case it's modified out from under us.
  std::map<aura::Window*, aura::client::CaptureDelegate*> delegates =
      delegates_;

  // If we're starting a new capture, cancel all touches that aren't
  // targeted to the capturing window.
  if (new_capture_window) {
    // Cancelling touches might cause |new_capture_window| to get deleted.
    // Track |new_capture_window| and check if it still exists before
    // committing |capture_window_|.
    aura::WindowTracker tracker;
    tracker.Add(new_capture_window);
    aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
        new_capture_window);
    if (!tracker.Contains(new_capture_window))
      new_capture_window = nullptr;
  }

  capture_window_ = new_capture_window;
  aura::Window* capture_root_window =
      capture_window_ ? capture_window_->GetRootWindow() : nullptr;
  capture_delegate_ = delegates_.find(capture_root_window) == delegates_.end()
                          ? nullptr
                          : delegates_[capture_root_window];

  for (const auto& it : delegates)
    it.second->UpdateCapture(old_capture_window, new_capture_window);

  if (capture_delegate_ != old_capture_delegate) {
    if (old_capture_delegate)
      old_capture_delegate->ReleaseNativeCapture();
    if (capture_delegate_)
      capture_delegate_->SetNativeCapture();
  }

  for (aura::client::CaptureClientObserver& observer : observers_)
    observer.OnCaptureChanged(old_capture_window, capture_window_);
}

void CaptureController::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(nullptr);
}

aura::Window* CaptureController::GetCaptureWindow() {
  return capture_window_;
}

aura::Window* CaptureController::GetGlobalCaptureWindow() {
  return capture_window_;
}

void CaptureController::AddObserver(
    aura::client::CaptureClientObserver* observer) {
  observers_.AddObserver(observer);
}

void CaptureController::RemoveObserver(
    aura::client::CaptureClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// ScopedCaptureClient:

ScopedCaptureClient::ScopedCaptureClient(aura::Window* root)
    : root_window_(root) {
  root->AddObserver(this);
  CaptureController::Get()->Attach(root);
}

ScopedCaptureClient::~ScopedCaptureClient() {
  Shutdown();
}

void ScopedCaptureClient::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, root_window_);
  Shutdown();
}

void ScopedCaptureClient::Shutdown() {
  if (!root_window_)
    return;

  root_window_->RemoveObserver(this);
  CaptureController::Get()->Detach(root_window_);
  root_window_ = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// CaptureController::TestApi

void ScopedCaptureClient::TestApi::SetDelegate(
    aura::client::CaptureDelegate* delegate) {
  CaptureController::Get()->delegates_[client_->root_window_] = delegate;
}

}  // namespace wm
