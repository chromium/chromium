// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_shape_updater.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/window_move_client.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostPlatform*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostPlatform*,
                             kHostForRootWindow,
                             nullptr)

namespace {

bool DetermineInactivity(ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL &&
      show_state != ui::SHOW_STATE_INACTIVE &&
      show_state != ui::SHOW_STATE_MAXIMIZED) {
    // It will behave like SHOW_STATE_NORMAL.
    NOTIMPLEMENTED_LOG_ONCE();
  }

  // See comment in PlatformWindow::Show().
  return show_state == ui::SHOW_STATE_INACTIVE;
}

ui::PlatformWindowOpacity GetPlatformWindowOpacity(
    Widget::InitParams::WindowOpacity opacity) {
  switch (opacity) {
    case Widget::InitParams::WindowOpacity::kInferred:
      return ui::PlatformWindowOpacity::kInferOpacity;
    case Widget::InitParams::WindowOpacity::kOpaque:
      return ui::PlatformWindowOpacity::kOpaqueWindow;
    case Widget::InitParams::WindowOpacity::kTranslucent:
      return ui::PlatformWindowOpacity::kTranslucentWindow;
  }
  return ui::PlatformWindowOpacity::kOpaqueWindow;
}

ui::PlatformWindowType GetPlatformWindowType(
    Widget::InitParams::Type window_type) {
  switch (window_type) {
    case Widget::InitParams::TYPE_WINDOW:
      return ui::PlatformWindowType::kWindow;
    case Widget::InitParams::TYPE_MENU:
      return ui::PlatformWindowType::kMenu;
    case Widget::InitParams::TYPE_TOOLTIP:
      return ui::PlatformWindowType::kTooltip;
    case Widget::InitParams::TYPE_DRAG:
      return ui::PlatformWindowType::kDrag;
    case Widget::InitParams::TYPE_BUBBLE:
      return ui::PlatformWindowType::kBubble;
    default:
      return ui::PlatformWindowType::kPopup;
  }
  NOTREACHED();
  return ui::PlatformWindowType::kPopup;
}

ui::PlatformWindowShadowType GetPlatformWindowShadowType(
    Widget::InitParams::ShadowType shadow_type) {
  switch (shadow_type) {
    case Widget::InitParams::ShadowType::kDefault:
      return ui::PlatformWindowShadowType::kDefault;
    case Widget::InitParams::ShadowType::kNone:
      return ui::PlatformWindowShadowType::kNone;
    case Widget::InitParams::ShadowType::kDrop:
      return ui::PlatformWindowShadowType::kDrop;
  }
  NOTREACHED();
  return ui::PlatformWindowShadowType::kNone;
}

