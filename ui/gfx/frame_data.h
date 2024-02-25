// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FRAME_DATA_H_
#define UI_GFX_FRAME_DATA_H_

#include <cstdint>

namespace gfx {

// Contains per frame data, and is passed along with SwapBuffer, PostSubbuffer,
// CommitOverlayPlanes type methods.
struct FrameData {
  explicit FrameData(int64_t seq = -1) : seq(seq) {}
  ~FrameData() = default;

  // Sequence number for this frame. The reserved value of -1 means that there
  // is no sequence number specified (that is, corresponds to no sequence
  // point). This may happen for some cases, like the ozone demo, tests, or
  // users of GLSurface other than SkiaRenderer.
  int64_t seq = -1;

  // Used to track swap of this frame swap with tracing. The value is taken from
  // |viz::Display::swapped_trace_id|.
  int64_t swap_trace_id = -1;
};

}  // namespace gfx

#endif  // UI_GFX_FRAME_DATA_H_
