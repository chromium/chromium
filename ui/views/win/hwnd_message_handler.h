// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

#include <windows.h>

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/win/window_event_target.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"
#include "ui/views/views_export.h"
#include "ui/views/win/pen_event_processor.h"
#include "ui/views/win/scoped_enable_unadjusted_mouse_events_win.h"

namespace gfx {
class ImageSkia;
class Insets;
}  // namespace gfx

namespace ui {
class AXFragmentRootWin;
class AXSystemCaretWin;
class SessionChangeObserver;
class TextInputClient;
class ViewProp;
class WinCursor;
}  // namespace ui

namespace views {

class FullscreenHandler;
class HWNDMessageHandlerDelegate;

namespace test {
class DesktopWindowTreeHostWinTestApi;
}

// These two messages aren't defined in winuser.h, but they are sent to windows
// with captions. They appear to paint the window caption and frame.
// Unfortunately if you override the standard non-client rendering as we do
// with CustomFrameWindow, sometimes Windows (not deterministically
// reproducibly but definitely frequently) will send these messages to the
// window and paint the standard caption/title over the top of the custom one.
// So we need to handle these messages in CustomFrameWindow to prevent this
// from happening.
constexpr int WM_NCUAHDRAWCAPTION = 0xAE;
constexpr int WM_NCUAHDRAWFRAME = 0xAF;

// The HWNDMessageHandler sends this message to itself on
// WM_WINDOWPOSCHANGING. It's used to inform the client if a
// WM_WINDOWPOSCHANGED won't be received.
constexpr int WM_WINDOWSIZINGFINISHED = WM_USER;

// An object that handles messages for a HWND that implements the views
// "Custom Frame" look. The purpose of this class is to isolate the windows-
// specific message handling from the code that wraps it. It is intended to be
// used by both a views::NativeWidget and an aura::WindowTreeHost
// implementation.
// TODO(beng): This object should eventually *become* the WindowImpl.
class VIEWS_EXPORT HWNDMessageHandler : public gfx::WindowImpl,
                                        public ui::InputMethodObserver,
                                        public ui::WindowEventTarget,
                                        public ui::AXFragmentRootDelegateWin {
 public:
  // See WindowImpl for details on |debugging_id|.
  static std::unique_ptr<HWNDMessageHandler> Create(
      HWNDMessageHandlerDelegate* delegate,
      const std::string& debugging_id,
      bool headless_mode);

  HWNDMessageHandler(const HWNDMessageHandler&) = delete;
  HWNDMessageHandler& operator=(const HWNDMessageHandler&) = delete;

  ~HWNDMessageHandler() override;

  virtual void Init(HWND parent, const gfx::Rect& bounds);
  virtual void InitModalType(ui::mojom::ModalType modal_type);

  virtual void Close();
  virtual void CloseNow();

  virtual gfx::Rect GetWindowBoundsInScreen() const;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const;
  virtual gfx::Rect GetRestoredBounds() const;
  // This accounts for the case where the widget size is the client size.
  virtual gfx::Rect GetClientAreaBounds() const;

  virtual void GetWindowPlacement(gfx::Rect* bounds,
                                  ui::mojom::WindowShowState* show_state) const;

  // Sets the bounds of the HWND to |bounds_in_pixels|. If the HWND size is not
  // changed, |force_size_changed| determines if we should pretend it is.
  virtual void SetBounds(const gfx::Rect& bounds_in_pixels,
                         bool force_size_changed);

  virtual void SetSize(const gfx::Size& size);
  virtual void CenterWindow(const gfx::Size& size);

  virtual void SetRegion(HRGN rgn);

  virtual void StackAbove(HWND other_hwnd);
  virtual void StackAtTop();

  // Sets the parent of the HWND if it is a child window. Otherwise, sets the
  // owner of the HWND.
  virtual void SetParentOrOwner(HWND new_parent);

  // Shows the window. If |show_state| is maximized, |pixel_restore_bounds| is
  // the bounds to restore the window to when going back to normal.
  virtual void Show(ui::mojom::WindowShowState show_state,
                    const gfx::Rect& pixel_restore_bounds);
  virtual void Hide();

  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();

  virtual void Activate();
  virtual void Deactivate();

  virtual void SetAlwaysOnTop(bool on_top);

  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual bool IsMinimized() const;
  virtual bool IsMaximized() const;
  virtual bool IsFullscreen() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool IsHeadless() const;

  virtual bool RunMoveLoop(const gfx::Vector2d& drag_offset,
                           bool hide_on_escape);
  virtual void EndMoveLoop();

  // Tells the HWND its client area has changed.
  virtual void SendFrameChanged();

  virtual void FlashFrame(bool flash);

  virtual void ClearNativeFocus();

  virtual void SetCapture();
  virtual void ReleaseCapture();
  virtual bool HasCapture() const;

  virtual FullscreenHandler* fullscreen_handler();

  virtual void SetVisibilityChangedAnimationsEnabled(bool enabled);

  // Returns true if the title changed.
  virtual bool SetTitle(const std::u16string& title);

  virtual void SetCursor(scoped_refptr<ui::WinCursor> cursor);

  virtual void FrameTypeChanged();

  virtual void PaintAsActiveChanged();

  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon);

