// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/logging.h"
#include "ui/base/resource/data_pack_with_resource_sharing_lacros.h"

namespace ui {

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
          << "Failed to load shared resource data pack from file. "
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

}  // namespace ui
