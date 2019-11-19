// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC_H_
#define UI_GL_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence_egl.h"

namespace gl {

class GL_EXPORT GLFenceAndroidNativeFenceSync : public GLFenceEGL {
 public:
  ~GLFenceAndroidNativeFenceSync() override;

  static std::unique_ptr<GLFenceAndroidNativeFenceSync> CreateForGpuFence();

  static std::unique_ptr<GLFenceAndroidNativeFenceSync> CreateFromGpuFence(
      const gfx::GpuFence&);

  std::unique_ptr<gfx::GpuFence> GetGpuFence() override;

  // This is a best effort to get status change time. It might fail and a null
  // TimeTicks will be returned in that case.
  base::TimeTicks GetStatusChangeTime();

  enum Status { kSignaled, kNotSignaled, kInvalid };
  static Status GetStatusChangeTimeForFence(int fd, base::TimeTicks* time);

 private:
  GLFenceAndroidNativeFenceSync();
  static std::unique_ptr<GLFenceAndroidNativeFenceSync> CreateInternal(
      EGLenum type,
      EGLint* attribs);

  DISALLOW_COPY_AND_ASSIGN(GLFenceAndroidNativeFenceSync);
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC_H_
