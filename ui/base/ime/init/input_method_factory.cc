// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/init/input_method_factory.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/ime/win/input_method_win_tsf.h"
#elif BUILDFLAG(IS_APPLE)
#include "ui/base/ime/mac/input_method_mac.h"
#elif BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#else
#include "ui/base/ime/input_method_minimal.h"
#endif

namespace {

ui::InputMethod* g_input_method_for_testing = nullptr;

bool g_input_method_set_for_testing = false;

bool g_create_input_method_called = false;

}  // namespace

namespace ui {

std::unique_ptr<InputMethod> CreateInputMethod(
    ImeKeyEventDispatcher* ime_key_event_dispatcher,
    gfx::AcceleratedWidget widget) {
  if (!g_create_input_method_called)
    g_create_input_method_called = true;

  if (g_input_method_for_testing) {
    ui::InputMethod* ret = g_input_method_for_testing;
    g_input_method_for_testing = nullptr;
    return base::WrapUnique(ret);
  }

  if (g_input_method_set_for_testing)
    return std::make_unique<MockInputMethod>(ime_key_event_dispatcher);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
    return base::WrapUnique(new MockInputMethod(ime_key_event_dispatcher));

#if BUILDFLAG(IS_WIN)
  return std::make_unique<InputMethodWinTSF>(ime_key_event_dispatcher, widget);
#elif BUILDFLAG(IS_APPLE)
  return std::make_unique<InputMethodMac>(ime_key_event_dispatcher);
#elif BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()->CreateInputMethod(
      ime_key_event_dispatcher, widget);
#else
  return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
#endif
}

void SetUpInputMethodForTesting(InputMethod* input_method) {
  g_input_method_for_testing = input_method;
}

ScopedTestInputMethodFactory::ScopedTestInputMethodFactory() {
  CHECK(!g_input_method_set_for_testing)
      << "ScopedTestInputMethodFactory was created after calling "
         "ui::SetUpInputMethodFactoryForTesting or inside another "
         "ScopedTestInputMethodFactory lifetime.";

  DLOG_IF(WARNING, g_create_input_method_called)
      << "ui::CreateInputMethod was already called. That can happen when other "
         "tests in the same process uses normal ui::InputMethod instance.";

  g_input_method_set_for_testing = true;
  g_create_input_method_called = false;
}

ScopedTestInputMethodFactory::~ScopedTestInputMethodFactory() {
  g_input_method_set_for_testing = false;
  g_create_input_method_called = false;
}

}  // namespace ui
