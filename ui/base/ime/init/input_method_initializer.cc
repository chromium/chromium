// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/init/input_method_initializer.h"

#include <ostream>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ime/chromeos/ime_bridge.h"
#elif defined(USE_AURA) && (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "base/check.h"
#include "ui/base/ime/linux/fake_input_method_context_factory.h"
#elif defined(OS_WIN)
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/win/tsf_bridge.h"
#endif

namespace {

#if defined(USE_AURA) && (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
const ui::LinuxInputMethodContextFactory*
    g_linux_input_method_context_factory_for_testing;
#endif

}  // namespace

namespace ui {

void InitializeInputMethod() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  IMEBridge::Initialize();
#elif defined(OS_WIN)
  TSFBridge::Initialize();
#endif
}

void ShutdownInputMethod() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  IMEBridge::Shutdown();
#elif defined(OS_WIN)
  TSFBridge::Shutdown();
#endif
}

void InitializeInputMethodForTesting() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  IMEBridge::Initialize();
#elif defined(USE_AURA) && (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  if (!g_linux_input_method_context_factory_for_testing)
    g_linux_input_method_context_factory_for_testing =
        new FakeInputMethodContextFactory();
  const LinuxInputMethodContextFactory* factory =
      LinuxInputMethodContextFactory::instance();
  CHECK(!factory || factory == g_linux_input_method_context_factory_for_testing)
      << "LinuxInputMethodContextFactory was already initialized somewhere "
      << "else.";
  LinuxInputMethodContextFactory::SetInstance(
      g_linux_input_method_context_factory_for_testing);
#elif defined(OS_WIN)
  TSFBridge::InitializeForTesting();
#endif
}

void ShutdownInputMethodForTesting() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  IMEBridge::Shutdown();
#elif defined(USE_AURA) && (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  const LinuxInputMethodContextFactory* factory =
      LinuxInputMethodContextFactory::instance();
  CHECK(!factory || factory == g_linux_input_method_context_factory_for_testing)
      << "An unknown LinuxInputMethodContextFactory was set.";
  LinuxInputMethodContextFactory::SetInstance(nullptr);
  delete g_linux_input_method_context_factory_for_testing;
  g_linux_input_method_context_factory_for_testing = nullptr;
#elif defined(OS_WIN)
  TSFBridge::Shutdown();
#endif
}

}  // namespace ui
