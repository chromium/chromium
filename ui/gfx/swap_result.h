// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWAP_RESULT_H_
#define UI_GFX_SWAP_RESULT_H_

#include "base/time/time.h"

namespace gfx {

enum class SwapResult {
  SWAP_ACK,
  SWAP_FAILED,
  SWAP_NAK_RECREATE_BUFFERS,
  SWAP_RESULT_LAST = SWAP_NAK_RECREATE_BUFFERS,
};

struct SwapTimings {
  // When the GPU service first started processing the SwapBuffers request.
  base::TimeTicks swap_start;

  // On most platforms, this is when the GPU service finished processing the
  // SwapBuffers request. On ChromeOS, this corresponds to the present time.
  // TODO(brianderson): Differentiate the concepts without introducing
  // dicontinuities in associated UMA data.
  base::TimeTicks swap_end;

  bool is_null() const { return swap_start.is_null() && swap_end.is_null(); }
};

// Sent by ImageTransportSurfaces to their clients in response to a SwapBuffers.
struct SwapResponse {
  // The swap's sequence id which helps clients determine which SwapBuffers
  // this corresponds to. We may receive responses out of order on platforms
  // that allow multiple swaps pending if a failed swap returns immediately
  // while a successful swap is still outstanding.
  uint64_t swap_id;

  // Indicates whether the swap succeeded or not.
  SwapResult result;

  // Timing information about the given swap.
  SwapTimings timings;
};

}  // namespace gfx

#endif  // UI_GFX_SWAP_RESULT_H_
