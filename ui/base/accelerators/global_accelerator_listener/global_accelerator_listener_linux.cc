// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/request.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/linux/linux_ui_delegate.h"

namespace ui {

namespace {

template <typename T>
std::optional<T> TakeFromDict(dbus_xdg::Dictionary& dict,
                              const std::string& key) {
  auto it = dict.find(key);
  if (it == dict.end()) {
    return std::nullopt;
  }
  auto result = std::move(it->second).Take<T>();
  dict.erase(it);
  return result;
}

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

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

GlobalAcceleratorListenerLinux::~GlobalAcceleratorListenerLinux() {
  CloseSession();
}

void GlobalAcceleratorListenerLinux::OnServiceStarted(bool service_started) {
  service_started_ = service_started;

  if (!*service_started_) {
    bound_commands_.clear();
    return;
  }

  global_shortcuts_proxy_ = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  dbus_utils::ConnectToSignal<"ost">(
      global_shortcuts_proxy_, kGlobalShortcutsInterface, kSignalActivated,
      base::BindRepeating(&GlobalAcceleratorListenerLinux::OnActivatedSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  if (HasGlobalShortcuts()) {
    CreateSession();
  }
}

void GlobalAcceleratorListenerLinux::CreateSession() {
  CHECK(!bus_->GetConnectionName().empty());

  std::string session_path_str = base::nix::XdgDesktopPortalSessionPath(
      bus_->GetConnectionName(), session_token_);
  dbus::ObjectPath session_path(session_path_str);
  session_proxy_ = bus_->GetObjectProxy(kPortalServiceName, session_path);

  dbus_xdg::Dictionary options;
  options["session_handle_token"] =
      dbus_utils::Variant::Wrap<"s">(session_token_);
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodCreateSession, std::move(options),
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
    gfx::AcceleratedWidget widget,
    Observer* observer) {
  // If starting the service failed, there's no need to add the command list.
  if (!service_started_.value_or(true)) {
    return;
  }

  context_window_ = widget;

  const std::string prefix =
      GetShortcutPrefix(accelerator_group_id, profile_id);
  for (const auto& [_, command] : commands) {
    std::string id = prefix + "-" + command.command_name();
    bound_commands_[id] = {command, accelerator_group_id, observer};
  }

  // Only proceed if there is at least one global command.
  if (!HasGlobalShortcuts()) {
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
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    VLOG(1) << "Failed to call CreateSession (error code "
            << static_cast<int>(results.error()) << ").";
    session_proxy_ = nullptr;
    return;
  }

  auto session_handle = TakeFromDict<std::string>(*results, "session_handle");
  if (!session_handle ||
      session_proxy_->object_path().value() != *session_handle) {
    LOG(ERROR) << "Expected session handle does not match.";
    session_proxy_ = nullptr;
    return;
  }

  // Now that the session is created, bind all accumulated shortcuts.
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodListShortcuts, dbus_xdg::Dictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnListShortcuts,
                     weak_ptr_factory_.GetWeakPtr()),
      session_proxy_->object_path());
}

void GlobalAcceleratorListenerLinux::OnListShortcuts(
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call ListShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
    return;
  }

  auto shortcuts = TakeFromDict<DbusShortcuts>(*results, "shortcuts");
  if (!shortcuts) {
    LOG(ERROR) << "Failed to parse shortcuts from ListShortcuts response.";
    return;
  }

  std::set<std::string> registered_ids;
  for (const DbusShortcut& shortcut : *shortcuts) {
    registered_ids.insert(std::get<0>(shortcut));
  }

  // If any bound command is not found among the registered shortcuts, bind
  // them.
  for (const auto& [modified_id, bound_cmd] : bound_commands_) {
    if (registered_ids.find(modified_id) == registered_ids.end()) {
      auto* delegate = ui::LinuxUiDelegate::GetInstance();
      if (delegate && context_window_ != gfx::kNullAcceleratedWidget) {
        delegate->ExportWindowHandle(
            context_window_,
            base::BindOnce(&GlobalAcceleratorListenerLinux::BindShortcuts,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(*shortcuts)));
      } else {
        BindShortcuts(std::move(*shortcuts), "");
      }
      return;
    }
  }
}

void GlobalAcceleratorListenerLinux::BindShortcuts(DbusShortcuts old_shortcuts,
                                                   std::string parent_handle) {
  DbusShortcuts shortcuts;
  for (auto& old_shortcut : old_shortcuts) {
    const std::string& id = std::get<0>(old_shortcut);
    dbus_xdg::Dictionary& properties = std::get<1>(old_shortcut);
    dbus_xdg::Dictionary new_props;
    auto description = TakeFromDict<std::string>(properties, "description");
    if (description) {
      new_props["description"] =
          dbus_utils::Variant::Wrap<"s">(std::move(*description));
    }
    shortcuts.emplace_back(id, std::move(new_props));
  }

  for (const auto& [modified_id, bound_cmd] : bound_commands_) {
    dbus_xdg::Dictionary props;
    props["description"] = dbus_utils::Variant::Wrap<"s">(
        base::UTF16ToUTF8(bound_cmd.command.description()));
    shortcuts.emplace_back(modified_id, std::move(props));
  }

  bind_state_ = BindState::kBindCalled;
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodBindShortcuts, dbus_xdg::Dictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnBindShortcuts,
                     weak_ptr_factory_.GetWeakPtr()),
      session_proxy_->object_path(), std::move(shortcuts),
      std::move(parent_handle));
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
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
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

void GlobalAcceleratorListenerLinux::OnActivatedSignal(
    dbus_utils::ConnectToSignalResultSig<"ost"> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse Activated signal.";
    return;
  }

  auto [session_handle, shortcut_id, timestamp] = std::move(result.value());

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

bool GlobalAcceleratorListenerLinux::HasGlobalShortcuts() const {
  return std::ranges::any_of(bound_commands_, [](const auto& pair) {
    return pair.second.command.global();
  });
}

}  // namespace ui
