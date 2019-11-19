// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls_internal_win.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/win_util.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"

namespace {

// InputDispatcher ------------------------------------------------------------

// InputDispatcher is used to listen for a mouse/keyboard event. Only one
// instance may be alive at a time. The callback is run when the appropriate
// event is received.
class InputDispatcher {
 public:
  // Constructs an InputDispatcher that will invoke |callback| when
  // |message_type| is received. This must be invoked on thread, after the input
  // is sent but before it is processed.
  static void CreateForMouseEvent(base::OnceClosure callback,
                                  WPARAM message_type);

  // Special case of CreateForMessage() for WM_KEYUP (can await multiple events
  // when modifiers are involved).
  static void CreateForKeyUp(base::OnceClosure callback,
                             int num_keyups_awaited);

  // Special case of CreateForMessage() for WM_MOUSEMOVE. Upon receipt, an error
  // message is logged if the destination of the move is not |screen_point|.
  // |callback| is run regardless after a sufficiently long delay. This
  // generally happens when another process has a window over the test's window,
  // or if |screen_point| is not over a window owned by the test.
  static void CreateForMouseMove(base::OnceClosure callback,
                                 const gfx::Point& screen_point);

 private:
  // Generic message
  InputDispatcher(base::OnceClosure callback,
                  WPARAM message_waiting_for,
                  UINT system_queue_flag);

  // WM_KEYUP
  InputDispatcher(base::OnceClosure callback,
                  WPARAM message_waiting_for,
                  UINT system_queue_flag,
                  int num_keyups_awaited);

  // WM_MOUSEMOVE
  InputDispatcher(base::OnceClosure callback,
                  WPARAM message_waiting_for,
                  UINT system_queue_flag,
                  const gfx::Point& screen_point);

  ~InputDispatcher();

  // Installs the dispatcher as the current hook.
  void InstallHook();

  // Callback from hook when a mouse message is received.
  static LRESULT CALLBACK MouseHook(int n_code, WPARAM w_param, LPARAM l_param);

  // Callback from hook when a key message is received.
  static LRESULT CALLBACK KeyHook(int n_code, WPARAM w_param, LPARAM l_param);

  // Invoked from the hook. If |message_id| matches message_waiting_for_
  // MatchingMessageProcessed() is invoked. |mouse_hook_struct| contains extra
  // information about the mouse event.
  void DispatchedMessage(UINT message_id,
                         const MOUSEHOOKSTRUCT* mouse_hook_struct);

  // Invoked when a matching event is found. Must be invoked through a task
  // posted from the hook so that the event, which is processed after the hook,
  // has already been handled.
  // |definitively_done| is set to true if this event is definitely the one we
  // were waiting for (i.e., we will resume regardless of the presence of
  // |system_queue_flag_| messages in the queue).
  void MatchingMessageProcessed(bool definitively_done);

  // Invoked when the hook for a mouse move is not called within a reasonable
  // time. This likely means that a window from another process is over a test
  // window, so the event does not reach this process.
  void OnTimeout();

  // The current dispatcher if a hook is installed; otherwise, nullptr;
  static InputDispatcher* current_dispatcher_;

  // Return value from SetWindowsHookEx.
  static HHOOK next_hook_;

  THREAD_CHECKER(thread_checker_);

  // The callback to run when the desired message is received.
  base::OnceClosure callback_;

  // The message on which the instance is waiting -- unused for WM_KEYUP
  // messages.
  const WPARAM message_waiting_for_;

  // The system queue flag (ref. ::GetQueueStatus) which the awaited event is
  // reflected in.
  const UINT system_queue_flag_;

  // The number of WM_KEYUP messages to receive before dispatching |callback_|.
  // Only relevant when |message_waiting_for_| is WM_KEYUP.
  int num_keyups_awaited_ = 0;

  // The desired mouse position for a mouse move event.
  const gfx::Point expected_mouse_location_;

  base::WeakPtrFactory<InputDispatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputDispatcher);
};

// static
InputDispatcher* InputDispatcher::current_dispatcher_ = nullptr;

