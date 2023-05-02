// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_thread.h"

namespace wolvic {

WvrThread::WvrThread(base::OnceCallback<void()> initialized_callback)
    : base::android::JavaHandlerThread("WvrThread"),
      initialized_callback_(std::move(initialized_callback)) {}

WvrThread::~WvrThread() {
  Stop();
}

void WvrThread::Init() {
  DCHECK(!wvr_manager_);
  wvr_manager_ = std::make_unique<WvrManager>();

  std::move(initialized_callback_).Run();
}

void WvrThread::CleanUp() {
  wvr_manager_.reset();
}

}  // namespace wolvic
