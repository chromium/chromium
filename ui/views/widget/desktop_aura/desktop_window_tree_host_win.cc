// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "base/win/win_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/win/event_creation_utils.h"
#include "ui/base/win/win_cursor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path_win.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostWin*)

namespace views {

namespace {

// While the mouse is locked we want the invisible mouse to stay within the
// confines of the screen so we keep it in a capture region the size of the
// screen.  However, on windows when the mouse hits the edge of the screen some
// events trigger and cause strange issues to occur. To stop those events from
// occurring we add a small border around the edge of the capture region.
// This constant controls how many pixels wide that border is.
const int kMouseCaptureRegionBorder = 5;

gfx::Size GetExpandedWindowSize(bool is_translucent, gfx::Size size) {
  if (!is_translucent) {
    return size;
  }

  // Some AMD drivers can't display windows that are less than 64x64 pixels,
  // so expand them to be at least that size. http://crbug.com/286609
  gfx::Size expanded(std::max(size.width(), 64), std::max(size.height(), 64));
  return expanded;
}

void InsetBottomRight(gfx::Rect* rect, const gfx::Vector2d& vector) {
  rect->Inset(gfx::Insets::TLBR(0, 0, vector.y(), vector.x()));
}

// Updates the cursor clip region. Used for mouse locking.
void UpdateMouseLockRegion(aura::Window* window, bool locked) {
  if (!locked) {
    ::ClipCursor(nullptr);
    return;
  }

  RECT window_rect =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(window, window->GetBoundsInScreen())
          .ToRECT();
  window_rect.left += kMouseCaptureRegionBorder;
  window_rect.right -= kMouseCaptureRegionBorder;
  window_rect.top += kMouseCaptureRegionBorder;
  window_rect.bottom -= kMouseCaptureRegionBorder;
  ::ClipCursor(&window_rect);
}

}  // namespace

DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*, kContentWindowForRootWindow, NULL)

// Identifies the DesktopWindowTreeHostWin associated with the
// WindowEventDispatcher.
DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostWin*,
                             kDesktopWindowTreeHostKey,
                             NULL)

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, public:

bool DesktopWindowTreeHostWin::is_cursor_visible_ = true;

DesktopWindowTreeHostWin::DesktopWindowTreeHostWin(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : native_widget_delegate_(native_widget_delegate->AsWidget()->GetWeakPtr()),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      drag_drop_client_(nullptr),
      should_animate_window_close_(false),
      pending_close_(false),
      has_non_client_view_(false) {}

DesktopWindowTreeHostWin::~DesktopWindowTreeHostWin() {
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  // Normally HandleDestroying() destroys the compositor (which is called
  // from WM_DESTROY) but it appears in some situations we can get
  // WM_NCDESTROY (which calls this function) without a WM_DESTROY. As a result
  // DestroyCompositor() is called from both places.
  DestroyCompositor();
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostWin::GetContentWindowForHWND(HWND hwnd) {
  // All HWND's we create should have WindowTreeHost instances associated with
  // them. There are exceptions like the content layer creating HWND's which
  // are not associated with WindowTreeHost instances.
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(hwnd);
  return host ? host->window()->GetProperty(kContentWindowForRootWindow) : NULL;
}

void DesktopWindowTreeHostWin::StartTouchDrag(gfx::Point screen_point) {
  // Send a mouse down and mouse move before do drag drop runs its own event
  // loop. This is required for ::DoDragDrop to start the drag.
  ui::SendMouseEvent(screen_point, MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE);
  ui::SendMouseEvent(screen_point, MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE);
  in_touch_drag_ = true;
}

void DesktopWindowTreeHostWin::FinishTouchDrag(gfx::Point screen_point) {
  if (in_touch_drag_) {
    in_touch_drag_ = false;
    ui::SendMouseEvent(screen_point, MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE);
  }
}
////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostWin::Init(const Widget::InitParams& params) {
  wants_mouse_events_when_inactive_ = params.wants_mouse_events_when_inactive;

  wm::SetAnimationHost(content_window(), this);
  if (params.type == Widget::InitParams::TYPE_WINDOW &&
      !params.remove_standard_frame)
    content_window()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  message_handler_ = HWNDMessageHandler::Create(
      this, native_widget_delegate_->AsWidget()->GetName(),
      params.ShouldInitAsHeadless());

  ConfigureWindowStyles(message_handler_.get(), params,
                        GetWidget()->widget_delegate(),
                        native_widget_delegate_.get());

  HWND parent_hwnd = nullptr;
  if (params.parent && params.parent->GetHost())
    parent_hwnd = params.parent->GetHost()->GetAcceleratedWidget();

  remove_standard_frame_ = params.remove_standard_frame;
  has_non_client_view_ = Widget::RequiresNonClientView(params.type);
  z_order_ = params.EffectiveZOrderLevel();

  // We don't have an HWND yet, so scale relative to the nearest screen.
  gfx::Rect pixel_bounds =
      display::win::ScreenWin::DIPToScreenRect(nullptr, params.bounds);
  message_handler_->Init(parent_hwnd, pixel_bounds);
  CreateCompositor(params.force_software_compositing);
  OnAcceleratedWidgetAvailable();
  InitHost();
  window()->Show();

  // Stack immediately above its parent so that it does not cover other
  // root-level windows, with the exception of menus, to allow them to be
  // displayed on top of other windows.
  if (params.parent && params.type != views::Widget::InitParams::TYPE_MENU) {
    StackAbove(params.parent);
  }
}

void DesktopWindowTreeHostWin::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  // The cursor is not necessarily visible when the root window is created.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window());
  if (cursor_client)
    is_cursor_visible_ = cursor_client->IsCursorVisible();

  window()->SetProperty(kContentWindowForRootWindow, content_window());
  window()->SetProperty(kDesktopWindowTreeHostKey, this);

  should_animate_window_close_ =
      content_window()->GetType() != aura::client::WINDOW_TYPE_NORMAL &&
      !wm::WindowAnimationsDisabled(content_window());
}

