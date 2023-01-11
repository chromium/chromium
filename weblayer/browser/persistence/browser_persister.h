// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_H_
#define WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "components/sessions/content/session_tab_helper_delegate.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/session_service_commands.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser_observer.h"

class SessionID;

namespace sessions {
class SessionCommand;
}

namespace weblayer {

class BrowserImpl;

// BrowserPersister is responsible for maintaining the state of tabs in a
// single Browser so that they can be restored at a later date. The state is
// written to a file. To avoid having to write the complete state anytime
// something changes interesting events (represented as SessionCommands) are
// written to disk. To restore, the events are read back and the state
// recreated. At certain times the file is truncated and rebuilt from the
// current state.
class BrowserPersister : public sessions::CommandStorageManagerDelegate,
                         public sessions::SessionTabHelperDelegate,
                         public BrowserObserver,
                         public TabImpl::DataObserver {
 public:
  BrowserPersister(const base::FilePath& path, BrowserImpl* browser);

  BrowserPersister(const BrowserPersister&) = delete;
  BrowserPersister& operator=(const BrowserPersister&) = delete;

  ~BrowserPersister() override;

  bool is_restore_in_progress() const { return is_restore_in_progress_; }

  void SaveIfNecessary();

 private:
  friend class BrowserPersisterTestHelper;

  using IdToRange = std::map<SessionID, std::pair<int, int>>;

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;
  void OnErrorWritingSessionCommands() override;

  // BrowserObserver;
  void OnTabAdded(Tab* tab) override;
  void OnTabRemoved(Tab* tab, bool active_tab_changed) override;
  void OnActiveTabChanged(Tab* tab) override;

  // TabImpl::DataObserver:
  void OnDataChanged(TabImpl* tab,
                     const std::map<std::string, std::string>& data) override;

  // sessions::SessionTabHelperDelegate:
  void SetTabUserAgentOverride(const SessionID& window_id,
                               const SessionID& tab_id,
                               const sessions::SerializedUserAgentOverride&
                                   user_agent_override) override;
  void SetSelectedNavigationIndex(const SessionID& window_id,
                                  const SessionID& tab_id,
                                  int index) override;
  void UpdateTabNavigation(
      const SessionID& window_id,
      const SessionID& tab_id,
      const sessions::SerializedNavigationEntry& navigation) override;
  void TabNavigationPathPruned(const SessionID& window_id,
                               const SessionID& tab_id,
                               int index,
                               int count) override;
  void TabNavigationPathEntriesDeleted(const SessionID& window_id,
                                       const SessionID& tab_id) override;

  // Schedules recreating the file on the next save.
  void ScheduleRebuildOnNextSave();

  // Called with the contents of the previous session.
  void OnGotLastSessionCommands(
      std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
      bool read_error);

  // Schedules commands to recreate the state of the specified tab.
  void BuildCommandsForTab(TabImpl* tab, int index_in_window);

  // Schedules commands to recreate the state of |browser_|.
  void BuildCommandsForBrowser();

  // Schedules the specified command.
  void ScheduleCommand(std::unique_ptr<sessions::SessionCommand> command);

  void ProcessRestoreCommands(
      const std::vector<std::unique_ptr<sessions::SessionWindow>>& windows);

  raw_ptr<BrowserImpl> browser_;

  // ID used for the browser. The sessions code requires each tab to be
  // associated with a browser.
  const SessionID browser_session_id_;

  std::unique_ptr<sessions::CommandStorageManager> command_storage_manager_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  IdToRange tab_to_available_range_;

  // Force session commands to be rebuild before next save event.
  bool rebuild_on_next_save_;

  base::ScopedMultiSourceObservation<TabImpl, TabImpl::DataObserver>
      data_observations_{this};

  // True while asynchronously reading the state to restore.
  bool is_restore_in_progress_ = true;

  base::WeakPtrFactory<BrowserPersister> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_H_
