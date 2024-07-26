// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/googletest/custom/gtest/internal/custom/stack_trace_getter.h"

#include <algorithm>
#include <iterator>

#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"

std::string StackTraceGetter::CurrentStackTrace(int max_depth, int skip_count) {
  base::debug::StackTrace stack_trace;

  base::span<const void* const> departure;
  if (stack_trace_upon_leaving_gtest_.has_value()) {
    departure = stack_trace_upon_leaving_gtest_->addresses();
  }

  base::span<const void* const> current = stack_trace.addresses();

  // Ignore the frames at the root of the current trace that match those of the
  // point of departure from GTest. These frames all relate to thread start and
  // test setup, and are irrelevant for diagnosing a failure in a given test.
  // Also ignore the very first mismatch, as this identifies two instructions
  // within the GTest function that called UponLeavingGTest, and is irrelevant
  // as well.
  {
    auto mismatch_pair = std::mismatch(departure.rbegin(), departure.rend(),
                                       current.rbegin(), current.rend());
    if (mismatch_pair.second != current.rend()) {
      current = current.subspan(
          0, current.size() - (mismatch_pair.second - current.rbegin() + 1));
    }
  }

  // Ignore the frames at the leaf of the current trace that match those of the
  // point of departure from GTest. These frames are the call(s) into
  // StackTrace's constructor, which are irrelevant. Also ignore the very first
  // mismatch, as it identifies two instructions within current function.
  {
    auto mismatch_pair = std::mismatch(departure.begin(), departure.end(),
                                       current.begin(), current.end());
    if (mismatch_pair.second != current.end()) {
      auto match_size = (mismatch_pair.second - current.begin()) + 1;
      current = current.subspan(match_size);
    }
  }

  // Ignore frames that the caller wishes to skip.
  if (skip_count >= 0 && static_cast<size_t>(skip_count) < current.size()) {
    current = current.subspan(skip_count);
  }

  // Only return as many as requested.
  if (max_depth >= 0 && static_cast<size_t>(max_depth) < current.size()) {
    current = current.subspan(0, static_cast<size_t>(max_depth));
  }

  return base::debug::StackTrace(current).ToString();
}

void StackTraceGetter::UponLeavingGTest() {
  // Remember the callstack as GTest is left.
  stack_trace_upon_leaving_gtest_.emplace();
}
