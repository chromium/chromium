// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/lacros/lacros_paths.h"
#include "ui/base/resource/data_pack_with_resource_sharing_lacros.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

namespace {

constexpr char k100PercentPack[] = "chrome_100_percent.pak";
constexpr char k200PercentPack[] = "chrome_200_percent.pak";

base::FilePath GetResourcesPakFilePath(const std::string& pak_name) {
  base::FilePath path;
  if (base::PathService::Get(base::DIR_ASSETS, &path))
    return path.AppendASCII(pak_name.c_str());

  // Return just the name of the pak file.
  return base::FilePath(pak_name.c_str());
}

base::FilePath GetGeneratedFilePath(const std::string& pak_name) {
  base::FilePath path;
  if (base::PathService::Get(chromeos::lacros_paths::USER_DATA_DIR, &path))
    return path.AppendASCII(pak_name.c_str());

  // Return just the name of the pak file.
  return base::FilePath(pak_name.c_str());
}

}  // namespace

void ResourceBundle::AddDataPackFromPathWithAshResourcesInternal(
    const base::FilePath& shared_resource_path,
    const base::FilePath& ash_path,
    const base::FilePath& lacros_path,
    ResourceScaleFactor scale_factor,
    bool optional) {
  auto data_pack = std::make_unique<DataPackWithResourceSharing>(scale_factor);
  if (data_pack->LoadFromPathWithAshResource(shared_resource_path, ash_path)) {
    AddResourceHandle(std::move(data_pack));
  } else {
    auto data_pack_with_lacros_resource =
        std::make_unique<DataPack>(scale_factor);
    if (data_pack_with_lacros_resource->LoadFromPath(lacros_path)) {
      LOG(WARNING)
          << "Failed to load shared resource data pack " << shared_resource_path
          << ": "
          << "Use lacros resource data pack instead of shared resource "
          << "data pack.";
      AddResourceHandle(std::move(data_pack_with_lacros_resource));
    } else if (!optional) {
      LOG(ERROR) << "Failed to load data pack from file."
                 << "\nSome features are not available.";
    }
  }
}

void ResourceBundle::AddDataPackFromPathWithAshResources(
    const base::FilePath& shared_resource_path,
    const base::FilePath& ash_path,
    const base::FilePath& lacros_path,
    ResourceScaleFactor scale_factor) {
  AddDataPackFromPathWithAshResourcesInternal(shared_resource_path, ash_path,
                                              lacros_path, scale_factor, false);
}

void ResourceBundle::AddOptionalDataPackFromPathWithAshResources(
    const base::FilePath& shared_resource_path,
    const base::FilePath& ash_path,
    const base::FilePath& lacros_path,
    ResourceScaleFactor scale_factor) {
  AddDataPackFromPathWithAshResourcesInternal(shared_resource_path, ash_path,
                                              lacros_path, scale_factor, true);
}

void ResourceBundle::LoadChromeResources() {
  // Always load the 1x data pack first as the 2x data pack contains both 1x and
  // 2x images. The 1x data pack only has 1x images, thus passes in an accurate
  // scale factor to gfx::ImageSkia::AddRepresentation.

  base::FilePath ash_resources_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableResourcesFileSharing) ||
      !base::PathService::Get(chromeos::lacros_paths::ASH_RESOURCES_DIR,
                              &ash_resources_dir)) {
    // If resource sharing feature is not enabled or ash resources dir path is
    // not available, use DataPack instead.
    if (IsScaleFactorSupported(k100Percent)) {
      AddDataPackFromPath(GetResourcesPakFilePath(k100PercentPack),
                          k100Percent);
    }

    if (IsScaleFactorSupported(k200Percent)) {
      AddOptionalDataPackFromPath(GetResourcesPakFilePath(k200PercentPack),
                                  k200Percent);
    }
    return;
  }

  if (IsScaleFactorSupported(k100Percent)) {
    AddDataPackFromPathWithAshResources(
        GetGeneratedFilePath(crosapi::kSharedChrome100PercentPackName),
        ash_resources_dir.Append(k100PercentPack),
        GetResourcesPakFilePath(k200PercentPack), k100Percent);
  }

  if (IsScaleFactorSupported(k200Percent)) {
    AddOptionalDataPackFromPathWithAshResources(
        GetGeneratedFilePath(crosapi::kSharedChrome200PercentPackName),
        ash_resources_dir.Append(k200PercentPack),
        GetResourcesPakFilePath(k200PercentPack), k200Percent);
  }
}

}  // namespace ui
