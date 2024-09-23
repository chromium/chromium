// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string.h>

#include "base/notreached.h"
#include "ui/gl/gl_mock.h"

namespace {
// This is called mainly to prevent the compiler combining the code of mock
// functions with identical contents, so that their function pointers will be
// different.
void MakeGlMockFunctionUnique(const char* func_name) {
  VLOG(2) << "Calling mock " << func_name;
}
}  // namespace

namespace gl {

void GL_BINDING_CALL
MockGLInterface::Mock_glAcquireTexturesANGLE(GLuint numTextures,
                                             const GLuint* textures,
                                             const GLenum* layouts) {
  MakeGlMockFunctionUnique("glAcquireTexturesANGLE");
  interface_->AcquireTexturesANGLE(numTextures, textures, layouts);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glActiveShaderProgram(GLuint pipeline, GLuint program) {
  MakeGlMockFunctionUnique("glActiveShaderProgram");
  interface_->ActiveShaderProgram(pipeline, program);
}

void GL_BINDING_CALL MockGLInterface::Mock_glActiveTexture(GLenum texture) {
  MakeGlMockFunctionUnique("glActiveTexture");
  interface_->ActiveTexture(texture);
}

void GL_BINDING_CALL MockGLInterface::Mock_glAttachShader(GLuint program,
                                                          GLuint shader) {
  MakeGlMockFunctionUnique("glAttachShader");
  interface_->AttachShader(program, shader);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBeginPixelLocalStorageANGLE(GLsizei n,
                                                    const GLenum* loadops) {
  MakeGlMockFunctionUnique("glBeginPixelLocalStorageANGLE");
  interface_->BeginPixelLocalStorageANGLE(n, loadops);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBeginQuery(GLenum target,
                                                        GLuint id) {
  MakeGlMockFunctionUnique("glBeginQuery");
  interface_->BeginQuery(target, id);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBeginQueryEXT(GLenum target,
                                                           GLuint id) {
  MakeGlMockFunctionUnique("glBeginQueryEXT");
  interface_->BeginQuery(target, id);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBeginTransformFeedback(GLenum primitiveMode) {
  MakeGlMockFunctionUnique("glBeginTransformFeedback");
  interface_->BeginTransformFeedback(primitiveMode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindAttribLocation(GLuint program,
                                           GLuint index,
                                           const char* name) {
  MakeGlMockFunctionUnique("glBindAttribLocation");
  interface_->BindAttribLocation(program, index, name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindBuffer(GLenum target,
                                                        GLuint buffer) {
  MakeGlMockFunctionUnique("glBindBuffer");
  interface_->BindBuffer(target, buffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindBufferBase(GLenum target,
                                                            GLuint index,
                                                            GLuint buffer) {
  MakeGlMockFunctionUnique("glBindBufferBase");
  interface_->BindBufferBase(target, index, buffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindBufferRange(GLenum target,
                                                             GLuint index,
                                                             GLuint buffer,
                                                             GLintptr offset,
                                                             GLsizeiptr size) {
  MakeGlMockFunctionUnique("glBindBufferRange");
  interface_->BindBufferRange(target, index, buffer, offset, size);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindFragDataLocationEXT(GLuint program,
                                                GLuint colorNumber,
                                                const char* name) {
  MakeGlMockFunctionUnique("glBindFragDataLocationEXT");
  interface_->BindFragDataLocation(program, colorNumber, name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindFragDataLocationIndexedEXT(GLuint program,
                                                       GLuint colorNumber,
                                                       GLuint index,
                                                       const char* name) {
  MakeGlMockFunctionUnique("glBindFragDataLocationIndexedEXT");
  interface_->BindFragDataLocationIndexed(program, colorNumber, index, name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindFramebuffer(GLenum target, GLuint framebuffer) {
  MakeGlMockFunctionUnique("glBindFramebuffer");
  interface_->BindFramebufferEXT(target, framebuffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindImageTexture(GLuint index,
                                                              GLuint texture,
                                                              GLint level,
                                                              GLboolean layered,
                                                              GLint layer,
                                                              GLenum access,
                                                              GLint format) {
  MakeGlMockFunctionUnique("glBindImageTexture");
  interface_->BindImageTextureEXT(index, texture, level, layered, layer, access,
                                  format);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindImageTextureEXT(GLuint index,
                                            GLuint texture,
                                            GLint level,
                                            GLboolean layered,
                                            GLint layer,
                                            GLenum access,
                                            GLint format) {
  MakeGlMockFunctionUnique("glBindImageTextureEXT");
  interface_->BindImageTextureEXT(index, texture, level, layered, layer, access,
                                  format);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindProgramPipeline(GLuint pipeline) {
  MakeGlMockFunctionUnique("glBindProgramPipeline");
  interface_->BindProgramPipeline(pipeline);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  MakeGlMockFunctionUnique("glBindRenderbuffer");
  interface_->BindRenderbufferEXT(target, renderbuffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindSampler(GLuint unit,
                                                         GLuint sampler) {
  MakeGlMockFunctionUnique("glBindSampler");
  interface_->BindSampler(unit, sampler);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindTexture(GLenum target,
                                                         GLuint texture) {
  MakeGlMockFunctionUnique("glBindTexture");
  interface_->BindTexture(target, texture);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindTransformFeedback(GLenum target, GLuint id) {
  MakeGlMockFunctionUnique("glBindTransformFeedback");
  interface_->BindTransformFeedback(target, id);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindUniformLocationCHROMIUM(GLuint program,
                                                    GLint location,
                                                    const char* name) {
  MakeGlMockFunctionUnique("glBindUniformLocationCHROMIUM");
  interface_->BindUniformLocationCHROMIUM(program, location, name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindVertexArray(GLuint array) {
  MakeGlMockFunctionUnique("glBindVertexArray");
  interface_->BindVertexArrayOES(array);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBindVertexArrayOES(GLuint array) {
  MakeGlMockFunctionUnique("glBindVertexArrayOES");
  interface_->BindVertexArrayOES(array);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBindVertexBuffer(GLuint bindingindex,
                                         GLuint buffer,
                                         GLintptr offset,
                                         GLsizei stride) {
  MakeGlMockFunctionUnique("glBindVertexBuffer");
  interface_->BindVertexBuffer(bindingindex, buffer, offset, stride);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendBarrierKHR(void) {
  MakeGlMockFunctionUnique("glBlendBarrierKHR");
  interface_->BlendBarrierKHR();
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendBarrierNV(void) {
  MakeGlMockFunctionUnique("glBlendBarrierNV");
  interface_->BlendBarrierKHR();
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendColor(GLclampf red,
                                                        GLclampf green,
                                                        GLclampf blue,
                                                        GLclampf alpha) {
  MakeGlMockFunctionUnique("glBlendColor");
  interface_->BlendColor(red, green, blue, alpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendEquation(GLenum mode) {
  MakeGlMockFunctionUnique("glBlendEquation");
  interface_->BlendEquation(mode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendEquationSeparate(GLenum modeRGB,
                                              GLenum modeAlpha) {
  MakeGlMockFunctionUnique("glBlendEquationSeparate");
  interface_->BlendEquationSeparate(modeRGB, modeAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendEquationSeparatei(GLuint buf,
                                               GLenum modeRGB,
                                               GLenum modeAlpha) {
  MakeGlMockFunctionUnique("glBlendEquationSeparatei");
  interface_->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendEquationSeparateiOES(GLuint buf,
                                                  GLenum modeRGB,
                                                  GLenum modeAlpha) {
  MakeGlMockFunctionUnique("glBlendEquationSeparateiOES");
  interface_->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendEquationi(GLuint buf,
                                                            GLenum mode) {
  MakeGlMockFunctionUnique("glBlendEquationi");
  interface_->BlendEquationiOES(buf, mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendEquationiOES(GLuint buf,
                                                               GLenum mode) {
  MakeGlMockFunctionUnique("glBlendEquationiOES");
  interface_->BlendEquationiOES(buf, mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendFunc(GLenum sfactor,
                                                       GLenum dfactor) {
  MakeGlMockFunctionUnique("glBlendFunc");
  interface_->BlendFunc(sfactor, dfactor);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendFuncSeparate(GLenum srcRGB,
                                          GLenum dstRGB,
                                          GLenum srcAlpha,
                                          GLenum dstAlpha) {
  MakeGlMockFunctionUnique("glBlendFuncSeparate");
  interface_->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendFuncSeparatei(GLuint buf,
                                           GLenum srcRGB,
                                           GLenum dstRGB,
                                           GLenum srcAlpha,
                                           GLenum dstAlpha) {
  MakeGlMockFunctionUnique("glBlendFuncSeparatei");
  interface_->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlendFuncSeparateiOES(GLuint buf,
                                              GLenum srcRGB,
                                              GLenum dstRGB,
                                              GLenum srcAlpha,
                                              GLenum dstAlpha) {
  MakeGlMockFunctionUnique("glBlendFuncSeparateiOES");
  interface_->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendFunci(GLuint buf,
                                                        GLenum sfactor,
                                                        GLenum dfactor) {
  MakeGlMockFunctionUnique("glBlendFunci");
  interface_->BlendFunciOES(buf, sfactor, dfactor);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlendFunciOES(GLuint buf,
                                                           GLenum sfactor,
                                                           GLenum dfactor) {
  MakeGlMockFunctionUnique("glBlendFunciOES");
  interface_->BlendFunciOES(buf, sfactor, dfactor);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlitFramebuffer(GLint srcX0,
                                                             GLint srcY0,
                                                             GLint srcX1,
                                                             GLint srcY1,
                                                             GLint dstX0,
                                                             GLint dstY0,
                                                             GLint dstX1,
                                                             GLint dstY1,
                                                             GLbitfield mask,
                                                             GLenum filter) {
  MakeGlMockFunctionUnique("glBlitFramebuffer");
  interface_->BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                              dstY1, mask, filter);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glBlitFramebufferANGLE(GLint srcX0,
                                             GLint srcY0,
                                             GLint srcX1,
                                             GLint srcY1,
                                             GLint dstX0,
                                             GLint dstY0,
                                             GLint dstX1,
                                             GLint dstY1,
                                             GLbitfield mask,
                                             GLenum filter) {
  MakeGlMockFunctionUnique("glBlitFramebufferANGLE");
  interface_->BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                              dstY1, mask, filter);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBlitFramebufferNV(GLint srcX0,
                                                               GLint srcY0,
                                                               GLint srcX1,
                                                               GLint srcY1,
                                                               GLint dstX0,
                                                               GLint dstY0,
                                                               GLint dstX1,
                                                               GLint dstY1,
                                                               GLbitfield mask,
                                                               GLenum filter) {
  MakeGlMockFunctionUnique("glBlitFramebufferNV");
  interface_->BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                              dstY1, mask, filter);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBufferData(GLenum target,
                                                        GLsizeiptr size,
                                                        const void* data,
                                                        GLenum usage) {
  MakeGlMockFunctionUnique("glBufferData");
  interface_->BufferData(target, size, data, usage);
}

void GL_BINDING_CALL MockGLInterface::Mock_glBufferSubData(GLenum target,
                                                           GLintptr offset,
                                                           GLsizeiptr size,
                                                           const void* data) {
  MakeGlMockFunctionUnique("glBufferSubData");
  interface_->BufferSubData(target, offset, size, data);
}

GLenum GL_BINDING_CALL
MockGLInterface::Mock_glCheckFramebufferStatus(GLenum target) {
  MakeGlMockFunctionUnique("glCheckFramebufferStatus");
  return interface_->CheckFramebufferStatusEXT(target);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClear(GLbitfield mask) {
  MakeGlMockFunctionUnique("glClear");
  interface_->Clear(mask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearBufferfi(GLenum buffer,
                                                           GLint drawbuffer,
                                                           const GLfloat depth,
                                                           GLint stencil) {
  MakeGlMockFunctionUnique("glClearBufferfi");
  interface_->ClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glClearBufferfv(GLenum buffer,
                                      GLint drawbuffer,
                                      const GLfloat* value) {
  MakeGlMockFunctionUnique("glClearBufferfv");
  interface_->ClearBufferfv(buffer, drawbuffer, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearBufferiv(GLenum buffer,
                                                           GLint drawbuffer,
                                                           const GLint* value) {
  MakeGlMockFunctionUnique("glClearBufferiv");
  interface_->ClearBufferiv(buffer, drawbuffer, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glClearBufferuiv(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLuint* value) {
  MakeGlMockFunctionUnique("glClearBufferuiv");
  interface_->ClearBufferuiv(buffer, drawbuffer, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearColor(GLclampf red,
                                                        GLclampf green,
                                                        GLclampf blue,
                                                        GLclampf alpha) {
  MakeGlMockFunctionUnique("glClearColor");
  interface_->ClearColor(red, green, blue, alpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearDepth(GLclampd depth) {
  MakeGlMockFunctionUnique("glClearDepth");
  interface_->ClearDepth(depth);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearDepthf(GLclampf depth) {
  MakeGlMockFunctionUnique("glClearDepthf");
  interface_->ClearDepthf(depth);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClearStencil(GLint s) {
  MakeGlMockFunctionUnique("glClearStencil");
  interface_->ClearStencil(s);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glClearTexImageEXT(GLuint texture,
                                         GLint level,
                                         GLenum format,
                                         GLenum type,
                                         const GLvoid* data) {
  MakeGlMockFunctionUnique("glClearTexImageEXT");
  interface_->ClearTexImage(texture, level, format, type, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glClearTexSubImage(GLuint texture,
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
  MakeGlMockFunctionUnique("glClearTexSubImage");
  interface_->ClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glClearTexSubImageEXT(GLuint texture,
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
  MakeGlMockFunctionUnique("glClearTexSubImageEXT");
  interface_->ClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type, data);
}

GLenum GL_BINDING_CALL
MockGLInterface::Mock_glClientWaitSync(GLsync sync,
                                       GLbitfield flags,
                                       GLuint64 timeout) {
  MakeGlMockFunctionUnique("glClientWaitSync");
  return interface_->ClientWaitSync(sync, flags, timeout);
}

void GL_BINDING_CALL MockGLInterface::Mock_glClipControlEXT(GLenum origin,
                                                            GLenum depth) {
  MakeGlMockFunctionUnique("glClipControlEXT");
  interface_->ClipControlEXT(origin, depth);
}

void GL_BINDING_CALL MockGLInterface::Mock_glColorMask(GLboolean red,
                                                       GLboolean green,
                                                       GLboolean blue,
                                                       GLboolean alpha) {
  MakeGlMockFunctionUnique("glColorMask");
  interface_->ColorMask(red, green, blue, alpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glColorMaski(GLuint buf,
                                                        GLboolean red,
                                                        GLboolean green,
                                                        GLboolean blue,
                                                        GLboolean alpha) {
  MakeGlMockFunctionUnique("glColorMaski");
  interface_->ColorMaskiOES(buf, red, green, blue, alpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glColorMaskiOES(GLuint buf,
                                                           GLboolean red,
                                                           GLboolean green,
                                                           GLboolean blue,
                                                           GLboolean alpha) {
  MakeGlMockFunctionUnique("glColorMaskiOES");
  interface_->ColorMaskiOES(buf, red, green, blue, alpha);
}

void GL_BINDING_CALL MockGLInterface::Mock_glCompileShader(GLuint shader) {
  MakeGlMockFunctionUnique("glCompileShader");
  interface_->CompileShader(shader);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexImage2D(GLenum target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLint border,
                                             GLsizei imageSize,
                                             const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexImage2D");
  interface_->CompressedTexImage2D(target, level, internalformat, width, height,
                                   border, imageSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexImage2DRobustANGLE(GLenum target,
                                                        GLint level,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLint border,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexImage2DRobustANGLE");
  interface_->CompressedTexImage2DRobustANGLE(target, level, internalformat,
                                              width, height, border, imageSize,
                                              dataSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexImage3D(GLenum target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLint border,
                                             GLsizei imageSize,
                                             const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexImage3D");
  interface_->CompressedTexImage3D(target, level, internalformat, width, height,
                                   depth, border, imageSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexImage3DRobustANGLE(GLenum target,
                                                        GLint level,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        GLint border,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexImage3DRobustANGLE");
  interface_->CompressedTexImage3DRobustANGLE(target, level, internalformat,
                                              width, height, depth, border,
                                              imageSize, dataSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexSubImage2D(GLenum target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLsizei imageSize,
                                                const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexSubImage2D");
  interface_->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                      height, format, imageSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexSubImage2DRobustANGLE(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLenum format,
                                                           GLsizei imageSize,
                                                           GLsizei dataSize,
                                                           const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexSubImage2DRobustANGLE");
  interface_->CompressedTexSubImage2DRobustANGLE(target, level, xoffset,
                                                 yoffset, width, height, format,
                                                 imageSize, dataSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexSubImage3D(GLenum target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLint zoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLenum format,
                                                GLsizei imageSize,
                                                const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexSubImage3D");
  interface_->CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                      width, height, depth, format, imageSize,
                                      data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCompressedTexSubImage3DRobustANGLE(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLint zoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLsizei depth,
                                                           GLenum format,
                                                           GLsizei imageSize,
                                                           GLsizei dataSize,
                                                           const void* data) {
  MakeGlMockFunctionUnique("glCompressedTexSubImage3DRobustANGLE");
  interface_->CompressedTexSubImage3DRobustANGLE(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCopyBufferSubData(GLenum readTarget,
                                          GLenum writeTarget,
                                          GLintptr readOffset,
                                          GLintptr writeOffset,
                                          GLsizeiptr size) {
  MakeGlMockFunctionUnique("glCopyBufferSubData");
  interface_->CopyBufferSubData(readTarget, writeTarget, readOffset,
                                writeOffset, size);
}

void GL_BINDING_CALL MockGLInterface::Mock_glCopySubTextureCHROMIUM(
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha) {
  MakeGlMockFunctionUnique("glCopySubTextureCHROMIUM");
  interface_->CopySubTextureCHROMIUM(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCopyTexImage2D(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border) {
  MakeGlMockFunctionUnique("glCopyTexImage2D");
  interface_->CopyTexImage2D(target, level, internalformat, x, y, width, height,
                             border);
}

void GL_BINDING_CALL MockGLInterface::Mock_glCopyTexSubImage2D(GLenum target,
                                                               GLint level,
                                                               GLint xoffset,
                                                               GLint yoffset,
                                                               GLint x,
                                                               GLint y,
                                                               GLsizei width,
                                                               GLsizei height) {
  MakeGlMockFunctionUnique("glCopyTexSubImage2D");
  interface_->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                                height);
}

void GL_BINDING_CALL MockGLInterface::Mock_glCopyTexSubImage3D(GLenum target,
                                                               GLint level,
                                                               GLint xoffset,
                                                               GLint yoffset,
                                                               GLint zoffset,
                                                               GLint x,
                                                               GLint y,
                                                               GLsizei width,
                                                               GLsizei height) {
  MakeGlMockFunctionUnique("glCopyTexSubImage3D");
  interface_->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y,
                                width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCopyTextureCHROMIUM(GLuint sourceId,
                                            GLint sourceLevel,
                                            GLenum destTarget,
                                            GLuint destId,
                                            GLint destLevel,
                                            GLint internalFormat,
                                            GLenum destType,
                                            GLboolean unpackFlipY,
                                            GLboolean unpackPremultiplyAlpha,
                                            GLboolean unpackUnmultiplyAlpha) {
  MakeGlMockFunctionUnique("glCopyTextureCHROMIUM");
  interface_->CopyTextureCHROMIUM(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glCreateMemoryObjectsEXT(GLsizei n,
                                               GLuint* memoryObjects) {
  MakeGlMockFunctionUnique("glCreateMemoryObjectsEXT");
  interface_->CreateMemoryObjectsEXT(n, memoryObjects);
}

GLuint GL_BINDING_CALL MockGLInterface::Mock_glCreateProgram(void) {
  MakeGlMockFunctionUnique("glCreateProgram");
  return interface_->CreateProgram();
}

GLuint GL_BINDING_CALL MockGLInterface::Mock_glCreateShader(GLenum type) {
  MakeGlMockFunctionUnique("glCreateShader");
  return interface_->CreateShader(type);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glCreateShaderProgramv(GLenum type,
                                             GLsizei count,
                                             const char* const* strings) {
  MakeGlMockFunctionUnique("glCreateShaderProgramv");
  return interface_->CreateShaderProgramv(type, count, strings);
}

void GL_BINDING_CALL MockGLInterface::Mock_glCullFace(GLenum mode) {
  MakeGlMockFunctionUnique("glCullFace");
  interface_->CullFace(mode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageCallback(GLDEBUGPROC callback,
                                             const void* userParam) {
  MakeGlMockFunctionUnique("glDebugMessageCallback");
  interface_->DebugMessageCallback(callback, userParam);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageCallbackKHR(GLDEBUGPROC callback,
                                                const void* userParam) {
  MakeGlMockFunctionUnique("glDebugMessageCallbackKHR");
  interface_->DebugMessageCallback(callback, userParam);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageControl(GLenum source,
                                            GLenum type,
                                            GLenum severity,
                                            GLsizei count,
                                            const GLuint* ids,
                                            GLboolean enabled) {
  MakeGlMockFunctionUnique("glDebugMessageControl");
  interface_->DebugMessageControl(source, type, severity, count, ids, enabled);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageControlKHR(GLenum source,
                                               GLenum type,
                                               GLenum severity,
                                               GLsizei count,
                                               const GLuint* ids,
                                               GLboolean enabled) {
  MakeGlMockFunctionUnique("glDebugMessageControlKHR");
  interface_->DebugMessageControl(source, type, severity, count, ids, enabled);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageInsert(GLenum source,
                                           GLenum type,
                                           GLuint id,
                                           GLenum severity,
                                           GLsizei length,
                                           const char* buf) {
  MakeGlMockFunctionUnique("glDebugMessageInsert");
  interface_->DebugMessageInsert(source, type, id, severity, length, buf);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageInsertKHR(GLenum source,
                                              GLenum type,
                                              GLuint id,
                                              GLenum severity,
                                              GLsizei length,
                                              const char* buf) {
  MakeGlMockFunctionUnique("glDebugMessageInsertKHR");
  interface_->DebugMessageInsert(source, type, id, severity, length, buf);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteBuffers(GLsizei n, const GLuint* buffers) {
  MakeGlMockFunctionUnique("glDeleteBuffers");
  interface_->DeleteBuffersARB(n, buffers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteFencesNV(GLsizei n, const GLuint* fences) {
  MakeGlMockFunctionUnique("glDeleteFencesNV");
  interface_->DeleteFencesNV(n, fences);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteFramebuffers(GLsizei n,
                                           const GLuint* framebuffers) {
  MakeGlMockFunctionUnique("glDeleteFramebuffers");
  interface_->DeleteFramebuffersEXT(n, framebuffers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteMemoryObjectsEXT(GLsizei n,
                                               const GLuint* memoryObjects) {
  MakeGlMockFunctionUnique("glDeleteMemoryObjectsEXT");
  interface_->DeleteMemoryObjectsEXT(n, memoryObjects);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDeleteProgram(GLuint program) {
  MakeGlMockFunctionUnique("glDeleteProgram");
  interface_->DeleteProgram(program);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteProgramPipelines(GLsizei n,
                                               const GLuint* pipelines) {
  MakeGlMockFunctionUnique("glDeleteProgramPipelines");
  interface_->DeleteProgramPipelines(n, pipelines);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDeleteQueries(GLsizei n,
                                                           const GLuint* ids) {
  MakeGlMockFunctionUnique("glDeleteQueries");
  interface_->DeleteQueries(n, ids);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteQueriesEXT(GLsizei n, const GLuint* ids) {
  MakeGlMockFunctionUnique("glDeleteQueriesEXT");
  interface_->DeleteQueries(n, ids);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteRenderbuffers(GLsizei n,
                                            const GLuint* renderbuffers) {
  MakeGlMockFunctionUnique("glDeleteRenderbuffers");
  interface_->DeleteRenderbuffersEXT(n, renderbuffers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteSamplers(GLsizei n, const GLuint* samplers) {
  MakeGlMockFunctionUnique("glDeleteSamplers");
  interface_->DeleteSamplers(n, samplers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteSemaphoresEXT(GLsizei n,
                                            const GLuint* semaphores) {
  MakeGlMockFunctionUnique("glDeleteSemaphoresEXT");
  interface_->DeleteSemaphoresEXT(n, semaphores);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDeleteShader(GLuint shader) {
  MakeGlMockFunctionUnique("glDeleteShader");
  interface_->DeleteShader(shader);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDeleteSync(GLsync sync) {
  MakeGlMockFunctionUnique("glDeleteSync");
  interface_->DeleteSync(sync);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteTextures(GLsizei n, const GLuint* textures) {
  MakeGlMockFunctionUnique("glDeleteTextures");
  interface_->DeleteTextures(n, textures);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
  MakeGlMockFunctionUnique("glDeleteTransformFeedbacks");
  interface_->DeleteTransformFeedbacks(n, ids);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
  MakeGlMockFunctionUnique("glDeleteVertexArrays");
  interface_->DeleteVertexArraysOES(n, arrays);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDeleteVertexArraysOES(GLsizei n, const GLuint* arrays) {
  MakeGlMockFunctionUnique("glDeleteVertexArraysOES");
  interface_->DeleteVertexArraysOES(n, arrays);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDepthFunc(GLenum func) {
  MakeGlMockFunctionUnique("glDepthFunc");
  interface_->DepthFunc(func);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDepthMask(GLboolean flag) {
  MakeGlMockFunctionUnique("glDepthMask");
  interface_->DepthMask(flag);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDepthRange(GLclampd zNear,
                                                        GLclampd zFar) {
  MakeGlMockFunctionUnique("glDepthRange");
  interface_->DepthRange(zNear, zFar);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDepthRangef(GLclampf zNear,
                                                         GLclampf zFar) {
  MakeGlMockFunctionUnique("glDepthRangef");
  interface_->DepthRangef(zNear, zFar);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDetachShader(GLuint program,
                                                          GLuint shader) {
  MakeGlMockFunctionUnique("glDetachShader");
  interface_->DetachShader(program, shader);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDisable(GLenum cap) {
  MakeGlMockFunctionUnique("glDisable");
  interface_->Disable(cap);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDisableExtensionANGLE(const char* name) {
  MakeGlMockFunctionUnique("glDisableExtensionANGLE");
  interface_->DisableExtensionANGLE(name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDisableVertexAttribArray(GLuint index) {
  MakeGlMockFunctionUnique("glDisableVertexAttribArray");
  interface_->DisableVertexAttribArray(index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDisablei(GLenum target,
                                                      GLuint index) {
  MakeGlMockFunctionUnique("glDisablei");
  interface_->DisableiOES(target, index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDisableiOES(GLenum target,
                                                         GLuint index) {
  MakeGlMockFunctionUnique("glDisableiOES");
  interface_->DisableiOES(target, index);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDiscardFramebufferEXT(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments) {
  MakeGlMockFunctionUnique("glDiscardFramebufferEXT");
  interface_->DiscardFramebufferEXT(target, numAttachments, attachments);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDispatchCompute(GLuint numGroupsX,
                                        GLuint numGroupsY,
                                        GLuint numGroupsZ) {
  MakeGlMockFunctionUnique("glDispatchCompute");
  interface_->DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDispatchComputeIndirect(GLintptr indirect) {
  MakeGlMockFunctionUnique("glDispatchComputeIndirect");
  interface_->DispatchComputeIndirect(indirect);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDrawArrays(GLenum mode,
                                                        GLint first,
                                                        GLsizei count) {
  MakeGlMockFunctionUnique("glDrawArrays");
  interface_->DrawArrays(mode, first, count);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawArraysIndirect(GLenum mode, const void* indirect) {
  MakeGlMockFunctionUnique("glDrawArraysIndirect");
  interface_->DrawArraysIndirect(mode, indirect);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawArraysInstanced(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount) {
  MakeGlMockFunctionUnique("glDrawArraysInstanced");
  interface_->DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawArraysInstancedANGLE(GLenum mode,
                                                 GLint first,
                                                 GLsizei count,
                                                 GLsizei primcount) {
  MakeGlMockFunctionUnique("glDrawArraysInstancedANGLE");
  interface_->DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawArraysInstancedBaseInstanceANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance) {
  MakeGlMockFunctionUnique("glDrawArraysInstancedBaseInstanceANGLE");
  interface_->DrawArraysInstancedBaseInstanceANGLE(mode, first, count,
                                                   primcount, baseinstance);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDrawArraysInstancedBaseInstanceEXT(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance) {
  MakeGlMockFunctionUnique("glDrawArraysInstancedBaseInstanceEXT");
  interface_->DrawArraysInstancedBaseInstanceANGLE(mode, first, count,
                                                   primcount, baseinstance);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDrawBuffer(GLenum mode) {
  MakeGlMockFunctionUnique("glDrawBuffer");
  interface_->DrawBuffer(mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDrawBuffers(GLsizei n,
                                                         const GLenum* bufs) {
  MakeGlMockFunctionUnique("glDrawBuffers");
  interface_->DrawBuffersARB(n, bufs);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawBuffersEXT(GLsizei n, const GLenum* bufs) {
  MakeGlMockFunctionUnique("glDrawBuffersEXT");
  interface_->DrawBuffersARB(n, bufs);
}

void GL_BINDING_CALL MockGLInterface::Mock_glDrawElements(GLenum mode,
                                                          GLsizei count,
                                                          GLenum type,
                                                          const void* indices) {
  MakeGlMockFunctionUnique("glDrawElements");
  interface_->DrawElements(mode, count, type, indices);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawElementsIndirect(GLenum mode,
                                             GLenum type,
                                             const void* indirect) {
  MakeGlMockFunctionUnique("glDrawElementsIndirect");
  interface_->DrawElementsIndirect(mode, type, indirect);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawElementsInstanced(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices,
                                              GLsizei primcount) {
  MakeGlMockFunctionUnique("glDrawElementsInstanced");
  interface_->DrawElementsInstancedANGLE(mode, count, type, indices, primcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawElementsInstancedANGLE(GLenum mode,
                                                   GLsizei count,
                                                   GLenum type,
                                                   const void* indices,
                                                   GLsizei primcount) {
  MakeGlMockFunctionUnique("glDrawElementsInstancedANGLE");
  interface_->DrawElementsInstancedANGLE(mode, count, type, indices, primcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint baseVertex,
    GLuint baseInstance) {
  MakeGlMockFunctionUnique(
      "glDrawElementsInstancedBaseVertexBaseInstanceANGLE");
  interface_->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, indices, primcount, baseVertex, baseInstance);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawElementsInstancedBaseVertexBaseInstanceEXT(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint baseVertex,
    GLuint baseInstance) {
  MakeGlMockFunctionUnique("glDrawElementsInstancedBaseVertexBaseInstanceEXT");
  interface_->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, indices, primcount, baseVertex, baseInstance);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDrawRangeElements(GLenum mode,
                                          GLuint start,
                                          GLuint end,
                                          GLsizei count,
                                          GLenum type,
                                          const void* indices) {
  MakeGlMockFunctionUnique("glDrawRangeElements");
  interface_->DrawRangeElements(mode, start, end, count, type, indices);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glEGLImageTargetRenderbufferStorageOES(
    GLenum target,
    GLeglImageOES image) {
  MakeGlMockFunctionUnique("glEGLImageTargetRenderbufferStorageOES");
  interface_->EGLImageTargetRenderbufferStorageOES(target, image);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glEGLImageTargetTexture2DOES(GLenum target,
                                                   GLeglImageOES image) {
  MakeGlMockFunctionUnique("glEGLImageTargetTexture2DOES");
  interface_->EGLImageTargetTexture2DOES(target, image);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEnable(GLenum cap) {
  MakeGlMockFunctionUnique("glEnable");
  interface_->Enable(cap);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glEnableVertexAttribArray(GLuint index) {
  MakeGlMockFunctionUnique("glEnableVertexAttribArray");
  interface_->EnableVertexAttribArray(index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEnablei(GLenum target,
                                                     GLuint index) {
  MakeGlMockFunctionUnique("glEnablei");
  interface_->EnableiOES(target, index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEnableiOES(GLenum target,
                                                        GLuint index) {
  MakeGlMockFunctionUnique("glEnableiOES");
  interface_->EnableiOES(target, index);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glEndPixelLocalStorageANGLE(GLsizei n,
                                                  const GLenum* storeops) {
  MakeGlMockFunctionUnique("glEndPixelLocalStorageANGLE");
  interface_->EndPixelLocalStorageANGLE(n, storeops);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEndQuery(GLenum target) {
  MakeGlMockFunctionUnique("glEndQuery");
  interface_->EndQuery(target);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEndQueryEXT(GLenum target) {
  MakeGlMockFunctionUnique("glEndQueryEXT");
  interface_->EndQuery(target);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glEndTilingQCOM(GLbitfield preserveMask) {
  MakeGlMockFunctionUnique("glEndTilingQCOM");
  interface_->EndTilingQCOM(preserveMask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glEndTransformFeedback(void) {
  MakeGlMockFunctionUnique("glEndTransformFeedback");
  interface_->EndTransformFeedback();
}

GLsync GL_BINDING_CALL MockGLInterface::Mock_glFenceSync(GLenum condition,
                                                         GLbitfield flags) {
  MakeGlMockFunctionUnique("glFenceSync");
  return interface_->FenceSync(condition, flags);
}

void GL_BINDING_CALL MockGLInterface::Mock_glFinish(void) {
  MakeGlMockFunctionUnique("glFinish");
  interface_->Finish();
}

void GL_BINDING_CALL MockGLInterface::Mock_glFinishFenceNV(GLuint fence) {
  MakeGlMockFunctionUnique("glFinishFenceNV");
  interface_->FinishFenceNV(fence);
}

void GL_BINDING_CALL MockGLInterface::Mock_glFlush(void) {
  MakeGlMockFunctionUnique("glFlush");
  interface_->Flush();
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFlushMappedBufferRange(GLenum target,
                                               GLintptr offset,
                                               GLsizeiptr length) {
  MakeGlMockFunctionUnique("glFlushMappedBufferRange");
  interface_->FlushMappedBufferRange(target, offset, length);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFlushMappedBufferRangeEXT(GLenum target,
                                                  GLintptr offset,
                                                  GLsizeiptr length) {
  MakeGlMockFunctionUnique("glFlushMappedBufferRangeEXT");
  interface_->FlushMappedBufferRange(target, offset, length);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferMemorylessPixelLocalStorageANGLE(
    GLint plane,
    GLenum internalformat) {
  MakeGlMockFunctionUnique("glFramebufferMemorylessPixelLocalStorageANGLE");
  interface_->FramebufferMemorylessPixelLocalStorageANGLE(plane,
                                                          internalformat);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferParameteri(GLenum target,
                                              GLenum pname,
                                              GLint param) {
  MakeGlMockFunctionUnique("glFramebufferParameteri");
  interface_->FramebufferParameteri(target, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferParameteriMESA(GLenum target,
                                                  GLenum pname,
                                                  GLint param) {
  MakeGlMockFunctionUnique("glFramebufferParameteriMESA");
  interface_->FramebufferParameteri(target, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferPixelLocalClearValuefvANGLE(
    GLint plane,
    const GLfloat* value) {
  MakeGlMockFunctionUnique("glFramebufferPixelLocalClearValuefvANGLE");
  interface_->FramebufferPixelLocalClearValuefvANGLE(plane, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferPixelLocalClearValueivANGLE(
    GLint plane,
    const GLint* value) {
  MakeGlMockFunctionUnique("glFramebufferPixelLocalClearValueivANGLE");
  interface_->FramebufferPixelLocalClearValueivANGLE(plane, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferPixelLocalClearValueuivANGLE(
    GLint plane,
    const GLuint* value) {
  MakeGlMockFunctionUnique("glFramebufferPixelLocalClearValueuivANGLE");
  interface_->FramebufferPixelLocalClearValueuivANGLE(plane, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferPixelLocalStorageInterruptANGLE() {
  MakeGlMockFunctionUnique("glFramebufferPixelLocalStorageInterruptANGLE");
  interface_->FramebufferPixelLocalStorageInterruptANGLE();
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferPixelLocalStorageRestoreANGLE() {
  MakeGlMockFunctionUnique("glFramebufferPixelLocalStorageRestoreANGLE");
  interface_->FramebufferPixelLocalStorageRestoreANGLE();
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferRenderbuffer(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  MakeGlMockFunctionUnique("glFramebufferRenderbuffer");
  interface_->FramebufferRenderbufferEXT(target, attachment, renderbuffertarget,
                                         renderbuffer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTexture2D(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  MakeGlMockFunctionUnique("glFramebufferTexture2D");
  interface_->FramebufferTexture2DEXT(target, attachment, textarget, texture,
                                      level);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTexture2DMultisampleEXT(GLenum target,
                                                           GLenum attachment,
                                                           GLenum textarget,
                                                           GLuint texture,
                                                           GLint level,
                                                           GLsizei samples) {
  MakeGlMockFunctionUnique("glFramebufferTexture2DMultisampleEXT");
  interface_->FramebufferTexture2DMultisampleEXT(target, attachment, textarget,
                                                 texture, level, samples);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTexture2DMultisampleIMG(GLenum target,
                                                           GLenum attachment,
                                                           GLenum textarget,
                                                           GLuint texture,
                                                           GLint level,
                                                           GLsizei samples) {
  MakeGlMockFunctionUnique("glFramebufferTexture2DMultisampleIMG");
  interface_->FramebufferTexture2DMultisampleEXT(target, attachment, textarget,
                                                 texture, level, samples);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTextureLayer(GLenum target,
                                                GLenum attachment,
                                                GLuint texture,
                                                GLint level,
                                                GLint layer) {
  MakeGlMockFunctionUnique("glFramebufferTextureLayer");
  interface_->FramebufferTextureLayer(target, attachment, texture, level,
                                      layer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTextureMultiviewOVR(GLenum target,
                                                       GLenum attachment,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLint baseViewIndex,
                                                       GLsizei numViews) {
  MakeGlMockFunctionUnique("glFramebufferTextureMultiviewOVR");
  interface_->FramebufferTextureMultiviewOVR(target, attachment, texture, level,
                                             baseViewIndex, numViews);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferTexturePixelLocalStorageANGLE(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer) {
  MakeGlMockFunctionUnique("glFramebufferTexturePixelLocalStorageANGLE");
  interface_->FramebufferTexturePixelLocalStorageANGLE(plane, backingtexture,
                                                       level, layer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glFrontFace(GLenum mode) {
  MakeGlMockFunctionUnique("glFrontFace");
  interface_->FrontFace(mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenBuffers(GLsizei n,
                                                        GLuint* buffers) {
  MakeGlMockFunctionUnique("glGenBuffers");
  interface_->GenBuffersARB(n, buffers);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenFencesNV(GLsizei n,
                                                         GLuint* fences) {
  MakeGlMockFunctionUnique("glGenFencesNV");
  interface_->GenFencesNV(n, fences);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  MakeGlMockFunctionUnique("glGenFramebuffers");
  interface_->GenFramebuffersEXT(n, framebuffers);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glGenProgramPipelines(GLsizei n, GLuint* pipelines) {
  MakeGlMockFunctionUnique("glGenProgramPipelines");
  return interface_->GenProgramPipelines(n, pipelines);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenQueries(GLsizei n,
                                                        GLuint* ids) {
  MakeGlMockFunctionUnique("glGenQueries");
  interface_->GenQueries(n, ids);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenQueriesEXT(GLsizei n,
                                                           GLuint* ids) {
  MakeGlMockFunctionUnique("glGenQueriesEXT");
  interface_->GenQueries(n, ids);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  MakeGlMockFunctionUnique("glGenRenderbuffers");
  interface_->GenRenderbuffersEXT(n, renderbuffers);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenSamplers(GLsizei n,
                                                         GLuint* samplers) {
  MakeGlMockFunctionUnique("glGenSamplers");
  interface_->GenSamplers(n, samplers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGenSemaphoresEXT(GLsizei n, GLuint* semaphores) {
  MakeGlMockFunctionUnique("glGenSemaphoresEXT");
  interface_->GenSemaphoresEXT(n, semaphores);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenTextures(GLsizei n,
                                                         GLuint* textures) {
  MakeGlMockFunctionUnique("glGenTextures");
  interface_->GenTextures(n, textures);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGenTransformFeedbacks(GLsizei n, GLuint* ids) {
  MakeGlMockFunctionUnique("glGenTransformFeedbacks");
  interface_->GenTransformFeedbacks(n, ids);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenVertexArrays(GLsizei n,
                                                             GLuint* arrays) {
  MakeGlMockFunctionUnique("glGenVertexArrays");
  interface_->GenVertexArraysOES(n, arrays);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGenVertexArraysOES(GLsizei n, GLuint* arrays) {
  MakeGlMockFunctionUnique("glGenVertexArraysOES");
  interface_->GenVertexArraysOES(n, arrays);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGenerateMipmap(GLenum target) {
  MakeGlMockFunctionUnique("glGenerateMipmap");
  interface_->GenerateMipmapEXT(target);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetActiveAttrib(GLuint program,
                                                             GLuint index,
                                                             GLsizei bufsize,
                                                             GLsizei* length,
                                                             GLint* size,
                                                             GLenum* type,
                                                             char* name) {
  MakeGlMockFunctionUnique("glGetActiveAttrib");
  interface_->GetActiveAttrib(program, index, bufsize, length, size, type,
                              name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetActiveUniform(GLuint program,
                                                              GLuint index,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* size,
                                                              GLenum* type,
                                                              char* name) {
  MakeGlMockFunctionUnique("glGetActiveUniform");
  interface_->GetActiveUniform(program, index, bufsize, length, size, type,
                               name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetActiveUniformBlockName(GLuint program,
                                                  GLuint uniformBlockIndex,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  char* uniformBlockName) {
  MakeGlMockFunctionUnique("glGetActiveUniformBlockName");
  interface_->GetActiveUniformBlockName(program, uniformBlockIndex, bufSize,
                                        length, uniformBlockName);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetActiveUniformBlockiv(GLuint program,
                                                GLuint uniformBlockIndex,
                                                GLenum pname,
                                                GLint* params) {
  MakeGlMockFunctionUnique("glGetActiveUniformBlockiv");
  interface_->GetActiveUniformBlockiv(program, uniformBlockIndex, pname,
                                      params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetActiveUniformBlockivRobustANGLE(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  MakeGlMockFunctionUnique("glGetActiveUniformBlockivRobustANGLE");
  interface_->GetActiveUniformBlockivRobustANGLE(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetActiveUniformsiv(GLuint program,
                                            GLsizei uniformCount,
                                            const GLuint* uniformIndices,
                                            GLenum pname,
                                            GLint* params) {
  MakeGlMockFunctionUnique("glGetActiveUniformsiv");
  interface_->GetActiveUniformsiv(program, uniformCount, uniformIndices, pname,
                                  params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetAttachedShaders(GLuint program,
                                           GLsizei maxcount,
                                           GLsizei* count,
                                           GLuint* shaders) {
  MakeGlMockFunctionUnique("glGetAttachedShaders");
  interface_->GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GL_BINDING_CALL
MockGLInterface::Mock_glGetAttribLocation(GLuint program, const char* name) {
  MakeGlMockFunctionUnique("glGetAttribLocation");
  return interface_->GetAttribLocation(program, name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetBooleani_v(GLenum target,
                                                           GLuint index,
                                                           GLboolean* data) {
  MakeGlMockFunctionUnique("glGetBooleani_v");
  interface_->GetBooleani_v(target, index, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBooleani_vRobustANGLE(GLenum target,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLboolean* data) {
  MakeGlMockFunctionUnique("glGetBooleani_vRobustANGLE");
  interface_->GetBooleani_vRobustANGLE(target, index, bufSize, length, data);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetBooleanv(GLenum pname,
                                                         GLboolean* params) {
  MakeGlMockFunctionUnique("glGetBooleanv");
  interface_->GetBooleanv(pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBooleanvRobustANGLE(GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLboolean* data) {
  MakeGlMockFunctionUnique("glGetBooleanvRobustANGLE");
  interface_->GetBooleanvRobustANGLE(pname, bufSize, length, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBufferParameteri64vRobustANGLE(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint64* params) {
  MakeGlMockFunctionUnique("glGetBufferParameteri64vRobustANGLE");
  interface_->GetBufferParameteri64vRobustANGLE(target, pname, bufSize, length,
                                                params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBufferParameteriv(GLenum target,
                                             GLenum pname,
                                             GLint* params) {
  MakeGlMockFunctionUnique("glGetBufferParameteriv");
  interface_->GetBufferParameteriv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBufferParameterivRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) {
  MakeGlMockFunctionUnique("glGetBufferParameterivRobustANGLE");
  interface_->GetBufferParameterivRobustANGLE(target, pname, bufSize, length,
                                              params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetBufferPointervRobustANGLE(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     void** params) {
  MakeGlMockFunctionUnique("glGetBufferPointervRobustANGLE");
  interface_->GetBufferPointervRobustANGLE(target, pname, bufSize, length,
                                           params);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glGetDebugMessageLog(GLuint count,
                                           GLsizei bufSize,
                                           GLenum* sources,
                                           GLenum* types,
                                           GLuint* ids,
                                           GLenum* severities,
                                           GLsizei* lengths,
                                           char* messageLog) {
  MakeGlMockFunctionUnique("glGetDebugMessageLog");
  return interface_->GetDebugMessageLog(count, bufSize, sources, types, ids,
                                        severities, lengths, messageLog);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glGetDebugMessageLogKHR(GLuint count,
                                              GLsizei bufSize,
                                              GLenum* sources,
                                              GLenum* types,
                                              GLuint* ids,
                                              GLenum* severities,
                                              GLsizei* lengths,
                                              char* messageLog) {
  MakeGlMockFunctionUnique("glGetDebugMessageLogKHR");
  return interface_->GetDebugMessageLog(count, bufSize, sources, types, ids,
                                        severities, lengths, messageLog);
}

GLenum GL_BINDING_CALL MockGLInterface::Mock_glGetError(void) {
  MakeGlMockFunctionUnique("glGetError");
  return interface_->GetError();
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetFenceivNV(GLuint fence,
                                                          GLenum pname,
                                                          GLint* params) {
  MakeGlMockFunctionUnique("glGetFenceivNV");
  interface_->GetFenceivNV(fence, pname, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetFloatv(GLenum pname,
                                                       GLfloat* params) {
  MakeGlMockFunctionUnique("glGetFloatv");
  interface_->GetFloatv(pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFloatvRobustANGLE(GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* data) {
  MakeGlMockFunctionUnique("glGetFloatvRobustANGLE");
  interface_->GetFloatvRobustANGLE(pname, bufSize, length, data);
}

GLint GL_BINDING_CALL
MockGLInterface::Mock_glGetFragDataIndexEXT(GLuint program, const char* name) {
  MakeGlMockFunctionUnique("glGetFragDataIndexEXT");
  return interface_->GetFragDataIndex(program, name);
}

GLint GL_BINDING_CALL
MockGLInterface::Mock_glGetFragDataLocation(GLuint program, const char* name) {
  MakeGlMockFunctionUnique("glGetFragDataLocation");
  return interface_->GetFragDataLocation(program, name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferAttachmentParameteriv(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  MakeGlMockFunctionUnique("glGetFramebufferAttachmentParameteriv");
  interface_->GetFramebufferAttachmentParameterivEXT(target, attachment, pname,
                                                     params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferAttachmentParameterivRobustANGLE(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  MakeGlMockFunctionUnique("glGetFramebufferAttachmentParameterivRobustANGLE");
  interface_->GetFramebufferAttachmentParameterivRobustANGLE(
      target, attachment, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferParameteriv(GLenum target,
                                                  GLenum pname,
                                                  GLint* params) {
  MakeGlMockFunctionUnique("glGetFramebufferParameteriv");
  interface_->GetFramebufferParameteriv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferParameterivRobustANGLE(GLenum target,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params) {
  MakeGlMockFunctionUnique("glGetFramebufferParameterivRobustANGLE");
  interface_->GetFramebufferParameterivRobustANGLE(target, pname, bufSize,
                                                   length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    GLfloat* params) {
  MakeGlMockFunctionUnique("glGetFramebufferPixelLocalStorageParameterfvANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterfvANGLE(plane, pname,
                                                              params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterfvRobustANGLE(
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params) {
  MakeGlMockFunctionUnique(
      "glGetFramebufferPixelLocalStorageParameterfvRobustANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterfvRobustANGLE(
      plane, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    GLint* params) {
  MakeGlMockFunctionUnique("glGetFramebufferPixelLocalStorageParameterivANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname,
                                                              params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterivRobustANGLE(
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  MakeGlMockFunctionUnique(
      "glGetFramebufferPixelLocalStorageParameterivRobustANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterivRobustANGLE(
      plane, pname, bufSize, length, params);
}

GLenum GL_BINDING_CALL MockGLInterface::Mock_glGetGraphicsResetStatus(void) {
  MakeGlMockFunctionUnique("glGetGraphicsResetStatus");
  return interface_->GetGraphicsResetStatusARB();
}

GLenum GL_BINDING_CALL MockGLInterface::Mock_glGetGraphicsResetStatusEXT(void) {
  MakeGlMockFunctionUnique("glGetGraphicsResetStatusEXT");
  return interface_->GetGraphicsResetStatusARB();
}

GLenum GL_BINDING_CALL MockGLInterface::Mock_glGetGraphicsResetStatusKHR(void) {
  MakeGlMockFunctionUnique("glGetGraphicsResetStatusKHR");
  return interface_->GetGraphicsResetStatusARB();
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetInteger64i_v(GLenum target,
                                                             GLuint index,
                                                             GLint64* data) {
  MakeGlMockFunctionUnique("glGetInteger64i_v");
  interface_->GetInteger64i_v(target, index, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetInteger64i_vRobustANGLE(GLenum target,
                                                   GLuint index,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* data) {
  MakeGlMockFunctionUnique("glGetInteger64i_vRobustANGLE");
  interface_->GetInteger64i_vRobustANGLE(target, index, bufSize, length, data);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetInteger64v(GLenum pname,
                                                           GLint64* params) {
  MakeGlMockFunctionUnique("glGetInteger64v");
  interface_->GetInteger64v(pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetInteger64vRobustANGLE(GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint64* data) {
  MakeGlMockFunctionUnique("glGetInteger64vRobustANGLE");
  interface_->GetInteger64vRobustANGLE(pname, bufSize, length, data);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetIntegeri_v(GLenum target,
                                                           GLuint index,
                                                           GLint* data) {
  MakeGlMockFunctionUnique("glGetIntegeri_v");
  interface_->GetIntegeri_v(target, index, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetIntegeri_vRobustANGLE(GLenum target,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* data) {
  MakeGlMockFunctionUnique("glGetIntegeri_vRobustANGLE");
  interface_->GetIntegeri_vRobustANGLE(target, index, bufSize, length, data);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetIntegerv(GLenum pname,
                                                         GLint* params) {
  MakeGlMockFunctionUnique("glGetIntegerv");
  interface_->GetIntegerv(pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetIntegervRobustANGLE(GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* data) {
  MakeGlMockFunctionUnique("glGetIntegervRobustANGLE");
  interface_->GetIntegervRobustANGLE(pname, bufSize, length, data);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetInternalformatSampleivNV(GLenum target,
                                                    GLenum internalformat,
                                                    GLsizei samples,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLint* params) {
  MakeGlMockFunctionUnique("glGetInternalformatSampleivNV");
  interface_->GetInternalformatSampleivNV(target, internalformat, samples,
                                          pname, bufSize, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetInternalformativ(GLenum target,
                                            GLenum internalformat,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLint* params) {
  MakeGlMockFunctionUnique("glGetInternalformativ");
  interface_->GetInternalformativ(target, internalformat, pname, bufSize,
                                  params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetInternalformativRobustANGLE(GLenum target,
                                                       GLenum internalformat,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  MakeGlMockFunctionUnique("glGetInternalformativRobustANGLE");
  interface_->GetInternalformativRobustANGLE(target, internalformat, pname,
                                             bufSize, length, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetMultisamplefv(GLenum pname,
                                                              GLuint index,
                                                              GLfloat* val) {
  MakeGlMockFunctionUnique("glGetMultisamplefv");
  interface_->GetMultisamplefv(pname, index, val);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetMultisamplefvRobustANGLE(GLenum pname,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLfloat* val) {
  MakeGlMockFunctionUnique("glGetMultisamplefvRobustANGLE");
  interface_->GetMultisamplefvRobustANGLE(pname, index, bufSize, length, val);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetObjectLabel(GLenum identifier,
                                                            GLuint name,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            char* label) {
  MakeGlMockFunctionUnique("glGetObjectLabel");
  interface_->GetObjectLabel(identifier, name, bufSize, length, label);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetObjectLabelKHR(GLenum identifier,
                                          GLuint name,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          char* label) {
  MakeGlMockFunctionUnique("glGetObjectLabelKHR");
  interface_->GetObjectLabel(identifier, name, bufSize, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetObjectPtrLabel(void* ptr,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               char* label) {
  MakeGlMockFunctionUnique("glGetObjectPtrLabel");
  interface_->GetObjectPtrLabel(ptr, bufSize, length, label);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetObjectPtrLabelKHR(void* ptr,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             char* label) {
  MakeGlMockFunctionUnique("glGetObjectPtrLabelKHR");
  interface_->GetObjectPtrLabel(ptr, bufSize, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetPointerv(GLenum pname,
                                                         void** params) {
  MakeGlMockFunctionUnique("glGetPointerv");
  interface_->GetPointerv(pname, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetPointervKHR(GLenum pname,
                                                            void** params) {
  MakeGlMockFunctionUnique("glGetPointervKHR");
  interface_->GetPointerv(pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetPointervRobustANGLERobustANGLE(GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          void** params) {
  MakeGlMockFunctionUnique("glGetPointervRobustANGLERobustANGLE");
  interface_->GetPointervRobustANGLERobustANGLE(pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramBinary(GLuint program,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLenum* binaryFormat,
                                         GLvoid* binary) {
  MakeGlMockFunctionUnique("glGetProgramBinary");
  interface_->GetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramBinaryOES(GLuint program,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLenum* binaryFormat,
                                            GLvoid* binary) {
  MakeGlMockFunctionUnique("glGetProgramBinaryOES");
  interface_->GetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetProgramInfoLog(GLuint program,
                                                               GLsizei bufsize,
                                                               GLsizei* length,
                                                               char* infolog) {
  MakeGlMockFunctionUnique("glGetProgramInfoLog");
  interface_->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramInterfaceiv(GLuint program,
                                              GLenum programInterface,
                                              GLenum pname,
                                              GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramInterfaceiv");
  interface_->GetProgramInterfaceiv(program, programInterface, pname, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetProgramInterfaceivRobustANGLE(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramInterfaceivRobustANGLE");
  interface_->GetProgramInterfaceivRobustANGLE(program, programInterface, pname,
                                               bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramPipelineInfoLog(GLuint pipeline,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLchar* infoLog) {
  MakeGlMockFunctionUnique("glGetProgramPipelineInfoLog");
  interface_->GetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramPipelineiv(GLuint pipeline,
                                             GLenum pname,
                                             GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramPipelineiv");
  interface_->GetProgramPipelineiv(pipeline, pname, params);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramResourceIndex(GLuint program,
                                                GLenum programInterface,
                                                const GLchar* name) {
  MakeGlMockFunctionUnique("glGetProgramResourceIndex");
  return interface_->GetProgramResourceIndex(program, programInterface, name);
}

GLint GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramResourceLocation(GLuint program,
                                                   GLenum programInterface,
                                                   const char* name) {
  MakeGlMockFunctionUnique("glGetProgramResourceLocation");
  return interface_->GetProgramResourceLocation(program, programInterface,
                                                name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramResourceName(GLuint program,
                                               GLenum programInterface,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLchar* name) {
  MakeGlMockFunctionUnique("glGetProgramResourceName");
  interface_->GetProgramResourceName(program, programInterface, index, bufSize,
                                     length, name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramResourceiv(GLuint program,
                                             GLenum programInterface,
                                             GLuint index,
                                             GLsizei propCount,
                                             const GLenum* props,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramResourceiv");
  interface_->GetProgramResourceiv(program, programInterface, index, propCount,
                                   props, bufSize, length, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetProgramiv(GLuint program,
                                                          GLenum pname,
                                                          GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramiv");
  interface_->GetProgramiv(program, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetProgramivRobustANGLE(GLuint program,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  MakeGlMockFunctionUnique("glGetProgramivRobustANGLE");
  interface_->GetProgramivRobustANGLE(program, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjecti64vEXT(GLuint id,
                                              GLenum pname,
                                              GLint64* params) {
  MakeGlMockFunctionUnique("glGetQueryObjecti64vEXT");
  interface_->GetQueryObjecti64v(id, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjecti64vRobustANGLE(GLuint id,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint64* params) {
  MakeGlMockFunctionUnique("glGetQueryObjecti64vRobustANGLE");
  interface_->GetQueryObjecti64vRobustANGLE(id, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectivEXT(GLuint id,
                                            GLenum pname,
                                            GLint* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectivEXT");
  interface_->GetQueryObjectiv(id, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectivRobustANGLE(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectivRobustANGLE");
  interface_->GetQueryObjectivRobustANGLE(id, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectui64vEXT(GLuint id,
                                               GLenum pname,
                                               GLuint64* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectui64vEXT");
  interface_->GetQueryObjectui64v(id, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectui64vRobustANGLE(GLuint id,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint64* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectui64vRobustANGLE");
  interface_->GetQueryObjectui64vRobustANGLE(id, pname, bufSize, length,
                                             params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetQueryObjectuiv(GLuint id,
                                                               GLenum pname,
                                                               GLuint* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectuiv");
  interface_->GetQueryObjectuiv(id, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectuivEXT(GLuint id,
                                             GLenum pname,
                                             GLuint* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectuivEXT");
  interface_->GetQueryObjectuiv(id, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryObjectuivRobustANGLE(GLuint id,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLuint* params) {
  MakeGlMockFunctionUnique("glGetQueryObjectuivRobustANGLE");
  interface_->GetQueryObjectuivRobustANGLE(id, pname, bufSize, length, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetQueryiv(GLenum target,
                                                        GLenum pname,
                                                        GLint* params) {
  MakeGlMockFunctionUnique("glGetQueryiv");
  interface_->GetQueryiv(target, pname, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetQueryivEXT(GLenum target,
                                                           GLenum pname,
                                                           GLint* params) {
  MakeGlMockFunctionUnique("glGetQueryivEXT");
  interface_->GetQueryiv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetQueryivRobustANGLE(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  MakeGlMockFunctionUnique("glGetQueryivRobustANGLE");
  interface_->GetQueryivRobustANGLE(target, pname, bufSize, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetRenderbufferParameteriv(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  MakeGlMockFunctionUnique("glGetRenderbufferParameteriv");
  interface_->GetRenderbufferParameterivEXT(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetRenderbufferParameterivRobustANGLE(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* params) {
  MakeGlMockFunctionUnique("glGetRenderbufferParameterivRobustANGLE");
  interface_->GetRenderbufferParameterivRobustANGLE(target, pname, bufSize,
                                                    length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameterIivRobustANGLE(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameterIivRobustANGLE");
  interface_->GetSamplerParameterIivRobustANGLE(sampler, pname, bufSize, length,
                                                params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLuint* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameterIuivRobustANGLE");
  interface_->GetSamplerParameterIuivRobustANGLE(sampler, pname, bufSize,
                                                 length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameterfv(GLuint sampler,
                                              GLenum pname,
                                              GLfloat* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameterfv");
  interface_->GetSamplerParameterfv(sampler, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameterfvRobustANGLE(GLuint sampler,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLfloat* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameterfvRobustANGLE");
  interface_->GetSamplerParameterfvRobustANGLE(sampler, pname, bufSize, length,
                                               params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameteriv(GLuint sampler,
                                              GLenum pname,
                                              GLint* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameteriv");
  interface_->GetSamplerParameteriv(sampler, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetSamplerParameterivRobustANGLE(GLuint sampler,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  MakeGlMockFunctionUnique("glGetSamplerParameterivRobustANGLE");
  interface_->GetSamplerParameterivRobustANGLE(sampler, pname, bufSize, length,
                                               params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetShaderInfoLog(GLuint shader,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              char* infolog) {
  MakeGlMockFunctionUnique("glGetShaderInfoLog");
  interface_->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetShaderPrecisionFormat(GLenum shadertype,
                                                 GLenum precisiontype,
                                                 GLint* range,
                                                 GLint* precision) {
  MakeGlMockFunctionUnique("glGetShaderPrecisionFormat");
  interface_->GetShaderPrecisionFormat(shadertype, precisiontype, range,
                                       precision);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetShaderSource(GLuint shader,
                                                             GLsizei bufsize,
                                                             GLsizei* length,
                                                             char* source) {
  MakeGlMockFunctionUnique("glGetShaderSource");
  interface_->GetShaderSource(shader, bufsize, length, source);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetShaderiv(GLuint shader,
                                                         GLenum pname,
                                                         GLint* params) {
  MakeGlMockFunctionUnique("glGetShaderiv");
  interface_->GetShaderiv(shader, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetShaderivRobustANGLE(GLuint shader,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) {
  MakeGlMockFunctionUnique("glGetShaderivRobustANGLE");
  interface_->GetShaderivRobustANGLE(shader, pname, bufSize, length, params);
}

const GLubyte* GL_BINDING_CALL MockGLInterface::Mock_glGetString(GLenum name) {
  MakeGlMockFunctionUnique("glGetString");
  return interface_->GetString(name);
}

const GLubyte* GL_BINDING_CALL
MockGLInterface::Mock_glGetStringi(GLenum name, GLuint index) {
  MakeGlMockFunctionUnique("glGetStringi");
  return interface_->GetStringi(name, index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetSynciv(GLsync sync,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* values) {
  MakeGlMockFunctionUnique("glGetSynciv");
  interface_->GetSynciv(sync, pname, bufSize, length, values);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameterfv(GLenum target,
                                               GLint level,
                                               GLenum pname,
                                               GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterfv");
  interface_->GetTexLevelParameterfv(target, level, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameterfvANGLE(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterfvANGLE");
  interface_->GetTexLevelParameterfv(target, level, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameterfvRobustANGLE(GLenum target,
                                                          GLint level,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterfvRobustANGLE");
  interface_->GetTexLevelParameterfvRobustANGLE(target, level, pname, bufSize,
                                                length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameteriv(GLenum target,
                                               GLint level,
                                               GLenum pname,
                                               GLint* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameteriv");
  interface_->GetTexLevelParameteriv(target, level, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameterivANGLE(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLint* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterivANGLE");
  interface_->GetTexLevelParameteriv(target, level, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexLevelParameterivRobustANGLE(GLenum target,
                                                          GLint level,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterivRobustANGLE");
  interface_->GetTexLevelParameterivRobustANGLE(target, level, pname, bufSize,
                                                length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexParameterIivRobustANGLE(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  MakeGlMockFunctionUnique("glGetTexParameterIivRobustANGLE");
  interface_->GetTexParameterIivRobustANGLE(target, pname, bufSize, length,
                                            params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexParameterIuivRobustANGLE(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint* params) {
  MakeGlMockFunctionUnique("glGetTexParameterIuivRobustANGLE");
  interface_->GetTexParameterIuivRobustANGLE(target, pname, bufSize, length,
                                             params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexParameterfv(GLenum target,
                                          GLenum pname,
                                          GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexParameterfv");
  interface_->GetTexParameterfv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexParameterfvRobustANGLE(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexParameterfvRobustANGLE");
  interface_->GetTexParameterfvRobustANGLE(target, pname, bufSize, length,
                                           params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetTexParameteriv(GLenum target,
                                                               GLenum pname,
                                                               GLint* params) {
  MakeGlMockFunctionUnique("glGetTexParameteriv");
  interface_->GetTexParameteriv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTexParameterivRobustANGLE(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  MakeGlMockFunctionUnique("glGetTexParameterivRobustANGLE");
  interface_->GetTexParameterivRobustANGLE(target, pname, bufSize, length,
                                           params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTransformFeedbackVarying(GLuint program,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLsizei* size,
                                                    GLenum* type,
                                                    char* name) {
  MakeGlMockFunctionUnique("glGetTransformFeedbackVarying");
  interface_->GetTransformFeedbackVarying(program, index, bufSize, length, size,
                                          type, name);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetTranslatedShaderSourceANGLE(GLuint shader,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       char* source) {
  MakeGlMockFunctionUnique("glGetTranslatedShaderSourceANGLE");
  interface_->GetTranslatedShaderSourceANGLE(shader, bufsize, length, source);
}

GLuint GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformBlockIndex(GLuint program,
                                             const char* uniformBlockName) {
  MakeGlMockFunctionUnique("glGetUniformBlockIndex");
  return interface_->GetUniformBlockIndex(program, uniformBlockName);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformIndices(GLuint program,
                                          GLsizei uniformCount,
                                          const char* const* uniformNames,
                                          GLuint* uniformIndices) {
  MakeGlMockFunctionUnique("glGetUniformIndices");
  interface_->GetUniformIndices(program, uniformCount, uniformNames,
                                uniformIndices);
}

GLint GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformLocation(GLuint program, const char* name) {
  MakeGlMockFunctionUnique("glGetUniformLocation");
  return interface_->GetUniformLocation(program, name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetUniformfv(GLuint program,
                                                          GLint location,
                                                          GLfloat* params) {
  MakeGlMockFunctionUnique("glGetUniformfv");
  interface_->GetUniformfv(program, location, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformfvRobustANGLE(GLuint program,
                                                GLint location,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* params) {
  MakeGlMockFunctionUnique("glGetUniformfvRobustANGLE");
  interface_->GetUniformfvRobustANGLE(program, location, bufSize, length,
                                      params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetUniformiv(GLuint program,
                                                          GLint location,
                                                          GLint* params) {
  MakeGlMockFunctionUnique("glGetUniformiv");
  interface_->GetUniformiv(program, location, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformivRobustANGLE(GLuint program,
                                                GLint location,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  MakeGlMockFunctionUnique("glGetUniformivRobustANGLE");
  interface_->GetUniformivRobustANGLE(program, location, bufSize, length,
                                      params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetUniformuiv(GLuint program,
                                                           GLint location,
                                                           GLuint* params) {
  MakeGlMockFunctionUnique("glGetUniformuiv");
  interface_->GetUniformuiv(program, location, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetUniformuivRobustANGLE(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLuint* params) {
  MakeGlMockFunctionUnique("glGetUniformuivRobustANGLE");
  interface_->GetUniformuivRobustANGLE(program, location, bufSize, length,
                                       params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribIivRobustANGLE(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribIivRobustANGLE");
  interface_->GetVertexAttribIivRobustANGLE(index, pname, bufSize, length,
                                            params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribIuivRobustANGLE(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribIuivRobustANGLE");
  interface_->GetVertexAttribIuivRobustANGLE(index, pname, bufSize, length,
                                             params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribPointerv(GLuint index,
                                                GLenum pname,
                                                void** pointer) {
  MakeGlMockFunctionUnique("glGetVertexAttribPointerv");
  interface_->GetVertexAttribPointerv(index, pname, pointer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribPointervRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           void** pointer) {
  MakeGlMockFunctionUnique("glGetVertexAttribPointervRobustANGLE");
  interface_->GetVertexAttribPointervRobustANGLE(index, pname, bufSize, length,
                                                 pointer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribfv(GLuint index,
                                          GLenum pname,
                                          GLfloat* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribfv");
  interface_->GetVertexAttribfv(index, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribfvRobustANGLE(GLuint index,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribfvRobustANGLE");
  interface_->GetVertexAttribfvRobustANGLE(index, pname, bufSize, length,
                                           params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetVertexAttribiv(GLuint index,
                                                               GLenum pname,
                                                               GLint* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribiv");
  interface_->GetVertexAttribiv(index, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetVertexAttribivRobustANGLE(GLuint index,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  MakeGlMockFunctionUnique("glGetVertexAttribivRobustANGLE");
  interface_->GetVertexAttribivRobustANGLE(index, pname, bufSize, length,
                                           params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetnUniformfvRobustANGLE(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  MakeGlMockFunctionUnique("glGetnUniformfvRobustANGLE");
  interface_->GetnUniformfvRobustANGLE(program, location, bufSize, length,
                                       params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetnUniformivRobustANGLE(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  MakeGlMockFunctionUnique("glGetnUniformivRobustANGLE");
  interface_->GetnUniformivRobustANGLE(program, location, bufSize, length,
                                       params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetnUniformuivRobustANGLE(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  MakeGlMockFunctionUnique("glGetnUniformuivRobustANGLE");
  interface_->GetnUniformuivRobustANGLE(program, location, bufSize, length,
                                        params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glHint(GLenum target, GLenum mode) {
  MakeGlMockFunctionUnique("glHint");
  interface_->Hint(target, mode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportMemoryFdEXT(GLuint memory,
                                          GLuint64 size,
                                          GLenum handleType,
                                          GLint fd) {
  MakeGlMockFunctionUnique("glImportMemoryFdEXT");
  interface_->ImportMemoryFdEXT(memory, size, handleType, fd);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportMemoryWin32HandleEXT(GLuint memory,
                                                   GLuint64 size,
                                                   GLenum handleType,
                                                   void* handle) {
  MakeGlMockFunctionUnique("glImportMemoryWin32HandleEXT");
  interface_->ImportMemoryWin32HandleEXT(memory, size, handleType, handle);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportMemoryZirconHandleANGLE(GLuint memory,
                                                      GLuint64 size,
                                                      GLenum handleType,
                                                      GLuint handle) {
  MakeGlMockFunctionUnique("glImportMemoryZirconHandleANGLE");
  interface_->ImportMemoryZirconHandleANGLE(memory, size, handleType, handle);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportSemaphoreFdEXT(GLuint semaphore,
                                             GLenum handleType,
                                             GLint fd) {
  MakeGlMockFunctionUnique("glImportSemaphoreFdEXT");
  interface_->ImportSemaphoreFdEXT(semaphore, handleType, fd);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportSemaphoreWin32HandleEXT(GLuint semaphore,
                                                      GLenum handleType,
                                                      void* handle) {
  MakeGlMockFunctionUnique("glImportSemaphoreWin32HandleEXT");
  interface_->ImportSemaphoreWin32HandleEXT(semaphore, handleType, handle);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glImportSemaphoreZirconHandleANGLE(GLuint semaphore,
                                                         GLenum handleType,
                                                         GLuint handle) {
  MakeGlMockFunctionUnique("glImportSemaphoreZirconHandleANGLE");
  interface_->ImportSemaphoreZirconHandleANGLE(semaphore, handleType, handle);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glInsertEventMarkerEXT(GLsizei length,
                                             const char* marker) {
  MakeGlMockFunctionUnique("glInsertEventMarkerEXT");
  interface_->InsertEventMarkerEXT(length, marker);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glInvalidateFramebuffer(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments) {
  MakeGlMockFunctionUnique("glInvalidateFramebuffer");
  interface_->InvalidateFramebuffer(target, numAttachments, attachments);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glInvalidateSubFramebuffer(GLenum target,
                                                 GLsizei numAttachments,
                                                 const GLenum* attachments,
                                                 GLint x,
                                                 GLint y,
                                                 GLint width,
                                                 GLint height) {
  MakeGlMockFunctionUnique("glInvalidateSubFramebuffer");
  interface_->InvalidateSubFramebuffer(target, numAttachments, attachments, x,
                                       y, width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glInvalidateTextureANGLE(GLenum target) {
  MakeGlMockFunctionUnique("glInvalidateTextureANGLE");
  interface_->InvalidateTextureANGLE(target);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsBuffer(GLuint buffer) {
  MakeGlMockFunctionUnique("glIsBuffer");
  return interface_->IsBuffer(buffer);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsEnabled(GLenum cap) {
  MakeGlMockFunctionUnique("glIsEnabled");
  return interface_->IsEnabled(cap);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsEnabledi(GLenum target,
                                                             GLuint index) {
  MakeGlMockFunctionUnique("glIsEnabledi");
  return interface_->IsEnablediOES(target, index);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsEnablediOES(GLenum target,
                                                                GLuint index) {
  MakeGlMockFunctionUnique("glIsEnablediOES");
  return interface_->IsEnablediOES(target, index);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsFenceNV(GLuint fence) {
  MakeGlMockFunctionUnique("glIsFenceNV");
  return interface_->IsFenceNV(fence);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glIsFramebuffer(GLuint framebuffer) {
  MakeGlMockFunctionUnique("glIsFramebuffer");
  return interface_->IsFramebufferEXT(framebuffer);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsProgram(GLuint program) {
  MakeGlMockFunctionUnique("glIsProgram");
  return interface_->IsProgram(program);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glIsProgramPipeline(GLuint pipeline) {
  MakeGlMockFunctionUnique("glIsProgramPipeline");
  return interface_->IsProgramPipeline(pipeline);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsQuery(GLuint query) {
  MakeGlMockFunctionUnique("glIsQuery");
  return interface_->IsQuery(query);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsQueryEXT(GLuint query) {
  MakeGlMockFunctionUnique("glIsQueryEXT");
  return interface_->IsQuery(query);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glIsRenderbuffer(GLuint renderbuffer) {
  MakeGlMockFunctionUnique("glIsRenderbuffer");
  return interface_->IsRenderbufferEXT(renderbuffer);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsSampler(GLuint sampler) {
  MakeGlMockFunctionUnique("glIsSampler");
  return interface_->IsSampler(sampler);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsShader(GLuint shader) {
  MakeGlMockFunctionUnique("glIsShader");
  return interface_->IsShader(shader);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsSync(GLsync sync) {
  MakeGlMockFunctionUnique("glIsSync");
  return interface_->IsSync(sync);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsTexture(GLuint texture) {
  MakeGlMockFunctionUnique("glIsTexture");
  return interface_->IsTexture(texture);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glIsTransformFeedback(GLuint id) {
  MakeGlMockFunctionUnique("glIsTransformFeedback");
  return interface_->IsTransformFeedback(id);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glIsVertexArray(GLuint array) {
  MakeGlMockFunctionUnique("glIsVertexArray");
  return interface_->IsVertexArrayOES(array);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glIsVertexArrayOES(GLuint array) {
  MakeGlMockFunctionUnique("glIsVertexArrayOES");
  return interface_->IsVertexArrayOES(array);
}

void GL_BINDING_CALL MockGLInterface::Mock_glLineWidth(GLfloat width) {
  MakeGlMockFunctionUnique("glLineWidth");
  interface_->LineWidth(width);
}

void GL_BINDING_CALL MockGLInterface::Mock_glLinkProgram(GLuint program) {
  MakeGlMockFunctionUnique("glLinkProgram");
  interface_->LinkProgram(program);
}

void* GL_BINDING_CALL MockGLInterface::Mock_glMapBufferOES(GLenum target,
                                                           GLenum access) {
  MakeGlMockFunctionUnique("glMapBufferOES");
  return interface_->MapBuffer(target, access);
}

void* GL_BINDING_CALL
MockGLInterface::Mock_glMapBufferRange(GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr length,
                                       GLbitfield access) {
  MakeGlMockFunctionUnique("glMapBufferRange");
  return interface_->MapBufferRange(target, offset, length, access);
}

void* GL_BINDING_CALL
MockGLInterface::Mock_glMapBufferRangeEXT(GLenum target,
                                          GLintptr offset,
                                          GLsizeiptr length,
                                          GLbitfield access) {
  MakeGlMockFunctionUnique("glMapBufferRangeEXT");
  return interface_->MapBufferRange(target, offset, length, access);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMaxShaderCompilerThreadsKHR(GLuint count) {
  MakeGlMockFunctionUnique("glMaxShaderCompilerThreadsKHR");
  interface_->MaxShaderCompilerThreadsKHR(count);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMemoryBarrier(GLbitfield barriers) {
  MakeGlMockFunctionUnique("glMemoryBarrier");
  interface_->MemoryBarrierEXT(barriers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMemoryBarrierByRegion(GLbitfield barriers) {
  MakeGlMockFunctionUnique("glMemoryBarrierByRegion");
  interface_->MemoryBarrierByRegion(barriers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMemoryBarrierEXT(GLbitfield barriers) {
  MakeGlMockFunctionUnique("glMemoryBarrierEXT");
  interface_->MemoryBarrierEXT(barriers);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMemoryObjectParameterivEXT(GLuint memoryObject,
                                                   GLenum pname,
                                                   const GLint* param) {
  MakeGlMockFunctionUnique("glMemoryObjectParameterivEXT");
  interface_->MemoryObjectParameterivEXT(memoryObject, pname, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glMinSampleShading(GLfloat value) {
  MakeGlMockFunctionUnique("glMinSampleShading");
  interface_->MinSampleShading(value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMultiDrawArraysANGLE(GLenum mode,
                                             const GLint* firsts,
                                             const GLsizei* counts,
                                             GLsizei drawcount) {
  MakeGlMockFunctionUnique("glMultiDrawArraysANGLE");
  interface_->MultiDrawArraysANGLE(mode, firsts, counts, drawcount);
}

void GL_BINDING_CALL MockGLInterface::Mock_glMultiDrawArraysInstancedANGLE(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    GLsizei drawcount) {
  MakeGlMockFunctionUnique("glMultiDrawArraysInstancedANGLE");
  interface_->MultiDrawArraysInstancedANGLE(mode, firsts, counts,
                                            instanceCounts, drawcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMultiDrawArraysInstancedBaseInstanceANGLE(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    const GLuint* baseInstances,
    GLsizei drawcount) {
  MakeGlMockFunctionUnique("glMultiDrawArraysInstancedBaseInstanceANGLE");
  interface_->MultiDrawArraysInstancedBaseInstanceANGLE(
      mode, firsts, counts, instanceCounts, baseInstances, drawcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMultiDrawElementsANGLE(GLenum mode,
                                               const GLsizei* counts,
                                               GLenum type,
                                               const GLvoid* const* indices,
                                               GLsizei drawcount) {
  MakeGlMockFunctionUnique("glMultiDrawElementsANGLE");
  interface_->MultiDrawElementsANGLE(mode, counts, type, indices, drawcount);
}

void GL_BINDING_CALL MockGLInterface::Mock_glMultiDrawElementsInstancedANGLE(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    GLsizei drawcount) {
  MakeGlMockFunctionUnique("glMultiDrawElementsInstancedANGLE");
  interface_->MultiDrawElementsInstancedANGLE(mode, counts, type, indices,
                                              instanceCounts, drawcount);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    const GLint* baseVertices,
    const GLuint* baseInstances,
    GLsizei drawcount) {
  MakeGlMockFunctionUnique(
      "glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE");
  interface_->MultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, counts, type, indices, instanceCounts, baseVertices, baseInstances,
      drawcount);
}

void GL_BINDING_CALL MockGLInterface::Mock_glObjectLabel(GLenum identifier,
                                                         GLuint name,
                                                         GLsizei length,
                                                         const char* label) {
  MakeGlMockFunctionUnique("glObjectLabel");
  interface_->ObjectLabel(identifier, name, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glObjectLabelKHR(GLenum identifier,
                                                            GLuint name,
                                                            GLsizei length,
                                                            const char* label) {
  MakeGlMockFunctionUnique("glObjectLabelKHR");
  interface_->ObjectLabel(identifier, name, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glObjectPtrLabel(void* ptr,
                                                            GLsizei length,
                                                            const char* label) {
  MakeGlMockFunctionUnique("glObjectPtrLabel");
  interface_->ObjectPtrLabel(ptr, length, label);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glObjectPtrLabelKHR(void* ptr,
                                          GLsizei length,
                                          const char* label) {
  MakeGlMockFunctionUnique("glObjectPtrLabelKHR");
  interface_->ObjectPtrLabel(ptr, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPatchParameteri(GLenum pname,
                                                             GLint value) {
  MakeGlMockFunctionUnique("glPatchParameteri");
  interface_->PatchParameteri(pname, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPatchParameteriOES(GLenum pname,
                                                                GLint value) {
  MakeGlMockFunctionUnique("glPatchParameteriOES");
  interface_->PatchParameteri(pname, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPauseTransformFeedback(void) {
  MakeGlMockFunctionUnique("glPauseTransformFeedback");
  interface_->PauseTransformFeedback();
}

void GL_BINDING_CALL MockGLInterface::Mock_glPixelLocalStorageBarrierANGLE() {
  MakeGlMockFunctionUnique("glPixelLocalStorageBarrierANGLE");
  interface_->PixelLocalStorageBarrierANGLE();
}

void GL_BINDING_CALL MockGLInterface::Mock_glPixelStorei(GLenum pname,
                                                         GLint param) {
  MakeGlMockFunctionUnique("glPixelStorei");
  interface_->PixelStorei(pname, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPointParameteri(GLenum pname,
                                                             GLint param) {
  MakeGlMockFunctionUnique("glPointParameteri");
  interface_->PointParameteri(pname, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPolygonMode(GLenum face,
                                                         GLenum mode) {
  MakeGlMockFunctionUnique("glPolygonMode");
  interface_->PolygonMode(face, mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPolygonModeANGLE(GLenum face,
                                                              GLenum mode) {
  MakeGlMockFunctionUnique("glPolygonModeANGLE");
  interface_->PolygonModeANGLE(face, mode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPolygonOffset(GLfloat factor,
                                                           GLfloat units) {
  MakeGlMockFunctionUnique("glPolygonOffset");
  interface_->PolygonOffset(factor, units);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPolygonOffsetClampEXT(GLfloat factor,
                                              GLfloat units,
                                              GLfloat clamp) {
  MakeGlMockFunctionUnique("glPolygonOffsetClampEXT");
  interface_->PolygonOffsetClampEXT(factor, units, clamp);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPopDebugGroup() {
  MakeGlMockFunctionUnique("glPopDebugGroup");
  interface_->PopDebugGroup();
}

void GL_BINDING_CALL MockGLInterface::Mock_glPopDebugGroupKHR() {
  MakeGlMockFunctionUnique("glPopDebugGroupKHR");
  interface_->PopDebugGroup();
}

void GL_BINDING_CALL MockGLInterface::Mock_glPopGroupMarkerEXT(void) {
  MakeGlMockFunctionUnique("glPopGroupMarkerEXT");
  interface_->PopGroupMarkerEXT();
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPrimitiveRestartIndex(GLuint index) {
  MakeGlMockFunctionUnique("glPrimitiveRestartIndex");
  interface_->PrimitiveRestartIndex(index);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramBinary(GLuint program,
                                                           GLenum binaryFormat,
                                                           const GLvoid* binary,
                                                           GLsizei length) {
  MakeGlMockFunctionUnique("glProgramBinary");
  interface_->ProgramBinary(program, binaryFormat, binary, length);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramBinaryOES(GLuint program,
                                         GLenum binaryFormat,
                                         const GLvoid* binary,
                                         GLsizei length) {
  MakeGlMockFunctionUnique("glProgramBinaryOES");
  interface_->ProgramBinary(program, binaryFormat, binary, length);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramParameteri(GLuint program,
                                                               GLenum pname,
                                                               GLint value) {
  MakeGlMockFunctionUnique("glProgramParameteri");
  interface_->ProgramParameteri(program, pname, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform1f(GLuint program,
                                                              GLint location,
                                                              GLfloat v0) {
  MakeGlMockFunctionUnique("glProgramUniform1f");
  interface_->ProgramUniform1f(program, location, v0);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform1fv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniform1fv");
  interface_->ProgramUniform1fv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform1i(GLuint program,
                                                              GLint location,
                                                              GLint v0) {
  MakeGlMockFunctionUnique("glProgramUniform1i");
  interface_->ProgramUniform1i(program, location, v0);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform1iv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLint* value) {
  MakeGlMockFunctionUnique("glProgramUniform1iv");
  interface_->ProgramUniform1iv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform1ui(GLuint program,
                                                               GLint location,
                                                               GLuint v0) {
  MakeGlMockFunctionUnique("glProgramUniform1ui");
  interface_->ProgramUniform1ui(program, location, v0);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform1uiv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLuint* value) {
  MakeGlMockFunctionUnique("glProgramUniform1uiv");
  interface_->ProgramUniform1uiv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform2f(GLuint program,
                                                              GLint location,
                                                              GLfloat v0,
                                                              GLfloat v1) {
  MakeGlMockFunctionUnique("glProgramUniform2f");
  interface_->ProgramUniform2f(program, location, v0, v1);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform2fv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniform2fv");
  interface_->ProgramUniform2fv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform2i(GLuint program,
                                                              GLint location,
                                                              GLint v0,
                                                              GLint v1) {
  MakeGlMockFunctionUnique("glProgramUniform2i");
  interface_->ProgramUniform2i(program, location, v0, v1);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform2iv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLint* value) {
  MakeGlMockFunctionUnique("glProgramUniform2iv");
  interface_->ProgramUniform2iv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform2ui(GLuint program,
                                                               GLint location,
                                                               GLuint v0,
                                                               GLuint v1) {
  MakeGlMockFunctionUnique("glProgramUniform2ui");
  interface_->ProgramUniform2ui(program, location, v0, v1);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform2uiv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLuint* value) {
  MakeGlMockFunctionUnique("glProgramUniform2uiv");
  interface_->ProgramUniform2uiv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform3f(GLuint program,
                                                              GLint location,
                                                              GLfloat v0,
                                                              GLfloat v1,
                                                              GLfloat v2) {
  MakeGlMockFunctionUnique("glProgramUniform3f");
  interface_->ProgramUniform3f(program, location, v0, v1, v2);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform3fv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniform3fv");
  interface_->ProgramUniform3fv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform3i(GLuint program,
                                                              GLint location,
                                                              GLint v0,
                                                              GLint v1,
                                                              GLint v2) {
  MakeGlMockFunctionUnique("glProgramUniform3i");
  interface_->ProgramUniform3i(program, location, v0, v1, v2);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform3iv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLint* value) {
  MakeGlMockFunctionUnique("glProgramUniform3iv");
  interface_->ProgramUniform3iv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform3ui(GLuint program,
                                                               GLint location,
                                                               GLuint v0,
                                                               GLuint v1,
                                                               GLuint v2) {
  MakeGlMockFunctionUnique("glProgramUniform3ui");
  interface_->ProgramUniform3ui(program, location, v0, v1, v2);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform3uiv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLuint* value) {
  MakeGlMockFunctionUnique("glProgramUniform3uiv");
  interface_->ProgramUniform3uiv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform4f(GLuint program,
                                                              GLint location,
                                                              GLfloat v0,
                                                              GLfloat v1,
                                                              GLfloat v2,
                                                              GLfloat v3) {
  MakeGlMockFunctionUnique("glProgramUniform4f");
  interface_->ProgramUniform4f(program, location, v0, v1, v2, v3);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform4fv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniform4fv");
  interface_->ProgramUniform4fv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform4i(GLuint program,
                                                              GLint location,
                                                              GLint v0,
                                                              GLint v1,
                                                              GLint v2,
                                                              GLint v3) {
  MakeGlMockFunctionUnique("glProgramUniform4i");
  interface_->ProgramUniform4i(program, location, v0, v1, v2, v3);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform4iv(GLuint program,
                                          GLint location,
                                          GLsizei count,
                                          const GLint* value) {
  MakeGlMockFunctionUnique("glProgramUniform4iv");
  interface_->ProgramUniform4iv(program, location, count, value);
}

void GL_BINDING_CALL MockGLInterface::Mock_glProgramUniform4ui(GLuint program,
                                                               GLint location,
                                                               GLuint v0,
                                                               GLuint v1,
                                                               GLuint v2,
                                                               GLuint v3) {
  MakeGlMockFunctionUnique("glProgramUniform4ui");
  interface_->ProgramUniform4ui(program, location, v0, v1, v2, v3);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniform4uiv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLuint* value) {
  MakeGlMockFunctionUnique("glProgramUniform4uiv");
  interface_->ProgramUniform4uiv(program, location, count, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix2fv(GLuint program,
                                                GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix2fv");
  interface_->ProgramUniformMatrix2fv(program, location, count, transpose,
                                      value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix2x3fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix2x3fv");
  interface_->ProgramUniformMatrix2x3fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix2x4fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix2x4fv");
  interface_->ProgramUniformMatrix2x4fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix3fv(GLuint program,
                                                GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix3fv");
  interface_->ProgramUniformMatrix3fv(program, location, count, transpose,
                                      value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix3x2fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix3x2fv");
  interface_->ProgramUniformMatrix3x2fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix3x4fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix3x4fv");
  interface_->ProgramUniformMatrix3x4fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix4fv(GLuint program,
                                                GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix4fv");
  interface_->ProgramUniformMatrix4fv(program, location, count, transpose,
                                      value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix4x2fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix4x2fv");
  interface_->ProgramUniformMatrix4x2fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProgramUniformMatrix4x3fv(GLuint program,
                                                  GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  MakeGlMockFunctionUnique("glProgramUniformMatrix4x3fv");
  interface_->ProgramUniformMatrix4x3fv(program, location, count, transpose,
                                        value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glProvokingVertexANGLE(GLenum provokeMode) {
  MakeGlMockFunctionUnique("glProvokingVertexANGLE");
  interface_->ProvokingVertexANGLE(provokeMode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPushDebugGroup(GLenum source,
                                       GLuint id,
                                       GLsizei length,
                                       const char* message) {
  MakeGlMockFunctionUnique("glPushDebugGroup");
  interface_->PushDebugGroup(source, id, length, message);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPushDebugGroupKHR(GLenum source,
                                          GLuint id,
                                          GLsizei length,
                                          const char* message) {
  MakeGlMockFunctionUnique("glPushDebugGroupKHR");
  interface_->PushDebugGroup(source, id, length, message);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPushGroupMarkerEXT(GLsizei length, const char* marker) {
  MakeGlMockFunctionUnique("glPushGroupMarkerEXT");
  interface_->PushGroupMarkerEXT(length, marker);
}

void GL_BINDING_CALL MockGLInterface::Mock_glQueryCounterEXT(GLuint id,
                                                             GLenum target) {
  MakeGlMockFunctionUnique("glQueryCounterEXT");
  interface_->QueryCounter(id, target);
}

void GL_BINDING_CALL MockGLInterface::Mock_glReadBuffer(GLenum src) {
  MakeGlMockFunctionUnique("glReadBuffer");
  interface_->ReadBuffer(src);
}

void GL_BINDING_CALL MockGLInterface::Mock_glReadPixels(GLint x,
                                                        GLint y,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLenum format,
                                                        GLenum type,
                                                        void* pixels) {
  MakeGlMockFunctionUnique("glReadPixels");
  interface_->ReadPixels(x, y, width, height, format, type, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glReadPixelsRobustANGLE(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLsizei* columns,
                                              GLsizei* rows,
                                              void* pixels) {
  MakeGlMockFunctionUnique("glReadPixelsRobustANGLE");
  interface_->ReadPixelsRobustANGLE(x, y, width, height, format, type, bufSize,
                                    length, columns, rows, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glReadnPixelsRobustANGLE(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* columns,
                                               GLsizei* rows,
                                               void* data) {
  MakeGlMockFunctionUnique("glReadnPixelsRobustANGLE");
  interface_->ReadnPixelsRobustANGLE(x, y, width, height, format, type, bufSize,
                                     length, columns, rows, data);
}

void GL_BINDING_CALL MockGLInterface::Mock_glReleaseShaderCompiler(void) {
  MakeGlMockFunctionUnique("glReleaseShaderCompiler");
  interface_->ReleaseShaderCompiler();
}

void GL_BINDING_CALL
MockGLInterface::Mock_glReleaseTexturesANGLE(GLuint numTextures,
                                             const GLuint* textures,
                                             GLenum* layouts) {
  MakeGlMockFunctionUnique("glReleaseTexturesANGLE");
  interface_->ReleaseTexturesANGLE(numTextures, textures, layouts);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorage(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorage");
  interface_->RenderbufferStorageEXT(target, internalformat, width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorageMultisample(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorageMultisample");
  interface_->RenderbufferStorageMultisample(target, samples, internalformat,
                                             width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorageMultisampleANGLE(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorageMultisampleANGLE");
  interface_->RenderbufferStorageMultisample(target, samples, internalformat,
                                             width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorageMultisampleAdvancedAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorageMultisampleAdvancedAMD");
  interface_->RenderbufferStorageMultisampleAdvancedAMD(
      target, samples, storageSamples, internalformat, width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorageMultisampleEXT(GLenum target,
                                                          GLsizei samples,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorageMultisampleEXT");
  interface_->RenderbufferStorageMultisampleEXT(target, samples, internalformat,
                                                width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRenderbufferStorageMultisampleIMG(GLenum target,
                                                          GLsizei samples,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height) {
  MakeGlMockFunctionUnique("glRenderbufferStorageMultisampleIMG");
  interface_->RenderbufferStorageMultisampleEXT(target, samples, internalformat,
                                                width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glRequestExtensionANGLE(const char* name) {
  MakeGlMockFunctionUnique("glRequestExtensionANGLE");
  interface_->RequestExtensionANGLE(name);
}

void GL_BINDING_CALL MockGLInterface::Mock_glResumeTransformFeedback(void) {
  MakeGlMockFunctionUnique("glResumeTransformFeedback");
  interface_->ResumeTransformFeedback();
}

void GL_BINDING_CALL MockGLInterface::Mock_glSampleCoverage(GLclampf value,
                                                            GLboolean invert) {
  MakeGlMockFunctionUnique("glSampleCoverage");
  interface_->SampleCoverage(value, invert);
}

void GL_BINDING_CALL MockGLInterface::Mock_glSampleMaski(GLuint maskNumber,
                                                         GLbitfield mask) {
  MakeGlMockFunctionUnique("glSampleMaski");
  interface_->SampleMaski(maskNumber, mask);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameterIivRobustANGLE(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLint* param) {
  MakeGlMockFunctionUnique("glSamplerParameterIivRobustANGLE");
  interface_->SamplerParameterIivRobustANGLE(sampler, pname, bufSize, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameterIuivRobustANGLE(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLuint* param) {
  MakeGlMockFunctionUnique("glSamplerParameterIuivRobustANGLE");
  interface_->SamplerParameterIuivRobustANGLE(sampler, pname, bufSize, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glSamplerParameterf(GLuint sampler,
                                                               GLenum pname,
                                                               GLfloat param) {
  MakeGlMockFunctionUnique("glSamplerParameterf");
  interface_->SamplerParameterf(sampler, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameterfv(GLuint sampler,
                                           GLenum pname,
                                           const GLfloat* params) {
  MakeGlMockFunctionUnique("glSamplerParameterfv");
  interface_->SamplerParameterfv(sampler, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameterfvRobustANGLE(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      const GLfloat* param) {
  MakeGlMockFunctionUnique("glSamplerParameterfvRobustANGLE");
  interface_->SamplerParameterfvRobustANGLE(sampler, pname, bufSize, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glSamplerParameteri(GLuint sampler,
                                                               GLenum pname,
                                                               GLint param) {
  MakeGlMockFunctionUnique("glSamplerParameteri");
  interface_->SamplerParameteri(sampler, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameteriv(GLuint sampler,
                                           GLenum pname,
                                           const GLint* params) {
  MakeGlMockFunctionUnique("glSamplerParameteriv");
  interface_->SamplerParameteriv(sampler, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSamplerParameterivRobustANGLE(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      const GLint* param) {
  MakeGlMockFunctionUnique("glSamplerParameterivRobustANGLE");
  interface_->SamplerParameterivRobustANGLE(sampler, pname, bufSize, param);
}

void GL_BINDING_CALL MockGLInterface::Mock_glScissor(GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height) {
  MakeGlMockFunctionUnique("glScissor");
  interface_->Scissor(x, y, width, height);
}

void GL_BINDING_CALL MockGLInterface::Mock_glSetFenceNV(GLuint fence,
                                                        GLenum condition) {
  MakeGlMockFunctionUnique("glSetFenceNV");
  interface_->SetFenceNV(fence, condition);
}

void GL_BINDING_CALL MockGLInterface::Mock_glShaderBinary(GLsizei n,
                                                          const GLuint* shaders,
                                                          GLenum binaryformat,
                                                          const void* binary,
                                                          GLsizei length) {
  MakeGlMockFunctionUnique("glShaderBinary");
  interface_->ShaderBinary(n, shaders, binaryformat, binary, length);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glShaderSource(GLuint shader,
                                     GLsizei count,
                                     const char* const* str,
                                     const GLint* length) {
  MakeGlMockFunctionUnique("glShaderSource");
  interface_->ShaderSource(shader, count, str, length);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glSignalSemaphoreEXT(GLuint semaphore,
                                           GLuint numBufferBarriers,
                                           const GLuint* buffers,
                                           GLuint numTextureBarriers,
                                           const GLuint* textures,
                                           const GLenum* dstLayouts) {
  MakeGlMockFunctionUnique("glSignalSemaphoreEXT");
  interface_->SignalSemaphoreEXT(semaphore, numBufferBarriers, buffers,
                                 numTextureBarriers, textures, dstLayouts);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glStartTilingQCOM(GLuint x,
                                        GLuint y,
                                        GLuint width,
                                        GLuint height,
                                        GLbitfield preserveMask) {
  MakeGlMockFunctionUnique("glStartTilingQCOM");
  interface_->StartTilingQCOM(x, y, width, height, preserveMask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilFunc(GLenum func,
                                                         GLint ref,
                                                         GLuint mask) {
  MakeGlMockFunctionUnique("glStencilFunc");
  interface_->StencilFunc(func, ref, mask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilFuncSeparate(GLenum face,
                                                                 GLenum func,
                                                                 GLint ref,
                                                                 GLuint mask) {
  MakeGlMockFunctionUnique("glStencilFuncSeparate");
  interface_->StencilFuncSeparate(face, func, ref, mask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilMask(GLuint mask) {
  MakeGlMockFunctionUnique("glStencilMask");
  interface_->StencilMask(mask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilMaskSeparate(GLenum face,
                                                                 GLuint mask) {
  MakeGlMockFunctionUnique("glStencilMaskSeparate");
  interface_->StencilMaskSeparate(face, mask);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilOp(GLenum fail,
                                                       GLenum zfail,
                                                       GLenum zpass) {
  MakeGlMockFunctionUnique("glStencilOp");
  interface_->StencilOp(fail, zfail, zpass);
}

void GL_BINDING_CALL MockGLInterface::Mock_glStencilOpSeparate(GLenum face,
                                                               GLenum fail,
                                                               GLenum zfail,
                                                               GLenum zpass) {
  MakeGlMockFunctionUnique("glStencilOpSeparate");
  interface_->StencilOpSeparate(face, fail, zfail, zpass);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glTestFenceNV(GLuint fence) {
  MakeGlMockFunctionUnique("glTestFenceNV");
  return interface_->TestFenceNV(fence);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexBuffer(GLenum target,
                                                       GLenum internalformat,
                                                       GLuint buffer) {
  MakeGlMockFunctionUnique("glTexBuffer");
  interface_->TexBuffer(target, internalformat, buffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexBufferEXT(GLenum target,
                                                          GLenum internalformat,
                                                          GLuint buffer) {
  MakeGlMockFunctionUnique("glTexBufferEXT");
  interface_->TexBuffer(target, internalformat, buffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexBufferOES(GLenum target,
                                                          GLenum internalformat,
                                                          GLuint buffer) {
  MakeGlMockFunctionUnique("glTexBufferOES");
  interface_->TexBuffer(target, internalformat, buffer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexBufferRange(GLenum target,
                                       GLenum internalformat,
                                       GLuint buffer,
                                       GLintptr offset,
                                       GLsizeiptr size) {
  MakeGlMockFunctionUnique("glTexBufferRange");
  interface_->TexBufferRange(target, internalformat, buffer, offset, size);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexBufferRangeEXT(GLenum target,
                                          GLenum internalformat,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizeiptr size) {
  MakeGlMockFunctionUnique("glTexBufferRangeEXT");
  interface_->TexBufferRange(target, internalformat, buffer, offset, size);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexBufferRangeOES(GLenum target,
                                          GLenum internalformat,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizeiptr size) {
  MakeGlMockFunctionUnique("glTexBufferRangeOES");
  interface_->TexBufferRange(target, internalformat, buffer, offset, size);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexImage2D(GLenum target,
                                                        GLint level,
                                                        GLint internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLint border,
                                                        GLenum format,
                                                        GLenum type,
                                                        const void* pixels) {
  MakeGlMockFunctionUnique("glTexImage2D");
  interface_->TexImage2D(target, level, internalformat, width, height, border,
                         format, type, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexImage2DExternalANGLE(GLenum target,
                                                GLint level,
                                                GLint internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border,
                                                GLenum format,
                                                GLenum type) {
  MakeGlMockFunctionUnique("glTexImage2DExternalANGLE");
  interface_->TexImage2DExternalANGLE(target, level, internalformat, width,
                                      height, border, format, type);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexImage2DRobustANGLE(GLenum target,
                                              GLint level,
                                              GLint internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  MakeGlMockFunctionUnique("glTexImage2DRobustANGLE");
  interface_->TexImage2DRobustANGLE(target, level, internalformat, width,
                                    height, border, format, type, bufSize,
                                    pixels);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexImage3D(GLenum target,
                                                        GLint level,
                                                        GLint internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        GLint border,
                                                        GLenum format,
                                                        GLenum type,
                                                        const void* pixels) {
  MakeGlMockFunctionUnique("glTexImage3D");
  interface_->TexImage3D(target, level, internalformat, width, height, depth,
                         border, format, type, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexImage3DRobustANGLE(GLenum target,
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
  MakeGlMockFunctionUnique("glTexImage3DRobustANGLE");
  interface_->TexImage3DRobustANGLE(target, level, internalformat, width,
                                    height, depth, border, format, type,
                                    bufSize, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameterIivRobustANGLE(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* params) {
  MakeGlMockFunctionUnique("glTexParameterIivRobustANGLE");
  interface_->TexParameterIivRobustANGLE(target, pname, bufSize, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameterIuivRobustANGLE(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLuint* params) {
  MakeGlMockFunctionUnique("glTexParameterIuivRobustANGLE");
  interface_->TexParameterIuivRobustANGLE(target, pname, bufSize, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexParameterf(GLenum target,
                                                           GLenum pname,
                                                           GLfloat param) {
  MakeGlMockFunctionUnique("glTexParameterf");
  interface_->TexParameterf(target, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameterfv(GLenum target,
                                       GLenum pname,
                                       const GLfloat* params) {
  MakeGlMockFunctionUnique("glTexParameterfv");
  interface_->TexParameterfv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameterfvRobustANGLE(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLfloat* params) {
  MakeGlMockFunctionUnique("glTexParameterfvRobustANGLE");
  interface_->TexParameterfvRobustANGLE(target, pname, bufSize, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexParameteri(GLenum target,
                                                           GLenum pname,
                                                           GLint param) {
  MakeGlMockFunctionUnique("glTexParameteri");
  interface_->TexParameteri(target, pname, param);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameteriv(GLenum target,
                                       GLenum pname,
                                       const GLint* params) {
  MakeGlMockFunctionUnique("glTexParameteriv");
  interface_->TexParameteriv(target, pname, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexParameterivRobustANGLE(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLint* params) {
  MakeGlMockFunctionUnique("glTexParameterivRobustANGLE");
  interface_->TexParameterivRobustANGLE(target, pname, bufSize, params);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexStorage2D(GLenum target,
                                                          GLsizei levels,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height) {
  MakeGlMockFunctionUnique("glTexStorage2D");
  interface_->TexStorage2DEXT(target, levels, internalformat, width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexStorage2DEXT(GLenum target,
                                        GLsizei levels,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height) {
  MakeGlMockFunctionUnique("glTexStorage2DEXT");
  interface_->TexStorage2DEXT(target, levels, internalformat, width, height);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexStorage2DMultisample(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLboolean fixedsamplelocations) {
  MakeGlMockFunctionUnique("glTexStorage2DMultisample");
  interface_->TexStorage2DMultisample(target, samples, internalformat, width,
                                      height, fixedsamplelocations);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexStorage3D(GLenum target,
                                                          GLsizei levels,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth) {
  MakeGlMockFunctionUnique("glTexStorage3D");
  interface_->TexStorage3D(target, levels, internalformat, width, height,
                           depth);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexStorageMem2DEXT(GLenum target,
                                           GLsizei levels,
                                           GLenum internalFormat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLuint memory,
                                           GLuint64 offset) {
  MakeGlMockFunctionUnique("glTexStorageMem2DEXT");
  interface_->TexStorageMem2DEXT(target, levels, internalFormat, width, height,
                                 memory, offset);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexStorageMemFlags2DANGLE(
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLuint memory,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void* imageCreateInfoPNext) {
  MakeGlMockFunctionUnique("glTexStorageMemFlags2DANGLE");
  interface_->TexStorageMemFlags2DANGLE(target, levels, internalFormat, width,
                                        height, memory, offset, createFlags,
                                        usageFlags, imageCreateInfoPNext);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexSubImage2D(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLenum format,
                                                           GLenum type,
                                                           const void* pixels) {
  MakeGlMockFunctionUnique("glTexSubImage2D");
  interface_->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                            format, type, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexSubImage2DRobustANGLE(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 GLsizei bufSize,
                                                 const void* pixels) {
  MakeGlMockFunctionUnique("glTexSubImage2DRobustANGLE");
  interface_->TexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width,
                                       height, format, type, bufSize, pixels);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexSubImage3D(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLint zoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLsizei depth,
                                                           GLenum format,
                                                           GLenum type,
                                                           const void* pixels) {
  MakeGlMockFunctionUnique("glTexSubImage3D");
  interface_->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                            height, depth, format, type, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexSubImage3DRobustANGLE(GLenum target,
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
  MakeGlMockFunctionUnique("glTexSubImage3DRobustANGLE");
  interface_->TexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, type,
                                       bufSize, pixels);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTransformFeedbackVaryings(GLuint program,
                                                  GLsizei count,
                                                  const char* const* varyings,
                                                  GLenum bufferMode) {
  MakeGlMockFunctionUnique("glTransformFeedbackVaryings");
  interface_->TransformFeedbackVaryings(program, count, varyings, bufferMode);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1f(GLint location,
                                                       GLfloat x) {
  MakeGlMockFunctionUnique("glUniform1f");
  interface_->Uniform1f(location, x);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1fv(GLint location,
                                                        GLsizei count,
                                                        const GLfloat* v) {
  MakeGlMockFunctionUnique("glUniform1fv");
  interface_->Uniform1fv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1i(GLint location,
                                                       GLint x) {
  MakeGlMockFunctionUnique("glUniform1i");
  interface_->Uniform1i(location, x);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1iv(GLint location,
                                                        GLsizei count,
                                                        const GLint* v) {
  MakeGlMockFunctionUnique("glUniform1iv");
  interface_->Uniform1iv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1ui(GLint location,
                                                        GLuint v0) {
  MakeGlMockFunctionUnique("glUniform1ui");
  interface_->Uniform1ui(location, v0);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform1uiv(GLint location,
                                                         GLsizei count,
                                                         const GLuint* v) {
  MakeGlMockFunctionUnique("glUniform1uiv");
  interface_->Uniform1uiv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2f(GLint location,
                                                       GLfloat x,
                                                       GLfloat y) {
  MakeGlMockFunctionUnique("glUniform2f");
  interface_->Uniform2f(location, x, y);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2fv(GLint location,
                                                        GLsizei count,
                                                        const GLfloat* v) {
  MakeGlMockFunctionUnique("glUniform2fv");
  interface_->Uniform2fv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2i(GLint location,
                                                       GLint x,
                                                       GLint y) {
  MakeGlMockFunctionUnique("glUniform2i");
  interface_->Uniform2i(location, x, y);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2iv(GLint location,
                                                        GLsizei count,
                                                        const GLint* v) {
  MakeGlMockFunctionUnique("glUniform2iv");
  interface_->Uniform2iv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2ui(GLint location,
                                                        GLuint v0,
                                                        GLuint v1) {
  MakeGlMockFunctionUnique("glUniform2ui");
  interface_->Uniform2ui(location, v0, v1);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform2uiv(GLint location,
                                                         GLsizei count,
                                                         const GLuint* v) {
  MakeGlMockFunctionUnique("glUniform2uiv");
  interface_->Uniform2uiv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3f(GLint location,
                                                       GLfloat x,
                                                       GLfloat y,
                                                       GLfloat z) {
  MakeGlMockFunctionUnique("glUniform3f");
  interface_->Uniform3f(location, x, y, z);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3fv(GLint location,
                                                        GLsizei count,
                                                        const GLfloat* v) {
  MakeGlMockFunctionUnique("glUniform3fv");
  interface_->Uniform3fv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3i(GLint location,
                                                       GLint x,
                                                       GLint y,
                                                       GLint z) {
  MakeGlMockFunctionUnique("glUniform3i");
  interface_->Uniform3i(location, x, y, z);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3iv(GLint location,
                                                        GLsizei count,
                                                        const GLint* v) {
  MakeGlMockFunctionUnique("glUniform3iv");
  interface_->Uniform3iv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3ui(GLint location,
                                                        GLuint v0,
                                                        GLuint v1,
                                                        GLuint v2) {
  MakeGlMockFunctionUnique("glUniform3ui");
  interface_->Uniform3ui(location, v0, v1, v2);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform3uiv(GLint location,
                                                         GLsizei count,
                                                         const GLuint* v) {
  MakeGlMockFunctionUnique("glUniform3uiv");
  interface_->Uniform3uiv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4f(GLint location,
                                                       GLfloat x,
                                                       GLfloat y,
                                                       GLfloat z,
                                                       GLfloat w) {
  MakeGlMockFunctionUnique("glUniform4f");
  interface_->Uniform4f(location, x, y, z, w);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4fv(GLint location,
                                                        GLsizei count,
                                                        const GLfloat* v) {
  MakeGlMockFunctionUnique("glUniform4fv");
  interface_->Uniform4fv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4i(GLint location,
                                                       GLint x,
                                                       GLint y,
                                                       GLint z,
                                                       GLint w) {
  MakeGlMockFunctionUnique("glUniform4i");
  interface_->Uniform4i(location, x, y, z, w);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4iv(GLint location,
                                                        GLsizei count,
                                                        const GLint* v) {
  MakeGlMockFunctionUnique("glUniform4iv");
  interface_->Uniform4iv(location, count, v);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4ui(GLint location,
                                                        GLuint v0,
                                                        GLuint v1,
                                                        GLuint v2,
                                                        GLuint v3) {
  MakeGlMockFunctionUnique("glUniform4ui");
  interface_->Uniform4ui(location, v0, v1, v2, v3);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUniform4uiv(GLint location,
                                                         GLsizei count,
                                                         const GLuint* v) {
  MakeGlMockFunctionUnique("glUniform4uiv");
  interface_->Uniform4uiv(location, count, v);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformBlockBinding(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLuint uniformBlockBinding) {
  MakeGlMockFunctionUnique("glUniformBlockBinding");
  interface_->UniformBlockBinding(program, uniformBlockIndex,
                                  uniformBlockBinding);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix2fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix2fv");
  interface_->UniformMatrix2fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix2x3fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix2x3fv");
  interface_->UniformMatrix2x3fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix2x4fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix2x4fv");
  interface_->UniformMatrix2x4fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix3fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix3fv");
  interface_->UniformMatrix3fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix3x2fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix3x2fv");
  interface_->UniformMatrix3x2fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix3x4fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix3x4fv");
  interface_->UniformMatrix3x4fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix4fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix4fv");
  interface_->UniformMatrix4fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix4x2fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix4x2fv");
  interface_->UniformMatrix4x2fv(location, count, transpose, value);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glUniformMatrix4x3fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  MakeGlMockFunctionUnique("glUniformMatrix4x3fv");
  interface_->UniformMatrix4x3fv(location, count, transpose, value);
}

GLboolean GL_BINDING_CALL MockGLInterface::Mock_glUnmapBuffer(GLenum target) {
  MakeGlMockFunctionUnique("glUnmapBuffer");
  return interface_->UnmapBuffer(target);
}

GLboolean GL_BINDING_CALL
MockGLInterface::Mock_glUnmapBufferOES(GLenum target) {
  MakeGlMockFunctionUnique("glUnmapBufferOES");
  return interface_->UnmapBuffer(target);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUseProgram(GLuint program) {
  MakeGlMockFunctionUnique("glUseProgram");
  interface_->UseProgram(program);
}

void GL_BINDING_CALL MockGLInterface::Mock_glUseProgramStages(GLuint pipeline,
                                                              GLbitfield stages,
                                                              GLuint program) {
  MakeGlMockFunctionUnique("glUseProgramStages");
  interface_->UseProgramStages(pipeline, stages, program);
}

void GL_BINDING_CALL MockGLInterface::Mock_glValidateProgram(GLuint program) {
  MakeGlMockFunctionUnique("glValidateProgram");
  interface_->ValidateProgram(program);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glValidateProgramPipeline(GLuint pipeline) {
  MakeGlMockFunctionUnique("glValidateProgramPipeline");
  interface_->ValidateProgramPipeline(pipeline);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttrib1f(GLuint indx,
                                                            GLfloat x) {
  MakeGlMockFunctionUnique("glVertexAttrib1f");
  interface_->VertexAttrib1f(indx, x);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
  MakeGlMockFunctionUnique("glVertexAttrib1fv");
  interface_->VertexAttrib1fv(indx, values);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttrib2f(GLuint indx,
                                                            GLfloat x,
                                                            GLfloat y) {
  MakeGlMockFunctionUnique("glVertexAttrib2f");
  interface_->VertexAttrib2f(indx, x, y);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
  MakeGlMockFunctionUnique("glVertexAttrib2fv");
  interface_->VertexAttrib2fv(indx, values);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttrib3f(GLuint indx,
                                                            GLfloat x,
                                                            GLfloat y,
                                                            GLfloat z) {
  MakeGlMockFunctionUnique("glVertexAttrib3f");
  interface_->VertexAttrib3f(indx, x, y, z);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
  MakeGlMockFunctionUnique("glVertexAttrib3fv");
  interface_->VertexAttrib3fv(indx, values);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttrib4f(GLuint indx,
                                                            GLfloat x,
                                                            GLfloat y,
                                                            GLfloat z,
                                                            GLfloat w) {
  MakeGlMockFunctionUnique("glVertexAttrib4f");
  interface_->VertexAttrib4f(indx, x, y, z, w);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  MakeGlMockFunctionUnique("glVertexAttrib4fv");
  interface_->VertexAttrib4fv(indx, values);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribBinding(GLuint attribindex,
                                            GLuint bindingindex) {
  MakeGlMockFunctionUnique("glVertexAttribBinding");
  interface_->VertexAttribBinding(attribindex, bindingindex);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribDivisor(GLuint index, GLuint divisor) {
  MakeGlMockFunctionUnique("glVertexAttribDivisor");
  interface_->VertexAttribDivisorANGLE(index, divisor);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
  MakeGlMockFunctionUnique("glVertexAttribDivisorANGLE");
  interface_->VertexAttribDivisorANGLE(index, divisor);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribDivisorEXT(GLuint index, GLuint divisor) {
  MakeGlMockFunctionUnique("glVertexAttribDivisorEXT");
  interface_->VertexAttribDivisorANGLE(index, divisor);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribFormat(GLuint attribindex,
                                           GLint size,
                                           GLenum type,
                                           GLboolean normalized,
                                           GLuint relativeoffset) {
  MakeGlMockFunctionUnique("glVertexAttribFormat");
  interface_->VertexAttribFormat(attribindex, size, type, normalized,
                                 relativeoffset);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttribI4i(GLuint indx,
                                                             GLint x,
                                                             GLint y,
                                                             GLint z,
                                                             GLint w) {
  MakeGlMockFunctionUnique("glVertexAttribI4i");
  interface_->VertexAttribI4i(indx, x, y, z, w);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribI4iv(GLuint indx, const GLint* values) {
  MakeGlMockFunctionUnique("glVertexAttribI4iv");
  interface_->VertexAttribI4iv(indx, values);
}

void GL_BINDING_CALL MockGLInterface::Mock_glVertexAttribI4ui(GLuint indx,
                                                              GLuint x,
                                                              GLuint y,
                                                              GLuint z,
                                                              GLuint w) {
  MakeGlMockFunctionUnique("glVertexAttribI4ui");
  interface_->VertexAttribI4ui(indx, x, y, z, w);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribI4uiv(GLuint indx, const GLuint* values) {
  MakeGlMockFunctionUnique("glVertexAttribI4uiv");
  interface_->VertexAttribI4uiv(indx, values);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribIFormat(GLuint attribindex,
                                            GLint size,
                                            GLenum type,
                                            GLuint relativeoffset) {
  MakeGlMockFunctionUnique("glVertexAttribIFormat");
  interface_->VertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribIPointer(GLuint indx,
                                             GLint size,
                                             GLenum type,
                                             GLsizei stride,
                                             const void* ptr) {
  MakeGlMockFunctionUnique("glVertexAttribIPointer");
  interface_->VertexAttribIPointer(indx, size, type, stride, ptr);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexAttribPointer(GLuint indx,
                                            GLint size,
                                            GLenum type,
                                            GLboolean normalized,
                                            GLsizei stride,
                                            const void* ptr) {
  MakeGlMockFunctionUnique("glVertexAttribPointer");
  interface_->VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glVertexBindingDivisor(GLuint bindingindex,
                                             GLuint divisor) {
  MakeGlMockFunctionUnique("glVertexBindingDivisor");
  interface_->VertexBindingDivisor(bindingindex, divisor);
}

void GL_BINDING_CALL MockGLInterface::Mock_glViewport(GLint x,
                                                      GLint y,
                                                      GLsizei width,
                                                      GLsizei height) {
  MakeGlMockFunctionUnique("glViewport");
  interface_->Viewport(x, y, width, height);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glWaitSemaphoreEXT(GLuint semaphore,
                                         GLuint numBufferBarriers,
                                         const GLuint* buffers,
                                         GLuint numTextureBarriers,
                                         const GLuint* textures,
                                         const GLenum* srcLayouts) {
  MakeGlMockFunctionUnique("glWaitSemaphoreEXT");
  interface_->WaitSemaphoreEXT(semaphore, numBufferBarriers, buffers,
                               numTextureBarriers, textures, srcLayouts);
}

void GL_BINDING_CALL MockGLInterface::Mock_glWaitSync(GLsync sync,
                                                      GLbitfield flags,
                                                      GLuint64 timeout) {
  MakeGlMockFunctionUnique("glWaitSync");
  interface_->WaitSync(sync, flags, timeout);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glWindowRectanglesEXT(GLenum mode,
                                            GLsizei n,
                                            const GLint* box) {
  MakeGlMockFunctionUnique("glWindowRectanglesEXT");
  interface_->WindowRectanglesEXT(mode, n, box);
}

static void MockGlInvalidFunction() {
  NOTREACHED_IN_MIGRATION();
}

GLFunctionPointerType GL_BINDING_CALL
MockGLInterface::GetGLProcAddress(const char* name) {
  if (strcmp(name, "glAcquireTexturesANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glAcquireTexturesANGLE);
  if (strcmp(name, "glActiveShaderProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glActiveShaderProgram);
  if (strcmp(name, "glActiveTexture") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glActiveTexture);
  if (strcmp(name, "glAttachShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glAttachShader);
  if (strcmp(name, "glBeginPixelLocalStorageANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBeginPixelLocalStorageANGLE);
  if (strcmp(name, "glBeginQuery") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBeginQuery);
  if (strcmp(name, "glBeginQueryEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBeginQueryEXT);
  if (strcmp(name, "glBeginTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBeginTransformFeedback);
  if (strcmp(name, "glBindAttribLocation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindAttribLocation);
  if (strcmp(name, "glBindBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBuffer);
  if (strcmp(name, "glBindBufferBase") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBufferBase);
  if (strcmp(name, "glBindBufferRange") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBufferRange);
  if (strcmp(name, "glBindFragDataLocationEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindFragDataLocationEXT);
  if (strcmp(name, "glBindFragDataLocationIndexedEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindFragDataLocationIndexedEXT);
  if (strcmp(name, "glBindFramebuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindFramebuffer);
  if (strcmp(name, "glBindImageTexture") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindImageTexture);
  if (strcmp(name, "glBindImageTextureEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindImageTextureEXT);
  if (strcmp(name, "glBindProgramPipeline") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindProgramPipeline);
  if (strcmp(name, "glBindRenderbuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindRenderbuffer);
  if (strcmp(name, "glBindSampler") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindSampler);
  if (strcmp(name, "glBindTexture") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindTexture);
  if (strcmp(name, "glBindTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindTransformFeedback);
  if (strcmp(name, "glBindUniformLocationCHROMIUM") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindUniformLocationCHROMIUM);
  if (strcmp(name, "glBindVertexArray") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindVertexArray);
  if (strcmp(name, "glBindVertexArrayOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindVertexArrayOES);
  if (strcmp(name, "glBindVertexBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindVertexBuffer);
  if (strcmp(name, "glBlendBarrierKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendBarrierKHR);
  if (strcmp(name, "glBlendBarrierNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendBarrierNV);
  if (strcmp(name, "glBlendColor") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendColor);
  if (strcmp(name, "glBlendEquation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendEquation);
  if (strcmp(name, "glBlendEquationSeparate") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendEquationSeparate);
  if (strcmp(name, "glBlendEquationSeparatei") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendEquationSeparatei);
  if (strcmp(name, "glBlendEquationSeparateiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendEquationSeparateiOES);
  if (strcmp(name, "glBlendEquationi") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendEquationi);
  if (strcmp(name, "glBlendEquationiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendEquationiOES);
  if (strcmp(name, "glBlendFunc") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFunc);
  if (strcmp(name, "glBlendFuncSeparate") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFuncSeparate);
  if (strcmp(name, "glBlendFuncSeparatei") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFuncSeparatei);
  if (strcmp(name, "glBlendFuncSeparateiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendFuncSeparateiOES);
  if (strcmp(name, "glBlendFunci") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFunci);
  if (strcmp(name, "glBlendFunciOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFunciOES);
  if (strcmp(name, "glBlitFramebuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebuffer);
  if (strcmp(name, "glBlitFramebufferANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebufferANGLE);
  if (strcmp(name, "glBlitFramebufferNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebufferNV);
  if (strcmp(name, "glBufferData") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBufferData);
  if (strcmp(name, "glBufferSubData") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBufferSubData);
  if (strcmp(name, "glCheckFramebufferStatus") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCheckFramebufferStatus);
  if (strcmp(name, "glClear") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClear);
  if (strcmp(name, "glClearBufferfi") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferfi);
  if (strcmp(name, "glClearBufferfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferfv);
  if (strcmp(name, "glClearBufferiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferiv);
  if (strcmp(name, "glClearBufferuiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferuiv);
  if (strcmp(name, "glClearColor") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearColor);
  if (strcmp(name, "glClearDepth") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearDepth);
  if (strcmp(name, "glClearDepthf") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearDepthf);
  if (strcmp(name, "glClearStencil") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearStencil);
  if (strcmp(name, "glClearTexImageEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexImageEXT);
  if (strcmp(name, "glClearTexSubImage") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexSubImage);
  if (strcmp(name, "glClearTexSubImageEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexSubImageEXT);
  if (strcmp(name, "glClientWaitSync") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClientWaitSync);
  if (strcmp(name, "glClipControlEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClipControlEXT);
  if (strcmp(name, "glColorMask") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glColorMask);
  if (strcmp(name, "glColorMaski") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glColorMaski);
  if (strcmp(name, "glColorMaskiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glColorMaskiOES);
  if (strcmp(name, "glCompileShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompileShader);
  if (strcmp(name, "glCompressedTexImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompressedTexImage2D);
  if (strcmp(name, "glCompressedTexImage2DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexImage2DRobustANGLE);
  if (strcmp(name, "glCompressedTexImage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompressedTexImage3D);
  if (strcmp(name, "glCompressedTexImage3DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexImage3DRobustANGLE);
  if (strcmp(name, "glCompressedTexSubImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage2D);
  if (strcmp(name, "glCompressedTexSubImage2DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage2DRobustANGLE);
  if (strcmp(name, "glCompressedTexSubImage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage3D);
  if (strcmp(name, "glCompressedTexSubImage3DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage3DRobustANGLE);
  if (strcmp(name, "glCopyBufferSubData") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyBufferSubData);
  if (strcmp(name, "glCopySubTextureCHROMIUM") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCopySubTextureCHROMIUM);
  if (strcmp(name, "glCopyTexImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexImage2D);
  if (strcmp(name, "glCopyTexSubImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexSubImage2D);
  if (strcmp(name, "glCopyTexSubImage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexSubImage3D);
  if (strcmp(name, "glCopyTextureCHROMIUM") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTextureCHROMIUM);
  if (strcmp(name, "glCreateMemoryObjectsEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCreateMemoryObjectsEXT);
  if (strcmp(name, "glCreateProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCreateProgram);
  if (strcmp(name, "glCreateShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCreateShader);
  if (strcmp(name, "glCreateShaderProgramv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCreateShaderProgramv);
  if (strcmp(name, "glCullFace") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCullFace);
  if (strcmp(name, "glDebugMessageCallback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDebugMessageCallback);
  if (strcmp(name, "glDebugMessageCallbackKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageCallbackKHR);
  if (strcmp(name, "glDebugMessageControl") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDebugMessageControl);
  if (strcmp(name, "glDebugMessageControlKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageControlKHR);
  if (strcmp(name, "glDebugMessageInsert") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDebugMessageInsert);
  if (strcmp(name, "glDebugMessageInsertKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageInsertKHR);
  if (strcmp(name, "glDeleteBuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteBuffers);
  if (strcmp(name, "glDeleteFencesNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteFencesNV);
  if (strcmp(name, "glDeleteFramebuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteFramebuffers);
  if (strcmp(name, "glDeleteMemoryObjectsEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteMemoryObjectsEXT);
  if (strcmp(name, "glDeleteProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteProgram);
  if (strcmp(name, "glDeleteProgramPipelines") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteProgramPipelines);
  if (strcmp(name, "glDeleteQueries") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteQueries);
  if (strcmp(name, "glDeleteQueriesEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteQueriesEXT);
  if (strcmp(name, "glDeleteRenderbuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteRenderbuffers);
  if (strcmp(name, "glDeleteSamplers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSamplers);
  if (strcmp(name, "glDeleteSemaphoresEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSemaphoresEXT);
  if (strcmp(name, "glDeleteShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteShader);
  if (strcmp(name, "glDeleteSync") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSync);
  if (strcmp(name, "glDeleteTextures") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteTextures);
  if (strcmp(name, "glDeleteTransformFeedbacks") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteTransformFeedbacks);
  if (strcmp(name, "glDeleteVertexArrays") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteVertexArrays);
  if (strcmp(name, "glDeleteVertexArraysOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteVertexArraysOES);
  if (strcmp(name, "glDepthFunc") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthFunc);
  if (strcmp(name, "glDepthMask") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthMask);
  if (strcmp(name, "glDepthRange") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthRange);
  if (strcmp(name, "glDepthRangef") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthRangef);
  if (strcmp(name, "glDetachShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDetachShader);
  if (strcmp(name, "glDisable") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDisable);
  if (strcmp(name, "glDisableExtensionANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDisableExtensionANGLE);
  if (strcmp(name, "glDisableVertexAttribArray") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDisableVertexAttribArray);
  if (strcmp(name, "glDisablei") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDisablei);
  if (strcmp(name, "glDisableiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDisableiOES);
  if (strcmp(name, "glDiscardFramebufferEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDiscardFramebufferEXT);
  if (strcmp(name, "glDispatchCompute") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDispatchCompute);
  if (strcmp(name, "glDispatchComputeIndirect") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDispatchComputeIndirect);
  if (strcmp(name, "glDrawArrays") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawArrays);
  if (strcmp(name, "glDrawArraysIndirect") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawArraysIndirect);
  if (strcmp(name, "glDrawArraysInstanced") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawArraysInstanced);
  if (strcmp(name, "glDrawArraysInstancedANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedANGLE);
  if (strcmp(name, "glDrawArraysInstancedBaseInstanceANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedBaseInstanceANGLE);
  if (strcmp(name, "glDrawArraysInstancedBaseInstanceEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedBaseInstanceEXT);
  if (strcmp(name, "glDrawBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffer);
  if (strcmp(name, "glDrawBuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffers);
  if (strcmp(name, "glDrawBuffersEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffersEXT);
  if (strcmp(name, "glDrawElements") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawElements);
  if (strcmp(name, "glDrawElementsIndirect") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawElementsIndirect);
  if (strcmp(name, "glDrawElementsInstanced") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstanced);
  if (strcmp(name, "glDrawElementsInstancedANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedANGLE);
  if (strcmp(name, "glDrawElementsInstancedBaseVertexBaseInstanceANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedBaseVertexBaseInstanceANGLE);
  if (strcmp(name, "glDrawElementsInstancedBaseVertexBaseInstanceEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedBaseVertexBaseInstanceEXT);
  if (strcmp(name, "glDrawRangeElements") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawRangeElements);
  if (strcmp(name, "glEGLImageTargetRenderbufferStorageOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEGLImageTargetRenderbufferStorageOES);
  if (strcmp(name, "glEGLImageTargetTexture2DOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEGLImageTargetTexture2DOES);
  if (strcmp(name, "glEnable") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEnable);
  if (strcmp(name, "glEnableVertexAttribArray") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEnableVertexAttribArray);
  if (strcmp(name, "glEnablei") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEnablei);
  if (strcmp(name, "glEnableiOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEnableiOES);
  if (strcmp(name, "glEndPixelLocalStorageANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEndPixelLocalStorageANGLE);
  if (strcmp(name, "glEndQuery") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndQuery);
  if (strcmp(name, "glEndQueryEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndQueryEXT);
  if (strcmp(name, "glEndTilingQCOM") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndTilingQCOM);
  if (strcmp(name, "glEndTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndTransformFeedback);
  if (strcmp(name, "glFenceSync") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFenceSync);
  if (strcmp(name, "glFinish") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFinish);
  if (strcmp(name, "glFinishFenceNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFinishFenceNV);
  if (strcmp(name, "glFlush") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFlush);
  if (strcmp(name, "glFlushMappedBufferRange") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFlushMappedBufferRange);
  if (strcmp(name, "glFlushMappedBufferRangeEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFlushMappedBufferRangeEXT);
  if (strcmp(name, "glFramebufferMemorylessPixelLocalStorageANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferMemorylessPixelLocalStorageANGLE);
  if (strcmp(name, "glFramebufferParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferParameteri);
  if (strcmp(name, "glFramebufferParameteriMESA") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferParameteriMESA);
  if (strcmp(name, "glFramebufferPixelLocalClearValuefvANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValuefvANGLE);
  if (strcmp(name, "glFramebufferPixelLocalClearValueivANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValueivANGLE);
  if (strcmp(name, "glFramebufferPixelLocalClearValueuivANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValueuivANGLE);
  if (strcmp(name, "glFramebufferPixelLocalStorageInterruptANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalStorageInterruptANGLE);
  if (strcmp(name, "glFramebufferPixelLocalStorageRestoreANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalStorageRestoreANGLE);
  if (strcmp(name, "glFramebufferRenderbuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferRenderbuffer);
  if (strcmp(name, "glFramebufferTexture2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFramebufferTexture2D);
  if (strcmp(name, "glFramebufferTexture2DMultisampleEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexture2DMultisampleEXT);
  if (strcmp(name, "glFramebufferTexture2DMultisampleIMG") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexture2DMultisampleIMG);
  if (strcmp(name, "glFramebufferTextureLayer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTextureLayer);
  if (strcmp(name, "glFramebufferTextureMultiviewOVR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTextureMultiviewOVR);
  if (strcmp(name, "glFramebufferTexturePixelLocalStorageANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexturePixelLocalStorageANGLE);
  if (strcmp(name, "glFrontFace") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFrontFace);
  if (strcmp(name, "glGenBuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenBuffers);
  if (strcmp(name, "glGenFencesNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenFencesNV);
  if (strcmp(name, "glGenFramebuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenFramebuffers);
  if (strcmp(name, "glGenProgramPipelines") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenProgramPipelines);
  if (strcmp(name, "glGenQueries") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenQueries);
  if (strcmp(name, "glGenQueriesEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenQueriesEXT);
  if (strcmp(name, "glGenRenderbuffers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenRenderbuffers);
  if (strcmp(name, "glGenSamplers") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenSamplers);
  if (strcmp(name, "glGenSemaphoresEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenSemaphoresEXT);
  if (strcmp(name, "glGenTextures") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenTextures);
  if (strcmp(name, "glGenTransformFeedbacks") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGenTransformFeedbacks);
  if (strcmp(name, "glGenVertexArrays") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenVertexArrays);
  if (strcmp(name, "glGenVertexArraysOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenVertexArraysOES);
  if (strcmp(name, "glGenerateMipmap") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenerateMipmap);
  if (strcmp(name, "glGetActiveAttrib") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveAttrib);
  if (strcmp(name, "glGetActiveUniform") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveUniform);
  if (strcmp(name, "glGetActiveUniformBlockName") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockName);
  if (strcmp(name, "glGetActiveUniformBlockiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockiv);
  if (strcmp(name, "glGetActiveUniformBlockivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockivRobustANGLE);
  if (strcmp(name, "glGetActiveUniformsiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveUniformsiv);
  if (strcmp(name, "glGetAttachedShaders") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetAttachedShaders);
  if (strcmp(name, "glGetAttribLocation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetAttribLocation);
  if (strcmp(name, "glGetBooleani_v") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetBooleani_v);
  if (strcmp(name, "glGetBooleani_vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBooleani_vRobustANGLE);
  if (strcmp(name, "glGetBooleanv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetBooleanv);
  if (strcmp(name, "glGetBooleanvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBooleanvRobustANGLE);
  if (strcmp(name, "glGetBufferParameteri64vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferParameteri64vRobustANGLE);
  if (strcmp(name, "glGetBufferParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetBufferParameteriv);
  if (strcmp(name, "glGetBufferParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferParameterivRobustANGLE);
  if (strcmp(name, "glGetBufferPointervRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferPointervRobustANGLE);
  if (strcmp(name, "glGetDebugMessageLog") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetDebugMessageLog);
  if (strcmp(name, "glGetDebugMessageLogKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetDebugMessageLogKHR);
  if (strcmp(name, "glGetError") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetError);
  if (strcmp(name, "glGetFenceivNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFenceivNV);
  if (strcmp(name, "glGetFloatv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFloatv);
  if (strcmp(name, "glGetFloatvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFloatvRobustANGLE);
  if (strcmp(name, "glGetFragDataIndexEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFragDataIndexEXT);
  if (strcmp(name, "glGetFragDataLocation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFragDataLocation);
  if (strcmp(name, "glGetFramebufferAttachmentParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferAttachmentParameteriv);
  if (strcmp(name, "glGetFramebufferAttachmentParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferAttachmentParameterivRobustANGLE);
  if (strcmp(name, "glGetFramebufferParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferParameteriv);
  if (strcmp(name, "glGetFramebufferParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferParameterivRobustANGLE);
  if (strcmp(name, "glGetFramebufferPixelLocalStorageParameterfvANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterfvANGLE);
  if (strcmp(name, "glGetFramebufferPixelLocalStorageParameterfvRobustANGLE") ==
      0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterfvRobustANGLE);
  if (strcmp(name, "glGetFramebufferPixelLocalStorageParameterivANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterivANGLE);
  if (strcmp(name, "glGetFramebufferPixelLocalStorageParameterivRobustANGLE") ==
      0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterivRobustANGLE);
  if (strcmp(name, "glGetGraphicsResetStatus") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetGraphicsResetStatus);
  if (strcmp(name, "glGetGraphicsResetStatusEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetGraphicsResetStatusEXT);
  if (strcmp(name, "glGetGraphicsResetStatusKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetGraphicsResetStatusKHR);
  if (strcmp(name, "glGetInteger64i_v") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInteger64i_v);
  if (strcmp(name, "glGetInteger64i_vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInteger64i_vRobustANGLE);
  if (strcmp(name, "glGetInteger64v") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInteger64v);
  if (strcmp(name, "glGetInteger64vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInteger64vRobustANGLE);
  if (strcmp(name, "glGetIntegeri_v") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetIntegeri_v);
  if (strcmp(name, "glGetIntegeri_vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetIntegeri_vRobustANGLE);
  if (strcmp(name, "glGetIntegerv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetIntegerv);
  if (strcmp(name, "glGetIntegervRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetIntegervRobustANGLE);
  if (strcmp(name, "glGetInternalformatSampleivNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInternalformatSampleivNV);
  if (strcmp(name, "glGetInternalformativ") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInternalformativ);
  if (strcmp(name, "glGetInternalformativRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInternalformativRobustANGLE);
  if (strcmp(name, "glGetMultisamplefv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetMultisamplefv);
  if (strcmp(name, "glGetMultisamplefvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetMultisamplefvRobustANGLE);
  if (strcmp(name, "glGetObjectLabel") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectLabel);
  if (strcmp(name, "glGetObjectLabelKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectLabelKHR);
  if (strcmp(name, "glGetObjectPtrLabel") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectPtrLabel);
  if (strcmp(name, "glGetObjectPtrLabelKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectPtrLabelKHR);
  if (strcmp(name, "glGetPointerv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetPointerv);
  if (strcmp(name, "glGetPointervKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetPointervKHR);
  if (strcmp(name, "glGetPointervRobustANGLERobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetPointervRobustANGLERobustANGLE);
  if (strcmp(name, "glGetProgramBinary") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramBinary);
  if (strcmp(name, "glGetProgramBinaryOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramBinaryOES);
  if (strcmp(name, "glGetProgramInfoLog") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramInfoLog);
  if (strcmp(name, "glGetProgramInterfaceiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramInterfaceiv);
  if (strcmp(name, "glGetProgramInterfaceivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramInterfaceivRobustANGLE);
  if (strcmp(name, "glGetProgramPipelineInfoLog") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramPipelineInfoLog);
  if (strcmp(name, "glGetProgramPipelineiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramPipelineiv);
  if (strcmp(name, "glGetProgramResourceIndex") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramResourceIndex);
  if (strcmp(name, "glGetProgramResourceLocation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramResourceLocation);
  if (strcmp(name, "glGetProgramResourceName") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramResourceName);
  if (strcmp(name, "glGetProgramResourceiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramResourceiv);
  if (strcmp(name, "glGetProgramiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramiv);
  if (strcmp(name, "glGetProgramivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramivRobustANGLE);
  if (strcmp(name, "glGetQueryObjecti64vEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjecti64vEXT);
  if (strcmp(name, "glGetQueryObjecti64vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjecti64vRobustANGLE);
  if (strcmp(name, "glGetQueryObjectivEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectivEXT);
  if (strcmp(name, "glGetQueryObjectivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectivRobustANGLE);
  if (strcmp(name, "glGetQueryObjectui64vEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectui64vEXT);
  if (strcmp(name, "glGetQueryObjectui64vRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectui64vRobustANGLE);
  if (strcmp(name, "glGetQueryObjectuiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectuiv);
  if (strcmp(name, "glGetQueryObjectuivEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectuivEXT);
  if (strcmp(name, "glGetQueryObjectuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectuivRobustANGLE);
  if (strcmp(name, "glGetQueryiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryiv);
  if (strcmp(name, "glGetQueryivEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryivEXT);
  if (strcmp(name, "glGetQueryivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryivRobustANGLE);
  if (strcmp(name, "glGetRenderbufferParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetRenderbufferParameteriv);
  if (strcmp(name, "glGetRenderbufferParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetRenderbufferParameterivRobustANGLE);
  if (strcmp(name, "glGetSamplerParameterIivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterIivRobustANGLE);
  if (strcmp(name, "glGetSamplerParameterIuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterIuivRobustANGLE);
  if (strcmp(name, "glGetSamplerParameterfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterfv);
  if (strcmp(name, "glGetSamplerParameterfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterfvRobustANGLE);
  if (strcmp(name, "glGetSamplerParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameteriv);
  if (strcmp(name, "glGetSamplerParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterivRobustANGLE);
  if (strcmp(name, "glGetShaderInfoLog") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderInfoLog);
  if (strcmp(name, "glGetShaderPrecisionFormat") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetShaderPrecisionFormat);
  if (strcmp(name, "glGetShaderSource") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderSource);
  if (strcmp(name, "glGetShaderiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderiv);
  if (strcmp(name, "glGetShaderivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetShaderivRobustANGLE);
  if (strcmp(name, "glGetString") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetString);
  if (strcmp(name, "glGetStringi") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetStringi);
  if (strcmp(name, "glGetSynciv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetSynciv);
  if (strcmp(name, "glGetTexLevelParameterfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterfv);
  if (strcmp(name, "glGetTexLevelParameterfvANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterfvANGLE);
  if (strcmp(name, "glGetTexLevelParameterfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterfvRobustANGLE);
  if (strcmp(name, "glGetTexLevelParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameteriv);
  if (strcmp(name, "glGetTexLevelParameterivANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterivANGLE);
  if (strcmp(name, "glGetTexLevelParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterivRobustANGLE);
  if (strcmp(name, "glGetTexParameterIivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterIivRobustANGLE);
  if (strcmp(name, "glGetTexParameterIuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterIuivRobustANGLE);
  if (strcmp(name, "glGetTexParameterfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetTexParameterfv);
  if (strcmp(name, "glGetTexParameterfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterfvRobustANGLE);
  if (strcmp(name, "glGetTexParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetTexParameteriv);
  if (strcmp(name, "glGetTexParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterivRobustANGLE);
  if (strcmp(name, "glGetTransformFeedbackVarying") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTransformFeedbackVarying);
  if (strcmp(name, "glGetTranslatedShaderSourceANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTranslatedShaderSourceANGLE);
  if (strcmp(name, "glGetUniformBlockIndex") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformBlockIndex);
  if (strcmp(name, "glGetUniformIndices") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformIndices);
  if (strcmp(name, "glGetUniformLocation") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformLocation);
  if (strcmp(name, "glGetUniformfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformfv);
  if (strcmp(name, "glGetUniformfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformfvRobustANGLE);
  if (strcmp(name, "glGetUniformiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformiv);
  if (strcmp(name, "glGetUniformivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformivRobustANGLE);
  if (strcmp(name, "glGetUniformuiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformuiv);
  if (strcmp(name, "glGetUniformuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformuivRobustANGLE);
  if (strcmp(name, "glGetVertexAttribIivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribIivRobustANGLE);
  if (strcmp(name, "glGetVertexAttribIuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribIuivRobustANGLE);
  if (strcmp(name, "glGetVertexAttribPointerv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribPointerv);
  if (strcmp(name, "glGetVertexAttribPointervRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribPointervRobustANGLE);
  if (strcmp(name, "glGetVertexAttribfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetVertexAttribfv);
  if (strcmp(name, "glGetVertexAttribfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribfvRobustANGLE);
  if (strcmp(name, "glGetVertexAttribiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetVertexAttribiv);
  if (strcmp(name, "glGetVertexAttribivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribivRobustANGLE);
  if (strcmp(name, "glGetnUniformfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetnUniformfvRobustANGLE);
  if (strcmp(name, "glGetnUniformivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetnUniformivRobustANGLE);
  if (strcmp(name, "glGetnUniformuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetnUniformuivRobustANGLE);
  if (strcmp(name, "glHint") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glHint);
  if (strcmp(name, "glImportMemoryFdEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glImportMemoryFdEXT);
  if (strcmp(name, "glImportMemoryWin32HandleEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportMemoryWin32HandleEXT);
  if (strcmp(name, "glImportMemoryZirconHandleANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportMemoryZirconHandleANGLE);
  if (strcmp(name, "glImportSemaphoreFdEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glImportSemaphoreFdEXT);
  if (strcmp(name, "glImportSemaphoreWin32HandleEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportSemaphoreWin32HandleEXT);
  if (strcmp(name, "glImportSemaphoreZirconHandleANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportSemaphoreZirconHandleANGLE);
  if (strcmp(name, "glInsertEventMarkerEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glInsertEventMarkerEXT);
  if (strcmp(name, "glInvalidateFramebuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateFramebuffer);
  if (strcmp(name, "glInvalidateSubFramebuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateSubFramebuffer);
  if (strcmp(name, "glInvalidateTextureANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateTextureANGLE);
  if (strcmp(name, "glIsBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsBuffer);
  if (strcmp(name, "glIsEnabled") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsEnabled);
  if (strcmp(name, "glIsEnabledi") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsEnabledi);
  if (strcmp(name, "glIsEnablediOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsEnablediOES);
  if (strcmp(name, "glIsFenceNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsFenceNV);
  if (strcmp(name, "glIsFramebuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsFramebuffer);
  if (strcmp(name, "glIsProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsProgram);
  if (strcmp(name, "glIsProgramPipeline") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsProgramPipeline);
  if (strcmp(name, "glIsQuery") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsQuery);
  if (strcmp(name, "glIsQueryEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsQueryEXT);
  if (strcmp(name, "glIsRenderbuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsRenderbuffer);
  if (strcmp(name, "glIsSampler") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsSampler);
  if (strcmp(name, "glIsShader") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsShader);
  if (strcmp(name, "glIsSync") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsSync);
  if (strcmp(name, "glIsTexture") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsTexture);
  if (strcmp(name, "glIsTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsTransformFeedback);
  if (strcmp(name, "glIsVertexArray") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsVertexArray);
  if (strcmp(name, "glIsVertexArrayOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsVertexArrayOES);
  if (strcmp(name, "glLineWidth") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glLineWidth);
  if (strcmp(name, "glLinkProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glLinkProgram);
  if (strcmp(name, "glMapBufferOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferOES);
  if (strcmp(name, "glMapBufferRange") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferRange);
  if (strcmp(name, "glMapBufferRangeEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferRangeEXT);
  if (strcmp(name, "glMaxShaderCompilerThreadsKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMaxShaderCompilerThreadsKHR);
  if (strcmp(name, "glMemoryBarrier") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMemoryBarrier);
  if (strcmp(name, "glMemoryBarrierByRegion") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMemoryBarrierByRegion);
  if (strcmp(name, "glMemoryBarrierEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMemoryBarrierEXT);
  if (strcmp(name, "glMemoryObjectParameterivEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMemoryObjectParameterivEXT);
  if (strcmp(name, "glMinSampleShading") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMinSampleShading);
  if (strcmp(name, "glMultiDrawArraysANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMultiDrawArraysANGLE);
  if (strcmp(name, "glMultiDrawArraysInstancedANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawArraysInstancedANGLE);
  if (strcmp(name, "glMultiDrawArraysInstancedBaseInstanceANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawArraysInstancedBaseInstanceANGLE);
  if (strcmp(name, "glMultiDrawElementsANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsANGLE);
  if (strcmp(name, "glMultiDrawElementsInstancedANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsInstancedANGLE);
  if (strcmp(name, "glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE") ==
      0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE);
  if (strcmp(name, "glObjectLabel") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectLabel);
  if (strcmp(name, "glObjectLabelKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectLabelKHR);
  if (strcmp(name, "glObjectPtrLabel") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectPtrLabel);
  if (strcmp(name, "glObjectPtrLabelKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectPtrLabelKHR);
  if (strcmp(name, "glPatchParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPatchParameteri);
  if (strcmp(name, "glPatchParameteriOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPatchParameteriOES);
  if (strcmp(name, "glPauseTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPauseTransformFeedback);
  if (strcmp(name, "glPixelLocalStorageBarrierANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPixelLocalStorageBarrierANGLE);
  if (strcmp(name, "glPixelStorei") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPixelStorei);
  if (strcmp(name, "glPointParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPointParameteri);
  if (strcmp(name, "glPolygonMode") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonMode);
  if (strcmp(name, "glPolygonModeANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonModeANGLE);
  if (strcmp(name, "glPolygonOffset") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonOffset);
  if (strcmp(name, "glPolygonOffsetClampEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPolygonOffsetClampEXT);
  if (strcmp(name, "glPopDebugGroup") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPopDebugGroup);
  if (strcmp(name, "glPopDebugGroupKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPopDebugGroupKHR);
  if (strcmp(name, "glPopGroupMarkerEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPopGroupMarkerEXT);
  if (strcmp(name, "glPrimitiveRestartIndex") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPrimitiveRestartIndex);
  if (strcmp(name, "glProgramBinary") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramBinary);
  if (strcmp(name, "glProgramBinaryOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramBinaryOES);
  if (strcmp(name, "glProgramParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramParameteri);
  if (strcmp(name, "glProgramUniform1f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1f);
  if (strcmp(name, "glProgramUniform1fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1fv);
  if (strcmp(name, "glProgramUniform1i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1i);
  if (strcmp(name, "glProgramUniform1iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1iv);
  if (strcmp(name, "glProgramUniform1ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1ui);
  if (strcmp(name, "glProgramUniform1uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform1uiv);
  if (strcmp(name, "glProgramUniform2f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2f);
  if (strcmp(name, "glProgramUniform2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2fv);
  if (strcmp(name, "glProgramUniform2i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2i);
  if (strcmp(name, "glProgramUniform2iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2iv);
  if (strcmp(name, "glProgramUniform2ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2ui);
  if (strcmp(name, "glProgramUniform2uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform2uiv);
  if (strcmp(name, "glProgramUniform3f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3f);
  if (strcmp(name, "glProgramUniform3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3fv);
  if (strcmp(name, "glProgramUniform3i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3i);
  if (strcmp(name, "glProgramUniform3iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3iv);
  if (strcmp(name, "glProgramUniform3ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3ui);
  if (strcmp(name, "glProgramUniform3uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform3uiv);
  if (strcmp(name, "glProgramUniform4f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4f);
  if (strcmp(name, "glProgramUniform4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4fv);
  if (strcmp(name, "glProgramUniform4i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4i);
  if (strcmp(name, "glProgramUniform4iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4iv);
  if (strcmp(name, "glProgramUniform4ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4ui);
  if (strcmp(name, "glProgramUniform4uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramUniform4uiv);
  if (strcmp(name, "glProgramUniformMatrix2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix2fv);
  if (strcmp(name, "glProgramUniformMatrix2x3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix2x3fv);
  if (strcmp(name, "glProgramUniformMatrix2x4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix2x4fv);
  if (strcmp(name, "glProgramUniformMatrix3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix3fv);
  if (strcmp(name, "glProgramUniformMatrix3x2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix3x2fv);
  if (strcmp(name, "glProgramUniformMatrix3x4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix3x4fv);
  if (strcmp(name, "glProgramUniformMatrix4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix4fv);
  if (strcmp(name, "glProgramUniformMatrix4x2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix4x2fv);
  if (strcmp(name, "glProgramUniformMatrix4x3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glProgramUniformMatrix4x3fv);
  if (strcmp(name, "glProvokingVertexANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProvokingVertexANGLE);
  if (strcmp(name, "glPushDebugGroup") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPushDebugGroup);
  if (strcmp(name, "glPushDebugGroupKHR") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPushDebugGroupKHR);
  if (strcmp(name, "glPushGroupMarkerEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPushGroupMarkerEXT);
  if (strcmp(name, "glQueryCounterEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glQueryCounterEXT);
  if (strcmp(name, "glReadBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReadBuffer);
  if (strcmp(name, "glReadPixels") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReadPixels);
  if (strcmp(name, "glReadPixelsRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glReadPixelsRobustANGLE);
  if (strcmp(name, "glReadnPixelsRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glReadnPixelsRobustANGLE);
  if (strcmp(name, "glReleaseShaderCompiler") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glReleaseShaderCompiler);
  if (strcmp(name, "glReleaseTexturesANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReleaseTexturesANGLE);
  if (strcmp(name, "glRenderbufferStorage") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glRenderbufferStorage);
  if (strcmp(name, "glRenderbufferStorageMultisample") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisample);
  if (strcmp(name, "glRenderbufferStorageMultisampleANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleANGLE);
  if (strcmp(name, "glRenderbufferStorageMultisampleAdvancedAMD") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleAdvancedAMD);
  if (strcmp(name, "glRenderbufferStorageMultisampleEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleEXT);
  if (strcmp(name, "glRenderbufferStorageMultisampleIMG") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleIMG);
  if (strcmp(name, "glRequestExtensionANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRequestExtensionANGLE);
  if (strcmp(name, "glResumeTransformFeedback") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glResumeTransformFeedback);
  if (strcmp(name, "glSampleCoverage") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSampleCoverage);
  if (strcmp(name, "glSampleMaski") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSampleMaski);
  if (strcmp(name, "glSamplerParameterIivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterIivRobustANGLE);
  if (strcmp(name, "glSamplerParameterIuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterIuivRobustANGLE);
  if (strcmp(name, "glSamplerParameterf") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameterf);
  if (strcmp(name, "glSamplerParameterfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameterfv);
  if (strcmp(name, "glSamplerParameterfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterfvRobustANGLE);
  if (strcmp(name, "glSamplerParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameteri);
  if (strcmp(name, "glSamplerParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameteriv);
  if (strcmp(name, "glSamplerParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterivRobustANGLE);
  if (strcmp(name, "glScissor") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glScissor);
  if (strcmp(name, "glSetFenceNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSetFenceNV);
  if (strcmp(name, "glShaderBinary") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glShaderBinary);
  if (strcmp(name, "glShaderSource") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glShaderSource);
  if (strcmp(name, "glSignalSemaphoreEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSignalSemaphoreEXT);
  if (strcmp(name, "glStartTilingQCOM") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStartTilingQCOM);
  if (strcmp(name, "glStencilFunc") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilFunc);
  if (strcmp(name, "glStencilFuncSeparate") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilFuncSeparate);
  if (strcmp(name, "glStencilMask") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilMask);
  if (strcmp(name, "glStencilMaskSeparate") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilMaskSeparate);
  if (strcmp(name, "glStencilOp") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilOp);
  if (strcmp(name, "glStencilOpSeparate") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilOpSeparate);
  if (strcmp(name, "glTestFenceNV") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTestFenceNV);
  if (strcmp(name, "glTexBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBuffer);
  if (strcmp(name, "glTexBufferEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferEXT);
  if (strcmp(name, "glTexBufferOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferOES);
  if (strcmp(name, "glTexBufferRange") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferRange);
  if (strcmp(name, "glTexBufferRangeEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferRangeEXT);
  if (strcmp(name, "glTexBufferRangeOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferRangeOES);
  if (strcmp(name, "glTexImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexImage2D);
  if (strcmp(name, "glTexImage2DExternalANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage2DExternalANGLE);
  if (strcmp(name, "glTexImage2DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage2DRobustANGLE);
  if (strcmp(name, "glTexImage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexImage3D);
  if (strcmp(name, "glTexImage3DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage3DRobustANGLE);
  if (strcmp(name, "glTexParameterIivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterIivRobustANGLE);
  if (strcmp(name, "glTexParameterIuivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterIuivRobustANGLE);
  if (strcmp(name, "glTexParameterf") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameterf);
  if (strcmp(name, "glTexParameterfv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameterfv);
  if (strcmp(name, "glTexParameterfvRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterfvRobustANGLE);
  if (strcmp(name, "glTexParameteri") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameteri);
  if (strcmp(name, "glTexParameteriv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameteriv);
  if (strcmp(name, "glTexParameterivRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterivRobustANGLE);
  if (strcmp(name, "glTexStorage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage2D);
  if (strcmp(name, "glTexStorage2DEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage2DEXT);
  if (strcmp(name, "glTexStorage2DMultisample") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexStorage2DMultisample);
  if (strcmp(name, "glTexStorage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage3D);
  if (strcmp(name, "glTexStorageMem2DEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorageMem2DEXT);
  if (strcmp(name, "glTexStorageMemFlags2DANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexStorageMemFlags2DANGLE);
  if (strcmp(name, "glTexSubImage2D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexSubImage2D);
  if (strcmp(name, "glTexSubImage2DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexSubImage2DRobustANGLE);
  if (strcmp(name, "glTexSubImage3D") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexSubImage3D);
  if (strcmp(name, "glTexSubImage3DRobustANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexSubImage3DRobustANGLE);
  if (strcmp(name, "glTransformFeedbackVaryings") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTransformFeedbackVaryings);
  if (strcmp(name, "glUniform1f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1f);
  if (strcmp(name, "glUniform1fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1fv);
  if (strcmp(name, "glUniform1i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1i);
  if (strcmp(name, "glUniform1iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1iv);
  if (strcmp(name, "glUniform1ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1ui);
  if (strcmp(name, "glUniform1uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1uiv);
  if (strcmp(name, "glUniform2f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2f);
  if (strcmp(name, "glUniform2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2fv);
  if (strcmp(name, "glUniform2i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2i);
  if (strcmp(name, "glUniform2iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2iv);
  if (strcmp(name, "glUniform2ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2ui);
  if (strcmp(name, "glUniform2uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2uiv);
  if (strcmp(name, "glUniform3f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3f);
  if (strcmp(name, "glUniform3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3fv);
  if (strcmp(name, "glUniform3i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3i);
  if (strcmp(name, "glUniform3iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3iv);
  if (strcmp(name, "glUniform3ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3ui);
  if (strcmp(name, "glUniform3uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3uiv);
  if (strcmp(name, "glUniform4f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4f);
  if (strcmp(name, "glUniform4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4fv);
  if (strcmp(name, "glUniform4i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4i);
  if (strcmp(name, "glUniform4iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4iv);
  if (strcmp(name, "glUniform4ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4ui);
  if (strcmp(name, "glUniform4uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4uiv);
  if (strcmp(name, "glUniformBlockBinding") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformBlockBinding);
  if (strcmp(name, "glUniformMatrix2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2fv);
  if (strcmp(name, "glUniformMatrix2x3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2x3fv);
  if (strcmp(name, "glUniformMatrix2x4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2x4fv);
  if (strcmp(name, "glUniformMatrix3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3fv);
  if (strcmp(name, "glUniformMatrix3x2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3x2fv);
  if (strcmp(name, "glUniformMatrix3x4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3x4fv);
  if (strcmp(name, "glUniformMatrix4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4fv);
  if (strcmp(name, "glUniformMatrix4x2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4x2fv);
  if (strcmp(name, "glUniformMatrix4x3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4x3fv);
  if (strcmp(name, "glUnmapBuffer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUnmapBuffer);
  if (strcmp(name, "glUnmapBufferOES") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUnmapBufferOES);
  if (strcmp(name, "glUseProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUseProgram);
  if (strcmp(name, "glUseProgramStages") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUseProgramStages);
  if (strcmp(name, "glValidateProgram") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glValidateProgram);
  if (strcmp(name, "glValidateProgramPipeline") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glValidateProgramPipeline);
  if (strcmp(name, "glVertexAttrib1f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib1f);
  if (strcmp(name, "glVertexAttrib1fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib1fv);
  if (strcmp(name, "glVertexAttrib2f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib2f);
  if (strcmp(name, "glVertexAttrib2fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib2fv);
  if (strcmp(name, "glVertexAttrib3f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib3f);
  if (strcmp(name, "glVertexAttrib3fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib3fv);
  if (strcmp(name, "glVertexAttrib4f") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib4f);
  if (strcmp(name, "glVertexAttrib4fv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib4fv);
  if (strcmp(name, "glVertexAttribBinding") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribBinding);
  if (strcmp(name, "glVertexAttribDivisor") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribDivisor);
  if (strcmp(name, "glVertexAttribDivisorANGLE") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glVertexAttribDivisorANGLE);
  if (strcmp(name, "glVertexAttribDivisorEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glVertexAttribDivisorEXT);
  if (strcmp(name, "glVertexAttribFormat") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribFormat);
  if (strcmp(name, "glVertexAttribI4i") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4i);
  if (strcmp(name, "glVertexAttribI4iv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4iv);
  if (strcmp(name, "glVertexAttribI4ui") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4ui);
  if (strcmp(name, "glVertexAttribI4uiv") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4uiv);
  if (strcmp(name, "glVertexAttribIFormat") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribIFormat);
  if (strcmp(name, "glVertexAttribIPointer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribIPointer);
  if (strcmp(name, "glVertexAttribPointer") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribPointer);
  if (strcmp(name, "glVertexBindingDivisor") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexBindingDivisor);
  if (strcmp(name, "glViewport") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glViewport);
  if (strcmp(name, "glWaitSemaphoreEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWaitSemaphoreEXT);
  if (strcmp(name, "glWaitSync") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWaitSync);
  if (strcmp(name, "glWindowRectanglesEXT") == 0)
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWindowRectanglesEXT);
  return reinterpret_cast<GLFunctionPointerType>(&MockGlInvalidFunction);
}

}  // namespace gl
