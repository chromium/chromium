// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dbus/xdg/request.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace dbus_xdg {
class Request;
enum class SystemdUnitStatus;
}  // namespace dbus_xdg

namespace ui {

// Linux-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts using the org.freedesktop.portal.GlobalShortcuts
// interface.
class GlobalAcceleratorListenerLinux : public GlobalAcceleratorListener {
 public:
  GlobalAcceleratorListenerLinux(scoped_refptr<dbus::Bus> bus,
                                 const std::string& session_token);

  GlobalAcceleratorListenerLinux(const GlobalAcceleratorListenerLinux&) =
      delete;
  GlobalAcceleratorListenerLinux& operator=(
      const GlobalAcceleratorListenerLinux&) = delete;

  ~GlobalAcceleratorListenerLinux() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GlobalAcceleratorListenerLinuxTest,
                           OnCommandsChanged);

  using DbusShortcut = DbusStruct<DbusString, DbusDictionary>;
  using DbusShortcuts = DbusArray<DbusShortcut>;

  // These are exposed in the header for testing.
  static constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
  static constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
  static constexpr char kGlobalShortcutsInterface[] =
      "org.freedesktop.portal.GlobalShortcuts";
  static constexpr char kSessionInterface[] = "org.freedesktop.portal.Session";

  static constexpr char kMethodCreateSession[] = "CreateSession";
  static constexpr char kMethodListShortcuts[] = "ListShortcuts";
  static constexpr char kMethodBindShortcuts[] = "BindShortcuts";
  static constexpr char kMethodCloseSession[] = "Close";
  static constexpr char kSignalActivated[] = "Activated";

  enum class BindState {
    kNotBound,
    kBindCalled,
    kNeedsRebind,
    kBound,
  };

  struct BoundCommand {
    ui::Command command;
    std::string accelerator_group_id;
    raw_ptr<Observer> observer = nullptr;
  };

  // GlobalAcceleratorListener:
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override;
  void StopListeningForAccelerator(const ui::Accelerator& accelerator) override;
  bool IsRegistrationHandledExternally() const override;
  void OnCommandsChanged(const std::string& accelerator_group_id,
                         const std::string& profile_id,
                         const ui::CommandMap& commands,
                         Observer* observer) override;

  void OnCreateSession(
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);
  void OnListShortcuts(
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);
  void OnBindShortcuts(
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);

  // Callbacks for DBus signals.
  void OnActivatedSignal(dbus::Signal* signal);

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  void OnSystemdUnitStarted(dbus_xdg::SystemdUnitStatus status);

  void OnServiceStarted(std::optional<bool> service_started);

  void CreateSession();

  void BindShortcuts(const DbusShortcuts& old_shortcuts);

  void CloseSession();

  // DBus components.
  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> global_shortcuts_proxy_ = nullptr;
  raw_ptr<dbus::ObjectProxy> session_proxy_ = nullptr;
  std::optional<bool> service_started_;
  std::unique_ptr<dbus_xdg::Request> request_;
  BindState bind_state_ = BindState::kNotBound;
  const std::string session_token_;

  std::map<std::string, BoundCommand> bound_commands_;

  base::WeakPtrFactory<GlobalAcceleratorListenerLinux> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_
