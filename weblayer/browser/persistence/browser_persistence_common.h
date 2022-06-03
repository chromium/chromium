// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTENCE_COMMON_H_
#define WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTENCE_COMMON_H_

#include <memory>
#include <vector>

class SessionID;

namespace sessions {
class SessionCommand;
}

// Common functions used in persisting/restoring the state (tabs, navigations)
// of a Browser.
namespace weblayer {

class BrowserImpl;
class Tab;
class TabImpl;

// Restores browser state from |commands|. This ensures |browser| contains at
// least one tab when done.
void RestoreBrowserState(
    BrowserImpl* browser,
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands);

// Creates and returns the minimal set of SessionCommands to configure a tab.
// This does not include any navigations.
std::vector<std::unique_ptr<sessions::SessionCommand>>
BuildCommandsForTabConfiguration(const SessionID& browser_session_id,
                                 TabImpl* tab,
                                 int index_in_browser);

// Convenience to return the SessionID for a Tab.
const SessionID& GetSessionIDForTab(Tab* tab);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTENCE_COMMON_H_
