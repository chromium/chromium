// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_paths.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#endif

namespace ui {

bool PathProvider(int key, base::FilePath* result) {
  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  base::FilePath cur;
  switch (key) {
    case DIR_LOCALES:
      if (!base::PathService::Get(base::DIR_MODULE, &cur))
        return false;
#if defined(OS_MACOSX)
      // On Mac, locale files are in Contents/Resources, a sibling of the
      // App dir.
      cur = cur.DirName();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#elif defined(OS_ANDROID)
      if (!base::PathService::Get(DIR_RESOURCE_PAKS_ANDROID, &cur))
        return false;
#else
      cur = cur.Append(FILE_PATH_LITERAL("locales"));
#endif
      create_dir = true;
      break;
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case UI_DIR_TEST_DATA:
      if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("ui"));
      cur = cur.Append(FILE_PATH_LITERAL("base"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur))  // we don't want to create this
        return false;
      break;
#if defined(OS_ANDROID)
    case DIR_RESOURCE_PAKS_ANDROID:
      if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("paks"));
      break;
#endif
    case UI_TEST_PAK:
#if defined(OS_ANDROID)
      if (!base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &cur))
        return false;
#else
      if (!base::PathService::Get(base::DIR_ASSETS, &cur))
        return false;
#endif
      cur = cur.AppendASCII("ui_test.pak");
      break;
    default:
      return false;
  }

  if (create_dir && !base::CreateDirectory(cur))
    return false;

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace ui
