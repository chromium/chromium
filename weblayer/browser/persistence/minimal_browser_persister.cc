// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistence/minimal_browser_persister.h"

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/persistence/browser_persistence_common.h"
#include "weblayer/browser/tab_impl.h"

using id_type = sessions::SessionCommand::id_type;
using size_type = sessions::SessionCommand::size_type;
using SessionCommands = std::vector<std::unique_ptr<sessions::SessionCommand>>;

namespace weblayer {

namespace {

// Max size used for saving state. Android caps the max at 1MB (although it can
// vary between version and phone). Android does not offer a define for this and
// further if the max is exceeded an exception is thrown. To be on the safe side
// this uses 512k.
constexpr int kMaxSizeInBytes = 512 * 1024;

// Size of the header, in bytes. This is used for versioning.
constexpr int kHeaderSizeInBytes = 4;

// Value used for the header.
constexpr int kHeaderValue = 1;

// This accumulates the SessionCommands needed to restore a Browser and
// ultimately generates a byte array from those commands.
class MinimalPersister {
 public:
  explicit MinimalPersister(int max_size_in_bytes)
      : max_size_in_bytes_(max_size_in_bytes) {}

  MinimalPersister(const MinimalPersister&) = delete;
  MinimalPersister& operator=(const MinimalPersister&) = delete;

  ~MinimalPersister() = default;

  // Convenience for adding a single command.
  bool AppendIfFits(std::unique_ptr<sessions::SessionCommand> command) {
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
    commands.push_back(std::move(command));
    return AppendIfFits(std::move(commands));
  }

  // Returns true if all |commands| were successfully added. A return value of
  // false indicates the max size has been reached and no more commands will be
  // accepted.
  [[nodiscard]] bool AppendIfFits(SessionCommands commands) {
    // The number of commands is written out as |size_type|, make sure the
    // count isn't exceeded.
    const int commands_size = CalculateSizeForCommands(commands);
    if (current_size_ + commands_size > max_size_in_bytes_ ||
        (commands.size() + commands_.size()) >
            std::numeric_limits<size_type>::max()) {
      return false;
    }
    current_size_ += commands_size;
    commands_.insert(commands_.end(), std::make_move_iterator(commands.begin()),
                     std::make_move_iterator(commands.end()));
    return true;
  }

  // Converts the commands to a byte array.
  std::vector<uint8_t> ToByteArray() const {
    std::vector<uint8_t> result(current_size_);
    uint8_t* result_ptr = &result.front();
    const uint32_t header = kHeaderValue;
    memcpy(result_ptr, &header, kHeaderSizeInBytes);
    result_ptr += kHeaderSizeInBytes;

    // Number of commands.
    const size_type num_commands = commands_.size();
    memcpy(result_ptr, &num_commands, sizeof(size_type));
    result_ptr += sizeof(size_type);

    // And the commands.
    for (auto& command : commands_) {
      const size_type total_command_size = command->GetSerializedSize();
      memcpy(result_ptr, &total_command_size, sizeof(size_type));
      result_ptr += sizeof(size_type);

      const id_type command_id = command->id();
      memcpy(result_ptr, &command_id, sizeof(id_type));
      result_ptr += sizeof(id_type);

      const size_type command_size = total_command_size - sizeof(id_type);
      if (command_size > 0) {
        memcpy(result_ptr, command->contents(), command_size);
        result_ptr += command_size;
      }
    }
    DCHECK_EQ(result_ptr - &(result.front()), current_size_);
    return result;
  }

 private:
  int CalculateSizeForCommands(const SessionCommands& commands) const {
    int commands_size = 0;
    for (auto& command : commands)
      commands_size += command->GetSerializedSize();
    // Each command is preceded by it's size.
    return commands_size + commands.size() * sizeof(size_type);
  }

  const int max_size_in_bytes_;
  int current_size_ = kHeaderSizeInBytes + sizeof(size_type);
  SessionCommands commands_;
};

// Used to restore the state created via MinimalPersister.
class MinimalRestorer {
 public:
  explicit MinimalRestorer(const std::vector<uint8_t>& value)
      : value_ptr_(&value.front()), value_ptr_end_(value_ptr_ + value.size()) {}

  MinimalRestorer(const MinimalRestorer&) = delete;
  MinimalRestorer& operator=(const MinimalRestorer&) = delete;
  ~MinimalRestorer() = default;

