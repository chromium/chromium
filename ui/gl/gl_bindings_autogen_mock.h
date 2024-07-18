// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// Silence presubmit and Tricium warnings about include guards
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

static void GL_BINDING_CALL Mock_glAcquireTexturesANGLE(GLuint numTextures,
                                                        const GLuint* textures,
                                                        const GLenum* layouts);
static void GL_BINDING_CALL Mock_glActiveShaderProgram(GLuint pipeline,
                                                       GLuint program);
static void GL_BINDING_CALL Mock_glActiveTexture(GLenum texture);
static void GL_BINDING_CALL Mock_glAttachShader(GLuint program, GLuint shader);
static void GL_BINDING_CALL
Mock_glBeginPixelLocalStorageANGLE(GLsizei n, const GLenum* loadops);
static void GL_BINDING_CALL Mock_glBeginQuery(GLenum target, GLuint id);
static void GL_BINDING_CALL Mock_glBeginQueryEXT(GLenum target, GLuint id);
static void GL_BINDING_CALL Mock_glBeginTransformFeedback(GLenum primitiveMode);
static void GL_BINDING_CALL Mock_glBindAttribLocation(GLuint program,
                                                      GLuint index,
                                                      const char* name);
static void GL_BINDING_CALL Mock_glBindBuffer(GLenum target, GLuint buffer);
static void GL_BINDING_CALL Mock_glBindBufferBase(GLenum target,
                                                  GLuint index,
                                                  GLuint buffer);
static void GL_BINDING_CALL Mock_glBindBufferRange(GLenum target,
                                                   GLuint index,
                                                   GLuint buffer,
                                                   GLintptr offset,
                                                   GLsizeiptr size);
static void GL_BINDING_CALL Mock_glBindFragDataLocationEXT(GLuint program,
                                                           GLuint colorNumber,
                                                           const char* name);
static void GL_BINDING_CALL
Mock_glBindFragDataLocationIndexedEXT(GLuint program,
                                      GLuint colorNumber,
                                      GLuint index,
                                      const char* name);
static void GL_BINDING_CALL Mock_glBindFramebuffer(GLenum target,
                                                   GLuint framebuffer);
static void GL_BINDING_CALL Mock_glBindImageTexture(GLuint index,
                                                    GLuint texture,
                                                    GLint level,
                                                    GLboolean layered,
                                                    GLint layer,
                                                    GLenum access,
                                                    GLint format);
static void GL_BINDING_CALL Mock_glBindImageTextureEXT(GLuint index,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLboolean layered,
                                                       GLint layer,
                                                       GLenum access,
                                                       GLint format);
static void GL_BINDING_CALL Mock_glBindProgramPipeline(GLuint pipeline);
static void GL_BINDING_CALL Mock_glBindRenderbuffer(GLenum target,
                                                    GLuint renderbuffer);
static void GL_BINDING_CALL Mock_glBindSampler(GLuint unit, GLuint sampler);
static void GL_BINDING_CALL Mock_glBindTexture(GLenum target, GLuint texture);
static void GL_BINDING_CALL Mock_glBindTransformFeedback(GLenum target,
                                                         GLuint id);
static void GL_BINDING_CALL
Mock_glBindUniformLocationCHROMIUM(GLuint program,
                                   GLint location,
                                   const char* name);
static void GL_BINDING_CALL Mock_glBindVertexArray(GLuint array);
static void GL_BINDING_CALL Mock_glBindVertexArrayOES(GLuint array);
static void GL_BINDING_CALL Mock_glBindVertexBuffer(GLuint bindingindex,
                                                    GLuint buffer,
                                                    GLintptr offset,
                                                    GLsizei stride);
static void GL_BINDING_CALL Mock_glBlendBarrierKHR(void);
static void GL_BINDING_CALL Mock_glBlendBarrierNV(void);
static void GL_BINDING_CALL Mock_glBlendColor(GLclampf red,
                                              GLclampf green,
                                              GLclampf blue,
                                              GLclampf alpha);
static void GL_BINDING_CALL Mock_glBlendEquation(GLenum mode);
static void GL_BINDING_CALL Mock_glBlendEquationSeparate(GLenum modeRGB,
                                                         GLenum modeAlpha);
static void GL_BINDING_CALL Mock_glBlendEquationSeparatei(GLuint buf,
                                                          GLenum modeRGB,
                                                          GLenum modeAlpha);
static void GL_BINDING_CALL Mock_glBlendEquationSeparateiOES(GLuint buf,
                                                             GLenum modeRGB,
                                                             GLenum modeAlpha);
static void GL_BINDING_CALL Mock_glBlendEquationi(GLuint buf, GLenum mode);
static void GL_BINDING_CALL Mock_glBlendEquationiOES(GLuint buf, GLenum mode);
static void GL_BINDING_CALL Mock_glBlendFunc(GLenum sfactor, GLenum dfactor);
static void GL_BINDING_CALL Mock_glBlendFuncSeparate(GLenum srcRGB,
                                                     GLenum dstRGB,
                                                     GLenum srcAlpha,
                                                     GLenum dstAlpha);
static void GL_BINDING_CALL Mock_glBlendFuncSeparatei(GLuint buf,
                                                      GLenum srcRGB,
                                                      GLenum dstRGB,
                                                      GLenum srcAlpha,
                                                      GLenum dstAlpha);
static void GL_BINDING_CALL Mock_glBlendFuncSeparateiOES(GLuint buf,
                                                         GLenum srcRGB,
                                                         GLenum dstRGB,
                                                         GLenum srcAlpha,
                                                         GLenum dstAlpha);
static void GL_BINDING_CALL Mock_glBlendFunci(GLuint buf,
                                              GLenum sfactor,
                                              GLenum dfactor);
static void GL_BINDING_CALL Mock_glBlendFunciOES(GLuint buf,
                                                 GLenum sfactor,
                                                 GLenum dfactor);
static void GL_BINDING_CALL Mock_glBlitFramebuffer(GLint srcX0,
                                                   GLint srcY0,
                                                   GLint srcX1,
                                                   GLint srcY1,
                                                   GLint dstX0,
                                                   GLint dstY0,
                                                   GLint dstX1,
                                                   GLint dstY1,
                                                   GLbitfield mask,
                                                   GLenum filter);
static void GL_BINDING_CALL Mock_glBlitFramebufferANGLE(GLint srcX0,
                                                        GLint srcY0,
                                                        GLint srcX1,
                                                        GLint srcY1,
                                                        GLint dstX0,
                                                        GLint dstY0,
                                                        GLint dstX1,
                                                        GLint dstY1,
                                                        GLbitfield mask,
                                                        GLenum filter);
static void GL_BINDING_CALL Mock_glBlitFramebufferNV(GLint srcX0,
                                                     GLint srcY0,
                                                     GLint srcX1,
                                                     GLint srcY1,
                                                     GLint dstX0,
                                                     GLint dstY0,
                                                     GLint dstX1,
                                                     GLint dstY1,
                                                     GLbitfield mask,
                                                     GLenum filter);
static void GL_BINDING_CALL Mock_glBufferData(GLenum target,
                                              GLsizeiptr size,
                                              const void* data,
                                              GLenum usage);
static void GL_BINDING_CALL Mock_glBufferSubData(GLenum target,
                                                 GLintptr offset,
                                                 GLsizeiptr size,
                                                 const void* data);
static GLenum GL_BINDING_CALL Mock_glCheckFramebufferStatus(GLenum target);
static void GL_BINDING_CALL Mock_glClear(GLbitfield mask);
static void GL_BINDING_CALL Mock_glClearBufferfi(GLenum buffer,
                                                 GLint drawbuffer,
                                                 const GLfloat depth,
                                                 GLint stencil);
static void GL_BINDING_CALL Mock_glClearBufferfv(GLenum buffer,
                                                 GLint drawbuffer,
                                                 const GLfloat* value);
static void GL_BINDING_CALL Mock_glClearBufferiv(GLenum buffer,
                                                 GLint drawbuffer,
                                                 const GLint* value);
static void GL_BINDING_CALL Mock_glClearBufferuiv(GLenum buffer,
                                                  GLint drawbuffer,
                                                  const GLuint* value);
static void GL_BINDING_CALL Mock_glClearColor(GLclampf red,
                                              GLclampf green,
                                              GLclampf blue,
                                              GLclampf alpha);
static void GL_BINDING_CALL Mock_glClearDepth(GLclampd depth);
static void GL_BINDING_CALL Mock_glClearDepthf(GLclampf depth);
static void GL_BINDING_CALL Mock_glClearStencil(GLint s);
static void GL_BINDING_CALL Mock_glClearTexImageEXT(GLuint texture,
                                                    GLint level,
                                                    GLenum format,
                                                    GLenum type,
                                                    const GLvoid* data);
static void GL_BINDING_CALL Mock_glClearTexSubImage(GLuint texture,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLint zoffset,
                                                    GLint width,
                                                    GLint height,
                                                    GLint depth,
                                                    GLenum format,
                                                    GLenum type,
                                                    const GLvoid* data);
static void GL_BINDING_CALL Mock_glClearTexSubImageEXT(GLuint texture,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLint width,
                                                       GLint height,
                                                       GLint depth,
                                                       GLenum format,
                                                       GLenum type,
                                                       const GLvoid* data);