ui::PlatformWindowInitProperties ConvertWidgetInitParamsToInitProperties(
    const Widget::InitParams& params) {
  ui::PlatformWindowInitProperties properties;
  properties.type = GetPlatformWindowType(params.type);
  properties.activatable =
      params.activatable == Widget::InitParams::ACTIVATABLE_YES;
  properties.force_show_in_taskbar = params.force_show_in_taskbar;
  properties.keep_on_top =
      params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal;
  properties.visible_on_all_workspaces = params.visible_on_all_workspaces;
  properties.remove_standard_frame = params.remove_standard_frame;
  properties.workspace = params.workspace;
  properties.opacity = GetPlatformWindowOpacity(params.opacity);
  properties.shadow_type = GetPlatformWindowShadowType(params.shadow_type);

  if (params.parent && params.parent->GetHost())
    properties.parent_widget = params.parent->GetHost()->GetAcceleratedWidget();

  return properties;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostPlatform:

DesktopWindowTreeHostPlatform::DesktopWindowTreeHostPlatform(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      window_move_client_(this) {}

DesktopWindowTreeHostPlatform::~DesktopWindowTreeHostPlatform() {
  window()->ClearProperty(kHostForRootWindow);
  DCHECK(!platform_window()) << "The host must be closed before destroying it.";
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
    gfx::AcceleratedWidget widget) {
  auto* host = DesktopWindowTreeHostPlatform::GetHostForWidget(widget);
  return host ? host->GetContentWindow() : nullptr;
}

// static
DesktopWindowTreeHostPlatform* DesktopWindowTreeHostPlatform::GetHostForWidget(
    gfx::AcceleratedWidget widget) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(widget);
  return host ? host->window()->GetProperty(kHostForRootWindow) : nullptr;
}

aura::Window* DesktopWindowTreeHostPlatform::GetContentWindow() {
  return desktop_native_widget_aura_->content_window();
}

void DesktopWindowTreeHostPlatform::Init(const Widget::InitParams& params) {
  if (params.type == Widget::InitParams::TYPE_WINDOW)
    GetContentWindow()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  // If we have a parent, record the parent/child relationship. We use this
  // data during destruction to make sure that when we try to close a parent
  // window, we also destroy all child windows.
  if (params.parent && params.parent->GetHost()) {
    window_parent_ =
        static_cast<DesktopWindowTreeHostPlatform*>(params.parent->GetHost());
    DCHECK(window_parent_);
    window_parent_->window_children_.insert(this);
  }

  ui::PlatformWindowInitProperties properties =
      ConvertWidgetInitParamsToInitProperties(params);
  AddAdditionalInitProperties(params, &properties);

  // Calculate initial bounds.
  properties.bounds = ToPixelRect(params.bounds);

  // Set extensions delegate.
  DCHECK(!properties.workspace_extension_delegate);
  properties.workspace_extension_delegate = this;

  CreateAndSetPlatformWindow(std::move(properties));

  // Disable compositing on tooltips as a workaround for
  // https://crbug.com/442111.
  CreateCompositor(viz::FrameSinkId(),
                   params.force_software_compositing ||
                       params.type == Widget::InitParams::TYPE_TOOLTIP);

  WindowTreeHost::OnAcceleratedWidgetAvailable();
  InitHost();
  window()->Show();
}

void DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kHostForRootWindow, this);
  // This reroutes RunMoveLoop requests to the DesktopWindowTreeHostPlatform.
  // The availability of this feature depends on a platform (PlatformWindow)
  // that implements RunMoveLoop.
  wm::SetWindowMoveClient(window(), &window_move_client_);
  platform_window()->SetUseNativeFrame(params.type ==
                                           Widget::InitParams::TYPE_WINDOW &&
                                       !params.remove_standard_frame);

  native_widget_delegate_->OnNativeWidgetCreated();
}

void DesktopWindowTreeHostPlatform::OnWidgetInitDone() {
  // WindowShape is updated from ShapeRects transformed from
  // NonClientView::GetWindowMask. We can guarantee that |NonClientView|
  // is created OnWidgetInitDone.
  if (ShouldUseLayerForShapedWindow()) {
    WindowShapeUpdater::CreateWindowShapeUpdater(
        this, this->desktop_native_widget_aura());
  }
}

void DesktopWindowTreeHostPlatform::OnActiveWindowChanged(bool active) {}