void DesktopWindowTreeHostWin::OnActiveWindowChanged(bool active) {}

void DesktopWindowTreeHostWin::OnWidgetInitDone() {}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostWin::CreateTooltip() {
  return std::make_unique<corewm::TooltipAura>();
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostWin::CreateDragDropClient() {
  auto res =
      std::make_unique<DesktopDragDropClientWin>(window(), GetHWND(), this);
  drag_drop_client_ = res->GetWeakPtr();
  return std::move(res);
}

void DesktopWindowTreeHostWin::Close() {
  // Calling Hide() can detach the content window's layer, so store it
  // beforehand so we can access it below.
  auto* window_layer = content_window()->layer();

  content_window()->Hide();
  // TODO(beng): Move this entire branch to DNWA so it can be shared with X11.
  if (should_animate_window_close_) {
    pending_close_ = true;
    // Animation may not start for a number of reasons.
    if (!window_layer->GetAnimator()->is_animating())
      message_handler_->Close();
    // else case, OnWindowHidingAnimationCompleted does the actual Close.
  } else {
    message_handler_->Close();
  }
}

void DesktopWindowTreeHostWin::CloseNow() {
  message_handler_->CloseNow();
}

aura::WindowTreeHost* DesktopWindowTreeHostWin::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostWin::Show(ui::mojom::WindowShowState show_state,
                                    const gfx::Rect& restore_bounds) {
  OnAcceleratedWidgetMadeVisible(true);

  gfx::Rect pixel_restore_bounds;
  if (show_state == ui::mojom::WindowShowState::kMaximized) {
    // The window parameter is intentionally passed as nullptr because a
    // non-null window parameter causes errors when restoring windows to saved
    // positions in variable-DPI situations. See https://crbug.com/1252564 for
    // details.
    pixel_restore_bounds =
        display::win::ScreenWin::DIPToScreenRect(nullptr, restore_bounds);
  }
  message_handler_->Show(show_state, pixel_restore_bounds);

  content_window()->Show();
}

bool DesktopWindowTreeHostWin::IsVisible() const {
  return message_handler_->IsVisible();
}

void DesktopWindowTreeHostWin::SetSize(const gfx::Size& size) {
  gfx::Size size_in_pixels =
      display::win::ScreenWin::DIPToScreenSize(GetHWND(), size);
  gfx::Size expanded =
      GetExpandedWindowSize(message_handler_->is_translucent(), size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded.width() - size_in_pixels.width(),
                    expanded.height() - size_in_pixels.height());
  message_handler_->SetSize(expanded);
}

void DesktopWindowTreeHostWin::StackAbove(aura::Window* window) {
  HWND hwnd = HWNDForNativeView(window);
  if (hwnd)
    message_handler_->StackAbove(hwnd);
}

void DesktopWindowTreeHostWin::StackAtTop() {
  message_handler_->StackAtTop();
}

void DesktopWindowTreeHostWin::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels =
      display::win::ScreenWin::DIPToScreenSize(GetHWND(), size);
  gfx::Size expanded_size;
  expanded_size =
      GetExpandedWindowSize(message_handler_->is_translucent(), size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded_size.width() - size_in_pixels.width(),
                    expanded_size.height() - size_in_pixels.height());
  message_handler_->CenterWindow(expanded_size);
}

void DesktopWindowTreeHostWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  message_handler_->GetWindowPlacement(bounds, show_state);
  InsetBottomRight(bounds, window_enlargement_);
  *bounds = display::win::ScreenWin::ScreenToDIPRect(GetHWND(), *bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetWindowBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetWindowBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return display::win::ScreenWin::ScreenToDIPRect(GetHWND(), pixel_bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetClientAreaBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetClientAreaBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return display::win::ScreenWin::ScreenToDIPRect(GetHWND(), pixel_bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetRestoredBounds() const {
  gfx::Rect pixel_bounds = message_handler_->GetRestoredBounds();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return display::win::ScreenWin::ScreenToDIPRect(GetHWND(), pixel_bounds);
}

std::string DesktopWindowTreeHostWin::GetWorkspace() const {
  return std::string();
}

gfx::Rect DesktopWindowTreeHostWin::GetWorkAreaBoundsInScreen() const {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(
      MonitorFromWindow(message_handler_->hwnd(), MONITOR_DEFAULTTONEAREST),
      &monitor_info);
  gfx::Rect pixel_bounds = gfx::Rect(monitor_info.rcWork);
  return display::win::ScreenWin::ScreenToDIPRect(GetHWND(), pixel_bounds);
}

void DesktopWindowTreeHostWin::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  if (!native_shape || native_shape->empty()) {
    message_handler_->SetRegion(nullptr);
    return;
  }

  // TODO(wez): This would be a lot simpler if we were passed an SkPath.
  // See crbug.com/410593.
  SkRegion shape;
  const float scale = display::win::ScreenWin::GetScaleFactorForHWND(GetHWND());
  if (scale > 1.0) {
    std::vector<SkIRect> sk_rects;
    for (const gfx::Rect& rect : *native_shape) {
      const SkIRect sk_rect = gfx::RectToSkIRect(rect);
      SkRect scaled_rect =
          SkRect::MakeLTRB(sk_rect.left() * scale, sk_rect.top() * scale,
                           sk_rect.right() * scale, sk_rect.bottom() * scale);
      SkIRect rounded_scaled_rect;
      scaled_rect.roundOut(&rounded_scaled_rect);
      sk_rects.push_back(rounded_scaled_rect);
    }
    shape.setRects(&sk_rects[0], static_cast<int>(sk_rects.size()));
  } else {
    for (const gfx::Rect& rect : *native_shape)
      shape.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
  }

  message_handler_->SetRegion(gfx::CreateHRGNFromSkRegion(shape));
}

void DesktopWindowTreeHostWin::SetParent(gfx::AcceleratedWidget parent) {
  message_handler_->SetParentOrOwner(parent);
}

void DesktopWindowTreeHostWin::Activate() {
  message_handler_->Activate();
}

void DesktopWindowTreeHostWin::Deactivate() {
  message_handler_->Deactivate();
}

bool DesktopWindowTreeHostWin::IsActive() const {
  return message_handler_->IsActive();
}

void DesktopWindowTreeHostWin::PaintAsActiveChanged() {
  message_handler_->PaintAsActiveChanged();
}

void DesktopWindowTreeHostWin::Maximize() {
  message_handler_->Maximize();
}

void DesktopWindowTreeHostWin::Minimize() {
  message_handler_->Minimize();
}

void DesktopWindowTreeHostWin::Restore() {
  message_handler_->Restore();
}

bool DesktopWindowTreeHostWin::IsMaximized() const {
  return message_handler_->IsMaximized();
}

bool DesktopWindowTreeHostWin::IsMinimized() const {
  return message_handler_->IsMinimized();
}

bool DesktopWindowTreeHostWin::HasCapture() const {
  return message_handler_->HasCapture();
}

void DesktopWindowTreeHostWin::SetZOrderLevel(ui::ZOrderLevel order) {
  z_order_ = order;
  // Emulate the multiple window levels provided by other platforms by
  // collapsing the z-order enum into kNormal = normal, everything else = always
  // on top.
  message_handler_->SetAlwaysOnTop(order != ui::ZOrderLevel::kNormal);
}

ui::ZOrderLevel DesktopWindowTreeHostWin::GetZOrderLevel() const {
  bool window_always_on_top = message_handler_->IsAlwaysOnTop();
  bool level_always_on_top = z_order_ != ui::ZOrderLevel::kNormal;

  if (window_always_on_top == level_always_on_top)
    return z_order_;

  // If something external has forced a window to be always-on-top, map it to
  // kFloatingWindow as a reasonable equivalent.
  return window_always_on_top ? ui::ZOrderLevel::kFloatingWindow
                              : ui::ZOrderLevel::kNormal;
}

bool DesktopWindowTreeHostWin::IsStackedAbove(aura::Window* window) {
  HWND above = GetHWND();
  HWND below = window->GetHost()->GetAcceleratedWidget();

  // Child windows are always above their parent windows.
  // Check to see if HWNDs have a Parent-Child relationship.
  if (IsChild(below, above))
    return true;

  if (IsChild(above, below))
    return false;

  // Check all HWNDs with lower z order than current HWND
  // to see if it matches or is a parent to the "below" HWND.
  bool result = false;
  HWND parent = above;
  while (parent && parent != GetDesktopWindow()) {
    HWND next = parent;
    while (next) {
      // GW_HWNDNEXT retrieves the next HWND below z order level.
      next = GetWindow(next, GW_HWNDNEXT);
      if (next == below || IsChild(next, below)) {
        result = true;
        break;
      }
    }
    parent = GetAncestor(parent, GA_PARENT);
  }

  return result;
}

void DesktopWindowTreeHostWin::SetVisibleOnAllWorkspaces(bool always_visible) {
  // Chrome does not yet support Windows 10 desktops.
}

bool DesktopWindowTreeHostWin::IsVisibleOnAllWorkspaces() const {
  return false;
}

bool DesktopWindowTreeHostWin::SetWindowTitle(const std::u16string& title) {
  return message_handler_->SetTitle(title);
}

void DesktopWindowTreeHostWin::ClearNativeFocus() {
  message_handler_->ClearNativeFocus();
}

Widget::MoveLoopResult DesktopWindowTreeHostWin::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  const bool hide_on_escape =
      escape_behavior == Widget::MoveLoopEscapeBehavior::kHide;
  return message_handler_->RunMoveLoop(drag_offset, hide_on_escape)
             ? Widget::MoveLoopResult::kSuccessful
             : Widget::MoveLoopResult::kCanceled;
}

void DesktopWindowTreeHostWin::EndMoveLoop() {
  message_handler_->EndMoveLoop();
}

void DesktopWindowTreeHostWin::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  message_handler_->SetVisibilityChangedAnimationsEnabled(value);
  if (desktop_native_widget_aura_->widget_type() !=
          Widget::InitParams::TYPE_WINDOW ||
      remove_standard_frame_) {
    content_window()->SetProperty(aura::client::kAnimationsDisabledKey, !value);
  }
}

std::unique_ptr<NonClientFrameView>
DesktopWindowTreeHostWin::CreateNonClientFrameView() {
  return ShouldUseNativeFrame() ? std::make_unique<NativeFrameView>(
                                      native_widget_delegate_->AsWidget())
                                : nullptr;
}

bool DesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  return true;
}

bool DesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent() const {
  // The window contents need to be transparent when the titlebar area is drawn
  // by the DWM rather than Chrome, so that area can show through.  This
  // function does not describe the transparency of the whole window appearance,
  // but merely of the content Chrome draws, so even when the system titlebars
  // appear opaque (Win 8+), the content above them needs to be transparent, or
  // they'll be covered by a black (undrawn) region.
  return ShouldUseNativeFrame() && !IsFullscreen();
}

void DesktopWindowTreeHostWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
}

void DesktopWindowTreeHostWin::SetFullscreen(bool fullscreen,
                                             int64_t target_display_id) {
  auto weak_ptr = GetWeakPtr();
  message_handler_->SetFullscreen(fullscreen, target_display_id);
  if (!weak_ptr)
    return;
  // TODO(sky): workaround for ScopedFullscreenVisibility showing window
  // directly. Instead of this should listen for visibility changes and then
  // update window.
  if (message_handler_->IsVisible() && !content_window()->TargetVisibility()) {
    OnAcceleratedWidgetMadeVisible(true);
    content_window()->Show();
  }
  desktop_native_widget_aura_->UpdateWindowTransparency();
}