static GLenum GL_BINDING_CALL Mock_glClientWaitSync(GLsync sync,
                                                    GLbitfield flags,
                                                    GLuint64 timeout);
static void GL_BINDING_CALL Mock_glClipControlEXT(GLenum origin, GLenum depth);
static void GL_BINDING_CALL Mock_glColorMask(GLboolean red,
                                             GLboolean green,
                                             GLboolean blue,
                                             GLboolean alpha);
static void GL_BINDING_CALL Mock_glColorMaski(GLuint buf,
                                              GLboolean red,
                                              GLboolean green,
                                              GLboolean blue,
                                              GLboolean alpha);
static void GL_BINDING_CALL Mock_glColorMaskiOES(GLuint buf,
                                                 GLboolean red,
                                                 GLboolean green,
                                                 GLboolean blue,
                                                 GLboolean alpha);
static void GL_BINDING_CALL Mock_glCompileShader(GLuint shader);
static void GL_BINDING_CALL Mock_glCompressedTexImage2D(GLenum target,
                                                        GLint level,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLint border,
                                                        GLsizei imageSize,
                                                        const void* data);
static void GL_BINDING_CALL
Mock_glCompressedTexImage2DRobustANGLE(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLsizei imageSize,
                                       GLsizei dataSize,
                                       const void* data);
static void GL_BINDING_CALL Mock_glCompressedTexImage3D(GLenum target,
                                                        GLint level,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        GLint border,
                                                        GLsizei imageSize,
                                                        const void* data);
static void GL_BINDING_CALL
Mock_glCompressedTexImage3DRobustANGLE(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLsizei imageSize,
                                       GLsizei dataSize,
                                       const void* data);
static void GL_BINDING_CALL Mock_glCompressedTexSubImage2D(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLenum format,
                                                           GLsizei imageSize,
                                                           const void* data);
static void GL_BINDING_CALL
Mock_glCompressedTexSubImage2DRobustANGLE(GLenum target,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLsizei imageSize,
                                          GLsizei dataSize,
                                          const void* data);
static void GL_BINDING_CALL Mock_glCompressedTexSubImage3D(GLenum target,
                                                           GLint level,
                                                           GLint xoffset,
                                                           GLint yoffset,
                                                           GLint zoffset,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLsizei depth,
                                                           GLenum format,
                                                           GLsizei imageSize,
                                                           const void* data);
static void GL_BINDING_CALL
Mock_glCompressedTexSubImage3DRobustANGLE(GLenum target,
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
                                          const void* data);
static void GL_BINDING_CALL Mock_glCopyBufferSubData(GLenum readTarget,
                                                     GLenum writeTarget,
                                                     GLintptr readOffset,
                                                     GLintptr writeOffset,
                                                     GLsizeiptr size);
static void GL_BINDING_CALL
Mock_glCopySubTextureCHROMIUM(GLuint sourceId,
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
                              GLboolean unpackUnmultiplyAlpha);
static void GL_BINDING_CALL Mock_glCopyTexImage2D(GLenum target,
                                                  GLint level,
                                                  GLenum internalformat,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLint border);
static void GL_BINDING_CALL Mock_glCopyTexSubImage2D(GLenum target,
                                                     GLint level,
                                                     GLint xoffset,
                                                     GLint yoffset,
                                                     GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height);
static void GL_BINDING_CALL Mock_glCopyTexSubImage3D(GLenum target,
                                                     GLint level,
                                                     GLint xoffset,
                                                     GLint yoffset,
                                                     GLint zoffset,
                                                     GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height);
static void GL_BINDING_CALL
Mock_glCopyTextureCHROMIUM(GLuint sourceId,
                           GLint sourceLevel,
                           GLenum destTarget,
                           GLuint destId,
                           GLint destLevel,
                           GLint internalFormat,
                           GLenum destType,
                           GLboolean unpackFlipY,
                           GLboolean unpackPremultiplyAlpha,
                           GLboolean unpackUnmultiplyAlpha);
static void GL_BINDING_CALL
Mock_glCreateMemoryObjectsEXT(GLsizei n, GLuint* memoryObjects);
static GLuint GL_BINDING_CALL Mock_glCreateProgram(void);
static GLuint GL_BINDING_CALL Mock_glCreateShader(GLenum type);
static GLuint GL_BINDING_CALL
Mock_glCreateShaderProgramv(GLenum type,
                            GLsizei count,
                            const char* const* strings);
static void GL_BINDING_CALL Mock_glCullFace(GLenum mode);
static void GL_BINDING_CALL Mock_glDebugMessageCallback(GLDEBUGPROC callback,
                                                        const void* userParam);
static void GL_BINDING_CALL
Mock_glDebugMessageCallbackKHR(GLDEBUGPROC callback, const void* userParam);
static void GL_BINDING_CALL Mock_glDebugMessageControl(GLenum source,
                                                       GLenum type,
                                                       GLenum severity,
                                                       GLsizei count,
                                                       const GLuint* ids,
                                                       GLboolean enabled);
static void GL_BINDING_CALL Mock_glDebugMessageControlKHR(GLenum source,
                                                          GLenum type,
                                                          GLenum severity,
                                                          GLsizei count,
                                                          const GLuint* ids,
                                                          GLboolean enabled);
static void GL_BINDING_CALL Mock_glDebugMessageInsert(GLenum source,
                                                      GLenum type,
                                                      GLuint id,
                                                      GLenum severity,
                                                      GLsizei length,
                                                      const char* buf);
static void GL_BINDING_CALL Mock_glDebugMessageInsertKHR(GLenum source,
                                                         GLenum type,
                                                         GLuint id,
                                                         GLenum severity,
                                                         GLsizei length,
                                                         const char* buf);
static void GL_BINDING_CALL Mock_glDeleteBuffers(GLsizei n,
                                                 const GLuint* buffers);
static void GL_BINDING_CALL Mock_glDeleteFencesNV(GLsizei n,
                                                  const GLuint* fences);
static void GL_BINDING_CALL
Mock_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
static void GL_BINDING_CALL
Mock_glDeleteMemoryObjectsEXT(GLsizei n, const GLuint* memoryObjects);
static void GL_BINDING_CALL Mock_glDeleteProgram(GLuint program);
static void GL_BINDING_CALL
Mock_glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines);
static void GL_BINDING_CALL Mock_glDeleteQueries(GLsizei n, const GLuint* ids);
static void GL_BINDING_CALL Mock_glDeleteQueriesEXT(GLsizei n,
                                                    const GLuint* ids);
static void GL_BINDING_CALL
Mock_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
static void GL_BINDING_CALL Mock_glDeleteSamplers(GLsizei n,
                                                  const GLuint* samplers);
static void GL_BINDING_CALL
Mock_glDeleteSemaphoresEXT(GLsizei n, const GLuint* semaphores);
static void GL_BINDING_CALL Mock_glDeleteShader(GLuint shader);
static void GL_BINDING_CALL Mock_glDeleteSync(GLsync sync);
static void GL_BINDING_CALL Mock_glDeleteTextures(GLsizei n,
                                                  const GLuint* textures);
static void GL_BINDING_CALL Mock_glDeleteTransformFeedbacks(GLsizei n,
                                                            const GLuint* ids);
static void GL_BINDING_CALL Mock_glDeleteVertexArrays(GLsizei n,
                                                      const GLuint* arrays);
static void GL_BINDING_CALL Mock_glDeleteVertexArraysOES(GLsizei n,
                                                         const GLuint* arrays);
static void GL_BINDING_CALL Mock_glDepthFunc(GLenum func);
static void GL_BINDING_CALL Mock_glDepthMask(GLboolean flag);
static void GL_BINDING_CALL Mock_glDepthRange(GLclampd zNear, GLclampd zFar);
static void GL_BINDING_CALL Mock_glDepthRangef(GLclampf zNear, GLclampf zFar);
static void GL_BINDING_CALL Mock_glDetachShader(GLuint program, GLuint shader);
static void GL_BINDING_CALL Mock_glDisable(GLenum cap);
static void GL_BINDING_CALL Mock_glDisableExtensionANGLE(const char* name);
static void GL_BINDING_CALL Mock_glDisableVertexAttribArray(GLuint index);
static void GL_BINDING_CALL Mock_glDisablei(GLenum target, GLuint index);
static void GL_BINDING_CALL Mock_glDisableiOES(GLenum target, GLuint index);
static void GL_BINDING_CALL
Mock_glDiscardFramebufferEXT(GLenum target,
                             GLsizei numAttachments,
                             const GLenum* attachments);
static void GL_BINDING_CALL Mock_glDispatchCompute(GLuint numGroupsX,
                                                   GLuint numGroupsY,
                                                   GLuint numGroupsZ);
static void GL_BINDING_CALL Mock_glDispatchComputeIndirect(GLintptr indirect);
static void GL_BINDING_CALL Mock_glDrawArrays(GLenum mode,
                                              GLint first,
                                              GLsizei count);
static void GL_BINDING_CALL Mock_glDrawArraysIndirect(GLenum mode,
                                                      const void* indirect);
static void GL_BINDING_CALL Mock_glDrawArraysInstanced(GLenum mode,
                                                       GLint first,
                                                       GLsizei count,
                                                       GLsizei primcount);
static void GL_BINDING_CALL Mock_glDrawArraysInstancedANGLE(GLenum mode,
                                                            GLint first,
                                                            GLsizei count,
                                                            GLsizei primcount);
