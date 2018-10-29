// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/native_widget_private.h"

#if defined(__OBJC__)
@class NativeWidgetMacNSWindow;
#else
class NativeWidgetMacNSWindow;
#endif

namespace views_bridge_mac {
namespace mojom {
class BridgedNativeWidget;
class CreateWindowParams;
}  // namespace mojom
}  // namespace views_bridge_mac

namespace views {
namespace test {
class HitTestNativeWidgetMac;
class MockNativeWidgetMac;
class WidgetTest;
}

class BridgeFactoryHost;
class BridgedNativeWidgetImpl;
class BridgedNativeWidgetHostImpl;

class VIEWS_EXPORT NativeWidgetMac : public internal::NativeWidgetPrivate {
 public:
  explicit NativeWidgetMac(internal::NativeWidgetDelegate* delegate);
  ~NativeWidgetMac() override;

  // Informs |delegate_| that the native widget is about to be destroyed.
  // BridgedNativeWidgetImpl::OnWindowWillClose() invokes this early when the
  // NSWindowDelegate informs the bridge that the window is being closed (later,
  // invoking OnWindowDestroyed()).
  void WindowDestroying();

  // Deletes |bridge_| and informs |delegate_| that the native widget is
  // destroyed.
  void WindowDestroyed();

  // Returns the vertical position that sheets should be anchored, in pixels
  // from the bottom of the window.
  virtual int SheetPositionY();

  // Returns in |override_titlebar_height| whether or not to override the
  // titlebar height and in |titlebar_height| the height of the titlebar.
  virtual void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                            float* titlebar_height);

  // Notifies that the widget starts to enter or exit fullscreen mode.
  virtual void OnWindowFullscreenStateChange() {}

  // Handle "Move focus to the window toolbar" shortcut.
  virtual void OnFocusWindowToolbar() {}

  // internal::NativeWidgetPrivate:
  void InitNativeWidget(const Widget::InitParams& params) override;
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
  void SetAlwaysOnTop(bool always_on_top) override;
  bool IsAlwaysOnTop() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void Maximize() override;
  void Minimize() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Restore() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void FlashFrame(bool flash_frame) override;
  void RunShellDrag(View* view,
                    const ui::OSExchangeData& data,
                    const gfx::Point& location,
                    int operation,
                    ui::DragDropTypes::DragEventSource source) override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;
  void SetCursor(gfx::NativeCursor cursor) override;
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
  void RepostNativeEvent(gfx::NativeEvent native_event) override;
  std::string GetName() const override;

 protected:
  virtual void PopulateCreateWindowParams(
      const Widget::InitParams& widget_params,
      views_bridge_mac::mojom::CreateWindowParams* params) {}

  // Creates the NSWindow that will be passed to the BridgedNativeWidgetImpl.
  // Called by InitNativeWidget. The return value will be autoreleased.
  // Note that some tests (in particular, views_unittests that interact
  // with ScopedFakeNSWindowFullscreen, on 10.10) assume that these windows
  // are autoreleased, and will crash if the window has a more precise
  // lifetime.
  virtual NativeWidgetMacNSWindow* CreateNSWindow(
      const views_bridge_mac::mojom::CreateWindowParams* params);

  // Return the BridgeFactoryHost that is to be used for creating this window
  // and all of its child windows. This will return nullptr if the native
  // windows are to be created in the current process.
  virtual BridgeFactoryHost* GetBridgeFactoryHost();

  // Optional hook for subclasses invoked by WindowDestroying().
  virtual void OnWindowDestroying(gfx::NativeWindow window) {}

  // Redispatch a keyboard event using the widget's window's CommandDispatcher.
  // Return true if the event is handled.
  bool RedispatchKeyEvent(NSEvent* event);

  internal::NativeWidgetDelegate* delegate() { return delegate_; }
  views_bridge_mac::mojom::BridgedNativeWidget* bridge() const;
  BridgedNativeWidgetImpl* bridge_impl() const;
  BridgedNativeWidgetHostImpl* bridge_host_for_testing() const {
    return bridge_host_.get();
  }

 private:
  friend class test::MockNativeWidgetMac;
  friend class test::HitTestNativeWidgetMac;
  friend class views::test::WidgetTest;

  internal::NativeWidgetDelegate* delegate_;
  std::unique_ptr<BridgedNativeWidgetHostImpl> bridge_host_;

  Widget::InitParams::Ownership ownership_;

  // Internal name.
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMac);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
