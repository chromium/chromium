// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/linux_input_method_context_factory.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/base/ime/linux/fake_input_method_context.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#endif

namespace ui {

LinuxInputMethodContextFactory& GetInputMethodContextFactoryForOzone() {
  static base::NoDestructor<LinuxInputMethodContextFactory> factory;
  return *factory;
}

LinuxInputMethodContextFactory& GetInputMethodContextFactoryForTest() {
  static base::NoDestructor<LinuxInputMethodContextFactory> factory;
  return *factory;
}

std::unique_ptr<LinuxInputMethodContext> CreateLinuxInputMethodContext(
    LinuxInputMethodContextDelegate* delegate) {
  // First, give the test-provided factory a chance to create the context.
  if (auto factory = GetInputMethodContextFactoryForTest())
    return factory.Run(delegate);

#if BUILDFLAG(IS_LINUX)
  // Give the toolkit a chance to create the context.
  if (auto* linux_ui = LinuxUi::instance()) {
    if (auto context = linux_ui->CreateInputMethodContext(delegate))
      return context;
  }
#endif

  // Finally, give the ozone platform a chance.
  if (auto factory = GetInputMethodContextFactoryForOzone()) {
    if (auto context = factory.Run(delegate))
      return context;
  }

  // As a last resort, use a fake context.
  return std::make_unique<FakeInputMethodContext>();
}

}  // namespace ui
