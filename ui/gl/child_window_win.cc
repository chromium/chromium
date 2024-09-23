// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/child_window_win.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gfx/win/window_impl.h"

namespace gl {

namespace {

ATOM g_window_class;

// This runs on the window owner thread.
void InitializeWindowClass() {
  if (g_window_class)
    return;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"Intermediate D3D Window",
      &base::win::WrappedWindowProc<::DefWindowProc>, CS_OWNDC, 0, 0, nullptr,
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr,
      nullptr, &intermediate_class);
  g_window_class = RegisterClassEx(&intermediate_class);
  if (!g_window_class) {
    LOG(ERROR) << "RegisterClass failed.";
    return;
  }
}

// Hidden popup window  used as a parent for the child surface window.
// Must be created and destroyed on the thread.
class HiddenPopupWindow : public gfx::WindowImpl {
 public:
  static HWND Create() {
    gfx::WindowImpl* window = new HiddenPopupWindow;

    window->set_window_style(WS_POPUP);
    window->set_window_ex_style(WS_EX_TOOLWINDOW);
    window->Init(GetDesktopWindow(), gfx::Rect());
    EnableWindow(window->hwnd(), FALSE);
    // The |window| instance is now owned by the window user data.
    DCHECK_EQ(window, gfx::GetWindowUserData(window->hwnd()));
    return window->hwnd();
  }

  static void Destroy(HWND window) {
    // This uses the fact that the window user data contains a pointer
    // to gfx::WindowImpl instance.
    gfx::WindowImpl* window_data =
        reinterpret_cast<gfx::WindowImpl*>(gfx::GetWindowUserData(window));
    DCHECK_EQ(window, window_data->hwnd());
    DestroyWindow(window);
    delete window_data;
  }

 private:
  // Explicitly do nothing in Close. We do this as some external apps may get a
  // handle to this window and attempt to close it.
  void OnClose() {}

  CR_BEGIN_MSG_MAP_EX(HiddenPopupWindow)
    CR_MSG_WM_CLOSE(OnClose)
  CR_END_MSG_MAP()

  CR_MSG_MAP_CLASS_DECLARATIONS(HiddenPopupWindow)
};

// This runs on the window owner thread.
void CreateWindowsOnThread(base::WaitableEvent* event,
                           HWND* child_window,
                           HWND* parent_window) {
  InitializeWindowClass();
  DCHECK(g_window_class);

  // Create hidden parent window on the current thread.
  *parent_window = HiddenPopupWindow::Create();
  // Create child window.
  // WS_EX_NOPARENTNOTIFY and WS_EX_LAYERED make the window transparent for
  // input. WS_EX_NOREDIRECTIONBITMAP avoids allocating a
  // bitmap that would otherwise be allocated with WS_EX_LAYERED, the bitmap is
  // only necessary if using Gdi objects with the window.
  // Using a size of 1x1 is fine because the window will be subsequently resized
  // using SetWindowPos whenever the parent window size changes.
  const HWND window = CreateWindowEx(
      WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
          WS_EX_NOREDIRECTIONBITMAP,
      reinterpret_cast<wchar_t*>(g_window_class), L"",
      WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE, 0, 0, /*width*/ 1,
      /*height*/ 1, *parent_window, nullptr, nullptr, nullptr);
  if (!window) {
    logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
    base::debug::Alias(&error);
    CHECK(false);
  }
  *child_window = window;
  event->Signal();
}

// This runs on the window owner thread.
void DestroyWindowsOnThread(HWND child_window, HWND hidden_popup_window) {
  DestroyWindow(child_window);
  HiddenPopupWindow::Destroy(hidden_popup_window);
}

#if DCHECK_IS_ON()
base::ThreadChecker& GetThreadChecker() {
  static base::ThreadChecker thread_checker;
  return thread_checker;
}
#endif

}  // namespace

class ChildWindowWin::ChildWindowThread
    : public base::RefCounted<ChildWindowThread> {
 public:
  // Returns the singleton instance of the thread.
  static scoped_refptr<ChildWindowThread> GetInstance() {
    DCHECK_CALLED_ON_VALID_THREAD(GetThreadChecker());
    static base::WeakPtr<ChildWindowThread> weak_instance;

    auto instance = base::WrapRefCounted(weak_instance.get());
    if (!instance) {
      instance = base::WrapRefCounted(new ChildWindowThread);
      weak_instance = instance->weak_ptr_factory_.GetWeakPtr();
    }

    return instance;
  }

  scoped_refptr<base::TaskRunner> task_runner() {
    DCHECK_CALLED_ON_VALID_THREAD(GetThreadChecker());
    return thread_.task_runner();
  }

 private:
  friend class base::RefCounted<ChildWindowThread>;

  ChildWindowThread() : thread_("Window owner thread") {
    DCHECK_CALLED_ON_VALID_THREAD(GetThreadChecker());
    base::Thread::Options options(base::MessagePumpType::UI, 0);
    thread_.StartWithOptions(std::move(options));
  }

  ~ChildWindowThread() {
    DCHECK_CALLED_ON_VALID_THREAD(GetThreadChecker());
    thread_.Stop();
  }

  base::Thread thread_;
  base::WeakPtrFactory<ChildWindowThread> weak_ptr_factory_{this};
};

ChildWindowWin::ChildWindowWin() = default;

void ChildWindowWin::Initialize() {
  if (window_)
    return;

  thread_ = ChildWindowThread::GetInstance();

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CreateWindowsOnThread, &event, &window_,
                                &initial_parent_window_));
  event.Wait();
}

ChildWindowWin::~ChildWindowWin() {
  if (thread_) {
    scoped_refptr<base::TaskRunner> task_runner = thread_->task_runner();
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&DestroyWindowsOnThread, window_,
                       initial_parent_window_),
        base::DoNothingWithBoundArgs(std::move(thread_)));
  }
}

void ChildWindowWin::Resize(const gfx::Size& size) {
  // Force a resize and redraw (but not a move, activate, etc.).
  constexpr UINT kFlags = SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE |
                          SWP_NOOWNERZORDER | SWP_NOREDRAW |
                          SWP_NOSENDCHANGING | SWP_NOZORDER;
  // When the browser process destroys its window, Windows will destroy
  // all of its child windows, including our window. This leads to a race
  // condition where SetWindowPos may return false if our window has been
  // destroyed before we finish processing Reshape requests for the
  // window.
  // Returning a failure from ChildWindowWin::Resize will cause the
  // outer Skia output device code to flag CONTEXT_LOST_RESHAPE_FAILED and
  // terminate the GPU process. Instead of handling failures from SetWindowPos,
  // we ignore its return value. The outer code will eventually be told of the
  // window's demise.
  if (!::SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                      kFlags)) {
    DPLOG(WARNING) << "::SetWindowPos failed";
  }
}

scoped_refptr<base::TaskRunner> ChildWindowWin::GetTaskRunnerForTesting() {
  DCHECK(thread_);
  return thread_->task_runner();
}

}  // namespace gl