bool DesktopWindowTreeHostWin::IsFullscreen() const {
  return message_handler_->IsFullscreen();
}

void DesktopWindowTreeHostWin::SetOpacity(float opacity) {
  content_window()->layer()->SetOpacity(opacity);
}

void DesktopWindowTreeHostWin::SetAspectRatio(
    const gfx::SizeF& aspect_ratio,
    const gfx::Size& excluded_margin) {
  DCHECK(!aspect_ratio.IsEmpty());
  message_handler_->SetAspectRatio(aspect_ratio.width() / aspect_ratio.height(),
                                   excluded_margin);
}

void DesktopWindowTreeHostWin::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  message_handler_->SetWindowIcons(window_icon, app_icon);
}

void DesktopWindowTreeHostWin::InitModalType(ui::mojom::ModalType modal_type) {
  message_handler_->InitModalType(modal_type);
}

void DesktopWindowTreeHostWin::FlashFrame(bool flash_frame) {
  message_handler_->FlashFrame(flash_frame);
}

bool DesktopWindowTreeHostWin::IsAnimatingClosed() const {
  return pending_close_;
}

void DesktopWindowTreeHostWin::SizeConstraintsChanged() {
  message_handler_->SizeConstraintsChanged();
}

bool DesktopWindowTreeHostWin::ShouldUpdateWindowTransparency() const {
  return true;
}

bool DesktopWindowTreeHostWin::ShouldUseDesktopNativeCursorManager() const {
  return true;
}

bool DesktopWindowTreeHostWin::ShouldCreateVisibilityController() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, WindowTreeHost implementation:

ui::EventSource* DesktopWindowTreeHostWin::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget DesktopWindowTreeHostWin::GetAcceleratedWidget() {
  return message_handler_->hwnd();
}

void DesktopWindowTreeHostWin::ShowImpl() {
  Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
}

void DesktopWindowTreeHostWin::HideImpl() {
  if (!pending_close_)
    message_handler_->Hide();
}

// GetBoundsInPixels and SetBoundsInPixels work in pixel coordinates, whereas
// other get/set methods work in DIP.

gfx::Rect DesktopWindowTreeHostWin::GetBoundsInPixels() const {
  gfx::Rect bounds(message_handler_->GetClientAreaBounds());
  // If the window bounds were expanded we need to return the original bounds
  // To achieve this we do the reverse of the expansion, i.e. add the
  // window_expansion_top_left_delta_ to the origin and subtract the
  // window_expansion_bottom_right_delta_ from the width and height.
  gfx::Rect without_expansion(
      bounds.x() + window_expansion_top_left_delta_.x(),
      bounds.y() + window_expansion_top_left_delta_.y(),
      bounds.width() - window_expansion_bottom_right_delta_.x() -
          window_enlargement_.x(),
      bounds.height() - window_expansion_bottom_right_delta_.y() -
          window_enlargement_.y());
  return without_expansion;
}

void DesktopWindowTreeHostWin::SetBoundsInPixels(const gfx::Rect& bounds) {
  // If the window bounds have to be expanded we need to subtract the
  // window_expansion_top_left_delta_ from the origin and add the
  // window_expansion_bottom_right_delta_ to the width and height
  gfx::Size old_content_size = GetBoundsInPixels().size();

  gfx::Rect expanded(
      bounds.x() - window_expansion_top_left_delta_.x(),
      bounds.y() - window_expansion_top_left_delta_.y(),
      bounds.width() + window_expansion_bottom_right_delta_.x(),
      bounds.height() + window_expansion_bottom_right_delta_.y());

  gfx::Rect new_expanded(
      expanded.origin(),
      GetExpandedWindowSize(message_handler_->is_translucent(),
                            expanded.size()));
  window_enlargement_ =
      gfx::Vector2d(new_expanded.width() - expanded.width(),
                    new_expanded.height() - expanded.height());
  // When |new_expanded| causes the window to be moved to a display with a
  // different DSF, HWNDMessageHandler::OnDpiChanged() will be called and the
  // window size will be scaled automatically.
  message_handler_->SetBounds(new_expanded, old_content_size != bounds.size());
}

gfx::Rect
DesktopWindowTreeHostWin::GetBoundsInAcceleratedWidgetPixelCoordinates() {
  if (message_handler_->IsMinimized())
    return gfx::Rect();
  const gfx::Rect client_bounds =
      message_handler_->GetClientAreaBoundsInScreen();
  const gfx::Rect window_bounds = message_handler_->GetWindowBoundsInScreen();
  if (window_bounds == client_bounds)
    return gfx::Rect(window_bounds.size());
  const gfx::Vector2d offset = client_bounds.origin() - window_bounds.origin();
  DCHECK(offset.x() >= 0 && offset.y() >= 0);
  return gfx::Rect(gfx::Point() + offset, client_bounds.size());
}

gfx::Point DesktopWindowTreeHostWin::GetLocationOnScreenInPixels() const {
  return GetBoundsInPixels().origin();
}

void DesktopWindowTreeHostWin::SetCapture() {
  message_handler_->SetCapture();
}

void DesktopWindowTreeHostWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

bool DesktopWindowTreeHostWin::CaptureSystemKeyEventsImpl(
    std::optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::CreateModifierKeyboardHook(
      std::move(dom_codes), GetAcceleratedWidget(),
      base::BindRepeating(&DesktopWindowTreeHostWin::HandleKeyEvent,
                          base::Unretained(this)));

  return keyboard_hook_ != nullptr;
}

void DesktopWindowTreeHostWin::ReleaseSystemKeyEventCapture() {
  keyboard_hook_.reset();
}

