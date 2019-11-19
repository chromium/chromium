// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"

namespace views {

namespace {

// This comparator facilitates constructing DesktopCaptureClient WeakPtr sets.
bool CompareWeakPtrs(const base::WeakPtr<DesktopCaptureClient>& lhs,
                     const base::WeakPtr<DesktopCaptureClient>& rhs) {
  return lhs.get() < rhs.get();
}

}  // namespace

// static
DesktopCaptureClient::ClientSet* DesktopCaptureClient::clients_ = nullptr;

// static
aura::Window* DesktopCaptureClient::GetCaptureWindowGlobal() {
  for (const auto& client : *clients_) {
    if (client && client->capture_window_)
      return client->capture_window_;
  }
  return nullptr;
}

DesktopCaptureClient::DesktopCaptureClient(aura::Window* root) : root_(root) {
  if (!clients_)
    clients_ = new ClientSet(&CompareWeakPtrs);
  clients_->insert(weak_factory_.GetWeakPtr());
  aura::client::SetCaptureClient(root, this);
}

DesktopCaptureClient::~DesktopCaptureClient() {
  aura::client::SetCaptureClient(root_, nullptr);
  base::EraseIf(*clients_, [this](const auto& c) { return c.get() == this; });
}

void DesktopCaptureClient::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;

  // We should only ever be told to capture a child of |root_|. Otherwise
  // things are going to be really confused.
  DCHECK(!new_capture_window ||
         (new_capture_window->GetRootWindow() == root_));
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;

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

  aura::client::CaptureDelegate* delegate = root_->GetHost()->dispatcher();
  delegate->UpdateCapture(old_capture_window, new_capture_window);

  // Initiate native capture updating.
  if (!capture_window_) {
    delegate->ReleaseNativeCapture();
  } else if (!old_capture_window) {
    delegate->SetNativeCapture();

    // Notify the other roots that we got capture. This is important so that
    // they reset state. Clients may be destroyed during the loop.
    ClientSet clients(*clients_);
    for (auto client : clients) {
      if (client && client.get() != this) {
        aura::client::CaptureDelegate* client_delegate =
            client->root_->GetHost()->dispatcher();
        client_delegate->OnOtherRootGotCapture();
      }
    }
  }  // else case is capture is remaining in our root, nothing to do.

  for (auto& observer : observers_)
    observer.OnCaptureChanged(old_capture_window, capture_window_);
}

void DesktopCaptureClient::ReleaseCapture(aura::Window* window) {
  if (capture_window_ == window)
    SetCapture(nullptr);
}

aura::Window* DesktopCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

aura::Window* DesktopCaptureClient::GetGlobalCaptureWindow() {
  return GetCaptureWindowGlobal();
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
