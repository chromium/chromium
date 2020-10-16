// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker_win.h"

#include <dwmapi.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "ui/aura/window_tree_host.h"

namespace aura {

namespace {

// ~16 ms = time between frames when frame rate is 60 FPS.
const base::TimeDelta kUpdateOcclusionDelay =
    base::TimeDelta::FromMilliseconds(16);

// This global variable can be accessed only on main thread.
NativeWindowOcclusionTrackerWin* g_tracker = nullptr;

}  // namespace

NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator*
    NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::instance_ =
        nullptr;

NativeWindowOcclusionTrackerWin*
NativeWindowOcclusionTrackerWin::GetOrCreateInstance() {
  if (!g_tracker)
    g_tracker = new NativeWindowOcclusionTrackerWin();

  return g_tracker;
}

void NativeWindowOcclusionTrackerWin::DeleteInstanceForTesting() {
  delete g_tracker;
  g_tracker = nullptr;
}

void NativeWindowOcclusionTrackerWin::Enable(Window* window) {
  DCHECK(window->IsRootWindow());
  if (window->HasObserver(this)) {
    DCHECK(FALSE) << "window shouldn't already be observing occlusion tracker";
    return;
  }
  // Add this as an observer so that we can be notified
  // when it's no longer true that all windows are minimized, and when the
  // window is destroyed.
  HWND root_window_hwnd = window->GetHost()->GetAcceleratedWidget();
  window->AddObserver(this);
  // Remember this mapping from hwnd to Window*.
  hwnd_root_window_map_[root_window_hwnd] = window;
  // Notify the occlusion thread of the new HWND to track.
  update_occlusion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowOcclusionCalculator::EnableOcclusionTrackingForWindow,
          base::Unretained(WindowOcclusionCalculator::GetInstance()),
          root_window_hwnd));
}

void NativeWindowOcclusionTrackerWin::Disable(Window* window) {
  DCHECK(window->IsRootWindow());
  HWND root_window_hwnd = window->GetHost()->GetAcceleratedWidget();
  // Check that the root_window_hwnd doesn't get cleared before this is called.
  DCHECK(root_window_hwnd);
  hwnd_root_window_map_.erase(root_window_hwnd);
  window->RemoveObserver(this);
  update_occlusion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowOcclusionCalculator::DisableOcclusionTrackingForWindow,
          base::Unretained(WindowOcclusionCalculator::GetInstance()),
          root_window_hwnd));
}

void NativeWindowOcclusionTrackerWin::OnWindowVisibilityChanged(Window* window,
                                                                bool visible) {
  if (!window->IsRootWindow())
    return;
  window->GetHost()->SetNativeWindowOcclusionState(
      visible ? Window::OcclusionState::UNKNOWN
              : Window::OcclusionState::HIDDEN);
  update_occlusion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowOcclusionCalculator::HandleVisibilityChanged,
                     base::Unretained(WindowOcclusionCalculator::GetInstance()),
                     visible));
}

void NativeWindowOcclusionTrackerWin::OnWindowDestroying(Window* window) {
  Disable(window);
}