bool DesktopWindowTreeHostWin::IsKeyLocked(ui::DomCode dom_code) {
  return keyboard_hook_ && keyboard_hook_->IsKeyLocked(dom_code);
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostWin::GetKeyboardLayoutMap() {
  return ui::GenerateDomKeyboardLayoutMap();
}

void DesktopWindowTreeHostWin::SetCursorNative(gfx::NativeCursor cursor) {
  TRACE_EVENT1("ui,input", "DesktopWindowTreeHostWin::SetCursorNative",
               "cursor", cursor.type());

  message_handler_->SetCursor(
      ui::WinCursor::FromPlatformCursor(cursor.platform()));
}

void DesktopWindowTreeHostWin::OnCursorVisibilityChangedNative(bool show) {
  if (is_cursor_visible_ == show)
    return;
  is_cursor_visible_ = show;
  ::ShowCursor(!!show);
}

void DesktopWindowTreeHostWin::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  POINT cursor_location = location_in_pixels.ToPOINT();
  ::ClientToScreen(GetHWND(), &cursor_location);
  ::SetCursorPos(cursor_location.x, cursor_location.y);
}

std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
DesktopWindowTreeHostWin::RequestUnadjustedMovement() {
  return message_handler_->RegisterUnadjustedMouseEvent();
}

void DesktopWindowTreeHostWin::LockMouse(aura::Window* window) {
  UpdateMouseLockRegion(window, true /*locked*/);
  WindowTreeHost::LockMouse(window);
}

void DesktopWindowTreeHostWin::UnlockMouse(aura::Window* window) {
  UpdateMouseLockRegion(window, false /*locked*/);
  WindowTreeHost::UnlockMouse(window);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, wm::AnimationHost implementation:

void DesktopWindowTreeHostWin::SetHostTransitionOffsets(
    const gfx::Vector2d& top_left_delta,
    const gfx::Vector2d& bottom_right_delta) {
  gfx::Rect bounds_without_expansion = GetBoundsInPixels();
  window_expansion_top_left_delta_ = top_left_delta;
  window_expansion_bottom_right_delta_ = bottom_right_delta;
  SetBoundsInPixels(bounds_without_expansion);
}

void DesktopWindowTreeHostWin::OnWindowHidingAnimationCompleted() {
  if (pending_close_)
    message_handler_->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, HWNDMessageHandlerDelegate implementation:

ui::InputMethod* DesktopWindowTreeHostWin::GetHWNDMessageDelegateInputMethod() {
  return GetInputMethod();
}

bool DesktopWindowTreeHostWin::HasNonClientView() const {
  return has_non_client_view_;
}

FrameMode DesktopWindowTreeHostWin::GetFrameMode() const {
  if (const Widget* widget = GetWidget()) {
    return widget->ShouldUseNativeFrame() ? FrameMode::SYSTEM_DRAWN
                                          : FrameMode::CUSTOM_DRAWN;
  }
  return FrameMode::SYSTEM_DRAWN;
}

bool DesktopWindowTreeHostWin::HasFrame() const {
  return !remove_standard_frame_;
}

void DesktopWindowTreeHostWin::SchedulePaint() {
  if (Widget* widget = GetWidget()) {
    widget->GetRootView()->SchedulePaint();
  }
}

bool DesktopWindowTreeHostWin::ShouldPaintAsActive() const {
  if (const Widget* widget = GetWidget()) {
    return widget->ShouldPaintAsActive();
  }
  return false;
}

bool DesktopWindowTreeHostWin::CanResize() const {
  if (const Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    return widget->widget_delegate()->CanResize();
  }
  return false;
}

bool DesktopWindowTreeHostWin::CanMaximize() const {
  if (const Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    return widget->widget_delegate()->CanMaximize();
  }
  return false;
}

bool DesktopWindowTreeHostWin::CanMinimize() const {
  if (const Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    return widget->widget_delegate()->CanMinimize();
  }
  return false;
}

bool DesktopWindowTreeHostWin::CanActivate() const {
  if (IsModalWindowActive())
    return true;
  return native_widget_delegate_ ? native_widget_delegate_->CanActivate()
                                 : false;
}

bool DesktopWindowTreeHostWin::WantsMouseEventsWhenInactive() const {
  return wants_mouse_events_when_inactive_;
}

bool DesktopWindowTreeHostWin::WidgetSizeIsClientSize() const {
  if (IsMaximized()) {
    return true;
  }
  if (const Widget* widget = GetWidget()) {
    return widget->ShouldUseNativeFrame();
  }
  return false;
}

bool DesktopWindowTreeHostWin::IsModal() const {
  return native_widget_delegate_ ? native_widget_delegate_->IsModal() : false;
}

int DesktopWindowTreeHostWin::GetInitialShowState() const {
  return CanActivate() ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;
}

int DesktopWindowTreeHostWin::GetNonClientComponent(
    const gfx::Point& point) const {
  gfx::Point dip_position =
      display::win::ScreenWin::ClientToDIPPoint(GetHWND(), point);
  return native_widget_delegate_->GetNonClientComponent(dip_position);
}

void DesktopWindowTreeHostWin::GetWindowMask(const gfx::Size& size,
                                             SkPath* path) {
  if (Widget* widget = GetWidget(); widget && widget->non_client_view()) {
    widget->non_client_view()->GetWindowMask(
        display::win::ScreenWin::ScreenToDIPSize(GetHWND(), size), path);
    // Convert path in DIPs to pixels.
    if (!path->isEmpty()) {
      const float scale =
          display::win::ScreenWin::GetScaleFactorForHWND(GetHWND());
      SkScalar sk_scale = SkFloatToScalar(scale);
      SkMatrix matrix;
      matrix.setScale(sk_scale, sk_scale);
      path->transform(matrix);
    }
  } else if (!window_enlargement_.IsZero()) {
    gfx::Rect bounds(WidgetSizeIsClientSize()
                         ? message_handler_->GetClientAreaBoundsInScreen()
                         : message_handler_->GetWindowBoundsInScreen());
    InsetBottomRight(&bounds, window_enlargement_);
    path->addRect(SkRect::MakeXYWH(0, 0, bounds.width(), bounds.height()));
  }
}

bool DesktopWindowTreeHostWin::GetClientAreaInsets(gfx::Insets* insets,
                                                   HMONITOR monitor) const {
  return false;
}

bool DesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  return false;
}

void DesktopWindowTreeHostWin::GetMinMaxSize(gfx::Size* min_size,
                                             gfx::Size* max_size) const {
  if (!native_widget_delegate_) {
    return;
  }

  *min_size = native_widget_delegate_->GetMinimumSize();
  *max_size = native_widget_delegate_->GetMaximumSize();
}

gfx::Size DesktopWindowTreeHostWin::GetRootViewSize() const {
  if (const Widget* widget = GetWidget()) {
    return widget->GetRootView()->size();
  }
  return gfx::Size();
}

gfx::Size DesktopWindowTreeHostWin::DIPToScreenSize(
    const gfx::Size& dip_size) const {
  return display::win::ScreenWin::DIPToScreenSize(GetHWND(), dip_size);
}

void DesktopWindowTreeHostWin::ResetWindowControls() {
  if (Widget* widget = GetWidget(); widget && widget->non_client_view()) {
    widget->non_client_view()->ResetWindowControls();
  }
}

gfx::NativeViewAccessible DesktopWindowTreeHostWin::GetNativeViewAccessible() {
  // This function may be called during shutdown when the |RootView| is nullptr.
  if (Widget* widget = GetWidget()) {
    return widget->GetRootView()->GetNativeViewAccessible();
  }
  return nullptr;
}

void DesktopWindowTreeHostWin::HandleActivationChanged(bool active) {
  // This can be invoked from HWNDMessageHandler::Init(), at which point we're
  // not in a good state and need to ignore it.
  // TODO(beng): Do we need this still now the host owns the dispatcher?
  if (!dispatcher())
    return;

  desktop_native_widget_aura_->HandleActivationChanged(active);
}

bool DesktopWindowTreeHostWin::HandleAppCommand(int command) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  if (Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    return widget->widget_delegate()->ExecuteWindowsCommand(command);
  }
  return false;
}

