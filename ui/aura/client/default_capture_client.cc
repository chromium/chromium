// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/default_capture_client.h"

#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace client {
namespace {

// Track the active capture window across root windows.
Window* global_capture_window_ = nullptr;

}  // namespace

DefaultCaptureClient::DefaultCaptureClient(Window* root_window)
    : root_window_(root_window), capture_window_(nullptr) {
  if (root_window_)
    SetCaptureClient(root_window_, this);
}

DefaultCaptureClient::~DefaultCaptureClient() {
  if (global_capture_window_ == capture_window_)
    global_capture_window_ = nullptr;
  if (root_window_)
    SetCaptureClient(root_window_, nullptr);
}

void DefaultCaptureClient::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  if (window)
    Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(window);

  Window* old_capture_window = capture_window_;
  capture_window_ = window;
  global_capture_window_ = window;

  CaptureDelegate* capture_delegate = nullptr;
  if (capture_window_) {
    DCHECK(!root_window_ || root_window_ == capture_window_->GetRootWindow());
    capture_delegate = capture_window_->GetHost()->dispatcher();
    capture_delegate->SetNativeCapture();
  } else {
    DCHECK(!root_window_ ||
           root_window_ == old_capture_window->GetRootWindow());
    capture_delegate = old_capture_window->GetHost()->dispatcher();
    capture_delegate->ReleaseNativeCapture();
  }

  capture_delegate->UpdateCapture(old_capture_window, capture_window_);

  for (CaptureClientObserver& observer : observers_)
    observer.OnCaptureChanged(old_capture_window, capture_window_);
}

void DefaultCaptureClient::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

Window* DefaultCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

Window* DefaultCaptureClient::GetGlobalCaptureWindow() {
  return global_capture_window_;
}

void DefaultCaptureClient::AddObserver(CaptureClientObserver* observer) {
  observers_.AddObserver(observer);
}

void DefaultCaptureClient::RemoveObserver(CaptureClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace client
}  // namespace aura