// static
HHOOK InputDispatcher::next_hook_ = nullptr;

// static
void InputDispatcher::CreateForMouseEvent(base::OnceClosure callback,
                                          WPARAM message_type) {
  DCHECK(message_type == WM_LBUTTONDOWN || message_type == WM_LBUTTONUP ||
         message_type == WM_MBUTTONDOWN || message_type == WM_MBUTTONUP ||
         message_type == WM_RBUTTONDOWN || message_type == WM_RBUTTONUP)
      << message_type;

  // Owns self.
  new InputDispatcher(std::move(callback), message_type, QS_MOUSEBUTTON);
}

// static
void InputDispatcher::CreateForKeyUp(base::OnceClosure callback,
                                     int num_keyups_awaited) {
  // Owns self.
  new InputDispatcher(std::move(callback), WM_KEYUP, QS_KEY,
                      num_keyups_awaited);
}

// static
void InputDispatcher::CreateForMouseMove(base::OnceClosure callback,
                                         const gfx::Point& screen_point) {
  // Owns self.
  new InputDispatcher(std::move(callback), WM_MOUSEMOVE, QS_MOUSEMOVE,
                      screen_point);
}

InputDispatcher::InputDispatcher(base::OnceClosure callback,
                                 WPARAM message_waiting_for,
                                 UINT system_queue_flag)
    : callback_(std::move(callback)),
      message_waiting_for_(message_waiting_for),
      system_queue_flag_(system_queue_flag) {
  InstallHook();
}

InputDispatcher::InputDispatcher(base::OnceClosure callback,
                                 WPARAM message_waiting_for,
                                 UINT system_queue_flag,
                                 int num_keyups_awaited)
    : callback_(std::move(callback)),
      message_waiting_for_(message_waiting_for),
      system_queue_flag_(system_queue_flag),
      num_keyups_awaited_(num_keyups_awaited) {
  DCHECK_EQ(message_waiting_for_, static_cast<WPARAM>(WM_KEYUP));
  InstallHook();
}

InputDispatcher::InputDispatcher(base::OnceClosure callback,
                                 WPARAM message_waiting_for,
                                 UINT system_queue_flag,
                                 const gfx::Point& screen_point)
    : callback_(std::move(callback)),
      message_waiting_for_(message_waiting_for),
      system_queue_flag_(system_queue_flag),
      expected_mouse_location_(screen_point) {
  DCHECK_EQ(message_waiting_for_, static_cast<WPARAM>(WM_MOUSEMOVE));
  InstallHook();
}

InputDispatcher::~InputDispatcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(current_dispatcher_, this);
  current_dispatcher_ = nullptr;
  UnhookWindowsHookEx(next_hook_);
  next_hook_ = nullptr;
}

void InputDispatcher::InstallHook() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!current_dispatcher_);

  current_dispatcher_ = this;

  int hook_type;
  HOOKPROC hook_function;
  if (message_waiting_for_ == WM_KEYUP) {
    hook_type = WH_KEYBOARD;
    hook_function = &KeyHook;
  } else {
    // WH_CALLWNDPROCRET does not generate mouse messages for some reason.
    hook_type = WH_MOUSE;
    hook_function = &MouseHook;
    if (message_waiting_for_ == WM_MOUSEMOVE) {
      // Things don't go well with move events sometimes. Bail out if it takes
      // too long.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&InputDispatcher::OnTimeout,
                         weak_factory_.GetWeakPtr()),
          TestTimeouts::action_timeout());
    }
  }
  next_hook_ =
      SetWindowsHookEx(hook_type, hook_function, nullptr, GetCurrentThreadId());
  DPCHECK(next_hook_);
}