void DesktopWindowTreeHostWin::HandleCancelMode() {
  dispatcher()->DispatchCancelModeEvent();
}

void DesktopWindowTreeHostWin::HandleCaptureLost() {
  OnHostLostWindowCapture();
}

void DesktopWindowTreeHostWin::HandleClose() {
  if (Widget* widget = GetWidget()) {
    widget->Close();
  }
}

bool DesktopWindowTreeHostWin::HandleCommand(int command) {
  if (Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    return widget->widget_delegate()->ExecuteWindowsCommand(command);
  }
  return false;
}

void DesktopWindowTreeHostWin::HandleAccelerator(
    const ui::Accelerator& accelerator) {
  if (Widget* widget = GetWidget()) {
    widget->GetFocusManager()->ProcessAccelerator(accelerator);
  }
}

void DesktopWindowTreeHostWin::HandleCreate() {
  native_widget_delegate_->OnNativeWidgetCreated();
}

void DesktopWindowTreeHostWin::HandleDestroying() {
  drag_drop_client_->OnNativeWidgetDestroying(GetHWND());
  if (native_widget_delegate_)
    native_widget_delegate_->OnNativeWidgetDestroying();

  // Destroy the compositor before destroying the HWND since shutdown
  // may try to swap to the window.
  DestroyCompositor();
}

void DesktopWindowTreeHostWin::HandleDestroyed() {
  desktop_native_widget_aura_->OnHostClosed();
}

bool DesktopWindowTreeHostWin::HandleInitialFocus(
    ui::mojom::WindowShowState show_state) {
  if (Widget* widget = GetWidget()) {
    return widget->SetInitialFocus(show_state);
  }
  return false;
}

void DesktopWindowTreeHostWin::HandleDisplayChange() {
  if (Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    widget->widget_delegate()->OnDisplayChanged();
  }
}

void DesktopWindowTreeHostWin::HandleBeginWMSizeMove() {
  if (native_widget_delegate_) {
    native_widget_delegate_->OnNativeWidgetBeginUserBoundsChange();
  }
}

void DesktopWindowTreeHostWin::HandleEndWMSizeMove() {
  if (native_widget_delegate_) {
    native_widget_delegate_->OnNativeWidgetEndUserBoundsChange();
  }
}

void DesktopWindowTreeHostWin::HandleMove() {
  // Adding/removing a monitor, or changing the primary monitor can cause a
  // WM_MOVE message before `OnDisplayChanged()`. Without this call, we would
  // DCHECK due to stale `DisplayInfo`s. See https:://crbug.com/1413940.
  display::win::ScreenWin::UpdateDisplayInfosIfNeeded();
  CheckForMonitorChange();
  OnHostMovedInPixels();
}

void DesktopWindowTreeHostWin::HandleWorkAreaChanged() {
  CheckForMonitorChange();
  if (Widget* widget = GetWidget(); widget && widget->widget_delegate()) {
    widget->widget_delegate()->OnWorkAreaChanged();
  }
}

void DesktopWindowTreeHostWin::HandleVisibilityChanged(bool visible) {
  if (native_widget_delegate_) {
    native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
  }
  if (visible) {
    UpdateAllowScreenshots();
  }
}

void DesktopWindowTreeHostWin::HandleWindowMinimizedOrRestored(bool restored) {
  // Ignore minimize/restore events that happen before widget initialization is
  // done. If a window is created minimized, and then activated, restoring
  // focus will fail because the root window is not visible, which is exposed by
  // ExtensionWindowCreateTest.AcceptState.
  if (!native_widget_delegate_->IsNativeWidgetInitialized())
    return;

  if (restored)
    window()->Show();
  else
    window()->Hide();
}

