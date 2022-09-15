// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/scoped_keyboard_hook.h"

#include "ui/aura/window_tree_host.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace aura {

ScopedKeyboardHook::ScopedKeyboardHook() = default;

ScopedKeyboardHook::ScopedKeyboardHook(
    base::WeakPtr<WindowTreeHost> window_tree_host)
    : window_tree_host_(window_tree_host) {
  DCHECK(window_tree_host_);
}

ScopedKeyboardHook::~ScopedKeyboardHook() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (window_tree_host_)
    window_tree_host_->ReleaseSystemKeyEventCapture();
}

bool ScopedKeyboardHook::IsKeyLocked(ui::DomCode dom_code) {
  return window_tree_host_ && window_tree_host_->IsKeyLocked(dom_code);
}

}  // namespace aura
