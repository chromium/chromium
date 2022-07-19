// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/linux_input_method_context_factory.h"

#include "base/no_destructor.h"
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/linux/linux_ui.h"

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

  // Next, give the ozone platform a chance.
  if (auto factory = GetInputMethodContextFactoryForOzone())
    if (auto context = factory.Run(delegate))
      return context;

  // Finally, let the toolkit create the context.
  if (auto* linux_ui = LinuxUi::instance()) {
    if (auto context = linux_ui->CreateInputMethodContext(delegate))
      return context;
  }

  // As a last resort, use a fake context.
  return std::make_unique<FakeInputMethodContext>();
}

}  // namespace ui
