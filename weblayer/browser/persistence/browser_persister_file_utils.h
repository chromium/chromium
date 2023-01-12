// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_
#define WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

namespace weblayer {

class ProfileImpl;

// Returns the set of known persistence ids for the profile at |path|.
base::flat_set<std::string> GetBrowserPersistenceIdsOnBackgroundThread(
    const base::FilePath& path);

// Returns the base path to save persistence information. `profile_path` is the
// path of the profile, and `browser_id` the persistence id.
//
// WARNING: persistence code writes more than one file. Historically it wrote
// to the value returned by this. Now it writes to the value returned by this
// with the suffix"_TIMESTAMP", where TIMESTAMP is the time stamp.
base::FilePath BuildBasePathForBrowserPersister(
    const base::FilePath& profile_path,
    const std::string& browser_id);

// Implementation of RemoveBrowserPersistenceStorage(). Tries to remove all
// the persistence files for the set of browser persistence ids.
void RemoveBrowserPersistenceStorageImpl(
    ProfileImpl* profile,
    base::OnceCallback<void(bool)> done_callback,
    base::flat_set<std::string> ids);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_
