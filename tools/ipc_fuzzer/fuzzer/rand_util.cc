// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"

namespace ipc_fuzzer {

std::mt19937* g_mersenne_twister = nullptr;

void InitRand() {
  g_mersenne_twister =
      new std::mt19937(static_cast<uint32_t>(base::RandUint64()));
}

}  // namespace ipc_fuzzer