std::unique_ptr<corewm::Tooltip>
DesktopWindowTreeHostPlatform::CreateTooltip() {
  return std::make_unique<corewm::TooltipAura>();
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostPlatform::CreateDragDropClient() {
  ui::WmDragHandler* drag_handler = ui::GetWmDragHandler(*(platform_window()));
  std::unique_ptr<DesktopDragDropClientOzone> drag_drop_client =
      std::make_unique<DesktopDragDropClientOzone>(window(), drag_handler);
  // Set a class property key, which allows |drag_drop_client| to be used for
  // drop action.
  SetWmDropHandler(platform_window(), drag_drop_client.get());
  return std::move(drag_drop_client);
}

void DesktopWindowTreeHostPlatform::Close() {
  // If we are in process of closing or the PlatformWindow has already been
  // closed, do nothing.
  if (close_widget_factory_.HasWeakPtrs() || !platform_window())
    return;

  GetContentWindow()->Hide();

  // Hide while waiting for the close.
  // Please note that it's better to call WindowTreeHost::Hide, which also calls
  // PlatformWindow::Hide and Compositor::SetVisible(false).
  Hide();

  // And we delay the close so that if we are called from an ATL callback,
  // we don't destroy the window before the callback returned (as the caller
  // may delete ourselves on destroy and the ATL callback would still
  // dereference us when the callback returns).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DesktopWindowTreeHostPlatform::CloseNow,
                                close_widget_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostPlatform::CloseNow() {
  if (!platform_window())
    return;

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    SetWmDropHandler(platform_window(), nullptr);
#endif

  platform_window()->PrepareForShutdown();

  ReleaseCapture();
  native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves.
  std::set<DesktopWindowTreeHostPlatform*> window_children_copy =
      window_children_;
  for (auto* child : window_children_copy)
    child->CloseNow();
  DCHECK(window_children_.empty());

  // If we have a parent, remove ourselves from its children list.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
    window_parent_ = nullptr;
  }

  // Destroy the compositor before destroying the |platform_window()| since
  // shutdown may try to swap, and the swap without a window may cause an error
  // in X Server or Wayland, which causes a crash with in-process renderer, for
  // example.
  DestroyCompositor();

  platform_window()->Close();
}

aura::WindowTreeHost* DesktopWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostPlatform::Show(ui::WindowShowState show_state,
                                         const gfx::Rect& restore_bounds) {
  if (compositor())
    SetVisible(true);

  platform_window()->Show(DetermineInactivity(show_state));

  switch (show_state) {
    case ui::SHOW_STATE_MAXIMIZED:
      platform_window()->Maximize();
      if (!restore_bounds.IsEmpty()) {
        // Enforce |restored_bounds_in_pixels_| since calling Maximize() could
        // have reset it.
        platform_window()->SetRestoredBoundsInPixels(
            ToPixelRect(restore_bounds));
      }
      break;
    case ui::SHOW_STATE_MINIMIZED:
      platform_window()->Minimize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      SetFullscreen(true);
      break;
    default:
      break;
  }

  if (native_widget_delegate_->CanActivate()) {
    if (show_state != ui::SHOW_STATE_INACTIVE)
      Activate();

    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See https://crbug.com/515594 for
    // example.
    native_widget_delegate_->SetInitialFocus(
        IsActive() ? show_state : ui::SHOW_STATE_INACTIVE);
  }

  GetContentWindow()->Show();
}

bool DesktopWindowTreeHostPlatform::IsVisible() const {
  return platform_window()->IsVisible();
}

void DesktopWindowTreeHostPlatform::SetSize(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  auto bounds_in_pixels = GetBoundsInPixels();
  bounds_in_pixels.set_size(size_in_pixels);
  WindowTreeHostPlatform::SetBoundsInPixels(bounds_in_pixels);
}

void DesktopWindowTreeHostPlatform::StackAbove(aura::Window* window) {
  if (!window || !window->GetRootWindow())
    return;

  platform_window()->StackAbove(window->GetHost()->GetAcceleratedWidget());
}

void DesktopWindowTreeHostPlatform::StackAtTop() {
  platform_window()->StackAtTop();
}

void DesktopWindowTreeHostPlatform::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  gfx::Rect parent_bounds_in_pixels = ToPixelRect(GetWorkAreaBoundsInScreen());

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(GetContentWindow())) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(GetContentWindow())->GetBoundsInScreen();
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds_in_pixels = ToPixelRect(transient_parent_rect);
    }
  }

  gfx::Rect window_bounds_in_pixels(
      parent_bounds_in_pixels.x() +
          (parent_bounds_in_pixels.width() - size_in_pixels.width()) / 2,
      parent_bounds_in_pixels.y() +
          (parent_bounds_in_pixels.height() - size_in_pixels.height()) / 2,
      size_in_pixels.width(), size_in_pixels.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds_in_pixels.AdjustToFit(parent_bounds_in_pixels);

  SetBoundsInPixels(window_bounds_in_pixels);
}

