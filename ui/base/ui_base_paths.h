// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_PATHS_H_
#define UI_BASE_UI_BASE_PATHS_H_

#include "base/component_export.h"
#include "build/build_config.h"

// This file declares path keys for various special directories. These can be
// used with the PathService to access these directories and files.

namespace ui {

enum {
  PATH_START = 3000,

#if !BUILDFLAG(IS_IOS)
  DIR_LOCALES,  // Directory where locale resources are stored.
#endif

  // Valid only in development environment; TODO(darin): move this
  UI_DIR_TEST_DATA,  // Directory where unit test data resides.

#if BUILDFLAG(IS_ANDROID)
  DIR_RESOURCE_PAKS_ANDROID,
#endif

  UI_TEST_PAK,

  PATH_END
};

// Call once to register the provider for the path keys defined above.
COMPONENT_EXPORT(UI_BASE) void RegisterPathProvider();

}  // namespace ui

#endif  // UI_BASE_UI_BASE_PATHS_H_