// static
LRESULT CALLBACK InputDispatcher::MouseHook(int n_code,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    current_dispatcher_->DispatchedMessage(
        static_cast<UINT>(w_param),
        reinterpret_cast<MOUSEHOOKSTRUCT*>(l_param));
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

// static
LRESULT CALLBACK InputDispatcher::KeyHook(int n_code,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    // Only send when the key is transitioning from pressed to released. Note
    // that the documentation for the bit state on KeyboardProc [1] can lead to
    // confusion. The relevant information is that the transition state (bit 31
    // -- zero-based) is always 1 for WM_KEYUP.
    // [1]
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644984.aspx
    //
    // While this documentation states that the previous key state (bit 30) is
    // always 1 on WM_KEYUP it has been observed to be 0 when the preceding
    // WM_KEYDOWN is intercepted (e.g., by an extension hooking a keyboard
    // shortcut).
    //
    // And to add to the confusion about bit 30, the documentation for WM_KEYUP
    // [2] and for general keyboard input [3] contradict each other, one saying
    // it's always set to 1, the other saying it's always set to 0 on
    // WM_KEYUP...
    // [2] https://docs.microsoft.com/en-us/windows/desktop/inputdev/wm-keyup
    // [3]
    // https://docs.microsoft.com/en-us/windows/desktop/inputdev/about-keyboard-input#keystroke-message-flags
    if (l_param & (1 << 31)) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                         current_dispatcher_->weak_factory_.GetWeakPtr(),
                         false));
    }
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

void InputDispatcher::DispatchedMessage(
    UINT message_id,
    const MOUSEHOOKSTRUCT* mouse_hook_struct) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (message_id == message_waiting_for_) {
    bool definitively_done = false;
    if (message_id == WM_MOUSEMOVE) {
      // Allow a slight offset, targets are never one pixel wide and pixel math
      // is imprecise (see SendMouseMoveImpl()).
      gfx::Point actual_location(mouse_hook_struct->pt);
      auto offset = expected_mouse_location_ - actual_location;
      definitively_done = std::abs(offset.x()) + std::abs(offset.y()) < 2;

      // Verify that the mouse ended up at the desired location.
      LOG_IF(ERROR, !definitively_done)
          << "Mouse moved to (" << mouse_hook_struct->pt.x << ", "
          << mouse_hook_struct->pt.y << ") rather than ("
          << expected_mouse_location_.x() << ", "
          << expected_mouse_location_.y()
          << "); check the math in SendMouseMoveImpl.";
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                       weak_factory_.GetWeakPtr(), definitively_done));
  } else if ((message_waiting_for_ == WM_LBUTTONDOWN &&
              message_id == WM_LBUTTONDBLCLK) ||
             (message_waiting_for_ == WM_MBUTTONDOWN &&
              message_id == WM_MBUTTONDBLCLK) ||
             (message_waiting_for_ == WM_RBUTTONDOWN &&
              message_id == WM_RBUTTONDBLCLK)) {
    LOG(WARNING) << "Double click event being treated as single-click. "
                 << "This may result in different event processing behavior. "
                 << "If you need a single click try moving the mouse between "
                 << "down events.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                                  weak_factory_.GetWeakPtr(), false));
  }
}

void InputDispatcher::MatchingMessageProcessed(bool definitively_done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (message_waiting_for_ == WM_KEYUP && --num_keyups_awaited_ > 0)
    return;

  // Unless specified otherwise by |definitively_done| : resume on the last
  // event of its type only (instead of the first one) to prevent flakes when
  // InputDispatcher is created while there are preexisting matching events
  // remaining in the queue. Emit a warning to help diagnose flakes should the
  // queue somehow never become empty of such events.
  if (HIWORD(::GetQueueStatus(system_queue_flag_)) && !definitively_done) {
    LOG(WARNING)
        << "Skipping message notification per remaining events in the queue "
           "(will try again shortly) : "
        << system_queue_flag_;

    // Check back on the next loop instead of relying on the remaining event
    // being caught by our hooks (all events don't seem to reliably make it
    // there).
    if (message_waiting_for_ == WM_KEYUP)
      ++num_keyups_awaited_;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                                  weak_factory_.GetWeakPtr(), false));
    return;
  }

  // Delete |this| before running the callback to allow callers to chain input
  // events.
  auto callback = std::move(callback_);
  delete this;
  std::move(callback).Run();
}

