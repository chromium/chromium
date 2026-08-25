// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_JXL_V0_6_WRAPPER_PARALLEL_RUNNER_H_
#define THIRD_PARTY_RUST_JXL_V0_6_WRAPPER_PARALLEL_RUNNER_H_

#include <cstddef>
#include <cstdint>

#include "third_party/rust/cxx/v1/cxx.h"

namespace blink::jxl_rs {

// Alias so the cxx bridge in lib.rs can express `void*` (cxx has no
// built-in `c_void` support, see dtolnay/cxx#1049).
using c_void = void;

// Runs `task(context, index)` for every `index` in [0, `num_tasks`),
// potentially in parallel on the Chromium thread pool. Blocks until all
// tasks have completed; the calling thread participates in running tasks
// while it waits. Falls back to sequential in-place execution when no
// thread pool exists in the process (e.g. some unit test environments).
//
// `task` must be safe to invoke concurrently from multiple threads.
void RunParallelTasks(size_t num_tasks,
                      void* context,
                      rust::Fn<void(void*, size_t)> task);

}  // namespace blink::jxl_rs

#endif  // THIRD_PARTY_RUST_JXL_V0_6_WRAPPER_PARALLEL_RUNNER_H_
