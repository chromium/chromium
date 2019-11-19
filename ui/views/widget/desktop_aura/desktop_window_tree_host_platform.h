// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace views {

class VIEWS_EXPORT DesktopWindowTreeHostPlatform
    : public aura::WindowTreeHostPlatform,
      public DesktopWindowTreeHost {
 public:
  DesktopWindowTreeHostPlatform(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostPlatform() override;

  // Accessor for DesktopNativeWidgetAura::content_window().
  aura::Window* GetContentWindow();
  const aura::Window* GetContentWindow() const;

  // DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  void OnActiveWindowChanged(bool active) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient(
      DesktopNativeCursorManager* cursor_manager) override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool HasCapture() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  bool SetWindowTitle(const base::string16& title) override;
  void ClearNativeFocus() override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  NonClientFrameView* CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowTransparency() const override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;

  // WindowTreeHost:
  gfx::Transform GetRootTransform() const override;
  void ShowImpl() override;
  void HideImpl() override;

  // PlatformWindowDelegateBase:
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnCloseRequest() override;
  void OnActivationChanged(bool active) override;
  base::Optional<gfx::Size> GetMinimumSizeForWindow() override;
  base::Optional<gfx::Size> GetMaximumSizeForWindow() override;

 protected:
  // TODO(https://crbug.com/990756): move these methods back to private
  // once DWTHX11 stops using them.
  internal::NativeWidgetDelegate* native_widget_delegate() {
    return native_widget_delegate_;
  }
  DesktopNativeWidgetAura* desktop_native_widget_aura() {
    return desktop_native_widget_aura_;
  }

  // These are not general purpose methods and must be used with care. Please
  // make sure you understand the rounding direction before using.
  gfx::Rect ToDIPRect(const gfx::Rect& rect_in_pixels) const;
  gfx::Rect ToPixelRect(const gfx::Rect& rect_in_dip) const;

 private:
  void Relayout();

  Widget* GetWidget();
  const Widget* GetWidget() const;

  // Set visibility and fire OnNativeWidgetVisibilityChanged() if it changed.
  void SetVisible(bool visible);

  // There are platform specific properties that Linux may want to add.
  virtual void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties);

  internal::NativeWidgetDelegate* const native_widget_delegate_;
  DesktopNativeWidgetAura* const desktop_native_widget_aura_;

  bool is_active_ = false;

  base::string16 window_title_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  DesktopWindowTreeHostPlatform* window_parent_ = nullptr;
  std::set<DesktopWindowTreeHostPlatform*> window_children_;

  // Keep track of PlatformWindow state so that we would react correctly and set
  // visibility only if the window was minimized or was unminimized from the
  // normal state.
  ui::PlatformWindowState old_state_ = ui::PlatformWindowState::kUnknown;

  base::WeakPtrFactory<DesktopWindowTreeHostPlatform> close_widget_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostPlatform);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