  // Set the fullscreen state. `target_display_id` indicates the display where
  // the window should be shown fullscreen; display::kInvalidDisplayId indicates
  // that no display was specified, so the current display may be used.
  virtual void SetFullscreen(bool fullscreen, int64_t target_display_id);

  // Updates the aspect ratio of the window.
  virtual void SetAspectRatio(float aspect_ratio,
                              const gfx::Size& excluded_mar);

  // Updates the window style to reflect whether it can be resized or maximized.
  virtual void SizeConstraintsChanged();

  // Lets pen events fall through to the default window handler until the next
  // WM_POINTERUP event.
  static void UseDefaultHandlerForPenEventsUntilPenUp();

  // Returns true if content is rendered to a child window instead of directly
  // to this window.
  virtual bool HasChildRenderingWindow();

  virtual void set_is_translucent(bool is_translucent);
  virtual bool is_translucent() const;

  virtual std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
  RegisterUnadjustedMouseEvent();

  virtual void set_using_wm_input(bool using_wm_input);
  virtual bool using_wm_input() const;

 protected:
  HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate,
                     const std::string& debugging_id);

  // Performs post initialization steps. Extracted from Init() so it could be
  // shared with the subclasses.
  void InitExtras();

  // Returns true if |insets| was modified to define a custom client area for
  // the window, false if the default client area should be used. If false is
  // returned, |insets| is not modified.  |monitor| is the monitor this
  // window is on.  Normally that would be determined from the HWND, but
  // during WM_NCCALCSIZE Windows does not return the correct monitor for the
  // HWND, so it must be passed in explicitly (see HWNDMessageHandler::
  // OnNCCalcSize for more details).
  bool GetClientAreaInsets(gfx::Insets* insets, HMONITOR monitor) const;

  // Helper function for setting the bounds of the HWND. For more information
  // please refer to the SetBounds() function.
  virtual void SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                                 bool force_size_changed);

  // These are shared with subclasses.
  static bool IsTopLevelWindow(HWND window);
  static bool GetMonitorAndRects(const RECT& rect,
                                 HMONITOR* monitor,
                                 gfx::Rect* monitor_rect,
                                 gfx::Rect* work_area);

 private:
  friend class ::views::test::DesktopWindowTreeHostWinTestApi;

  using TouchIDs = std::set<DWORD>;
  enum class DwmFrameState { kOff, kOn };

  // Overridden from WindowImpl:
  HICON GetDefaultWindowIcon() const override;
  HICON GetSmallWindowIcon() const override;
  LRESULT OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) override;

  // Overridden from InputMethodObserver
  void OnFocus() override;
  void OnBlur() override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;

  // Overridden from WindowEventTarget
  LRESULT HandleMouseMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override;
  LRESULT HandleKeyboardMessage(unsigned int message,
                                WPARAM w_param,
                                LPARAM l_param,
                                bool* handled) override;
  LRESULT HandleTouchMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override;
  LRESULT HandlePointerMessage(unsigned int message,
                               WPARAM w_param,
                               LPARAM l_param,
                               bool* handled) override;
  LRESULT HandleInputMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override;
  LRESULT HandleScrollMessage(unsigned int message,
                              WPARAM w_param,
                              LPARAM l_param,
                              bool* handled) override;
  LRESULT HandleNcHitTestMessage(unsigned int message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 bool* handled) override;
  void HandleParentChanged() override;
  void ApplyPinchZoomScale(float scale) override;
  void ApplyPinchZoomBegin() override;
  void ApplyPinchZoomEnd() override;
  void ApplyPanGestureScroll(int scroll_x, int scroll_y) override;
  void ApplyPanGestureFling(int scroll_x, int scroll_y) override;
  void ApplyPanGestureScrollBegin(int scroll_x, int scroll_y) override;
  void ApplyPanGestureScrollEnd(bool transitioning_to_pinch) override;
  void ApplyPanGestureFlingBegin() override;
  void ApplyPanGestureFlingEnd() override;

  // Overridden from AXFragmentRootDelegateWin.
  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override;
  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override;
  bool IsAXFragmentRootAControlElement() override;

  void ApplyPanGestureEvent(int scroll_x,
                            int scroll_y,
                            ui::EventMomentumPhase momentum_phase,
                            ui::ScrollEventPhase phase);

  // Returns the auto-hide edges of the appbar. See
  // ViewsDelegate::GetAppbarAutohideEdges() for details. If the edges change,
  // OnAppbarAutohideEdgesChanged() is called.
  int GetAppbarAutohideEdges(HMONITOR monitor);

  // Callback if the autohide edges have changed. See
  // ViewsDelegate::GetAppbarAutohideEdges() for details.
  void OnAppbarAutohideEdgesChanged();

  // Can be called after the delegate has had the opportunity to set focus and
  // did not do so.
  void SetInitialFocus();

  // Called after the WM_ACTIVATE message has been processed by the default
  // windows procedure.
  void PostProcessActivateMessage(int activation_state,
                                  bool minimized,
                                  HWND window_gaining_or_losing_activation);

  // Enables disabled owner windows that may have been disabled due to this
  // window's modality.
  void RestoreEnabledIfNecessary();

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  // Responds to the client area changing size, either at window creation time
  // or subsequently.
  void ClientAreaSizeChanged();

  // Resets the window region for the current widget bounds if necessary.
  // If |force| is true, the window region is reset to NULL even for native
  // frame windows.
  void ResetWindowRegion(bool force, bool redraw);

  // Calls DefWindowProc, safely wrapping the call in a ScopedRedrawLock to
  // prevent frame flicker. DefWindowProc handling can otherwise render the
  // classic-look window title bar directly.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates();
  void UnlockUpdates();

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Attempts to force the window to be redrawn, ensuring that it gets
  // onscreen.
  void ForceRedrawWindow(int attempts);

  // Returns whether Windows should help with frame rendering (i.e. we're using
  // the glass frame).
  bool IsFrameSystemDrawn() const;

  // Returns true if IsFrameSystemDrawn() and there's actually a frame to draw.
  bool HasSystemFrame() const;

  // Adds or removes the frame extension into client area with
  // DwmExtendFrameIntoClientArea.
  void SetDwmFrameExtension(DwmFrameState state);

  // Message Handlers ----------------------------------------------------------

  CR_BEGIN_MSG_MAP_EX(HWNDMessageHandler)
    // Range handlers must go first!
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK,
                                OnMouseRange)

    // CustomFrameWindow hacks
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWCAPTION, OnNCUAHDrawCaption)
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWFRAME, OnNCUAHDrawFrame)

    // Win 8.1 and newer
    CR_MESSAGE_HANDLER_EX(WM_DPICHANGED, OnDpiChanged)

    // Non-atlcrack.h handlers
    CR_MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)

    // Mouse events.
    CR_MESSAGE_HANDLER_EX(WM_MOUSEACTIVATE, OnMouseActivate)
    CR_MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_NCMOUSELEAVE, OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_SETCURSOR, OnSetCursor);

    // Pointer events.
    CR_MESSAGE_HANDLER_EX(WM_POINTERACTIVATE, OnPointerActivate)
    CR_MESSAGE_HANDLER_EX(WM_POINTERDOWN, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_POINTERUP, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_POINTERUPDATE, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_POINTERENTER, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_POINTERLEAVE, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_NCPOINTERDOWN, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_NCPOINTERUP, OnPointerEvent)
    CR_MESSAGE_HANDLER_EX(WM_NCPOINTERUPDATE, OnPointerEvent)

    // Key events.
    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)

    CR_MESSAGE_HANDLER_EX(WM_INPUT, OnInputEvent)
    // IME Events.
    CR_MESSAGE_HANDLER_EX(WM_IME_SETCONTEXT, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_STARTCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_ENDCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_REQUEST, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_NOTIFY, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnImeMessages)

    // Scroll events
    CR_MESSAGE_HANDLER_EX(WM_VSCROLL, OnScrollMessage)
    CR_MESSAGE_HANDLER_EX(WM_HSCROLL, OnScrollMessage)

    // Touch Events.
    CR_MESSAGE_HANDLER_EX(WM_TOUCH, OnTouchEvent)

    CR_MESSAGE_HANDLER_EX(WM_WINDOWSIZINGFINISHED, OnWindowSizingFinished)

    // Uses the general handler macro since the specific handler macro
    // MSG_WM_NCACTIVATE would convert WPARAM type to BOOL type. The high
    // word of WPARAM could be set when the window is minimized or restored.
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    CR_MSG_WM_ACTIVATEAPP(OnActivateApp)
    CR_MSG_WM_APPCOMMAND(OnAppCommand)
    CR_MSG_WM_CANCELMODE(OnCancelMode)
    CR_MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_COMMAND(OnCommand)
    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_DISPLAYCHANGE(OnDisplayChange)
    CR_MSG_WM_ENTERMENULOOP(OnEnterMenuLoop)
    CR_MSG_WM_EXITMENULOOP(OnExitMenuLoop)
    CR_MSG_WM_ENTERSIZEMOVE(OnEnterSizeMove)
    CR_MSG_WM_ERASEBKGND(OnEraseBkgnd)
    CR_MSG_WM_EXITSIZEMOVE(OnExitSizeMove)
    CR_MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
    CR_MSG_WM_INITMENU(OnInitMenu)
    CR_MSG_WM_INPUTLANGCHANGE(OnInputLangChange)
    CR_MSG_WM_KILLFOCUS(OnKillFocus)
    CR_MSG_WM_MOVE(OnMove)
    CR_MSG_WM_MOVING(OnMoving)
    CR_MSG_WM_NCCALCSIZE(OnNCCalcSize)
    CR_MSG_WM_NCCREATE(OnNCCreate)
    CR_MSG_WM_NCHITTEST(OnNCHitTest)
    CR_MSG_WM_NCPAINT(OnNCPaint)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_SETFOCUS(OnSetFocus)
    CR_MSG_WM_SETICON(OnSetIcon)
    CR_MSG_WM_SETTEXT(OnSetText)
    CR_MSG_WM_SETTINGCHANGE(OnSettingChange)
    CR_MSG_WM_SIZE(OnSize)
    CR_MSG_WM_SIZING(OnSizing)
    CR_MSG_WM_SYSCOMMAND(OnSysCommand)
    CR_MSG_WM_THEMECHANGED(OnThemeChanged)
    CR_MSG_WM_TIMECHANGE(OnTimeChange)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
    CR_MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
  CR_END_MSG_MAP()

  // Message Handlers.
  // This list is in _ALPHABETICAL_ order!
  // TODO(beng): Once this object becomes the WindowImpl, these methods can
  //             be made private.
  void OnActivateApp(BOOL active, DWORD thread_id);
  // TODO(beng): return BOOL is temporary until this object becomes a
  //             WindowImpl.
  BOOL OnAppCommand(HWND window, int command, WORD device, WORD keystate);
  void OnCancelMode();
  void OnCaptureChanged(HWND window);
  void OnClose();
  void OnCommand(UINT notification_code, int command, HWND window);
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnDisplayChange(UINT bits_per_pixel, const gfx::Size& screen_size);
  LRESULT OnDpiChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  void OnEnterMenuLoop(BOOL from_track_popup_menu);
  void OnEnterSizeMove();
  LRESULT OnEraseBkgnd(HDC dc);
  void OnExitMenuLoop(BOOL is_shortcut_menu);
  void OnExitSizeMove();
  void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInitMenu(HMENU menu);
  LRESULT OnInputEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnKillFocus(HWND focused_window);
  LRESULT OnMouseActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnPointerActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnPointerEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnMove(const gfx::Point& point);
  void OnMoving(UINT param, const RECT* new_bounds);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  LRESULT OnNCCreate(LPCREATESTRUCT lpCreateStruct);
  LRESULT OnNCHitTest(const gfx::Point& point);
  void OnNCPaint(HRGN rgn);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  void OnPaint(HDC dc);
  LRESULT OnReflectedMessage(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnScrollMessage(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);
  void OnSetFocus(HWND last_focused_window);
  LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  LRESULT OnSetText(const wchar_t* text);
  void OnSettingChange(UINT flags, const wchar_t* section);
  void OnSize(UINT param, const gfx::Size& size);
  void OnSizing(UINT param, RECT* rect);
  void OnSysCommand(UINT notification_code, const gfx::Point& point);
  void OnThemeChanged();
  void OnTimeChange();
  LRESULT OnTouchEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnWindowPosChanging(WINDOWPOS* window_pos);
  void OnWindowPosChanged(WINDOWPOS* window_pos);
  LRESULT OnWindowSizingFinished(UINT message, WPARAM w_param, LPARAM l_param);

  // Receives Windows Session Change notifications.
  void OnSessionChange(WPARAM status_code, const bool* is_current_session);

  using TouchEvents = std::vector<ui::TouchEvent>;
  // Helper to handle the list of touch events passed in. We need this because
  // touch events on windows don't fire if we enter a modal loop in the context
  // of a touch event.
  void HandleTouchEvents(const TouchEvents& touch_events);

  // Resets the flag which indicates that we are in the context of a touch down
  // event.
  void ResetTouchDownContext();

  // Helper to handle mouse events.
  // The |message|, |w_param|, |l_param| parameters identify the Windows mouse
  // message and its parameters respectively.
  // The |track_mouse| parameter indicates if we should track the mouse.
  LRESULT HandleMouseEventInternal(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   bool track_mouse);

  // We handle 2 kinds of WM_POINTER events: PT_TOUCH and PT_PEN. This helper
  // handles client area events of PT_TOUCH, and non-client area events of both
  // kinds.
  LRESULT HandlePointerEventTypeTouchOrNonClient(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param);

  // Helper to handle client area events of PT_PEN.
  LRESULT HandlePointerEventTypePenClient(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param);

  // Helper to handle client area events of PT_PEN.
  LRESULT HandlePointerEventTypePen(UINT message,
                                    UINT32 pointer_id,
                                    POINTER_PEN_INFO pointer_pen_info);
  // Returns true if the mouse message passed in is an OS synthesized mouse
  // message.
  // |message| identifies the mouse message.
  // |message_time| is the time when the message occurred.
  // |l_param| indicates the location of the mouse message.
  bool IsSynthesizedMouseMessage(unsigned int message,
                                 int message_time,
                                 LPARAM l_param);

  // Provides functionality to transition a frame to DWM.
  void PerformDwmTransition();

  // Updates DWM frame to extend into client area if needed.
  void UpdateDwmFrame();

  // Generates a touch event and adds it to the |touch_events| parameter.
  // |point| is the point where the touch was initiated.
  // |id| is the event id associated with the touch event.
  // |time_stamp| is the time stamp associated with the message.
  void GenerateTouchEvent(ui::EventType event_type,
                          const gfx::Point& point,
                          ui::PointerId id,
                          base::TimeTicks time_stamp,
                          TouchEvents* touch_events);

  // Handles WM_NCLBUTTONDOWN and WM_NCMOUSEMOVE messages on the caption.
  // Returns true if the message was handled.
  bool HandleMouseInputForCaption(unsigned int message,
                                  WPARAM w_param,
                                  LPARAM l_param);

  // Checks if there is a full screen window on the same monitor as the
  // |window| which is becoming active. If yes then we reduce the size of the
  // fullscreen window by 1 px to ensure that maximized windows on the same
  // monitor don't draw over the taskbar.
  void CheckAndHandleBackgroundFullscreenOnMonitor(HWND window);

  // Provides functionality to reduce the bounds of the fullscreen window by 1
  // px on activation loss to a window on the same monitor.
  void OnBackgroundFullscreen();

  // Deletes the system caret used for accessibility. This will result in any
  // clients that are still holding onto its |IAccessible| to get a failure code
  // if they request its location.
  void DestroyAXSystemCaret();

  // Updates |rect| to adhere to the |aspect_ratio| of the window. |param|
  // refers to the edge of the window being sized.
  void SizeWindowToAspectRatio(UINT param, gfx::Rect* rect);

  // Get the cursor position, which may be mocked if running a test
  POINT GetCursorPos() const;

  raw_ptr<HWNDMessageHandlerDelegate> delegate_;

  std::unique_ptr<FullscreenHandler> fullscreen_handler_;

  // Set to true in Close() and false is CloseNow().
  bool waiting_for_close_now_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // The current cursor.
  scoped_refptr<ui::WinCursor> current_cursor_;

  // The icon created from the bitmap image of the window icon.
  base::win::ScopedHICON window_icon_;

  // The icon created from the bitmap image of the app icon.
  base::win::ScopedHICON app_icon_;

  // The aspect ratio for the window. This is only used for sizing operations
  // for the non-client area.
  std::optional<float> aspect_ratio_;

  // Size to exclude from aspect ratio calculation.
  gfx::Size excluded_margin_;

  // The current DPI.
  int dpi_;

  // This is true if the window is created with a specific size/location, as
  // opposed to having them set after window creation.
  bool initial_bounds_valid_ = false;

  // Whether EnableNonClientDpiScaling was called successfully with this window.
  // This flag exists because EnableNonClientDpiScaling must be called during
  // WM_NCCREATE and EnableChildWindowDpiMessage is called after window
  // creation. We don't want to call both, so this helps us determine if a call
  // to EnableChildWindowDpiMessage is necessary.
  bool called_enable_non_client_dpi_scaling_;

  // Event handling ------------------------------------------------------------

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The set of touch devices currently down.
  TouchIDs touch_ids_;

  // ScopedRedrawLock ----------------------------------------------------------

  // Represents the number of ScopedRedrawLocks active against this widget.
  // If this is greater than zero, the widget should be locked against updates.
  int lock_updates_count_;

  // Window resizing -----------------------------------------------------------

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // Keeps track of the last size type param received from a WM_SIZE message.
  UINT last_size_param_ = SIZE_RESTORED;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  // True the first time nccalc is called on a sizable widget
  bool is_first_nccalc_;

  // Copy of custom window region specified via SetRegion(), if any.
  base::win::ScopedRegion custom_window_region_;

  // If > 0 indicates a menu is running (we're showing a native menu).
  int menu_depth_;

  // Generates touch-ids for touch-events.
  ui::SequentialIDGenerator id_generator_;

  PenEventProcessor pen_processor_;

  // Stores a pointer to the WindowEventTarget interface implemented by this
  // class. Allows callers to retrieve the interface pointer.
  std::unique_ptr<ui::ViewProp> prop_window_target_;

  // Number of active touch down contexts. This is incremented on touch down
  // events and decremented later using a delayed task.
  // We need this to ignore WM_MOUSEACTIVATE messages generated in response to
  // touch input. This is fine because activation still works correctly via
  // native SetFocus calls invoked in the views code.
  int touch_down_contexts_;

  // Time the last touch or pen message was received. Used to flag mouse
  // messages synthesized by Windows for touch which are not flagged by the OS
  // as synthesized mouse messages. For more information please refer to the
  // IsMouseEventFromTouch function.
  static LONG last_touch_or_pen_message_time_;

  // When true, this flag makes us discard window management mouse messages.
  // Windows sends window management mouse messages at the mouse location when
  // window states change (e.g. tooltips or status bubbles opening/closing).
  // Those system generated messages should be ignored while the pen is active
  // over the client area, where it is not in sync with the mouse position.
  // Reset to false when we get user mouse input again.
  static bool is_pen_active_in_client_area_;

  // If true, all pen events in the client area should be handled. If false,
  // unhandled pen events will be allowed to fall through to the default
  // handler. This should only be false in limited cases, as the default handler
  // generates duplicate WM_MOUSE compatibility events for pen events it sees.
  static bool handle_pen_events_in_client_area_;

  // Time the last WM_MOUSEHWHEEL message is received. Please refer to the
  // HandleMouseEventInternal function as to why this is needed.
  LONG last_mouse_hwheel_time_;

  // On Windows Vista and beyond, if we are transitioning from custom frame
  // to Aero(glass) we delay setting the DWM related properties in full
  // screen mode as DWM is not supported in full screen windows. We perform
  // the DWM related operations when the window comes out of fullscreen mode.
  // This member variable is set to true if the window is transitioning to
  // glass. Defaults to false.
  bool dwm_transition_desired_;

  // True if HandleWindowSizeChanging has been called in the delegate, but not
  // HandleClientSizeChanged.
  bool sent_window_size_changing_;

  // This is used to keep track of whether a WM_WINDOWPOSCHANGED has
  // been received after the WM_WINDOWPOSCHANGING.
  uint32_t current_window_size_message_ = 0;

  // Manages observation of Windows Session Change messages.
  std::unique_ptr<ui::SessionChangeObserver> session_change_observer_;

  // Some assistive software need to track the location of the caret.
  std::unique_ptr<ui::AXSystemCaretWin> ax_system_caret_;

  // Implements IRawElementProviderFragmentRoot when UIA is enabled.
  std::unique_ptr<ui::AXFragmentRootWin> ax_fragment_root_;

  // Set to true when we return a UIA object. Determines whether we need to
  // call UIA to clean up object references on window destruction.
  // This is important to avoid triggering a cross-thread COM call which could
  // cause re-entrancy during teardown. https://crbug.com/1087553
  bool did_return_uia_object_;

  // The location where the user clicked on the caption. We cache this when we
  // receive the WM_NCLBUTTONDOWN message. We use this in the subsequent
  // WM_NCMOUSEMOVE message to see if the mouse actually moved.
  // Please refer to the HandleMouseEventInternal function for details on why
  // this is needed.
  gfx::Point caption_left_button_click_pos_;

  // Set to true if the left mouse button has been pressed on the caption.
  // Defaults to false.
  bool left_button_down_on_caption_;

  // Set to true if the window is a background fullscreen window, i.e a
  // fullscreen window which lost activation. Defaults to false.
  bool background_fullscreen_hack_;

  // True if the window should have no border and its contents should be
  // partially or fully transparent.
  bool is_translucent_ = false;

  // True if the window should process WM_POINTER for touch events and
  // not WM_TOUCH events.
  bool pointer_events_for_touch_;

  // True if DWM frame should be cleared on next WM_ERASEBKGND message.  This is
  // necessary to avoid white flashing in the titlebar area around the
  // minimize/maximize/close buttons.  Clearing the frame on every WM_ERASEBKGND
  // message causes black flickering in the titlebar region so we do it on for
  // the first message after frame type changes.
  bool needs_dwm_frame_clear_ = true;

  // True if is handling mouse WM_INPUT messages.
  bool using_wm_input_ = false;

  // True if we're displaying the system menu on the title bar. If we are,
  // then we want to ignore right mouse clicks instead of bringing up a
  // context menu.
  bool handling_mouse_menu_ = false;

  // This is set to true when we call ShowWindow(SC_RESTORE), in order to
  // call HandleWindowMinimizedOrRestored() when we get a WM_ACTIVATE message.
  bool notify_restore_on_activate_ = false;

  // Counts the number of drag events received after a drag started event.
  // This will be used to ignore a drag event to 0, 0, if it is one of the
  // first few drag events after a drag started event. We randomly receive
  // bogus 0, 0 drag events after the start of a drag. See
  // https://crbug.com/1270828.
  int num_drag_events_after_press_ = 0;

  // Records ::GetLastError when ::ReleaseCapture fails. Logged in the DCHECK
  // in `SetCapture` to diagnose https://crbug.com/1386013.
  DWORD release_capture_errno_ = 0;

  // This is a map of the HMONITOR to full screeen window instance. It is safe
  // to keep a raw pointer to the HWNDMessageHandler instance as we track the
  // window destruction and ensure that the map is cleaned up.
  using FullscreenWindowMonitorMap = std::map<HMONITOR, HWNDMessageHandler*>;
  static base::LazyInstance<FullscreenWindowMonitorMap>::DestructorAtExit
      fullscreen_monitor_map_;

  // How many pixels the window is expected to grow from OnWindowPosChanging().
  // Used to fill the newly exposed pixels black in OnPaint() before the
  // browser compositor is able to redraw at the new window size.
  gfx::Size exposed_pixels_;

  // Populated if the cursor position is being mocked for testing purposes.
  std::optional<gfx::Point> mock_cursor_position_;

  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      observation_{this};

  // The WeakPtrFactories below (one inside the
  // CR_MSG_MAP_CLASS_DECLARATIONS macro and autohide_factory_) must
  // occur last in the class definition so they get destroyed last.

  CR_MSG_MAP_CLASS_DECLARATIONS(HWNDMessageHandler)

  // The factory used to lookup appbar autohide edges.
  base::WeakPtrFactory<HWNDMessageHandler> autohide_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
