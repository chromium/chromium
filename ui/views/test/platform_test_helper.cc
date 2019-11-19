// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {
namespace {

PlatformTestHelper::Factory g_test_helper_factory;

}  // namespace

PlatformTestHelper::PlatformTestHelper() = default;

PlatformTestHelper::~PlatformTestHelper() = default;

void PlatformTestHelper::set_factory(Factory factory) {
  DCHECK_NE(factory.is_null(), g_test_helper_factory.is_null());
  g_test_helper_factory = std::move(factory);
}

// static
std::unique_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return g_test_helper_factory.is_null()
             ? base::WrapUnique(new PlatformTestHelper)
             : g_test_helper_factory.Run();
}

#if defined(USE_AURA)
void PlatformTestHelper::SimulateNativeDestroy(Widget* widget) {
  delete widget->GetNativeView();
}
#endif

void PlatformTestHelper::InitializeContextFactory(
    ui::ContextFactory** context_factory,
    ui::ContextFactoryPrivate** context_factory_private) {
  const bool enable_pixel_output = false;
  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output);
  *context_factory = context_factories_->GetContextFactory();
  *context_factory_private = context_factories_->GetContextFactoryPrivate();
}

}  // namespace views
