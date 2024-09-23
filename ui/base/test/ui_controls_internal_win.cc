// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/test/ui_controls_internal_win.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "base/win/win_util.h"
#include "ui/base/win/event_creation_utils.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"

namespace {

bool IsKeyEvent(WPARAM message_type) {
  return message_type == WM_KEYDOWN || message_type == WM_KEYUP;
}

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

  // Constructs an InputDispatcher that will invoke `callback` after
  // `num_key_events_awaited` events of type `wait_for` have been received.
  static void CreateForKeyEvent(base::OnceClosure callback,
                                ui_controls::KeyEventType wait_for,
                                int num_key_events_awaited);

  // Special case of CreateForMessage() for WM_MOUSEMOVE. Upon receipt, an error
  // message is logged if the destination of the move is not |screen_point|.
  // |callback| is run regardless after a sufficiently long delay. This
  // generally happens when another process has a window over the test's window,
  // or if |screen_point| is not over a window owned by the test.
  static void CreateForMouseMove(base::OnceClosure callback,
                                 const gfx::Point& screen_point);

  InputDispatcher(const InputDispatcher&) = delete;
  InputDispatcher& operator=(const InputDispatcher&) = delete;

 private:
  // Generic message
  InputDispatcher(base::OnceClosure callback,
                  WPARAM message_waiting_for,
                  UINT system_queue_flag);

  // WM_KEYDOWN or WM_KEYUP
  InputDispatcher(base::OnceClosure callback,
                  WPARAM message_waiting_for,
                  UINT system_queue_flag,
                  int num_key_events_awaited);

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

  // The message on which the instance is waiting.
  const WPARAM message_waiting_for_;

  // The system queue flag (ref. ::GetQueueStatus) which the awaited event is
  // reflected in.
  const UINT system_queue_flag_;

  // The number of messages to receive before dispatching `callback_`. Only
  // relevant when `message_waiting_for_` is WM_KEYDOWN or WM_KEYUP.
  int num_key_events_awaited_ = 0;

  // The desired mouse position for a mouse move event.
  const gfx::Point expected_mouse_location_;

  // Whether all desired messages were observed, but MatchingMessageProcessed()
  // is flushing remaining messages of type `system_queue_flag_`.
  bool flushing_messages_ = false;

  base::WeakPtrFactory<InputDispatcher> weak_factory_{this};
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
void InputDispatcher::CreateForKeyEvent(base::OnceClosure callback,
                                        ui_controls::KeyEventType wait_for,
                                        int num_key_events_awaited) {
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);
  // Owns self.
  new InputDispatcher(
      std::move(callback),
      wait_for == ui_controls::KeyEventType::kKeyPress ? WM_KEYDOWN : WM_KEYUP,
      QS_KEY, num_key_events_awaited);
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
                                 int num_key_events_awaited)
    : callback_(std::move(callback)),
      message_waiting_for_(message_waiting_for),
      system_queue_flag_(system_queue_flag),
      num_key_events_awaited_(num_key_events_awaited) {
  CHECK(IsKeyEvent(message_waiting_for_));
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
  CHECK_EQ(message_waiting_for_, static_cast<WPARAM>(WM_MOUSEMOVE));
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
  if (IsKeyEvent(message_waiting_for_)) {
    hook_type = WH_KEYBOARD;
    hook_function = &KeyHook;
  } else {
    // WH_CALLWNDPROCRET does not generate mouse messages for some reason.
    hook_type = WH_MOUSE;
    hook_function = &MouseHook;
    if (message_waiting_for_ == WM_MOUSEMOVE) {
      // Things don't go well with move events sometimes. Bail out if it takes
      // too long.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
  if (n_code == HC_ACTION) {
    const WPARAM type = (HIWORD(l_param) & KF_UP) ? WM_KEYUP : WM_KEYDOWN;
    CHECK(current_dispatcher_);
    if (type == current_dispatcher_->message_waiting_for_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                         current_dispatcher_->weak_factory_.GetWeakPtr(),
                         false));
    }
  }
  return CallNextHookEx(next_hook_, n_code, w_param, l_param);
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InputDispatcher::MatchingMessageProcessed,
                                  weak_factory_.GetWeakPtr(), false));
  }
}

