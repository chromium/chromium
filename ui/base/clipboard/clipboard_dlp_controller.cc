// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_dlp_controller.h"

namespace ui {

// static
ClipboardDlpController* ClipboardDlpController::Get() {
  return g_clipboard_dlp_controller_;
}

// static
void ClipboardDlpController::DeleteInstance() {
  if (!g_clipboard_dlp_controller_)
    return;

  delete g_clipboard_dlp_controller_;
}

ClipboardDlpController::ClipboardDlpController() {
  g_clipboard_dlp_controller_ = this;
}

ClipboardDlpController::~ClipboardDlpController() {
  g_clipboard_dlp_controller_ = nullptr;
}

ClipboardDlpController* ClipboardDlpController::g_clipboard_dlp_controller_ =
    nullptr;

}  // namespace ui