static void GL_BINDING_CALL
Mock_glDrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount,
                                            GLuint baseinstance);
static void GL_BINDING_CALL
Mock_glDrawArraysInstancedBaseInstanceEXT(GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei primcount,
                                          GLuint baseinstance);
static void GL_BINDING_CALL Mock_glDrawBuffer(GLenum mode);
static void GL_BINDING_CALL Mock_glDrawBuffers(GLsizei n, const GLenum* bufs);
static void GL_BINDING_CALL Mock_glDrawBuffersEXT(GLsizei n,
                                                  const GLenum* bufs);
static void GL_BINDING_CALL Mock_glDrawElements(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices);
static void GL_BINDING_CALL Mock_glDrawElementsIndirect(GLenum mode,
                                                        GLenum type,
                                                        const void* indirect);
static void GL_BINDING_CALL Mock_glDrawElementsInstanced(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void* indices,
                                                         GLsizei primcount);
static void GL_BINDING_CALL
Mock_glDrawElementsInstancedANGLE(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices,
                                  GLsizei primcount);
static void GL_BINDING_CALL
Mock_glDrawElementsInstancedBaseVertexBaseInstanceANGLE(GLenum mode,
                                                        GLsizei count,
                                                        GLenum type,
                                                        const void* indices,
                                                        GLsizei primcount,
                                                        GLint baseVertex,
                                                        GLuint baseInstance);
static void GL_BINDING_CALL
Mock_glDrawElementsInstancedBaseVertexBaseInstanceEXT(GLenum mode,
                                                      GLsizei count,
                                                      GLenum type,
                                                      const void* indices,
                                                      GLsizei primcount,
                                                      GLint baseVertex,
                                                      GLuint baseInstance);
static void GL_BINDING_CALL Mock_glDrawRangeElements(GLenum mode,
                                                     GLuint start,
                                                     GLuint end,
                                                     GLsizei count,
                                                     GLenum type,
                                                     const void* indices);
static void GL_BINDING_CALL
Mock_glEGLImageTargetRenderbufferStorageOES(GLenum target, GLeglImageOES image);
static void GL_BINDING_CALL
Mock_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image);
static void GL_BINDING_CALL Mock_glEnable(GLenum cap);
static void GL_BINDING_CALL Mock_glEnableVertexAttribArray(GLuint index);
static void GL_BINDING_CALL Mock_glEnablei(GLenum target, GLuint index);
static void GL_BINDING_CALL Mock_glEnableiOES(GLenum target, GLuint index);
static void GL_BINDING_CALL
Mock_glEndPixelLocalStorageANGLE(GLsizei n, const GLenum* storeops);
static void GL_BINDING_CALL Mock_glEndQuery(GLenum target);
static void GL_BINDING_CALL Mock_glEndQueryEXT(GLenum target);
static void GL_BINDING_CALL Mock_glEndTilingQCOM(GLbitfield preserveMask);
static void GL_BINDING_CALL Mock_glEndTransformFeedback(void);
static GLsync GL_BINDING_CALL Mock_glFenceSync(GLenum condition,
                                               GLbitfield flags);
static void GL_BINDING_CALL Mock_glFinish(void);
static void GL_BINDING_CALL Mock_glFinishFenceNV(GLuint fence);
static void GL_BINDING_CALL Mock_glFlush(void);
static void GL_BINDING_CALL Mock_glFlushMappedBufferRange(GLenum target,
                                                          GLintptr offset,
                                                          GLsizeiptr length);
static void GL_BINDING_CALL Mock_glFlushMappedBufferRangeEXT(GLenum target,
                                                             GLintptr offset,
                                                             GLsizeiptr length);
static void GL_BINDING_CALL
Mock_glFramebufferMemorylessPixelLocalStorageANGLE(GLint plane,
                                                   GLenum internalformat);
static void GL_BINDING_CALL Mock_glFramebufferParameteri(GLenum target,
                                                         GLenum pname,
                                                         GLint param);
static void GL_BINDING_CALL Mock_glFramebufferParameteriMESA(GLenum target,
                                                             GLenum pname,
                                                             GLint param);
static void GL_BINDING_CALL
Mock_glFramebufferPixelLocalClearValuefvANGLE(GLint plane,
                                              const GLfloat* value);
static void GL_BINDING_CALL
Mock_glFramebufferPixelLocalClearValueivANGLE(GLint plane, const GLint* value);
static void GL_BINDING_CALL
Mock_glFramebufferPixelLocalClearValueuivANGLE(GLint plane,
                                               const GLuint* value);
static void GL_BINDING_CALL Mock_glFramebufferPixelLocalStorageInterruptANGLE();
static void GL_BINDING_CALL Mock_glFramebufferPixelLocalStorageRestoreANGLE();
static void GL_BINDING_CALL
Mock_glFramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer);
static void GL_BINDING_CALL Mock_glFramebufferTexture2D(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level);
static void GL_BINDING_CALL
Mock_glFramebufferTexture2DMultisampleEXT(GLenum target,
                                          GLenum attachment,
                                          GLenum textarget,
                                          GLuint texture,
                                          GLint level,
                                          GLsizei samples);
static void GL_BINDING_CALL
Mock_glFramebufferTexture2DMultisampleIMG(GLenum target,
                                          GLenum attachment,
                                          GLenum textarget,
                                          GLuint texture,
                                          GLint level,
                                          GLsizei samples);
static void GL_BINDING_CALL Mock_glFramebufferTextureLayer(GLenum target,
                                                           GLenum attachment,
                                                           GLuint texture,
                                                           GLint level,
                                                           GLint layer);
static void GL_BINDING_CALL
Mock_glFramebufferTextureMultiviewOVR(GLenum target,
                                      GLenum attachment,
                                      GLuint texture,
                                      GLint level,
                                      GLint baseViewIndex,
                                      GLsizei numViews);
static void GL_BINDING_CALL
Mock_glFramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                                GLuint backingtexture,
                                                GLint level,
                                                GLint layer);
static void GL_BINDING_CALL Mock_glFrontFace(GLenum mode);
static void GL_BINDING_CALL Mock_glGenBuffers(GLsizei n, GLuint* buffers);
static void GL_BINDING_CALL Mock_glGenFencesNV(GLsizei n, GLuint* fences);
static void GL_BINDING_CALL Mock_glGenFramebuffers(GLsizei n,
                                                   GLuint* framebuffers);
static GLuint GL_BINDING_CALL Mock_glGenProgramPipelines(GLsizei n,
                                                         GLuint* pipelines);
static void GL_BINDING_CALL Mock_glGenQueries(GLsizei n, GLuint* ids);
static void GL_BINDING_CALL Mock_glGenQueriesEXT(GLsizei n, GLuint* ids);
static void GL_BINDING_CALL Mock_glGenRenderbuffers(GLsizei n,
                                                    GLuint* renderbuffers);
static void GL_BINDING_CALL Mock_glGenSamplers(GLsizei n, GLuint* samplers);
static void GL_BINDING_CALL Mock_glGenSemaphoresEXT(GLsizei n,
                                                    GLuint* semaphores);
static void GL_BINDING_CALL Mock_glGenTextures(GLsizei n, GLuint* textures);
static void GL_BINDING_CALL Mock_glGenTransformFeedbacks(GLsizei n,
                                                         GLuint* ids);
static void GL_BINDING_CALL Mock_glGenVertexArrays(GLsizei n, GLuint* arrays);
static void GL_BINDING_CALL Mock_glGenVertexArraysOES(GLsizei n,
                                                      GLuint* arrays);
static void GL_BINDING_CALL Mock_glGenerateMipmap(GLenum target);
static void GL_BINDING_CALL Mock_glGetActiveAttrib(GLuint program,
                                                   GLuint index,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   GLint* size,
                                                   GLenum* type,
                                                   char* name);
static void GL_BINDING_CALL Mock_glGetActiveUniform(GLuint program,
                                                    GLuint index,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    GLint* size,
                                                    GLenum* type,
                                                    char* name);
static void GL_BINDING_CALL
Mock_glGetActiveUniformBlockName(GLuint program,
                                 GLuint uniformBlockIndex,
                                 GLsizei bufSize,
                                 GLsizei* length,
                                 char* uniformBlockName);
static void GL_BINDING_CALL
Mock_glGetActiveUniformBlockiv(GLuint program,
                               GLuint uniformBlockIndex,
                               GLenum pname,
                               GLint* params);
static void GL_BINDING_CALL
Mock_glGetActiveUniformBlockivRobustANGLE(GLuint program,
                                          GLuint uniformBlockIndex,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params);
static void GL_BINDING_CALL
Mock_glGetActiveUniformsiv(GLuint program,
                           GLsizei uniformCount,
                           const GLuint* uniformIndices,
                           GLenum pname,
                           GLint* params);
static void GL_BINDING_CALL Mock_glGetAttachedShaders(GLuint program,
                                                      GLsizei maxcount,
                                                      GLsizei* count,
                                                      GLuint* shaders);
static GLint GL_BINDING_CALL Mock_glGetAttribLocation(GLuint program,
                                                      const char* name);
static void GL_BINDING_CALL Mock_glGetBooleani_v(GLenum target,
                                                 GLuint index,
                                                 GLboolean* data);
