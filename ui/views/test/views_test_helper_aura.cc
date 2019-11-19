// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_aura.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace views {

// static
ViewsTestHelper* ViewsTestHelper::Create(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new ViewsTestHelperAura(context_factory, context_factory_private);
}

ViewsTestHelperAura::ViewsTestHelperAura(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private)
    : context_factory_(context_factory),
      context_factory_private_(context_factory_private) {
  aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
}

ViewsTestHelperAura::~ViewsTestHelperAura() = default;

void ViewsTestHelperAura::SetUp() {
  aura_test_helper_->SetUp(context_factory_, context_factory_private_);

  // GetContext() may return null. See comment in GetContext().
  gfx::NativeWindow root_window = GetContext();
  if (!root_window)
    return;

  new wm::DefaultActivationClient(root_window);

  if (!aura::client::GetScreenPositionClient(root_window)) {
    screen_position_client_ =
        std::make_unique<wm::DefaultScreenPositionClient>();
    aura::client::SetScreenPositionClient(root_window,
                                          screen_position_client_.get());
  }
}

void ViewsTestHelperAura::TearDown() {
  // GetContext() may return null. See comment in GetContext().
  if (GetContext()) {
    // Ensure all Widgets (and windows) are closed in unit tests. This is done
    // automatically when the RootWindow is torn down, but is an error on
    // platforms that must ensure no Compositors are alive when the
    // ContextFactory is torn down.
    // So, although it's optional, check the root window to detect failures
    // before they hit the CQ on other platforms.
    DCHECK(aura_test_helper_->root_window()->children().empty())
        << "Not all windows were closed.";

    if (screen_position_client_.get() ==
        aura::client::GetScreenPositionClient(GetContext()))
      aura::client::SetScreenPositionClient(GetContext(), nullptr);
  }

  aura_test_helper_->TearDown();
  CHECK(!wm::CaptureController::Get() ||
        !wm::CaptureController::Get()->is_active());
}

gfx::NativeWindow ViewsTestHelperAura::GetContext() {
  return aura_test_helper_->root_window();
}

}  // namespace views
