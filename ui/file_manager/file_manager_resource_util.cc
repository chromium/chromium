// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/file_manager/file_manager_resource_util.h"

#include "base/check.h"
#include "ui/file_manager/grit/file_manager_gen_resources_map.h"
#include "ui/file_manager/grit/file_manager_resources_map.h"

namespace file_manager {

const GritResourceMap* GetFileManagerResources(size_t* size) {
  DCHECK(size);
  *size = kFileManagerResourcesSize;
  return kFileManagerResources;
}

const GritResourceMap* GetFileManagerGenResources(size_t* size) {
  DCHECK(size);
  *size = kFileManagerGenResourcesSize;
  return kFileManagerGenResources;
}

}  // namespace keyboard
