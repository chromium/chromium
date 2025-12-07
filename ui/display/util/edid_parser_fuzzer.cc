// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/gfx/geometry/size.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> edid;
  edid.assign(data, UNSAFE_TODO(data + size));
  // Ctor already parses |edid|, which is what we want here.
  display::EdidParser edid_parser(std::move(edid));
  return 0;
}
