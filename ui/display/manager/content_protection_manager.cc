// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/content_protection_manager.h"

#include <utility>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "ui/display/manager/apply_content_protection_task.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/query_content_protection_task.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"

namespace display {

namespace {

// HDCP requires suppressing content within 2 seconds when authentication drops.
constexpr auto kDisplaySecurityPollingPeriod = base::Seconds(2);

}  // namespace

ContentProtectionManager::ContentProtectionManager(
    DisplayLayoutManager* layout_manager,
    ConfigurationDisabledCallback config_disabled_callback)
    : layout_manager_(layout_manager),
      config_disabled_callback_(std::move(config_disabled_callback)) {}

ContentProtectionManager::~ContentProtectionManager() {
  // Destroy to fire failure callbacks before weak pointers for |this| expire.
  tasks_ = {};
}

ContentProtectionManager::ClientId ContentProtectionManager::RegisterClient() {
  if (disabled())
    return std::nullopt;

  ClientId client_id = next_client_id_++;
  bool success = requests_.emplace(*client_id, ContentProtections()).second;
  DCHECK(success);

  return client_id;
}

void ContentProtectionManager::UnregisterClient(ClientId client_id) {
  if (disabled())
    return;

  DCHECK(GetContentProtections(client_id));
  requests_.erase(*client_id);

  QueueTask(std::make_unique<ApplyContentProtectionTask>(
      layout_manager_, native_display_delegate_, AggregateContentProtections(),
      base::BindOnce(&ContentProtectionManager::OnContentProtectionApplied,
                     weak_ptr_factory_.GetWeakPtr(),
                     ApplyContentProtectionCallback(), std::nullopt)));

  ToggleDisplaySecurityPolling();
}

void ContentProtectionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  QueueDisplaySecurityQueries();
}

void ContentProtectionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContentProtectionManager::QueryContentProtection(
    ClientId client_id,
    int64_t display_id,
    QueryContentProtectionCallback callback) {
  DCHECK(disabled() || GetContentProtections(client_id));

  if (disabled() || !GetDisplay(display_id)) {
    std::move(callback).Run(/*success=*/false, DISPLAY_CONNECTION_TYPE_NONE,
                            CONTENT_PROTECTION_METHOD_NONE);
    return;
  }

  QueueTask(std::make_unique<QueryContentProtectionTask>(
      layout_manager_, native_display_delegate_, display_id,
      base::BindOnce(&ContentProtectionManager::OnContentProtectionQueried,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     client_id, display_id)));
}

