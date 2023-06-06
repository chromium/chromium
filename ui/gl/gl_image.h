// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"

namespace gpu::gles2 {
class GLES2DecoderImpl;
class GLES2DecoderPassthroughImpl;
}  // namespace gpu::gles2

namespace gl {

// Encapsulates an image that can be bound and/or copied to a texture, hiding
// platform specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage(const GLImage&) = delete;
  GLImage& operator=(const GLImage&) = delete;

 protected:
  // NOTE: We are in the process of eliminating client usage of GLImage. As part
  // of this effort, we have moved its public interface to be protected with
  // friend'ing of existing users. DO NOT ADD MORE client usage - instead, reach
  // out to shared-image-team@ with your use case.
  // See crbug.com/1382031.
  GLImage() = default;

  virtual ~GLImage() = default;

 private:
  friend class gpu::gles2::GLES2DecoderImpl;
  friend class gpu::gles2::GLES2DecoderPassthroughImpl;

  friend class base::RefCounted<GLImage>;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_H_
