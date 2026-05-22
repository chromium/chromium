// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace views {

namespace {

using CaptureChangedCallbackList =
    base::RepeatingCallbackList<void(DesktopCaptureClient*)>;

CaptureChangedCallbackList& GetCaptureChangedCallbacks() {
  static base::NoDestructor<CaptureChangedCallbackList> callbacks;
  return *callbacks;
}

DesktopCaptureClient* active_client_ = nullptr;

}  // namespace

// static
aura::Window* DesktopCaptureClient::GetCaptureWindowGlobal() {
  return active_client_ ? active_client_->capture_window_ : nullptr;
}

DesktopCaptureClient::DesktopCaptureClient(aura::Window* root) : root_(root) {
  subscription_ = GetCaptureChangedCallbacks().Add(base::BindRepeating(
      &DesktopCaptureClient::OnCaptureChanged, base::Unretained(this)));
  aura::client::SetCaptureClient(root, this);
}

DesktopCaptureClient::~DesktopCaptureClient() {
  aura::client::SetCaptureClient(root_, nullptr);
  if (active_client_ == this) {
    active_client_ = nullptr;
  }
}

void DesktopCaptureClient::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window) {
    return;
  }

  // We should only ever be told to capture a child of |root_|. Otherwise
  // things are going to be really confused.
  DCHECK(!new_capture_window || (new_capture_window->GetRootWindow() == root_));
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;

  // If we're starting a new capture, cancel all touches that aren't
  // targeted to the capturing window.
  if (new_capture_window) {
    // Cancelling touches might cause |new_capture_window| to get deleted.
    // Track |new_capture_window| and check if it still exists before
    // committing |capture_window_|.
    base::WeakPtr<aura::Window> new_capture_window_weak =
        new_capture_window->GetWeakPtrAsWindow();
    aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
        new_capture_window);
    if (!new_capture_window_weak) {
      new_capture_window = nullptr;
    }
  }

  capture_window_ = new_capture_window;
  if (capture_window_) {
    active_client_ = this;
  } else if (active_client_ == this) {
    active_client_ = nullptr;
  }

  aura::client::CaptureDelegate* delegate = root_->GetHost()->dispatcher();
  delegate->UpdateCapture(old_capture_window, new_capture_window);

  // Initiate native capture updating.
  if (!capture_window_) {
    delegate->ReleaseNativeCapture();
  } else if (!old_capture_window) {
    delegate->SetNativeCapture();

    // Notify the other roots that we got capture. This is important so that
    // they reset state.
    GetCaptureChangedCallbacks().Notify(this);
  }  // else case is capture is remaining in our root, nothing to do.

  observers_.Notify(&aura::client::CaptureClientObserver::OnCaptureChanged,
                    old_capture_window, capture_window_);
}

void DesktopCaptureClient::ReleaseCapture(aura::Window* window) {
  if (capture_window_ == window) {
    SetCapture(nullptr);
  }
}

aura::Window* DesktopCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

aura::Window* DesktopCaptureClient::GetGlobalCaptureWindow() {
  return GetCaptureWindowGlobal();
}

void DesktopCaptureClient::OnCaptureChanged(DesktopCaptureClient* client) {
  if (client != this) {
    aura::client::CaptureDelegate* client_delegate =
        root_->GetHost()->dispatcher();
    client_delegate->OnOtherRootGotCapture();
  }
}

void DesktopCaptureClient::AddObserver(
    aura::client::CaptureClientObserver* observer) {
  observers_.AddObserver(observer);
}

void DesktopCaptureClient::RemoveObserver(
    aura::client::CaptureClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace views