static void GL_BINDING_CALL Mock_glGetBooleani_vRobustANGLE(GLenum target,
                                                            GLuint index,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLboolean* data);
static void GL_BINDING_CALL Mock_glGetBooleanv(GLenum pname, GLboolean* params);
static void GL_BINDING_CALL Mock_glGetBooleanvRobustANGLE(GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLboolean* data);
static void GL_BINDING_CALL
Mock_glGetBufferParameteri64vRobustANGLE(GLenum target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint64* params);
static void GL_BINDING_CALL Mock_glGetBufferParameteriv(GLenum target,
                                                        GLenum pname,
                                                        GLint* params);
static void GL_BINDING_CALL
Mock_glGetBufferParameterivRobustANGLE(GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint* params);
static void GL_BINDING_CALL Mock_glGetBufferPointervRobustANGLE(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                void** params);
static GLuint GL_BINDING_CALL Mock_glGetDebugMessageLog(GLuint count,
                                                        GLsizei bufSize,
                                                        GLenum* sources,
                                                        GLenum* types,
                                                        GLuint* ids,
                                                        GLenum* severities,
                                                        GLsizei* lengths,
                                                        char* messageLog);
static GLuint GL_BINDING_CALL Mock_glGetDebugMessageLogKHR(GLuint count,
                                                           GLsizei bufSize,
                                                           GLenum* sources,
                                                           GLenum* types,
                                                           GLuint* ids,
                                                           GLenum* severities,
                                                           GLsizei* lengths,
                                                           char* messageLog);
static GLenum GL_BINDING_CALL Mock_glGetError(void);
static void GL_BINDING_CALL Mock_glGetFenceivNV(GLuint fence,
                                                GLenum pname,
                                                GLint* params);
static void GL_BINDING_CALL Mock_glGetFloatv(GLenum pname, GLfloat* params);
static void GL_BINDING_CALL Mock_glGetFloatvRobustANGLE(GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLfloat* data);
static GLint GL_BINDING_CALL Mock_glGetFragDataIndexEXT(GLuint program,
                                                        const char* name);
static GLint GL_BINDING_CALL Mock_glGetFragDataLocation(GLuint program,
                                                        const char* name);
