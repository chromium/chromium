// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/init/input_method_initializer.h"

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/ime_bridge.h"
#elif defined(USE_AURA) && defined(OS_LINUX)
#include "base/logging.h"
#include "ui/base/ime/linux/fake_input_method_context_factory.h"
#elif defined(OS_WIN)
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/win/tsf_bridge.h"
#endif

namespace {

#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
const ui::LinuxInputMethodContextFactory*
    g_linux_input_method_context_factory_for_testing;
#endif

}  // namespace

namespace ui {

void InitializeInputMethod() {
#if defined(OS_CHROMEOS)
  IMEBridge::Initialize();
#elif defined(OS_WIN)
  TSFBridge::Initialize();
#endif
}

void ShutdownInputMethod() {
#if defined(OS_CHROMEOS)
  IMEBridge::Shutdown();
#elif defined(OS_WIN)
  TSFBridge::Shutdown();
#endif
}

void InitializeInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  IMEBridge::Initialize();
#elif defined(USE_AURA) && defined(OS_LINUX)
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
  // Make sure COM is initialized because TSF depends on COM.
  CoInitialize(nullptr);
  TSFBridge::Initialize();
#endif
}

void ShutdownInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  IMEBridge::Shutdown();
#elif defined(USE_AURA) && defined(OS_LINUX)
  const LinuxInputMethodContextFactory* factory =
      LinuxInputMethodContextFactory::instance();
  CHECK(!factory || factory == g_linux_input_method_context_factory_for_testing)
      << "An unknown LinuxInputMethodContextFactory was set.";
  LinuxInputMethodContextFactory::SetInstance(NULL);
  delete g_linux_input_method_context_factory_for_testing;
  g_linux_input_method_context_factory_for_testing = NULL;
#elif defined(OS_WIN)
  TSFBridge::Shutdown();
  CoUninitialize();
#endif
}

}  // namespace ui
