// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_BINDERS_H_
#define UI_GL_SCOPED_BINDERS_H_

#include "base/macros.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLStateRestorer;

class GL_EXPORT ScopedFramebufferBinder {
 public:
  explicit ScopedFramebufferBinder(unsigned int fbo);
  ~ScopedFramebufferBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_fbo_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFramebufferBinder);
};

class GL_EXPORT ScopedActiveTexture {
 public:
  ScopedActiveTexture(unsigned int texture);
  ~ScopedActiveTexture();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_texture_;

  DISALLOW_COPY_AND_ASSIGN(ScopedActiveTexture);
};

class GL_EXPORT ScopedTextureBinder {
 public:
  ScopedTextureBinder(unsigned int target, unsigned int id);
  ~ScopedTextureBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int target_;
  int old_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureBinder);
};

class GL_EXPORT ScopedUseProgram {
 public:
  ScopedUseProgram(unsigned int program);
  ~ScopedUseProgram();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_program_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseProgram);
};

class GL_EXPORT ScopedVertexAttribArray {
 public:
  ScopedVertexAttribArray(unsigned int index,
                          int size,
                          unsigned int type,
                          char normalized,
                          int stride,
                          const void* pointer);
  ~ScopedVertexAttribArray();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int buffer_;
  int enabled_;
  int index_;
  int size_;
  int type_;
  int normalized_;
  int stride_;
  void* pointer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedVertexAttribArray);
};

class GL_EXPORT ScopedBufferBinder {
 public:
  ScopedBufferBinder(unsigned int target, unsigned int index);
  ~ScopedBufferBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  GLStateRestorer* state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int target_;
  int old_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBufferBinder);
};

class GL_EXPORT ScopedViewport {
 public:
  ScopedViewport(int x, int y, int width, int height);
  ~ScopedViewport();

 private:
  int data_[4] = {};

  DISALLOW_COPY_AND_ASSIGN(ScopedViewport);
};

class GL_EXPORT ScopedVertexAttribPointer {
 public:
  ScopedVertexAttribPointer(unsigned index,
                            int size,
                            unsigned type,
                            char normalized,
                            int stride,
                            const void* pointer);
  ~ScopedVertexAttribPointer();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedVertexAttribPointer);
};

class GL_EXPORT ScopedColorMask {
 public:
  ScopedColorMask(char red, char green, char blue, char alpha);
  ~ScopedColorMask();

 private:
  unsigned char colors_[4] = {};

  DISALLOW_COPY_AND_ASSIGN(ScopedColorMask);
};

class GL_EXPORT ScopedCapability {
 public:
  ScopedCapability(unsigned capability, unsigned char enabled);
  ~ScopedCapability();

 private:
  unsigned capability_;
  unsigned char enabled_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCapability);
};

}  // namespace gl

#endif  // UI_GL_SCOPED_BINDERS_H_
