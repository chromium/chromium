// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_WEBLAYER_PATHS_H_
#define WEBLAYER_COMMON_WEBLAYER_PATHS_H_

#include "build/build_config.h"

// This file declares path keys for weblayer.  These can be used with
// the PathService to access various special directories and files.

namespace weblayer {

enum {
  PATH_START = 1000,

  DIR_USER_DATA = PATH_START,  // Directory where user data can be written.

#if BUILDFLAG(IS_ANDROID)
  DIR_CRASH_DUMPS,  // Directory where crash dumps are written.
#endif

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_WEBLAYER_PATHS_H_
