// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FRAME_DATA_H_
#define UI_GFX_FRAME_DATA_H_

#include <cstdint>

#include "build/build_config.h"
#include "ui/gfx/ca_layer_result.h"

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

  // The HDR headroom of the display that this frame is being swapped to. This
  // is used if tone mapping needs to be baked into overlays.
  float display_hdr_headroom = 1.f;

#if BUILDFLAG(IS_APPLE)
  // The result of CoreAnimation delegated compositing. Value originates from
  // the overlay processor and is used by integration tests to ensure we don't
  // fall out of delegated mode.
  gfx::CALayerResult ca_layer_error_code = gfx::kCALayerSuccess;
#endif
};

}  // namespace gfx

#endif  // UI_GFX_FRAME_DATA_H_
