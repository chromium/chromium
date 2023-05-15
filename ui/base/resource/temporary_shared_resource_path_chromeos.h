// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_TEMPORARY_SHARED_RESOURCE_PATH_CHROMEOS_H_
#define UI_BASE_RESOURCE_TEMPORARY_SHARED_RESOURCE_PATH_CHROMEOS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ui {

// Return temporary shared resource path name for `shared_resource_path` which
// represents the name of shared resource pak.
// Shared resource pak is renamed to this temporary name on Lacros launch to
// avoid being accessed asynchronously before the verification. It will be moved
// back after it's verified in DataPackWithResourceSharing.
// Note that the temporary shared resource file path might differ between ash
// and lacros if the version is not up to date.
COMPONENT_EXPORT(UI_DATA_PACK)
base::FilePath GetPathForTemporarySharedResourceFile(
    const base::FilePath& shared_resource_path);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_TEMPORARY_SHARED_RESOURCE_PATH_CHROMEOS_H_