void DesktopWindowTreeHostPlatform::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();

  if (IsFullscreen())
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  else if (IsMinimized())
    *show_state = ui::SHOW_STATE_MINIMIZED;
  else if (IsMaximized())
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  else if (!IsActive())
    *show_state = ui::SHOW_STATE_INACTIVE;
  else
    *show_state = ui::SHOW_STATE_NORMAL;
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWindowBoundsInScreen() const {
  return ToDIPRect(GetBoundsInPixels());
}

gfx::Rect DesktopWindowTreeHostPlatform::GetClientAreaBoundsInScreen() const {
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetRestoredBounds() const {
  // We can't reliably track the restored bounds of a window, but we can get
  // the 90% case down. When *chrome* is the process that requests maximizing
  // or restoring bounds, we can record the current bounds before we request
  // maximization, and clear it when we detect a state change.
  gfx::Rect restored_bounds = platform_window()->GetRestoredBoundsInPixels();

  // When window is resized, |restored bounds| is not set and empty.
  // If |restored bounds| is empty, it returns the current window size.
  if (!restored_bounds.IsEmpty())
    return ToDIPRect(restored_bounds);

  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostPlatform::GetWorkspace() const {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  return workspace_extension ? workspace_extension->GetWorkspace()
                             : std::string();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWorkAreaBoundsInScreen() const {
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .work_area();
}

void DesktopWindowTreeHostPlatform::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  // TODO(crbug.com/1158733) : When supporting PlatformWindow::SetShape,
  // Calls ui::Layer::SetAlphaShape and sets |is_shape_explicitly_set_| to true.
  platform_window()->SetShape(std::move(native_shape), GetRootTransform());
}

void DesktopWindowTreeHostPlatform::Activate() {
  platform_window()->Activate();
}

void DesktopWindowTreeHostPlatform::Deactivate() {
  ReleaseCapture();
  platform_window()->Deactivate();
}

bool DesktopWindowTreeHostPlatform::IsActive() const {
  return is_active_;
}

void DesktopWindowTreeHostPlatform::Maximize() {
  platform_window()->Maximize();
  if (IsMinimized())
    Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostPlatform::Minimize() {
  ReleaseCapture();
  platform_window()->Minimize();
}

void DesktopWindowTreeHostPlatform::Restore() {
  platform_window()->Restore();
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

bool DesktopWindowTreeHostPlatform::IsMaximized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMaximized;
}

bool DesktopWindowTreeHostPlatform::IsMinimized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMinimized;
}

bool DesktopWindowTreeHostPlatform::HasCapture() const {
  return platform_window()->HasCapture();
}

void DesktopWindowTreeHostPlatform::SetZOrderLevel(ui::ZOrderLevel order) {
  platform_window()->SetZOrderLevel(order);
}

ui::ZOrderLevel DesktopWindowTreeHostPlatform::GetZOrderLevel() const {
  return platform_window()->GetZOrderLevel();
}

void DesktopWindowTreeHostPlatform::SetVisibleOnAllWorkspaces(
    bool always_visible) {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  if (workspace_extension)
    workspace_extension->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopWindowTreeHostPlatform::IsVisibleOnAllWorkspaces() const {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  return workspace_extension ? workspace_extension->IsVisibleOnAllWorkspaces()
                             : false;
}

bool DesktopWindowTreeHostPlatform::SetWindowTitle(
    const base::string16& title) {
  if (window_title_ == title)
    return false;

  window_title_ = title;
  platform_window()->SetTitle(window_title_);
  return true;
}

void DesktopWindowTreeHostPlatform::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our content_window, which will trigger a cascade
  // of focus changes into views.
  if (GetContentWindow() && aura::client::GetFocusClient(GetContentWindow()) &&
      GetContentWindow()->Contains(
          aura::client::GetFocusClient(GetContentWindow())
              ->GetFocusedWindow())) {
    aura::client::GetFocusClient(GetContentWindow())
        ->FocusWindow(GetContentWindow());
  }
}

Widget::MoveLoopResult DesktopWindowTreeHostPlatform::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  auto* move_loop_handler = ui::GetWmMoveLoopHandler(*platform_window());
  if (move_loop_handler && move_loop_handler->RunMoveLoop(drag_offset))
    return Widget::MOVE_LOOP_SUCCESSFUL;
  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostPlatform::EndMoveLoop() {
  auto* move_loop_handler = ui::GetWmMoveLoopHandler(*platform_window());
  if (move_loop_handler)
    move_loop_handler->EndMoveLoop();
}

void DesktopWindowTreeHostPlatform::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  platform_window()->SetVisibilityChangedAnimationsEnabled(value);
}

std::unique_ptr<NonClientFrameView>
DesktopWindowTreeHostPlatform::CreateNonClientFrameView() {
  return ShouldUseNativeFrame() ? std::make_unique<NativeFrameView>(GetWidget())
                                : nullptr;
}

bool DesktopWindowTreeHostPlatform::ShouldUseNativeFrame() const {
  return platform_window()->ShouldUseNativeFrame();
}

bool DesktopWindowTreeHostPlatform::ShouldWindowContentsBeTransparent() const {
  return platform_window()->ShouldWindowContentsBeTransparent() ||
         ShouldUseLayerForShapedWindow();
}

void DesktopWindowTreeHostPlatform::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FrameType::kDefault) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }
  platform_window()->SetUseNativeFrame(new_type ==
                                       Widget::FrameType::kForceNative);

  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  if (GetWidget()->non_client_view())
    GetWidget()->non_client_view()->UpdateFrame();
}

