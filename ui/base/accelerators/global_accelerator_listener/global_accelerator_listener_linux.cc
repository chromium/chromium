// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/session.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/linux/xdg_shortcut.h"
#include "ui/base/ui_base_features.h"
#include "ui/linux/linux_ui_delegate.h"

namespace ui {

namespace {

constexpr char kStateDescriptionKey[] = "description";
constexpr char kStateTriggerKey[] = "trigger";
constexpr char kStateStateKey[] = "state";

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

bool ShouldSetPreferredTrigger() {
  auto env = base::Environment::Create();
  const base::nix::DesktopEnvironment desktop_environment =
      base::nix::GetDesktopEnvironment(env.get());
  const bool default_preferred_trigger =
      desktop_environment != base::nix::DESKTOP_ENVIRONMENT_GNOME;
  return base::FeatureList::GetStateIfOverridden(
             features::kGlobalShortcutsPortalPreferredTrigger)
      .value_or(default_preferred_trigger);
}

std::string GetDescription(const ui::Command& command) {
  // Some portal backends ignore shortcuts without a description, so fall back
  // to the command name.
  std::string description = base::UTF16ToUTF8(command.description());
  return description.empty() ? command.command_name() : description;
}

}  // namespace

GlobalAcceleratorListenerLinux::BoundCommand::BoundCommand() = default;
GlobalAcceleratorListenerLinux::BoundCommand::~BoundCommand() = default;
GlobalAcceleratorListenerLinux::BoundCommand::BoundCommand(
    const BoundCommand&) = default;
GlobalAcceleratorListenerLinux::BoundCommand&
GlobalAcceleratorListenerLinux::BoundCommand::operator=(const BoundCommand&) =
    default;
GlobalAcceleratorListenerLinux::BoundCommand::BoundCommand(BoundCommand&&) =
    default;
GlobalAcceleratorListenerLinux::BoundCommand&
GlobalAcceleratorListenerLinux::BoundCommand::operator=(BoundCommand&&) =
    default;

GlobalAcceleratorListenerLinux::GlobalAcceleratorListenerLinux(
    scoped_refptr<dbus::Bus> bus,
    const std::string& session_token)
    : bus_(std::move(bus)),
      session_token_(session_token),
      set_preferred_trigger_(ShouldSetPreferredTrigger()) {
  if (!bus_) {
    bus_ = dbus_thread_linux::GetSharedSessionBus();
  }

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(), kGlobalShortcutsInterface,
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

GlobalAcceleratorListenerLinux::~GlobalAcceleratorListenerLinux() {
  CloseSession();
}

void GlobalAcceleratorListenerLinux::OnServiceStarted(uint32_t version) {
  service_started_ = (version != 0);
  portal_version_ = version;

  if (!*service_started_) {
    bound_commands_.clear();
    return;
  }

  global_shortcuts_proxy_ = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  dbus_utils::ConnectToSignal<"osta{sv}">(
      global_shortcuts_proxy_, kGlobalShortcutsInterface, kSignalActivated,
      base::BindRepeating(&GlobalAcceleratorListenerLinux::OnActivatedSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  MaybeBind();
}

void GlobalAcceleratorListenerLinux::CreateSession() {
  CHECK(!bus_->GetConnectionName().empty());

  dbus_xdg::Dictionary options;
  options["session_handle_token"] =
      dbus_utils::Variant::Wrap<"s">(session_token_);
  session_ = dbus_xdg::Session::CreateWithRequest(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      std::move(options),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnCreateSessionResponse,
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
    base::RepeatingCallback<void(const std::string&, const std::string&)>
        execute_command) {
  // If starting the service failed, there's no need to add the command list.
  if (!service_started_.value_or(true)) {
    return;
  }

  context_window_ = widget;

  const std::string id_prefix =
      GetShortcutPrefix(accelerator_group_id, profile_id) + "-";
  loaded_prefixes_.insert(id_prefix);

  // Only global commands are bound. Binding non-global commands would show
  // them in the portal dialog, and some backends would show the dialog again
  // whenever the set of non-global commands changes.
  base::flat_set<std::string> global_ids;
  for (const auto& [_, command] : commands) {
    if (command.global()) {
      global_ids.insert(id_prefix + command.command_name());
    }
  }

  // Replace the command set for this prefix instead of only inserting. This
  // avoids retaining stale commands after command updates. Shortcuts that are
  // no longer global are forgotten, so making them global again offers them
  // again.
  auto is_stale = [&](const auto& pair) {
    return pair.first.starts_with(id_prefix) &&
           !global_ids.contains(pair.first);
  };
  std::erase_if(bound_commands_, is_stale);
  std::erase_if(offered_shortcuts_, is_stale);
  std::erase_if(untracked_shortcuts_, [&](const auto& pair) {
    return pair.first.starts_with(id_prefix);
  });

  for (const auto& [_, command] : commands) {
    if (!command.global()) {
      continue;
    }
    std::string id = id_prefix + command.command_name();
    auto& bc = bound_commands_[id];
    bc.command = command;
    bc.accelerator_group_id = accelerator_group_id;
    bc.execute_command = execute_command;

    std::string trigger = AcceleratorToXdgShortcut(command.accelerator());
    auto [it, inserted] = offered_shortcuts_.try_emplace(id);
    OfferedShortcut& offered = it->second;
    if (!inserted && offered.state == OfferState::kDeclined &&
        offered.trigger != trigger) {
      // The user changed the accelerator of a declined shortcut, so offer it
      // again.
      offered.state = OfferState::kNew;
    }
    offered.description = GetDescription(command);
    offered.trigger = std::move(trigger);
  }

  SaveState();
  MaybeBind();
}

void GlobalAcceleratorListenerLinux::SetStateStore(
    std::unique_ptr<StateStore> store) {
  if (state_store_ || !store) {
    return;
  }
  // The store must be set before any commands are loaded, so that the loaded
  // state doesn't need to be merged with the commands.
  CHECK(loaded_prefixes_.empty());
  CHECK(offered_shortcuts_.empty());
  state_store_ = std::move(store);
  LoadState();
  // Don't bind yet. Shortcuts whose owner isn't loaded don't need to be
  // activated, and they're included in the bind triggered by the next
  // OnCommandsChanged().
}

void GlobalAcceleratorListenerLinux::MaybeBind() {
  if (GetIdsToBind().empty()) {
    CloseSession();
    return;
  }

  // Wait until the service has started.
  if (!service_started_.value_or(false)) {
    return;
  }

  // If there is no session yet, create one.
  if (!session_) {
    CreateSession();
    return;
  }

  // If session creation is still in progress, wait for it to complete.
  if (!session_->path().IsValid()) {
    return;
  }

  switch (bind_state_) {
    case BindState::kNotBound:
      // ListShortcuts is in progress. OnListShortcuts() will bind the latest
      // set of shortcuts.
      return;
    case BindState::kBindCalled:
    case BindState::kNeedsRebind:
      // Wait for an existing bind to finish before re-binding. This is
      // required since GNOME has a quirk where the shortcut dialogs need to be
      // dismissed in the order they're created, otherwise the portal interface
      // will indicate an error. If shortcuts that were added during the bind
      // have been removed again, no rebind is needed.
      bind_state_ = HasIdsToBindNotIn(pending_bind_ids_)
                        ? BindState::kNeedsRebind
                        : BindState::kBindCalled;
      return;
    case BindState::kBound:
      // Some backends only allow one bind per session, so re-create the
      // session to bind new shortcuts.
      if (HasIdsToBindNotIn(session_ids_)) {
        CloseSession();
        CreateSession();
      }
      return;
  }
}

void GlobalAcceleratorListenerLinux::OnCreateSessionResponse(
    dbus_xdg::Session* session) {
  if (!session) {
    VLOG(1) << "Failed to create portal session.";
    session_.reset();
    return;
  }

  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodListShortcuts, dbus_xdg::Dictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnListShortcuts,
                     weak_ptr_factory_.GetWeakPtr()),
      session->path());
}

void GlobalAcceleratorListenerLinux::OnListShortcuts(
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
  std::optional<DbusShortcuts> shortcuts;
  if (results.has_value()) {
    shortcuts = TakeFromDict<DbusShortcuts>(*results, "shortcuts");
    if (!shortcuts) {
      LOG(ERROR) << "Failed to parse shortcuts from ListShortcuts response.";
    }
  } else {
    LOG(ERROR) << "Failed to call ListShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
  }

  // Continue with an empty list on failure. Some backends never list
  // shortcuts, so binding is required anyway.
  base::flat_set<std::string> listed_ids;
  if (shortcuts) {
    for (auto& [id, properties] : *shortcuts) {
      listed_ids.insert(id);
      auto it = offered_shortcuts_.find(id);
      if (it != offered_shortcuts_.end()) {
        // The backend already knows this shortcut, so binding it again won't
        // show a dialog.
        it->second.state = OfferState::kKnown;
        continue;
      }
      if (IsIdOfLoadedGroup(id)) {
        // A shortcut of a loaded group that is no longer global, or that was
        // bound by an older version that also bound non-global commands.
        continue;
      }
      // A shortcut that isn't tracked, e.g. one bound by an older version that
      // didn't persist offered shortcuts. Pass it through to BindShortcuts so
      // that it isn't removed from the portal.
      auto description = TakeFromDict<std::string>(properties, "description");
      untracked_shortcuts_[id] =
          description && !description->empty() ? *description : id;
    }
  }
  SaveState();

  const base::flat_set<std::string> ids_to_bind = GetIdsToBind();
  if (ids_to_bind.empty()) {
    CloseSession();
    return;
  }

  // Older backends that implement version 1 of the interface show a dialog on
  // every bind, but keep shortcuts that are listed active, so skip binding if
  // every shortcut is listed.
  if (portal_version_ < 2 &&
      std::ranges::all_of(
          ids_to_bind,
          [&](const std::string& id) { return listed_ids.contains(id); })) {
    session_ids_ = ids_to_bind;
    bind_state_ = BindState::kBound;
    return;
  }

  auto* delegate = ui::LinuxUiDelegate::GetInstance();
  if (delegate && context_window_ != gfx::kNullAcceleratedWidget) {
    delegate->ExportWindowHandle(
        context_window_,
        base::BindOnce(&GlobalAcceleratorListenerLinux::BindShortcuts,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    BindShortcuts("");
  }
}

void GlobalAcceleratorListenerLinux::BindShortcuts(std::string parent_handle) {
  if (!session_ || !session_->path().IsValid()) {
    return;
  }

  // Each bind replaces the set of shortcuts that the portal has stored for the
  // app, so always bind the complete set. The map ensures there are no
  // duplicate IDs.
  std::map<std::string, dbus_xdg::Dictionary> shortcut_map;
  for (const auto& [id, description] : untracked_shortcuts_) {
    shortcut_map[id]["description"] =
        dbus_utils::Variant::Wrap<"s">(description);
  }

  const base::flat_set<std::string> ids_to_bind = GetIdsToBind();
  for (const std::string& id : ids_to_bind) {
    dbus_xdg::Dictionary& props = shortcut_map[id];
    props.clear();
    auto bound_it = bound_commands_.find(id);
    if (bound_it != bound_commands_.end()) {
      const ui::Command& command = bound_it->second.command;
      props["description"] =
          dbus_utils::Variant::Wrap<"s">(GetDescription(command));
      std::string trigger = AcceleratorToXdgShortcut(command.accelerator());
      if (set_preferred_trigger_ && !trigger.empty()) {
        props["preferred_trigger"] =
            dbus_utils::Variant::Wrap<"s">(std::move(trigger));
      }
    } else {
      // The owner of the shortcut isn't loaded, so only include it to keep it
      // stored in the portal.
      props["description"] = dbus_utils::Variant::Wrap<"s">(
          std::string(offered_shortcuts_.at(id).description));
    }
  }

  DbusShortcuts shortcuts;
  shortcuts.reserve(shortcut_map.size());
  for (auto& [id, props] : shortcut_map) {
    shortcuts.emplace_back(id, std::move(props));
  }

  pending_bind_ids_ = ids_to_bind;
  bind_state_ = BindState::kBindCalled;
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodBindShortcuts, dbus_xdg::Dictionary(),
      base::BindOnce(&GlobalAcceleratorListenerLinux::OnBindShortcuts,
                     weak_ptr_factory_.GetWeakPtr()),
      session_->path(), std::move(shortcuts), std::move(parent_handle));
}

void GlobalAcceleratorListenerLinux::CloseSession() {
  if (!session_) {
    return;
  }
  session_.reset();
  request_.reset();
  bind_state_ = BindState::kNotBound;
  session_ids_.clear();
  pending_bind_ids_.clear();
}

void GlobalAcceleratorListenerLinux::OnBindShortcuts(
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
  const bool needs_rebind = bind_state_ == BindState::kNeedsRebind;
  base::flat_set<std::string> bound_ids = std::move(pending_bind_ids_);
  pending_bind_ids_.clear();

  if (results.has_value()) {
    for (const std::string& id : bound_ids) {
      auto it = offered_shortcuts_.find(id);
      if (it != offered_shortcuts_.end()) {
        it->second.state = OfferState::kKnown;
      }
    }
    SaveState();
    session_ids_ = std::move(bound_ids);
    bind_state_ = BindState::kBound;
    if (needs_rebind) {
      CloseSession();
      CreateSession();
    }
    return;
  }

  LOG(ERROR) << "Failed to call BindShortcuts (error code "
             << static_cast<int>(results.error()) << ").";

  // If the user cancelled the dialog, or the backend failed to apply the
  // shortcuts after showing it (e.g. xdg-desktop-portal-gnome without GNOME
  // Shell, which always responds with kRequestCancelledOther), don't offer the
  // new shortcuts again. Otherwise the dialog would be shown on every start.
  // If there are no new shortcuts and the user cancelled the dialog, the portal
  // showed it for known shortcuts, e.g. because its stored shortcuts were reset
  // or a different backend is used now, so decline those instead. Known
  // shortcuts aren't declined on kRequestCancelledOther, since a backend may
  // respond with it without showing a dialog (e.g. if it isn't ready yet), and
  // declining would permanently deactivate working shortcuts.
  bool newly_declined = false;
  const bool cancelled_by_user =
      results.error() == dbus_xdg::ResponseError::kRequestCancelledByUser;
  if (cancelled_by_user ||
      results.error() == dbus_xdg::ResponseError::kRequestCancelledOther) {
    const bool has_new = std::ranges::any_of(bound_ids, [&](const auto& id) {
      auto it = offered_shortcuts_.find(id);
      return it != offered_shortcuts_.end() &&
             it->second.state == OfferState::kNew;
    });
    for (const std::string& id : bound_ids) {
      auto it = offered_shortcuts_.find(id);
      if (it == offered_shortcuts_.end() ||
          it->second.state == OfferState::kDeclined) {
        continue;
      }
      if (it->second.state == OfferState::kNew ||
          (!has_new && cancelled_by_user)) {
        it->second.state = OfferState::kDeclined;
        newly_declined = true;
      }
    }
    SaveState();
  }

  base::EraseIf(bound_ids, [&](const std::string& id) {
    auto it = offered_shortcuts_.find(id);
    return it != offered_shortcuts_.end() &&
           it->second.state == OfferState::kDeclined;
  });
  session_ids_ = std::move(bound_ids);
  bind_state_ = BindState::kBound;

  if (GetIdsToBind().empty()) {
    // Everything was declined, so there's nothing to keep the session for.
    CloseSession();
    return;
  }

  // Retry with the remaining shortcuts so that previously accepted shortcuts
  // stay active. This can't loop indefinitely, since a retry only happens if a
  // shortcut was declined or shortcuts were added during the bind.
  if (newly_declined || needs_rebind) {
    CloseSession();
    CreateSession();
  }
}

void GlobalAcceleratorListenerLinux::OnActivatedSignal(
    dbus_utils::ConnectToSignalResultSig<"osta{sv}"> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse Activated signal.";
    return;
  }

  auto [session_handle, shortcut_id, timestamp, options] =
      std::move(result.value());

  // Only process the signal if it comes from our current session.
  if (!session_ || session_->path() != session_handle) {
    return;
  }

  auto it = bound_commands_.find(shortcut_id);
  if (it == bound_commands_.end()) {
    return;
  }

  const auto& cmd = it->second;
  cmd.execute_command.Run(cmd.accelerator_group_id, cmd.command.command_name());
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

void GlobalAcceleratorListenerLinux::PruneStaleCommands() {
  // The shortcuts stay in `offered_shortcuts_` so that they remain stored in
  // the portal while their owner isn't loaded.
  std::erase_if(bound_commands_, [](const auto& pair) {
    return pair.second.execute_command.IsCancelled();
  });
}

base::flat_set<std::string> GlobalAcceleratorListenerLinux::GetIdsToBind()
    const {
  std::vector<std::string> ids;
  ids.reserve(offered_shortcuts_.size());
  for (const auto& [id, offered] : offered_shortcuts_) {
    if (offered.state != OfferState::kDeclined) {
      ids.push_back(id);
    }
  }
  return base::flat_set<std::string>(std::move(ids));
}

bool GlobalAcceleratorListenerLinux::HasIdsToBindNotIn(
    const base::flat_set<std::string>& ids) const {
  return std::ranges::any_of(
      GetIdsToBind(), [&](const std::string& id) { return !ids.contains(id); });
}

bool GlobalAcceleratorListenerLinux::IsIdOfLoadedGroup(
    const std::string& id) const {
  return std::ranges::any_of(loaded_prefixes_, [&](const std::string& prefix) {
    return id.starts_with(prefix);
  });
}

void GlobalAcceleratorListenerLinux::LoadState() {
  if (!state_store_) {
    return;
  }
  base::DictValue state = state_store_->Load();
  for (auto [id, value] : state) {
    const base::DictValue* entry = value.GetIfDict();
    if (!entry) {
      continue;
    }
    const std::string* description = entry->FindString(kStateDescriptionKey);
    const std::string* trigger = entry->FindString(kStateTriggerKey);
    std::optional<int> state_value = entry->FindInt(kStateStateKey);
    if (!description || !state_value ||
        *state_value < static_cast<int>(OfferState::kNew) ||
        *state_value > static_cast<int>(OfferState::kDeclined)) {
      continue;
    }
    // The trigger is only needed for declined shortcuts.
    offered_shortcuts_[id] = {*description, trigger ? *trigger : std::string(),
                              static_cast<OfferState>(*state_value)};
  }
}

void GlobalAcceleratorListenerLinux::SaveState() {
  if (!state_store_) {
    return;
  }
  base::DictValue state;
  for (const auto& [id, offered] : offered_shortcuts_) {
    state.Set(id, base::DictValue()
                      .Set(kStateDescriptionKey, offered.description)
                      .Set(kStateTriggerKey, offered.trigger)
                      .Set(kStateStateKey, static_cast<int>(offered.state)));
  }
  state_store_->Save(std::move(state));
}

}  // namespace ui
