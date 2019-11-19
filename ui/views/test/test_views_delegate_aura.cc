// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/views/buildflags.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

namespace views {

TestViewsDelegate::TestViewsDelegate() = default;

TestViewsDelegate::~TestViewsDelegate() = default;

#if defined(OS_WIN)
HICON TestViewsDelegate::GetSmallWindowIcon() const {
  return nullptr;
}
#endif

void TestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  if (params->opacity == Widget::InitParams::INFER_OPACITY) {
    params->opacity = use_transparent_windows_ ?
        Widget::InitParams::TRANSLUCENT_WINDOW :
        Widget::InitParams::OPAQUE_WINDOW;
  }
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  if (!params->native_widget && use_desktop_native_widgets_)
    params->native_widget = new DesktopNativeWidgetAura(delegate);
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)
}

ui::ContextFactory* TestViewsDelegate::GetContextFactory() {
  if (context_factory_)
    return context_factory_;
  if (aura::Env::GetInstance())
    return aura::Env::GetInstance()->context_factory();
  return nullptr;
}

ui::ContextFactoryPrivate* TestViewsDelegate::GetContextFactoryPrivate() {
  if (context_factory_private_)
    return context_factory_private_;
  if (aura::Env::GetInstance())
    return aura::Env::GetInstance()->context_factory_private();
  return nullptr;
}

}  // namespace views
