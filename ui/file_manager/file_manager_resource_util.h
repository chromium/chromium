// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_FILE_MANAGER_FILE_MANAGER_RESOURCE_UTIL_H_
#define UI_FILE_MANAGER_FILE_MANAGER_RESOURCE_UTIL_H_

#include <stddef.h>

#include "ui/file_manager/file_manager_export.h"

struct GritResourceMap;

namespace file_manager {

// Get the list of resources. |size| is populated with the number of resources
// in the returned array.
FILE_MANAGER_EXPORT const GritResourceMap* GetFileManagerResources(
    size_t* size);

}  // namespace file_manager

#endif  // UI_FILE_MANAGER_FILE_MANAGER_RESOURCE_UTIL_H_
