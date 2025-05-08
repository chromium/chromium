// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/systemd.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

namespace ui {

namespace {

std::string GetShortcutPrefix(const std::string& accelerator_group_id,
                              const std::string& profile_id) {
  return base::HexEncode(
             crypto::SHA256HashString(accelerator_group_id + profile_id))
      .substr(0, 32);
}

}  // namespace

GlobalAcceleratorListenerLinux::GlobalAcceleratorListenerLinux(
    scoped_refptr<dbus::Bus> bus,
    const std::string& session_token)
    : bus_(std::move(bus)), session_token_(session_token) {
  if (!bus_) {
    bus_ = dbus_thread_linux::GetSharedSessionBus();
  }

  dbus_xdg::SetSystemdScopeUnitNameForXdgPortal(
      bus_.get(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnSystemdUnitStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

GlobalAcceleratorListenerLinux::~GlobalAcceleratorListenerLinux() {
  CloseSession();
}

void GlobalAcceleratorListenerLinux::OnSystemdUnitStarted(
    dbus_xdg::SystemdUnitStatus) {
  // Intentionally ignoring the status.
  dbus_utils::CheckForServiceAndStart(
      bus_.get(), kPortalServiceName,
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlobalAcceleratorListenerLinux::OnServiceStarted(
    std::optional<bool> service_started) {
  service_started_ = service_started.value_or(false);

  if (!*service_started_) {
    bound_commands_.clear();
    return;
  }

  global_shortcuts_proxy_ = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  global_shortcuts_proxy_->ConnectToSignal(
      kGlobalShortcutsInterface, kSignalActivated,
      base::BindRepeating(&GlobalAcceleratorListenerLinux::OnActivatedSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!bound_commands_.empty()) {
    CreateSession();
  }
}

void GlobalAcceleratorListenerLinux::CreateSession() {
  CHECK(!bus_->GetConnectionName().empty());

  std::string session_path_str = base::nix::XdgDesktopPortalSessionPath(
      bus_->GetConnectionName(), session_token_);
  dbus::ObjectPath session_path(session_path_str);
  session_proxy_ = bus_->GetObjectProxy(kPortalServiceName, session_path);

  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodCreateSession, DbusParameters(),
      MakeDbusDictionary("session_handle_token", DbusString(session_token_)),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnCreateSession,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlobalAcceleratorListenerLinux::StartListening() {}

void GlobalAcceleratorListenerLinux::StopListening() {}

bool GlobalAcceleratorListenerLinux::StartListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  // Shortcut registration is now handled in OnCommandsChanged()
  return false;
}

void GlobalAcceleratorListenerLinux::StopListeningForAccelerator(
    const ui::Accelerator& accelerator) {}

bool GlobalAcceleratorListenerLinux::IsRegistrationHandledExternally() const {
  return true;
}

void GlobalAcceleratorListenerLinux::OnCommandsChanged(
    const std::string& accelerator_group_id,
    const std::string& profile_id,
    const ui::CommandMap& commands,
    Observer* observer) {
  // If starting the service failed, there's no need to add the command list.
  if (!service_started_.value_or(true)) {
    return;
  }

  const std::string prefix =
      GetShortcutPrefix(accelerator_group_id, profile_id);
  for (const auto& [_, command] : commands) {
    std::string id = prefix + "-" + command.command_name();
    if (bound_commands_.find(id) == bound_commands_.end()) {
      bound_commands_[id] = {command, accelerator_group_id, observer};
    }
  }

  // Only proceed if there is at least one global command.
  if (std::none_of(
          bound_commands_.begin(), bound_commands_.end(),
          [](const auto& pair) { return pair.second.command.global(); })) {
    return;
  }

  // Wait until the service has started.
  if (!service_started_.value_or(false)) {
    return;
  }

  // If there is no session yet, create one.
  if (!session_proxy_) {
    CreateSession();
    return;
  }

  if (bind_state_ == BindState::kBindCalled) {
    // Wait for an existing bind to finish before re-binding. This is required
    // since GNOME has a quirk where the shortcut dialogs need to be dismissed
    // in the order they're created, otherwise the portal interface will
    // indicate an error.
    bind_state_ = BindState::kNeedsRebind;
  } else if (bind_state_ == BindState::kBound) {
    CloseSession();
    CreateSession();
  }
}

void GlobalAcceleratorListenerLinux::OnCreateSession(
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    VLOG(1) << "Failed to call CreateSession (error code "
            << static_cast<int>(results.error()) << ").";
    session_proxy_ = nullptr;
    return;
  }

  auto* session_handle = results->GetAs<DbusString>("session_handle");
  if (!session_handle ||
      session_proxy_->object_path().value() != session_handle->value()) {
    LOG(ERROR) << "Expected session handle does not match.";
    session_proxy_ = nullptr;
    return;
  }

  // Now that the session is created, bind all accumulated shortcuts.
  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodListShortcuts);
  dbus::MessageWriter writer(&method_call);
  writer.AppendObjectPath(session_proxy_->object_path());
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodListShortcuts, DbusObjectPath(session_proxy_->object_path()),
      DbusDictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnListShortcuts,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlobalAcceleratorListenerLinux::OnListShortcuts(
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call ListShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
    return;
  }

  auto* shortcuts = results->GetAs<DbusShortcuts>("shortcuts");
  if (!shortcuts) {
    LOG(ERROR) << "No shortcuts in ListShortcuts response.";
    return;
  }

  std::set<std::string> registered_ids;
  for (const DbusShortcut& shortcut : shortcuts->value()) {
    const DbusString& id = std::get<0>(shortcut.value());
    registered_ids.insert(id.value());
  }

  // If any bound command is not found among the registered shortcuts, bind
  // them.
  for (const auto& [modified_id, bound_cmd] : bound_commands_) {
    if (registered_ids.find(modified_id) == registered_ids.end()) {
      BindShortcuts(*shortcuts);
      return;
    }
  }
}

void GlobalAcceleratorListenerLinux::BindShortcuts(
    const DbusShortcuts& old_shortcuts) {
  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodBindShortcuts);
  dbus::MessageWriter writer(&method_call);

  writer.AppendObjectPath(session_proxy_->object_path());

  DbusShortcuts shortcuts;
  for (const auto& old_shortcut : old_shortcuts.value()) {
    const std::string& id = std::get<0>(old_shortcut.value()).value();
    const DbusDictionary& properties = std::get<1>(old_shortcut.value());
    DbusDictionary new_props;
    if (auto* desc = properties.GetAs<DbusString>("description")) {
      new_props.PutAs("description", DbusString(desc->value()));
    }
    shortcuts.value().emplace_back(DbusString(id), std::move(new_props));
  }

  for (const auto& [modified_id, bound_cmd] : bound_commands_) {
    DbusDictionary props = MakeDbusDictionary(
        "description",
        DbusString(base::UTF16ToUTF8(bound_cmd.command.description())));
    shortcuts.value().push_back(
        MakeDbusStruct(DbusString(modified_id), std::move(props)));
  }

  bind_state_ = BindState::kBindCalled;
  DbusString empty_parent_window;
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodBindShortcuts,
      MakeDbusParameters(DbusObjectPath(session_proxy_->object_path()),
                         std::move(shortcuts), std::move(empty_parent_window)),
      DbusDictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnBindShortcuts,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlobalAcceleratorListenerLinux::CloseSession() {
  if (!session_proxy_) {
    return;
  }
  dbus::MethodCall method_call(kSessionInterface, kMethodCloseSession);
  session_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, base::DoNothing());
  session_proxy_ = nullptr;
  request_.reset();
  bind_state_ = BindState::kNotBound;
}

void GlobalAcceleratorListenerLinux::OnBindShortcuts(
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call BindShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
    return;
  }
  // Shortcuts successfully bound.
  if (bind_state_ == BindState::kNeedsRebind) {
    CloseSession();
    CreateSession();
  } else {
    CHECK_EQ(bind_state_, BindState::kBindCalled);
    bind_state_ = BindState::kBound;
  }
}

void GlobalAcceleratorListenerLinux::OnActivatedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  dbus::ObjectPath session_handle;
  std::string shortcut_id;
  uint64_t timestamp;

  if (!reader.PopObjectPath(&session_handle) ||
      !reader.PopString(&shortcut_id) || !reader.PopUint64(&timestamp)) {
    LOG(ERROR) << "Failed to parse Activated signal.";
    return;
  }

  // Only process the signal if it comes from our current session.
  if (!session_proxy_ || session_proxy_->object_path() != session_handle) {
    return;
  }

  auto it = bound_commands_.find(shortcut_id);
  if (it == bound_commands_.end()) {
    return;
  }

  const auto& cmd = it->second;
  it->second.observer->ExecuteCommand(cmd.accelerator_group_id,
                                      cmd.command.command_name());
}

void GlobalAcceleratorListenerLinux::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal: " << interface_name << "."
               << signal_name;
  }
}

}  // namespace ui