static void GL_BINDING_CALL
Mock_glGetFramebufferAttachmentParameteriv(GLenum target,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferAttachmentParameterivRobustANGLE(GLenum target,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params);
static void GL_BINDING_CALL Mock_glGetFramebufferParameteriv(GLenum target,
                                                             GLenum pname,
                                                             GLint* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferParameterivRobustANGLE(GLenum target,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferPixelLocalStorageParameterfvANGLE(GLint plane,
                                                       GLenum pname,
                                                       GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferPixelLocalStorageParameterfvRobustANGLE(GLint plane,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                       GLenum pname,
                                                       GLint* params);
static void GL_BINDING_CALL
Mock_glGetFramebufferPixelLocalStorageParameterivRobustANGLE(GLint plane,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params);
static GLenum GL_BINDING_CALL Mock_glGetGraphicsResetStatus(void);
static GLenum GL_BINDING_CALL Mock_glGetGraphicsResetStatusEXT(void);
static GLenum GL_BINDING_CALL Mock_glGetGraphicsResetStatusKHR(void);
static void GL_BINDING_CALL Mock_glGetInteger64i_v(GLenum target,
                                                   GLuint index,
                                                   GLint64* data);
static void GL_BINDING_CALL Mock_glGetInteger64i_vRobustANGLE(GLenum target,
                                                              GLuint index,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint64* data);
static void GL_BINDING_CALL Mock_glGetInteger64v(GLenum pname, GLint64* params);
static void GL_BINDING_CALL Mock_glGetInteger64vRobustANGLE(GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint64* data);
static void GL_BINDING_CALL Mock_glGetIntegeri_v(GLenum target,
                                                 GLuint index,
                                                 GLint* data);
static void GL_BINDING_CALL Mock_glGetIntegeri_vRobustANGLE(GLenum target,
                                                            GLuint index,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* data);
static void GL_BINDING_CALL Mock_glGetIntegerv(GLenum pname, GLint* params);
static void GL_BINDING_CALL Mock_glGetIntegervRobustANGLE(GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* data);
static void GL_BINDING_CALL
Mock_glGetInternalformatSampleivNV(GLenum target,
                                   GLenum internalformat,
                                   GLsizei samples,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLint* params);
static void GL_BINDING_CALL Mock_glGetInternalformativ(GLenum target,
                                                       GLenum internalformat,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLint* params);
static void GL_BINDING_CALL
Mock_glGetInternalformativRobustANGLE(GLenum target,
                                      GLenum internalformat,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint* params);
static void GL_BINDING_CALL Mock_glGetMultisamplefv(GLenum pname,
                                                    GLuint index,
                                                    GLfloat* val);
static void GL_BINDING_CALL Mock_glGetMultisamplefvRobustANGLE(GLenum pname,
                                                               GLuint index,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLfloat* val);
static void GL_BINDING_CALL Mock_glGetObjectLabel(GLenum identifier,
                                                  GLuint name,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  char* label);
static void GL_BINDING_CALL Mock_glGetObjectLabelKHR(GLenum identifier,
                                                     GLuint name,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     char* label);
static void GL_BINDING_CALL Mock_glGetObjectPtrLabel(void* ptr,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     char* label);
static void GL_BINDING_CALL Mock_glGetObjectPtrLabelKHR(void* ptr,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        char* label);
static void GL_BINDING_CALL Mock_glGetPointerv(GLenum pname, void** params);
static void GL_BINDING_CALL Mock_glGetPointervKHR(GLenum pname, void** params);
static void GL_BINDING_CALL
Mock_glGetPointervRobustANGLERobustANGLE(GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         void** params);
static void GL_BINDING_CALL Mock_glGetProgramBinary(GLuint program,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLenum* binaryFormat,
                                                    GLvoid* binary);
static void GL_BINDING_CALL Mock_glGetProgramBinaryOES(GLuint program,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLenum* binaryFormat,
                                                       GLvoid* binary);
static void GL_BINDING_CALL Mock_glGetProgramInfoLog(GLuint program,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     char* infolog);
static void GL_BINDING_CALL
Mock_glGetProgramInterfaceiv(GLuint program,
                             GLenum programInterface,
                             GLenum pname,
                             GLint* params);
static void GL_BINDING_CALL
Mock_glGetProgramInterfaceivRobustANGLE(GLuint program,
                                        GLenum programInterface,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLint* params);
static void GL_BINDING_CALL Mock_glGetProgramPipelineInfoLog(GLuint pipeline,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLchar* infoLog);
static void GL_BINDING_CALL Mock_glGetProgramPipelineiv(GLuint pipeline,
                                                        GLenum pname,
                                                        GLint* params);
static GLuint GL_BINDING_CALL
Mock_glGetProgramResourceIndex(GLuint program,
                               GLenum programInterface,
                               const GLchar* name);
static GLint GL_BINDING_CALL
Mock_glGetProgramResourceLocation(GLuint program,
                                  GLenum programInterface,
                                  const char* name);
static void GL_BINDING_CALL
Mock_glGetProgramResourceName(GLuint program,
                              GLenum programInterface,
                              GLuint index,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLchar* name);
static void GL_BINDING_CALL Mock_glGetProgramResourceiv(GLuint program,
                                                        GLenum programInterface,
                                                        GLuint index,
                                                        GLsizei propCount,
                                                        const GLenum* props,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params);
static void GL_BINDING_CALL Mock_glGetProgramiv(GLuint program,
                                                GLenum pname,
                                                GLint* params);
static void GL_BINDING_CALL Mock_glGetProgramivRobustANGLE(GLuint program,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params);
static void GL_BINDING_CALL Mock_glGetQueryObjecti64vEXT(GLuint id,
                                                         GLenum pname,
                                                         GLint64* params);
static void GL_BINDING_CALL
Mock_glGetQueryObjecti64vRobustANGLE(GLuint id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint64* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectivEXT(GLuint id,
                                                       GLenum pname,
                                                       GLint* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectivRobustANGLE(GLuint id,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLint* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectui64vEXT(GLuint id,
                                                          GLenum pname,
                                                          GLuint64* params);
static void GL_BINDING_CALL
Mock_glGetQueryObjectui64vRobustANGLE(GLuint id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLuint64* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectuiv(GLuint id,
                                                     GLenum pname,
                                                     GLuint* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectuivEXT(GLuint id,
                                                        GLenum pname,
                                                        GLuint* params);
static void GL_BINDING_CALL Mock_glGetQueryObjectuivRobustANGLE(GLuint id,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLuint* params);
static void GL_BINDING_CALL Mock_glGetQueryiv(GLenum target,
                                              GLenum pname,
                                              GLint* params);
static void GL_BINDING_CALL Mock_glGetQueryivEXT(GLenum target,
                                                 GLenum pname,
                                                 GLint* params);
static void GL_BINDING_CALL Mock_glGetQueryivRobustANGLE(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params);
static void GL_BINDING_CALL Mock_glGetRenderbufferParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLint* params);
static void GL_BINDING_CALL
Mock_glGetRenderbufferParameterivRobustANGLE(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params);
static void GL_BINDING_CALL
Mock_glGetSamplerParameterIivRobustANGLE(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params);
static void GL_BINDING_CALL
Mock_glGetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLuint* params);
static void GL_BINDING_CALL Mock_glGetSamplerParameterfv(GLuint sampler,
                                                         GLenum pname,
                                                         GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetSamplerParameterfvRobustANGLE(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLfloat* params);
static void GL_BINDING_CALL Mock_glGetSamplerParameteriv(GLuint sampler,
                                                         GLenum pname,
                                                         GLint* params);
static void GL_BINDING_CALL
Mock_glGetSamplerParameterivRobustANGLE(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLint* params);
static void GL_BINDING_CALL Mock_glGetShaderInfoLog(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* infolog);
static void GL_BINDING_CALL
Mock_glGetShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision);
static void GL_BINDING_CALL Mock_glGetShaderSource(GLuint shader,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   char* source);
static void GL_BINDING_CALL Mock_glGetShaderiv(GLuint shader,
                                               GLenum pname,
                                               GLint* params);
static void GL_BINDING_CALL Mock_glGetShaderivRobustANGLE(GLuint shader,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params);
static const GLubyte* GL_BINDING_CALL Mock_glGetString(GLenum name);
static const GLubyte* GL_BINDING_CALL Mock_glGetStringi(GLenum name,
                                                        GLuint index);
static void GL_BINDING_CALL Mock_glGetSynciv(GLsync sync,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* values);
static void GL_BINDING_CALL Mock_glGetTexLevelParameterfv(GLenum target,
                                                          GLint level,
                                                          GLenum pname,
                                                          GLfloat* params);
static void GL_BINDING_CALL Mock_glGetTexLevelParameterfvANGLE(GLenum target,
                                                               GLint level,
                                                               GLenum pname,
                                                               GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetTexLevelParameterfvRobustANGLE(GLenum target,
                                         GLint level,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLfloat* params);
static void GL_BINDING_CALL Mock_glGetTexLevelParameteriv(GLenum target,
                                                          GLint level,
                                                          GLenum pname,
                                                          GLint* params);
static void GL_BINDING_CALL Mock_glGetTexLevelParameterivANGLE(GLenum target,
                                                               GLint level,
                                                               GLenum pname,
                                                               GLint* params);
static void GL_BINDING_CALL
Mock_glGetTexLevelParameterivRobustANGLE(GLenum target,
                                         GLint level,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params);
static void GL_BINDING_CALL
Mock_glGetTexParameterIivRobustANGLE(GLenum target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint* params);
static void GL_BINDING_CALL
Mock_glGetTexParameterIuivRobustANGLE(GLenum target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLuint* params);
static void GL_BINDING_CALL Mock_glGetTexParameterfv(GLenum target,
                                                     GLenum pname,
                                                     GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetTexParameterfvRobustANGLE(GLenum target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLfloat* params);
static void GL_BINDING_CALL Mock_glGetTexParameteriv(GLenum target,
                                                     GLenum pname,
                                                     GLint* params);
static void GL_BINDING_CALL Mock_glGetTexParameterivRobustANGLE(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params);
static void GL_BINDING_CALL Mock_glGetTransformFeedbackVarying(GLuint program,
                                                               GLuint index,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLsizei* size,
                                                               GLenum* type,
                                                               char* name);
static void GL_BINDING_CALL
Mock_glGetTranslatedShaderSourceANGLE(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* source);
static GLuint GL_BINDING_CALL
Mock_glGetUniformBlockIndex(GLuint program, const char* uniformBlockName);
static void GL_BINDING_CALL
Mock_glGetUniformIndices(GLuint program,
                         GLsizei uniformCount,
                         const char* const* uniformNames,
                         GLuint* uniformIndices);
static GLint GL_BINDING_CALL Mock_glGetUniformLocation(GLuint program,
                                                       const char* name);
static void GL_BINDING_CALL Mock_glGetUniformfv(GLuint program,
                                                GLint location,
                                                GLfloat* params);
static void GL_BINDING_CALL Mock_glGetUniformfvRobustANGLE(GLuint program,
                                                           GLint location,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLfloat* params);
static void GL_BINDING_CALL Mock_glGetUniformiv(GLuint program,
                                                GLint location,
                                                GLint* params);
static void GL_BINDING_CALL Mock_glGetUniformivRobustANGLE(GLuint program,
                                                           GLint location,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params);
static void GL_BINDING_CALL Mock_glGetUniformuiv(GLuint program,
                                                 GLint location,
                                                 GLuint* params);
static void GL_BINDING_CALL Mock_glGetUniformuivRobustANGLE(GLuint program,
                                                            GLint location,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLuint* params);
static void GL_BINDING_CALL
Mock_glGetVertexAttribIivRobustANGLE(GLuint index,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint* params);
static void GL_BINDING_CALL
Mock_glGetVertexAttribIuivRobustANGLE(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLuint* params);
static void GL_BINDING_CALL Mock_glGetVertexAttribPointerv(GLuint index,
                                                           GLenum pname,
                                                           void** pointer);
static void GL_BINDING_CALL
Mock_glGetVertexAttribPointervRobustANGLE(GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          void** pointer);
static void GL_BINDING_CALL Mock_glGetVertexAttribfv(GLuint index,
                                                     GLenum pname,
                                                     GLfloat* params);
static void GL_BINDING_CALL
Mock_glGetVertexAttribfvRobustANGLE(GLuint index,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLfloat* params);
static void GL_BINDING_CALL Mock_glGetVertexAttribiv(GLuint index,
                                                     GLenum pname,
                                                     GLint* params);
static void GL_BINDING_CALL Mock_glGetVertexAttribivRobustANGLE(GLuint index,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params);
static void GL_BINDING_CALL Mock_glGetnUniformfvRobustANGLE(GLuint program,
                                                            GLint location,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLfloat* params);
static void GL_BINDING_CALL Mock_glGetnUniformivRobustANGLE(GLuint program,
                                                            GLint location,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* params);
static void GL_BINDING_CALL Mock_glGetnUniformuivRobustANGLE(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLuint* params);
static void GL_BINDING_CALL Mock_glHint(GLenum target, GLenum mode);
static void GL_BINDING_CALL Mock_glImportMemoryFdEXT(GLuint memory,
                                                     GLuint64 size,
                                                     GLenum handleType,
                                                     GLint fd);
static void GL_BINDING_CALL Mock_glImportMemoryWin32HandleEXT(GLuint memory,
                                                              GLuint64 size,
                                                              GLenum handleType,
                                                              void* handle);
static void GL_BINDING_CALL
Mock_glImportMemoryZirconHandleANGLE(GLuint memory,
                                     GLuint64 size,
                                     GLenum handleType,
                                     GLuint handle);
static void GL_BINDING_CALL Mock_glImportSemaphoreFdEXT(GLuint semaphore,
                                                        GLenum handleType,
                                                        GLint fd);
static void GL_BINDING_CALL
Mock_glImportSemaphoreWin32HandleEXT(GLuint semaphore,
                                     GLenum handleType,
                                     void* handle);
static void GL_BINDING_CALL
Mock_glImportSemaphoreZirconHandleANGLE(GLuint semaphore,
                                        GLenum handleType,
                                        GLuint handle);
static void GL_BINDING_CALL Mock_glInsertEventMarkerEXT(GLsizei length,
                                                        const char* marker);
static void GL_BINDING_CALL
Mock_glInvalidateFramebuffer(GLenum target,
                             GLsizei numAttachments,
                             const GLenum* attachments);
static void GL_BINDING_CALL
Mock_glInvalidateSubFramebuffer(GLenum target,
                                GLsizei numAttachments,
                                const GLenum* attachments,
                                GLint x,
                                GLint y,
                                GLint width,
                                GLint height);
static void GL_BINDING_CALL Mock_glInvalidateTextureANGLE(GLenum target);
static GLboolean GL_BINDING_CALL Mock_glIsBuffer(GLuint buffer);
static GLboolean GL_BINDING_CALL Mock_glIsEnabled(GLenum cap);
static GLboolean GL_BINDING_CALL Mock_glIsEnabledi(GLenum target, GLuint index);
static GLboolean GL_BINDING_CALL Mock_glIsEnablediOES(GLenum target,
                                                      GLuint index);
static GLboolean GL_BINDING_CALL Mock_glIsFenceNV(GLuint fence);
static GLboolean GL_BINDING_CALL Mock_glIsFramebuffer(GLuint framebuffer);
static GLboolean GL_BINDING_CALL Mock_glIsProgram(GLuint program);
static GLboolean GL_BINDING_CALL Mock_glIsProgramPipeline(GLuint pipeline);
static GLboolean GL_BINDING_CALL Mock_glIsQuery(GLuint query);
static GLboolean GL_BINDING_CALL Mock_glIsQueryEXT(GLuint query);
static GLboolean GL_BINDING_CALL Mock_glIsRenderbuffer(GLuint renderbuffer);
static GLboolean GL_BINDING_CALL Mock_glIsSampler(GLuint sampler);
static GLboolean GL_BINDING_CALL Mock_glIsShader(GLuint shader);
static GLboolean GL_BINDING_CALL Mock_glIsSync(GLsync sync);
static GLboolean GL_BINDING_CALL Mock_glIsTexture(GLuint texture);
static GLboolean GL_BINDING_CALL Mock_glIsTransformFeedback(GLuint id);
static GLboolean GL_BINDING_CALL Mock_glIsVertexArray(GLuint array);
static GLboolean GL_BINDING_CALL Mock_glIsVertexArrayOES(GLuint array);
static void GL_BINDING_CALL Mock_glLineWidth(GLfloat width);
static void GL_BINDING_CALL Mock_glLinkProgram(GLuint program);
static void* GL_BINDING_CALL Mock_glMapBufferOES(GLenum target, GLenum access);
static void* GL_BINDING_CALL Mock_glMapBufferRange(GLenum target,
                                                   GLintptr offset,
                                                   GLsizeiptr length,
                                                   GLbitfield access);
static void* GL_BINDING_CALL Mock_glMapBufferRangeEXT(GLenum target,
                                                      GLintptr offset,
                                                      GLsizeiptr length,
                                                      GLbitfield access);
static void GL_BINDING_CALL Mock_glMaxShaderCompilerThreadsKHR(GLuint count);
static void GL_BINDING_CALL Mock_glMemoryBarrier(GLbitfield barriers);
static void GL_BINDING_CALL Mock_glMemoryBarrierByRegion(GLbitfield barriers);
static void GL_BINDING_CALL Mock_glMemoryBarrierEXT(GLbitfield barriers);
static void GL_BINDING_CALL
Mock_glMemoryObjectParameterivEXT(GLuint memoryObject,
                                  GLenum pname,
                                  const GLint* param);
static void GL_BINDING_CALL Mock_glMinSampleShading(GLfloat value);
static void GL_BINDING_CALL Mock_glMultiDrawArraysANGLE(GLenum mode,
                                                        const GLint* firsts,
                                                        const GLsizei* counts,
                                                        GLsizei drawcount);
static void GL_BINDING_CALL
Mock_glMultiDrawArraysInstancedANGLE(GLenum mode,
                                     const GLint* firsts,
                                     const GLsizei* counts,
                                     const GLsizei* instanceCounts,
                                     GLsizei drawcount);
static void GL_BINDING_CALL
Mock_glMultiDrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                                 const GLint* firsts,
                                                 const GLsizei* counts,
                                                 const GLsizei* instanceCounts,
                                                 const GLuint* baseInstances,
                                                 GLsizei drawcount);
static void GL_BINDING_CALL
Mock_glMultiDrawElementsANGLE(GLenum mode,
                              const GLsizei* counts,
                              GLenum type,
                              const GLvoid* const* indices,
                              GLsizei drawcount);
static void GL_BINDING_CALL
Mock_glMultiDrawElementsInstancedANGLE(GLenum mode,
                                       const GLsizei* counts,
                                       GLenum type,
                                       const GLvoid* const* indices,
                                       const GLsizei* instanceCounts,
                                       GLsizei drawcount);
static void GL_BINDING_CALL
Mock_glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    const GLint* baseVertices,
    const GLuint* baseInstances,
    GLsizei drawcount);
static void GL_BINDING_CALL Mock_glObjectLabel(GLenum identifier,
                                               GLuint name,
                                               GLsizei length,
                                               const char* label);
static void GL_BINDING_CALL Mock_glObjectLabelKHR(GLenum identifier,
                                                  GLuint name,
                                                  GLsizei length,
                                                  const char* label);
static void GL_BINDING_CALL Mock_glObjectPtrLabel(void* ptr,
                                                  GLsizei length,
                                                  const char* label);
static void GL_BINDING_CALL Mock_glObjectPtrLabelKHR(void* ptr,
                                                     GLsizei length,
                                                     const char* label);
static void GL_BINDING_CALL Mock_glPatchParameteri(GLenum pname, GLint value);
static void GL_BINDING_CALL Mock_glPatchParameteriOES(GLenum pname,
                                                      GLint value);
static void GL_BINDING_CALL Mock_glPauseTransformFeedback(void);
static void GL_BINDING_CALL Mock_glPixelLocalStorageBarrierANGLE();
static void GL_BINDING_CALL Mock_glPixelStorei(GLenum pname, GLint param);
static void GL_BINDING_CALL Mock_glPointParameteri(GLenum pname, GLint param);
static void GL_BINDING_CALL Mock_glPolygonMode(GLenum face, GLenum mode);
static void GL_BINDING_CALL Mock_glPolygonModeANGLE(GLenum face, GLenum mode);
static void GL_BINDING_CALL Mock_glPolygonOffset(GLfloat factor, GLfloat units);
static void GL_BINDING_CALL Mock_glPolygonOffsetClampEXT(GLfloat factor,
                                                         GLfloat units,
                                                         GLfloat clamp);
static void GL_BINDING_CALL Mock_glPopDebugGroup();
static void GL_BINDING_CALL Mock_glPopDebugGroupKHR();
static void GL_BINDING_CALL Mock_glPopGroupMarkerEXT(void);
static void GL_BINDING_CALL Mock_glPrimitiveRestartIndex(GLuint index);
static void GL_BINDING_CALL Mock_glProgramBinary(GLuint program,
                                                 GLenum binaryFormat,
                                                 const GLvoid* binary,
                                                 GLsizei length);
static void GL_BINDING_CALL Mock_glProgramBinaryOES(GLuint program,
                                                    GLenum binaryFormat,
                                                    const GLvoid* binary,
                                                    GLsizei length);
static void GL_BINDING_CALL Mock_glProgramParameteri(GLuint program,
                                                     GLenum pname,
                                                     GLint value);
static void GL_BINDING_CALL Mock_glProgramUniform1f(GLuint program,
                                                    GLint location,
                                                    GLfloat v0);
static void GL_BINDING_CALL Mock_glProgramUniform1fv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLfloat* value);
static void GL_BINDING_CALL Mock_glProgramUniform1i(GLuint program,
                                                    GLint location,
                                                    GLint v0);
static void GL_BINDING_CALL Mock_glProgramUniform1iv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLint* value);
static void GL_BINDING_CALL Mock_glProgramUniform1ui(GLuint program,
                                                     GLint location,
                                                     GLuint v0);
static void GL_BINDING_CALL Mock_glProgramUniform1uiv(GLuint program,
                                                      GLint location,
                                                      GLsizei count,
                                                      const GLuint* value);
static void GL_BINDING_CALL Mock_glProgramUniform2f(GLuint program,
                                                    GLint location,
                                                    GLfloat v0,
                                                    GLfloat v1);
static void GL_BINDING_CALL Mock_glProgramUniform2fv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLfloat* value);
static void GL_BINDING_CALL Mock_glProgramUniform2i(GLuint program,
                                                    GLint location,
                                                    GLint v0,
                                                    GLint v1);
static void GL_BINDING_CALL Mock_glProgramUniform2iv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLint* value);
static void GL_BINDING_CALL Mock_glProgramUniform2ui(GLuint program,
                                                     GLint location,
                                                     GLuint v0,
                                                     GLuint v1);
static void GL_BINDING_CALL Mock_glProgramUniform2uiv(GLuint program,
                                                      GLint location,
                                                      GLsizei count,
                                                      const GLuint* value);
static void GL_BINDING_CALL Mock_glProgramUniform3f(GLuint program,
                                                    GLint location,
                                                    GLfloat v0,
                                                    GLfloat v1,
                                                    GLfloat v2);
static void GL_BINDING_CALL Mock_glProgramUniform3fv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLfloat* value);
static void GL_BINDING_CALL Mock_glProgramUniform3i(GLuint program,
                                                    GLint location,
                                                    GLint v0,
                                                    GLint v1,
                                                    GLint v2);
static void GL_BINDING_CALL Mock_glProgramUniform3iv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLint* value);
static void GL_BINDING_CALL Mock_glProgramUniform3ui(GLuint program,
                                                     GLint location,
                                                     GLuint v0,
                                                     GLuint v1,
                                                     GLuint v2);
static void GL_BINDING_CALL Mock_glProgramUniform3uiv(GLuint program,
                                                      GLint location,
                                                      GLsizei count,
                                                      const GLuint* value);
static void GL_BINDING_CALL Mock_glProgramUniform4f(GLuint program,
                                                    GLint location,
                                                    GLfloat v0,
                                                    GLfloat v1,
                                                    GLfloat v2,
                                                    GLfloat v3);
static void GL_BINDING_CALL Mock_glProgramUniform4fv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLfloat* value);
static void GL_BINDING_CALL Mock_glProgramUniform4i(GLuint program,
                                                    GLint location,
                                                    GLint v0,
                                                    GLint v1,
                                                    GLint v2,
                                                    GLint v3);