void DesktopWindowTreeHostWin::HandleClientSizeChanged(
    const gfx::Size& new_size) {
  CheckForMonitorChange();
  if (dispatcher())
    OnHostResizedInPixels(new_size);
}

void DesktopWindowTreeHostWin::HandleFrameChanged() {
  // Replace the frame and layout the contents.
  if (Widget* widget = GetWidget(); widget && widget->non_client_view()) {
    widget->non_client_view()->UpdateFrame();
  }
}

void DesktopWindowTreeHostWin::HandleNativeFocus(HWND last_focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
}

void DesktopWindowTreeHostWin::HandleNativeBlur(HWND focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
}

bool DesktopWindowTreeHostWin::HandleMouseEvent(ui::MouseEvent* event) {
  // Ignore native platform events for test purposes
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents())
    return true;

  SendEventToSink(event);
  return event->handled();
}

void DesktopWindowTreeHostWin::HandleKeyEvent(ui::KeyEvent* event) {
  // Bypass normal handling of alt-space, which would otherwise consume the
  // corresponding WM_SYSCHAR.  This allows HandleIMEMessage() to show the
  // system menu in this case.  If we instead showed the system menu here, the
  // WM_SYSCHAR would trigger a beep when processed by the native event handler.
  if ((event->type() == ui::EventType::kKeyPressed) &&
      (event->key_code() == ui::VKEY_SPACE) &&
      (event->flags() & ui::EF_ALT_DOWN) &&
      !(event->flags() & ui::EF_CONTROL_DOWN)) {
    if (Widget* widget = GetWidget(); widget && widget->non_client_view()) {
      return;
    }
  }

  SendEventToSink(event);
}

void DesktopWindowTreeHostWin::HandleTouchEvent(ui::TouchEvent* event) {
  // HWNDMessageHandler asynchronously processes touch events. Because of this
  // it's possible for the aura::WindowEventDispatcher to have been destroyed
  // by the time we attempt to process them.
  Widget* widget = GetWidget();
  if (!widget || !widget->GetNativeView()) {
    return;
  }
  if (in_touch_drag_) {
    POINT event_point;
    event_point.x = event->location().x();
    event_point.y = event->location().y();
    ::ClientToScreen(GetHWND(), &event_point);
    gfx::Point screen_point(event_point);
    // Send equivalent mouse events, because Ole32 drag drop doesn't seem to
    // handle pointer events.
    if (event->type() == ui::EventType::kTouchMoved) {
      ui::SendMouseEvent(screen_point, MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE);
    } else if (event->type() == ui::EventType::kTouchReleased) {
      FinishTouchDrag(screen_point);
    }
  }
  // TODO(crbug.com/40312079) Calling ::SetCursorPos for
  // ui::EventType::kTouchPressed events here would fix web ui tab strip drags
  // when the cursor is not over the Chrome window - The TODO is to figure out
  // if that's reasonable, since it would change the cursor pos on every touch
  // event. Or figure out if there is a less intrusive way of fixing the cursor
  // position. If we can do that, we can remove the call to ::SetCursorPos in
  // DesktopDragDropClientWin::StartDragAndDrop. Note that calling SetCursorPos
  // at the start of StartDragAndDrop breaks touch drag and drop, so it has to
  // be called some time before we get to StartDragAndDrop.

  // Currently we assume the window that has capture gets touch events too.
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(GetCapture());
  if (host) {
    DesktopWindowTreeHostWin* target =
        host->window()->GetProperty(kDesktopWindowTreeHostKey);
    if (target && target->HasCapture() && target != this) {
      POINT target_location(event->location().ToPOINT());
      ClientToScreen(GetHWND(), &target_location);
      ScreenToClient(target->GetHWND(), &target_location);
      ui::TouchEvent target_event(*event, static_cast<View*>(nullptr),
                                  static_cast<View*>(nullptr));
      target_event.set_location(gfx::Point(target_location));
      target_event.set_root_location(target_event.location());
      target->SendEventToSink(&target_event);
      return;
    }
  }
  SendEventToSink(event);
}

bool DesktopWindowTreeHostWin::HandleIMEMessage(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                LRESULT* result) {
  // Show the system menu at an appropriate location on alt-space.
  if ((message == WM_SYSCHAR) && (w_param == VK_SPACE)) {
    if (Widget* widget = GetWidget(); widget && widget->non_client_view()) {
      const auto* frame = GetWidget()->non_client_view()->frame_view();
      ShowSystemMenuAtScreenPixelLocation(
          GetHWND(), frame->GetSystemMenuScreenPixelLocation());
      return true;
    }
  }

  CHROME_MSG msg = {};
  msg.hwnd = GetHWND();
  msg.message = message;
  msg.wParam = w_param;
  msg.lParam = l_param;
  return GetInputMethod()->OnUntranslatedIMEMessage(msg, result);
}

void DesktopWindowTreeHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
  GetInputMethod()->OnInputLocaleChanged();
}

void DesktopWindowTreeHostWin::HandlePaintAccelerated(
    const gfx::Rect& invalid_rect) {
  if (compositor())
    compositor()->ScheduleRedrawRect(invalid_rect);
}

void DesktopWindowTreeHostWin::HandleMenuLoop(bool in_menu_loop) {
  if (in_menu_loop) {
    tooltip_disabler_ = std::make_unique<wm::ScopedTooltipDisabler>(window());
  } else {
    tooltip_disabler_.reset();
  }
}

bool DesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param,
                                            LRESULT* result) {
  return false;
}

void DesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {}

bool DesktopWindowTreeHostWin::HandleScrollEvent(ui::ScrollEvent* event) {
  SendEventToSink(event);
  return event->handled();
}