NativeWindowOcclusionTrackerWin::NativeWindowOcclusionTrackerWin()
    :  // Use a COMSTATaskRunner so that registering and unregistering
       // event hooks will happen on the same thread, as required by Windows,
       // and the task runner will have a message loop to call
       // EventHookCallback.
      update_occlusion_task_runner_(base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(),
           // This may be needed to determine that a window is no longer
           // occluded.
           base::TaskPriority::USER_VISIBLE,
           // Occlusion calculation doesn't need to happen on shutdown.
           // event hooks should also be cleaned up by Windows.
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      session_change_observer_(
          base::BindRepeating(&NativeWindowOcclusionTrackerWin::OnSessionChange,
                              base::Unretained(this))) {
  WindowOcclusionCalculator::CreateInstance(
      update_occlusion_task_runner_, base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(
          &NativeWindowOcclusionTrackerWin::UpdateOcclusionState,
          weak_factory_.GetWeakPtr()));
}

NativeWindowOcclusionTrackerWin::~NativeWindowOcclusionTrackerWin() {
  // This code is intended to be used in tests and shouldn't be reached in
  // production.

  // The occlusion tracker should be destroyed after all windows; window
  // destructors should call Disable() and thus remove them from the map, so by
  // the time we reach here the map should be empty.  (Proceeding with a
  // non-empty map would result in CheckedObserver failure since any remaining
  // windows still have the tracker as a registered observer.)
  DCHECK(hwnd_root_window_map_.empty())
      << "Occlusion tracker torn down while a Window still exists";

  // |occlusion_calculator_| must be deleted on its sequence because it needs
  // to unregister event hooks on COMSTA thread.  This blocks the main thread.
  base::WaitableEvent done_event;
  update_occlusion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowOcclusionCalculator::DeleteInstanceForTesting,
                     &done_event));
  done_event.Wait();
}

// static
bool NativeWindowOcclusionTrackerWin::IsWindowVisibleAndFullyOpaque(
    HWND hwnd,
    gfx::Rect* window_rect) {
  // Filter out windows that are not "visible", IsWindowVisible().
  if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
    return false;

  // Filter out minimized windows.
  if (IsIconic(hwnd))
    return false;

  LONG ex_styles = GetWindowLong(hwnd, GWL_EXSTYLE);
  // Filter out "transparent" windows, windows where the mouse clicks fall
  // through them.
  if (ex_styles & WS_EX_TRANSPARENT)
    return false;

  // Filter out "tool windows", which are floating windows that do not appear on
  // the taskbar or ALT-TAB. Floating windows can have larger window rectangles
  // than what is visible to the user, so by filtering them out we will avoid
  // incorrectly marking native windows as occluded.
  if (ex_styles & WS_EX_TOOLWINDOW)
    return false;

  // Filter out layered windows that are not opaque or that set a transparency
  // colorkey.
  if (ex_styles & WS_EX_LAYERED) {
    BYTE alpha;
    DWORD flags;

    // GetLayeredWindowAttributes only works if the application has
    // previously called SetLayeredWindowAttributes on the window.
    // The function will fail if the layered window was setup with
    // UpdateLayeredWindow. Treat this failure as the window being transparent.
    // See Remarks section of
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlayeredwindowattributes
    if (!GetLayeredWindowAttributes(hwnd, nullptr, &alpha, &flags))
      return false;

    if (flags & LWA_ALPHA && alpha < 255)
      return false;
    if (flags & LWA_COLORKEY)
      return false;
  }

  // Filter out windows that do not have a simple rectangular region.
  base::win::ScopedRegion region(CreateRectRgn(0, 0, 0, 0));
  if (GetWindowRgn(hwnd, region.get()) == COMPLEXREGION)
    return false;

  // Windows 10 has cloaked windows, windows with WS_VISIBLE attribute but
  // not displayed. explorer.exe, in particular has one that's the
  // size of the desktop. It's usually behind Chrome windows in the z-order,
  // but using a remote desktop can move it up in the z-order. So, ignore them.
  DWORD reason;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &reason,
                                      sizeof(reason))) &&
      reason != 0) {
    return false;
  }

  RECT win_rect;
  // Filter out windows that take up zero area. The call to GetWindowRect is one
  // of the most expensive parts of this function, so it is last.
  if (!GetWindowRect(hwnd, &win_rect))
    return false;
  if (IsRectEmpty(&win_rect))
    return false;
  *window_rect = gfx::Rect(win_rect);
  return true;
}