static void GL_BINDING_CALL Mock_glProgramUniform4iv(GLuint program,
                                                     GLint location,
                                                     GLsizei count,
                                                     const GLint* value);
static void GL_BINDING_CALL Mock_glProgramUniform4ui(GLuint program,
                                                     GLint location,
                                                     GLuint v0,
                                                     GLuint v1,
                                                     GLuint v2,
                                                     GLuint v3);
static void GL_BINDING_CALL Mock_glProgramUniform4uiv(GLuint program,
                                                      GLint location,
                                                      GLsizei count,
                                                      const GLuint* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix2fv(GLuint program,
                               GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix2x3fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix2x4fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix3fv(GLuint program,
                               GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix3x2fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix3x4fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix4fv(GLuint program,
                               GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix4x2fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL
Mock_glProgramUniformMatrix4x3fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value);
static void GL_BINDING_CALL Mock_glProvokingVertexANGLE(GLenum provokeMode);
static void GL_BINDING_CALL Mock_glPushDebugGroup(GLenum source,
                                                  GLuint id,
                                                  GLsizei length,
                                                  const char* message);
static void GL_BINDING_CALL Mock_glPushDebugGroupKHR(GLenum source,
                                                     GLuint id,
                                                     GLsizei length,
                                                     const char* message);
static void GL_BINDING_CALL Mock_glPushGroupMarkerEXT(GLsizei length,
                                                      const char* marker);
static void GL_BINDING_CALL Mock_glQueryCounterEXT(GLuint id, GLenum target);
static void GL_BINDING_CALL Mock_glReadBuffer(GLenum src);
static void GL_BINDING_CALL Mock_glReadPixels(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              void* pixels);
static void GL_BINDING_CALL Mock_glReadPixelsRobustANGLE(GLint x,
                                                         GLint y,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLenum format,
                                                         GLenum type,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLsizei* columns,
                                                         GLsizei* rows,
                                                         void* pixels);
static void GL_BINDING_CALL Mock_glReadnPixelsRobustANGLE(GLint x,
                                                          GLint y,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLsizei* columns,
                                                          GLsizei* rows,
                                                          void* data);
static void GL_BINDING_CALL Mock_glReleaseShaderCompiler(void);
static void GL_BINDING_CALL Mock_glReleaseTexturesANGLE(GLuint numTextures,
                                                        const GLuint* textures,
                                                        GLenum* layouts);
static void GL_BINDING_CALL Mock_glRenderbufferStorage(GLenum target,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height);
static void GL_BINDING_CALL
Mock_glRenderbufferStorageMultisample(GLenum target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height);
static void GL_BINDING_CALL
Mock_glRenderbufferStorageMultisampleANGLE(GLenum target,
                                           GLsizei samples,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height);
static void GL_BINDING_CALL
Mock_glRenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                                 GLsizei samples,
                                                 GLsizei storageSamples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height);
static void GL_BINDING_CALL
Mock_glRenderbufferStorageMultisampleEXT(GLenum target,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height);
static void GL_BINDING_CALL
Mock_glRenderbufferStorageMultisampleIMG(GLenum target,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height);
static void GL_BINDING_CALL Mock_glRequestExtensionANGLE(const char* name);
static void GL_BINDING_CALL Mock_glResumeTransformFeedback(void);
static void GL_BINDING_CALL Mock_glSampleCoverage(GLclampf value,
                                                  GLboolean invert);
static void GL_BINDING_CALL Mock_glSampleMaski(GLuint maskNumber,
                                               GLbitfield mask);
static void GL_BINDING_CALL
Mock_glSamplerParameterIivRobustANGLE(GLuint sampler,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      const GLint* param);
static void GL_BINDING_CALL
Mock_glSamplerParameterIuivRobustANGLE(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLuint* param);
static void GL_BINDING_CALL Mock_glSamplerParameterf(GLuint sampler,
                                                     GLenum pname,
                                                     GLfloat param);
static void GL_BINDING_CALL Mock_glSamplerParameterfv(GLuint sampler,
                                                      GLenum pname,
                                                      const GLfloat* params);
static void GL_BINDING_CALL
Mock_glSamplerParameterfvRobustANGLE(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     const GLfloat* param);
static void GL_BINDING_CALL Mock_glSamplerParameteri(GLuint sampler,
                                                     GLenum pname,
                                                     GLint param);
static void GL_BINDING_CALL Mock_glSamplerParameteriv(GLuint sampler,
                                                      GLenum pname,
                                                      const GLint* params);
static void GL_BINDING_CALL
Mock_glSamplerParameterivRobustANGLE(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     const GLint* param);
static void GL_BINDING_CALL Mock_glScissor(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height);
static void GL_BINDING_CALL Mock_glSetFenceNV(GLuint fence, GLenum condition);
static void GL_BINDING_CALL Mock_glShaderBinary(GLsizei n,
                                                const GLuint* shaders,
                                                GLenum binaryformat,
                                                const void* binary,
                                                GLsizei length);
static void GL_BINDING_CALL Mock_glShaderSource(GLuint shader,
                                                GLsizei count,
                                                const char* const* str,
                                                const GLint* length);
static void GL_BINDING_CALL Mock_glSignalSemaphoreEXT(GLuint semaphore,
                                                      GLuint numBufferBarriers,
                                                      const GLuint* buffers,
                                                      GLuint numTextureBarriers,
                                                      const GLuint* textures,
                                                      const GLenum* dstLayouts);
static void GL_BINDING_CALL Mock_glStartTilingQCOM(GLuint x,
                                                   GLuint y,
                                                   GLuint width,
                                                   GLuint height,
                                                   GLbitfield preserveMask);
static void GL_BINDING_CALL Mock_glStencilFunc(GLenum func,
                                               GLint ref,
                                               GLuint mask);
static void GL_BINDING_CALL Mock_glStencilFuncSeparate(GLenum face,
                                                       GLenum func,
                                                       GLint ref,
                                                       GLuint mask);
static void GL_BINDING_CALL Mock_glStencilMask(GLuint mask);
static void GL_BINDING_CALL Mock_glStencilMaskSeparate(GLenum face,
                                                       GLuint mask);
static void GL_BINDING_CALL Mock_glStencilOp(GLenum fail,
                                             GLenum zfail,
                                             GLenum zpass);
static void GL_BINDING_CALL Mock_glStencilOpSeparate(GLenum face,
                                                     GLenum fail,
                                                     GLenum zfail,
                                                     GLenum zpass);
static GLboolean GL_BINDING_CALL Mock_glTestFenceNV(GLuint fence);
static void GL_BINDING_CALL Mock_glTexBuffer(GLenum target,
                                             GLenum internalformat,
                                             GLuint buffer);
static void GL_BINDING_CALL Mock_glTexBufferEXT(GLenum target,
                                                GLenum internalformat,
                                                GLuint buffer);
static void GL_BINDING_CALL Mock_glTexBufferOES(GLenum target,
                                                GLenum internalformat,
                                                GLuint buffer);
static void GL_BINDING_CALL Mock_glTexBufferRange(GLenum target,
                                                  GLenum internalformat,
                                                  GLuint buffer,
                                                  GLintptr offset,
                                                  GLsizeiptr size);
static void GL_BINDING_CALL Mock_glTexBufferRangeEXT(GLenum target,
                                                     GLenum internalformat,
                                                     GLuint buffer,
                                                     GLintptr offset,
                                                     GLsizeiptr size);
static void GL_BINDING_CALL Mock_glTexBufferRangeOES(GLenum target,
                                                     GLenum internalformat,
                                                     GLuint buffer,
                                                     GLintptr offset,
                                                     GLsizeiptr size);
static void GL_BINDING_CALL Mock_glTexImage2D(GLenum target,
                                              GLint level,
                                              GLint internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLenum format,
                                              GLenum type,
                                              const void* pixels);
static void GL_BINDING_CALL Mock_glTexImage2DExternalANGLE(GLenum target,
                                                           GLint level,
                                                           GLint internalformat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLint border,
                                                           GLenum format,
                                                           GLenum type);
static void GL_BINDING_CALL Mock_glTexImage2DRobustANGLE(GLenum target,
                                                         GLint level,
                                                         GLint internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLint border,
                                                         GLenum format,
                                                         GLenum type,
                                                         GLsizei bufSize,
                                                         const void* pixels);
static void GL_BINDING_CALL Mock_glTexImage3D(GLenum target,
                                              GLint level,
                                              GLint internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLint border,
                                              GLenum format,
                                              GLenum type,
                                              const void* pixels);
static void GL_BINDING_CALL Mock_glTexImage3DRobustANGLE(GLenum target,
                                                         GLint level,
                                                         GLint internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth,
                                                         GLint border,
                                                         GLenum format,
                                                         GLenum type,
                                                         GLsizei bufSize,
                                                         const void* pixels);
static void GL_BINDING_CALL
Mock_glTexParameterIivRobustANGLE(GLenum target,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLint* params);
static void GL_BINDING_CALL
Mock_glTexParameterIuivRobustANGLE(GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLuint* params);
static void GL_BINDING_CALL Mock_glTexParameterf(GLenum target,
                                                 GLenum pname,
                                                 GLfloat param);
static void GL_BINDING_CALL Mock_glTexParameterfv(GLenum target,
                                                  GLenum pname,
                                                  const GLfloat* params);
static void GL_BINDING_CALL
Mock_glTexParameterfvRobustANGLE(GLenum target,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 const GLfloat* params);
static void GL_BINDING_CALL Mock_glTexParameteri(GLenum target,
                                                 GLenum pname,
                                                 GLint param);
static void GL_BINDING_CALL Mock_glTexParameteriv(GLenum target,
                                                  GLenum pname,
                                                  const GLint* params);
static void GL_BINDING_CALL
Mock_glTexParameterivRobustANGLE(GLenum target,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 const GLint* params);
static void GL_BINDING_CALL Mock_glTexStorage2D(GLenum target,
                                                GLsizei levels,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height);
static void GL_BINDING_CALL Mock_glTexStorage2DEXT(GLenum target,
                                                   GLsizei levels,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height);
static void GL_BINDING_CALL
Mock_glTexStorage2DMultisample(GLenum target,
                               GLsizei samples,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLboolean fixedsamplelocations);
static void GL_BINDING_CALL Mock_glTexStorage3D(GLenum target,
                                                GLsizei levels,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth);
static void GL_BINDING_CALL Mock_glTexStorageMem2DEXT(GLenum target,
                                                      GLsizei levels,
                                                      GLenum internalFormat,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLuint memory,
                                                      GLuint64 offset);
static void GL_BINDING_CALL
Mock_glTexStorageMemFlags2DANGLE(GLenum target,
                                 GLsizei levels,
                                 GLenum internalFormat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLuint memory,
                                 GLuint64 offset,
                                 GLbitfield createFlags,
                                 GLbitfield usageFlags,
                                 const void* imageCreateInfoPNext);
static void GL_BINDING_CALL Mock_glTexSubImage2D(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 const void* pixels);
static void GL_BINDING_CALL Mock_glTexSubImage2DRobustANGLE(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLenum format,
                                                            GLenum type,
                                                            GLsizei bufSize,
                                                            const void* pixels);
static void GL_BINDING_CALL Mock_glTexSubImage3D(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLenum format,
                                                 GLenum type,
                                                 const void* pixels);
static void GL_BINDING_CALL Mock_glTexSubImage3DRobustANGLE(GLenum target,
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
                                                            const void* pixels);
static void GL_BINDING_CALL
Mock_glTransformFeedbackVaryings(GLuint program,
                                 GLsizei count,
                                 const char* const* varyings,
                                 GLenum bufferMode);
static void GL_BINDING_CALL Mock_glUniform1f(GLint location, GLfloat x);
static void GL_BINDING_CALL Mock_glUniform1fv(GLint location,
                                              GLsizei count,
                                              const GLfloat* v);
static void GL_BINDING_CALL Mock_glUniform1i(GLint location, GLint x);
static void GL_BINDING_CALL Mock_glUniform1iv(GLint location,
                                              GLsizei count,
                                              const GLint* v);
static void GL_BINDING_CALL Mock_glUniform1ui(GLint location, GLuint v0);
static void GL_BINDING_CALL Mock_glUniform1uiv(GLint location,
                                               GLsizei count,
                                               const GLuint* v);
static void GL_BINDING_CALL Mock_glUniform2f(GLint location,
                                             GLfloat x,
                                             GLfloat y);
static void GL_BINDING_CALL Mock_glUniform2fv(GLint location,
                                              GLsizei count,
                                              const GLfloat* v);
static void GL_BINDING_CALL Mock_glUniform2i(GLint location, GLint x, GLint y);
static void GL_BINDING_CALL Mock_glUniform2iv(GLint location,
                                              GLsizei count,
                                              const GLint* v);
static void GL_BINDING_CALL Mock_glUniform2ui(GLint location,
                                              GLuint v0,
                                              GLuint v1);
static void GL_BINDING_CALL Mock_glUniform2uiv(GLint location,
                                               GLsizei count,
                                               const GLuint* v);
static void GL_BINDING_CALL Mock_glUniform3f(GLint location,
                                             GLfloat x,
                                             GLfloat y,
                                             GLfloat z);
static void GL_BINDING_CALL Mock_glUniform3fv(GLint location,
                                              GLsizei count,
                                              const GLfloat* v);
static void GL_BINDING_CALL Mock_glUniform3i(GLint location,
                                             GLint x,
                                             GLint y,
                                             GLint z);
static void GL_BINDING_CALL Mock_glUniform3iv(GLint location,
                                              GLsizei count,
                                              const GLint* v);
static void GL_BINDING_CALL Mock_glUniform3ui(GLint location,
                                              GLuint v0,
                                              GLuint v1,
                                              GLuint v2);
static void GL_BINDING_CALL Mock_glUniform3uiv(GLint location,
                                               GLsizei count,
                                               const GLuint* v);
static void GL_BINDING_CALL
Mock_glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
static void GL_BINDING_CALL Mock_glUniform4fv(GLint location,
                                              GLsizei count,
                                              const GLfloat* v);
static void GL_BINDING_CALL
Mock_glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
static void GL_BINDING_CALL Mock_glUniform4iv(GLint location,
                                              GLsizei count,
                                              const GLint* v);
static void GL_BINDING_CALL
Mock_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
static void GL_BINDING_CALL Mock_glUniform4uiv(GLint location,
                                               GLsizei count,
                                               const GLuint* v);
static void GL_BINDING_CALL
Mock_glUniformBlockBinding(GLuint program,
                           GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding);
static void GL_BINDING_CALL Mock_glUniformMatrix2fv(GLint location,
                                                    GLsizei count,
                                                    GLboolean transpose,
                                                    const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix2x3fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix2x4fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix3fv(GLint location,
                                                    GLsizei count,
                                                    GLboolean transpose,
                                                    const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix3x2fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix3x4fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix4fv(GLint location,
                                                    GLsizei count,
                                                    GLboolean transpose,
                                                    const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix4x2fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static void GL_BINDING_CALL Mock_glUniformMatrix4x3fv(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
static GLboolean GL_BINDING_CALL Mock_glUnmapBuffer(GLenum target);
static GLboolean GL_BINDING_CALL Mock_glUnmapBufferOES(GLenum target);
static void GL_BINDING_CALL Mock_glUseProgram(GLuint program);
static void GL_BINDING_CALL Mock_glUseProgramStages(GLuint pipeline,
                                                    GLbitfield stages,
                                                    GLuint program);
static void GL_BINDING_CALL Mock_glValidateProgram(GLuint program);
static void GL_BINDING_CALL Mock_glValidateProgramPipeline(GLuint pipeline);
static void GL_BINDING_CALL Mock_glVertexAttrib1f(GLuint indx, GLfloat x);
static void GL_BINDING_CALL Mock_glVertexAttrib1fv(GLuint indx,
                                                   const GLfloat* values);
static void GL_BINDING_CALL Mock_glVertexAttrib2f(GLuint indx,
                                                  GLfloat x,
                                                  GLfloat y);
static void GL_BINDING_CALL Mock_glVertexAttrib2fv(GLuint indx,
                                                   const GLfloat* values);
static void GL_BINDING_CALL Mock_glVertexAttrib3f(GLuint indx,
                                                  GLfloat x,
                                                  GLfloat y,
                                                  GLfloat z);
static void GL_BINDING_CALL Mock_glVertexAttrib3fv(GLuint indx,
                                                   const GLfloat* values);
static void GL_BINDING_CALL
Mock_glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
static void GL_BINDING_CALL Mock_glVertexAttrib4fv(GLuint indx,
                                                   const GLfloat* values);
static void GL_BINDING_CALL Mock_glVertexAttribBinding(GLuint attribindex,
                                                       GLuint bindingindex);
static void GL_BINDING_CALL Mock_glVertexAttribDivisor(GLuint index,
                                                       GLuint divisor);
static void GL_BINDING_CALL Mock_glVertexAttribDivisorANGLE(GLuint index,
                                                            GLuint divisor);
static void GL_BINDING_CALL Mock_glVertexAttribDivisorEXT(GLuint index,
                                                          GLuint divisor);
static void GL_BINDING_CALL Mock_glVertexAttribFormat(GLuint attribindex,
                                                      GLint size,
                                                      GLenum type,
                                                      GLboolean normalized,
                                                      GLuint relativeoffset);
static void GL_BINDING_CALL
Mock_glVertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w);
static void GL_BINDING_CALL Mock_glVertexAttribI4iv(GLuint indx,
                                                    const GLint* values);
static void GL_BINDING_CALL
Mock_glVertexAttribI4ui(GLuint indx, GLuint x, GLuint y, GLuint z, GLuint w);
static void GL_BINDING_CALL Mock_glVertexAttribI4uiv(GLuint indx,
                                                     const GLuint* values);
static void GL_BINDING_CALL Mock_glVertexAttribIFormat(GLuint attribindex,
                                                       GLint size,
                                                       GLenum type,
                                                       GLuint relativeoffset);
static void GL_BINDING_CALL Mock_glVertexAttribIPointer(GLuint indx,
                                                        GLint size,
                                                        GLenum type,
                                                        GLsizei stride,
                                                        const void* ptr);
static void GL_BINDING_CALL Mock_glVertexAttribPointer(GLuint indx,
                                                       GLint size,
                                                       GLenum type,
                                                       GLboolean normalized,
                                                       GLsizei stride,
                                                       const void* ptr);
static void GL_BINDING_CALL Mock_glVertexBindingDivisor(GLuint bindingindex,
                                                        GLuint divisor);
static void GL_BINDING_CALL Mock_glViewport(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height);
static void GL_BINDING_CALL Mock_glWaitSemaphoreEXT(GLuint semaphore,
                                                    GLuint numBufferBarriers,
                                                    const GLuint* buffers,
                                                    GLuint numTextureBarriers,
                                                    const GLuint* textures,
                                                    const GLenum* srcLayouts);
static void GL_BINDING_CALL Mock_glWaitSync(GLsync sync,
                                            GLbitfield flags,
                                            GLuint64 timeout);
static void GL_BINDING_CALL Mock_glWindowRectanglesEXT(GLenum mode,
                                                       GLsizei n,
                                                       const GLint* box);
