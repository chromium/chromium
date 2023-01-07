// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/googletest/custom/gtest/internal/custom/stack_trace_getter.h"

#include <algorithm>
#include <iterator>

std::string StackTraceGetter::CurrentStackTrace(int max_depth, int skip_count) {
  base::debug::StackTrace stack_trace;

  size_t departure_frame_count = 0;
  const void* const* departure_addresses =
      stack_trace_upon_leaving_gtest_.has_value()
          ? stack_trace_upon_leaving_gtest_->Addresses(&departure_frame_count)
          : nullptr;

  size_t current_frame_count = 0;
  const void* const* current_addresses =
      stack_trace.Addresses(&current_frame_count);

  // Ignore the frames at the root of the current trace that match those of the
  // point of departure from GTest. These frames all relate to thread start and
  // test setup, and are irrelevant for diagnosing a failure in a given test.
  // Also ignore the very first mismatch, as this identifies two instructions
  // within the GTest function that called UponLeavingGTest, and is irrelevant
  // as well.
  {
    auto departure_rbegin =
        std::make_reverse_iterator(departure_addresses + departure_frame_count);
    auto departure_rend = std::make_reverse_iterator(departure_addresses);
    auto current_rbegin =
        std::make_reverse_iterator(current_addresses + current_frame_count);
    auto current_rend = std::make_reverse_iterator(current_addresses);
    auto mismatch_pair = std::mismatch(departure_rbegin, departure_rend,
                                       current_rbegin, current_rend);
    if (mismatch_pair.second != current_rend)
      current_frame_count -= (mismatch_pair.second - current_rbegin) + 1;
  }

  // Ignore the frames at the leaf of the current trace that match those of the
  // point of departure from GTest. These frames are the call(s) into
  // StackTrace's constructor, which are irrelevant. Also ignore the very first
  // mismatch, as it identifies two instructions within current function.
  {
    auto mismatch_pair = std::mismatch(
        departure_addresses, departure_addresses + departure_frame_count,
        current_addresses, current_addresses + current_frame_count);
    if (mismatch_pair.second != current_addresses + current_frame_count) {
      auto match_size = (mismatch_pair.second - current_addresses) + 1;
      current_frame_count -= match_size;
      current_addresses += match_size;
    }
  }

  // Ignore frames that the caller wishes to skip.
  if (skip_count >= 0 &&
      static_cast<size_t>(skip_count) < current_frame_count) {
    current_frame_count -= skip_count;
    current_addresses += skip_count;
  }

  // Only return as many as requested.
  if (max_depth >= 0 && static_cast<size_t>(max_depth) < current_frame_count)
    current_frame_count = static_cast<size_t>(max_depth);

  return base::debug::StackTrace(current_addresses, current_frame_count)
      .ToString();
}

void StackTraceGetter::UponLeavingGTest() {
  // Remember the callstack as GTest is left.
  stack_trace_upon_leaving_gtest_.emplace();
}