void InputDispatcher::OnTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << "Timed out waiting for mouse move event. The test will now "
                "continue, but may fail.";

  auto callback = std::move(callback_);
  delete this;
  std::move(callback).Run();
}

// Private functions ----------------------------------------------------------

UINT MapVirtualKeyToScanCode(UINT code) {
  UINT ret_code = MapVirtualKey(code, MAPVK_VK_TO_VSC);
  // We have to manually mark the following virtual
  // keys as extended or else their scancodes depend
  // on NumLock state.
  // For ex. VK_DOWN will be mapped onto either DOWN or NumPad2
  // depending on NumLock state which can lead to tests failures.
  switch (code) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_NEXT:
    case VK_PRIOR:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
      ret_code |= KF_EXTENDED;
      break;
    default:
      break;
  }
  return ret_code;
}

// Whether scan code should be used for |key|.
// When sending keyboard events by SendInput() function, Windows does not
// "smartly" add scan code if virtual key-code is used. So these key events
// won't have scan code or DOM UI Event code string.
// But we cannot blindly send all events with scan code. For some layout
// dependent keys, the Windows may not translate them to what they used to be,
// because the test cases are usually running in headless environment with
// default keyboard layout. So fall back to use virtual key code for these keys.
bool ShouldSendThroughScanCode(ui::KeyboardCode key) {
  const DWORD native_code = ui::WindowsKeyCodeForKeyboardCode(key);
  const DWORD scan_code = MapVirtualKeyToScanCode(native_code);
  return native_code == MapVirtualKey(scan_code, MAPVK_VSC_TO_VK);
}

// Append an INPUT structure with the appropriate keyboard event
// parameters required by SendInput
void AppendKeyboardInput(ui::KeyboardCode key,
                         bool key_up,
                         std::vector<INPUT>* input) {
  INPUT key_input = {};
  key_input.type = INPUT_KEYBOARD;
  key_input.ki.wVk = ui::WindowsKeyCodeForKeyboardCode(key);
  if (ShouldSendThroughScanCode(key)) {
    key_input.ki.wScan = MapVirtualKeyToScanCode(key_input.ki.wVk);
    // When KEYEVENTF_SCANCODE is used, ki.wVk is ignored, so we do not need to
    // clear it.
    key_input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if ((key_input.ki.wScan & 0xFF00) != 0)
      key_input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  if (key_up)
    key_input.ki.dwFlags |= KEYEVENTF_KEYUP;
  input->push_back(key_input);
}

// Append an INPUT structure with a simple mouse up or down event to be used
// by SendInput.
void AppendMouseInput(DWORD flags, std::vector<INPUT>* input) {
  INPUT mouse_input = {};
  mouse_input.type = INPUT_MOUSE;
  mouse_input.mi.dwFlags = flags;
  input->push_back(mouse_input);
}

// Append an INPUT array with optional accelerator keys that may be pressed
// with a keyboard or mouse event. This array will be sent by SendInput.
void AppendAcceleratorInputs(bool control,
                             bool shift,
                             bool alt,
                             bool key_up,
                             std::vector<INPUT>* input) {
  if (control)
    AppendKeyboardInput(ui::VKEY_CONTROL, key_up, input);
  if (alt)
    AppendKeyboardInput(ui::VKEY_LMENU, key_up, input);
  if (shift)
    AppendKeyboardInput(ui::VKEY_SHIFT, key_up, input);
}

}  // namespace