void InputDispatcher::MatchingMessageProcessed(bool definitively_done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Guard against re-entrancy.
  if (flushing_messages_)
    return;

  if (IsKeyEvent(message_waiting_for_)) {
    --num_key_events_awaited_;
    if (num_key_events_awaited_ != 0) {
      return;
    }
  }

  // Unless specified otherwise by |definitively_done| : resume on the last
  // event of its type only (instead of the first one) to prevent flakes when
  // InputDispatcher is created while there are preexisting matching events
  // remaining in the queue. Emit a warning to help diagnose flakes should the
  // queue somehow never become empty of such events.
  if (!definitively_done) {
    while (HIWORD(::GetQueueStatus(system_queue_flag_))) {
      LOG(WARNING) << "Got all expected messages, but the queue still contains "
                      "messages of type "
                   << system_queue_flag_
                   << ". Pumping messages until it's no longer the case.";

      // RunLoop::Run() calls MessagePumpForUI::ProcessNextWindowsMessage(),
      // which should remove at least one message from the queue.
      flushing_messages_ = true;
      auto weak_ptr = weak_factory_.GetWeakPtr();
      base::RunLoop().RunUntilIdle();
      DCHECK(weak_ptr);
    }
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
void AppendAcceleratorInputs(int accelerator_state,
                             bool key_up,
                             std::vector<INPUT>* input) {
  if (accelerator_state & ui_controls::kControl) {
    AppendKeyboardInput(ui::VKEY_CONTROL, key_up, input);
  }
  if (accelerator_state & ui_controls::kAlt) {
    AppendKeyboardInput(ui::VKEY_LMENU, key_up, input);
  }
  if (accelerator_state & ui_controls::kShift) {
    AppendKeyboardInput(ui::VKEY_SHIFT, key_up, input);
  }
}

}  // namespace

namespace ui_controls {
namespace internal {

bool SendKeyPressReleaseImpl(HWND window,
                             ui::KeyboardCode key,
                             int accelerator_state,
                             KeyEventType wait_for,
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
      InputDispatcher::CreateForKeyEvent(std::move(task), wait_for, 1);
    return true;
  }

  std::vector<INPUT> input;
  AppendAcceleratorInputs(accelerator_state, false, &input);
  AppendKeyboardInput(key, false, &input);

  AppendKeyboardInput(key, true, &input);
  AppendAcceleratorInputs(accelerator_state, true, &input);

  if (input.size() > std::numeric_limits<UINT>::max())
    return false;

  if (::SendInput(static_cast<UINT>(input.size()), input.data(),
                  sizeof(INPUT)) != input.size()) {
    return false;
  }

  if (task)
    InputDispatcher::CreateForKeyEvent(std::move(task), wait_for,
                                       input.size() / 2);
  return true;
}

bool SendMouseMoveImpl(int screen_x, int screen_y, base::OnceClosure task) {
  gfx::Point screen_point =
      display::win::ScreenWin::DIPToScreenPoint({screen_x, screen_y});

  // Check if the mouse is already there.
  POINT current_pos;
  ::GetCursorPos(&current_pos);
  if (screen_point.x() == current_pos.x && screen_point.y() == current_pos.y) {
    if (task)
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(task));
    return true;
  }

  if (!ui::SendMouseEvent(screen_point,
                          MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE)) {
    return false;
  }

  if (task)
    InputDispatcher::CreateForMouseMove(std::move(task),
                                        {screen_point.x(), screen_point.y()});
  return true;
}

bool SendMouseEventsImpl(MouseButton type,
                         int button_state,
                         base::OnceClosure task,
                         int accelerator_state) {
  DWORD down_flags = 0;
  DWORD up_flags = 0;
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
  }

  std::vector<INPUT> input;
  if (button_state & DOWN) {
    AppendAcceleratorInputs(accelerator_state, false, &input);
    AppendMouseInput(down_flags, &input);
  }

  if (button_state & UP) {
    AppendMouseInput(up_flags, &input);
    AppendAcceleratorInputs(accelerator_state, true, &input);
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
  if (action & kTouchMove) {
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
  if (action & kTouchRelease) {
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
