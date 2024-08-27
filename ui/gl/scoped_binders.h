// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_BINDERS_H_
#define UI_GL_SCOPED_BINDERS_H_

#include <array>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLStateRestorer;

class GL_EXPORT ScopedFramebufferBinder {
 public:
  explicit ScopedFramebufferBinder(unsigned int fbo);

  ScopedFramebufferBinder(const ScopedFramebufferBinder&) = delete;
  ScopedFramebufferBinder& operator=(const ScopedFramebufferBinder&) = delete;

  ~ScopedFramebufferBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_fbo_;
};

class GL_EXPORT ScopedActiveTexture {
 public:
  ScopedActiveTexture(unsigned int texture);

  ScopedActiveTexture(const ScopedActiveTexture&) = delete;
  ScopedActiveTexture& operator=(const ScopedActiveTexture&) = delete;

  ~ScopedActiveTexture();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_texture_;
};

class GL_EXPORT ScopedTextureBinder {
 public:
  ScopedTextureBinder(unsigned int target, unsigned int id);

  ScopedTextureBinder(const ScopedTextureBinder&) = delete;
  ScopedTextureBinder& operator=(const ScopedTextureBinder&) = delete;

  ~ScopedTextureBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int target_;
  int old_id_;
};

class GL_EXPORT ScopedUseProgram {
 public:
  ScopedUseProgram(unsigned int program);

  ScopedUseProgram(const ScopedUseProgram&) = delete;
  ScopedUseProgram& operator=(const ScopedUseProgram&) = delete;

  ~ScopedUseProgram();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int old_program_;
};

class GL_EXPORT ScopedVertexAttribArray {
 public:
  ScopedVertexAttribArray(unsigned int index,
                          int size,
                          unsigned int type,
                          char normalized,
                          int stride,
                          const void* pointer);

  ScopedVertexAttribArray(const ScopedVertexAttribArray&) = delete;
  ScopedVertexAttribArray& operator=(const ScopedVertexAttribArray&) = delete;

  ~ScopedVertexAttribArray();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int buffer_;
  int enabled_;
  int index_;
  int size_;
  int type_;
  int normalized_;
  int stride_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION void* pointer_;
};

class GL_EXPORT ScopedBufferBinder {
 public:
  ScopedBufferBinder(unsigned int target, unsigned int index);

  ScopedBufferBinder(const ScopedBufferBinder&) = delete;
  ScopedBufferBinder& operator=(const ScopedBufferBinder&) = delete;

  ~ScopedBufferBinder();

 private:
  // Whenever possible we prefer to use the current GLContext's
  // GLStateRestorer to maximize driver compabitility.
  raw_ptr<GLStateRestorer> state_restorer_;

  // Failing that we use GL calls to save and restore state.
  int target_;
  int old_id_;
};

class GL_EXPORT ScopedViewport {
 public:
  ScopedViewport(int x, int y, int width, int height);

  ScopedViewport(const ScopedViewport&) = delete;
  ScopedViewport& operator=(const ScopedViewport&) = delete;

  ~ScopedViewport();

 private:
  std::array<int, 4> data_ = {};
};

class GL_EXPORT ScopedVertexAttribPointer {
 public:
  ScopedVertexAttribPointer(unsigned index,
                            int size,
                            unsigned type,
                            char normalized,
                            int stride,
                            const void* pointer);

  ScopedVertexAttribPointer(const ScopedVertexAttribPointer&) = delete;
  ScopedVertexAttribPointer& operator=(const ScopedVertexAttribPointer&) =
      delete;

  ~ScopedVertexAttribPointer();
};

class GL_EXPORT ScopedColorMask {
 public:
  ScopedColorMask(char red, char green, char blue, char alpha);

  ScopedColorMask(const ScopedColorMask&) = delete;
  ScopedColorMask& operator=(const ScopedColorMask&) = delete;

  ~ScopedColorMask();

 private:
  std::array<unsigned char, 4> colors_ = {};
};

class GL_EXPORT ScopedCapability {
 public:
  ScopedCapability(unsigned capability, unsigned char enabled);

  ScopedCapability(const ScopedCapability&) = delete;
  ScopedCapability& operator=(const ScopedCapability&) = delete;

  ~ScopedCapability();

 private:
  unsigned capability_;
  unsigned char enabled_;
};

}  // namespace gl

#endif  // UI_GL_SCOPED_BINDERS_H_
