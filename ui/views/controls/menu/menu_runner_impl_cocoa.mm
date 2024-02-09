// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#include <dispatch/dispatch.h>

#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/numerics/safe_conversions.h"
#import "components/remote_cocoa/app_shim/menu_controller_cocoa_delegate_impl.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/cocoa/menu_utils.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller_cocoa_delegate_params.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::internal {

MenuRunnerImplCocoa::MenuRunnerImplCocoa(
    ui::MenuModel* menu_model,
    base::RepeatingClosure on_menu_closed_callback)
    : menu_model_(menu_model),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)) {}

bool MenuRunnerImplCocoa::IsRunning() const {
  return running_;
}

void MenuRunnerImplCocoa::Release() {
  if (IsRunning()) {
    if (delete_after_run_)
      return;  // We already canceled.

    delete_after_run_ = true;

    // Reset |menu_controller_| to ensure it clears itself as a delegate to
    // prevent NSMenu attempting to access the weak pointer to the ui::MenuModel
    // it holds (which is not owned by |this|). Toolkit-views menus use
    // MenuRunnerImpl::empty_delegate_ to handle this case.
    [menu_controller_ cancel];
    menu_controller_ = nil;
    menu_model_ = nullptr;
  } else {
    delete this;
  }
}

void MenuRunnerImplCocoa::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    int32_t run_types,
    gfx::NativeView native_view_for_gestures,
    std::optional<gfx::RoundedCornersF> corners,
    std::optional<std::string> show_menu_host_duration_histogram) {
  DCHECK(!IsRunning());
  DCHECK(parent);
  CHECK(run_types & MenuRunner::CONTEXT_MENU);

  menu_delegate_ = [[MenuControllerCocoaDelegateImpl alloc]
      initWithParams:MenuControllerParamsForWidget(parent)];
  menu_controller_ = [[MenuControllerCocoa alloc] initWithModel:menu_model_
                                                       delegate:menu_delegate_
                                         useWithPopUpButtonCell:NO];

  closing_event_time_ = base::TimeTicks();
  running_ = true;

  NSWindow* window = parent->GetNativeWindow().GetNativeNSWindow();
  NSView* view = parent->GetNativeView().GetNativeNSView();

  ui::ShowContextMenu(
      menu_controller_.menu,
      ui::EventForPositioningContextMenu(bounds.CenterPoint(), window), view,
      /*allow_nested_tasks=*/false,
      views::ElementTrackerViews::GetContextForWidget(parent));

  closing_event_time_ = ui::EventTimeForNow();
  running_ = false;

  if (delete_after_run_) {
    delete this;
    return;
  }

  // Don't invoke the callback if Release() was called, since that usually means
  // the owning instance is being destroyed.
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void MenuRunnerImplCocoa::Cancel() {
  [menu_controller_ cancel];
}

base::TimeTicks MenuRunnerImplCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplCocoa::~MenuRunnerImplCocoa() = default;

}  // namespace views::internal
