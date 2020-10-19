// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/stl_util.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "base/strings/string_util.h"
#endif

namespace l10n_util {

bool IsLocaleSupportedByOS(const std::string& locale) {
  // Return true blindly for now.
  return true;
}

}  // namespace l10n_util