void DesktopWindowTreeHostPlatform::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;

  platform_window()->ToggleFullscreen();

  // The state must change synchronously to let media react on fullscreen
  // changes.
  DCHECK_EQ(fullscreen, IsFullscreen());

  if (IsFullscreen() == fullscreen)
    ScheduleRelayout();
  // Else: the widget will be relaid out either when the window bounds change
  // or when |platform_window|'s fullscreen state changes.
}

bool DesktopWindowTreeHostPlatform::IsFullscreen() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kFullScreen;
}

void DesktopWindowTreeHostPlatform::SetOpacity(float opacity) {
  GetContentWindow()->layer()->SetOpacity(opacity);
  platform_window()->SetOpacity(opacity);
}

void DesktopWindowTreeHostPlatform::SetAspectRatio(
    const gfx::SizeF& aspect_ratio) {
  platform_window()->SetAspectRatio(aspect_ratio);
}

void DesktopWindowTreeHostPlatform::SetWindowIcons(
    const gfx::ImageSkia& window_icon,
    const gfx::ImageSkia& app_icon) {
  platform_window()->SetWindowIcons(window_icon, app_icon);
}

void DesktopWindowTreeHostPlatform::InitModalType(ui::ModalType modal_type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::FlashFrame(bool flash_frame) {
  platform_window()->FlashFrame(flash_frame);
}

bool DesktopWindowTreeHostPlatform::IsAnimatingClosed() const {
  return platform_window()->IsAnimatingClosed();
}

bool DesktopWindowTreeHostPlatform::IsTranslucentWindowOpacitySupported()
    const {
  return platform_window()->IsTranslucentWindowOpacitySupported();
}

void DesktopWindowTreeHostPlatform::SizeConstraintsChanged() {
  platform_window()->SizeConstraintsChanged();
}

bool DesktopWindowTreeHostPlatform::ShouldUpdateWindowTransparency() const {
  return true;
}

bool DesktopWindowTreeHostPlatform::ShouldUseDesktopNativeCursorManager()
    const {
  return true;
}

bool DesktopWindowTreeHostPlatform::ShouldCreateVisibilityController() const {
  return true;
}

gfx::Transform DesktopWindowTreeHostPlatform::GetRootTransform() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  // This might be called before the |platform_window| is created. Thus,
  // explicitly check if that exists before trying to access its visibility and
  // the display where it is shown.
  if (platform_window()) {
    display = display::Screen::GetScreen()->GetDisplayNearestWindow(
        GetWidget()->GetNativeWindow());
  }

  float scale = display.device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

void DesktopWindowTreeHostPlatform::ShowImpl() {
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostPlatform::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);
}

