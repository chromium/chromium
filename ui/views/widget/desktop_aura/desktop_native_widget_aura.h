// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
class WindowEventDispatcher;
class WindowTreeHost;
namespace client {
class DragDropClient;
class ScreenPositionClient;
class WindowParentingClient;
}  // namespace client
}  // namespace aura

namespace wm {
class CompoundEventFilter;
class CursorManager;
class FocusController;
class ShadowController;
class VisibilityController;
class WindowModalityController;
}  // namespace wm

namespace views {
namespace corewm {
class TooltipController;
}
class DesktopCaptureClient;
class DesktopEventClient;
class DesktopNativeCursorManager;
class DesktopWindowTreeHost;
class FocusManagerEventHandler;
class TooltipManagerAura;
class WindowReorderer;

// DesktopNativeWidgetAura handles top-level widgets on Windows, Linux, and
// Chrome OS with mash.
class VIEWS_EXPORT DesktopNativeWidgetAura
    : public internal::NativeWidgetPrivate,
      public aura::WindowDelegate,
      public wm::ActivationDelegate,
      public wm::ActivationChangeObserver,
      public aura::client::FocusChangeObserver,
      public aura::client::DragDropDelegate,
      public aura::WindowTreeHostObserver {
 public:
  explicit DesktopNativeWidgetAura(internal::NativeWidgetDelegate* delegate);

  DesktopNativeWidgetAura(const DesktopNativeWidgetAura&) = delete;
  DesktopNativeWidgetAura& operator=(const DesktopNativeWidgetAura&) = delete;

  ~DesktopNativeWidgetAura() override;

  // Maps from window to DesktopNativeWidgetAura. |window| must be a root
  // window.
  static DesktopNativeWidgetAura* ForWindow(aura::Window* window);

  // Called by our DesktopWindowTreeHost after it has deleted native resources;
  // this is the signal that we should start our shutdown.
  virtual void OnHostClosed();

  // TODO(beng): remove this method and replace with an implementation of
  //             WindowDestroying() that takes the window being destroyed.
  // Called from ~DesktopWindowTreeHost. This takes the WindowEventDispatcher
  // as by the time we get here |dispatcher_| is NULL.
  virtual void OnDesktopWindowTreeHostDestroyed(aura::WindowTreeHost* host);

  wm::CompoundEventFilter* root_window_event_filter() {
    return root_window_event_filter_.get();
  }

  aura::WindowTreeHost* host() { return host_.get(); }

  DesktopWindowTreeHost* desktop_window_tree_host_for_testing() {
    return desktop_window_tree_host_.get();
  }

  aura::Window* content_window() { return content_window_; }

  views::corewm::TooltipController* tooltip_controller();

  Widget::InitParams::Type widget_type() const { return widget_type_; }

  // Ensures that the correct window is activated/deactivated based on whether
  // we are being activated/deactivated.
  void HandleActivationChanged(bool active);

  // Called before the window tree host will close.
  void OnHostWillClose();

  // Overridden from internal::NativeWidgetPrivate:
  gfx::NativeWindow GetNativeWindow() const override;

  // Configures the appropriate aura::Windows based on the
  // DesktopWindowTreeHost's transparency.
  void UpdateWindowTransparency();

  base::WeakPtr<internal::NativeWidgetPrivate> GetWeakPtr() override;

 protected:
  // internal::NativeWidgetPrivate:
  void InitNativeWidget(Widget::InitParams params) override;
  void OnWidgetInitDone() override;
  void ReparentNativeViewImpl(gfx::NativeView new_parent) override;
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  gfx::NativeView GetNativeView() const override;
  Widget* GetTopLevelWidget() override;
  const ui::Compositor* GetCompositor() const override;
  const ui::Layer* GetLayer() const override;
  void ReorderNativeViews() override;
  void ViewRemoved(View* view) override;
  void SetNativeWindowProperty(const char* name, void* value) override;
  void* GetNativeWindowProperty(const char* name) const override;
  TooltipManager* GetTooltipManager() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  ui::InputMethod* GetInputMethod() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::mojom::WindowShowState* maximized) const override;
  bool SetWindowTitle(const std::u16string& title) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  const gfx::ImageSkia* GetWindowIcon() override;
  const gfx::ImageSkia* GetWindowAppIcon() override;
  void InitModalType(ui::mojom::ModalType modal_type) override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetBoundsConstrained(const gfx::Rect& bounds) override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(gfx::NativeView native_view) override;
  void StackAtTop() override;
  bool IsStackedAbove(gfx::NativeView native_view) override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> shape) override;
  void Close() override;
  void CloseNow() override;
  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  void Hide() override;
  bool IsVisible() const override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void PaintAsActiveChanged() override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void Maximize() override;
  void Minimize() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Restore() override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  bool IsFullscreen() const override;
  void SetCanAppearInExistingFullscreenSpaces(
      bool can_appear_in_existing_fullscreen_spaces) override;
  void SetOpacity(float opacity) override;
  // See NativeWidgetPrivate::SetAspectRatio for more information.
  void SetAspectRatio(const gfx::SizeF& aspect_ratio,
                      const gfx::Size& excluded_margin) override;
  void FlashFrame(bool flash_frame) override;
  void RunShellDrag(std::unique_ptr<ui::OSExchangeData> data,
                    const gfx::Point& location,
                    int operation,
                    ui::mojom::DragEventSource source) override;
  void CancelShellDrag(View* view) override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;
  void ScheduleLayout() override;
  void SetCursor(const ui::Cursor& cursor) override;
  bool IsMouseEventsEnabled() const override;
  bool IsMouseButtonDown() const override;
  void ClearNativeFocus() override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  bool IsMoveLoopSupported() const override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  void SetVisibilityAnimationDuration(const base::TimeDelta& duration) override;
  void SetVisibilityAnimationTransition(
      Widget::VisibilityTransition transition) override;
  ui::GestureRecognizer* GetGestureRecognizer() override;
  ui::GestureConsumer* GetGestureConsumer() override;
  void OnSizeConstraintsChanged() override;
  void OnNativeViewHierarchyWillChange() override;
  void OnNativeViewHierarchyChanged() override;
  bool SetAllowScreenshots(bool allow) override;
  bool AreScreenshotsAllowed() override;
  std::string GetName() const override;

  // aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;
  void UpdateVisualState() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  std::string_view GetLogContext() const override;

  // wm::ActivationDelegate:
  bool ShouldActivate() const override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // aura::WindowTreeHostObserver:
  void OnHostCloseRequested(aura::WindowTreeHost* host) override;
  void OnHostResized(aura::WindowTreeHost* host) override;
  void OnHostWorkspaceChanged(aura::WindowTreeHost* host) override;
  void OnHostMovedInPixels(aura::WindowTreeHost* host) override;

 private:
  friend class RootWindowDestructionObserver;

  void RootWindowDestroyed();

  // Notify the root view of our widget of a native accessibility event.
  void NotifyAccessibilityEvent(ax::mojom::Event event_type);

  void PerformDrop(views::DropHelper::DropCallback drop_cb,
                   std::unique_ptr<ui::OSExchangeData> data,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  std::unique_ptr<aura::WindowTreeHost> host_;
  raw_ptr<DesktopWindowTreeHost, DanglingUntriaged> desktop_window_tree_host_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_ =
      Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;

  // Internal name.
  std::string name_;

  std::unique_ptr<DesktopCaptureClient> capture_client_;

  // This is the return value from GetNativeView().
  // WARNING: this may be NULL, in particular during shutdown it becomes NULL.
  raw_ptr<aura::Window, AcrossTasksDanglingUntriaged> content_window_;

  base::WeakPtr<internal::NativeWidgetDelegate> native_widget_delegate_;

  // This is a unique ptr to enforce scenarios where NativeWidget
  // will own the NativeWidgetDelegate.
  // Used for ownership model: NativeWidgetOwnsWidget.
  std::unique_ptr<internal::NativeWidgetDelegate> owned_native_widget_delegate;

  std::unique_ptr<wm::FocusController> focus_client_;
  std::unique_ptr<aura::client::ScreenPositionClient> position_client_;
  std::unique_ptr<aura::client::DragDropClient> drag_drop_client_;
  std::unique_ptr<aura::client::WindowParentingClient> window_parenting_client_;
  std::unique_ptr<DesktopEventClient> event_client_;
  std::unique_ptr<FocusManagerEventHandler> focus_manager_event_handler_;

  // Toplevel event filter which dispatches to other event filters.
  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;

  std::unique_ptr<DropHelper> drop_helper_;
  int last_drop_operation_ = ui::DragDropTypes::DRAG_NONE;

  std::unique_ptr<corewm::TooltipController> tooltip_controller_;
  std::unique_ptr<TooltipManagerAura> tooltip_manager_;

  std::unique_ptr<wm::VisibilityController> visibility_controller_;

  std::unique_ptr<wm::WindowModalityController> window_modality_controller_;

  bool restore_focus_on_activate_ = false;

  // This flag is used to ensure that the activation client correctly sees
  // whether this widget should receive activation when handling an activation
  // change event in `HandleActivationChanged()`.This is needed as the widget
  // may not have propagated its new activation state to its delegate before the
  // activation client decides which window to activate next.
  std::optional<bool> should_activate_;

  gfx::NativeCursor cursor_;
  // We must manually reference count the number of users of |cursor_manager_|
  // because the cursors created by |cursor_manager_| are shared among the
  // DNWAs. We can't just stuff this in a LazyInstance because we need to
  // destroy this as the last DNWA happens; we can't put it off until
  // (potentially) after we tear down the X11 connection because that's a
  // crash.
  static int cursor_reference_count_;
  static wm::CursorManager* cursor_manager_;
  static DesktopNativeCursorManager* native_cursor_manager_;

  std::unique_ptr<wm::ShadowController> shadow_controller_;

  // Reorders child windows of |window_| associated with a view based on the
  // order of the associated views in the widget's view hierarchy.
  std::unique_ptr<WindowReorderer> window_reorderer_;

  // See class documentation for Widget in widget.h for a note about type.
  Widget::InitParams::Type widget_type_ = Widget::InitParams::TYPE_WINDOW;

  // See DesktopWindowTreeHost::ShouldUseDesktopNativeCursorManager().
  bool use_desktop_native_cursor_manager_ = false;

#if BUILDFLAG(IS_WIN)
  // Used to track and discard duplicate events; Windows appears to
  // generate them in some circumstances after a key press.
  gfx::Point last_mouse_loc_;
#endif

  // The following factory is used to provide references to the
  // DesktopNativeWidgetAura instance and used for calls to close to run drop
  // callback.
  base::WeakPtrFactory<DesktopNativeWidgetAura> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_
