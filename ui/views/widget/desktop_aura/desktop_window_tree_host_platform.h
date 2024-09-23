// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/desktop_aura/window_move_client_platform.h"

namespace ui {
class PaintContext;
}  // namespace ui

namespace views {

namespace corewm {
class TooltipController;
}

class VIEWS_EXPORT DesktopWindowTreeHostPlatform
    : public aura::WindowTreeHostPlatform,
      public DesktopWindowTreeHost,
      public ui::WorkspaceExtensionDelegate {
 public:
  DesktopWindowTreeHostPlatform(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  DesktopWindowTreeHostPlatform(const DesktopWindowTreeHostPlatform&) = delete;
  DesktopWindowTreeHostPlatform& operator=(
      const DesktopWindowTreeHostPlatform&) = delete;

  ~DesktopWindowTreeHostPlatform() override;

  // A way of converting a |widget| into the content_window()
  // of the associated DesktopNativeWidgetAura.
  static aura::Window* GetContentWindowForWidget(gfx::AcceleratedWidget widget);

  // A way of converting a |widget| into this object.
  static DesktopWindowTreeHostPlatform* GetHostForWidget(
      gfx::AcceleratedWidget widget);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

  // Accessor for DesktopNativeWidgetAura::content_window().
  aura::Window* GetContentWindow();
  const aura::Window* GetContentWindow() const;

  // DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  void OnActiveWindowChanged(bool active) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient() override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  bool IsStackedAbove(aura::Window* window) override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  void SetParent(gfx::AcceleratedWidget parent) override;
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
  bool SetWindowTitle(const std::u16string& title) override;
  void ClearNativeFocus() override;
  bool IsMoveLoopSupported() const override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen, int64_t display_id) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio,
                      const gfx::Size& excluded_margin) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::mojom::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  bool IsAnimatingClosed() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowTransparency() const override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;
  void UpdateWindowShapeIfNeeded(const ui::PaintContext& context) override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  void SetAllowScreenshots(bool allow) override;
  bool AreScreenshotsAllowed() override;

  // WindowTreeHost:
  gfx::Transform GetRootTransform() const override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect CalculateRootWindowBounds() const override;
  gfx::Rect GetBoundsInDIP() const override;

  // CompositorObserver:
  void OnCompositorVisibilityChanging(ui::Compositor* compositor,
                                      bool visible) override;
  void OnCompositorVisibilityChanged(ui::Compositor* compositor,
                                     bool visible) override;

  // PlatformWindowDelegate:
  gfx::Insets CalculateInsetsInDIP(
      ui::PlatformWindowState window_state) const override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override;
  void OnCloseRequest() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnWillDestroyAcceleratedWidget() override;
  bool OnRotateFocus(ui::PlatformWindowDelegate::RotateDirection direction,
                     bool reset) override;
  void OnActivationChanged(bool active) override;
  std::optional<gfx::Size> GetMinimumSizeForWindow() const override;
  std::optional<gfx::Size> GetMaximumSizeForWindow() const override;
  bool CanMaximize() const override;
  bool CanFullscreen() const override;
  SkPath GetWindowMaskForWindowShapeInPixels() override;
  std::optional<ui::OwnedWindowAnchor> GetOwnedWindowAnchorAndRectInDIP()
      override;
  gfx::Rect ConvertRectToPixels(const gfx::Rect& rect_in_dip) const override;
  gfx::Rect ConvertRectToDIP(const gfx::Rect& rect_in_pixels) const override;
  gfx::PointF ConvertScreenPointToLocalDIP(
      const gfx::Point& screen_in_pixels) const override;
  gfx::Insets ConvertInsetsToPixels(
      const gfx::Insets& insets_dip) const override;

  // ui::WorkspaceExtensionDelegate:
  void OnWorkspaceChanged() override;

  DesktopWindowTreeHostPlatform* window_parent() const {
    return window_parent_;
  }

  // Tells the window manager to lower the |platform_window()| owned by this
  // host down the stack so that it does not obscure any sibling windows.
  // This is supported when running on x11
  virtual void LowerWindow() {}

 protected:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformImplTest,
                           MouseNCEvents);
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformImplHighDPITest,
                           MouseNCEvents);

  // See comment for variable open_windows_.
  static std::list<gfx::AcceleratedWidget>& open_windows();

  static bool has_open_windows();

  // These are not general purpose methods and must be used with care. Please
  // make sure you understand the rounding direction before using.
  gfx::Rect ToDIPRect(const gfx::Rect& rect_in_pixels) const;
  gfx::Rect ToPixelRect(const gfx::Rect& rect_in_dip) const;

  Widget* GetWidget();
  const Widget* GetWidget() const;

  views::corewm::TooltipController* tooltip_controller();

  void ScheduleRelayout();

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformTest,
                           UpdateWindowShapeFromWindowMask);
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformTest,
                           MakesParentChildRelationship);
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformTest, OnRotateFocus);

  // Set visibility and fire OnNativeWidgetVisibilityChanged() if it changed.
  void SetVisible(bool visible);

  // There are platform specific properties that Linux may want to add.
  virtual void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties);

  // Returns window mask to clip canvas to update window shape of
  // the content window.
  virtual SkPath GetWindowMaskForClipping() const;

  // Helper method that returns the display for the |window()|.
  display::Display GetDisplayNearestRootWindow() const;

  // Impl for rotation logic.
  static bool RotateFocusForWidget(
      Widget& widget,
      ui::PlatformWindowDelegate::RotateDirection direction,
      bool reset);

  const base::WeakPtr<internal::NativeWidgetDelegate> native_widget_delegate_;
  const raw_ptr<DesktopNativeWidgetAura> desktop_native_widget_aura_;

  bool is_active_ = false;

  std::u16string window_title_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  raw_ptr<DesktopWindowTreeHostPlatform> window_parent_ = nullptr;
  std::set<raw_ptr<DesktopWindowTreeHostPlatform, SetExperimental>>
      window_children_;

  // Used for tab dragging in move loop requests.
  WindowMoveClientPlatform window_move_client_;

  // The content window shape can be set from either SetShape or default window
  // mask. When explicitly setting from SetShape, |explicitly_set_shape_:true|
  // to prevent clipping the canvas before painting for default window mask.
  bool is_shape_explicitly_set_ = false;

  base::WeakPtrFactory<DesktopWindowTreeHostPlatform> close_widget_factory_{
      this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_PLATFORM_H_