void ContentProtectionManager::ApplyContentProtection(
    ClientId client_id,
    int64_t display_id,
    uint32_t protection_mask,
    ApplyContentProtectionCallback callback) {
  ContentProtections* protections = GetContentProtections(client_id);
  DCHECK(disabled() || protections);

  if (disabled() || !GetDisplay(display_id)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  protections->insert_or_assign(display_id, protection_mask);

  if (HasExternalDisplaysWithContentProtection()) {
    hdcp_key_manager_.SetKeyIfRequired(
        layout_manager_->GetDisplayStates(), display_id,
        base::BindOnce(&ContentProtectionManager::QueueContentProtectionTask,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       client_id));
  } else {
    QueueContentProtectionTask(std::move(callback), client_id,
                               /* is_key_set=*/false);
  }
}

void ContentProtectionManager::QueueContentProtectionTask(
    ApplyContentProtectionCallback callback,
    ClientId client_id,
    bool) {
  QueueTask(std::make_unique<ApplyContentProtectionTask>(
      layout_manager_, native_display_delegate_, AggregateContentProtections(),
      base::BindOnce(&ContentProtectionManager::OnContentProtectionApplied,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     client_id)));

  ToggleDisplaySecurityPolling();
}

const DisplaySnapshot* ContentProtectionManager::GetDisplay(
    int64_t display_id) const {
  for (const DisplaySnapshot* display : layout_manager_->GetDisplayStates())
    if (display->display_id() == display_id)
      return display;

  return nullptr;
}

ContentProtectionManager::ContentProtections
ContentProtectionManager::AggregateContentProtections() const {
  ContentProtections protections;

  for (const auto& requests_pair : requests_)
    for (const auto& protections_pair : requests_pair.second)
      protections[protections_pair.first] |= protections_pair.second;

  return protections;
}

ContentProtectionManager::ContentProtections*
ContentProtectionManager::GetContentProtections(ClientId client_id) {
  if (!client_id)
    return nullptr;

  auto it = requests_.find(*client_id);
  return it == requests_.end() ? nullptr : &it->second;
}

void ContentProtectionManager::QueueTask(std::unique_ptr<Task> task) {
  tasks_.emplace(std::move(task));

  if (tasks_.size() == 1)
    tasks_.front()->Run();
}

void ContentProtectionManager::DequeueTask() {
  DCHECK(!tasks_.empty());
  tasks_.pop();

  if (!tasks_.empty())
    tasks_.front()->Run();
}

void ContentProtectionManager::KillTasks() {
  // Filter out content protection requests for removed displays.
  for (auto& requests : requests_) {
    base::EraseIf(requests.second,
                  [this](const auto& pair) { return !GetDisplay(pair.first); });
  }

  // Fire failure callbacks.
  tasks_ = {};

  ToggleDisplaySecurityPolling();
}

void ContentProtectionManager::OnContentProtectionQueried(
    QueryContentProtectionCallback callback,
    ClientId client_id,
    int64_t display_id,
    Task::Status status,
    uint32_t connection_mask,
    uint32_t protection_mask) {
  // Only run callback if client is still registered.
  if (const auto* protections = GetContentProtections(client_id)) {
    bool success = status == Task::Status::SUCCESS;

    // Conceal protections requested by other clients.
    uint32_t client_mask = CONTENT_PROTECTION_METHOD_NONE;

    if (success) {
      auto it = protections->find(display_id);
      if (it != protections->end())
        client_mask = it->second;
    }

    protection_mask &= client_mask;

    std::move(callback).Run(success, connection_mask, protection_mask);
  }

  if (status != Task::Status::KILLED)
    DequeueTask();
}

void ContentProtectionManager::OnContentProtectionApplied(
    ApplyContentProtectionCallback callback,
    ClientId client_id,
    Task::Status status) {
  // Only run callback if client is still registered.
  if (GetContentProtections(client_id))
    std::move(callback).Run(status == Task::Status::SUCCESS);

  if (status != Task::Status::KILLED)
    DequeueTask();
}

void ContentProtectionManager::OnDisplayConfigurationChanged(
    const DisplayConfigurator::DisplayStateList&) {
  KillTasks();
}

void ContentProtectionManager::OnDisplayConfigurationChangeFailed(
    const DisplayConfigurator::DisplayStateList&,
    MultipleDisplayState) {
  KillTasks();
}

bool ContentProtectionManager::HasExternalDisplaysWithContentProtection()
    const {
  const auto displays = layout_manager_->GetDisplayStates();
  if (base::ranges::all_of(displays, [](const DisplaySnapshot* display) {
        return display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
      })) {
    return false;
  }

  const auto protections = AggregateContentProtections();
  return base::ranges::any_of(protections, [](const auto& pair) {
    return pair.second != CONTENT_PROTECTION_METHOD_NONE;
  });
}

void ContentProtectionManager::ToggleDisplaySecurityPolling() {
  if (HasExternalDisplaysWithContentProtection()) {
    if (!security_timer_.IsRunning()) {
      security_timer_.Start(
          FROM_HERE, kDisplaySecurityPollingPeriod,
          base::BindRepeating(
              &ContentProtectionManager::QueueDisplaySecurityQueries,
              weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    security_timer_.Stop();

    // Query once if polling was stopped or not started. The latter case happens
    // when all displays are internal.
    QueueDisplaySecurityQueries();
  }
}

bool ContentProtectionManager::TriggerDisplaySecurityTimeoutForTesting() {
  if (!security_timer_.IsRunning())
    return false;

  security_timer_.user_task().Run();
  return true;
}

void ContentProtectionManager::QueueDisplaySecurityQueries() {
  if (disabled())
    return;

  for (DisplaySnapshot* display : layout_manager_->GetDisplayStates()) {
    int64_t display_id = display->display_id();

    QueueTask(std::make_unique<QueryContentProtectionTask>(
        layout_manager_, native_display_delegate_, display_id,
        base::BindOnce(&ContentProtectionManager::OnDisplaySecurityQueried,
                       weak_ptr_factory_.GetWeakPtr(), display_id)));
  }
}

void ContentProtectionManager::OnDisplaySecurityQueried(
    int64_t display_id,
    Task::Status status,
    uint32_t connection_mask,
    uint32_t protection_mask) {
  if (GetDisplay(display_id)) {
    // Internal display is secure if not mirrored on unsecure external display.
    const bool secure = status == Task::Status::SUCCESS &&
                        (protection_mask != CONTENT_PROTECTION_METHOD_NONE ||
                         connection_mask == DISPLAY_CONNECTION_TYPE_INTERNAL);

    for (Observer& observer : observers_)
      observer.OnDisplaySecurityMaybeChanged(display_id, secure);
  }

  if (status != Task::Status::KILLED)
    DequeueTask();
}

}  // namespace display
