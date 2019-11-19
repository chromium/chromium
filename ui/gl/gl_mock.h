// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements mock GL Interface for unit testing. The interface
// corresponds to the set of functionally distinct GL functions defined in
// generate_bindings.py, which may originate from either desktop GL or GLES.

#ifndef UI_GL_GL_MOCK_H_
#define UI_GL_GL_MOCK_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

using GLFunctionPointerType = void (*)();

class MockGLInterface {
 public:
  MockGLInterface();
  virtual ~MockGLInterface();

  // Set the functions called from the mock GL implementation for the purposes
  // of testing.
  static void SetGLInterface(MockGLInterface* gl_interface);

  // Find an entry point to the mock GL implementation.
  static GLFunctionPointerType GL_BINDING_CALL
  GetGLProcAddress(const char* name);

  // Include the auto-generated parts of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.

  // Member functions
  #include "gl_mock_autogen_gl.h"

  // TODO(zmo): crbug.com/456340
  // Functions that cannot be mocked because they have more than 10 args.
  void CompressedTexSubImage3D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLenum format,
                               GLsizei image_size,
                               const void* data) {
    if (data == nullptr) {
      CompressedTexSubImage3DNoData(target, level, xoffset, yoffset, zoffset,
                                    width, height, depth, format, image_size);
    } else {
      CompressedTexSubImage3DWithData(target, level, xoffset, yoffset, zoffset,
                                      width, height, depth, format, image_size);
    }
  }
  void ClearTexSubImage(GLuint texture,
                        GLint level,
                        GLint xoffset,
                        GLint yoffset,
                        GLint zoffset,
                        GLint width,
                        GLint height,
                        GLint depth,
                        GLenum format,
                        GLenum type,
                        const GLvoid* data) {
    NOTREACHED();
  }
  MOCK_METHOD10(CompressedTexSubImage3DNoData,
                void(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLsizei image_size));
  MOCK_METHOD10(CompressedTexSubImage3DWithData,
                void(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLsizei image_size));

  void CompressedTexSubImage3DRobustANGLE(GLenum /*target*/,
                                          GLint /*level*/,
                                          GLint /*xoffset*/,
                                          GLint /*yoffset*/,
                                          GLint /*zoffset*/,
                                          GLsizei /*width*/,
                                          GLsizei /*height*/,
                                          GLsizei /*depth*/,
                                          GLenum /*format*/,
                                          GLsizei /*imageSize*/,
                                          GLsizei /*dataSize*/,
                                          const void* /*data*/) {
    NOTREACHED();
  }

  void CopySubTextureCHROMIUM(GLuint /*sourceId*/,
                              GLint /*sourceLevel*/,
                              GLenum /*destTarget*/,
                              GLuint /*destId*/,
                              GLint /*destLevel*/,
                              GLint /*xoffset*/,
                              GLint /*yoffset*/,
                              GLint /*x*/,
                              GLint /*y*/,
                              GLsizei /*width*/,
                              GLsizei /*height*/,
                              GLboolean /*unpackFlipY*/,
                              GLboolean /*unpackPremultiplyAlpha*/,
                              GLboolean /*unpackUnmultiplyAlpha*/) {
    NOTREACHED();
  }

  void TexImage3DRobustANGLE(GLenum target,
                             GLint level,
                             GLint internalformat,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLint border,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             const void* pixels) {
    NOTREACHED();
  }

  void TexSubImage3D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
      GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
      const void* pixels) {
    if (pixels == nullptr) {
      TexSubImage3DNoData(target, level, xoffset, yoffset, zoffset,
                          width, height, depth, format, type);
    } else {
      TexSubImage3DWithData(target, level, xoffset, yoffset, zoffset, width,
                            height, depth, format, type);
    }
  }

  void TexSubImage3DRobustANGLE(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint zoffset,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                const void* pixels) {
    NOTREACHED();
  }

  MOCK_METHOD10(TexSubImage3DNoData,
                void(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type));
  MOCK_METHOD10(TexSubImage3DWithData,
                void(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type));

  void ReadPixelsRobustANGLE(GLint /*x*/,
                             GLint /*y*/,
                             GLsizei /*width*/,
                             GLsizei /*height*/,
                             GLenum /*format*/,
                             GLenum /*type*/,
                             GLsizei /*bufSize*/,
                             GLsizei* /*length*/,
                             GLsizei* /*columns*/,
                             GLsizei* /*rows*/,
                             void* /*pixels*/) {
    NOTREACHED();
  }

  void ReadnPixelsRobustANGLE(GLint /*x*/,
                              GLint /*y*/,
                              GLsizei /*width*/,
                              GLsizei /*height*/,
                              GLenum /*format*/,
                              GLenum /*type*/,
                              GLsizei /*bufSize*/,
                              GLsizei* /*length*/,
                              GLsizei* /*columns*/,
                              GLsizei* /*rows*/,
                              void* /*data*/) {
    NOTREACHED();
  }

 private:
  static MockGLInterface* interface_;

  // Static mock functions that invoke the member functions of interface_.
  #include "gl_bindings_autogen_mock.h"

  static void GL_BINDING_CALL Mock_glTexSubImage3DNoData(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
      GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type);
};

}  // namespace gl

#endif  // UI_GL_GL_MOCK_H_
