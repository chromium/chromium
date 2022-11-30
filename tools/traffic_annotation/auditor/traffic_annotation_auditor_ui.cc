// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
  printf(
      "traffic_annotation_auditor has been removed from the codebase. Please "
      "use auditor.py instead.\n"
      "See tools/traffic_annotation/scripts/auditor/README.md for "
      "instructions.\n");
  return 1;
}
