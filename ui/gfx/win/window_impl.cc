// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/window_impl.h"

#include <list>

#include "base/at_exit.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/win/resource_exhaustion.h"
#include "base/win/win_util.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/gfx/win/crash_id_helper.h"
#include "ui/gfx/win/hwnd_util.h"

namespace gfx {

static const DWORD kWindowDefaultChildStyle =
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
static const DWORD kWindowDefaultStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

///////////////////////////////////////////////////////////////////////////////
// WindowImpl class tracking.

// Several external scripts rely explicitly on this base class name for
// acquiring the window handle and will break if this is modified!
// static
const wchar_t* const WindowImpl::kBaseClassName = L"Chrome_WidgetWin_";

// WindowImpl class information used for registering unique windows.
struct ClassInfo {
  ClassInfo(int style, HICON icon, HICON small_icon)
      : style(style), icon(icon), small_icon(small_icon) {}

  // Compares two ClassInfos. Returns true if all members match.
  bool Equals(const ClassInfo& other) const {
    return (other.style == style && other.icon == icon &&
            other.small_icon == small_icon);
  }

  UINT style;
  HICON icon;
  HICON small_icon;
};

// WARNING: this class may be used on multiple threads.
class ClassRegistrar {
 public:
  ClassRegistrar(const ClassRegistrar&) = delete;
  ClassRegistrar& operator=(const ClassRegistrar&) = delete;

  ~ClassRegistrar();

  static ClassRegistrar* GetInstance();

  void UnregisterClasses();

  // Returns the atom identifying the class matching |class_info|,
  // creating and registering a new class if the class is not yet known.
  ATOM RetrieveClassAtom(const ClassInfo& class_info);

 private:
  // Represents a registered window class.
  struct RegisteredClass {
    RegisteredClass(const ClassInfo& info,
                    const std::wstring& name,
                    ATOM atom,
                    HINSTANCE instance);

    // Info used to create the class.
    ClassInfo info;

    // The name given to the window class
    std::wstring name;

    // The atom identifying the window class.
    ATOM atom;

    // The handle of the module containing the window proceedure.
    HMODULE instance;
  };

  ClassRegistrar();
  friend struct base::DefaultSingletonTraits<ClassRegistrar>;

  typedef std::list<RegisteredClass> RegisteredClasses;
  RegisteredClasses registered_classes_;

  // Counter of how many classes have been registered so far.
  int registered_count_;

  base::Lock lock_;
};

ClassRegistrar::~ClassRegistrar() {}

// static
ClassRegistrar* ClassRegistrar::GetInstance() {
  return base::Singleton<ClassRegistrar,
                         base::LeakySingletonTraits<ClassRegistrar>>::get();
}

void ClassRegistrar::UnregisterClasses() {
  for (RegisteredClasses::iterator i = registered_classes_.begin();
        i != registered_classes_.end(); ++i) {
     if (UnregisterClass(MAKEINTATOM(i->atom), i->instance)) {
       registered_classes_.erase(i);
     } else {
       LOG(ERROR) << "Failed to unregister class " << i->name
                  << ". Error = " << GetLastError();
     }
   }
}

ATOM ClassRegistrar::RetrieveClassAtom(const ClassInfo& class_info) {
  base::AutoLock auto_lock(lock_);
  for (RegisteredClasses::const_iterator i = registered_classes_.begin();
       i != registered_classes_.end(); ++i) {
    if (class_info.Equals(i->info))
      return i->atom;
  }

  // No class found, need to register one.
  std::wstring name = std::wstring(WindowImpl::kBaseClassName) +
                      base::NumberToWString(registered_count_++);
  // We're not supposed to have many window classes, so if registered_count_
  // gets above a certain small threshold, we may as well have a resource leak
  // caused by repeatedly registering a class with auto-generated name, which
  // would eventually lead to user atom table exhaustion.
  // TODO(crbug.com/40925772): remove when source of ATOM leak is found.
  if (registered_count_ == 128) {
    base::debug::DumpWithoutCrashing();
  }

  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      name.c_str(), &base::win::WrappedWindowProc<WindowImpl::WndProc>,
      class_info.style, 0, 0, NULL,
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), NULL,
      class_info.icon, class_info.small_icon, &window_class);
  HMODULE instance = window_class.hInstance;
  ATOM atom = RegisterClassEx(&window_class);
  if (!atom) {
    // Perhaps the Window session has run out of atoms; see
    // https://crbug.com/653493.
    base::win::OnResourceExhausted();
  }

  registered_classes_.push_back(RegisteredClass(
      class_info, name, atom, instance));

  return atom;
}

ClassRegistrar::RegisteredClass::RegisteredClass(const ClassInfo& info,
                                                 const std::wstring& name,
                                                 ATOM atom,
                                                 HMODULE instance)
    : info(info), name(name), atom(atom), instance(instance) {}

ClassRegistrar::ClassRegistrar() : registered_count_(0) {}


///////////////////////////////////////////////////////////////////////////////
// WindowImpl, public

