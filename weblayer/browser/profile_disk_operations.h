// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_
#define WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace weblayer {

struct ProfileInfo {
  ProfileInfo(bool is_incognito,
              const std::string& name,
              const base::FilePath& data_path,
              const base::FilePath& cache_path);
  ProfileInfo();
  ProfileInfo(const ProfileInfo&);
  ProfileInfo& operator=(const ProfileInfo&);
  ~ProfileInfo();

  bool is_incognito = false;
  // The profile name supplied by client code. For non-incognito profiles name
  // can only contain alphanumeric and underscore to be valid.
  std::string name;
  // Path where persistent profile data is stored. This will be empty if
  // icognito.
  base::FilePath data_path;
  // Path where cache profile data is stored. Depending on the OS, this may
  // be the same as |data_path|; the OS may delete data in this directory.
  base::FilePath cache_path;
};

// |name| must be a valid profile name. Ensures that both data and cache path
// directories are created. The paths returned may be different from the name
// to avoid reusing directories that are marked as deleted.
ProfileInfo CreateProfileInfo(const std::string& name, bool is_incognito);

base::FilePath ComputeBrowserPersisterDataBaseDir(const ProfileInfo& info);
void MarkProfileAsDeleted(const ProfileInfo& info);
void TryNukeProfileFromDisk(const ProfileInfo& info);

// Return names of profiles on disk. Invalid profile names are ignored.
// Profiles marked as deleted are ignored.
std::vector<std::string> ListProfileNames();

// This should be called before any |MarkProfileAsDeleted| for a single process
// to avoid races.
void NukeProfilesMarkedForDeletion();

// Functions exposed for testing.
namespace internal {

bool IsValidNameForNonIncognitoProfile(const std::string& name);
std::string CheckDirNameAndExtractName(const std::string& dir_name);
bool IsProfileMarkedForDeletion(const std::string& dir_name);

}  // namespace internal

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_
