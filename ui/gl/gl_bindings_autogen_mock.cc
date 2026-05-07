// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string_view>

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
MockGLInterface::Mock_glBlendEquationSeparateiOES(GLuint buf,
                                                  GLenum modeRGB,
                                                  GLenum modeAlpha) {
  MakeGlMockFunctionUnique("glBlendEquationSeparateiOES");
  interface_->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
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
MockGLInterface::Mock_glBlendFuncSeparateiOES(GLuint buf,
                                              GLenum srcRGB,
                                              GLenum dstRGB,
                                              GLenum srcAlpha,
                                              GLenum dstAlpha) {
  MakeGlMockFunctionUnique("glBlendFuncSeparateiOES");
  interface_->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
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

void GL_BINDING_CALL
MockGLInterface::Mock_glBlobCacheCallbacksANGLE(GLSETBLOBPROCANGLE set,
                                                GLGETBLOBPROCANGLE get,
                                                const void* userData) {
  MakeGlMockFunctionUnique("glBlobCacheCallbacksANGLE");
  interface_->BlobCacheCallbacksANGLE(set, get, userData);
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

void GL_BINDING_CALL MockGLInterface::Mock_glCullFace(GLenum mode) {
  MakeGlMockFunctionUnique("glCullFace");
  interface_->CullFace(mode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageCallbackKHR(GLDEBUGPROC callback,
                                                const void* userParam) {
  MakeGlMockFunctionUnique("glDebugMessageCallbackKHR");
  interface_->DebugMessageCallbackKHR(callback, userParam);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageControlKHR(GLenum source,
                                               GLenum type,
                                               GLenum severity,
                                               GLsizei count,
                                               const GLuint* ids,
                                               GLboolean enabled) {
  MakeGlMockFunctionUnique("glDebugMessageControlKHR");
  interface_->DebugMessageControlKHR(source, type, severity, count, ids,
                                     enabled);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glDebugMessageInsertKHR(GLenum source,
                                              GLenum type,
                                              GLuint id,
                                              GLenum severity,
                                              GLsizei length,
                                              const char* buf) {
  MakeGlMockFunctionUnique("glDebugMessageInsertKHR");
  interface_->DebugMessageInsertKHR(source, type, id, severity, length, buf);
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
MockGLInterface::Mock_glDisableVertexAttribArray(GLuint index) {
  MakeGlMockFunctionUnique("glDisableVertexAttribArray");
  interface_->DisableVertexAttribArray(index);
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

void GL_BINDING_CALL MockGLInterface::Mock_glDrawArrays(GLenum mode,
                                                        GLint first,
                                                        GLsizei count) {
  MakeGlMockFunctionUnique("glDrawArrays");
  interface_->DrawArrays(mode, first, count);
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

void GL_BINDING_CALL
MockGLInterface::Mock_glEndPixelLocalStorageImplicitANGLE() {
  MakeGlMockFunctionUnique("glEndPixelLocalStorageImplicitANGLE");
  interface_->EndPixelLocalStorageImplicitANGLE();
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
    GLenum internalformat,
    GLbitfield usage) {
  MakeGlMockFunctionUnique("glFramebufferMemorylessPixelLocalStorageANGLE");
  interface_->FramebufferMemorylessPixelLocalStorageANGLE(plane, internalformat,
                                                          usage);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glFramebufferParameteriMESA(GLenum target,
                                                  GLenum pname,
                                                  GLint param) {
  MakeGlMockFunctionUnique("glFramebufferParameteriMESA");
  interface_->FramebufferParameteriMESA(target, pname, param);
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
    GLint layer,
    GLbitfield usage) {
  MakeGlMockFunctionUnique("glFramebufferTexturePixelLocalStorageANGLE");
  interface_->FramebufferTexturePixelLocalStorageANGLE(plane, backingtexture,
                                                       level, layer, usage);
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
MockGLInterface::Mock_glGetDebugMessageLogKHR(GLuint count,
                                              GLsizei bufSize,
                                              GLenum* sources,
                                              GLenum* types,
                                              GLuint* ids,
                                              GLenum* severities,
                                              GLsizei* lengths,
                                              char* messageLog) {
  MakeGlMockFunctionUnique("glGetDebugMessageLogKHR");
  return interface_->GetDebugMessageLogKHR(count, bufSize, sources, types, ids,
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
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterfvRobustANGLE(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLfloat* params) {
  MakeGlMockFunctionUnique(
      "glGetFramebufferPixelLocalStorageParameterfvRobustANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterfvRobustANGLE(
      plane, pname, paramCount, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameterivRobustANGLE(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLint* params) {
  MakeGlMockFunctionUnique(
      "glGetFramebufferPixelLocalStorageParameterivRobustANGLE");
  interface_->GetFramebufferPixelLocalStorageParameterivRobustANGLE(
      plane, pname, paramCount, length, params);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetFramebufferPixelLocalStorageParameteruivRobustANGLE(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLuint* params) {
  MakeGlMockFunctionUnique(
      "glGetFramebufferPixelLocalStorageParameteruivRobustANGLE");
  interface_->GetFramebufferPixelLocalStorageParameteruivRobustANGLE(
      plane, pname, paramCount, length, params);
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

void GL_BINDING_CALL
MockGLInterface::Mock_glGetMultisamplefvRobustANGLE(GLenum pname,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLfloat* val) {
  MakeGlMockFunctionUnique("glGetMultisamplefvRobustANGLE");
  interface_->GetMultisamplefvRobustANGLE(pname, index, bufSize, length, val);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetObjectLabelKHR(GLenum identifier,
                                          GLuint name,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          char* label) {
  MakeGlMockFunctionUnique("glGetObjectLabelKHR");
  interface_->GetObjectLabelKHR(identifier, name, bufSize, length, label);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glGetObjectPtrLabelKHR(void* ptr,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             char* label) {
  MakeGlMockFunctionUnique("glGetObjectPtrLabelKHR");
  interface_->GetObjectPtrLabelKHR(ptr, bufSize, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glGetPointervKHR(GLenum pname,
                                                            void** params) {
  MakeGlMockFunctionUnique("glGetPointervKHR");
  interface_->GetPointervKHR(pname, params);
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
MockGLInterface::Mock_glGetTexLevelParameterfvANGLE(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLfloat* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterfvANGLE");
  interface_->GetTexLevelParameterfvANGLE(target, level, pname, params);
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
MockGLInterface::Mock_glGetTexLevelParameterivANGLE(GLenum target,
                                                    GLint level,
                                                    GLenum pname,
                                                    GLint* params) {
  MakeGlMockFunctionUnique("glGetTexLevelParameterivANGLE");
  interface_->GetTexLevelParameterivANGLE(target, level, pname, params);
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
MockGLInterface::Mock_glMemoryObjectParameterivEXT(GLuint memoryObject,
                                                   GLenum pname,
                                                   const GLint* param) {
  MakeGlMockFunctionUnique("glMemoryObjectParameterivEXT");
  interface_->MemoryObjectParameterivEXT(memoryObject, pname, param);
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

void GL_BINDING_CALL MockGLInterface::Mock_glObjectLabelKHR(GLenum identifier,
                                                            GLuint name,
                                                            GLsizei length,
                                                            const char* label) {
  MakeGlMockFunctionUnique("glObjectLabelKHR");
  interface_->ObjectLabelKHR(identifier, name, length, label);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glObjectPtrLabelKHR(void* ptr,
                                          GLsizei length,
                                          const char* label) {
  MakeGlMockFunctionUnique("glObjectPtrLabelKHR");
  interface_->ObjectPtrLabelKHR(ptr, length, label);
}

void GL_BINDING_CALL MockGLInterface::Mock_glPatchParameteriOES(GLenum pname,
                                                                GLint value) {
  MakeGlMockFunctionUnique("glPatchParameteriOES");
  interface_->PatchParameteriOES(pname, value);
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

void GL_BINDING_CALL MockGLInterface::Mock_glPopDebugGroupKHR() {
  MakeGlMockFunctionUnique("glPopDebugGroupKHR");
  interface_->PopDebugGroupKHR();
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

void GL_BINDING_CALL
MockGLInterface::Mock_glProvokingVertexANGLE(GLenum provokeMode) {
  MakeGlMockFunctionUnique("glProvokingVertexANGLE");
  interface_->ProvokingVertexANGLE(provokeMode);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glPushDebugGroupKHR(GLenum source,
                                          GLuint id,
                                          GLsizei length,
                                          const char* message) {
  MakeGlMockFunctionUnique("glPushDebugGroupKHR");
  interface_->PushDebugGroupKHR(source, id, length, message);
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

void GL_BINDING_CALL MockGLInterface::Mock_glTexBufferEXT(GLenum target,
                                                          GLenum internalformat,
                                                          GLuint buffer) {
  MakeGlMockFunctionUnique("glTexBufferEXT");
  interface_->TexBufferOES(target, internalformat, buffer);
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexBufferOES(GLenum target,
                                                          GLenum internalformat,
                                                          GLuint buffer) {
  MakeGlMockFunctionUnique("glTexBufferOES");
  interface_->TexBufferOES(target, internalformat, buffer);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexBufferRangeEXT(GLenum target,
                                          GLenum internalformat,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizeiptr size) {
  MakeGlMockFunctionUnique("glTexBufferRangeEXT");
  interface_->TexBufferRangeOES(target, internalformat, buffer, offset, size);
}

void GL_BINDING_CALL
MockGLInterface::Mock_glTexBufferRangeOES(GLenum target,
                                          GLenum internalformat,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizeiptr size) {
  MakeGlMockFunctionUnique("glTexBufferRangeOES");
  interface_->TexBufferRangeOES(target, internalformat, buffer, offset, size);
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

void GL_BINDING_CALL MockGLInterface::Mock_glValidateProgram(GLuint program) {
  MakeGlMockFunctionUnique("glValidateProgram");
  interface_->ValidateProgram(program);
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
  NOTREACHED();
}

GLFunctionPointerType GL_BINDING_CALL
MockGLInterface::GetGLProcAddress(const char* name) {
  std::string_view name_view(name);
  if (name_view == "glAcquireTexturesANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glAcquireTexturesANGLE);
  }
  if (name_view == "glActiveTexture") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glActiveTexture);
  }
  if (name_view == "glAttachShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glAttachShader);
  }
  if (name_view == "glBeginPixelLocalStorageANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBeginPixelLocalStorageANGLE);
  }
  if (name_view == "glBeginQuery") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBeginQuery);
  }
  if (name_view == "glBeginQueryEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBeginQueryEXT);
  }
  if (name_view == "glBeginTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBeginTransformFeedback);
  }
  if (name_view == "glBindAttribLocation") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindAttribLocation);
  }
  if (name_view == "glBindBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBuffer);
  }
  if (name_view == "glBindBufferBase") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBufferBase);
  }
  if (name_view == "glBindBufferRange") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindBufferRange);
  }
  if (name_view == "glBindFragDataLocationEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindFragDataLocationEXT);
  }
  if (name_view == "glBindFragDataLocationIndexedEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindFragDataLocationIndexedEXT);
  }
  if (name_view == "glBindFramebuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindFramebuffer);
  }
  if (name_view == "glBindRenderbuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindRenderbuffer);
  }
  if (name_view == "glBindSampler") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindSampler);
  }
  if (name_view == "glBindTexture") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindTexture);
  }
  if (name_view == "glBindTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindTransformFeedback);
  }
  if (name_view == "glBindUniformLocationCHROMIUM") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBindUniformLocationCHROMIUM);
  }
  if (name_view == "glBindVertexArray") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindVertexArray);
  }
  if (name_view == "glBindVertexArrayOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBindVertexArrayOES);
  }
  if (name_view == "glBlendBarrierKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendBarrierKHR);
  }
  if (name_view == "glBlendBarrierNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendBarrierNV);
  }
  if (name_view == "glBlendColor") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendColor);
  }
  if (name_view == "glBlendEquation") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendEquation);
  }
  if (name_view == "glBlendEquationSeparate") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendEquationSeparate);
  }
  if (name_view == "glBlendEquationSeparateiOES") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendEquationSeparateiOES);
  }
  if (name_view == "glBlendEquationiOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendEquationiOES);
  }
  if (name_view == "glBlendFunc") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFunc);
  }
  if (name_view == "glBlendFuncSeparate") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFuncSeparate);
  }
  if (name_view == "glBlendFuncSeparateiOES") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlendFuncSeparateiOES);
  }
  if (name_view == "glBlendFunciOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlendFunciOES);
  }
  if (name_view == "glBlitFramebuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebuffer);
  }
  if (name_view == "glBlitFramebufferANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebufferANGLE);
  }
  if (name_view == "glBlitFramebufferNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBlitFramebufferNV);
  }
  if (name_view == "glBlobCacheCallbacksANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glBlobCacheCallbacksANGLE);
  }
  if (name_view == "glBufferData") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBufferData);
  }
  if (name_view == "glBufferSubData") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glBufferSubData);
  }
  if (name_view == "glCheckFramebufferStatus") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCheckFramebufferStatus);
  }
  if (name_view == "glClear") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClear);
  }
  if (name_view == "glClearBufferfi") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferfi);
  }
  if (name_view == "glClearBufferfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferfv);
  }
  if (name_view == "glClearBufferiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferiv);
  }
  if (name_view == "glClearBufferuiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearBufferuiv);
  }
  if (name_view == "glClearColor") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearColor);
  }
  if (name_view == "glClearDepth") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearDepth);
  }
  if (name_view == "glClearDepthf") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearDepthf);
  }
  if (name_view == "glClearStencil") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearStencil);
  }
  if (name_view == "glClearTexImageEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexImageEXT);
  }
  if (name_view == "glClearTexSubImage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexSubImage);
  }
  if (name_view == "glClearTexSubImageEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClearTexSubImageEXT);
  }
  if (name_view == "glClientWaitSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClientWaitSync);
  }
  if (name_view == "glClipControlEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glClipControlEXT);
  }
  if (name_view == "glColorMask") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glColorMask);
  }
  if (name_view == "glColorMaskiOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glColorMaskiOES);
  }
  if (name_view == "glCompileShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompileShader);
  }
  if (name_view == "glCompressedTexImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompressedTexImage2D);
  }
  if (name_view == "glCompressedTexImage3D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCompressedTexImage3D);
  }
  if (name_view == "glCompressedTexSubImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage2D);
  }
  if (name_view == "glCompressedTexSubImage3D") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCompressedTexSubImage3D);
  }
  if (name_view == "glCopyBufferSubData") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyBufferSubData);
  }
  if (name_view == "glCopySubTextureCHROMIUM") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCopySubTextureCHROMIUM);
  }
  if (name_view == "glCopyTexImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexImage2D);
  }
  if (name_view == "glCopyTexSubImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexSubImage2D);
  }
  if (name_view == "glCopyTexSubImage3D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTexSubImage3D);
  }
  if (name_view == "glCopyTextureCHROMIUM") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCopyTextureCHROMIUM);
  }
  if (name_view == "glCreateMemoryObjectsEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glCreateMemoryObjectsEXT);
  }
  if (name_view == "glCreateProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCreateProgram);
  }
  if (name_view == "glCreateShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCreateShader);
  }
  if (name_view == "glCullFace") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glCullFace);
  }
  if (name_view == "glDebugMessageCallbackKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageCallbackKHR);
  }
  if (name_view == "glDebugMessageControlKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageControlKHR);
  }
  if (name_view == "glDebugMessageInsertKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDebugMessageInsertKHR);
  }
  if (name_view == "glDeleteBuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteBuffers);
  }
  if (name_view == "glDeleteFencesNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteFencesNV);
  }
  if (name_view == "glDeleteFramebuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteFramebuffers);
  }
  if (name_view == "glDeleteMemoryObjectsEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteMemoryObjectsEXT);
  }
  if (name_view == "glDeleteProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteProgram);
  }
  if (name_view == "glDeleteQueries") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteQueries);
  }
  if (name_view == "glDeleteQueriesEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteQueriesEXT);
  }
  if (name_view == "glDeleteRenderbuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteRenderbuffers);
  }
  if (name_view == "glDeleteSamplers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSamplers);
  }
  if (name_view == "glDeleteSemaphoresEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSemaphoresEXT);
  }
  if (name_view == "glDeleteShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteShader);
  }
  if (name_view == "glDeleteSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteSync);
  }
  if (name_view == "glDeleteTextures") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteTextures);
  }
  if (name_view == "glDeleteTransformFeedbacks") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteTransformFeedbacks);
  }
  if (name_view == "glDeleteVertexArrays") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDeleteVertexArrays);
  }
  if (name_view == "glDeleteVertexArraysOES") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDeleteVertexArraysOES);
  }
  if (name_view == "glDepthFunc") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthFunc);
  }
  if (name_view == "glDepthMask") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthMask);
  }
  if (name_view == "glDepthRange") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthRange);
  }
  if (name_view == "glDepthRangef") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDepthRangef);
  }
  if (name_view == "glDetachShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDetachShader);
  }
  if (name_view == "glDisable") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDisable);
  }
  if (name_view == "glDisableVertexAttribArray") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDisableVertexAttribArray);
  }
  if (name_view == "glDisableiOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDisableiOES);
  }
  if (name_view == "glDiscardFramebufferEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDiscardFramebufferEXT);
  }
  if (name_view == "glDrawArrays") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawArrays);
  }
  if (name_view == "glDrawArraysInstanced") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawArraysInstanced);
  }
  if (name_view == "glDrawArraysInstancedANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedANGLE);
  }
  if (name_view == "glDrawArraysInstancedBaseInstanceANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedBaseInstanceANGLE);
  }
  if (name_view == "glDrawArraysInstancedBaseInstanceEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawArraysInstancedBaseInstanceEXT);
  }
  if (name_view == "glDrawBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffer);
  }
  if (name_view == "glDrawBuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffers);
  }
  if (name_view == "glDrawBuffersEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawBuffersEXT);
  }
  if (name_view == "glDrawElements") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawElements);
  }
  if (name_view == "glDrawElementsInstanced") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstanced);
  }
  if (name_view == "glDrawElementsInstancedANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedANGLE);
  }
  if (name_view == "glDrawElementsInstancedBaseVertexBaseInstanceANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedBaseVertexBaseInstanceANGLE);
  }
  if (name_view == "glDrawElementsInstancedBaseVertexBaseInstanceEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glDrawElementsInstancedBaseVertexBaseInstanceEXT);
  }
  if (name_view == "glDrawRangeElements") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glDrawRangeElements);
  }
  if (name_view == "glEGLImageTargetRenderbufferStorageOES") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEGLImageTargetRenderbufferStorageOES);
  }
  if (name_view == "glEGLImageTargetTexture2DOES") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEGLImageTargetTexture2DOES);
  }
  if (name_view == "glEnable") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEnable);
  }
  if (name_view == "glEnableVertexAttribArray") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEnableVertexAttribArray);
  }
  if (name_view == "glEnableiOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEnableiOES);
  }
  if (name_view == "glEndPixelLocalStorageANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEndPixelLocalStorageANGLE);
  }
  if (name_view == "glEndPixelLocalStorageImplicitANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glEndPixelLocalStorageImplicitANGLE);
  }
  if (name_view == "glEndQuery") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndQuery);
  }
  if (name_view == "glEndQueryEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndQueryEXT);
  }
  if (name_view == "glEndTilingQCOM") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndTilingQCOM);
  }
  if (name_view == "glEndTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glEndTransformFeedback);
  }
  if (name_view == "glFenceSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFenceSync);
  }
  if (name_view == "glFinish") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFinish);
  }
  if (name_view == "glFinishFenceNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFinishFenceNV);
  }
  if (name_view == "glFlush") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFlush);
  }
  if (name_view == "glFlushMappedBufferRange") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFlushMappedBufferRange);
  }
  if (name_view == "glFlushMappedBufferRangeEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFlushMappedBufferRangeEXT);
  }
  if (name_view == "glFramebufferMemorylessPixelLocalStorageANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferMemorylessPixelLocalStorageANGLE);
  }
  if (name_view == "glFramebufferParameteriMESA") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferParameteriMESA);
  }
  if (name_view == "glFramebufferPixelLocalClearValuefvANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValuefvANGLE);
  }
  if (name_view == "glFramebufferPixelLocalClearValueivANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValueivANGLE);
  }
  if (name_view == "glFramebufferPixelLocalClearValueuivANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalClearValueuivANGLE);
  }
  if (name_view == "glFramebufferPixelLocalStorageInterruptANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalStorageInterruptANGLE);
  }
  if (name_view == "glFramebufferPixelLocalStorageRestoreANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferPixelLocalStorageRestoreANGLE);
  }
  if (name_view == "glFramebufferRenderbuffer") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferRenderbuffer);
  }
  if (name_view == "glFramebufferTexture2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFramebufferTexture2D);
  }
  if (name_view == "glFramebufferTexture2DMultisampleEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexture2DMultisampleEXT);
  }
  if (name_view == "glFramebufferTexture2DMultisampleIMG") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexture2DMultisampleIMG);
  }
  if (name_view == "glFramebufferTextureLayer") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTextureLayer);
  }
  if (name_view == "glFramebufferTextureMultiviewOVR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTextureMultiviewOVR);
  }
  if (name_view == "glFramebufferTexturePixelLocalStorageANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glFramebufferTexturePixelLocalStorageANGLE);
  }
  if (name_view == "glFrontFace") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glFrontFace);
  }
  if (name_view == "glGenBuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenBuffers);
  }
  if (name_view == "glGenFencesNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenFencesNV);
  }
  if (name_view == "glGenFramebuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenFramebuffers);
  }
  if (name_view == "glGenQueries") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenQueries);
  }
  if (name_view == "glGenQueriesEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenQueriesEXT);
  }
  if (name_view == "glGenRenderbuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenRenderbuffers);
  }
  if (name_view == "glGenSamplers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenSamplers);
  }
  if (name_view == "glGenSemaphoresEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenSemaphoresEXT);
  }
  if (name_view == "glGenTextures") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenTextures);
  }
  if (name_view == "glGenTransformFeedbacks") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGenTransformFeedbacks);
  }
  if (name_view == "glGenVertexArrays") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenVertexArrays);
  }
  if (name_view == "glGenVertexArraysOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenVertexArraysOES);
  }
  if (name_view == "glGenerateMipmap") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGenerateMipmap);
  }
  if (name_view == "glGetActiveAttrib") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveAttrib);
  }
  if (name_view == "glGetActiveUniform") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveUniform);
  }
  if (name_view == "glGetActiveUniformBlockName") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockName);
  }
  if (name_view == "glGetActiveUniformBlockiv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockiv);
  }
  if (name_view == "glGetActiveUniformBlockivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetActiveUniformBlockivRobustANGLE);
  }
  if (name_view == "glGetActiveUniformsiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetActiveUniformsiv);
  }
  if (name_view == "glGetAttachedShaders") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetAttachedShaders);
  }
  if (name_view == "glGetAttribLocation") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetAttribLocation);
  }
  if (name_view == "glGetBooleanv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetBooleanv);
  }
  if (name_view == "glGetBooleanvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBooleanvRobustANGLE);
  }
  if (name_view == "glGetBufferParameteri64vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferParameteri64vRobustANGLE);
  }
  if (name_view == "glGetBufferParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetBufferParameteriv);
  }
  if (name_view == "glGetBufferParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferParameterivRobustANGLE);
  }
  if (name_view == "glGetBufferPointervRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetBufferPointervRobustANGLE);
  }
  if (name_view == "glGetDebugMessageLogKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetDebugMessageLogKHR);
  }
  if (name_view == "glGetError") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetError);
  }
  if (name_view == "glGetFenceivNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFenceivNV);
  }
  if (name_view == "glGetFloatv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFloatv);
  }
  if (name_view == "glGetFloatvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFloatvRobustANGLE);
  }
  if (name_view == "glGetFragDataIndexEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFragDataIndexEXT);
  }
  if (name_view == "glGetFragDataLocation") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetFragDataLocation);
  }
  if (name_view == "glGetFramebufferAttachmentParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferAttachmentParameteriv);
  }
  if (name_view == "glGetFramebufferAttachmentParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferAttachmentParameterivRobustANGLE);
  }
  if (name_view == "glGetFramebufferPixelLocalStorageParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterfvRobustANGLE);
  }
  if (name_view == "glGetFramebufferPixelLocalStorageParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameterivRobustANGLE);
  }
  if (name_view == "glGetFramebufferPixelLocalStorageParameteruivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetFramebufferPixelLocalStorageParameteruivRobustANGLE);
  }
  if (name_view == "glGetGraphicsResetStatusEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetGraphicsResetStatusEXT);
  }
  if (name_view == "glGetGraphicsResetStatusKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetGraphicsResetStatusKHR);
  }
  if (name_view == "glGetInteger64i_v") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInteger64i_v);
  }
  if (name_view == "glGetInteger64i_vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInteger64i_vRobustANGLE);
  }
  if (name_view == "glGetInteger64v") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInteger64v);
  }
  if (name_view == "glGetInteger64vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInteger64vRobustANGLE);
  }
  if (name_view == "glGetIntegeri_v") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetIntegeri_v);
  }
  if (name_view == "glGetIntegeri_vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetIntegeri_vRobustANGLE);
  }
  if (name_view == "glGetIntegerv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetIntegerv);
  }
  if (name_view == "glGetIntegervRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetIntegervRobustANGLE);
  }
  if (name_view == "glGetInternalformatSampleivNV") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInternalformatSampleivNV);
  }
  if (name_view == "glGetInternalformativ") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetInternalformativ);
  }
  if (name_view == "glGetInternalformativRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetInternalformativRobustANGLE);
  }
  if (name_view == "glGetMultisamplefvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetMultisamplefvRobustANGLE);
  }
  if (name_view == "glGetObjectLabelKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectLabelKHR);
  }
  if (name_view == "glGetObjectPtrLabelKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetObjectPtrLabelKHR);
  }
  if (name_view == "glGetPointervKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetPointervKHR);
  }
  if (name_view == "glGetProgramBinary") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramBinary);
  }
  if (name_view == "glGetProgramBinaryOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramBinaryOES);
  }
  if (name_view == "glGetProgramInfoLog") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramInfoLog);
  }
  if (name_view == "glGetProgramiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetProgramiv);
  }
  if (name_view == "glGetProgramivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetProgramivRobustANGLE);
  }
  if (name_view == "glGetQueryObjecti64vEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjecti64vEXT);
  }
  if (name_view == "glGetQueryObjecti64vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjecti64vRobustANGLE);
  }
  if (name_view == "glGetQueryObjectivEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectivEXT);
  }
  if (name_view == "glGetQueryObjectivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectivRobustANGLE);
  }
  if (name_view == "glGetQueryObjectui64vEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectui64vEXT);
  }
  if (name_view == "glGetQueryObjectui64vRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectui64vRobustANGLE);
  }
  if (name_view == "glGetQueryObjectuiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectuiv);
  }
  if (name_view == "glGetQueryObjectuivEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryObjectuivEXT);
  }
  if (name_view == "glGetQueryObjectuivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryObjectuivRobustANGLE);
  }
  if (name_view == "glGetQueryiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryiv);
  }
  if (name_view == "glGetQueryivEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetQueryivEXT);
  }
  if (name_view == "glGetQueryivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetQueryivRobustANGLE);
  }
  if (name_view == "glGetRenderbufferParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetRenderbufferParameteriv);
  }
  if (name_view == "glGetRenderbufferParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetRenderbufferParameterivRobustANGLE);
  }
  if (name_view == "glGetSamplerParameterfv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterfv);
  }
  if (name_view == "glGetSamplerParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterfvRobustANGLE);
  }
  if (name_view == "glGetSamplerParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameteriv);
  }
  if (name_view == "glGetSamplerParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetSamplerParameterivRobustANGLE);
  }
  if (name_view == "glGetShaderInfoLog") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderInfoLog);
  }
  if (name_view == "glGetShaderPrecisionFormat") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetShaderPrecisionFormat);
  }
  if (name_view == "glGetShaderSource") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderSource);
  }
  if (name_view == "glGetShaderiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetShaderiv);
  }
  if (name_view == "glGetShaderivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetShaderivRobustANGLE);
  }
  if (name_view == "glGetString") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetString);
  }
  if (name_view == "glGetStringi") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetStringi);
  }
  if (name_view == "glGetSynciv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetSynciv);
  }
  if (name_view == "glGetTexLevelParameterfvANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterfvANGLE);
  }
  if (name_view == "glGetTexLevelParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterfvRobustANGLE);
  }
  if (name_view == "glGetTexLevelParameterivANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterivANGLE);
  }
  if (name_view == "glGetTexLevelParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexLevelParameterivRobustANGLE);
  }
  if (name_view == "glGetTexParameterfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetTexParameterfv);
  }
  if (name_view == "glGetTexParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterfvRobustANGLE);
  }
  if (name_view == "glGetTexParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetTexParameteriv);
  }
  if (name_view == "glGetTexParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTexParameterivRobustANGLE);
  }
  if (name_view == "glGetTransformFeedbackVarying") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTransformFeedbackVarying);
  }
  if (name_view == "glGetTranslatedShaderSourceANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetTranslatedShaderSourceANGLE);
  }
  if (name_view == "glGetUniformBlockIndex") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformBlockIndex);
  }
  if (name_view == "glGetUniformIndices") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformIndices);
  }
  if (name_view == "glGetUniformLocation") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformLocation);
  }
  if (name_view == "glGetUniformfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformfv);
  }
  if (name_view == "glGetUniformfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformfvRobustANGLE);
  }
  if (name_view == "glGetUniformiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformiv);
  }
  if (name_view == "glGetUniformivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformivRobustANGLE);
  }
  if (name_view == "glGetUniformuiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetUniformuiv);
  }
  if (name_view == "glGetUniformuivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetUniformuivRobustANGLE);
  }
  if (name_view == "glGetVertexAttribIivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribIivRobustANGLE);
  }
  if (name_view == "glGetVertexAttribIuivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribIuivRobustANGLE);
  }
  if (name_view == "glGetVertexAttribPointerv") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribPointerv);
  }
  if (name_view == "glGetVertexAttribPointervRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribPointervRobustANGLE);
  }
  if (name_view == "glGetVertexAttribfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetVertexAttribfv);
  }
  if (name_view == "glGetVertexAttribfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribfvRobustANGLE);
  }
  if (name_view == "glGetVertexAttribiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glGetVertexAttribiv);
  }
  if (name_view == "glGetVertexAttribivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glGetVertexAttribivRobustANGLE);
  }
  if (name_view == "glHint") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glHint);
  }
  if (name_view == "glImportMemoryFdEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glImportMemoryFdEXT);
  }
  if (name_view == "glImportMemoryWin32HandleEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportMemoryWin32HandleEXT);
  }
  if (name_view == "glImportMemoryZirconHandleANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportMemoryZirconHandleANGLE);
  }
  if (name_view == "glImportSemaphoreFdEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glImportSemaphoreFdEXT);
  }
  if (name_view == "glImportSemaphoreWin32HandleEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportSemaphoreWin32HandleEXT);
  }
  if (name_view == "glImportSemaphoreZirconHandleANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glImportSemaphoreZirconHandleANGLE);
  }
  if (name_view == "glInsertEventMarkerEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glInsertEventMarkerEXT);
  }
  if (name_view == "glInvalidateFramebuffer") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateFramebuffer);
  }
  if (name_view == "glInvalidateSubFramebuffer") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateSubFramebuffer);
  }
  if (name_view == "glInvalidateTextureANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glInvalidateTextureANGLE);
  }
  if (name_view == "glIsBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsBuffer);
  }
  if (name_view == "glIsEnabled") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsEnabled);
  }
  if (name_view == "glIsEnablediOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsEnablediOES);
  }
  if (name_view == "glIsFenceNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsFenceNV);
  }
  if (name_view == "glIsFramebuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsFramebuffer);
  }
  if (name_view == "glIsProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsProgram);
  }
  if (name_view == "glIsQuery") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsQuery);
  }
  if (name_view == "glIsQueryEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsQueryEXT);
  }
  if (name_view == "glIsRenderbuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsRenderbuffer);
  }
  if (name_view == "glIsSampler") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsSampler);
  }
  if (name_view == "glIsShader") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsShader);
  }
  if (name_view == "glIsSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsSync);
  }
  if (name_view == "glIsTexture") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsTexture);
  }
  if (name_view == "glIsTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsTransformFeedback);
  }
  if (name_view == "glIsVertexArray") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsVertexArray);
  }
  if (name_view == "glIsVertexArrayOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glIsVertexArrayOES);
  }
  if (name_view == "glLineWidth") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glLineWidth);
  }
  if (name_view == "glLinkProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glLinkProgram);
  }
  if (name_view == "glMapBufferOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferOES);
  }
  if (name_view == "glMapBufferRange") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferRange);
  }
  if (name_view == "glMapBufferRangeEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMapBufferRangeEXT);
  }
  if (name_view == "glMaxShaderCompilerThreadsKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMaxShaderCompilerThreadsKHR);
  }
  if (name_view == "glMemoryObjectParameterivEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMemoryObjectParameterivEXT);
  }
  if (name_view == "glMultiDrawArraysANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glMultiDrawArraysANGLE);
  }
  if (name_view == "glMultiDrawArraysInstancedANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawArraysInstancedANGLE);
  }
  if (name_view == "glMultiDrawArraysInstancedBaseInstanceANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawArraysInstancedBaseInstanceANGLE);
  }
  if (name_view == "glMultiDrawElementsANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsANGLE);
  }
  if (name_view == "glMultiDrawElementsInstancedANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsInstancedANGLE);
  }
  if (name_view == "glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE);
  }
  if (name_view == "glObjectLabelKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectLabelKHR);
  }
  if (name_view == "glObjectPtrLabelKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glObjectPtrLabelKHR);
  }
  if (name_view == "glPatchParameteriOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPatchParameteriOES);
  }
  if (name_view == "glPauseTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPauseTransformFeedback);
  }
  if (name_view == "glPixelLocalStorageBarrierANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPixelLocalStorageBarrierANGLE);
  }
  if (name_view == "glPixelStorei") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPixelStorei);
  }
  if (name_view == "glPointParameteri") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPointParameteri);
  }
  if (name_view == "glPolygonMode") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonMode);
  }
  if (name_view == "glPolygonModeANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonModeANGLE);
  }
  if (name_view == "glPolygonOffset") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPolygonOffset);
  }
  if (name_view == "glPolygonOffsetClampEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPolygonOffsetClampEXT);
  }
  if (name_view == "glPopDebugGroupKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPopDebugGroupKHR);
  }
  if (name_view == "glPopGroupMarkerEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPopGroupMarkerEXT);
  }
  if (name_view == "glPrimitiveRestartIndex") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glPrimitiveRestartIndex);
  }
  if (name_view == "glProgramBinary") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramBinary);
  }
  if (name_view == "glProgramBinaryOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramBinaryOES);
  }
  if (name_view == "glProgramParameteri") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProgramParameteri);
  }
  if (name_view == "glProvokingVertexANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glProvokingVertexANGLE);
  }
  if (name_view == "glPushDebugGroupKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPushDebugGroupKHR);
  }
  if (name_view == "glPushGroupMarkerEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glPushGroupMarkerEXT);
  }
  if (name_view == "glQueryCounterEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glQueryCounterEXT);
  }
  if (name_view == "glReadBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReadBuffer);
  }
  if (name_view == "glReadPixels") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReadPixels);
  }
  if (name_view == "glReadPixelsRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glReadPixelsRobustANGLE);
  }
  if (name_view == "glReleaseShaderCompiler") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glReleaseShaderCompiler);
  }
  if (name_view == "glReleaseTexturesANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glReleaseTexturesANGLE);
  }
  if (name_view == "glRenderbufferStorage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glRenderbufferStorage);
  }
  if (name_view == "glRenderbufferStorageMultisample") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisample);
  }
  if (name_view == "glRenderbufferStorageMultisampleANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleANGLE);
  }
  if (name_view == "glRenderbufferStorageMultisampleAdvancedAMD") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleAdvancedAMD);
  }
  if (name_view == "glRenderbufferStorageMultisampleEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleEXT);
  }
  if (name_view == "glRenderbufferStorageMultisampleIMG") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRenderbufferStorageMultisampleIMG);
  }
  if (name_view == "glRequestExtensionANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glRequestExtensionANGLE);
  }
  if (name_view == "glResumeTransformFeedback") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glResumeTransformFeedback);
  }
  if (name_view == "glSampleCoverage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSampleCoverage);
  }
  if (name_view == "glSamplerParameterf") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameterf);
  }
  if (name_view == "glSamplerParameterfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameterfv);
  }
  if (name_view == "glSamplerParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterfvRobustANGLE);
  }
  if (name_view == "glSamplerParameteri") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameteri);
  }
  if (name_view == "glSamplerParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSamplerParameteriv);
  }
  if (name_view == "glSamplerParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glSamplerParameterivRobustANGLE);
  }
  if (name_view == "glScissor") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glScissor);
  }
  if (name_view == "glSetFenceNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSetFenceNV);
  }
  if (name_view == "glShaderBinary") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glShaderBinary);
  }
  if (name_view == "glShaderSource") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glShaderSource);
  }
  if (name_view == "glSignalSemaphoreEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glSignalSemaphoreEXT);
  }
  if (name_view == "glStartTilingQCOM") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStartTilingQCOM);
  }
  if (name_view == "glStencilFunc") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilFunc);
  }
  if (name_view == "glStencilFuncSeparate") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilFuncSeparate);
  }
  if (name_view == "glStencilMask") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilMask);
  }
  if (name_view == "glStencilMaskSeparate") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilMaskSeparate);
  }
  if (name_view == "glStencilOp") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilOp);
  }
  if (name_view == "glStencilOpSeparate") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glStencilOpSeparate);
  }
  if (name_view == "glTestFenceNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTestFenceNV);
  }
  if (name_view == "glTexBufferEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferEXT);
  }
  if (name_view == "glTexBufferOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferOES);
  }
  if (name_view == "glTexBufferRangeEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferRangeEXT);
  }
  if (name_view == "glTexBufferRangeOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexBufferRangeOES);
  }
  if (name_view == "glTexImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexImage2D);
  }
  if (name_view == "glTexImage2DExternalANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage2DExternalANGLE);
  }
  if (name_view == "glTexImage2DRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage2DRobustANGLE);
  }
  if (name_view == "glTexImage3D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexImage3D);
  }
  if (name_view == "glTexImage3DRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexImage3DRobustANGLE);
  }
  if (name_view == "glTexParameterf") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameterf);
  }
  if (name_view == "glTexParameterfv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameterfv);
  }
  if (name_view == "glTexParameterfvRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterfvRobustANGLE);
  }
  if (name_view == "glTexParameteri") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameteri);
  }
  if (name_view == "glTexParameteriv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexParameteriv);
  }
  if (name_view == "glTexParameterivRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexParameterivRobustANGLE);
  }
  if (name_view == "glTexStorage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage2D);
  }
  if (name_view == "glTexStorage2DEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage2DEXT);
  }
  if (name_view == "glTexStorage3D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorage3D);
  }
  if (name_view == "glTexStorageMem2DEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexStorageMem2DEXT);
  }
  if (name_view == "glTexStorageMemFlags2DANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexStorageMemFlags2DANGLE);
  }
  if (name_view == "glTexSubImage2D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexSubImage2D);
  }
  if (name_view == "glTexSubImage2DRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexSubImage2DRobustANGLE);
  }
  if (name_view == "glTexSubImage3D") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glTexSubImage3D);
  }
  if (name_view == "glTexSubImage3DRobustANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTexSubImage3DRobustANGLE);
  }
  if (name_view == "glTransformFeedbackVaryings") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glTransformFeedbackVaryings);
  }
  if (name_view == "glUniform1f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1f);
  }
  if (name_view == "glUniform1fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1fv);
  }
  if (name_view == "glUniform1i") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1i);
  }
  if (name_view == "glUniform1iv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1iv);
  }
  if (name_view == "glUniform1ui") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1ui);
  }
  if (name_view == "glUniform1uiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform1uiv);
  }
  if (name_view == "glUniform2f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2f);
  }
  if (name_view == "glUniform2fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2fv);
  }
  if (name_view == "glUniform2i") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2i);
  }
  if (name_view == "glUniform2iv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2iv);
  }
  if (name_view == "glUniform2ui") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2ui);
  }
  if (name_view == "glUniform2uiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform2uiv);
  }
  if (name_view == "glUniform3f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3f);
  }
  if (name_view == "glUniform3fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3fv);
  }
  if (name_view == "glUniform3i") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3i);
  }
  if (name_view == "glUniform3iv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3iv);
  }
  if (name_view == "glUniform3ui") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3ui);
  }
  if (name_view == "glUniform3uiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform3uiv);
  }
  if (name_view == "glUniform4f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4f);
  }
  if (name_view == "glUniform4fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4fv);
  }
  if (name_view == "glUniform4i") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4i);
  }
  if (name_view == "glUniform4iv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4iv);
  }
  if (name_view == "glUniform4ui") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4ui);
  }
  if (name_view == "glUniform4uiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniform4uiv);
  }
  if (name_view == "glUniformBlockBinding") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformBlockBinding);
  }
  if (name_view == "glUniformMatrix2fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2fv);
  }
  if (name_view == "glUniformMatrix2x3fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2x3fv);
  }
  if (name_view == "glUniformMatrix2x4fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix2x4fv);
  }
  if (name_view == "glUniformMatrix3fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3fv);
  }
  if (name_view == "glUniformMatrix3x2fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3x2fv);
  }
  if (name_view == "glUniformMatrix3x4fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix3x4fv);
  }
  if (name_view == "glUniformMatrix4fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4fv);
  }
  if (name_view == "glUniformMatrix4x2fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4x2fv);
  }
  if (name_view == "glUniformMatrix4x3fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUniformMatrix4x3fv);
  }
  if (name_view == "glUnmapBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUnmapBuffer);
  }
  if (name_view == "glUnmapBufferOES") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUnmapBufferOES);
  }
  if (name_view == "glUseProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glUseProgram);
  }
  if (name_view == "glValidateProgram") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glValidateProgram);
  }
  if (name_view == "glVertexAttrib1f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib1f);
  }
  if (name_view == "glVertexAttrib1fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib1fv);
  }
  if (name_view == "glVertexAttrib2f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib2f);
  }
  if (name_view == "glVertexAttrib2fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib2fv);
  }
  if (name_view == "glVertexAttrib3f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib3f);
  }
  if (name_view == "glVertexAttrib3fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib3fv);
  }
  if (name_view == "glVertexAttrib4f") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib4f);
  }
  if (name_view == "glVertexAttrib4fv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttrib4fv);
  }
  if (name_view == "glVertexAttribDivisor") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribDivisor);
  }
  if (name_view == "glVertexAttribDivisorANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glVertexAttribDivisorANGLE);
  }
  if (name_view == "glVertexAttribDivisorEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_glVertexAttribDivisorEXT);
  }
  if (name_view == "glVertexAttribI4i") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4i);
  }
  if (name_view == "glVertexAttribI4iv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4iv);
  }
  if (name_view == "glVertexAttribI4ui") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4ui);
  }
  if (name_view == "glVertexAttribI4uiv") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribI4uiv);
  }
  if (name_view == "glVertexAttribIPointer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribIPointer);
  }
  if (name_view == "glVertexAttribPointer") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glVertexAttribPointer);
  }
  if (name_view == "glViewport") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glViewport);
  }
  if (name_view == "glWaitSemaphoreEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWaitSemaphoreEXT);
  }
  if (name_view == "glWaitSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWaitSync);
  }
  if (name_view == "glWindowRectanglesEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_glWindowRectanglesEXT);
  }
  return reinterpret_cast<GLFunctionPointerType>(&MockGlInvalidFunction);
}

}  // namespace gl
