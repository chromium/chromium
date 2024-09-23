// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/events/event_constants.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"

#if BUILDFLAG(IS_MAC)
#error "This file must not be included on macOS; Chromium Mac doesn't use Aura."
#endif

namespace aura {
class Window;
}

namespace views {

class DropHelper;
class FocusManagerEventHandler;
class TooltipManagerAura;
class WindowReorderer;

class VIEWS_EXPORT NativeWidgetAura : public internal::NativeWidgetPrivate,
                                      public aura::WindowDelegate,
                                      public aura::WindowObserver,
                                      public wm::ActivationDelegate,
                                      public wm::ActivationChangeObserver,
                                      public wm::TransientWindowObserver,
                                      public aura::client::FocusChangeObserver,
                                      public aura::client::DragDropDelegate {
 public:
  explicit NativeWidgetAura(internal::NativeWidgetDelegate* delegate);

  NativeWidgetAura(const NativeWidgetAura&) = delete;
  NativeWidgetAura& operator=(const NativeWidgetAura&) = delete;

  // Called internally by NativeWidgetAura and DesktopNativeWidgetAura to
  // associate |native_widget| with |window|.
  static void RegisterNativeWidgetForWindow(
      internal::NativeWidgetPrivate* native_widget,
      aura::Window* window);

  // Assign an icon to aura window.
  static void AssignIconToAuraWindow(aura::Window* window,
                                     const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon);

  // If necessary, sets the ShadowElevation of |window| from |params|.
  static void SetShadowElevationFromInitParams(
      aura::Window* window,
      const Widget::InitParams& params);

  // Sets the window property aura::client::kResizeBehaviorKey based on the
  // values from the delegate.
  static void SetResizeBehaviorFromDelegate(WidgetDelegate* delegate,
                                            aura::Window* window);

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
  gfx::NativeWindow GetNativeWindow() const override;
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
  bool IsStackedAbove(gfx::NativeView widget) override;
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
  base::WeakPtr<internal::NativeWidgetPrivate> GetWeakPtr() override;

  // aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
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

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnResizeLoopStarted(aura::Window* window) override;
  void OnResizeLoopEnded(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // wm::ActivationDelegate:
  bool ShouldActivate() const override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // aura::TransientWindowObserver:
  void OnTransientParentChanged(aura::Window* new_parent) override;

 protected:
  ~NativeWidgetAura() override;

  internal::NativeWidgetDelegate* delegate() { return delegate_.get(); }

 private:
  void SetInitialFocus(ui::mojom::WindowShowState show_state);

  // Set the bounds with target 'display_id'. This will place the widget in that
  // 'display' even if the more than half of bounds are outside of the display.
  // If the 'display_id' is nullopt or the display does not exist, it will use
  // the display that matches 'bounds'.
  void SetBoundsInternal(const gfx::Rect& bounds,
                         std::optional<int64_t> display_id);

  base::WeakPtr<internal::NativeWidgetDelegate> delegate_;
  std::unique_ptr<internal::NativeWidgetDelegate> owned_delegate_;

  // WARNING: set to NULL when destroyed. As the Widget is not necessarily
  // destroyed along with |window_| all usage of |window_| should first verify
  // non-NULL.
  raw_ptr<aura::Window> window_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_ =
      Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;

  ui::Cursor cursor_;

  std::unique_ptr<TooltipManagerAura> tooltip_manager_;

  // Reorders child windows of |window_| associated with a view based on the
  // order of the associated views in the widget's view hierarchy.
  std::unique_ptr<WindowReorderer> window_reorderer_;

  std::unique_ptr<DropHelper> drop_helper_;
  int last_drop_operation_;

  // Native widget's handler to receive events before the event target.
  std::unique_ptr<FocusManagerEventHandler> focus_manager_event_handler_;

  // The following factory is used to provide references to the NativeWidgetAura
  // instance. We need a separate factory from the |close_widget_factory_|
  // because the close widget factory is currently used to ensure that the
  // CloseNow task is only posted once. We check whether there are any weak
  // pointers from close_widget_factory_| before posting
  // the CloseNow task, so we can't have any other weak pointers for this to
  // work properly. CloseNow can destroy the aura::Window
  // which will not destroy the NativeWidget if WIDGET_OWNS_NATIVE_WIDGET, and
  // we need to make sure we do not attempt to destroy the aura::Window twice.
  // TODO(crbug.com/40232479): The two factories can be combined if the
  // WIDGET_OWNS_NATIVE_WIDGET is removed.
  base::WeakPtrFactory<NativeWidgetAura> weak_factory{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_