WindowImpl::WindowImpl(const std::string& debugging_id)
    : debugging_id_(debugging_id), class_style_(CS_DBLCLKS) {}

WindowImpl::~WindowImpl() {
  ClearUserData();
}

// static
void WindowImpl::UnregisterClassesAtExit() {
  base::AtExitManager::RegisterTask(
      base::BindOnce(&ClassRegistrar::UnregisterClasses,
                     base::Unretained(ClassRegistrar::GetInstance())));
}

void WindowImpl::Init(HWND parent, const Rect& bounds) {
  if (window_style_ == 0)
    window_style_ = parent ? kWindowDefaultChildStyle : kWindowDefaultStyle;

  if (parent == HWND_DESKTOP) {
    // Only non-child windows can have HWND_DESKTOP (0) as their parent.
    CHECK((window_style_ & WS_CHILD) == 0);
    parent = GetWindowToParentTo(false);
  } else if (parent == ::GetDesktopWindow()) {
    // Any type of window can have the "Desktop Window" as their parent.
    parent = GetWindowToParentTo(true);
  } else if (parent != HWND_MESSAGE) {
    CHECK(::IsWindow(parent));
  }

  int x, y, width, height;
  if (bounds.IsEmpty()) {
    x = y = width = height = CW_USEDEFAULT;
  } else {
    x = bounds.x();
    y = bounds.y();
    width = bounds.width();
    height = bounds.height();
  }

  ATOM atom = GetWindowClassAtom();
  auto weak_this = weak_factory_.GetWeakPtr();
  HWND hwnd = CreateWindowEx(window_ex_style_, reinterpret_cast<wchar_t*>(atom),
                             NULL, window_style_, x, y, width, height, parent,
                             NULL, NULL, this);
  const DWORD create_window_error = ::GetLastError();

  // First nccalcszie (during CreateWindow) for captioned windows is
  // deliberately ignored so force a second one here to get the right
  // non-client set up.
  if (hwnd && (window_style_ & WS_CAPTION)) {
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
  }

  if (!hwnd_ && create_window_error == 0) {
    bool still_alive = !!weak_this;
    base::debug::Alias(&still_alive);
    base::debug::Alias(&hwnd);
    base::debug::Alias(&atom);
    bool got_create = got_create_;
    base::debug::Alias(&got_create);
    bool got_valid_hwnd = got_valid_hwnd_;
    base::debug::Alias(&got_valid_hwnd);
    WNDCLASSEX class_info;
    memset(&class_info, 0, sizeof(WNDCLASSEX));
    class_info.cbSize = sizeof(WNDCLASSEX);
    BOOL got_class = GetClassInfoEx(GetModuleHandle(NULL),
                                    reinterpret_cast<wchar_t*>(atom),
                                    &class_info);
    base::debug::Alias(&got_class);
    bool procs_match = got_class && class_info.lpfnWndProc ==
        base::win::WrappedWindowProc<&WindowImpl::WndProc>;
    base::debug::Alias(&procs_match);
    CHECK(false);
  }

  CheckWindowCreated(hwnd_, create_window_error);

  // The window procedure should have set the data for us.
  CHECK_EQ(this, GetWindowUserData(hwnd));
}

HICON WindowImpl::GetDefaultWindowIcon() const {
  return nullptr;
}

HICON WindowImpl::GetSmallWindowIcon() const {
  return nullptr;
}

LRESULT WindowImpl::OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;

  HWND hwnd = hwnd_;
  if (message == WM_NCDESTROY)
    hwnd_ = nullptr;

  // Handle the message if it's in our message map; otherwise, let the system
  // handle it.
  if (!ProcessWindowMessage(hwnd, message, w_param, l_param, result))
    result = DefWindowProc(hwnd, message, w_param, l_param);

  return result;
}

void WindowImpl::ClearUserData() {
  if (::IsWindow(hwnd_))
    gfx::SetWindowUserData(hwnd_, nullptr);
}

// static
LRESULT CALLBACK WindowImpl::WndProc(HWND hwnd,
                                     UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  WindowImpl* window = nullptr;
  if (message == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l_param);
    window = reinterpret_cast<WindowImpl*>(cs->lpCreateParams);
    DCHECK(window);
    gfx::SetWindowUserData(hwnd, window);
    window->hwnd_ = hwnd;
    window->got_create_ = true;
    if (hwnd)
      window->got_valid_hwnd_ = true;
  } else {
    window = reinterpret_cast<WindowImpl*>(GetWindowUserData(hwnd));
  }

  if (!window)
    return 0;

  auto logger =
      CrashIdHelper::Get()->OnWillProcessMessages(window->debugging_id_);
  return window->OnWndProc(message, w_param, l_param);
}

ATOM WindowImpl::GetWindowClassAtom() {
  HICON icon = GetDefaultWindowIcon();
  HICON small_icon = GetSmallWindowIcon();
  ClassInfo class_info(initial_class_style(), icon, small_icon);
  return ClassRegistrar::GetInstance()->RetrieveClassAtom(class_info);
}

}  // namespace gfx