namespace ui_controls {
namespace internal {

bool SendKeyPressImpl(HWND window,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      base::OnceClosure task) {
  // SendInput only works as we expect it if one of our windows is the
  // foreground window already.
  HWND target_window = (::GetActiveWindow() &&
                        ::GetWindow(::GetActiveWindow(), GW_OWNER) == window) ?
                       ::GetActiveWindow() :
                       window;
  if (window && ::GetForegroundWindow() != target_window)
    return false;

  // If a pop-up menu is open, it won't receive events sent using SendInput.
  // Check for a pop-up menu using its window class (#32768) and if one
  // exists, send the key event directly there.
  HWND popup_menu = ::FindWindow(L"#32768", 0);
  if (popup_menu != NULL && popup_menu == ::GetTopWindow(NULL)) {
    WPARAM w_param = ui::WindowsKeyCodeForKeyboardCode(key);
    LPARAM l_param = 0;
    ::SendMessage(popup_menu, WM_KEYDOWN, w_param, l_param);
    ::SendMessage(popup_menu, WM_KEYUP, w_param, l_param);

    if (task)
      InputDispatcher::CreateForKeyUp(std::move(task), 1);
    return true;
  }

  std::vector<INPUT> input;
  AppendAcceleratorInputs(control, shift, alt, false, &input);
  AppendKeyboardInput(key, false, &input);

  AppendKeyboardInput(key, true, &input);
  AppendAcceleratorInputs(control, shift, alt, true, &input);

  if (input.size() > std::numeric_limits<UINT>::max())
    return false;

  if (::SendInput(static_cast<UINT>(input.size()), input.data(),
                  sizeof(INPUT)) != input.size()) {
    return false;
  }

  if (task)
    InputDispatcher::CreateForKeyUp(std::move(task), input.size() / 2);
  return true;
}

bool SendMouseMoveImpl(long screen_x, long screen_y, base::OnceClosure task) {
  gfx::Point screen_point =
      display::win::ScreenWin::DIPToScreenPoint({screen_x, screen_y});
  screen_x = screen_point.x();
  screen_y = screen_point.y();

  // Get the max screen coordinate for use in computing the normalized absolute
  // coordinates required by SendInput.
  const int max_x = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  const int max_y = ::GetSystemMetrics(SM_CYSCREEN) - 1;

  // Clamp the inputs.
  if (screen_x < 0)
    screen_x = 0;
  else if (screen_x > max_x)
    screen_x = max_x;
  if (screen_y < 0)
    screen_y = 0;
  else if (screen_y > max_y)
    screen_y = max_y;

  // Check if the mouse is already there.
  POINT current_pos;
  ::GetCursorPos(&current_pos);
  if (screen_x == current_pos.x && screen_y == current_pos.y) {
    if (task)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    return true;
  }

  // Form the input data containing the normalized absolute coordinates. As of
  // Windows 10 Fall Creators Update, moving to an absolute position of zero
  // does not work. It seems that moving to 1,1 does, though.
  INPUT input = {INPUT_MOUSE};
  input.mi.dx =
      static_cast<LONG>(std::max(1.0, std::ceil(screen_x * (65535.0 / max_x))));
  input.mi.dy =
      static_cast<LONG>(std::max(1.0, std::ceil(screen_y * (65535.0 / max_y))));
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

  if (!::SendInput(1, &input, sizeof(input)))
    return false;

  if (task)
    InputDispatcher::CreateForMouseMove(std::move(task), {screen_x, screen_y});
  return true;
}

bool SendMouseEventsImpl(MouseButton type,
                         int button_state,
                         base::OnceClosure task,
                         int accelerator_state) {
  DWORD down_flags = MOUSEEVENTF_ABSOLUTE;
  DWORD up_flags = MOUSEEVENTF_ABSOLUTE;
  UINT last_event;

  switch (type) {
    case LEFT:
      down_flags |= MOUSEEVENTF_LEFTDOWN;
      up_flags |= MOUSEEVENTF_LEFTUP;
      last_event = (button_state & UP) ? WM_LBUTTONUP : WM_LBUTTONDOWN;
      break;

    case MIDDLE:
      down_flags |= MOUSEEVENTF_MIDDLEDOWN;
      up_flags |= MOUSEEVENTF_MIDDLEUP;
      last_event = (button_state & UP) ? WM_MBUTTONUP : WM_MBUTTONDOWN;
      break;

    case RIGHT:
      down_flags |= MOUSEEVENTF_RIGHTDOWN;
      up_flags |= MOUSEEVENTF_RIGHTUP;
      last_event = (button_state & UP) ? WM_RBUTTONUP : WM_RBUTTONDOWN;
      break;

    default:
      NOTREACHED();
      return false;
  }

  std::vector<INPUT> input;
  if (button_state & DOWN) {
    AppendAcceleratorInputs(accelerator_state & kControl,
                            accelerator_state & kShift,
                            accelerator_state & kAlt, false, &input);
    AppendMouseInput(down_flags, &input);
  }

  if (button_state & UP) {
    AppendMouseInput(up_flags, &input);
    AppendAcceleratorInputs(accelerator_state & kControl,
                            accelerator_state & kShift,
                            accelerator_state & kAlt, true, &input);
  }

  if (input.size() > std::numeric_limits<UINT>::max())
    return false;

  if (::SendInput(static_cast<UINT>(input.size()), input.data(),
                  sizeof(INPUT)) != input.size()) {
    return false;
  }

  if (task)
    InputDispatcher::CreateForMouseEvent(std::move(task), last_event);
  return true;
}

bool SendTouchEventsImpl(int action, int num, int x, int y) {
  const int kTouchesLengthCap = 16;
  DCHECK_LE(num, kTouchesLengthCap);

  using InitializeTouchInjectionFn = BOOL(WINAPI*)(UINT32, DWORD);
  static const auto initialize_touch_injection =
      reinterpret_cast<InitializeTouchInjectionFn>(
          base::win::GetUser32FunctionPointer("InitializeTouchInjection"));
  if (!initialize_touch_injection ||
      !initialize_touch_injection(num, TOUCH_FEEDBACK_INDIRECT)) {
    return false;
  }

  using InjectTouchInputFn = BOOL(WINAPI*)(UINT32, POINTER_TOUCH_INFO*);
  static const auto inject_touch_input = reinterpret_cast<InjectTouchInputFn>(
      base::win::GetUser32FunctionPointer("InjectTouchInput"));
  if (!inject_touch_input)
    return false;

  POINTER_TOUCH_INFO pointer_touch_info[kTouchesLengthCap];
  for (int i = 0; i < num; i++) {
    POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
    memset(&contact, 0, sizeof(POINTER_TOUCH_INFO));
    contact.pointerInfo.pointerType = PT_TOUCH;
    contact.pointerInfo.pointerId = i;
    contact.pointerInfo.ptPixelLocation.y = y;
    contact.pointerInfo.ptPixelLocation.x = x + 10 * i;

    contact.touchFlags = TOUCH_FLAG_NONE;
    contact.touchMask =
        TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
    contact.orientation = 90;
    contact.pressure = 32000;

    // defining contact area
    contact.rcContact.top = contact.pointerInfo.ptPixelLocation.y - 2;
    contact.rcContact.bottom = contact.pointerInfo.ptPixelLocation.y + 2;
    contact.rcContact.left = contact.pointerInfo.ptPixelLocation.x - 2;
    contact.rcContact.right = contact.pointerInfo.ptPixelLocation.x + 2;

    contact.pointerInfo.pointerFlags =
        POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
  }
  // Injecting the touch down on screen
  if (!inject_touch_input(num, pointer_touch_info))
    return false;

  // Injecting the touch move on screen
  if (action & MOVE) {
    for (int i = 0; i < num; i++) {
      POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
      contact.pointerInfo.ptPixelLocation.y = y + 10;
      contact.pointerInfo.ptPixelLocation.x = x + 10 * i + 30;
      contact.pointerInfo.pointerFlags =
          POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
    }
    if (!inject_touch_input(num, pointer_touch_info))
      return false;
  }

  // Injecting the touch up on screen
  if (action & RELEASE) {
    for (int i = 0; i < num; i++) {
      POINTER_TOUCH_INFO& contact = pointer_touch_info[i];
      contact.pointerInfo.ptPixelLocation.y = y + 10;
      contact.pointerInfo.ptPixelLocation.x = x + 10 * i + 30;
      contact.pointerInfo.pointerFlags = POINTER_FLAG_UP | POINTER_FLAG_INRANGE;
    }
    if (!inject_touch_input(num, pointer_touch_info))
      return false;
  }

  return true;
}

}  // namespace internal
}  // namespace ui_controls
