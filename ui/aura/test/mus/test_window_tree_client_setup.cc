// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/test_window_tree_client_setup.h"

#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/test/window_occlusion_tracker_test_api.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/display/display.h"

namespace aura {

TestWindowTreeClientSetup::TestWindowTreeClientSetup() = default;

TestWindowTreeClientSetup::~TestWindowTreeClientSetup() {
  if (window_tree_)
    window_tree_->set_delegate(nullptr);
}

void TestWindowTreeClientSetup::Init(
    WindowTreeClientDelegate* window_tree_delegate) {
  CommonInit(window_tree_delegate);
  WindowTreeClientPrivate(window_tree_client_.get())
      .OnEmbed(window_tree_.get());
}

void TestWindowTreeClientSetup::InitWithoutEmbed(
    WindowTreeClientDelegate* window_tree_delegate) {
  CommonInit(window_tree_delegate);
  WindowTreeClientPrivate(window_tree_client_.get())
      .SetTree(window_tree_.get());
}

std::unique_ptr<WindowTreeClient>
TestWindowTreeClientSetup::OwnWindowTreeClient() {
  DCHECK(window_tree_client_);
  return std::move(window_tree_client_);
}

WindowTreeClient* TestWindowTreeClientSetup::window_tree_client() {
  return window_tree_client_.get();
}

void TestWindowTreeClientSetup::CommonInit(
    WindowTreeClientDelegate* window_tree_delegate) {
  window_tree_ = std::make_unique<TestWindowTree>();
  window_tree_->set_delegate(this);
  window_tree_client_ =
      WindowTreeClientPrivate::CreateWindowTreeClient(window_tree_delegate);
  window_tree_->set_client(window_tree_client_.get());

  window_occlusion_tracker_ = test::WindowOcclusionTrackerTestApi::Create();
}

void TestWindowTreeClientSetup::TrackOcclusionState(ws::Id window_id) {
  window_occlusion_tracker_->Track(
      WindowTreeClientPrivate(window_tree_client_.get())
          .GetWindowByServerId(window_id));
}

void TestWindowTreeClientSetup::PauseWindowOcclusionTracking() {
  test::WindowOcclusionTrackerTestApi(window_occlusion_tracker_.get()).Pause();
}

void TestWindowTreeClientSetup::UnpauseWindowOcclusionTracking() {
  test::WindowOcclusionTrackerTestApi(window_occlusion_tracker_.get())
      .Unpause();
}

}  // namespace aura