  // Creates SessionCommands from the previously generated state. An empty
  // vector is returned if there is an error in decoding.
  SessionCommands RestoreCommands() {
    uint32_t header = 0;
    if (!Extract(&header, kHeaderSizeInBytes) || header != kHeaderValue)
      return {};

    size_type num_commands = 0;
    if (!Extract(&num_commands, sizeof(size_type)) || num_commands == 0)
      return {};

    SessionCommands commands;
    for (int i = 0; i < num_commands; ++i) {
      size_type command_size = 0;
      if (!Extract(&command_size, sizeof(size_type)) ||
          !HasAvailable(command_size)) {
        return {};
      }
      id_type command_id = 0;
      if (!Extract(&command_id, sizeof(id_type)))
        return {};
      command_size -= sizeof(id_type);
      std::unique_ptr<sessions::SessionCommand> command =
          std::make_unique<sessions::SessionCommand>(command_id, command_size);
      if (command_size > 0 && !Extract(command->contents(), command_size))
        return {};
      commands.push_back(std::move(command));
    }
    return commands;
  }

 private:
  // If there is |bytes| available to be read, it is copied to |dest| and true
  // is returned.
  bool Extract(void* dest, int bytes) {
    if (!HasAvailable(bytes))
      return false;
    memcpy(dest, value_ptr_, bytes);
    value_ptr_ += bytes;
    return true;
  }

  // Returns true if there are |bytes| more bytes available to read.
  bool HasAvailable(int bytes) const {
    return value_ptr_ + bytes <= value_ptr_end_;
  }

  const uint8_t* value_ptr_;
  const uint8_t* value_ptr_end_;
};

// Iterates over the NavigationEntries of a tab in the order they should be
// written. Starts at the pending NavigationEntry if it exists, and skips over
// a NavigationEntry if it is the initial NavigationEntry.
class NavigationEntryIterator {
 public:
  explicit NavigationEntryIterator(Tab* tab)
      : controller_(
            static_cast<TabImpl*>(tab)->web_contents()->GetController()),
        at_pending_(controller_->GetPendingEntry() != nullptr &&
                    controller_->GetPendingEntryIndex() != -1),
        entry_index_(at_pending_ ? controller_->GetPendingEntryIndex()
                                 : controller_->GetCurrentEntryIndex()) {
    // GetPendingEntryIndex() returns -1 for new entries, which this implicitly
    // skips (Chrome's persistence code does the same).

    if (content::NavigationEntry* current_entry = entry()) {
      // If `entry_index_` points to the initial NavigationEntry, skip it, as
      // initial NavigationEntries should not be persisted.
      if (!at_pending_ && current_entry->IsInitialEntry())
        Next();
    }
  }
  NavigationEntryIterator(const NavigationEntryIterator&) = delete;
  NavigationEntryIterator& operator=(const NavigationEntryIterator&) = delete;
  ~NavigationEntryIterator() = default;

  // Returns the index of the current entry.
  int entry_index() const { return entry_index_; }

  // Returns the current entry.
  content::NavigationEntry* entry() const {
    if (at_pending_)
      return controller_->GetPendingEntry();
    return entry_index_ == -1 ? nullptr
                              : controller_->GetEntryAtIndex(entry_index_);
  }

  // Returns true if the end has been reached.
  bool at_end() const { return !at_pending_ && entry_index_ == -1; }

  // advances to the next entry, returning true if there is one.
  bool Next() {
    if (at_end())
      return false;
    if (at_pending_) {
      at_pending_ = false;
      entry_index_ = controller_->GetCurrentEntryIndex();
      if (entry_index_ == controller_->GetPendingEntryIndex())
        --entry_index_;
    } else if (entry_index_ != -1) {
      --entry_index_;
    }

    // Skip over the initial NavigationEntry as it shouldn't be persisted.
    content::NavigationEntry* entry =
        controller_->GetEntryAtIndex(entry_index_);
    if (entry && entry->IsInitialEntry()) {
      DCHECK_EQ(0, entry_index_);
      entry_index_ = -1;
    }
    return !at_end();
  }

