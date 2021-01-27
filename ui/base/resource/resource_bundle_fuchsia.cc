// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/base_paths.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "ui/gfx/image/image.h"

namespace ui {

void ResourceBundle::LoadCommonResources() {
  constexpr char kCommonResourcesPakPath[] = "common_resources.pak";

  base::FilePath asset_root;
  bool result = base::PathService::Get(base::DIR_ASSETS, &asset_root);
  DCHECK(result);

  AddDataPackFromPath(asset_root.Append(kCommonResourcesPakPath),
                      ui::SCALE_FACTOR_100P);
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

}  // namespace ui
