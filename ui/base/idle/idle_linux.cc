// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/config/linux/dbus/buildflags.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/display/screen.h"

#if BUILDFLAG(USE_DBUS)
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#endif

namespace ui {

#if BUILDFLAG(USE_DBUS)

namespace {

const char kMethodName[] = "GetActive";
const char kSignalName[] = "ActiveChanged";

// Various names under which the service may be found in different Linux desktop
// environments.
struct Services {
  const char* service_name;
  const char* object_path;
  const char* interface;
};
constexpr auto kServices = std::to_array<Services>({
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
});

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

  static DBusScreenSaverWatcher* GetInstance() {
    static base::NoDestructor<DBusScreenSaverWatcher> instance;
    return instance.get();
  }

  DBusScreenSaverWatcher(const DBusScreenSaverWatcher&) = delete;
  DBusScreenSaverWatcher& operator=(const DBusScreenSaverWatcher&) = delete;

  LockState lock_state() const { return lock_state_; }

  base::CallbackListSubscription AddCallback(
      base::RepeatingCallback<void(bool)> callback) {
    return callbacks_.Add(std::move(callback));
  }

 private:
  friend class base::NoDestructor<DBusScreenSaverWatcher>;

  DBusScreenSaverWatcher() : bus_(dbus_thread_linux::GetSharedSessionBus()) {
    TryCurrentService();
  }

  ~DBusScreenSaverWatcher() = default;

  // Starts the initialisation sequence for the current service.  Failure at any
  // step will increment the service counter and re-start the process.
  void TryCurrentService() {
    // Detach the proxy, if we have one from the previous attempt.
    if (current_service_ > 0) {
      bus_->RemoveObjectProxy(
          kServices[current_service_ - 1].service_name,
          dbus::ObjectPath(kServices[current_service_ - 1].object_path),
          base::DoNothing());
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
    dbus_utils::NameHasOwner(
        bus_.get(), kServices[current_service_].service_name,
        base::BindOnce(&DBusScreenSaverWatcher::OnServiceHasOwner,
                       weak_factory_.GetWeakPtr()));
  }

  void OnServiceHasOwner(std::optional<bool> name_has_owner) {
    DCHECK_LT(current_service_, kServiceCount);

    if (!name_has_owner.value_or(false)) {
      VLOG(1) << kServices[current_service_].service_name
              << " D-Bus service does not exist";
      ++current_service_;
      return TryCurrentService();
    }

    // Now connect the ActiveChanged signal.
    dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
        kServices[current_service_].service_name,
        dbus::ObjectPath(kServices[current_service_].object_path));
    proxy->ConnectToSignal(
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
    dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
        kServices[current_service_].service_name,
        dbus::ObjectPath(kServices[current_service_].object_path));
    proxy->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DBusScreenSaverWatcher::OnGetActive,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGetActive(dbus::Response* response, dbus::ErrorResponse*) {
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
    if (!reader.PopBool(&active) || reader.HasMoreData()) {
      return false;
    }
    LockState new_lock_state =
        active ? LockState::kLocked : LockState::kUnlocked;
    if (lock_state_ == new_lock_state) {
      return true;
    }

    lock_state_ = new_lock_state;
    callbacks_.Notify(lock_state_ == LockState::kLocked);

    return true;
  }

  LockState lock_state_ = LockState::kUnknown;

  // Index of the service (in the kServices array) that is currently being
  // initialised or used.  A value out of bounds means that no working service
  // was found.
  size_t current_service_ = 0;

  scoped_refptr<dbus::Bus> bus_;
  base::RepeatingCallbackList<void(bool)> callbacks_;

  base::WeakPtrFactory<DBusScreenSaverWatcher> weak_factory_{this};
};

}  // namespace

#endif  // BUILDFLAG(USE_DBUS)

base::CallbackListSubscription AddScreenLockCallback(
    base::RepeatingCallback<void(bool)> callback) {
#if BUILDFLAG(USE_DBUS)
  return DBusScreenSaverWatcher::GetInstance()->AddCallback(
      std::move(callback));
#else
  return {};
#endif
}

int CalculateIdleTime() {
  auto* const screen = display::Screen::Get();
  // The screen can be nullptr in tests.
  if (!screen) {
    return 0;
  }
  return screen->CalculateIdleTime().InSeconds();
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value()) {
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;
  }

#if BUILDFLAG(USE_DBUS)
  auto lock_state = DBusScreenSaverWatcher::GetInstance()->lock_state();
  if (lock_state != DBusScreenSaverWatcher::LockState::kUnknown) {
    return lock_state == DBusScreenSaverWatcher::LockState::kLocked;
  }
#endif

  auto* const screen = display::Screen::Get();
  // The screen can be nullptr in tests.
  if (!screen) {
    return false;
  }
  return screen->IsScreenSaverActive();
}

}  // namespace ui
