// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

namespace ui {

void ResourceBundle::LoadCommonResources() {
  LoadChromeResources();
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

}  // namespace ui