void NativeWindowOcclusionTrackerWin::UpdateOcclusionState(
    const base::flat_map<HWND, Window::OcclusionState>&
        root_window_hwnds_occlusion_state) {
  num_visible_root_windows_ = 0;
  for (const auto& root_window_pair : root_window_hwnds_occlusion_state) {
    auto it = hwnd_root_window_map_.find(root_window_pair.first);
    // The window was destroyed while processing occlusion.
    if (it == hwnd_root_window_map_.end())
      continue;
    // Check Window::IsVisible here, on the UI thread, because it can't be
    // checked on the occlusion calculation thread. Do this first before
    // checking screen_locked_ so that hidden windows remain hidden.
    if (!it->second->IsVisible()) {
      it->second->GetHost()->SetNativeWindowOcclusionState(
          Window::OcclusionState::HIDDEN);
      continue;
    }
    // If the screen is locked, ignore occlusion state results and
    // mark the window as occluded.
    it->second->GetHost()->SetNativeWindowOcclusionState(
        screen_locked_ ? Window::OcclusionState::OCCLUDED
                       : root_window_pair.second);
    num_visible_root_windows_++;
  }
}

void NativeWindowOcclusionTrackerWin::OnSessionChange(
    WPARAM status_code,
    const bool* is_current_session) {
  if (is_current_session && !*is_current_session)
    return;
  if (status_code == WTS_SESSION_UNLOCK) {
    // UNLOCK will cause a foreground window change, which will
    // trigger an occlusion calculation on its own.
    screen_locked_ = false;
  } else if (status_code == WTS_SESSION_LOCK && is_current_session) {
    screen_locked_ = true;
    // Set all visible root windows as occluded. If not visible,
    // set them as hidden.
    for (const auto& root_window_hwnd_pair : hwnd_root_window_map_) {
      root_window_hwnd_pair.second->GetHost()->SetNativeWindowOcclusionState(
          IsIconic(root_window_hwnd_pair.first)
              ? Window::OcclusionState::HIDDEN
              : Window::OcclusionState::OCCLUDED);
    }
  }
}

NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    WindowOcclusionCalculator(
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
        UpdateOcclusionStateCallback update_occlusion_state_callback)
    : task_runner_(task_runner),
      ui_thread_task_runner_(ui_thread_task_runner),
      update_occlusion_state_callback_(update_occlusion_state_callback) {
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    ::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr, CLSCTX_ALL,
                       IID_PPV_ARGS(&virtual_desktop_manager_));
  }
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ~WindowOcclusionCalculator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterEventHooks();
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::CreateInstance(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    UpdateOcclusionStateCallback update_occlusion_state_callback) {
  DCHECK(!instance_);
  instance_ = new WindowOcclusionCalculator(task_runner, ui_thread_task_runner,
                                            update_occlusion_state_callback);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    DeleteInstanceForTesting(base::WaitableEvent* done_event) {
  DCHECK(instance_);
  delete instance_;
  instance_ = nullptr;
  done_event->Signal();
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    EnableOcclusionTrackingForWindow(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  root_window_hwnds_occlusion_state_[hwnd] = Window::OcclusionState::UNKNOWN;
  if (global_event_hooks_.empty())
    RegisterEventHooks();

  // Schedule an occlusion calculation so that the newly tracked window does
  // not have a stale occlusion status.
  ScheduleOcclusionCalculationIfNeeded();
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    DisableOcclusionTrackingForWindow(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  root_window_hwnds_occlusion_state_.erase(hwnd);
  if (root_window_hwnds_occlusion_state_.empty()) {
    UnregisterEventHooks();
    if (occlusion_update_timer_.IsRunning())
      occlusion_update_timer_.Stop();
  }
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    HandleVisibilityChanged(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // May have gone from having no visible windows to having one, in
  // which case we need to register event hooks, and make sure that an
  // occlusion calculation is scheduled.
  if (visible) {
    MaybeRegisterEventHooks();
    ScheduleOcclusionCalculationIfNeeded();
  }
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    MaybeRegisterEventHooks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (global_event_hooks_.empty())
    RegisterEventHooks();
}

// static
void CALLBACK
NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::EventHookCallback(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime) {
  if (instance_)
    instance_->ProcessEventHookCallback(event, hwnd, idObject, idChild);
}

// static
BOOL CALLBACK NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ComputeNativeWindowOcclusionStatusCallback(HWND hwnd, LPARAM lParam) {
  if (instance_) {
    return instance_->ProcessComputeNativeWindowOcclusionStatusCallback(
        hwnd, reinterpret_cast<base::flat_set<DWORD>*>(lParam));
  }
  return FALSE;
}

// static
BOOL CALLBACK NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    UpdateVisibleWindowProcessIdsCallback(HWND hwnd, LPARAM lParam) {
  if (instance_) {
    instance_->ProcessUpdateVisibleWindowProcessIdsCallback(hwnd);
    return TRUE;
  }
  return FALSE;
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    UpdateVisibleWindowProcessIds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pids_for_location_change_hook_.clear();
  EnumWindows(&UpdateVisibleWindowProcessIdsCallback, 0);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ComputeNativeWindowOcclusionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (root_window_hwnds_occlusion_state_.empty())
    return;
  // Set up initial conditions for occlusion calculation.
  bool should_unregister_event_hooks = true;

  // Compute the SkRegion for the screen.
  int screen_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int screen_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  SkRegion screen_region = SkRegion(
      SkIRect::MakeLTRB(screen_left, screen_top,
                        screen_left + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                        screen_top + GetSystemMetrics(SM_CYVIRTUALSCREEN)));
  num_root_windows_with_unknown_occlusion_state_ = 0;

  for (auto& root_window_pair : root_window_hwnds_occlusion_state_) {
    HWND hwnd = root_window_pair.first;

    // IsIconic() checks for a minimized window. Immediately set the state of
    // minimized windows to HIDDEN.
    if (IsIconic(hwnd)) {
      root_window_pair.second = Window::OcclusionState::HIDDEN;
    } else if (IsWindowOnCurrentVirtualDesktop(hwnd) == false) {
      // If window is not on the current virtual desktop, immediately
      // set the state of the window to OCCLUDED.
      root_window_pair.second = Window::OcclusionState::OCCLUDED;
      // Don't unregister event hooks when not on current desktop. There's no
      // notification when that changes, so we can't reregister event hooks.
      should_unregister_event_hooks = false;
    } else {
      root_window_pair.second = Window::OcclusionState::UNKNOWN;
      should_unregister_event_hooks = false;
      num_root_windows_with_unknown_occlusion_state_++;
    }
  }
  // Unregister event hooks if all native windows are minimized.
  if (should_unregister_event_hooks) {
    UnregisterEventHooks();
  } else {
    base::flat_set<DWORD> current_pids_with_visible_windows;
    unoccluded_desktop_region_ = screen_region;
    // Calculate unoccluded region if there is a non-minimized native window.
    // Also compute |current_pids_with_visible_windows| as we enumerate
    // the windows.
    EnumWindows(&ComputeNativeWindowOcclusionStatusCallback,
                reinterpret_cast<LPARAM>(&current_pids_with_visible_windows));
    // Check if |pids_for_location_change_hook_| has any pids of processes
    // currently without visible windows. If so, unhook the win event,
    // remove the pid from |pids_for_location_change_hook_| and remove
    // the corresponding event hook from |process_event_hooks_|.
    base::flat_set<DWORD> pids_to_remove;
    for (auto loc_change_pid : pids_for_location_change_hook_) {
      if (current_pids_with_visible_windows.find(loc_change_pid) ==
          current_pids_with_visible_windows.end()) {
        // Remove the event hook from our map, and unregister the event hook.
        // It's possible the eventhook will no longer be valid, but if we don't
        // unregister the event hook, a process that toggles between having
        // visible windows and not having visible windows could cause duplicate
        // event hooks to get registered for the process.
        UnhookWinEvent(process_event_hooks_[loc_change_pid]);
        process_event_hooks_.erase(loc_change_pid);
        pids_to_remove.insert(loc_change_pid);
      }
    }
    if (!pids_to_remove.empty()) {
      // EraseIf is O(n) so erase pids not found in one fell swoop.
      base::EraseIf(pids_for_location_change_hook_,
                    [&pids_to_remove](DWORD pid) {
                      return pids_to_remove.find(pid) != pids_to_remove.end();
                    });
    }
  }
  // Post a task to the browser ui thread to update the window occlusion state
  // on the root windows.
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(update_occlusion_state_callback_,
                                root_window_hwnds_occlusion_state_));
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ScheduleOcclusionCalculationIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!occlusion_update_timer_.IsRunning())
    occlusion_update_timer_.Start(
        FROM_HERE, kUpdateOcclusionDelay, this,
        &WindowOcclusionCalculator::ComputeNativeWindowOcclusionStatus);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    RegisterGlobalEventHook(UINT event_min, UINT event_max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HWINEVENTHOOK event_hook =
      SetWinEventHook(event_min, event_max, nullptr, &EventHookCallback, 0, 0,
                      WINEVENT_OUTOFCONTEXT);

  global_event_hooks_.push_back(event_hook);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    RegisterEventHookForProcess(DWORD pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pids_for_location_change_hook_.insert(pid);
  process_event_hooks_[pid] = SetWinEventHook(
      EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
      &EventHookCallback, pid, 0, WINEVENT_OUTOFCONTEXT);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    RegisterEventHooks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(global_event_hooks_.empty());

  // Detects native window move (drag) and resizing events.
  RegisterGlobalEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND);

  // Detects native window minimize and restore from taskbar events.
  RegisterGlobalEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND);

  // Detects foreground window changing.
  RegisterGlobalEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND);

  // Detects object state changes, e.g., enable/disable state, native window
  // maximize and native window restore events.
  RegisterGlobalEventHook(EVENT_OBJECT_STATECHANGE, EVENT_OBJECT_STATECHANGE);

  // Cloaking and uncloaking of windows should trigger an occlusion calculation.
  // In particular, switching virtual desktops seems to generate these events.
  RegisterGlobalEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED);

  // Determine which subset of processes to set EVENT_OBJECT_LOCATIONCHANGE on
  // because otherwise event throughput is very high, as it generates events
  // for location changes of all objects, including the mouse moving on top of a
  // window.
  UpdateVisibleWindowProcessIds();
  for (DWORD pid : pids_for_location_change_hook_)
    RegisterEventHookForProcess(pid);
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    UnregisterEventHooks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  window_is_moving_ = false;
  for (HWINEVENTHOOK event_hook : global_event_hooks_)
    UnhookWinEvent(event_hook);
  global_event_hooks_.clear();

  for (DWORD pid : pids_for_location_change_hook_)
    UnhookWinEvent(process_event_hooks_[pid]);
  process_event_hooks_.clear();

  pids_for_location_change_hook_.clear();
}

