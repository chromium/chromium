// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget_observer.h"

#if defined(__OBJC__)
@class NativeWidgetMacNSWindow;
#else
class NativeWidgetMacNSWindow;
#endif

namespace remote_cocoa {
namespace mojom {
class CreateWindowParams;
class NativeWidgetNSWindow;
class ValidateUserInterfaceItemResult;
}  // namespace mojom

class ApplicationHost;
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

namespace views {
namespace test {
class MockNativeWidgetMac;
class NativeWidgetMacTest;
}  // namespace test

class NativeWidgetMacNSWindowHost;
class Widget;

class VIEWS_EXPORT NativeWidgetMac : public internal::NativeWidgetPrivate,
                                     public FocusChangeListener,
                                     public ui::ImeKeyEventDispatcher,
                                     public WidgetObserver {
 public:
  explicit NativeWidgetMac(internal::NativeWidgetDelegate* delegate);
  NativeWidgetMac(const NativeWidgetMac&) = delete;
  NativeWidgetMac& operator=(const NativeWidgetMac&) = delete;
  ~NativeWidgetMac() override;

  // Informs |delegate_| that the native widget is about to be destroyed.
  // NativeWidgetNSWindowBridge::OnWindowWillClose() invokes this early when the
  // NSWindowDelegate informs the bridge that the window is being closed (later,
  // invoking OnWindowDestroyed()).
  void WindowDestroying();

  // Deletes |bridge_| and informs |delegate_| that the native widget is
  // destroyed.
  void WindowDestroyed();

  // Called when the backing NSWindow gains or loses key status.
  void OnWindowKeyStatusChanged(bool is_key, bool is_content_first_responder);

  // The vertical position from which sheets should be anchored, from the top
  // of the content view.
  virtual int32_t SheetOffsetY();

  // Returns in |override_titlebar_height| whether or not to override the
  // titlebar height and in |titlebar_height| the height of the titlebar.
  virtual void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                            float* titlebar_height);

  // Called when the window begins transitioning to or from being fullscreen.
  virtual void OnWindowFullscreenTransitionStart() {}

  // Called when the window has completed its transition to or from being
  // fullscreen. Note that if there are multiple consecutive transitions
  // (because a new transition was initiated before the previous one completed)
  // then this will only be called when all transitions have competed.
  virtual void OnWindowFullscreenTransitionComplete() {}

  // Handle "Move focus to the window toolbar" shortcut.
  virtual void OnFocusWindowToolbar() {}

  // Allows subclasses to override the behavior for
  // -[NSUserInterfaceValidations validateUserInterfaceItem].
  virtual void ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) {}

  // Returns in |will_execute| whether or not ExecuteCommand() will execute
  // the chrome command |command| with |window_open_disposition| and
  // |is_before_first_responder|.
  virtual bool WillExecuteCommand(int32_t command,
                                  WindowOpenDisposition window_open_disposition,
                                  bool is_before_first_responder);

  // Execute the chrome command |command| with |window_open_disposition|. If
  // |is_before_first_responder| then only call ExecuteCommand if the command
  // is reserved and extension shortcut handling is not suspended. Returns in
  // |was_executed| whether or not ExecuteCommand was called (regardless of what
  // the return value for ExecuteCommand was).
  virtual bool ExecuteCommand(int32_t command,
                              WindowOpenDisposition window_open_disposition,
                              bool is_before_first_responder);

  ui::Compositor* GetCompositor() {
    return const_cast<ui::Compositor*>(
        const_cast<const NativeWidgetMac*>(this)->GetCompositor());
  }

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
  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
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
  void ShowEmojiPanel() override;
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

  // Calls |callback| with the newly created NativeWidget whenever a
  // NativeWidget is created.
  static base::CallbackListSubscription RegisterInitNativeWidgetCallback(
      const base::RepeatingCallback<void(NativeWidgetMac*)>& callback);

 protected:
  // The argument to SetBounds is sometimes in screen coordinates and sometimes
  // in parent window coordinates. This function will take that bounds argument
  // and convert it to screen coordinates if needed.
  gfx::Rect ConvertBoundsToScreenIfNeeded(const gfx::Rect& bounds) const;

  virtual void PopulateCreateWindowParams(
      const Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params);

  // Creates the NSWindow that will be passed to the NativeWidgetNSWindowBridge.
  // Called by InitNativeWidget.
  virtual NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params);

  // Return the BridgeFactoryHost that is to be used for creating this window
  // and all of its child windows. This will return nullptr if the native
  // windows are to be created in the current process.
  virtual remote_cocoa::ApplicationHost* GetRemoteCocoaApplicationHost();

  // Called after the window has been initialized. Allows subclasses to perform
  // additional initialization.
  virtual void OnWindowInitialized() {}

  // Optional hook for subclasses invoked by WindowDestroying().
  virtual void OnWindowDestroying(gfx::NativeWindow window) {}

  internal::NativeWidgetDelegate* delegate_for_testing() {
    return delegate_.get();
  }

  // Return the mojo interface for the NSWindow. The interface may be
  // implemented in-process or out-of-process.
  remote_cocoa::mojom::NativeWidgetNSWindow* GetNSWindowMojo() const;

  // Return the bridge structure only if this widget is in-process.
  remote_cocoa::NativeWidgetNSWindowBridge* GetInProcessNSWindowBridge() const;

  NativeWidgetMacNSWindowHost* GetNSWindowHost() const {
    return ns_window_host_.get();
  }

  // Unregister focus listeners from previous focus manager, and register them
  // with the |new_focus_manager|. Updates |focus_manager_|.
  void SetFocusManager(FocusManager* new_focus_manager);

  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* key) override;

  // WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override;

 private:
  friend class test::MockNativeWidgetMac;
  friend class views::test::NativeWidgetMacTest;
  class ZoomFocusMonitor;

  // Applies to all `Widget::InitParams::Ownership` types.
  const base::WeakPtr<internal::NativeWidgetDelegate> delegate_;
  // Only applies to `Widget::InitParams::Ownership::NATIVE_WIDGET_OWNS_WIDGET`.
  std::unique_ptr<internal::NativeWidgetDelegate> owned_delegate_;
  std::unique_ptr<NativeWidgetMacNSWindowHost> ns_window_host_;

  Widget::InitParams::Ownership ownership_ =
      Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;

  // Internal name.
  std::string name_;

  ui::ZOrderLevel z_order_level_ = ui::ZOrderLevel::kNormal;

  Widget::InitParams::Type type_;

  // Weak pointer to the FocusManager with with |zoom_focus_monitor_| and
  // |ns_window_host_| are registered.
  raw_ptr<FocusManager> focus_manager_ = nullptr;
  std::unique_ptr<ui::InputMethod> input_method_;
  std::unique_ptr<ZoomFocusMonitor> zoom_focus_monitor_;
  // Held while this widget is active if it's a child.
  std::unique_ptr<Widget::PaintAsActiveLock> parent_key_lock_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  // The following factory is used to provide references to the NativeWidgetMac
  // instance.
  base::WeakPtrFactory<NativeWidgetMac> weak_factory{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
