// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/display/screen.h"

#if defined(USE_DBUS)
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#endif

namespace ui {

#if defined(USE_DBUS)

namespace {

const char kMethodName[] = "GetActive";
const char kSignalName[] = "ActiveChanged";

// Various names under which the service may be found in different Linux desktop
// environments.
struct {
  const char* service_name;
  const char* object_path;
  const char* interface;
} constexpr kServices[] = {
    // ksmserver, light-locker, etc.
    {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
     "org.freedesktop.ScreenSaver"},
    // cinnamon-screensaver
    {"org.cinnamon.ScreenSaver", "/org/cinnamon/ScreenSaver",
     "org.cinnamon.ScreenSaver"},
    // gnome-screensaver
    {"org.gnome.ScreenSaver", "/org/gnome/ScreenSaver",
     "org.gnome.ScreenSaver"},
    // mate-screensaver
    {"org.mate.ScreenSaver", "/org/mate/ScreenSaver", "org.mate.ScreenSaver"},
    // xfce4-screensaver
    {"org.xfce.ScreenSaver", "/org/xfce/ScreenSaver", "org.xfce.ScreenSaver"},
};

constexpr size_t kServiceCount = sizeof(kServices) / sizeof(kServices[0]);

// This class tries to find the name under which the ScreenSaver D-Bus service
// is registered, and if found the one, connects to the service and subscribes
// to its notifications.
class DBusScreenSaverWatcher {
 public:
  enum class LockState {
    kUnknown,
    kLocked,
    kUnlocked,
  };

  DBusScreenSaverWatcher()
      : task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits(
                base::MayBlock(),
                base::TaskPriority::USER_VISIBLE,
                base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN))) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = task_runner_;
    bus_ = base::MakeRefCounted<dbus::Bus>(options);

    TryCurrentService();
  }

  LockState lock_state() const { return lock_state_; }

 private:
  ~DBusScreenSaverWatcher() = default;

  // Starts the initialisation sequence for the current service.  Failure at any
  // step will increment the service counter and re-start the process.
  void TryCurrentService() {
    // Detach the proxy, if we have one from the previous attempt.
    if (proxy_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&dbus::ObjectProxy::Detach, base::Unretained(proxy_)));
      proxy_ = nullptr;
    }

    if (current_service_ >= kServiceCount) {
      if (current_service_ == kServiceCount) {
        // Log the warning once.
        LOG(WARNING)
            << "None of the known D-Bus ScreenSaver services could be used.";
        ++current_service_;
      }
      return;
    }

    // Calling methods on a non-existent service will lead to a timeout rather
    // than an immediate error, so check for service existence first.
    dbus::ObjectProxy* dbus_proxy = bus_->GetObjectProxy(
        DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    dbus::MethodCall name_has_owner_call(DBUS_INTERFACE_DBUS, "NameHasOwner");
    dbus::MessageWriter writer(&name_has_owner_call);
    writer.AppendString(kServices[current_service_].service_name);
    dbus_proxy->CallMethod(
        &name_has_owner_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DBusScreenSaverWatcher::OnServiceHasOwner,
                       weak_factory_.GetWeakPtr()));
  }

  void OnServiceHasOwner(dbus::Response* response) {
    DCHECK_LT(current_service_, kServiceCount);

    dbus::MessageReader reader(response);
    bool owned = false;
    if (!response || !reader.PopBool(&owned) || !owned) {
      VLOG(1) << kServices[current_service_].service_name
              << " D-Bus service does not exist";
      ++current_service_;
      return TryCurrentService();
    }

    // Now connect the ActiveChanged signal.
    proxy_ = bus_->GetObjectProxy(
        kServices[current_service_].service_name,
        dbus::ObjectPath(kServices[current_service_].object_path));
    proxy_->ConnectToSignal(
        kServices[current_service_].interface, kSignalName,
        base::BindRepeating(&DBusScreenSaverWatcher::OnActiveChanged,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&DBusScreenSaverWatcher::OnActiveChangedSignalConnected,
                       weak_factory_.GetWeakPtr()));
  }

  void OnActiveChangedSignalConnected(const std::string& interface,
                                      const std::string& signal,
                                      bool succeeded) {
    DCHECK_LT(current_service_, kServiceCount);
    DCHECK_EQ(interface, kServices[current_service_].interface);
    DCHECK_EQ(signal, kSignalName);

    if (!succeeded) {
      VLOG(1) << "Cannot connect to " << kSignalName << " signal of "
              << interface << " D-Bus service";
      ++current_service_;
      return TryCurrentService();
    }

    // Some service owners (e.g., gsd-screensaver-proxy) advertise the correct
    // methods on org.freedesktop.ScreenSaver, but calling them will result in
    // a NotImplemented DBus error.  To ensure the service owner will send
    // state change events, and to have the correct current state of the lock,
    // make an explicit method call and check that no error is returned.
    dbus::MethodCall method_call(kServices[current_service_].interface,
                                 kMethodName);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&DBusScreenSaverWatcher::OnGetActive,
                                      weak_factory_.GetWeakPtr()));
  }

  void OnGetActive(dbus::Response* response) {
    DCHECK_LT(current_service_, kServiceCount);

    if (!response || !UpdateLockState(response)) {
      VLOG(1)
          << "Call to " << kMethodName << " method of "
          << kServices[current_service_].interface << " D-Bus service failed";
      ++current_service_;
      return TryCurrentService();
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

  LockState lock_state_ = LockState::kUnknown;

  // Index of the service (in the kServices array) that is currently being
  // initialised or used.  A value out of bounds means that no working service
  // was found.
  size_t current_service_ = 0;

  scoped_refptr<dbus::Bus> bus_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<DBusScreenSaverWatcher> weak_factory_{this};
};

DBusScreenSaverWatcher* GetDBusScreenSaverWatcher() {
  static base::NoDestructor<DBusScreenSaverWatcher> impl;
  return impl.get();
}

}  // namespace

#endif  // defined(USE_DBUS)

int CalculateIdleTime() {
  auto* const screen = display::Screen::GetScreen();
  // The screen can be nullptr in tests.
  if (!screen)
    return 0;
  return screen->CalculateIdleTime().InSeconds();
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

#if defined(USE_DBUS)
  auto lock_state = GetDBusScreenSaverWatcher()->lock_state();
  if (lock_state != DBusScreenSaverWatcher::LockState::kUnknown)
    return lock_state == DBusScreenSaverWatcher::LockState::kLocked;
#endif

  auto* const screen = display::Screen::GetScreen();
  // The screen can be nullptr in tests.
  if (!screen)
    return false;
  return screen->IsScreenSaverActive();
}

}  // namespace ui
