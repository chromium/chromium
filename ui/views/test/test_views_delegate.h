// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
#define UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_delegate.h"

namespace views {

class TestViewsDelegate : public ViewsDelegate {
 public:
  TestViewsDelegate();
  ~TestViewsDelegate() override;

  // If set to |true|, forces widgets that do not provide a native widget to use
  // DesktopNativeWidgetAura instead of whatever the default native widget would
  // be. This has no effect on ChromeOS.
  void set_use_desktop_native_widgets(bool desktop) {
    use_desktop_native_widgets_ = desktop;
  }

  void set_use_transparent_windows(bool transparent) {
    use_transparent_windows_ = transparent;
  }

  // Allows tests to provide a ContextFactory via the ViewsDelegate interface.
  void set_context_factory(ui::ContextFactory* context_factory) {
    context_factory_ = context_factory;
  }

  void set_context_factory_private(
      ui::ContextFactoryPrivate* context_factory_private) {
    context_factory_private_ = context_factory_private;
  }

  // For convenience, we create a layout provider by default, but embedders
  // that use their own layout provider subclasses may need to set those classes
  // as the layout providers for their tests.
  void set_layout_provider(std::unique_ptr<LayoutProvider> layout_provider) {
    layout_provider_.swap(layout_provider);
  }

  // ViewsDelegate:
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override;
#endif
  void OnBeforeWidgetInit(Widget::InitParams* params,
                          internal::NativeWidgetDelegate* delegate) override;
  ui::ContextFactory* GetContextFactory() override;
  ui::ContextFactoryPrivate* GetContextFactoryPrivate() override;

 private:
  ui::ContextFactory* context_factory_ = nullptr;
  ui::ContextFactoryPrivate* context_factory_private_ = nullptr;
  bool use_desktop_native_widgets_ = false;
  bool use_transparent_windows_ = false;
  std::unique_ptr<LayoutProvider> layout_provider_ =
      std::make_unique<LayoutProvider>();

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
