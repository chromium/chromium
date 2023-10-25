// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_paths.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#endif

namespace ui {

bool PathProvider(int key, base::FilePath* result) {
  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  base::FilePath cur;
  switch (key) {
#if !BUILDFLAG(IS_IOS)
    // DIR_LOCALES is unsupported on iOS.
    case DIR_LOCALES:
#if BUILDFLAG(IS_ANDROID)
      if (!base::PathService::Get(DIR_RESOURCE_PAKS_ANDROID, &cur))
        return false;
#elif BUILDFLAG(IS_MAC)
      if (!base::PathService::Get(base::DIR_MODULE, &cur))
        return false;
      // On Mac, locale files are in Contents/Resources, a sibling of the
      // App dir.
      cur = cur.DirName();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      if (!base::PathService::Get(base::DIR_ASSETS, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("locales"));
#endif
      create_dir = true;
      break;
#endif  // !BUILDFLAG(IS_IOS)
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case UI_DIR_TEST_DATA:
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("ui"));
      cur = cur.Append(FILE_PATH_LITERAL("base"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur))  // we don't want to create this
        return false;
      break;
#if BUILDFLAG(IS_ANDROID)
    case DIR_RESOURCE_PAKS_ANDROID:
      if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("paks"));
      break;
#endif
    case UI_TEST_PAK:
#if BUILDFLAG(IS_ANDROID)
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