bool DesktopWindowTreeHostWin::HandleGestureEvent(ui::GestureEvent* event) {
  SendEventToSink(event);
  return event->handled();
}

void DesktopWindowTreeHostWin::HandleWindowSizeChanging() {
  if (compositor())
    compositor()->DisableSwapUntilResize();
}

void DesktopWindowTreeHostWin::HandleWindowSizeUnchanged() {
  // A resize may not have occurred if the window size happened not to have
  // changed (can occur on Windows 10 when snapping a window to the side of
  // the screen). In that case do a resize to the current size to reenable
  // swaps.
  if (compositor())
    compositor()->ReenableSwap();
}

void DesktopWindowTreeHostWin::HandleWindowScaleFactorChanged(
    float window_scale_factor) {
  // TODO(ccameron): This will violate surface invariants, and is insane.
  // Shouldn't the scale factor and window pixel size changes be sent
  // atomically? And how does this interact with updates to display::Display?
  // Should we expect the display::Display to be updated before this? If so,
  // why can't we use the DisplayObserver that the base WindowTreeHost is
  // using?
  if (compositor()) {
    compositor()->SetScaleAndSize(
        window_scale_factor, message_handler_->GetClientAreaBounds().size(),
        window()->GetLocalSurfaceId());
  }
}

void DesktopWindowTreeHostWin::HandleHeadlessWindowBoundsChanged(
    const gfx::Rect& bounds) {
  window()->SetProperty(aura::client::kHeadlessBoundsKey, bounds);
}

DesktopNativeCursorManager*
DesktopWindowTreeHostWin::GetSingletonDesktopNativeCursorManager() {
  return new DesktopNativeCursorManagerWin();
}

void DesktopWindowTreeHostWin::SetBoundsInDIP(const gfx::Rect& bounds) {
  // The window parameter is intentionally passed as nullptr on Windows because
  // a non-null window parameter causes errors when restoring windows to saved
  // positions in variable-DPI situations. See https://crbug.com/1224715 for
  // details.
  aura::Window* root = nullptr;
  const gfx::Rect bounds_in_pixels =
      display::Screen::GetScreen()->DIPToScreenRectInWindow(
          root, AdjustedContentBounds(bounds));
  AsWindowTreeHost()->SetBoundsInPixels(bounds_in_pixels);
}

void DesktopWindowTreeHostWin::SetAllowScreenshots(bool allow) {
  if (allow_screenshots_ == allow) {
    return;
  }

  allow_screenshots_ = allow;

  // If the window is not visible, do not set the window display affinity
  // because `SetWindowDisplayAffinity` will attempt to compose the window,
  // resulting in a blank window. Instead, we will update it in the `Show`
  // function.
  if (!IsVisible()) {
    return;
  }

  UpdateAllowScreenshots();
}

bool DesktopWindowTreeHostWin::AreScreenshotsAllowed() {
  DWORD affinity;
  if (GetWindowDisplayAffinity(GetHWND(), &affinity)) {
    return affinity == WDA_NONE;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, private:

Widget* DesktopWindowTreeHostWin::GetWidget() {
  return native_widget_delegate_ ? native_widget_delegate_->AsWidget()
                                 : nullptr;
}

const Widget* DesktopWindowTreeHostWin::GetWidget() const {
  return native_widget_delegate_ ? native_widget_delegate_->AsWidget()
                                 : nullptr;
}

HWND DesktopWindowTreeHostWin::GetHWND() const {
  return message_handler_->hwnd();
}

bool DesktopWindowTreeHostWin::IsModalWindowActive() const {
  // This function can get called during window creation which occurs before
  // dispatcher() has been created.
  if (!dispatcher())
    return false;

  const auto is_active = [](const aura::Window* child) {
    return child->GetProperty(aura::client::kModalKey) !=
               ui::mojom::ModalType::kNone &&
           child->TargetVisibility();
  };
  return base::ranges::any_of(window()->children(), is_active);
}

void DesktopWindowTreeHostWin::CheckForMonitorChange() {
  HMONITOR monitor_from_window =
      ::MonitorFromWindow(GetHWND(), MONITOR_DEFAULTTOPRIMARY);
  if (monitor_from_window == last_monitor_from_window_)
    return;
  last_monitor_from_window_ = monitor_from_window;
  OnHostDisplayChanged();
}

gfx::Rect DesktopWindowTreeHostWin::AdjustedContentBounds(
    const gfx::Rect& bounds) {
  gfx::Size minimum_size;
  gfx::Size maximum_size;
  GetMinMaxSize(&minimum_size, &maximum_size);

  gfx::Size bounds_size = bounds.size();

  if (!maximum_size.IsEmpty()) {
    bounds_size.SetToMin(maximum_size);
  }

  if (!minimum_size.IsEmpty()) {
    bounds_size.SetToMax(minimum_size);
  }

  gfx::Rect adjusted_bounds = bounds;
  adjusted_bounds.set_size(bounds_size);
  return adjusted_bounds;
}

aura::Window* DesktopWindowTreeHostWin::content_window() {
  return desktop_native_widget_aura_->content_window();
}

void DesktopWindowTreeHostWin::UpdateAllowScreenshots() {
  if (AreScreenshotsAllowed() == allow_screenshots_) {
    return;
  }

  // When screenshots are not allowed, set the affinity to WDA_MONITOR.
  // This is used instead of WDA_EXCLUDEFROMCAPTURE because the latter renders
  // the window with "no content", which appears as a black rectangle on the
  // screen, whereas the former completely removes the window from the screen.
  // The former is better indication to the user that the contents of the window
  // are being explicitly not shown.
  SetWindowDisplayAffinity(GetHWND(),
                           allow_screenshots_ ? WDA_NONE : WDA_MONITOR);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostWin(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
