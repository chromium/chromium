// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/init/input_method_initializer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/win/tsf_bridge.h"
#endif

namespace ui {

void InitializeInputMethod() {
#if !BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(IS_WIN)
  TSFBridge::Initialize();
#endif
}

void ShutdownInputMethod() {
#if !BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(IS_WIN)
  TSFBridge::Shutdown();
#endif
}

void InitializeInputMethodForTesting() {
#if defined(USE_AURA) && BUILDFLAG(IS_LINUX)
  GetInputMethodContextFactoryForTest() =
      base::BindRepeating([](LinuxInputMethodContextDelegate* delegate)
                              -> std::unique_ptr<LinuxInputMethodContext> {
        return std::make_unique<FakeInputMethodContext>();
      });
#elif BUILDFLAG(IS_WIN)
  TSFBridge::InitializeForTesting();
#endif
}

void ShutdownInputMethodForTesting() {
#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // The function owns the factory (as a static variable that's returned by
  // reference), so setting this to an empty factory will free the old one.
  GetInputMethodContextFactoryForTest() = LinuxInputMethodContextFactory();
#elif BUILDFLAG(IS_WIN)
  TSFBridge::Shutdown();
#endif
}

}  // namespace ui
