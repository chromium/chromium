// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/default_event_generator_delegate.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace test {

DefaultEventGeneratorDelegate::DefaultEventGeneratorDelegate(
    gfx::NativeWindow root_window)
    : root_window_(root_window) {}

void DefaultEventGeneratorDelegate::SetTargetWindow(
    gfx::NativeWindow target_window) {
  root_window_ = target_window->GetRootWindow();
}

ui::EventTarget* DefaultEventGeneratorDelegate::GetTargetAt(
    const gfx::Point& location) {
  return root_window_->GetHost()->window();
}

client::ScreenPositionClient*
DefaultEventGeneratorDelegate::GetScreenPositionClient(
    const Window* window) const {
  return client::GetScreenPositionClient(root_window_);
}

}  // namespace test
}  // namespace aura
