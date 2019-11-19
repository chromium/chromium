// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/weblayer_paths.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#endif

namespace weblayer {

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;

  switch (key) {
#if defined(OS_ANDROID)
    case weblayer::DIR_CRASH_DUMPS:
      if (!base::android::GetCacheDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crashpad"));
      if (!base::PathExists(cur))
        base::CreateDirectory(cur);
      *result = cur;
      return true;
#endif
    default:
      return false;
  }
}

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace weblayer
