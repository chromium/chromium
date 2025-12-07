// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_MAC)
extern "C" {
// Allows the Views Examples.app main() to dynamically invoke this function.
__attribute__((visibility("default"))) int ViewsExamplesMain(int argc,
                                                             char** argv);
}
#endif

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_H_
