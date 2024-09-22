// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_

#include "base/win/windows_types.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

class SkPath;

namespace gfx {
class Insets;
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace ui {
class Accelerator;
class InputMethod;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
class GestureEvent;
}  // namespace ui

namespace views {

enum class FrameMode {
  SYSTEM_DRAWN,              // "glass" frame
  SYSTEM_DRAWN_NO_CONTROLS,  // "glass" frame but with custom window controls
  CUSTOM_DRAWN               // "opaque" frame
};

// Implemented by the object that uses the HWNDMessageHandler to handle
// notifications from the underlying HWND and service requests for data.
class VIEWS_EXPORT HWNDMessageHandlerDelegate {
 public:
  // Returns the input method currently used in this window.
  virtual ui::InputMethod* GetHWNDMessageDelegateInputMethod() = 0;

  // True if the widget associated with this window has a non-client view.
  virtual bool HasNonClientView() const = 0;

  // Returns who we want to be drawing the frame. Either the system (Windows)
  // will handle it or Chrome will custom draw it.
  virtual FrameMode GetFrameMode() const = 0;

  // True if a frame should be drawn. This will return true for some windows
  // that don't have a visible frame. Those usually have the WS_POPUP style, for
  // which Windows will remove the frame automatically if the frame mode is
  // SYSTEM_DRAWN.
  // TODO(bsep): Investigate deleting this when v2 Apps support is removed.
  virtual bool HasFrame() const = 0;

  // True if the window should paint as active (regardless of whether it has
  // system focus).
  virtual bool ShouldPaintAsActive() const = 0;

  virtual void SchedulePaint() = 0;

  virtual bool CanResize() const = 0;
  virtual bool CanMaximize() const = 0;
  virtual bool CanMinimize() const = 0;
  virtual bool CanActivate() const = 0;

  // Returns true if the delegate wants mouse events when inactive and the
  // window is clicked and should not become activated. A return value of false
  // indicates the mouse events will be dropped.
  virtual bool WantsMouseEventsWhenInactive() const = 0;

  virtual bool WidgetSizeIsClientSize() const = 0;

  // Returns true if the delegate represents a modal window.
  virtual bool IsModal() const = 0;

  // Returns the show state that should be used for the application's first
  // window.
  virtual int GetInitialShowState() const = 0;

  virtual int GetNonClientComponent(const gfx::Point& point) const = 0;
  virtual void GetWindowMask(const gfx::Size& size, SkPath* mask) = 0;

  // Returns true if the delegate modifies |insets| to define a custom client
  // area for the window, false if the default client area should be used. If
  // false is returned, |insets| is not modified.  |monitor| is the monitor
  // this window is on.  Normally that would be determined from the HWND, but
  // during WM_NCCALCSIZE Windows does not return the correct monitor for the
  // HWND, so it must be passed in explicitly (see HWNDMessageHandler::
  // OnNCCalcSize for more details).
  virtual bool GetClientAreaInsets(gfx::Insets* insets,
                                   HMONITOR monitor) const = 0;

  // Returns true if DWM frame should be extended into client area by |insets|.
  // Insets are specified in screen pixels not DIP because that's what DWM uses.
  virtual bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const = 0;

  // Returns the minimum and maximum size the window can be resized to by the
  // user.
  virtual void GetMinMaxSize(gfx::Size* min_size,
                             gfx::Size* max_size) const = 0;

  // Returns the current size of the RootView.
  virtual gfx::Size GetRootViewSize() const = 0;

  virtual gfx::Size DIPToScreenSize(const gfx::Size& dip_size) const = 0;

  virtual void ResetWindowControls() = 0;

  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // TODO(beng): Investigate migrating these methods to On* prefixes once
  // HWNDMessageHandler is the WindowImpl.

  // Called when the window was activated or deactivated. |active| reflects the
  // new state.
  virtual void HandleActivationChanged(bool active) = 0;

  // Called when a well known "app command" from the system was performed.
  // Returns true if the command was handled.
  virtual bool HandleAppCommand(int command) = 0;

  // Called from WM_CANCELMODE.
  virtual void HandleCancelMode() = 0;

  // Called when the window has lost mouse capture.
  virtual void HandleCaptureLost() = 0;

  // Called when the user tried to close the window.
  virtual void HandleClose() = 0;

  // Called when a command defined by the application was performed. Returns
  // true if the command was handled.
  virtual bool HandleCommand(int command) = 0;

  // Called when an accelerator is invoked.
  virtual void HandleAccelerator(const ui::Accelerator& accelerator) = 0;