 private:
  const raw_ref<content::NavigationController> controller_;
  bool at_pending_;
  int entry_index_ = -1;
};

// The first pass persists the pending or current entry. Returns true if room
// for more commands, false if size exceeded.
bool PersistTabStatePrimaryPass(const SessionID& browser_session_id,
                                Tab* tab,
                                MinimalPersister* builder) {
  NavigationEntryIterator iterator(tab);
  if (iterator.at_end())
    return true;

  const SessionID& session_id = GetSessionIDForTab(tab);
  BrowserImpl* browser = static_cast<TabImpl*>(tab)->browser();
  auto tabs = browser->GetTabs();
  DCHECK(base::Contains(tabs, tab));
  const int tab_index =
      static_cast<int>(base::ranges::find(tabs, tab) - tabs.begin());
  if (!builder->AppendIfFits(BuildCommandsForTabConfiguration(
          browser_session_id, static_cast<TabImpl*>(tab), tab_index))) {
    return false;
  }

  const sessions::SerializedNavigationEntry serialized_entry =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          iterator.entry_index(), iterator.entry());
  return builder->AppendIfFits(
      CreateUpdateTabNavigationCommand(session_id, serialized_entry));
}

// The second pass persists two more navigations. Returns true if room for more
// commands, false if size exceeded.
bool PersistTabStateSecondaryPass(const SessionID& browser_session_id,
                                  Tab* tab,
                                  int max_navigations_per_tab,
                                  MinimalPersister* builder) {
  NavigationEntryIterator iterator(tab);
  if (iterator.at_end())
    return true;

  const SessionID& session_id = GetSessionIDForTab(tab);
  // Subtract 1 from `max_navigations_per_tab` as the first pass wrote a
  // navigation.
  for (int i = 0; i < max_navigations_per_tab - 1; ++i) {
    // Skips the navigation that was written during the first pass.
    if (!iterator.Next())
      return true;

    const sessions::SerializedNavigationEntry serialized_entry =
        sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
            iterator.entry_index(), iterator.entry());
    if (!builder->AppendIfFits(
            CreateUpdateTabNavigationCommand(session_id, serialized_entry))) {
      return false;
    }
  }
  return true;
}

// Returns the tabs in the order they should be persisted.
std::vector<Tab*> GetTabsInPersistOrder(BrowserImpl* browser) {
  // Move the active tab to be first.
  std::vector<Tab*> tabs = browser->GetTabs();
  Tab* active_tab = browser->GetActiveTab();
  if (tabs.size() <= 1 || !active_tab)
    return tabs;
  base::Erase(tabs, active_tab);
  tabs.insert(tabs.begin(), active_tab);
  return tabs;
}

// Returns the index of active tab.
int GetActiveTabIndex(BrowserImpl* browser) {
  if (!browser->GetActiveTab())
    return -1;
  const std::vector<Tab*>& tabs = browser->GetTabs();
  return static_cast<int>(base::ranges::find(tabs, browser->GetActiveTab()) -
                          tabs.begin());
}

}  // namespace

std::vector<uint8_t> PersistMinimalState(BrowserImpl* browser,
                                         int max_navigations_per_tab,
                                         int max_size_in_bytes) {
  MinimalPersister builder(max_size_in_bytes == 0 ? kMaxSizeInBytes
                                                  : max_size_in_bytes);
  const SessionID browser_session_id = SessionID::NewUnique();
  if (!builder.AppendIfFits(sessions::CreateSetWindowTypeCommand(
          browser_session_id,
          sessions::SessionWindow::WindowType::TYPE_NORMAL))) {
    return {};
  }
  const int active_tab_index = GetActiveTabIndex(browser);
  if (active_tab_index != -1 &&
      !builder.AppendIfFits(sessions::CreateSetSelectedTabInWindowCommand(
          browser_session_id, active_tab_index))) {
    return {};
  }

  // As the size available to write commands is limited, this generates commands
  // in the following order:
  // . active tabs pending navigation entry, if no pending then last committed.
  // . remaining tabs pending navigation entry or last committed if no pending.
  // . active tabs last committed and one navigation before it.
  // . remaining tabs last committed and a limited number of preceding
  //   navigations.
  std::vector<Tab*> tabs = GetTabsInPersistOrder(browser);
  for (Tab* tab : tabs) {
    if (!PersistTabStatePrimaryPass(browser_session_id, tab, &builder))
      return builder.ToByteArray();
  }

  // Use a default of 5 for the max number of navigations to persist.
  if (max_navigations_per_tab == 0)
    max_navigations_per_tab = 5;

  for (Tab* tab : tabs) {
    if (!PersistTabStateSecondaryPass(browser_session_id, tab,
                                      max_navigations_per_tab, &builder)) {
      return builder.ToByteArray();
    }
  }

  return builder.ToByteArray();
}

void RestoreMinimalStateForBrowser(BrowserImpl* browser,
                                   const std::vector<uint8_t>& value) {
  MinimalRestorer restorer(value);
  browser->set_is_minimal_restore_in_progress(true);
  RestoreBrowserState(browser, restorer.RestoreCommands());
  browser->set_is_minimal_restore_in_progress(false);
}

}  // namespace weblayer
