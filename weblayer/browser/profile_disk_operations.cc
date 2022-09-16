// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_disk_operations.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/base32/base32.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

// Variables named |name| is a string passed in from the embedder to identify a
// profile. It can only contain alphanumeric and underscore.
//
// Variables named |dir_name| generally refers to the directory name of the
// profile. It may be the |name| exactly, or it may be <name>.<number>, if a
// profile is created with a name matching a profile marked for deletion.
// |dir_name| is an implementation detail of this file and should not be exposed
// as a concept out of this file.

namespace {

// Cannot be part of a valid name. This prevents the client ever specifying a
// name that collides a different one with a suffix.
constexpr char kSuffixDelimiter = '.';

bool IsValidProfileNameChar(char c) {
  return base::IsAsciiDigit(c) || base::IsAsciiAlpha(c) || c == '_';
}

// Return the data path directory to profiles.
base::FilePath GetProfileRootDataDir() {
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  return path.AppendASCII("profiles");
}

base::FilePath GetProfileMarkerRootDataDir() {
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  path = path.AppendASCII("profiles_to_delete");
  base::CreateDirectory(path);
  return path;
}

base::FilePath GetDataPathFromDirName(const std::string& dir_name) {
  return GetProfileRootDataDir().AppendASCII(dir_name.c_str());
}

#if BUILDFLAG(IS_POSIX)
base::FilePath GetCachePathFromDirName(const std::string& dir_name) {
  base::FilePath cache_path;
  CHECK(base::PathService::Get(base::DIR_CACHE, &cache_path));
  cache_path = cache_path.AppendASCII("profiles").AppendASCII(dir_name.c_str());
  return cache_path;
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace

ProfileInfo::ProfileInfo(bool is_incognito,
                         const std::string& name,
                         const base::FilePath& data_path,
                         const base::FilePath& cache_path)
    : is_incognito(is_incognito),
      name(name),
      data_path(data_path),
      cache_path(cache_path) {}

ProfileInfo::ProfileInfo() = default;
ProfileInfo::ProfileInfo(const ProfileInfo&) = default;
ProfileInfo& ProfileInfo::operator=(const ProfileInfo&) = default;
ProfileInfo::~ProfileInfo() = default;

ProfileInfo CreateProfileInfo(const std::string& name, bool is_incognito) {
  if (is_incognito)
    return {is_incognito, name, base::FilePath(), base::FilePath()};

  CHECK(internal::IsValidNameForNonIncognitoProfile(name));
  std::string dir_name = name;
  int suffix = 0;
  while (internal::IsProfileMarkedForDeletion(dir_name)) {
    suffix++;
    dir_name = name;
    dir_name.append(1, kSuffixDelimiter).append(base::NumberToString(suffix));
  }

  base::FilePath data_path = GetDataPathFromDirName(dir_name);
  base::CreateDirectory(data_path);
  base::FilePath cache_path = data_path;
#if BUILDFLAG(IS_POSIX)
  cache_path = GetCachePathFromDirName(dir_name);
  base::CreateDirectory(cache_path);
#endif
  return {is_incognito, name, data_path, cache_path};
}

base::FilePath ComputeBrowserPersisterDataBaseDir(const ProfileInfo& info) {
  base::FilePath base_path;
  if (info.is_incognito) {
    CHECK(base::PathService::Get(DIR_USER_DATA, &base_path));
    if (info.name.empty()) {
      // Originally the Android side of WebLayer only supported a single
      // incognito file with an empty name.
      std::string file_name = "Incognito Restore Data";
      base_path = base_path.AppendASCII(file_name);
    } else {
      std::string file_name = "Named Profile Incognito Restore Data";
      base_path = base_path.AppendASCII(file_name).AppendASCII(
          base32::Base32Encode(info.name));
    }
  } else {
    base_path = info.data_path.AppendASCII("Restore Data");
  }
  return base_path;
}

void MarkProfileAsDeleted(const ProfileInfo& info) {
  if (info.is_incognito)
    return;

  base::FilePath data_root_path = GetProfileRootDataDir();
  base::FilePath marker_path = GetProfileMarkerRootDataDir();
  CHECK(data_root_path.AppendRelativePath(info.data_path, &marker_path));
  base::File file(marker_path,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!base::PathExists(marker_path)) {
    LOG(WARNING) << "Failure in deleting profile data. Profile:" << info.name
                 << " error:" << static_cast<int>(file.error_details());
  }
}

void TryNukeProfileFromDisk(const ProfileInfo& info) {
  if (info.is_incognito) {
    // Incognito. Just delete session data.
    base::DeletePathRecursively(ComputeBrowserPersisterDataBaseDir(info));
    return;
  }

  // This may fail, but that is ok since the marker is not deleted.
  base::DeletePathRecursively(info.data_path);
#if BUILDFLAG(IS_POSIX)
  base::DeletePathRecursively(info.cache_path);
#endif
}

std::vector<std::string> ListProfileNames() {
  base::FilePath root_dir = GetProfileRootDataDir();
  std::vector<std::string> profile_names;
  base::FileEnumerator enumerator(root_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string dir_name = enumerator.GetInfo().GetName().MaybeAsASCII();
    std::string name = internal::CheckDirNameAndExtractName(dir_name);
    if (!name.empty() && !internal::IsProfileMarkedForDeletion(dir_name))
      profile_names.push_back(name);
  }
  return profile_names;
}

void NukeProfilesMarkedForDeletion() {
  base::FilePath marker_root_dir = GetProfileMarkerRootDataDir();
  base::FileEnumerator enumerator(marker_root_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath marker_path = enumerator.Next(); !marker_path.empty();
       marker_path = enumerator.Next()) {
    std::string dir_name = enumerator.GetInfo().GetName().MaybeAsASCII();
    if (!internal::CheckDirNameAndExtractName(dir_name).empty()) {
      // Delete cache and data directory first before deleting marker.
      bool delete_success = true;
#if BUILDFLAG(IS_POSIX)
      delete_success |=
          base::DeletePathRecursively(GetCachePathFromDirName(dir_name));
#endif  // BUILDFLAG(IS_POSIX)
      delete_success |=
          base::DeletePathRecursively(GetDataPathFromDirName(dir_name));
      // Only delete the marker if deletion is successful.
      if (delete_success) {
        base::DeleteFile(marker_path);
      }
    }
  }
}

namespace internal {

bool IsValidNameForNonIncognitoProfile(const std::string& name) {
  for (char c : name) {
    if (!IsValidProfileNameChar(c))
      return false;
  }
  return !name.empty();
}

// If |dir_name| is valid, then return the |name|. Otherwise return the empty
// string.
std::string CheckDirNameAndExtractName(const std::string& dir_name) {
  std::vector<std::string> parts =
      base::SplitString(dir_name, std::string(1, kSuffixDelimiter),
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() == 0 || parts.size() > 2)
    return std::string();

  if (!IsValidNameForNonIncognitoProfile(parts[0]))
    return std::string();

  if (parts.size() > 1) {
    if (parts[1].empty())
      return std::string();

    for (char c : parts[1]) {
      if (!base::IsAsciiDigit(c))
        return std::string();
    }
  }

  return parts[0];
}

bool IsProfileMarkedForDeletion(const std::string& dir_name) {
  base::FilePath marker =
      GetProfileMarkerRootDataDir().AppendASCII(dir_name.c_str());
  return base::PathExists(marker);
}

}  // namespace internal

}  // namespace weblayer