  // Called when the HWND is created.
  virtual void HandleCreate() = 0;

  // Called when the HWND is being destroyed, before any child HWNDs are
  // destroyed.
  virtual void HandleDestroying() = 0;

  // Called after the HWND is destroyed, after all child HWNDs have been
  // destroyed.
  virtual void HandleDestroyed() = 0;

  // Called when the HWND is to be focused for the first time. This is called
  // when the window is shown for the first time. Returns true if the delegate
  // set focus and no default processing should be done by the message handler.
  virtual bool HandleInitialFocus(ui::mojom::WindowShowState show_state) = 0;

  // Called when display settings are adjusted on the system.
  virtual void HandleDisplayChange() = 0;

  // Called when the user begins or ends a size/move operation using the window
  // manager.
  virtual void HandleBeginWMSizeMove() = 0;
  virtual void HandleEndWMSizeMove() = 0;

  // Called when the window's position changed.
  virtual void HandleMove() = 0;

  // Called when the system's work area has changed.
  virtual void HandleWorkAreaChanged() = 0;

  // Called when the window's visibility changed. |visible| holds the new state.
  virtual void HandleVisibilityChanged(bool visible) = 0;

  // Called when a top level window is minimized or restored.
  virtual void HandleWindowMinimizedOrRestored(bool restored) = 0;

  // Called when the window's client size changed. |new_size| holds the new
  // size.
  virtual void HandleClientSizeChanged(const gfx::Size& new_size) = 0;

  // Called when the window's frame has changed.
  virtual void HandleFrameChanged() = 0;

  // Called when focus shifted to this HWND from |last_focused_window|.
  virtual void HandleNativeFocus(HWND last_focused_window) = 0;

  // Called when focus shifted from the HWND to a different window.
  virtual void HandleNativeBlur(HWND focused_window) = 0;

  // Called when a mouse event is received. Returns true if the event was
  // handled by the delegate.
  virtual bool HandleMouseEvent(ui::MouseEvent* event) = 0;

  // Called when an untranslated key event is received (i.e. pre-IME
  // translation).
  virtual void HandleKeyEvent(ui::KeyEvent* event) = 0;

  // Called when a touch event is received.
  virtual void HandleTouchEvent(ui::TouchEvent* event) = 0;

  // Called when an IME message needs to be processed by the delegate. Returns
  // true if the event was handled and no default processing should be
  // performed.
  virtual bool HandleIMEMessage(UINT message,
                                WPARAM w_param,
                                LPARAM l_param,
                                LRESULT* result) = 0;

  // Called when the system input language changes.
  virtual void HandleInputLanguageChange(DWORD character_set,
                                         HKL input_language_id) = 0;

  // Called to compel the delegate to paint |invalid_rect| accelerated.
  virtual void HandlePaintAccelerated(const gfx::Rect& invalid_rect) = 0;

  // Invoked on entering/exiting a menu loop.
  virtual void HandleMenuLoop(bool in_menu_loop) = 0;

  // Catch-all message handling and filtering. Called before
  // HWNDMessageHandler's built-in handling, which may pre-empt some
  // expectations in Views/Aura if messages are consumed. Returns true if the
  // message was consumed by the delegate and should not be processed further
  // by the HWNDMessageHandler. In this case, |result| is returned. |result| is
  // not modified otherwise.
  virtual bool PreHandleMSG(UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* result) = 0;

  // Like PreHandleMSG, but called after HWNDMessageHandler's built-in handling
  // has run and after DefWindowProc.
  virtual void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) = 0;

  // Called when a scroll event is received. Returns true if the event was
  // handled by the delegate.
  virtual bool HandleScrollEvent(ui::ScrollEvent* event) = 0;

  // Called when a gesture event is received. Returns true if the event was
  // handled by the delegate.
  virtual bool HandleGestureEvent(ui::GestureEvent* event) = 0;

  // Called when the window size is about to change.
  virtual void HandleWindowSizeChanging() = 0;

  // Called after HandleWindowSizeChanging() when it's determined the window
  // size didn't actually change.
  virtual void HandleWindowSizeUnchanged() = 0;

  // Called when the window scale factor has changed.
  virtual void HandleWindowScaleFactorChanged(float window_scale_factor) = 0;

  // Called when the headless window bounds has changed.
  virtual void HandleHeadlessWindowBoundsChanged(const gfx::Rect& bounds) = 0;

 protected:
  virtual ~HWNDMessageHandlerDelegate() = default;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_
