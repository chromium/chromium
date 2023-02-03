// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PRESENTATION_FEEDBACK_H_
#define UI_GFX_PRESENTATION_FEEDBACK_H_

#include <stdint.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/ca_layer_result.h"

namespace gfx {

// The feedback for gl::GLSurface methods |SwapBuffers|, |SwapBuffersAsync|,
// |SwapBuffersWithBounds|, |PostSubBuffer|, |PostSubBufferAsync|,
// |CommitOverlayPlanes|,|CommitOverlayPlanesAsync|, etc.
struct PresentationFeedback {
  enum Flags {
    // The presentation was synchronized to VSYNC.
    kVSync = 1 << 0,

    // The presentation |timestamp| is converted from hardware clock by driver.
    // Sampling a clock in user space is not acceptable for this flag.
    kHWClock = 1 << 1,

    // The display hardware signalled that it started using the new content. The
    // opposite of this is e.g. a timer being used to guess when the display
    // hardware has switched to the new image content.
    kHWCompletion = 1 << 2,

    // The presentation of this update was done zero-copy. Possible zero-copy
    // cases include direct scanout of a fullscreen surface and a surface on a
    // hardware overlay.
    kZeroCopy = 1 << 3,

    // The presentation of this update failed. |timestamp| is the time of the
    // failure.
    kFailure = 1 << 4,
  };

  PresentationFeedback() = default;
  PresentationFeedback(base::TimeTicks timestamp,
                       base::TimeDelta interval,
                       uint32_t flags)
      : timestamp(timestamp), interval(interval), flags(flags) {}

  static PresentationFeedback Failure() {
    return {base::TimeTicks::Now(), base::TimeDelta(), Flags::kFailure};
  }

  bool failed() const { return !!(flags & Flags::kFailure); }

  // The time when a buffer begins scan-out. If a buffer is never presented on
  // a screen, the |timestamp| will be set to the time of the failure.
  base::TimeTicks timestamp;

  // An estimated interval from the |timestamp| to the next refresh.
  base::TimeDelta interval;

  // A combination of Flags. It indicates the kind of the |timestamp|.
  uint32_t flags = 0;

  // The following are additional timestamps that are reported if available on
  // the underlying platform. If not available, the timestamp is set to 0.

  // A buffer sent to the system compositor or display controller for
  // presentation is returned to chromium's compositor with an out fence for
  // synchronization. This fence indicates when reads from this buffer for
  // presentation (on the GPU or display controller) have been finished and it
  // is safe to write new data to this buffer. Since this fence may not have
  // been signalled when the swap for a new frame is issued, this timestamp is
  // meant to track the latency from when a swap is issued on the GPU thread to
  // when the GPU can start rendering to this buffer.
  base::TimeTicks available_timestamp;

  // The time when the GPU has finished completing all the drawing commands on
  // the primary plane. On Android, SurfaceFlinger does not latch to a buffer
  // until this fence has been signalled.
  base::TimeTicks ready_timestamp;

  // The time when the primary plane is latched by the system compositor for its
  // next rendering update. On Android this corresponds to the SurfaceFlinger
  // latch time.
  base::TimeTicks latch_timestamp;

  // The time when write operations have completed, corresponding to the time
  // when rendering on the GPU finished.
  base::TimeTicks writes_done_timestamp;

#if BUILDFLAG(IS_APPLE)
  gfx::CALayerResult ca_layer_error_code = gfx::kCALayerSuccess;
#endif
};

inline bool operator==(const PresentationFeedback& lhs,
                       const PresentationFeedback& rhs) {
  return lhs.timestamp == rhs.timestamp && lhs.interval == rhs.interval &&
         lhs.flags == rhs.flags &&
         lhs.available_timestamp == rhs.available_timestamp &&
         lhs.ready_timestamp == rhs.ready_timestamp &&
         lhs.latch_timestamp == rhs.latch_timestamp &&
#if BUILDFLAG(IS_APPLE)
         lhs.ca_layer_error_code == rhs.ca_layer_error_code &&
#endif
         lhs.writes_done_timestamp == rhs.writes_done_timestamp;
}

inline bool operator!=(const PresentationFeedback& lhs,
                       const PresentationFeedback& rhs) {
  return !(lhs == rhs);
}

}  // namespace gfx

#endif  // UI_GFX_PRESENTATION_FEEDBACK_H_