void DesktopWindowTreeHostPlatform::OnClosed() {
  wm::SetWindowMoveClient(window(), nullptr);
  SetWmDropHandler(platform_window(), nullptr);
  desktop_native_widget_aura_->OnHostWillClose();
  SetPlatformWindow(nullptr);
  desktop_native_widget_aura_->OnHostClosed();
}

void DesktopWindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
  bool was_minimized = old_state_ == ui::PlatformWindowState::kMinimized;
  bool is_minimized = new_state == ui::PlatformWindowState::kMinimized;

  // Propagate minimization/restore to compositor to avoid drawing 'blank'
  // frames that could be treated as previews, which show content even if a
  // window is minimized.
  if (is_minimized != was_minimized) {
    if (is_minimized) {
      SetVisible(false);
      GetContentWindow()->Hide();
    } else {
      GetContentWindow()->Show();
      SetVisible(true);
    }
  }

  old_state_ = new_state;

  // Now that we have different window properties, we may need to relayout the
  // window. (The windows code doesn't need this because their window change is
  // synchronous.)
  ScheduleRelayout();
}

void DesktopWindowTreeHostPlatform::OnCloseRequest() {
  GetWidget()->Close();
}

void DesktopWindowTreeHostPlatform::OnWillDestroyAcceleratedWidget() {
  desktop_native_widget_aura_->OnHostWillClose();
}

void DesktopWindowTreeHostPlatform::OnActivationChanged(bool active) {
  if (is_active_ == active)
    return;
  is_active_ = active;
  aura::WindowTreeHostPlatform::OnActivationChanged(active);
  desktop_native_widget_aura_->HandleActivationChanged(active);
  ScheduleRelayout();
}

base::Optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMinimumSizeForWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate()->GetMinimumSize()))
      .size();
}

base::Optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMaximumSizeForWindow() {
  return ToPixelRect(gfx::Rect(native_widget_delegate()->GetMaximumSize()))
      .size();
}

SkPath DesktopWindowTreeHostPlatform::GetWindowMaskForWindowShapeInPixels() {
  if (!GetWidget()->non_client_view())
    return SkPath();

  SkPath window_mask;
  // Some frame views define a custom (non-rectanguar) window mask.
  // If so, use it to define the window shape. If not, fall through.
  GetWidget()->non_client_view()->GetWindowMask(
      GetWindowBoundsInScreen().size(), &window_mask);
  // Convert SkPath in DIPs to pixels.
  if (!window_mask.isEmpty())
    window_mask.transform(SkMatrix(GetRootTransform().matrix()));
  return window_mask;
}

void DesktopWindowTreeHostPlatform::OnWorkspaceChanged() {
  OnHostWorkspaceChanged();
}

gfx::Rect DesktopWindowTreeHostPlatform::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  gfx::RectF rect_in_dip = gfx::RectF(rect_in_pixels);
  GetRootTransform().TransformRectReverse(&rect_in_dip);
  return gfx::ToEnclosingRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostPlatform::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels = gfx::RectF(rect_in_dip);
  GetRootTransform().TransformRect(&rect_in_pixels);
  return gfx::ToEnclosingRect(rect_in_pixels);
}

void DesktopWindowTreeHostPlatform::ScheduleRelayout() {
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    if (non_client_view->frame_view())
      non_client_view->frame_view()->InvalidateLayout();
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
  }
}

Widget* DesktopWindowTreeHostPlatform::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopWindowTreeHostPlatform::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

void DesktopWindowTreeHostPlatform::SetVisible(bool visible) {
  if (compositor())
    compositor()->SetVisible(visible);

  native_widget_delegate()->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopWindowTreeHostPlatform::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {}

bool DesktopWindowTreeHostPlatform::ShouldUseLayerForShapedWindow() const {
  return platform_window()->ShouldUseLayerForShapedWindow();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost:

// Linux subclasses this host and adds some Linux specific bits.
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostPlatform(native_widget_delegate,
                                           desktop_native_widget_aura);
}
#endif

}  // namespace views
