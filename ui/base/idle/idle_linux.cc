// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "ui/base/idle/idle.h"

#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/base/idle/idle_internal.h"

#if defined(USE_X11)
#include "ui/base/x/x11_idle_query.h"
#include "ui/base/x/x11_screensaver.h"
#else
#include "base/notreached.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#endif

namespace ui {

namespace {

const char kMethodName[] = "GetActive";
const char kSignalName[] = "ActiveChanged";

struct {
  const char* service_name;
  const char* object_path;
  const char* interface;
} constexpr kInterfaces[] = {
    // ksmserver, light-locker, etc.
    {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
     "org.freedesktop.ScreenSaver"},
    // cinnamon-screensaver
    {"org.cinnamon.ScreenSaver", "/org/cinnamon/ScreenSaver",
     "org.cinnamon.ScreenSaver"},
    // gnome-screensaver
    {"org.gnome.ScreenSaver", "/", "org.gnome.ScreenSaver"},
    // mate-screensaver
    {"org.mate.ScreenSaver", "/", "org.mate.ScreenSaver"},
    // xfce4-screensaver
    {"org.xfce.ScreenSaver", "/", "org.xfce.ScreenSaver"},
};

bool ServiceNameHasOwner(dbus::Bus* bus, const char* service_name) {
  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall name_has_owner_call(DBUS_INTERFACE_DBUS, "NameHasOwner");
  dbus::MessageWriter writer(&name_has_owner_call);
  writer.AppendString(service_name);
  std::unique_ptr<dbus::Response> name_has_owner_response =
      dbus_proxy->CallMethodAndBlock(&name_has_owner_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  dbus::MessageReader reader(name_has_owner_response.get());
  bool owned = false;
  return name_has_owner_response && reader.PopBool(&owned) && owned;
}

// This class checks for availability of various DBus screensaver interfaces
// and listens for screensaver events on the first one found.
class IdleLinuxImpl {
 public:
  enum class LockState {
    kUnknown,
    kLocked,
    kUnlocked,
  };

  IdleLinuxImpl()
      : task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits(
                base::MayBlock(),
                base::TaskPriority::USER_VISIBLE,
                base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN))) {}

  void Init() {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&IdleLinuxImpl::InitOnTaskRunner,
                                          base::Unretained(this)));
  }

  LockState lock_state() const { return lock_state_; }

 private:
  ~IdleLinuxImpl() = default;

  void InitOnTaskRunner() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = task_runner_;
    bus_ = base::MakeRefCounted<dbus::Bus>(options);

    for (const auto& interface : kInterfaces) {
      // Calling methods on a non-existent service will lead to a timeout rather
      // than an immediate error, so check for service existence first.
      if (!ServiceNameHasOwner(bus_.get(), interface.service_name))
        continue;

      // To avoid a race condition, start listenting for state changes before
      // querying the state.
      auto* proxy = bus_->GetObjectProxy(
          interface.service_name, dbus::ObjectPath(interface.object_path));
      if (!proxy->ConnectToSignalAndBlock(
              interface.interface, kSignalName,
              base::BindRepeating(&IdleLinuxImpl::OnActiveChanged,
                                  base::Unretained(this)))) {
        continue;
      }

      // Some service owners (eg. gsd-screensaver-proxy) advertise the correct
      // methods on org.freedesktop.ScreenSaver, but calling them will result in
      // a NotImplemented DBus error.  To ensure the service owner will send
      // state change events, make an explicit method call and check that no
      // error is returned.
      dbus::MethodCall method_call(interface.interface, kMethodName);
      auto response = proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
      if (response && UpdateLockState(response.get()))
        return;

      // Try the next interface.
      proxy->Detach();
    }
  }

  void OnActiveChanged(dbus::Signal* signal) { UpdateLockState(signal); }

  bool UpdateLockState(dbus::Message* message) {
    dbus::MessageReader reader(message);
    bool active;
    if (!reader.PopBool(&active) || reader.HasMoreData())
      return false;
    lock_state_ = active ? LockState::kLocked : LockState::kUnlocked;
    return true;
  }

  // Only accessed on the task runner.
  scoped_refptr<dbus::Bus> bus_;

  // Only accessed on the main thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Read on the main thread and written on the task runner.
  std::atomic<LockState> lock_state_{LockState::kUnknown};
};

IdleLinuxImpl* GetIdleLinuxImpl() {
  static base::NoDestructor<IdleLinuxImpl> impl;
  return impl.get();
}

}  // namespace

int CalculateIdleTime() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    auto* const screen = display::Screen::GetScreen();
    // The screen can be nullptr in tests.
    if (!screen)
      return 0;
    return screen->CalculateIdleTime().InSeconds();
  }
#endif
#if defined(USE_X11)
  IdleQueryX11 idle_query;
  return idle_query.IdleTime();
#else
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
#endif
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  auto lock_state = GetIdleLinuxImpl()->lock_state();
  if (lock_state != IdleLinuxImpl::LockState::kUnknown)
    return lock_state == IdleLinuxImpl::LockState::kLocked;

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    auto* const screen = display::Screen::GetScreen();
    // The screen can be nullptr in tests.
    if (!screen)
      return false;
    return screen->IsScreenSaverActive();
  }
#endif
#if defined(USE_X11)
  // Usually the screensaver is used to lock the screen.
  return IsXScreensaverActive();
#else
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
#endif
}

}  // namespace ui
