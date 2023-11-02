// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_STATE_RESTORER_H_
#define UI_GL_GL_STATE_RESTORER_H_

#include "ui/gl/gl_export.h"

namespace gpu {
namespace gles2 {
  class GLES2Decoder;
}  // namespace gles2
}  // namespace gpu

namespace gl {

// An interface for Restoring GL State.
// This will expand over time to provide an more optimizable implementation.
class GL_EXPORT GLStateRestorer {
 public:
  GLStateRestorer();

  GLStateRestorer(const GLStateRestorer&) = delete;
  GLStateRestorer& operator=(const GLStateRestorer&) = delete;

  virtual ~GLStateRestorer();

  virtual bool IsInitialized() = 0;
  virtual void RestoreState(const GLStateRestorer* prev_state) = 0;
  virtual void RestoreAllTextureUnitAndSamplerBindings() = 0;
  virtual void RestoreActiveTexture() = 0;
  virtual void RestoreActiveTextureUnitBinding(unsigned int target) = 0;
  virtual void RestoreAllExternalTextureBindingsIfNeeded() = 0;
  virtual void RestoreFramebufferBindings() = 0;
  virtual void RestoreProgramBindings() = 0;
  virtual void RestoreBufferBinding(unsigned int target) = 0;
  virtual void RestoreVertexAttribArray(unsigned int index) = 0;
  virtual void PauseQueries() = 0;
  virtual void ResumeQueries() = 0;
};

}  // namespace gl

#endif  // UI_GL_GL_STATE_RESTORER_H_
