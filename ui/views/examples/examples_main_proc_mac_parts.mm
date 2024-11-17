// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main_proc_mac_parts.h"

#import <Cocoa/Cocoa.h>

void ExamplesMainProcMacParts() {
  // This allows an unbundled program to get the input focus and have
  // an icon in the Dock.
  ProcessSerialNumber psn = {0, kCurrentProcess};
  TransformProcessType(&psn, kProcessTransformToForegroundApplication);
}
