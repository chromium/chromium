// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/test_fonts/fontconfig/fontconfig_util_linux.h"

#include <fontconfig/fontconfig.h>
#include <libgen.h>
#include <unistd.h>

#include <cassert>
#include <climits>
#include <cstdlib>
#include <string>

namespace test_fonts {

std::string GetSysrootDir() {
  char buf[PATH_MAX + 1];
  auto count = readlink("/proc/self/exe", buf, PATH_MAX);
  assert(count > 0);
  buf[count] = '\0';
  return dirname(buf);
}

void SetUpFontconfig() {
  auto sysroot = GetSysrootDir();
  setenv("FONTCONFIG_SYSROOT", sysroot.c_str(), 1);
}

}  // namespace test_fonts
