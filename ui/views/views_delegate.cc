// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_delegate.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/native_widget_private.h"

#if defined(USE_AURA)
#include "ui/views/touchui/touch_selection_menu_runner_views.h"
#endif

namespace views {

namespace {

ViewsDelegate* views_delegate = nullptr;

}  // namespace

ViewsDelegate::ViewsDelegate() {
  DCHECK(!views_delegate);
  views_delegate = this;

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_CHROMEOS_ASH)
  // TouchSelectionMenuRunnerViews is not supported on Mac or Cast.
  // It is also not used on Ash (the ChromeViewsDelegate() for Ash will
  // immediately replace this). But tests running without the Chrome layer
  // will not get the replacement.
  touch_selection_menu_runner_ =
      std::make_unique<TouchSelectionMenuRunnerViews>();
#endif
}

ViewsDelegate::~ViewsDelegate() {
  DCHECK_EQ(this, views_delegate);
  views_delegate = nullptr;
}

ViewsDelegate* ViewsDelegate::GetInstance() {
  return views_delegate;
}

void ViewsDelegate::SaveWindowPlacement(const Widget* widget,
                                        const std::string& window_name,
                                        const gfx::Rect& bounds,
                                        ui::mojom::WindowShowState show_state) {
}

bool ViewsDelegate::GetSavedWindowPlacement(
    const Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  return false;
}

void ViewsDelegate::NotifyMenuItemFocused(const std::u16string& menu_name,
                                          const std::u16string& menu_item_name,
                                          int item_index,
                                          int item_count,
                                          bool has_submenu) {}

ViewsDelegate::ProcessMenuAcceleratorResult
ViewsDelegate::ProcessAcceleratorWhileMenuShowing(
    const ui::Accelerator& accelerator) {
  return ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;
}

bool ViewsDelegate::ShouldCloseMenuIfMouseCaptureLost() const {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ViewsDelegate::ShouldWindowHaveRoundedCorners(
    const gfx::NativeWindow window) const {
  return false;
}
#endif

#if BUILDFLAG(IS_WIN)
HICON ViewsDelegate::GetDefaultWindowIcon() const {
  return nullptr;
}

HICON ViewsDelegate::GetSmallWindowIcon() const {
  return nullptr;
}

bool ViewsDelegate::IsWindowInMetro(gfx::NativeWindow window) const {
  return false;
}
#elif BUILDFLAG(ENABLE_DESKTOP_AURA) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
gfx::ImageSkia* ViewsDelegate::GetDefaultWindowIcon() const {
  return nullptr;
}
#endif

std::unique_ptr<NonClientFrameView>
ViewsDelegate::CreateDefaultNonClientFrameView(Widget* widget) {
  return nullptr;
}

bool ViewsDelegate::IsShuttingDown() const {
  return false;
}

void ViewsDelegate::AddRef() {}

void ViewsDelegate::ReleaseRef() {}

void ViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {}

bool ViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  return false;
}

#if BUILDFLAG(IS_MAC)
ui::ContextFactory* ViewsDelegate::GetContextFactory() {
  return nullptr;
}
#endif

std::string ViewsDelegate::GetApplicationName() {
  base::FilePath program = base::CommandLine::ForCurrentProcess()->GetProgram();
  return program.BaseName().AsUTF8Unsafe();
}

#if BUILDFLAG(IS_WIN)
int ViewsDelegate::GetAppbarAutohideEdges(HMONITOR monitor,
                                          base::OnceClosure callback) {
  return EDGE_BOTTOM;
}
#endif

#if defined(USE_AURA)
void ViewsDelegate::SetTouchSelectionMenuRunner(
    std::unique_ptr<TouchSelectionMenuRunnerViews> menu_runner) {
  touch_selection_menu_runner_ = std::move(menu_runner);
}
#endif

}  // namespace views
