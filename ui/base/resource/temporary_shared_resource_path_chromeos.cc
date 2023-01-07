// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/temporary_shared_resource_path_chromeos.h"

namespace ui {

base::FilePath GetPathForTemporarySharedResourceFile(
    const base::FilePath& shared_resource_path) {
  return base::FilePath(shared_resource_path.value() +
                        FILE_PATH_LITERAL(".temp"));
}

}  // namespace ui
