// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_
#define UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/display/manager/content_protection_key_manager.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DisplayLayoutManager;
class DisplaySnapshot;
class NativeDisplayDelegate;

namespace test {
class ContentProtectionManagerTest;
}  // namespace test

// Fulfills client requests to query and apply per-display or all display
// content protection, and notifies observers of display security changes.
// Changes are detected by polling as required by the kernel API, since
// authentication latency depends on hardware topology, and the hardware may
// temporarily drop authentication, in which case the kernel automatically tries
// to re-establish protection.
class DISPLAY_MANAGER_EXPORT ContentProtectionManager
    : public DisplayConfigurator::Observer {
 public:
  // |connection_mask| is a DisplayConnectionType bitmask, and |protection_mask|
  // is a ContentProtectionMethod bitmask.
  using QueryContentProtectionCallback = base::OnceCallback<
      void(bool success, uint32_t connection_mask, uint32_t protection_mask)>;
  using ApplyContentProtectionCallback = base::OnceCallback<void(bool success)>;

  using ContentProtections =
      base::flat_map<int64_t /* display_id */, uint32_t /* protection_mask */>;

  // Though only run once, a task must outlive its asynchronous operations, so
  // cannot be a OnceCallback.
  struct Task {
    enum class Status { KILLED, FAILURE, SUCCESS };

    virtual ~Task() = default;
    virtual void Run() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called after the secure state of a display has been changed.
    virtual void OnDisplaySecurityMaybeChanged(int64_t display_id,
                                               bool secure) = 0;
  };

  // Returns whether display configuration is disabled, in which case API calls
  // are no-ops resulting in failure callbacks.
  using ConfigurationDisabledCallback = base::RepeatingCallback<bool()>;

  ContentProtectionManager(DisplayLayoutManager*,
                           ConfigurationDisabledCallback);

  ContentProtectionManager(const ContentProtectionManager&) = delete;
  ContentProtectionManager& operator=(const ContentProtectionManager&) = delete;

  ~ContentProtectionManager() override;

  void set_native_display_delegate(NativeDisplayDelegate* delegate) {
    native_display_delegate_ = delegate;
    hdcp_key_manager_.set_native_display_delegate(delegate);
  }

  using ClientId = std::optional<uint64_t>;

  // On display reconfiguration, pending requests are cancelled, i.e. clients
  // receive failure callbacks, and are responsible for renewing requests. If a
  // client unregisters with pending requests, the callbacks are not run.
  ClientId RegisterClient();
  void UnregisterClient(ClientId client_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Queries protection against the client's latest request on the same display,
  // i.e. the result is CONTENT_PROTECTION_METHOD_NONE unless the client has
  // previously applied protection on that display, such that requests from
  // other clients are concealed.
  void QueryContentProtection(ClientId client_id,
                              int64_t display_id,
                              QueryContentProtectionCallback callback);

  // |protection_mask| is a ContentProtectionMethod bitmask. Callback success
  // does not mean that protection is active, but merely that the request went
  // through. The client must periodically query protection status until it no
  // longer requires protection and applies CONTENT_PROTECTION_METHOD_NONE. If
  // protection becomes temporarily unavailable, the client is not required to
  // renew the request, but should keep querying to detect if automatic retries
  // to establish protection are successful.
  void ApplyContentProtection(ClientId client_id,
                              int64_t display_id,
                              uint32_t protection_mask,
                              ApplyContentProtectionCallback callback);

  void SetProvisionedKeyRequest(
      ContentProtectionKeyManager::ProvisionedKeyRequest request) {
    hdcp_key_manager_.set_provisioned_key_request(request);
  }

 private:
  friend class test::ContentProtectionManagerTest;

  bool disabled() const {
    return !native_display_delegate_ || config_disabled_callback_.Run();
  }

  const DisplaySnapshot* GetDisplay(int64_t display_id) const;

  // Returns cumulative content protections given all client requests.
  ContentProtections AggregateContentProtections() const;

  // Returns content protections for |client_id|, or nullptr if invalid.
  ContentProtections* GetContentProtections(ClientId client_id);

  void QueueTask(std::unique_ptr<Task> task);
  void DequeueTask();
  void KillTasks();

  // Called on task completion. Responsible for running the client callback, and
  // dequeuing the next pending task.
  void OnContentProtectionQueried(QueryContentProtectionCallback callback,
                                  ClientId client_id,
                                  int64_t display_id,
                                  Task::Status status,
                                  uint32_t connection_mask,
                                  uint32_t protection_mask);
  void OnContentProtectionApplied(ApplyContentProtectionCallback callback,
                                  ClientId client_id,
                                  Task::Status status);

  // DisplayConfigurator::Observer overrides:
  void OnDisplayConfigurationChanged(
      const DisplayConfigurator::DisplayStateList&) override;
  void OnDisplayConfigurationChangeFailed(
      const DisplayConfigurator::DisplayStateList&,
      MultipleDisplayState) override;

  bool HasExternalDisplaysWithContentProtection() const;

  // Toggles timer for periodic security queries given latest client requests.
  void ToggleDisplaySecurityPolling();

  // Forces timer to fire if running, and returns whether it was running.
  bool TriggerDisplaySecurityTimeoutForTesting();

  // Queries protection status for all displays, and notifies observers whether
  // each display is secure. Called periodically while protection is requested.
  void QueueDisplaySecurityQueries();
  void OnDisplaySecurityQueried(int64_t display_id,
                                Task::Status status,
                                uint32_t connection_mask,
                                uint32_t protection_mask);

  void QueueContentProtectionTask(ApplyContentProtectionCallback callback,
                                  ClientId client_id,
                                  bool is_key_set);

  const raw_ptr<DisplayLayoutManager> layout_manager_;  // Not owned.
  raw_ptr<NativeDisplayDelegate> native_display_delegate_ =
      nullptr;  // Not owned.

  const ConfigurationDisabledCallback config_disabled_callback_;

  uint64_t next_client_id_ = 0;

  // Content protections requested by each client.
  base::flat_map<uint64_t, ContentProtections> requests_;

  // Pending tasks to query or apply content protection.
  base::queue<std::unique_ptr<Task>> tasks_;

  base::ObserverList<Observer> observers_;

  // Used for periodic queries to notify observers of display security changes.
  base::RepeatingTimer security_timer_;

  ContentProtectionKeyManager hdcp_key_manager_;

  base::WeakPtrFactory<ContentProtectionManager> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONTENT_PROTECTION_MANAGER_H_
