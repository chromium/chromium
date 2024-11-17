// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_hwnd_utils.h"

#include <dwmapi.h>

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {

namespace {

void CalculateWindowStylesFromInitParams(
    const Widget::InitParams& params,
    WidgetDelegate* widget_delegate,
    internal::NativeWidgetDelegate* native_widget_delegate,
    bool is_translucent,
    DWORD* style,
    DWORD* ex_style,
    DWORD* class_style) {
  *style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  *ex_style = 0;
  *class_style = CS_DBLCLKS;

  // Set type-independent style attributes.
  if (params.child)
    *style |= WS_CHILD;
  if (params.show_state == ui::mojom::WindowShowState::kMaximized) {
    *style |= WS_MAXIMIZE;
  }
  if (params.show_state == ui::mojom::WindowShowState::kMinimized) {
    *style |= WS_MINIMIZE;
  }
  if (!params.accept_events)
    *ex_style |= WS_EX_TRANSPARENT;
  DCHECK_NE(Widget::InitParams::Activatable::kDefault, params.activatable);
  if (params.activatable == Widget::InitParams::Activatable::kNo)
    *ex_style |= WS_EX_NOACTIVATE;
  if (params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal)
    *ex_style |= WS_EX_TOPMOST;
  if (params.shadow_type == Widget::InitParams::ShadowType::kDrop)
    *class_style |= CS_DROPSHADOW;

  // Set type-dependent style attributes.
  switch (params.type) {
    case Widget::InitParams::TYPE_WINDOW: {
      // WS_OVERLAPPEDWINDOW is equivalent to:
      //   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
      //   WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
      *style |= WS_OVERLAPPEDWINDOW;
      if (!widget_delegate->CanMaximize())
        *style &= static_cast<DWORD>(~WS_MAXIMIZEBOX);
      if (!widget_delegate->CanMinimize())
        *style &= static_cast<DWORD>(~WS_MINIMIZEBOX);
      if (!widget_delegate->CanResize())
        *style &= static_cast<DWORD>(~(WS_THICKFRAME | WS_MAXIMIZEBOX));
      if (params.remove_standard_frame)
        *style &= static_cast<DWORD>(~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX));

      if (native_widget_delegate->IsDialogBox()) {
        *style |= DS_MODALFRAME;
        // NOTE: Turning this off means we lose the close button, which is bad.
        // Turning it on though means the user can maximize or size the window
        // from the system menu, which is worse. We may need to provide our own
        // menu to get the close button to appear properly.
        // style &= ~WS_SYSMENU;

        // Set the WS_POPUP style for modal dialogs. This ensures that the owner
        // window is activated on destruction. This style should not be set for
        // non-modal non-top-level dialogs like constrained windows.
        *style |= native_widget_delegate->IsModal() ? WS_POPUP : 0;
      }
      *ex_style |=
          native_widget_delegate->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;

      // See layered window comment below.
      if (is_translucent)
        *style &= static_cast<DWORD>(~(WS_THICKFRAME | WS_CAPTION));
      break;
    }
    case Widget::InitParams::TYPE_CONTROL:
      *style |= WS_VISIBLE;
      break;
    case Widget::InitParams::TYPE_BUBBLE:
      *style |= WS_POPUP;
      *style |= WS_CLIPCHILDREN;
      if (!params.force_show_in_taskbar)
        *ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_POPUP:
      *style |= WS_POPUP;
      if (!params.force_show_in_taskbar)
        *ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_MENU:
      *style |= WS_POPUP;
      if (params.remove_standard_frame) {
        *style |= WS_THICKFRAME;
      }
      if (!params.force_show_in_taskbar)
        *ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_DRAG:
    case Widget::InitParams::TYPE_TOOLTIP:
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      *style |= WS_POPUP;
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

bool DidClientAreaSizeChange(const WINDOWPOS* window_pos) {
  return !(window_pos->flags & SWP_NOSIZE) ||
         window_pos->flags & SWP_FRAMECHANGED;
}

bool DidMinimizedChange(UINT old_size_param, UINT new_size_param) {
  return (
      (old_size_param == SIZE_MINIMIZED && new_size_param != SIZE_MINIMIZED) ||
      (old_size_param != SIZE_MINIMIZED && new_size_param == SIZE_MINIMIZED));
}

void ConfigureWindowStyles(
    HWNDMessageHandler* handler,
    const Widget::InitParams& params,
    WidgetDelegate* widget_delegate,
    internal::NativeWidgetDelegate* native_widget_delegate) {
  // Configure the HWNDMessageHandler with the appropriate
  DWORD style = 0;
  DWORD ex_style = 0;
  DWORD class_style = 0;
  // Layered windows do not work with Direct3D, so a different mechanism needs
  // to be used to allow for transparent borderless windows.
  //
  // 1- To allow the contents of the swapchain to blend with the contents
  //    behind it, it must must be created with D3DFMT_A8R8G8B8 in D3D9Ex, or
  //    with DXGI_ALPHA_MODE_PREMULTIPLIED with DirectComposition.
  // 2- When the window is created but before it is presented, call
  //    DwmExtendFrameIntoClientArea passing -1 as the margins so that
  //    it's blended with the content below the window and not just black.
  // 3- To avoid having a window frame and to avoid blurring the contents
  //    behind the window, the window must have WS_POPUP in its style and must
  //    not have not have WM_SIZEBOX, WS_THICKFRAME or WS_CAPTION in its
  //    style.
  //
  // Software composited windows can continue to use WS_EX_LAYERED.
  bool is_translucent =
      (params.opacity == Widget::InitParams::WindowOpacity::kTranslucent);

  CalculateWindowStylesFromInitParams(params, widget_delegate,
                                      native_widget_delegate, is_translucent,
                                      &style, &ex_style, &class_style);
  handler->set_is_translucent(is_translucent);
  handler->set_initial_class_style(class_style);
  handler->set_window_style(handler->window_style() | style);
  handler->set_window_ex_style(handler->window_ex_style() | ex_style);
}

}  // namespace views
