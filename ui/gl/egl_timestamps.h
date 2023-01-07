// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_EGL_TIMESTAMPS_H_
#define UI_GL_EGL_TIMESTAMPS_H_

#include "base/time/time.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Interface to query EGL timestamps.
class GL_EXPORT EGLTimestampClient {
 public:
  virtual ~EGLTimestampClient() {}

  // Returns whether EGL Timestamps are supported or not.
  virtual bool IsEGLTimestampSupported() const = 0;

  // Returns false if the egl timestamps are pending for the given frame id. If
  // timestamps are pending, it means the frame is not yet done. Also returns
  // the presentation time, composite interval and presentation flags for a
  // frame as out parameters.
  virtual bool GetFrameTimestampInfoIfAvailable(
      base::TimeTicks* presentation_time,
      base::TimeDelta* composite_interval,
      base::TimeTicks* writes_done_time,
      uint32_t* presentation_flags,
      int frame_id) = 0;
};

}  // namespace gl

#endif  // UI_GL_EGL_TIMESTAMPS_H_
