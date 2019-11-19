// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_

#include "base/macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/native_widget_private.h"

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
class HitTestNativeWidgetMac;
class MockNativeWidgetMac;
class WidgetTest;
}
class NativeWidgetMacNSWindowHost;

class VIEWS_EXPORT NativeWidgetMac : public internal::NativeWidgetPrivate {
 public:
  explicit NativeWidgetMac(internal::NativeWidgetDelegate* delegate);
  ~NativeWidgetMac() override;

  // Informs |delegate_| that the native widget is about to be destroyed.
  // NativeWidgetNSWindowBridge::OnWindowWillClose() invokes this early when the
  // NSWindowDelegate informs the bridge that the window is being closed (later,
  // invoking OnWindowDestroyed()).
  void WindowDestroying();

  // Deletes |bridge_| and informs |delegate_| that the native widget is
  // destroyed.
  void WindowDestroyed();

  // The vertical position from which sheets should be anchored, from the top
  // of the content view.
  virtual int32_t SheetOffsetY();

  // Returns in |override_titlebar_height| whether or not to override the
  // titlebar height and in |titlebar_height| the height of the titlebar.
  virtual void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                            float* titlebar_height);

  // Notifies that the widget starts to enter or exit fullscreen mode.
  virtual void OnWindowFullscreenStateChange() {}

  // Handle "Move focus to the window toolbar" shortcut.
  virtual void OnFocusWindowToolbar() {}

  // Allows subclasses to override the behavior for
  // -[NSUserInterfaceValidations validateUserInterfaceItem].
  virtual void ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) {}

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
  NonClientFrameView* CreateNonClientFrameView() override;
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
                          ui::WindowShowState* show_state) const override;
  bool SetWindowTitle(const base::string16& title) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetBoundsConstrained(const gfx::Rect& bounds) override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(gfx::NativeView native_view) override;
  void StackAtTop() override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> shape) override;
  void Close() override;
  void CloseNow() override;
  void Show(ui::WindowShowState show_state,
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
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetCanAppearInExistingFullscreenSpaces(
      bool can_appear_in_existing_fullscreen_spaces) override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void FlashFrame(bool flash_frame) override;
  void RunShellDrag(View* view,
                    std::unique_ptr<ui::OSExchangeData> data,
                    const gfx::Point& location,
                    int operation,
                    ui::DragDropTypes::DragEventSource source) override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;
  void ScheduleLayout() override;
  void SetCursor(gfx::NativeCursor cursor) override;
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
  bool IsTranslucentWindowOpacitySupported() const override;
  ui::GestureRecognizer* GetGestureRecognizer() override;
  void OnSizeConstraintsChanged() override;
  std::string GetName() const override;

  // Calls |callback| with the newly created NativeWidget whenever a
  // NativeWidget is created.
  static void SetInitNativeWidgetCallback(
      base::RepeatingCallback<void(NativeWidgetMac*)> callback);

 protected:
  virtual void PopulateCreateWindowParams(
      const Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) {}

  // Creates the NSWindow that will be passed to the NativeWidgetNSWindowBridge.
  // Called by InitNativeWidget. The return value will be autoreleased.
  // Note that some tests (in particular, views_unittests that interact
  // with ScopedFakeNSWindowFullscreen, on 10.10) assume that these windows
  // are autoreleased, and will crash if the window has a more precise
  // lifetime.
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

  internal::NativeWidgetDelegate* delegate() { return delegate_; }

  // Return the mojo interface for the NSWindow. The interface may be
  // implemented in-process or out-of-process.
  remote_cocoa::mojom::NativeWidgetNSWindow* GetNSWindowMojo() const;

  // Return the bridge structure only if this widget is in-process.
  remote_cocoa::NativeWidgetNSWindowBridge* GetInProcessNSWindowBridge() const;

  NativeWidgetMacNSWindowHost* GetNSWindowHost() const {
    return ns_window_host_.get();
  }

 private:
  friend class test::MockNativeWidgetMac;
  friend class test::HitTestNativeWidgetMac;
  friend class views::test::WidgetTest;

  class ZoomFocusMonitor;
  std::unique_ptr<ZoomFocusMonitor> zoom_focus_monitor_;

  internal::NativeWidgetDelegate* delegate_;
  std::unique_ptr<NativeWidgetMacNSWindowHost> ns_window_host_;

  Widget::InitParams::Ownership ownership_;

  // Internal name.
  std::string name_;

  ui::ZOrderLevel z_order_level_ = ui::ZOrderLevel::kNormal;

  Widget::InitParams::Type type_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMac);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
