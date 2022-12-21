// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_H_
#define WEBLAYER_PUBLIC_BROWSER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

namespace weblayer {

class BrowserObserver;
class BrowserRestoreObserver;
class Profile;
class Tab;

// Represents an ordered list of Tabs, with one active. Browser does not own
// the set of Tabs.
class Browser {
 public:
  struct PersistenceInfo {
    PersistenceInfo();
    PersistenceInfo(const PersistenceInfo& other);
    ~PersistenceInfo();

    // Uniquely identifies this browser for session restore, empty is not a
    // valid id.
    std::string id;

    // Last key used to encrypt incognito profile.
    std::vector<uint8_t> last_crypto_key;
  };

  // Creates a new Browser. |persistence_info|, if non-null, is used for saving
  // and restoring the state of the browser.
  static std::unique_ptr<Browser> Create(
      Profile* profile,
      const PersistenceInfo* persistence_info);

  virtual ~Browser() {}

  virtual void AddTab(Tab* tab) = 0;
  virtual void DestroyTab(Tab* tab) = 0;
  virtual void SetActiveTab(Tab* tab) = 0;
  virtual Tab* GetActiveTab() = 0;
  virtual std::vector<Tab*> GetTabs() = 0;

  // Creates a tab attached to this browser. The returned tab is owned by the
  // browser.
  virtual Tab* CreateTab() = 0;

  // Called early on in shutdown, before any tabs have been removed.
  virtual void PrepareForShutdown() = 0;

  // Returns the id supplied to Create() that is used for persistence.
  virtual std::string GetPersistenceId() = 0;

  // Returns true if this Browser is in the process of restoring the previous
  // state.
  virtual bool IsRestoringPreviousState() = 0;

  virtual void AddObserver(BrowserObserver* observer) = 0;
  virtual void RemoveObserver(BrowserObserver* observer) = 0;

  virtual void AddBrowserRestoreObserver(BrowserRestoreObserver* observer) = 0;
  virtual void RemoveBrowserRestoreObserver(
      BrowserRestoreObserver* observer) = 0;

  virtual void VisibleSecurityStateOfActiveTabChanged() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_H_
