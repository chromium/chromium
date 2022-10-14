// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_EGL_FENCE_UTILS_H_
#define UI_GL_ANDROID_EGL_FENCE_UTILS_H_

#include "base/files/scoped_file.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Create and inserts an egl fence and exports a ScopedFD from it.
GL_EXPORT base::ScopedFD CreateEglFenceAndExportFd();

// Create and insert an EGL fence and imports the provided fence fd.
GL_EXPORT bool InsertEglFenceAndWait(base::ScopedFD acquire_fence_fd);

}  // namespace gl

#endif  // UI_GL_ANDROID_EGL_FENCE_UTILS_H_