bool NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ProcessComputeNativeWindowOcclusionStatusCallback(
        HWND hwnd,
        base::flat_set<DWORD>* current_pids_with_visible_windows) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SkRegion curr_unoccluded_destkop = unoccluded_desktop_region_;
  gfx::Rect window_rect;
  bool window_is_occluding =
      WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(hwnd, &window_rect);
  if (window_is_occluding) {
    // Hook this window's process with EVENT_OBJECT_LOCATION_CHANGE, if we are
    // not already doing so.
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    current_pids_with_visible_windows->insert(pid);
    if (!base::Contains(process_event_hooks_, pid))
      RegisterEventHookForProcess(pid);

    // If no more root windows to consider, return true so we can continue
    // looking for windows we haven't hooked.
    if (num_root_windows_with_unknown_occlusion_state_ == 0)
      return true;

    SkRegion window_region(SkIRect::MakeLTRB(window_rect.x(), window_rect.y(),
                                             window_rect.right(),
                                             window_rect.bottom()));
    unoccluded_desktop_region_.op(window_region, SkRegion::kDifference_Op);
  } else if (num_root_windows_with_unknown_occlusion_state_ == 0) {
    // This window can't occlude other windows, but we've determined the
    // occlusion state of all root windows, so we can return.
    return true;
  }

  // Check if |hwnd| is a root window; if so, we're done figuring out
  // if it's occluded because we've seen all the windows "over" it.
  auto it = root_window_hwnds_occlusion_state_.find(hwnd);
  if (it == root_window_hwnds_occlusion_state_.end() ||
      it->second != Window::OcclusionState::UNKNOWN) {
    return true;
  }

  // On Win7, default theme makes root windows have complex regions by
  // default. But we can still check if their bounding rect is occluded.
  if (!window_is_occluding) {
    RECT rect;
    if (::GetWindowRect(hwnd, &rect) != 0) {
      SkRegion window_region(
          SkIRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom));

      curr_unoccluded_destkop.op(window_region, SkRegion::kDifference_Op);
    }
  }
  it->second = (unoccluded_desktop_region_ == curr_unoccluded_destkop)
                   ? Window::OcclusionState::OCCLUDED
                   : Window::OcclusionState::VISIBLE;
  num_root_windows_with_unknown_occlusion_state_--;
  return true;
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ProcessEventHookCallback(DWORD event,
                             HWND hwnd,
                             LONG id_object,
                             LONG id_child) {
  // Can't do DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_) here. See
  // comment before call to PostTask below as to why.

  // No need to calculate occlusion if a zero HWND generated the event. This
  // happens if there is no window associated with the event, e.g., mouse move
  // events.
  if (!hwnd)
    return;

  // We only care about events for window objects. In particular, we don't care
  // about OBJID_CARET, which is spammy.
  if (id_object != OBJID_WINDOW)
    return;

  // Don't continually calculate occlusion while a window is moving, but rather
  // once at the beginning and once at the end.
  if (event == EVENT_SYSTEM_MOVESIZESTART) {
    window_is_moving_ = true;
  } else if (event == EVENT_SYSTEM_MOVESIZEEND) {
    window_is_moving_ = false;
  } else if (window_is_moving_) {
    if (event == EVENT_OBJECT_LOCATIONCHANGE ||
        event == EVENT_OBJECT_STATECHANGE) {
      return;
    }
    // If we get an event that isn't a location/state change, then we probably
    // missed the movesizeend notification, or got events out of order. In
    // that case, we want to go back to calculating occlusion.
    window_is_moving_ = false;
  }

  // ProcessEventHookCallback is called from the task_runner's PeekMessage
  // call, on the task runner's thread, but before the task_tracker thread sets
  // up the thread sequence. In order to prevent DCHECK failures with the
  // |occlusion_update_timer_, we need to call
  // ScheduleOcclusionCalculationIfNeeded from a task.
  // See WorkerThreadCOMDelegate::GetWorkFromWindowsMessageQueue().
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowOcclusionCalculator::ScheduleOcclusionCalculationIfNeeded,
          weak_factory_.GetWeakPtr()));
}

void NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    ProcessUpdateVisibleWindowProcessIdsCallback(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gfx::Rect window_rect;
  if (WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(hwnd, &window_rect)) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    pids_for_location_change_hook_.insert(pid);
  }
}

bool NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator::
    WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(
        HWND hwnd,
        gfx::Rect* window_rect) {
  return IsWindowVisibleAndFullyOpaque(hwnd, window_rect) &&
         (IsWindowOnCurrentVirtualDesktop(hwnd) == true);
}

base::Optional<bool> NativeWindowOcclusionTrackerWin::
    WindowOcclusionCalculator::IsWindowOnCurrentVirtualDesktop(HWND hwnd) {
  if (!virtual_desktop_manager_)
    return true;

  BOOL on_current_desktop;
  if (SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
          hwnd, &on_current_desktop))) {
    return on_current_desktop;
  }
  return base::nullopt;
}

}  // namespace aura
