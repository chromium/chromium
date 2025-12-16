// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_
#define UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window_export.h"

namespace ui {
class WinCursor;

class WIN_WINDOW_EXPORT WinWindow : public PlatformWindow,
                                    public gfx::WindowImpl,
                                    public InputMethodObserver {
 public:
  WinWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);

  WinWindow(const WinWindow&) = delete;
  WinWindow& operator=(const WinWindow&) = delete;

  ~WinWindow() override;

  // Set an `input_method` object to handle IME-related window messages. If it
  // is not set, the window will use the default handler function to process
  // these messages.
  // When `input_method` is destroyed, it will be automatically set to nullptr.
  void SetInputMethod(InputMethod* input_method);

 private:
  void Destroy();

  // InputMethodObserver:
  void OnInputMethodDestroyed(const InputMethod* input_method) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void OnTextInputStateChanged(const TextInputClient* client) override;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void StackAbove(gfx::AcceleratedWidget widget) override;
  void StackAtTop() override;
  void FlashFrame(bool flash_frame) override;
  void SetVisibilityChangedAnimationsEnabled(bool enabled) override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  bool IsAnimatingClosed() const override;

  bool IsFullscreen() const;

  CR_BEGIN_MSG_MAP_EX(WinWindow)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK,
                                OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_SETCURSOR, OnSetCursor);
    CR_MESSAGE_HANDLER_EX(WM_CAPTURECHANGED, OnCaptureChanged)

    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_IME_SETCONTEXT, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_STARTCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_ENDCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_REQUEST, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_NOTIFY, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_CHAR, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)
    CR_MSG_WM_INPUTLANGCHANGE(OnInputLangChange)

    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  CR_END_MSG_MAP()

  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnCaptureChanged(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  LRESULT OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  void OnClose();
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnPaint(HDC);
  void OnWindowPosChanged(WINDOWPOS* window_pos);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);

  raw_ptr<PlatformWindowDelegate> delegate_;

  // Keep a reference to the current cursor to make sure the wrapped HCURSOR
  // isn't destroyed after the call to SetCursor().
  scoped_refptr<WinCursor> cursor_;

  // An `InputMethod` pointer for handling IME messages.
  // When the `input_method_` is destroyed, it will be set to null.
  raw_ptr<InputMethod> input_method_;

  CR_MSG_MAP_CLASS_DECLARATIONS(WinWindow)
};

namespace test {

// Set true to let WindowTreeHostWin use a popup window
// with no frame/title so that the window size and test's
// expectations matches.
WIN_WINDOW_EXPORT void SetUsePopupAsRootWindowForTest(bool use);

}  // namespace test

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_
