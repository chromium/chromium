// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef UI_GL_GL_BINDINGS_AUTOGEN_GL_H_
#define UI_GL_GL_BINDINGS_AUTOGEN_GL_H_

#include <string>

namespace gl {

class GLContext;

typedef void(GL_BINDING_CALL* glAcquireTexturesANGLEProc)(
    GLuint numTextures,
    const GLuint* textures,
    const GLenum* layouts);
typedef void(GL_BINDING_CALL* glActiveTextureProc)(GLenum texture);
typedef void(GL_BINDING_CALL* glAttachShaderProc)(GLuint program,
                                                  GLuint shader);
typedef void(GL_BINDING_CALL* glBeginPixelLocalStorageANGLEProc)(
    GLsizei n,
    const GLenum* loadops);
typedef void(GL_BINDING_CALL* glBeginQueryProc)(GLenum target, GLuint id);
typedef void(GL_BINDING_CALL* glBeginTransformFeedbackProc)(
    GLenum primitiveMode);
typedef void(GL_BINDING_CALL* glBindAttribLocationProc)(GLuint program,
                                                        GLuint index,
                                                        const char* name);
typedef void(GL_BINDING_CALL* glBindBufferProc)(GLenum target, GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferBaseProc)(GLenum target,
                                                    GLuint index,
                                                    GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferRangeProc)(GLenum target,
                                                     GLuint index,
                                                     GLuint buffer,
                                                     GLintptr offset,
                                                     GLsizeiptr size);
typedef void(GL_BINDING_CALL* glBindFragDataLocationProc)(GLuint program,
                                                          GLuint colorNumber,
                                                          const char* name);
typedef void(GL_BINDING_CALL* glBindFragDataLocationIndexedProc)(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name);
typedef void(GL_BINDING_CALL* glBindFramebufferEXTProc)(GLenum target,
                                                        GLuint framebuffer);
typedef void(GL_BINDING_CALL* glBindRenderbufferEXTProc)(GLenum target,
                                                         GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glBindSamplerProc)(GLuint unit, GLuint sampler);
typedef void(GL_BINDING_CALL* glBindTextureProc)(GLenum target, GLuint texture);
typedef void(GL_BINDING_CALL* glBindTransformFeedbackProc)(GLenum target,
                                                           GLuint id);
typedef void(GL_BINDING_CALL* glBindUniformLocationCHROMIUMProc)(
    GLuint program,
    GLint location,
    const char* name);
typedef void(GL_BINDING_CALL* glBindVertexArrayOESProc)(GLuint array);
typedef void(GL_BINDING_CALL* glBlendBarrierKHRProc)(void);
typedef void(GL_BINDING_CALL* glBlendColorProc)(GLclampf red,
                                                GLclampf green,
                                                GLclampf blue,
                                                GLclampf alpha);
typedef void(GL_BINDING_CALL* glBlendEquationProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glBlendEquationiOESProc)(GLuint buf, GLenum mode);
typedef void(GL_BINDING_CALL* glBlendEquationSeparateProc)(GLenum modeRGB,
                                                           GLenum modeAlpha);
typedef void(GL_BINDING_CALL* glBlendEquationSeparateiOESProc)(
    GLuint buf,
    GLenum modeRGB,
    GLenum modeAlpha);
typedef void(GL_BINDING_CALL* glBlendFuncProc)(GLenum sfactor, GLenum dfactor);
typedef void(GL_BINDING_CALL* glBlendFunciOESProc)(GLuint buf,
                                                   GLenum sfactor,
                                                   GLenum dfactor);
typedef void(GL_BINDING_CALL* glBlendFuncSeparateProc)(GLenum srcRGB,
                                                       GLenum dstRGB,
                                                       GLenum srcAlpha,
                                                       GLenum dstAlpha);
typedef void(GL_BINDING_CALL* glBlendFuncSeparateiOESProc)(GLuint buf,
                                                           GLenum srcRGB,
                                                           GLenum dstRGB,
                                                           GLenum srcAlpha,
                                                           GLenum dstAlpha);
typedef void(GL_BINDING_CALL* glBlitFramebufferProc)(GLint srcX0,
                                                     GLint srcY0,
                                                     GLint srcX1,
                                                     GLint srcY1,
                                                     GLint dstX0,
                                                     GLint dstY0,
                                                     GLint dstX1,
                                                     GLint dstY1,
                                                     GLbitfield mask,
                                                     GLenum filter);
typedef void(GL_BINDING_CALL* glBlobCacheCallbacksANGLEProc)(
    GLSETBLOBPROCANGLE set,
    GLGETBLOBPROCANGLE get,
    const void* userData);
typedef void(GL_BINDING_CALL* glBufferDataProc)(GLenum target,
                                                GLsizeiptr size,
                                                const void* data,
                                                GLenum usage);
typedef void(GL_BINDING_CALL* glBufferSubDataProc)(GLenum target,
                                                   GLintptr offset,
                                                   GLsizeiptr size,
                                                   const void* data);
typedef GLenum(GL_BINDING_CALL* glCheckFramebufferStatusEXTProc)(GLenum target);
typedef void(GL_BINDING_CALL* glClearProc)(GLbitfield mask);
typedef void(GL_BINDING_CALL* glClearBufferfiProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLfloat depth,
                                                   GLint stencil);
typedef void(GL_BINDING_CALL* glClearBufferfvProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLfloat* value);
typedef void(GL_BINDING_CALL* glClearBufferivProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLint* value);
typedef void(GL_BINDING_CALL* glClearBufferuivProc)(GLenum buffer,
                                                    GLint drawbuffer,
                                                    const GLuint* value);
typedef void(GL_BINDING_CALL* glClearColorProc)(GLclampf red,
                                                GLclampf green,
                                                GLclampf blue,
                                                GLclampf alpha);
typedef void(GL_BINDING_CALL* glClearDepthProc)(GLclampd depth);
typedef void(GL_BINDING_CALL* glClearDepthfProc)(GLclampf depth);
typedef void(GL_BINDING_CALL* glClearStencilProc)(GLint s);
typedef void(GL_BINDING_CALL* glClearTexImageProc)(GLuint texture,
                                                   GLint level,
                                                   GLenum format,
                                                   GLenum type,
                                                   const GLvoid* data);
typedef void(GL_BINDING_CALL* glClearTexSubImageProc)(GLuint texture,
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
typedef GLenum(GL_BINDING_CALL* glClientWaitSyncProc)(GLsync sync,
                                                      GLbitfield flags,
                                                      GLuint64 timeout);
typedef void(GL_BINDING_CALL* glClipControlEXTProc)(GLenum origin,
                                                    GLenum depth);
typedef void(GL_BINDING_CALL* glColorMaskProc)(GLboolean red,
                                               GLboolean green,
                                               GLboolean blue,
                                               GLboolean alpha);
typedef void(GL_BINDING_CALL* glColorMaskiOESProc)(GLuint buf,
                                                   GLboolean red,
                                                   GLboolean green,
                                                   GLboolean blue,
                                                   GLboolean alpha);
typedef void(GL_BINDING_CALL* glCompileShaderProc)(GLuint shader);
typedef void(GL_BINDING_CALL* glCompressedTexImage2DProc)(GLenum target,
                                                          GLint level,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLint border,
                                                          GLsizei imageSize,
                                                          const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexImage3DProc)(GLenum target,
                                                          GLint level,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLint border,
                                                          GLsizei imageSize,
                                                          const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage2DProc)(GLenum target,
                                                             GLint level,
                                                             GLint xoffset,
                                                             GLint yoffset,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             GLenum format,
                                                             GLsizei imageSize,
                                                             const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage3DProc)(GLenum target,
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
typedef void(GL_BINDING_CALL* glCopyBufferSubDataProc)(GLenum readTarget,
                                                       GLenum writeTarget,
                                                       GLintptr readOffset,
                                                       GLintptr writeOffset,
                                                       GLsizeiptr size);
typedef void(GL_BINDING_CALL* glCopySubTextureCHROMIUMProc)(
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
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCopyTexImage2DProc)(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLint x,
                                                    GLint y,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border);
typedef void(GL_BINDING_CALL* glCopyTexSubImage2DProc)(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTexSubImage3DProc)(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTextureCHROMIUMProc)(
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint internalFormat,
    GLenum destType,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCreateMemoryObjectsEXTProc)(
    GLsizei n,
    GLuint* memoryObjects);
typedef GLuint(GL_BINDING_CALL* glCreateProgramProc)(void);
typedef GLuint(GL_BINDING_CALL* glCreateShaderProc)(GLenum type);
typedef void(GL_BINDING_CALL* glCullFaceProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glDebugMessageCallbackKHRProc)(
    GLDEBUGPROC callback,
    const void* userParam);
typedef void(GL_BINDING_CALL* glDebugMessageControlKHRProc)(GLenum source,
                                                            GLenum type,
                                                            GLenum severity,
                                                            GLsizei count,
                                                            const GLuint* ids,
                                                            GLboolean enabled);
typedef void(GL_BINDING_CALL* glDebugMessageInsertKHRProc)(GLenum source,
                                                           GLenum type,
                                                           GLuint id,
                                                           GLenum severity,
                                                           GLsizei length,
                                                           const char* buf);
typedef void(GL_BINDING_CALL* glDeleteBuffersARBProc)(GLsizei n,
                                                      const GLuint* buffers);
typedef void(GL_BINDING_CALL* glDeleteFencesNVProc)(GLsizei n,
                                                    const GLuint* fences);
typedef void(GL_BINDING_CALL* glDeleteFramebuffersEXTProc)(
    GLsizei n,
    const GLuint* framebuffers);
typedef void(GL_BINDING_CALL* glDeleteMemoryObjectsEXTProc)(
    GLsizei n,
    const GLuint* memoryObjects);
typedef void(GL_BINDING_CALL* glDeleteProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glDeleteQueriesProc)(GLsizei n,
                                                   const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteRenderbuffersEXTProc)(
    GLsizei n,
    const GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glDeleteSamplersProc)(GLsizei n,
                                                    const GLuint* samplers);
typedef void(GL_BINDING_CALL* glDeleteSemaphoresEXTProc)(
    GLsizei n,
    const GLuint* semaphores);
typedef void(GL_BINDING_CALL* glDeleteShaderProc)(GLuint shader);
typedef void(GL_BINDING_CALL* glDeleteSyncProc)(GLsync sync);
typedef void(GL_BINDING_CALL* glDeleteTexturesProc)(GLsizei n,
                                                    const GLuint* textures);
typedef void(GL_BINDING_CALL* glDeleteTransformFeedbacksProc)(
    GLsizei n,
    const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteVertexArraysOESProc)(
    GLsizei n,
    const GLuint* arrays);
typedef void(GL_BINDING_CALL* glDepthFuncProc)(GLenum func);
typedef void(GL_BINDING_CALL* glDepthMaskProc)(GLboolean flag);
typedef void(GL_BINDING_CALL* glDepthRangeProc)(GLclampd zNear, GLclampd zFar);
typedef void(GL_BINDING_CALL* glDepthRangefProc)(GLclampf zNear, GLclampf zFar);
typedef void(GL_BINDING_CALL* glDetachShaderProc)(GLuint program,
                                                  GLuint shader);
typedef void(GL_BINDING_CALL* glDisableProc)(GLenum cap);
typedef void(GL_BINDING_CALL* glDisableiOESProc)(GLenum target, GLuint index);
typedef void(GL_BINDING_CALL* glDisableVertexAttribArrayProc)(GLuint index);
typedef void(GL_BINDING_CALL* glDiscardFramebufferEXTProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glDrawArraysProc)(GLenum mode,
                                                GLint first,
                                                GLsizei count);
typedef void(GL_BINDING_CALL* glDrawArraysInstancedANGLEProc)(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount);
typedef void(GL_BINDING_CALL* glDrawArraysInstancedBaseInstanceANGLEProc)(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance);
typedef void(GL_BINDING_CALL* glDrawBufferProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glDrawBuffersARBProc)(GLsizei n,
                                                    const GLenum* bufs);
typedef void(GL_BINDING_CALL* glDrawElementsProc)(GLenum mode,
                                                  GLsizei count,
                                                  GLenum type,
                                                  const void* indices);
typedef void(GL_BINDING_CALL* glDrawElementsInstancedANGLEProc)(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount);
typedef void(
    GL_BINDING_CALL* glDrawElementsInstancedBaseVertexBaseInstanceANGLEProc)(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint baseVertex,
    GLuint baseInstance);
typedef void(GL_BINDING_CALL* glDrawRangeElementsProc)(GLenum mode,
                                                       GLuint start,
                                                       GLuint end,
                                                       GLsizei count,
                                                       GLenum type,
                                                       const void* indices);
typedef void(GL_BINDING_CALL* glEGLImageTargetRenderbufferStorageOESProc)(
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEGLImageTargetTexStorageEXTProc)(
    GLenum target,
    GLeglImageOES image,
    const GLint* attrib_list);
typedef void(GL_BINDING_CALL* glEGLImageTargetTexture2DOESProc)(
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEnableProc)(GLenum cap);
typedef void(GL_BINDING_CALL* glEnableiOESProc)(GLenum target, GLuint index);
typedef void(GL_BINDING_CALL* glEnableVertexAttribArrayProc)(GLuint index);
typedef void(GL_BINDING_CALL* glEndPixelLocalStorageANGLEProc)(
    GLsizei n,
    const GLenum* storeops);
typedef void(GL_BINDING_CALL* glEndPixelLocalStorageImplicitANGLEProc)();
typedef void(GL_BINDING_CALL* glEndQueryProc)(GLenum target);
typedef void(GL_BINDING_CALL* glEndTilingQCOMProc)(GLbitfield preserveMask);
typedef void(GL_BINDING_CALL* glEndTransformFeedbackProc)(void);
typedef GLsync(GL_BINDING_CALL* glFenceSyncProc)(GLenum condition,
                                                 GLbitfield flags);
typedef void(GL_BINDING_CALL* glFinishProc)(void);
typedef void(GL_BINDING_CALL* glFinishFenceNVProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glFlushProc)(void);
typedef void(GL_BINDING_CALL* glFlushMappedBufferRangeProc)(GLenum target,
                                                            GLintptr offset,
                                                            GLsizeiptr length);
typedef void(
    GL_BINDING_CALL* glFramebufferMemorylessPixelLocalStorageANGLEProc)(
    GLint plane,
    GLenum internalformat,
    GLbitfield usage);
typedef void(GL_BINDING_CALL* glFramebufferParameteriMESAProc)(GLenum target,
                                                               GLenum pname,
                                                               GLint param);
typedef void(GL_BINDING_CALL* glFramebufferPixelLocalClearValuefvANGLEProc)(
    GLint plane,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glFramebufferPixelLocalClearValueivANGLEProc)(
    GLint plane,
    const GLint* value);
typedef void(GL_BINDING_CALL* glFramebufferPixelLocalClearValueuivANGLEProc)(
    GLint plane,
    const GLuint* value);
typedef void(
    GL_BINDING_CALL* glFramebufferPixelLocalStorageInterruptANGLEProc)();
typedef void(GL_BINDING_CALL* glFramebufferPixelLocalStorageRestoreANGLEProc)();
typedef void(GL_BINDING_CALL* glFramebufferRenderbufferEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glFramebufferTexture2DEXTProc)(GLenum target,
                                                             GLenum attachment,
                                                             GLenum textarget,
                                                             GLuint texture,
                                                             GLint level);
typedef void(GL_BINDING_CALL* glFramebufferTexture2DMultisampleEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
typedef void(GL_BINDING_CALL* glFramebufferTextureLayerProc)(GLenum target,
                                                             GLenum attachment,
                                                             GLuint texture,
                                                             GLint level,
                                                             GLint layer);
typedef void(GL_BINDING_CALL* glFramebufferTextureMultiviewOVRProc)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
typedef void(GL_BINDING_CALL* glFramebufferTexturePixelLocalStorageANGLEProc)(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer,
    GLbitfield usage);
typedef void(GL_BINDING_CALL* glFrontFaceProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glGenBuffersARBProc)(GLsizei n, GLuint* buffers);
typedef void(GL_BINDING_CALL* glGenerateMipmapEXTProc)(GLenum target);
typedef void(GL_BINDING_CALL* glGenFencesNVProc)(GLsizei n, GLuint* fences);
typedef void(GL_BINDING_CALL* glGenFramebuffersEXTProc)(GLsizei n,
                                                        GLuint* framebuffers);
typedef void(GL_BINDING_CALL* glGenQueriesProc)(GLsizei n, GLuint* ids);
typedef void(GL_BINDING_CALL* glGenRenderbuffersEXTProc)(GLsizei n,
                                                         GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glGenSamplersProc)(GLsizei n, GLuint* samplers);
typedef void(GL_BINDING_CALL* glGenSemaphoresEXTProc)(GLsizei n,
                                                      GLuint* semaphores);
typedef void(GL_BINDING_CALL* glGenTexturesProc)(GLsizei n, GLuint* textures);
typedef void(GL_BINDING_CALL* glGenTransformFeedbacksProc)(GLsizei n,
                                                           GLuint* ids);
typedef void(GL_BINDING_CALL* glGenVertexArraysOESProc)(GLsizei n,
                                                        GLuint* arrays);
typedef void(GL_BINDING_CALL* glGetActiveAttribProc)(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     GLint* size,
                                                     GLenum* type,
                                                     char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformProc)(GLuint program,
                                                      GLuint index,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* size,
                                                      GLenum* type,
                                                      char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockivProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockivRobustANGLEProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockNameProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLsizei bufSize,
    GLsizei* length,
    char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetActiveUniformsivProc)(
    GLuint program,
    GLsizei uniformCount,
    const GLuint* uniformIndices,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetAttachedShadersProc)(GLuint program,
                                                        GLsizei maxcount,
                                                        GLsizei* count,
                                                        GLuint* shaders);
typedef GLint(GL_BINDING_CALL* glGetAttribLocationProc)(GLuint program,
                                                        const char* name);
typedef void(GL_BINDING_CALL* glGetBooleanvProc)(GLenum pname,
                                                 GLboolean* params);
typedef void(GL_BINDING_CALL* glGetBooleanvRobustANGLEProc)(GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLboolean* data);
typedef void(GL_BINDING_CALL* glGetBufferParameteri64vRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetBufferParameterivProc)(GLenum target,
                                                          GLenum pname,
                                                          GLint* params);
typedef void(GL_BINDING_CALL* glGetBufferParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetBufferPointervRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** params);
typedef GLuint(GL_BINDING_CALL* glGetDebugMessageLogKHRProc)(GLuint count,
                                                             GLsizei bufSize,
                                                             GLenum* sources,
                                                             GLenum* types,
                                                             GLuint* ids,
                                                             GLenum* severities,
                                                             GLsizei* lengths,
                                                             char* messageLog);
typedef GLenum(GL_BINDING_CALL* glGetErrorProc)(void);
typedef void(GL_BINDING_CALL* glGetFenceivNVProc)(GLuint fence,
                                                  GLenum pname,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetFloatvProc)(GLenum pname, GLfloat* params);
typedef void(GL_BINDING_CALL* glGetFloatvRobustANGLEProc)(GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* data);
typedef GLint(GL_BINDING_CALL* glGetFragDataIndexProc)(GLuint program,
                                                       const char* name);
typedef GLint(GL_BINDING_CALL* glGetFragDataLocationProc)(GLuint program,
                                                          const char* name);
typedef void(GL_BINDING_CALL* glGetFramebufferAttachmentParameterivEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetFramebufferAttachmentParameterivRobustANGLEProc)(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL*
                 glGetFramebufferPixelLocalStorageParameterfvRobustANGLEProc)(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL*
                 glGetFramebufferPixelLocalStorageParameterivRobustANGLEProc)(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL*
                 glGetFramebufferPixelLocalStorageParameteruivRobustANGLEProc)(
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLuint* params);
typedef GLenum(GL_BINDING_CALL* glGetGraphicsResetStatusARBProc)(void);
typedef void(GL_BINDING_CALL* glGetInteger64i_vProc)(GLenum target,
                                                     GLuint index,
                                                     GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64i_vRobustANGLEProc)(GLenum target,
                                                                GLuint index,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64vProc)(GLenum pname,
                                                   GLint64* params);
typedef void(GL_BINDING_CALL* glGetInteger64vRobustANGLEProc)(GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint64* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vProc)(GLenum target,
                                                   GLuint index,
                                                   GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vRobustANGLEProc)(GLenum target,
                                                              GLuint index,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegervProc)(GLenum pname, GLint* params);
typedef void(GL_BINDING_CALL* glGetIntegervRobustANGLEProc)(GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* data);
typedef void(GL_BINDING_CALL* glGetInternalformativProc)(GLenum target,
                                                         GLenum internalformat,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLint* params);
typedef void(GL_BINDING_CALL* glGetInternalformativRobustANGLEProc)(
    GLenum target,
    GLenum internalformat,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetInternalformatSampleivNVProc)(
    GLenum target,
    GLenum internalformat,
    GLsizei samples,
    GLenum pname,
    GLsizei bufSize,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetMultisamplefvRobustANGLEProc)(
    GLenum pname,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* val);
typedef void(GL_BINDING_CALL* glGetObjectLabelKHRProc)(GLenum identifier,
                                                       GLuint name,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       char* label);
typedef void(GL_BINDING_CALL* glGetObjectPtrLabelKHRProc)(void* ptr,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          char* label);
typedef void(GL_BINDING_CALL* glGetPointervKHRProc)(GLenum pname,
                                                    void** params);
typedef void(GL_BINDING_CALL* glGetProgramBinaryProc)(GLuint program,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLenum* binaryFormat,
                                                      GLvoid* binary);
typedef void(GL_BINDING_CALL* glGetProgramInfoLogProc)(GLuint program,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       char* infolog);
typedef void(GL_BINDING_CALL* glGetProgramivProc)(GLuint program,
                                                  GLenum pname,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramivRobustANGLEProc)(GLuint program,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryivProc)(GLenum target,
                                                GLenum pname,
                                                GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryivRobustANGLEProc)(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vProc)(GLuint id,
                                                        GLenum pname,
                                                        GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivProc)(GLuint id,
                                                      GLenum pname,
                                                      GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vProc)(GLuint id,
                                                         GLenum pname,
                                                         GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivProc)(GLuint id,
                                                       GLenum pname,
                                                       GLuint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetRenderbufferParameterivEXTProc)(
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetRenderbufferParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterfvProc)(GLuint sampler,
                                                           GLenum pname,
                                                           GLfloat* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterfvRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterivProc)(GLuint sampler,
                                                           GLenum pname,
                                                           GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderInfoLogProc)(GLuint shader,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      char* infolog);
typedef void(GL_BINDING_CALL* glGetShaderivProc)(GLuint shader,
                                                 GLenum pname,
                                                 GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderivRobustANGLEProc)(GLuint shader,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderPrecisionFormatProc)(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision);
typedef void(GL_BINDING_CALL* glGetShaderSourceProc)(GLuint shader,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     char* source);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringProc)(GLenum name);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringiProc)(GLenum name,
                                                          GLuint index);
typedef void(GL_BINDING_CALL* glGetSyncivProc)(GLsync sync,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* values);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterfvANGLEProc)(
    GLenum target,
    GLint level,
    GLenum pname,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterfvRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterivANGLEProc)(GLenum target,
                                                                 GLint level,
                                                                 GLenum pname,
                                                                 GLint* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterivRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvProc)(GLenum target,
                                                       GLenum pname,
                                                       GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivProc)(GLenum target,
                                                       GLenum pname,
                                                       GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTransformFeedbackVaryingProc)(
    GLuint program,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLsizei* size,
    GLenum* type,
    char* name);
typedef void(GL_BINDING_CALL* glGetTranslatedShaderSourceANGLEProc)(
    GLuint shader,
    GLsizei bufsize,
    GLsizei* length,
    char* source);
typedef GLuint(GL_BINDING_CALL* glGetUniformBlockIndexProc)(
    GLuint program,
    const char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetUniformfvProc)(GLuint program,
                                                  GLint location,
                                                  GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformfvRobustANGLEProc)(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformIndicesProc)(
    GLuint program,
    GLsizei uniformCount,
    const char* const* uniformNames,
    GLuint* uniformIndices);
typedef void(GL_BINDING_CALL* glGetUniformivProc)(GLuint program,
                                                  GLint location,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetUniformivRobustANGLEProc)(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params);
typedef GLint(GL_BINDING_CALL* glGetUniformLocationProc)(GLuint program,
                                                         const char* name);
typedef void(GL_BINDING_CALL* glGetUniformuivProc)(GLuint program,
                                                   GLint location,
                                                   GLuint* params);
typedef void(GL_BINDING_CALL* glGetUniformuivRobustANGLEProc)(GLuint program,
                                                              GLint location,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvProc)(GLuint index,
                                                       GLenum pname,
                                                       GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIuivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivProc)(GLuint index,
                                                       GLenum pname,
                                                       GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribPointervProc)(GLuint index,
                                                             GLenum pname,
                                                             void** pointer);
typedef void(GL_BINDING_CALL* glGetVertexAttribPointervRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** pointer);
typedef void(GL_BINDING_CALL* glHintProc)(GLenum target, GLenum mode);
typedef void(GL_BINDING_CALL* glImportMemoryFdEXTProc)(GLuint memory,
                                                       GLuint64 size,
                                                       GLenum handleType,
                                                       GLint fd);
typedef void(GL_BINDING_CALL* glImportMemoryWin32HandleEXTProc)(
    GLuint memory,
    GLuint64 size,
    GLenum handleType,
    void* handle);
typedef void(GL_BINDING_CALL* glImportMemoryZirconHandleANGLEProc)(
    GLuint memory,
    GLuint64 size,
    GLenum handleType,
    GLuint handle);
typedef void(GL_BINDING_CALL* glImportSemaphoreFdEXTProc)(GLuint semaphore,
                                                          GLenum handleType,
                                                          GLint fd);
typedef void(GL_BINDING_CALL* glImportSemaphoreWin32HandleEXTProc)(
    GLuint semaphore,
    GLenum handleType,
    void* handle);
typedef void(GL_BINDING_CALL* glImportSemaphoreZirconHandleANGLEProc)(
    GLuint semaphore,
    GLenum handleType,
    GLuint handle);
typedef void(GL_BINDING_CALL* glInsertEventMarkerEXTProc)(GLsizei length,
                                                          const char* marker);
typedef void(GL_BINDING_CALL* glInvalidateFramebufferProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glInvalidateSubFramebufferProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments,
    GLint x,
    GLint y,
    GLint width,
    GLint height);
typedef void(GL_BINDING_CALL* glInvalidateTextureANGLEProc)(GLenum target);
typedef GLboolean(GL_BINDING_CALL* glIsBufferProc)(GLuint buffer);
typedef GLboolean(GL_BINDING_CALL* glIsEnabledProc)(GLenum cap);
typedef GLboolean(GL_BINDING_CALL* glIsEnablediOESProc)(GLenum target,
                                                        GLuint index);
typedef GLboolean(GL_BINDING_CALL* glIsFenceNVProc)(GLuint fence);
typedef GLboolean(GL_BINDING_CALL* glIsFramebufferEXTProc)(GLuint framebuffer);
typedef GLboolean(GL_BINDING_CALL* glIsProgramProc)(GLuint program);
typedef GLboolean(GL_BINDING_CALL* glIsQueryProc)(GLuint query);
typedef GLboolean(GL_BINDING_CALL* glIsRenderbufferEXTProc)(
    GLuint renderbuffer);
typedef GLboolean(GL_BINDING_CALL* glIsSamplerProc)(GLuint sampler);
typedef GLboolean(GL_BINDING_CALL* glIsShaderProc)(GLuint shader);
typedef GLboolean(GL_BINDING_CALL* glIsSyncProc)(GLsync sync);
typedef GLboolean(GL_BINDING_CALL* glIsTextureProc)(GLuint texture);
typedef GLboolean(GL_BINDING_CALL* glIsTransformFeedbackProc)(GLuint id);
typedef GLboolean(GL_BINDING_CALL* glIsVertexArrayOESProc)(GLuint array);
typedef void(GL_BINDING_CALL* glLineWidthProc)(GLfloat width);
typedef void(GL_BINDING_CALL* glLinkProgramProc)(GLuint program);
typedef void*(GL_BINDING_CALL* glMapBufferProc)(GLenum target, GLenum access);
typedef void*(GL_BINDING_CALL* glMapBufferRangeProc)(GLenum target,
                                                     GLintptr offset,
                                                     GLsizeiptr length,
                                                     GLbitfield access);
typedef void(GL_BINDING_CALL* glMaxShaderCompilerThreadsKHRProc)(GLuint count);
typedef void(GL_BINDING_CALL* glMemoryObjectParameterivEXTProc)(
    GLuint memoryObject,
    GLenum pname,
    const GLint* param);
typedef void(GL_BINDING_CALL* glMultiDrawArraysANGLEProc)(GLenum mode,
                                                          const GLint* firsts,
                                                          const GLsizei* counts,
                                                          GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawArraysInstancedANGLEProc)(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawArraysInstancedBaseInstanceANGLEProc)(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    const GLuint* baseInstances,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawElementsANGLEProc)(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawElementsInstancedANGLEProc)(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL*
                 glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEProc)(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    const GLint* baseVertices,
    const GLuint* baseInstances,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glObjectLabelKHRProc)(GLenum identifier,
                                                    GLuint name,
                                                    GLsizei length,
                                                    const char* label);
typedef void(GL_BINDING_CALL* glObjectPtrLabelKHRProc)(void* ptr,
                                                       GLsizei length,
                                                       const char* label);
typedef void(GL_BINDING_CALL* glPatchParameteriOESProc)(GLenum pname,
                                                        GLint value);
typedef void(GL_BINDING_CALL* glPauseTransformFeedbackProc)(void);
typedef void(GL_BINDING_CALL* glPixelLocalStorageBarrierANGLEProc)();
typedef void(GL_BINDING_CALL* glPixelStoreiProc)(GLenum pname, GLint param);
typedef void(GL_BINDING_CALL* glPointParameteriProc)(GLenum pname, GLint param);
typedef void(GL_BINDING_CALL* glPolygonModeProc)(GLenum face, GLenum mode);
typedef void(GL_BINDING_CALL* glPolygonModeANGLEProc)(GLenum face, GLenum mode);
typedef void(GL_BINDING_CALL* glPolygonOffsetProc)(GLfloat factor,
                                                   GLfloat units);
typedef void(GL_BINDING_CALL* glPolygonOffsetClampEXTProc)(GLfloat factor,
                                                           GLfloat units,
                                                           GLfloat clamp);
typedef void(GL_BINDING_CALL* glPopDebugGroupKHRProc)();
typedef void(GL_BINDING_CALL* glPopGroupMarkerEXTProc)(void);
typedef void(GL_BINDING_CALL* glPrimitiveRestartIndexProc)(GLuint index);
typedef void(GL_BINDING_CALL* glProgramBinaryProc)(GLuint program,
                                                   GLenum binaryFormat,
                                                   const GLvoid* binary,
                                                   GLsizei length);
typedef void(GL_BINDING_CALL* glProgramParameteriProc)(GLuint program,
                                                       GLenum pname,
                                                       GLint value);
typedef void(GL_BINDING_CALL* glProvokingVertexANGLEProc)(GLenum provokeMode);
typedef void(GL_BINDING_CALL* glPushDebugGroupKHRProc)(GLenum source,
                                                       GLuint id,
                                                       GLsizei length,
                                                       const char* message);
typedef void(GL_BINDING_CALL* glPushGroupMarkerEXTProc)(GLsizei length,
                                                        const char* marker);
typedef void(GL_BINDING_CALL* glQueryCounterProc)(GLuint id, GLenum target);
typedef void(GL_BINDING_CALL* glReadBufferProc)(GLenum src);
typedef void(GL_BINDING_CALL* glReadPixelsProc)(GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                void* pixels);
typedef void(GL_BINDING_CALL* glReadPixelsRobustANGLEProc)(GLint x,
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
typedef void(GL_BINDING_CALL* glReleaseShaderCompilerProc)(void);
typedef void(GL_BINDING_CALL* glReleaseTexturesANGLEProc)(
    GLuint numTextures,
    const GLuint* textures,
    GLenum* layouts);
typedef void(GL_BINDING_CALL* glRenderbufferStorageEXTProc)(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleProc)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleAdvancedAMDProc)(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleEXTProc)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRequestExtensionANGLEProc)(const char* name);
typedef void(GL_BINDING_CALL* glResumeTransformFeedbackProc)(void);
typedef void(GL_BINDING_CALL* glSampleCoverageProc)(GLclampf value,
                                                    GLboolean invert);
typedef void(GL_BINDING_CALL* glSamplerParameterfProc)(GLuint sampler,
                                                       GLenum pname,
                                                       GLfloat param);
typedef void(GL_BINDING_CALL* glSamplerParameterfvProc)(GLuint sampler,
                                                        GLenum pname,
                                                        const GLfloat* params);
typedef void(GL_BINDING_CALL* glSamplerParameterfvRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* param);
typedef void(GL_BINDING_CALL* glSamplerParameteriProc)(GLuint sampler,
                                                       GLenum pname,
                                                       GLint param);
typedef void(GL_BINDING_CALL* glSamplerParameterivProc)(GLuint sampler,
                                                        GLenum pname,
                                                        const GLint* params);
typedef void(GL_BINDING_CALL* glSamplerParameterivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLint* param);
typedef void(GL_BINDING_CALL* glScissorProc)(GLint x,
                                             GLint y,
                                             GLsizei width,
                                             GLsizei height);
typedef void(GL_BINDING_CALL* glSetFenceNVProc)(GLuint fence, GLenum condition);
typedef void(GL_BINDING_CALL* glShaderBinaryProc)(GLsizei n,
                                                  const GLuint* shaders,
                                                  GLenum binaryformat,
                                                  const void* binary,
                                                  GLsizei length);
typedef void(GL_BINDING_CALL* glShaderSourceProc)(GLuint shader,
                                                  GLsizei count,
                                                  const char* const* str,
                                                  const GLint* length);
typedef void(GL_BINDING_CALL* glSignalSemaphoreEXTProc)(
    GLuint semaphore,
    GLuint numBufferBarriers,
    const GLuint* buffers,
    GLuint numTextureBarriers,
    const GLuint* textures,
    const GLenum* dstLayouts);
typedef void(GL_BINDING_CALL* glStartTilingQCOMProc)(GLuint x,
                                                     GLuint y,
                                                     GLuint width,
                                                     GLuint height,
                                                     GLbitfield preserveMask);
typedef void(GL_BINDING_CALL* glStencilFuncProc)(GLenum func,
                                                 GLint ref,
                                                 GLuint mask);
typedef void(GL_BINDING_CALL* glStencilFuncSeparateProc)(GLenum face,
                                                         GLenum func,
                                                         GLint ref,
                                                         GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskProc)(GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskSeparateProc)(GLenum face,
                                                         GLuint mask);
typedef void(GL_BINDING_CALL* glStencilOpProc)(GLenum fail,
                                               GLenum zfail,
                                               GLenum zpass);
typedef void(GL_BINDING_CALL* glStencilOpSeparateProc)(GLenum face,
                                                       GLenum fail,
                                                       GLenum zfail,
                                                       GLenum zpass);
typedef GLboolean(GL_BINDING_CALL* glTestFenceNVProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glTexBufferOESProc)(GLenum target,
                                                  GLenum internalformat,
                                                  GLuint buffer);
typedef void(GL_BINDING_CALL* glTexBufferRangeOESProc)(GLenum target,
                                                       GLenum internalformat,
                                                       GLuint buffer,
                                                       GLintptr offset,
                                                       GLsizeiptr size);
typedef void(GL_BINDING_CALL* glTexImage2DProc)(GLenum target,
                                                GLint level,
                                                GLint internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border,
                                                GLenum format,
                                                GLenum type,
                                                const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage2DExternalANGLEProc)(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type);
typedef void(GL_BINDING_CALL* glTexImage2DRobustANGLEProc)(GLenum target,
                                                           GLint level,
                                                           GLint internalformat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLint border,
                                                           GLenum format,
                                                           GLenum type,
                                                           GLsizei bufSize,
                                                           const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DProc)(GLenum target,
                                                GLint level,
                                                GLint internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLint border,
                                                GLenum format,
                                                GLenum type,
                                                const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DRobustANGLEProc)(GLenum target,
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
typedef void(GL_BINDING_CALL* glTexParameterfProc)(GLenum target,
                                                   GLenum pname,
                                                   GLfloat param);
typedef void(GL_BINDING_CALL* glTexParameterfvProc)(GLenum target,
                                                    GLenum pname,
                                                    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameterfvRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameteriProc)(GLenum target,
                                                   GLenum pname,
                                                   GLint param);
typedef void(GL_BINDING_CALL* glTexParameterivProc)(GLenum target,
                                                    GLenum pname,
                                                    const GLint* params);
typedef void(GL_BINDING_CALL* glTexParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLint* params);
typedef void(GL_BINDING_CALL* glTexStorage2DEXTProc)(GLenum target,
                                                     GLsizei levels,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height);
typedef void(GL_BINDING_CALL* glTexStorage3DProc)(GLenum target,
                                                  GLsizei levels,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth);
typedef void(GL_BINDING_CALL* glTexStorageMem2DEXTProc)(GLenum target,
                                                        GLsizei levels,
                                                        GLenum internalFormat,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLuint memory,
                                                        GLuint64 offset);
typedef void(GL_BINDING_CALL* glTexStorageMemFlags2DANGLEProc)(
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLuint memory,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void* imageCreateInfoPNext);
typedef void(GL_BINDING_CALL* glTexSubImage2DProc)(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLenum format,
                                                   GLenum type,
                                                   const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage2DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLsizei bufSize,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage3DProc)(GLenum target,
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
typedef void(GL_BINDING_CALL* glTexSubImage3DRobustANGLEProc)(
    GLenum target,
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
typedef void(GL_BINDING_CALL* glTransformFeedbackVaryingsProc)(
    GLuint program,
    GLsizei count,
    const char* const* varyings,
    GLenum bufferMode);
typedef void(GL_BINDING_CALL* glTrimMemoryANGLEProc)(GLenum trimLevel);
typedef void(GL_BINDING_CALL* glUniform1fProc)(GLint location, GLfloat x);
typedef void(GL_BINDING_CALL* glUniform1fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform1iProc)(GLint location, GLint x);
typedef void(GL_BINDING_CALL* glUniform1ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform1uiProc)(GLint location, GLuint v0);
typedef void(GL_BINDING_CALL* glUniform1uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform2fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y);
typedef void(GL_BINDING_CALL* glUniform2fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform2iProc)(GLint location,
                                               GLint x,
                                               GLint y);
typedef void(GL_BINDING_CALL* glUniform2ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform2uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1);
typedef void(GL_BINDING_CALL* glUniform2uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform3fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y,
                                               GLfloat z);
typedef void(GL_BINDING_CALL* glUniform3fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform3iProc)(GLint location,
                                               GLint x,
                                               GLint y,
                                               GLint z);
typedef void(GL_BINDING_CALL* glUniform3ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform3uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1,
                                                GLuint v2);
typedef void(GL_BINDING_CALL* glUniform3uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform4fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y,
                                               GLfloat z,
                                               GLfloat w);
typedef void(GL_BINDING_CALL* glUniform4fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform4iProc)(GLint location,
                                               GLint x,
                                               GLint y,
                                               GLint z,
                                               GLint w);
typedef void(GL_BINDING_CALL* glUniform4ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform4uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1,
                                                GLuint v2,
                                                GLuint v3);
typedef void(GL_BINDING_CALL* glUniform4uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniformBlockBindingProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLuint uniformBlockBinding);
typedef void(GL_BINDING_CALL* glUniformMatrix2fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x3fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x4fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x2fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x4fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x2fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x3fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef GLboolean(GL_BINDING_CALL* glUnmapBufferProc)(GLenum target);
typedef void(GL_BINDING_CALL* glUseProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glValidateProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glVertexAttrib1fProc)(GLuint indx, GLfloat x);
typedef void(GL_BINDING_CALL* glVertexAttrib1fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib2fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y);
typedef void(GL_BINDING_CALL* glVertexAttrib2fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib3fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y,
                                                    GLfloat z);
typedef void(GL_BINDING_CALL* glVertexAttrib3fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib4fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y,
                                                    GLfloat z,
                                                    GLfloat w);
typedef void(GL_BINDING_CALL* glVertexAttrib4fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttribDivisorANGLEProc)(GLuint index,
                                                              GLuint divisor);
typedef void(GL_BINDING_CALL* glVertexAttribI4iProc)(GLuint indx,
                                                     GLint x,
                                                     GLint y,
                                                     GLint z,
                                                     GLint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4ivProc)(GLuint indx,
                                                      const GLint* values);
typedef void(GL_BINDING_CALL* glVertexAttribI4uiProc)(GLuint indx,
                                                      GLuint x,
                                                      GLuint y,
                                                      GLuint z,
                                                      GLuint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4uivProc)(GLuint indx,
                                                       const GLuint* values);
typedef void(GL_BINDING_CALL* glVertexAttribIPointerProc)(GLuint indx,
                                                          GLint size,
                                                          GLenum type,
                                                          GLsizei stride,
                                                          const void* ptr);
typedef void(GL_BINDING_CALL* glVertexAttribPointerProc)(GLuint indx,
                                                         GLint size,
                                                         GLenum type,
                                                         GLboolean normalized,
                                                         GLsizei stride,
                                                         const void* ptr);
typedef void(GL_BINDING_CALL* glViewportProc)(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height);
typedef void(GL_BINDING_CALL* glWaitSemaphoreEXTProc)(GLuint semaphore,
                                                      GLuint numBufferBarriers,
                                                      const GLuint* buffers,
                                                      GLuint numTextureBarriers,
                                                      const GLuint* textures,
                                                      const GLenum* srcLayouts);
typedef void(GL_BINDING_CALL* glWaitSyncProc)(GLsync sync,
                                              GLbitfield flags,
                                              GLuint64 timeout);
typedef void(GL_BINDING_CALL* glWindowRectanglesEXTProc)(GLenum mode,
                                                         GLsizei n,
                                                         const GLint* box);
typedef void(GL_BINDING_CALL* glAcquireTexturesANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint numTextures,
    const GLuint* textures,
    const GLenum* layouts);
typedef void(GL_BINDING_CALL* glActiveTextureContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum texture);
typedef void(GL_BINDING_CALL* glAttachShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint shader);
typedef void(GL_BINDING_CALL* glBeginPixelLocalStorageANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLenum* loadops);
typedef void(GL_BINDING_CALL* glBeginQueryContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint id);
typedef void(GL_BINDING_CALL* glBeginTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum primitiveMode);
typedef void(GL_BINDING_CALL* glBindAttribLocationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint index,
    const char* name);
typedef void(GL_BINDING_CALL* glBindBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferBaseContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferRangeContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLuint buffer,
    GLintptr offset,
    GLsizeiptr size);
typedef void(GL_BINDING_CALL* glBindFragDataLocationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint colorNumber,
    const char* name);
typedef void(GL_BINDING_CALL* glBindFragDataLocationIndexedContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name);
typedef void(GL_BINDING_CALL* glBindFramebufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint framebuffer);
typedef void(GL_BINDING_CALL* glBindRenderbufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glBindSamplerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint unit,
    GLuint sampler);
typedef void(GL_BINDING_CALL* glBindTextureContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint texture);
typedef void(GL_BINDING_CALL* glBindTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint id);
typedef void(GL_BINDING_CALL* glBindUniformLocationCHROMIUMContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    const char* name);
typedef void(GL_BINDING_CALL* glBindVertexArrayOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint array);
typedef void(GL_BINDING_CALL* glBlendBarrierKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glBlendColorContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampf red,
    GLclampf green,
    GLclampf blue,
    GLclampf alpha);
typedef void(GL_BINDING_CALL* glBlendEquationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode);
typedef void(GL_BINDING_CALL* glBlendEquationiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buf,
    GLenum mode);
typedef void(GL_BINDING_CALL* glBlendEquationSeparateContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum modeRGB,
    GLenum modeAlpha);
typedef void(GL_BINDING_CALL* glBlendEquationSeparateiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buf,
    GLenum modeRGB,
    GLenum modeAlpha);
typedef void(GL_BINDING_CALL* glBlendFuncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum sfactor,
    GLenum dfactor);
typedef void(GL_BINDING_CALL* glBlendFunciOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buf,
    GLenum sfactor,
    GLenum dfactor);
typedef void(GL_BINDING_CALL* glBlendFuncSeparateContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum srcRGB,
    GLenum dstRGB,
    GLenum srcAlpha,
    GLenum dstAlpha);
typedef void(GL_BINDING_CALL* glBlendFuncSeparateiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buf,
    GLenum srcRGB,
    GLenum dstRGB,
    GLenum srcAlpha,
    GLenum dstAlpha);
typedef void(GL_BINDING_CALL* glBlitFramebufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter);
typedef void(GL_BINDING_CALL* glBlobCacheCallbacksANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLSETBLOBPROCANGLE set,
    GLGETBLOBPROCANGLE get,
    const void* userData);
typedef void(GL_BINDING_CALL* glBufferDataContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLenum usage);
typedef void(GL_BINDING_CALL* glBufferSubDataContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data);
typedef GLenum(GL_BINDING_CALL* glCheckFramebufferStatusEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target);
typedef void(GL_BINDING_CALL* glClearContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                       GLeglContextANGLE ctx,
                                                       GLbitfield mask);
typedef void(GL_BINDING_CALL* glClearBufferfiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum buffer,
    GLint drawbuffer,
    const GLfloat depth,
    GLint stencil);
typedef void(GL_BINDING_CALL* glClearBufferfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum buffer,
    GLint drawbuffer,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glClearBufferivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum buffer,
    GLint drawbuffer,
    const GLint* value);
typedef void(GL_BINDING_CALL* glClearBufferuivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum buffer,
    GLint drawbuffer,
    const GLuint* value);
typedef void(GL_BINDING_CALL* glClearColorContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampf red,
    GLclampf green,
    GLclampf blue,
    GLclampf alpha);
typedef void(GL_BINDING_CALL* glClearDepthContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampd depth);
typedef void(GL_BINDING_CALL* glClearDepthfContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampf depth);
typedef void(GL_BINDING_CALL* glClearStencilContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint s);
typedef void(GL_BINDING_CALL* glClearTexImageContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint texture,
    GLint level,
    GLenum format,
    GLenum type,
    const GLvoid* data);
typedef void(GL_BINDING_CALL* glClearTexSubImageContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint texture,
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
typedef GLenum(GL_BINDING_CALL* glClientWaitSyncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsync sync,
    GLbitfield flags,
    GLuint64 timeout);
typedef void(GL_BINDING_CALL* glClipControlEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum origin,
    GLenum depth);
typedef void(GL_BINDING_CALL* glColorMaskContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLboolean red,
    GLboolean green,
    GLboolean blue,
    GLboolean alpha);
typedef void(GL_BINDING_CALL* glColorMaskiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buf,
    GLboolean red,
    GLboolean green,
    GLboolean blue,
    GLboolean alpha);
typedef void(GL_BINDING_CALL* glCompileShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader);
typedef void(GL_BINDING_CALL* glCompressedTexImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei imageSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexImage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei imageSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei imageSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
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
typedef void(GL_BINDING_CALL* glCopyBufferSubDataContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum readTarget,
    GLenum writeTarget,
    GLintptr readOffset,
    GLintptr writeOffset,
    GLsizeiptr size);
typedef void(GL_BINDING_CALL* glCopySubTextureCHROMIUMContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
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
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCopyTexImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border);
typedef void(GL_BINDING_CALL* glCopyTexSubImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTexSubImage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTextureCHROMIUMContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint internalFormat,
    GLenum destType,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCreateMemoryObjectsEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* memoryObjects);
typedef GLuint(GL_BINDING_CALL* glCreateProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef GLuint(GL_BINDING_CALL* glCreateShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum type);
typedef void(GL_BINDING_CALL* glCullFaceContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                          GLeglContextANGLE ctx,
                                                          GLenum mode);
typedef void(GL_BINDING_CALL* glDebugMessageCallbackKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLDEBUGPROC callback,
    const void* userParam);
typedef void(GL_BINDING_CALL* glDebugMessageControlKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum source,
    GLenum type,
    GLenum severity,
    GLsizei count,
    const GLuint* ids,
    GLboolean enabled);
typedef void(GL_BINDING_CALL* glDebugMessageInsertKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const char* buf);
typedef void(GL_BINDING_CALL* glDeleteBuffersARBContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* buffers);
typedef void(GL_BINDING_CALL* glDeleteFencesNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* fences);
typedef void(GL_BINDING_CALL* glDeleteFramebuffersEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* framebuffers);
typedef void(GL_BINDING_CALL* glDeleteMemoryObjectsEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* memoryObjects);
typedef void(GL_BINDING_CALL* glDeleteProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program);
typedef void(GL_BINDING_CALL* glDeleteQueriesContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteRenderbuffersEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glDeleteSamplersContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* samplers);
typedef void(GL_BINDING_CALL* glDeleteSemaphoresEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* semaphores);
typedef void(GL_BINDING_CALL* glDeleteShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader);
typedef void(GL_BINDING_CALL* glDeleteSyncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsync sync);
typedef void(GL_BINDING_CALL* glDeleteTexturesContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* textures);
typedef void(GL_BINDING_CALL* glDeleteTransformFeedbacksContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteVertexArraysOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* arrays);
typedef void(GL_BINDING_CALL* glDepthFuncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum func);
typedef void(GL_BINDING_CALL* glDepthMaskContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLboolean flag);
typedef void(GL_BINDING_CALL* glDepthRangeContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampd zNear,
    GLclampd zFar);
typedef void(GL_BINDING_CALL* glDepthRangefContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampf zNear,
    GLclampf zFar);
typedef void(GL_BINDING_CALL* glDetachShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint shader);
typedef void(GL_BINDING_CALL* glDisableContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                         GLeglContextANGLE ctx,
                                                         GLenum cap);
typedef void(GL_BINDING_CALL* glDisableiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index);
typedef void(GL_BINDING_CALL* glDisableVertexAttribArrayContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index);
typedef void(GL_BINDING_CALL* glDiscardFramebufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glDrawArraysContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLint first,
    GLsizei count);
typedef void(GL_BINDING_CALL* glDrawArraysInstancedANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount);
typedef void(
    GL_BINDING_CALL* glDrawArraysInstancedBaseInstanceANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance);
typedef void(GL_BINDING_CALL* glDrawBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode);
typedef void(GL_BINDING_CALL* glDrawBuffersARBContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLenum* bufs);
typedef void(GL_BINDING_CALL* glDrawElementsContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices);
typedef void(GL_BINDING_CALL* glDrawElementsInstancedANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount);
typedef void(
    GL_BINDING_CALL*
        glDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint baseVertex,
    GLuint baseInstance);
typedef void(GL_BINDING_CALL* glDrawRangeElementsContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLuint start,
    GLuint end,
    GLsizei count,
    GLenum type,
    const void* indices);
typedef void(
    GL_BINDING_CALL* glEGLImageTargetRenderbufferStorageOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEGLImageTargetTexStorageEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLeglImageOES image,
    const GLint* attrib_list);
typedef void(GL_BINDING_CALL* glEGLImageTargetTexture2DOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEnableContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                        GLeglContextANGLE ctx,
                                                        GLenum cap);
typedef void(GL_BINDING_CALL* glEnableiOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index);
typedef void(GL_BINDING_CALL* glEnableVertexAttribArrayContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index);
typedef void(GL_BINDING_CALL* glEndPixelLocalStorageANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLenum* storeops);
typedef void(
    GL_BINDING_CALL* glEndPixelLocalStorageImplicitANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glEndQueryContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                          GLeglContextANGLE ctx,
                                                          GLenum target);
typedef void(GL_BINDING_CALL* glEndTilingQCOMContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLbitfield preserveMask);
typedef void(GL_BINDING_CALL* glEndTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef GLsync(GL_BINDING_CALL* glFenceSyncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum condition,
    GLbitfield flags);
typedef void(GL_BINDING_CALL* glFinishContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                        GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glFinishFenceNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint fence);
typedef void(GL_BINDING_CALL* glFlushContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                       GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glFlushMappedBufferRangeContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLintptr offset,
    GLsizeiptr length);
typedef void(GL_BINDING_CALL*
                 glFramebufferMemorylessPixelLocalStorageANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    GLenum internalformat,
    GLbitfield usage);
typedef void(GL_BINDING_CALL* glFramebufferParameteriMESAContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint param);
typedef void(
    GL_BINDING_CALL* glFramebufferPixelLocalClearValuefvANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    const GLfloat* value);
typedef void(
    GL_BINDING_CALL* glFramebufferPixelLocalClearValueivANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    const GLint* value);
typedef void(
    GL_BINDING_CALL* glFramebufferPixelLocalClearValueuivANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    const GLuint* value);
typedef void(GL_BINDING_CALL*
                 glFramebufferPixelLocalStorageInterruptANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL*
                 glFramebufferPixelLocalStorageRestoreANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glFramebufferRenderbufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glFramebufferTexture2DEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level);
typedef void(
    GL_BINDING_CALL* glFramebufferTexture2DMultisampleEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
typedef void(GL_BINDING_CALL* glFramebufferTextureLayerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint layer);
typedef void(GL_BINDING_CALL* glFramebufferTextureMultiviewOVRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
typedef void(GL_BINDING_CALL*
                 glFramebufferTexturePixelLocalStorageANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer,
    GLbitfield usage);
typedef void(GL_BINDING_CALL* glFrontFaceContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode);
typedef void(GL_BINDING_CALL* glGenBuffersARBContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* buffers);
typedef void(GL_BINDING_CALL* glGenerateMipmapEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target);
typedef void(GL_BINDING_CALL* glGenFencesNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* fences);
typedef void(GL_BINDING_CALL* glGenFramebuffersEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* framebuffers);
typedef void(GL_BINDING_CALL* glGenQueriesContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* ids);
typedef void(GL_BINDING_CALL* glGenRenderbuffersEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glGenSamplersContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* samplers);
typedef void(GL_BINDING_CALL* glGenSemaphoresEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* semaphores);
typedef void(GL_BINDING_CALL* glGenTexturesContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* textures);
typedef void(GL_BINDING_CALL* glGenTransformFeedbacksContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* ids);
typedef void(GL_BINDING_CALL* glGenVertexArraysOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    GLuint* arrays);
typedef void(GL_BINDING_CALL* glGetActiveAttribContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint index,
    GLsizei bufsize,
    GLsizei* length,
    GLint* size,
    GLenum* type,
    char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint index,
    GLsizei bufsize,
    GLsizei* length,
    GLint* size,
    GLenum* type,
    char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetActiveUniformBlockivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockNameContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint uniformBlockIndex,
    GLsizei bufSize,
    GLsizei* length,
    char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetActiveUniformsivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei uniformCount,
    const GLuint* uniformIndices,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetAttachedShadersContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei maxcount,
    GLsizei* count,
    GLuint* shaders);
typedef GLint(GL_BINDING_CALL* glGetAttribLocationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    const char* name);
typedef void(GL_BINDING_CALL* glGetBooleanvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLboolean* params);
typedef void(GL_BINDING_CALL* glGetBooleanvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLboolean* data);
typedef void(
    GL_BINDING_CALL* glGetBufferParameteri64vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetBufferParameterivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetBufferParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetBufferPointervRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** params);
typedef GLuint(GL_BINDING_CALL* glGetDebugMessageLogKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint count,
    GLsizei bufSize,
    GLenum* sources,
    GLenum* types,
    GLuint* ids,
    GLenum* severities,
    GLsizei* lengths,
    char* messageLog);
typedef GLenum(GL_BINDING_CALL* glGetErrorContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glGetFenceivNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint fence,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetFloatvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetFloatvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* data);
typedef GLint(GL_BINDING_CALL* glGetFragDataIndexContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    const char* name);
typedef GLint(GL_BINDING_CALL* glGetFragDataLocationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    const char* name);
typedef void(
    GL_BINDING_CALL* glGetFramebufferAttachmentParameterivEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL*
        glGetFramebufferAttachmentParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(
    GL_BINDING_CALL*
        glGetFramebufferPixelLocalStorageParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLfloat* params);
typedef void(
    GL_BINDING_CALL*
        glGetFramebufferPixelLocalStorageParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLint* params);
typedef void(
    GL_BINDING_CALL*
        glGetFramebufferPixelLocalStorageParameteruivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint plane,
    GLenum pname,
    GLsizei paramCount,
    GLsizei* length,
    GLuint* params);
typedef GLenum(GL_BINDING_CALL* glGetGraphicsResetStatusARBContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glGetInteger64i_vContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64i_vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64vContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetInteger64vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegervContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetIntegervRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* data);
typedef void(GL_BINDING_CALL* glGetInternalformativContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLenum pname,
    GLsizei bufSize,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetInternalformativRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetInternalformatSampleivNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLsizei samples,
    GLenum pname,
    GLsizei bufSize,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetMultisamplefvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* val);
typedef void(GL_BINDING_CALL* glGetObjectLabelKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum identifier,
    GLuint name,
    GLsizei bufSize,
    GLsizei* length,
    char* label);
typedef void(GL_BINDING_CALL* glGetObjectPtrLabelKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    void* ptr,
    GLsizei bufSize,
    GLsizei* length,
    char* label);
typedef void(GL_BINDING_CALL* glGetPointervKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    void** params);
typedef void(GL_BINDING_CALL* glGetProgramBinaryContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei bufSize,
    GLsizei* length,
    GLenum* binaryFormat,
    GLvoid* binary);
typedef void(GL_BINDING_CALL* glGetProgramInfoLogContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei bufsize,
    GLsizei* length,
    char* infolog);
typedef void(GL_BINDING_CALL* glGetProgramivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetRenderbufferParameterivEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetRenderbufferParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLfloat* params);
typedef void(
    GL_BINDING_CALL* glGetSamplerParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetSamplerParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderInfoLogContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLsizei bufsize,
    GLsizei* length,
    char* infolog);
typedef void(GL_BINDING_CALL* glGetShaderivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderPrecisionFormatContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision);
typedef void(GL_BINDING_CALL* glGetShaderSourceContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLsizei bufsize,
    GLsizei* length,
    char* source);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum name);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum name,
    GLuint index);
typedef void(GL_BINDING_CALL* glGetSyncivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsync sync,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* values);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterfvANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum pname,
    GLfloat* params);
typedef void(
    GL_BINDING_CALL* glGetTexLevelParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterivANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetTexLevelParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTransformFeedbackVaryingContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLsizei* size,
    GLenum* type,
    char* name);
typedef void(GL_BINDING_CALL* glGetTranslatedShaderSourceANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLsizei bufsize,
    GLsizei* length,
    char* source);
typedef GLuint(GL_BINDING_CALL* glGetUniformBlockIndexContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    const char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetUniformfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformIndicesContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei uniformCount,
    const char* const* uniformNames,
    GLuint* uniformIndices);
typedef void(GL_BINDING_CALL* glGetUniformivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetUniformivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef GLint(GL_BINDING_CALL* glGetUniformLocationContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    const char* name);
typedef void(GL_BINDING_CALL* glGetUniformuivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetUniformuivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLint location,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIuivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribPointervContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    void** pointer);
typedef void(
    GL_BINDING_CALL* glGetVertexAttribPointervRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** pointer);
typedef void(GL_BINDING_CALL* glHintContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                      GLeglContextANGLE ctx,
                                                      GLenum target,
                                                      GLenum mode);
typedef void(GL_BINDING_CALL* glImportMemoryFdEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint memory,
    GLuint64 size,
    GLenum handleType,
    GLint fd);
typedef void(GL_BINDING_CALL* glImportMemoryWin32HandleEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint memory,
    GLuint64 size,
    GLenum handleType,
    void* handle);
typedef void(GL_BINDING_CALL* glImportMemoryZirconHandleANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint memory,
    GLuint64 size,
    GLenum handleType,
    GLuint handle);
typedef void(GL_BINDING_CALL* glImportSemaphoreFdEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint semaphore,
    GLenum handleType,
    GLint fd);
typedef void(GL_BINDING_CALL* glImportSemaphoreWin32HandleEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint semaphore,
    GLenum handleType,
    void* handle);
typedef void(
    GL_BINDING_CALL* glImportSemaphoreZirconHandleANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint semaphore,
    GLenum handleType,
    GLuint handle);
typedef void(GL_BINDING_CALL* glInsertEventMarkerEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei length,
    const char* marker);
typedef void(GL_BINDING_CALL* glInvalidateFramebufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glInvalidateSubFramebufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments,
    GLint x,
    GLint y,
    GLint width,
    GLint height);
typedef void(GL_BINDING_CALL* glInvalidateTextureANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target);
typedef GLboolean(GL_BINDING_CALL* glIsBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint buffer);
typedef GLboolean(GL_BINDING_CALL* glIsEnabledContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum cap);
typedef GLboolean(GL_BINDING_CALL* glIsEnablediOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLuint index);
typedef GLboolean(GL_BINDING_CALL* glIsFenceNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint fence);
typedef GLboolean(GL_BINDING_CALL* glIsFramebufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint framebuffer);
typedef GLboolean(GL_BINDING_CALL* glIsProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program);
typedef GLboolean(GL_BINDING_CALL* glIsQueryContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint query);
typedef GLboolean(GL_BINDING_CALL* glIsRenderbufferEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint renderbuffer);
typedef GLboolean(GL_BINDING_CALL* glIsSamplerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler);
typedef GLboolean(GL_BINDING_CALL* glIsShaderContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader);
typedef GLboolean(GL_BINDING_CALL* glIsSyncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsync sync);
typedef GLboolean(GL_BINDING_CALL* glIsTextureContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint texture);
typedef GLboolean(GL_BINDING_CALL* glIsTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id);
typedef GLboolean(GL_BINDING_CALL* glIsVertexArrayOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint array);
typedef void(GL_BINDING_CALL* glLineWidthContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLfloat width);
typedef void(GL_BINDING_CALL* glLinkProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program);
typedef void*(GL_BINDING_CALL* glMapBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum access);
typedef void*(GL_BINDING_CALL* glMapBufferRangeContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLintptr offset,
    GLsizeiptr length,
    GLbitfield access);
typedef void(GL_BINDING_CALL* glMaxShaderCompilerThreadsKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint count);
typedef void(GL_BINDING_CALL* glMemoryObjectParameterivEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint memoryObject,
    GLenum pname,
    const GLint* param);
typedef void(GL_BINDING_CALL* glMultiDrawArraysANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawArraysInstancedANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL*
                 glMultiDrawArraysInstancedBaseInstanceANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    const GLuint* baseInstances,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glMultiDrawElementsANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    GLsizei drawcount);
typedef void(
    GL_BINDING_CALL* glMultiDrawElementsInstancedANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    GLsizei drawcount);
typedef void(
    GL_BINDING_CALL*
        glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    const GLint* baseVertices,
    const GLuint* baseInstances,
    GLsizei drawcount);
typedef void(GL_BINDING_CALL* glObjectLabelKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum identifier,
    GLuint name,
    GLsizei length,
    const char* label);
typedef void(GL_BINDING_CALL* glObjectPtrLabelKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    void* ptr,
    GLsizei length,
    const char* label);
typedef void(GL_BINDING_CALL* glPatchParameteriOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLint value);
typedef void(GL_BINDING_CALL* glPauseTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glPixelLocalStorageBarrierANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glPixelStoreiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLint param);
typedef void(GL_BINDING_CALL* glPointParameteriContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum pname,
    GLint param);
typedef void(GL_BINDING_CALL* glPolygonModeContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum face,
    GLenum mode);
typedef void(GL_BINDING_CALL* glPolygonModeANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum face,
    GLenum mode);
typedef void(GL_BINDING_CALL* glPolygonOffsetContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLfloat factor,
    GLfloat units);
typedef void(GL_BINDING_CALL* glPolygonOffsetClampEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLfloat factor,
    GLfloat units,
    GLfloat clamp);
typedef void(GL_BINDING_CALL* glPopDebugGroupKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glPopGroupMarkerEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glPrimitiveRestartIndexContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index);
typedef void(GL_BINDING_CALL* glProgramBinaryContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLenum binaryFormat,
    const GLvoid* binary,
    GLsizei length);
typedef void(GL_BINDING_CALL* glProgramParameteriContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLenum pname,
    GLint value);
typedef void(GL_BINDING_CALL* glProvokingVertexANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum provokeMode);
typedef void(GL_BINDING_CALL* glPushDebugGroupKHRContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum source,
    GLuint id,
    GLsizei length,
    const char* message);
typedef void(GL_BINDING_CALL* glPushGroupMarkerEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei length,
    const char* marker);
typedef void(GL_BINDING_CALL* glQueryCounterContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint id,
    GLenum target);
typedef void(GL_BINDING_CALL* glReadBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum src);
typedef void(GL_BINDING_CALL* glReadPixelsContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    void* pixels);
typedef void(GL_BINDING_CALL* glReadPixelsRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint x,
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
typedef void(GL_BINDING_CALL* glReleaseShaderCompilerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glReleaseTexturesANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint numTextures,
    const GLuint* textures,
    GLenum* layouts);
typedef void(GL_BINDING_CALL* glRenderbufferStorageEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL*
                 glRenderbufferStorageMultisampleAdvancedAMDContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(
    GL_BINDING_CALL* glRenderbufferStorageMultisampleEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRequestExtensionANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    const char* name);
typedef void(GL_BINDING_CALL* glResumeTransformFeedbackContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx);
typedef void(GL_BINDING_CALL* glSampleCoverageContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLclampf value,
    GLboolean invert);
typedef void(GL_BINDING_CALL* glSamplerParameterfContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLfloat param);
typedef void(GL_BINDING_CALL* glSamplerParameterfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    const GLfloat* params);
typedef void(GL_BINDING_CALL* glSamplerParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* param);
typedef void(GL_BINDING_CALL* glSamplerParameteriContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLint param);
typedef void(GL_BINDING_CALL* glSamplerParameterivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    const GLint* params);
typedef void(GL_BINDING_CALL* glSamplerParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLint* param);
typedef void(GL_BINDING_CALL* glScissorContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                         GLeglContextANGLE ctx,
                                                         GLint x,
                                                         GLint y,
                                                         GLsizei width,
                                                         GLsizei height);
typedef void(GL_BINDING_CALL* glSetFenceNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint fence,
    GLenum condition);
typedef void(GL_BINDING_CALL* glShaderBinaryContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLsizei n,
    const GLuint* shaders,
    GLenum binaryformat,
    const void* binary,
    GLsizei length);
typedef void(GL_BINDING_CALL* glShaderSourceContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint shader,
    GLsizei count,
    const char* const* str,
    const GLint* length);
typedef void(GL_BINDING_CALL* glSignalSemaphoreEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint semaphore,
    GLuint numBufferBarriers,
    const GLuint* buffers,
    GLuint numTextureBarriers,
    const GLuint* textures,
    const GLenum* dstLayouts);
typedef void(GL_BINDING_CALL* glStartTilingQCOMContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint x,
    GLuint y,
    GLuint width,
    GLuint height,
    GLbitfield preserveMask);
typedef void(GL_BINDING_CALL* glStencilFuncContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum func,
    GLint ref,
    GLuint mask);
typedef void(GL_BINDING_CALL* glStencilFuncSeparateContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum face,
    GLenum func,
    GLint ref,
    GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskSeparateContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum face,
    GLuint mask);
typedef void(GL_BINDING_CALL* glStencilOpContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum fail,
    GLenum zfail,
    GLenum zpass);
typedef void(GL_BINDING_CALL* glStencilOpSeparateContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum face,
    GLenum fail,
    GLenum zfail,
    GLenum zpass);
typedef GLboolean(GL_BINDING_CALL* glTestFenceNVContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint fence);
typedef void(GL_BINDING_CALL* glTexBufferOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLuint buffer);
typedef void(GL_BINDING_CALL* glTexBufferRangeOESContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum internalformat,
    GLuint buffer,
    GLintptr offset,
    GLsizeiptr size);
typedef void(GL_BINDING_CALL* glTexImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage2DExternalANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type);
typedef void(GL_BINDING_CALL* glTexImage2DRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    GLsizei bufSize,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
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
typedef void(GL_BINDING_CALL* glTexParameterfContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLfloat param);
typedef void(GL_BINDING_CALL* glTexParameterfvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameterfvRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameteriContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLint param);
typedef void(GL_BINDING_CALL* glTexParameterivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    const GLint* params);
typedef void(GL_BINDING_CALL* glTexParameterivRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLint* params);
typedef void(GL_BINDING_CALL* glTexStorage2DEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei levels,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glTexStorage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei levels,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth);
typedef void(GL_BINDING_CALL* glTexStorageMem2DEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLuint memory,
    GLuint64 offset);
typedef void(GL_BINDING_CALL* glTexStorageMemFlags2DANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLuint memory,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void* imageCreateInfoPNext);
typedef void(GL_BINDING_CALL* glTexSubImage2DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage2DRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLsizei bufSize,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage3DContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
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
typedef void(GL_BINDING_CALL* glTexSubImage3DRobustANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target,
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
typedef void(GL_BINDING_CALL* glTransformFeedbackVaryingsContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLsizei count,
    const char* const* varyings,
    GLenum bufferMode);
typedef void(GL_BINDING_CALL* glTrimMemoryANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum trimLevel);
typedef void(GL_BINDING_CALL* glUniform1fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLfloat x);
typedef void(GL_BINDING_CALL* glUniform1fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform1iContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLint x);
typedef void(GL_BINDING_CALL* glUniform1ivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLint* v);
typedef void(GL_BINDING_CALL* glUniform1uiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLuint v0);
typedef void(GL_BINDING_CALL* glUniform1uivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform2fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLfloat x,
    GLfloat y);
typedef void(GL_BINDING_CALL* glUniform2fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform2iContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLint x,
    GLint y);
typedef void(GL_BINDING_CALL* glUniform2ivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLint* v);
typedef void(GL_BINDING_CALL* glUniform2uiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLuint v0,
    GLuint v1);
typedef void(GL_BINDING_CALL* glUniform2uivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform3fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLfloat x,
    GLfloat y,
    GLfloat z);
typedef void(GL_BINDING_CALL* glUniform3fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform3iContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLint x,
    GLint y,
    GLint z);
typedef void(GL_BINDING_CALL* glUniform3ivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLint* v);
typedef void(GL_BINDING_CALL* glUniform3uiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLuint v0,
    GLuint v1,
    GLuint v2);
typedef void(GL_BINDING_CALL* glUniform3uivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform4fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLfloat x,
    GLfloat y,
    GLfloat z,
    GLfloat w);
typedef void(GL_BINDING_CALL* glUniform4fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform4iContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLint x,
    GLint y,
    GLint z,
    GLint w);
typedef void(GL_BINDING_CALL* glUniform4ivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLint* v);
typedef void(GL_BINDING_CALL* glUniform4uiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLuint v0,
    GLuint v1,
    GLuint v2,
    GLuint v3);
typedef void(GL_BINDING_CALL* glUniform4uivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    const GLuint* v);
typedef void(GL_BINDING_CALL* glUniformBlockBindingContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program,
    GLuint uniformBlockIndex,
    GLuint uniformBlockBinding);
typedef void(GL_BINDING_CALL* glUniformMatrix2fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x3fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x4fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x2fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x4fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x2fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x3fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value);
typedef GLboolean(GL_BINDING_CALL* glUnmapBufferContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum target);
typedef void(GL_BINDING_CALL* glUseProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program);
typedef void(GL_BINDING_CALL* glValidateProgramContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint program);
typedef void(GL_BINDING_CALL* glVertexAttrib1fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLfloat x);
typedef void(GL_BINDING_CALL* glVertexAttrib1fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib2fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLfloat x,
    GLfloat y);
typedef void(GL_BINDING_CALL* glVertexAttrib2fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib3fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLfloat x,
    GLfloat y,
    GLfloat z);
typedef void(GL_BINDING_CALL* glVertexAttrib3fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib4fContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLfloat x,
    GLfloat y,
    GLfloat z,
    GLfloat w);
typedef void(GL_BINDING_CALL* glVertexAttrib4fvContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttribDivisorANGLEContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint index,
    GLuint divisor);
typedef void(GL_BINDING_CALL* glVertexAttribI4iContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLint x,
    GLint y,
    GLint z,
    GLint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4ivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLint* values);
typedef void(GL_BINDING_CALL* glVertexAttribI4uiContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLuint x,
    GLuint y,
    GLuint z,
    GLuint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4uivContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    const GLuint* values);
typedef void(GL_BINDING_CALL* glVertexAttribIPointerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLint size,
    GLenum type,
    GLsizei stride,
    const void* ptr);
typedef void(GL_BINDING_CALL* glVertexAttribPointerContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint indx,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr);
typedef void(GL_BINDING_CALL* glViewportContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                          GLeglContextANGLE ctx,
                                                          GLint x,
                                                          GLint y,
                                                          GLsizei width,
                                                          GLsizei height);
typedef void(GL_BINDING_CALL* glWaitSemaphoreEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLuint semaphore,
    GLuint numBufferBarriers,
    const GLuint* buffers,
    GLuint numTextureBarriers,
    const GLuint* textures,
    const GLenum* srcLayouts);
typedef void(GL_BINDING_CALL* glWaitSyncContextANGLEProc)(GLeglDisplayANGLE dpy,
                                                          GLeglContextANGLE ctx,
                                                          GLsync sync,
                                                          GLbitfield flags,
                                                          GLuint64 timeout);
typedef void(GL_BINDING_CALL* glWindowRectanglesEXTContextANGLEProc)(
    GLeglDisplayANGLE dpy,
    GLeglContextANGLE ctx,
    GLenum mode,
    GLsizei n,
    const GLint* box);

struct ExtensionsGL {
  bool b_GL_AMD_framebuffer_multisample_advanced;
  bool b_GL_ANGLE_base_vertex_base_instance;
  bool b_GL_ANGLE_blob_cache;
  bool b_GL_ANGLE_explicit_context;
  bool b_GL_ANGLE_framebuffer_blit;
  bool b_GL_ANGLE_framebuffer_multisample;
  bool b_GL_ANGLE_get_tex_level_parameter;
  bool b_GL_ANGLE_instanced_arrays;
  bool b_GL_ANGLE_memory_object_flags;
  bool b_GL_ANGLE_memory_object_fuchsia;
  bool b_GL_ANGLE_multi_draw;
  bool b_GL_ANGLE_polygon_mode;
  bool b_GL_ANGLE_provoking_vertex;
  bool b_GL_ANGLE_renderability_validation;
  bool b_GL_ANGLE_request_extension;
  bool b_GL_ANGLE_robust_client_memory;
  bool b_GL_ANGLE_robust_resource_initialization;
  bool b_GL_ANGLE_semaphore_fuchsia;
  bool b_GL_ANGLE_shader_pixel_local_storage;
  bool b_GL_ANGLE_texture_external_update;
  bool b_GL_ANGLE_translated_shader_source;
  bool b_GL_ANGLE_trim_memory;
  bool b_GL_ANGLE_vulkan_image;
  bool b_GL_ANGLE_webgl_compatibility;
  bool b_GL_CHROMIUM_bind_uniform_location;
  bool b_GL_CHROMIUM_copy_texture;
  bool b_GL_CHROMIUM_gles_depth_binding_hack;
  bool b_GL_CHROMIUM_glgetstringi_hack;
  bool b_GL_EXT_EGL_image_array;
  bool b_GL_EXT_EGL_image_storage;
  bool b_GL_EXT_base_instance;
  bool b_GL_EXT_blend_func_extended;
  bool b_GL_EXT_clear_texture;
  bool b_GL_EXT_clip_control;
  bool b_GL_EXT_debug_marker;
  bool b_GL_EXT_discard_framebuffer;
  bool b_GL_EXT_disjoint_timer_query;
  bool b_GL_EXT_draw_buffers;
  bool b_GL_EXT_framebuffer_multisample;
  bool b_GL_EXT_instanced_arrays;
  bool b_GL_EXT_map_buffer_range;
  bool b_GL_EXT_memory_object;
  bool b_GL_EXT_memory_object_fd;
  bool b_GL_EXT_memory_object_win32;
  bool b_GL_EXT_multisampled_render_to_texture;
  bool b_GL_EXT_occlusion_query_boolean;
  bool b_GL_EXT_polygon_offset_clamp;
  bool b_GL_EXT_robustness;
  bool b_GL_EXT_semaphore;
  bool b_GL_EXT_semaphore_fd;
  bool b_GL_EXT_semaphore_win32;
  bool b_GL_EXT_texture_buffer;
  bool b_GL_EXT_texture_format_BGRA8888;
  bool b_GL_EXT_texture_storage;
  bool b_GL_EXT_texture_swizzle;
  bool b_GL_EXT_unpack_subimage;
  bool b_GL_EXT_window_rectangles;
  bool b_GL_IMG_multisampled_render_to_texture;
  bool b_GL_KHR_blend_equation_advanced;
  bool b_GL_KHR_debug;
  bool b_GL_KHR_parallel_shader_compile;
  bool b_GL_KHR_robustness;
  bool b_GL_MESA_framebuffer_flip_y;
  bool b_GL_MESA_sampler_objects;
  bool b_GL_NV_blend_equation_advanced;
  bool b_GL_NV_fence;
  bool b_GL_NV_framebuffer_blit;
  bool b_GL_NV_internalformat_sample_query;
  bool b_GL_NV_pack_subimage;
  bool b_GL_OES_EGL_image;
  bool b_GL_OES_draw_buffers_indexed;
  bool b_GL_OES_get_program_binary;
  bool b_GL_OES_mapbuffer;
  bool b_GL_OES_tessellation_shader;
  bool b_GL_OES_texture_buffer;
  bool b_GL_OES_vertex_array_object;
  bool b_GL_OVR_multiview;
  bool b_GL_OVR_multiview2;
  bool b_GL_QCOM_tiled_rendering;
};

struct ProcsGL {
  glAcquireTexturesANGLEProc glAcquireTexturesANGLEFn;
  glActiveTextureProc glActiveTextureFn;
  glAttachShaderProc glAttachShaderFn;
  glBeginPixelLocalStorageANGLEProc glBeginPixelLocalStorageANGLEFn;
  glBeginQueryProc glBeginQueryFn;
  glBeginTransformFeedbackProc glBeginTransformFeedbackFn;
  glBindAttribLocationProc glBindAttribLocationFn;
  glBindBufferProc glBindBufferFn;
  glBindBufferBaseProc glBindBufferBaseFn;
  glBindBufferRangeProc glBindBufferRangeFn;
  glBindFragDataLocationProc glBindFragDataLocationFn;
  glBindFragDataLocationIndexedProc glBindFragDataLocationIndexedFn;
  glBindFramebufferEXTProc glBindFramebufferEXTFn;
  glBindRenderbufferEXTProc glBindRenderbufferEXTFn;
  glBindSamplerProc glBindSamplerFn;
  glBindTextureProc glBindTextureFn;
  glBindTransformFeedbackProc glBindTransformFeedbackFn;
  glBindUniformLocationCHROMIUMProc glBindUniformLocationCHROMIUMFn;
  glBindVertexArrayOESProc glBindVertexArrayOESFn;
  glBlendBarrierKHRProc glBlendBarrierKHRFn;
  glBlendColorProc glBlendColorFn;
  glBlendEquationProc glBlendEquationFn;
  glBlendEquationiOESProc glBlendEquationiOESFn;
  glBlendEquationSeparateProc glBlendEquationSeparateFn;
  glBlendEquationSeparateiOESProc glBlendEquationSeparateiOESFn;
  glBlendFuncProc glBlendFuncFn;
  glBlendFunciOESProc glBlendFunciOESFn;
  glBlendFuncSeparateProc glBlendFuncSeparateFn;
  glBlendFuncSeparateiOESProc glBlendFuncSeparateiOESFn;
  glBlitFramebufferProc glBlitFramebufferFn;
  glBlobCacheCallbacksANGLEProc glBlobCacheCallbacksANGLEFn;
  glBufferDataProc glBufferDataFn;
  glBufferSubDataProc glBufferSubDataFn;
  glCheckFramebufferStatusEXTProc glCheckFramebufferStatusEXTFn;
  glClearProc glClearFn;
  glClearBufferfiProc glClearBufferfiFn;
  glClearBufferfvProc glClearBufferfvFn;
  glClearBufferivProc glClearBufferivFn;
  glClearBufferuivProc glClearBufferuivFn;
  glClearColorProc glClearColorFn;
  glClearDepthProc glClearDepthFn;
  glClearDepthfProc glClearDepthfFn;
  glClearStencilProc glClearStencilFn;
  glClearTexImageProc glClearTexImageFn;
  glClearTexSubImageProc glClearTexSubImageFn;
  glClientWaitSyncProc glClientWaitSyncFn;
  glClipControlEXTProc glClipControlEXTFn;
  glColorMaskProc glColorMaskFn;
  glColorMaskiOESProc glColorMaskiOESFn;
  glCompileShaderProc glCompileShaderFn;
  glCompressedTexImage2DProc glCompressedTexImage2DFn;
  glCompressedTexImage3DProc glCompressedTexImage3DFn;
  glCompressedTexSubImage2DProc glCompressedTexSubImage2DFn;
  glCompressedTexSubImage3DProc glCompressedTexSubImage3DFn;
  glCopyBufferSubDataProc glCopyBufferSubDataFn;
  glCopySubTextureCHROMIUMProc glCopySubTextureCHROMIUMFn;
  glCopyTexImage2DProc glCopyTexImage2DFn;
  glCopyTexSubImage2DProc glCopyTexSubImage2DFn;
  glCopyTexSubImage3DProc glCopyTexSubImage3DFn;
  glCopyTextureCHROMIUMProc glCopyTextureCHROMIUMFn;
  glCreateMemoryObjectsEXTProc glCreateMemoryObjectsEXTFn;
  glCreateProgramProc glCreateProgramFn;
  glCreateShaderProc glCreateShaderFn;
  glCullFaceProc glCullFaceFn;
  glDebugMessageCallbackKHRProc glDebugMessageCallbackKHRFn;
  glDebugMessageControlKHRProc glDebugMessageControlKHRFn;
  glDebugMessageInsertKHRProc glDebugMessageInsertKHRFn;
  glDeleteBuffersARBProc glDeleteBuffersARBFn;
  glDeleteFencesNVProc glDeleteFencesNVFn;
  glDeleteFramebuffersEXTProc glDeleteFramebuffersEXTFn;
  glDeleteMemoryObjectsEXTProc glDeleteMemoryObjectsEXTFn;
  glDeleteProgramProc glDeleteProgramFn;
  glDeleteQueriesProc glDeleteQueriesFn;
  glDeleteRenderbuffersEXTProc glDeleteRenderbuffersEXTFn;
  glDeleteSamplersProc glDeleteSamplersFn;
  glDeleteSemaphoresEXTProc glDeleteSemaphoresEXTFn;
  glDeleteShaderProc glDeleteShaderFn;
  glDeleteSyncProc glDeleteSyncFn;
  glDeleteTexturesProc glDeleteTexturesFn;
  glDeleteTransformFeedbacksProc glDeleteTransformFeedbacksFn;
  glDeleteVertexArraysOESProc glDeleteVertexArraysOESFn;
  glDepthFuncProc glDepthFuncFn;
  glDepthMaskProc glDepthMaskFn;
  glDepthRangeProc glDepthRangeFn;
  glDepthRangefProc glDepthRangefFn;
  glDetachShaderProc glDetachShaderFn;
  glDisableProc glDisableFn;
  glDisableiOESProc glDisableiOESFn;
  glDisableVertexAttribArrayProc glDisableVertexAttribArrayFn;
  glDiscardFramebufferEXTProc glDiscardFramebufferEXTFn;
  glDrawArraysProc glDrawArraysFn;
  glDrawArraysInstancedANGLEProc glDrawArraysInstancedANGLEFn;
  glDrawArraysInstancedBaseInstanceANGLEProc
      glDrawArraysInstancedBaseInstanceANGLEFn;
  glDrawBufferProc glDrawBufferFn;
  glDrawBuffersARBProc glDrawBuffersARBFn;
  glDrawElementsProc glDrawElementsFn;
  glDrawElementsInstancedANGLEProc glDrawElementsInstancedANGLEFn;
  glDrawElementsInstancedBaseVertexBaseInstanceANGLEProc
      glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn;
  glDrawRangeElementsProc glDrawRangeElementsFn;
  glEGLImageTargetRenderbufferStorageOESProc
      glEGLImageTargetRenderbufferStorageOESFn;
  glEGLImageTargetTexStorageEXTProc glEGLImageTargetTexStorageEXTFn;
  glEGLImageTargetTexture2DOESProc glEGLImageTargetTexture2DOESFn;
  glEnableProc glEnableFn;
  glEnableiOESProc glEnableiOESFn;
  glEnableVertexAttribArrayProc glEnableVertexAttribArrayFn;
  glEndPixelLocalStorageANGLEProc glEndPixelLocalStorageANGLEFn;
  glEndPixelLocalStorageImplicitANGLEProc glEndPixelLocalStorageImplicitANGLEFn;
  glEndQueryProc glEndQueryFn;
  glEndTilingQCOMProc glEndTilingQCOMFn;
  glEndTransformFeedbackProc glEndTransformFeedbackFn;
  glFenceSyncProc glFenceSyncFn;
  glFinishProc glFinishFn;
  glFinishFenceNVProc glFinishFenceNVFn;
  glFlushProc glFlushFn;
  glFlushMappedBufferRangeProc glFlushMappedBufferRangeFn;
  glFramebufferMemorylessPixelLocalStorageANGLEProc
      glFramebufferMemorylessPixelLocalStorageANGLEFn;
  glFramebufferParameteriMESAProc glFramebufferParameteriMESAFn;
  glFramebufferPixelLocalClearValuefvANGLEProc
      glFramebufferPixelLocalClearValuefvANGLEFn;
  glFramebufferPixelLocalClearValueivANGLEProc
      glFramebufferPixelLocalClearValueivANGLEFn;
  glFramebufferPixelLocalClearValueuivANGLEProc
      glFramebufferPixelLocalClearValueuivANGLEFn;
  glFramebufferPixelLocalStorageInterruptANGLEProc
      glFramebufferPixelLocalStorageInterruptANGLEFn;
  glFramebufferPixelLocalStorageRestoreANGLEProc
      glFramebufferPixelLocalStorageRestoreANGLEFn;
  glFramebufferRenderbufferEXTProc glFramebufferRenderbufferEXTFn;
  glFramebufferTexture2DEXTProc glFramebufferTexture2DEXTFn;
  glFramebufferTexture2DMultisampleEXTProc
      glFramebufferTexture2DMultisampleEXTFn;
  glFramebufferTextureLayerProc glFramebufferTextureLayerFn;
  glFramebufferTextureMultiviewOVRProc glFramebufferTextureMultiviewOVRFn;
  glFramebufferTexturePixelLocalStorageANGLEProc
      glFramebufferTexturePixelLocalStorageANGLEFn;
  glFrontFaceProc glFrontFaceFn;
  glGenBuffersARBProc glGenBuffersARBFn;
  glGenerateMipmapEXTProc glGenerateMipmapEXTFn;
  glGenFencesNVProc glGenFencesNVFn;
  glGenFramebuffersEXTProc glGenFramebuffersEXTFn;
  glGenQueriesProc glGenQueriesFn;
  glGenRenderbuffersEXTProc glGenRenderbuffersEXTFn;
  glGenSamplersProc glGenSamplersFn;
  glGenSemaphoresEXTProc glGenSemaphoresEXTFn;
  glGenTexturesProc glGenTexturesFn;
  glGenTransformFeedbacksProc glGenTransformFeedbacksFn;
  glGenVertexArraysOESProc glGenVertexArraysOESFn;
  glGetActiveAttribProc glGetActiveAttribFn;
  glGetActiveUniformProc glGetActiveUniformFn;
  glGetActiveUniformBlockivProc glGetActiveUniformBlockivFn;
  glGetActiveUniformBlockivRobustANGLEProc
      glGetActiveUniformBlockivRobustANGLEFn;
  glGetActiveUniformBlockNameProc glGetActiveUniformBlockNameFn;
  glGetActiveUniformsivProc glGetActiveUniformsivFn;
  glGetAttachedShadersProc glGetAttachedShadersFn;
  glGetAttribLocationProc glGetAttribLocationFn;
  glGetBooleanvProc glGetBooleanvFn;
  glGetBooleanvRobustANGLEProc glGetBooleanvRobustANGLEFn;
  glGetBufferParameteri64vRobustANGLEProc glGetBufferParameteri64vRobustANGLEFn;
  glGetBufferParameterivProc glGetBufferParameterivFn;
  glGetBufferParameterivRobustANGLEProc glGetBufferParameterivRobustANGLEFn;
  glGetBufferPointervRobustANGLEProc glGetBufferPointervRobustANGLEFn;
  glGetDebugMessageLogKHRProc glGetDebugMessageLogKHRFn;
  glGetErrorProc glGetErrorFn;
  glGetFenceivNVProc glGetFenceivNVFn;
  glGetFloatvProc glGetFloatvFn;
  glGetFloatvRobustANGLEProc glGetFloatvRobustANGLEFn;
  glGetFragDataIndexProc glGetFragDataIndexFn;
  glGetFragDataLocationProc glGetFragDataLocationFn;
  glGetFramebufferAttachmentParameterivEXTProc
      glGetFramebufferAttachmentParameterivEXTFn;
  glGetFramebufferAttachmentParameterivRobustANGLEProc
      glGetFramebufferAttachmentParameterivRobustANGLEFn;
  glGetFramebufferPixelLocalStorageParameterfvRobustANGLEProc
      glGetFramebufferPixelLocalStorageParameterfvRobustANGLEFn;
  glGetFramebufferPixelLocalStorageParameterivRobustANGLEProc
      glGetFramebufferPixelLocalStorageParameterivRobustANGLEFn;
  glGetFramebufferPixelLocalStorageParameteruivRobustANGLEProc
      glGetFramebufferPixelLocalStorageParameteruivRobustANGLEFn;
  glGetGraphicsResetStatusARBProc glGetGraphicsResetStatusARBFn;
  glGetInteger64i_vProc glGetInteger64i_vFn;
  glGetInteger64i_vRobustANGLEProc glGetInteger64i_vRobustANGLEFn;
  glGetInteger64vProc glGetInteger64vFn;
  glGetInteger64vRobustANGLEProc glGetInteger64vRobustANGLEFn;
  glGetIntegeri_vProc glGetIntegeri_vFn;
  glGetIntegeri_vRobustANGLEProc glGetIntegeri_vRobustANGLEFn;
  glGetIntegervProc glGetIntegervFn;
  glGetIntegervRobustANGLEProc glGetIntegervRobustANGLEFn;
  glGetInternalformativProc glGetInternalformativFn;
  glGetInternalformativRobustANGLEProc glGetInternalformativRobustANGLEFn;
  glGetInternalformatSampleivNVProc glGetInternalformatSampleivNVFn;
  glGetMultisamplefvRobustANGLEProc glGetMultisamplefvRobustANGLEFn;
  glGetObjectLabelKHRProc glGetObjectLabelKHRFn;
  glGetObjectPtrLabelKHRProc glGetObjectPtrLabelKHRFn;
  glGetPointervKHRProc glGetPointervKHRFn;
  glGetProgramBinaryProc glGetProgramBinaryFn;
  glGetProgramInfoLogProc glGetProgramInfoLogFn;
  glGetProgramivProc glGetProgramivFn;
  glGetProgramivRobustANGLEProc glGetProgramivRobustANGLEFn;
  glGetQueryivProc glGetQueryivFn;
  glGetQueryivRobustANGLEProc glGetQueryivRobustANGLEFn;
  glGetQueryObjecti64vProc glGetQueryObjecti64vFn;
  glGetQueryObjecti64vRobustANGLEProc glGetQueryObjecti64vRobustANGLEFn;
  glGetQueryObjectivProc glGetQueryObjectivFn;
  glGetQueryObjectivRobustANGLEProc glGetQueryObjectivRobustANGLEFn;
  glGetQueryObjectui64vProc glGetQueryObjectui64vFn;
  glGetQueryObjectui64vRobustANGLEProc glGetQueryObjectui64vRobustANGLEFn;
  glGetQueryObjectuivProc glGetQueryObjectuivFn;
  glGetQueryObjectuivRobustANGLEProc glGetQueryObjectuivRobustANGLEFn;
  glGetRenderbufferParameterivEXTProc glGetRenderbufferParameterivEXTFn;
  glGetRenderbufferParameterivRobustANGLEProc
      glGetRenderbufferParameterivRobustANGLEFn;
  glGetSamplerParameterfvProc glGetSamplerParameterfvFn;
  glGetSamplerParameterfvRobustANGLEProc glGetSamplerParameterfvRobustANGLEFn;
  glGetSamplerParameterivProc glGetSamplerParameterivFn;
  glGetSamplerParameterivRobustANGLEProc glGetSamplerParameterivRobustANGLEFn;
  glGetShaderInfoLogProc glGetShaderInfoLogFn;
  glGetShaderivProc glGetShaderivFn;
  glGetShaderivRobustANGLEProc glGetShaderivRobustANGLEFn;
  glGetShaderPrecisionFormatProc glGetShaderPrecisionFormatFn;
  glGetShaderSourceProc glGetShaderSourceFn;
  glGetStringProc glGetStringFn;
  glGetStringiProc glGetStringiFn;
  glGetSyncivProc glGetSyncivFn;
  glGetTexLevelParameterfvANGLEProc glGetTexLevelParameterfvANGLEFn;
  glGetTexLevelParameterfvRobustANGLEProc glGetTexLevelParameterfvRobustANGLEFn;
  glGetTexLevelParameterivANGLEProc glGetTexLevelParameterivANGLEFn;
  glGetTexLevelParameterivRobustANGLEProc glGetTexLevelParameterivRobustANGLEFn;
  glGetTexParameterfvProc glGetTexParameterfvFn;
  glGetTexParameterfvRobustANGLEProc glGetTexParameterfvRobustANGLEFn;
  glGetTexParameterivProc glGetTexParameterivFn;
  glGetTexParameterivRobustANGLEProc glGetTexParameterivRobustANGLEFn;
  glGetTransformFeedbackVaryingProc glGetTransformFeedbackVaryingFn;
  glGetTranslatedShaderSourceANGLEProc glGetTranslatedShaderSourceANGLEFn;
  glGetUniformBlockIndexProc glGetUniformBlockIndexFn;
  glGetUniformfvProc glGetUniformfvFn;
  glGetUniformfvRobustANGLEProc glGetUniformfvRobustANGLEFn;
  glGetUniformIndicesProc glGetUniformIndicesFn;
  glGetUniformivProc glGetUniformivFn;
  glGetUniformivRobustANGLEProc glGetUniformivRobustANGLEFn;
  glGetUniformLocationProc glGetUniformLocationFn;
  glGetUniformuivProc glGetUniformuivFn;
  glGetUniformuivRobustANGLEProc glGetUniformuivRobustANGLEFn;
  glGetVertexAttribfvProc glGetVertexAttribfvFn;
  glGetVertexAttribfvRobustANGLEProc glGetVertexAttribfvRobustANGLEFn;
  glGetVertexAttribIivRobustANGLEProc glGetVertexAttribIivRobustANGLEFn;
  glGetVertexAttribIuivRobustANGLEProc glGetVertexAttribIuivRobustANGLEFn;
  glGetVertexAttribivProc glGetVertexAttribivFn;
  glGetVertexAttribivRobustANGLEProc glGetVertexAttribivRobustANGLEFn;
  glGetVertexAttribPointervProc glGetVertexAttribPointervFn;
  glGetVertexAttribPointervRobustANGLEProc
      glGetVertexAttribPointervRobustANGLEFn;
  glHintProc glHintFn;
  glImportMemoryFdEXTProc glImportMemoryFdEXTFn;
  glImportMemoryWin32HandleEXTProc glImportMemoryWin32HandleEXTFn;
  glImportMemoryZirconHandleANGLEProc glImportMemoryZirconHandleANGLEFn;
  glImportSemaphoreFdEXTProc glImportSemaphoreFdEXTFn;
  glImportSemaphoreWin32HandleEXTProc glImportSemaphoreWin32HandleEXTFn;
  glImportSemaphoreZirconHandleANGLEProc glImportSemaphoreZirconHandleANGLEFn;
  glInsertEventMarkerEXTProc glInsertEventMarkerEXTFn;
  glInvalidateFramebufferProc glInvalidateFramebufferFn;
  glInvalidateSubFramebufferProc glInvalidateSubFramebufferFn;
  glInvalidateTextureANGLEProc glInvalidateTextureANGLEFn;
  glIsBufferProc glIsBufferFn;
  glIsEnabledProc glIsEnabledFn;
  glIsEnablediOESProc glIsEnablediOESFn;
  glIsFenceNVProc glIsFenceNVFn;
  glIsFramebufferEXTProc glIsFramebufferEXTFn;
  glIsProgramProc glIsProgramFn;
  glIsQueryProc glIsQueryFn;
  glIsRenderbufferEXTProc glIsRenderbufferEXTFn;
  glIsSamplerProc glIsSamplerFn;
  glIsShaderProc glIsShaderFn;
  glIsSyncProc glIsSyncFn;
  glIsTextureProc glIsTextureFn;
  glIsTransformFeedbackProc glIsTransformFeedbackFn;
  glIsVertexArrayOESProc glIsVertexArrayOESFn;
  glLineWidthProc glLineWidthFn;
  glLinkProgramProc glLinkProgramFn;
  glMapBufferProc glMapBufferFn;
  glMapBufferRangeProc glMapBufferRangeFn;
  glMaxShaderCompilerThreadsKHRProc glMaxShaderCompilerThreadsKHRFn;
  glMemoryObjectParameterivEXTProc glMemoryObjectParameterivEXTFn;
  glMultiDrawArraysANGLEProc glMultiDrawArraysANGLEFn;
  glMultiDrawArraysInstancedANGLEProc glMultiDrawArraysInstancedANGLEFn;
  glMultiDrawArraysInstancedBaseInstanceANGLEProc
      glMultiDrawArraysInstancedBaseInstanceANGLEFn;
  glMultiDrawElementsANGLEProc glMultiDrawElementsANGLEFn;
  glMultiDrawElementsInstancedANGLEProc glMultiDrawElementsInstancedANGLEFn;
  glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEProc
      glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn;
  glObjectLabelKHRProc glObjectLabelKHRFn;
  glObjectPtrLabelKHRProc glObjectPtrLabelKHRFn;
  glPatchParameteriOESProc glPatchParameteriOESFn;
  glPauseTransformFeedbackProc glPauseTransformFeedbackFn;
  glPixelLocalStorageBarrierANGLEProc glPixelLocalStorageBarrierANGLEFn;
  glPixelStoreiProc glPixelStoreiFn;
  glPointParameteriProc glPointParameteriFn;
  glPolygonModeProc glPolygonModeFn;
  glPolygonModeANGLEProc glPolygonModeANGLEFn;
  glPolygonOffsetProc glPolygonOffsetFn;
  glPolygonOffsetClampEXTProc glPolygonOffsetClampEXTFn;
  glPopDebugGroupKHRProc glPopDebugGroupKHRFn;
  glPopGroupMarkerEXTProc glPopGroupMarkerEXTFn;
  glPrimitiveRestartIndexProc glPrimitiveRestartIndexFn;
  glProgramBinaryProc glProgramBinaryFn;
  glProgramParameteriProc glProgramParameteriFn;
  glProvokingVertexANGLEProc glProvokingVertexANGLEFn;
  glPushDebugGroupKHRProc glPushDebugGroupKHRFn;
  glPushGroupMarkerEXTProc glPushGroupMarkerEXTFn;
  glQueryCounterProc glQueryCounterFn;
  glReadBufferProc glReadBufferFn;
  glReadPixelsProc glReadPixelsFn;
  glReadPixelsRobustANGLEProc glReadPixelsRobustANGLEFn;
  glReleaseShaderCompilerProc glReleaseShaderCompilerFn;
  glReleaseTexturesANGLEProc glReleaseTexturesANGLEFn;
  glRenderbufferStorageEXTProc glRenderbufferStorageEXTFn;
  glRenderbufferStorageMultisampleProc glRenderbufferStorageMultisampleFn;
  glRenderbufferStorageMultisampleAdvancedAMDProc
      glRenderbufferStorageMultisampleAdvancedAMDFn;
  glRenderbufferStorageMultisampleEXTProc glRenderbufferStorageMultisampleEXTFn;
  glRequestExtensionANGLEProc glRequestExtensionANGLEFn;
  glResumeTransformFeedbackProc glResumeTransformFeedbackFn;
  glSampleCoverageProc glSampleCoverageFn;
  glSamplerParameterfProc glSamplerParameterfFn;
  glSamplerParameterfvProc glSamplerParameterfvFn;
  glSamplerParameterfvRobustANGLEProc glSamplerParameterfvRobustANGLEFn;
  glSamplerParameteriProc glSamplerParameteriFn;
  glSamplerParameterivProc glSamplerParameterivFn;
  glSamplerParameterivRobustANGLEProc glSamplerParameterivRobustANGLEFn;
  glScissorProc glScissorFn;
  glSetFenceNVProc glSetFenceNVFn;
  glShaderBinaryProc glShaderBinaryFn;
  glShaderSourceProc glShaderSourceFn;
  glSignalSemaphoreEXTProc glSignalSemaphoreEXTFn;
  glStartTilingQCOMProc glStartTilingQCOMFn;
  glStencilFuncProc glStencilFuncFn;
  glStencilFuncSeparateProc glStencilFuncSeparateFn;
  glStencilMaskProc glStencilMaskFn;
  glStencilMaskSeparateProc glStencilMaskSeparateFn;
  glStencilOpProc glStencilOpFn;
  glStencilOpSeparateProc glStencilOpSeparateFn;
  glTestFenceNVProc glTestFenceNVFn;
  glTexBufferOESProc glTexBufferOESFn;
  glTexBufferRangeOESProc glTexBufferRangeOESFn;
  glTexImage2DProc glTexImage2DFn;
  glTexImage2DExternalANGLEProc glTexImage2DExternalANGLEFn;
  glTexImage2DRobustANGLEProc glTexImage2DRobustANGLEFn;
  glTexImage3DProc glTexImage3DFn;
  glTexImage3DRobustANGLEProc glTexImage3DRobustANGLEFn;
  glTexParameterfProc glTexParameterfFn;
  glTexParameterfvProc glTexParameterfvFn;
  glTexParameterfvRobustANGLEProc glTexParameterfvRobustANGLEFn;
  glTexParameteriProc glTexParameteriFn;
  glTexParameterivProc glTexParameterivFn;
  glTexParameterivRobustANGLEProc glTexParameterivRobustANGLEFn;
  glTexStorage2DEXTProc glTexStorage2DEXTFn;
  glTexStorage3DProc glTexStorage3DFn;
  glTexStorageMem2DEXTProc glTexStorageMem2DEXTFn;
  glTexStorageMemFlags2DANGLEProc glTexStorageMemFlags2DANGLEFn;
  glTexSubImage2DProc glTexSubImage2DFn;
  glTexSubImage2DRobustANGLEProc glTexSubImage2DRobustANGLEFn;
  glTexSubImage3DProc glTexSubImage3DFn;
  glTexSubImage3DRobustANGLEProc glTexSubImage3DRobustANGLEFn;
  glTransformFeedbackVaryingsProc glTransformFeedbackVaryingsFn;
  glTrimMemoryANGLEProc glTrimMemoryANGLEFn;
  glUniform1fProc glUniform1fFn;
  glUniform1fvProc glUniform1fvFn;
  glUniform1iProc glUniform1iFn;
  glUniform1ivProc glUniform1ivFn;
  glUniform1uiProc glUniform1uiFn;
  glUniform1uivProc glUniform1uivFn;
  glUniform2fProc glUniform2fFn;
  glUniform2fvProc glUniform2fvFn;
  glUniform2iProc glUniform2iFn;
  glUniform2ivProc glUniform2ivFn;
  glUniform2uiProc glUniform2uiFn;
  glUniform2uivProc glUniform2uivFn;
  glUniform3fProc glUniform3fFn;
  glUniform3fvProc glUniform3fvFn;
  glUniform3iProc glUniform3iFn;
  glUniform3ivProc glUniform3ivFn;
  glUniform3uiProc glUniform3uiFn;
  glUniform3uivProc glUniform3uivFn;
  glUniform4fProc glUniform4fFn;
  glUniform4fvProc glUniform4fvFn;
  glUniform4iProc glUniform4iFn;
  glUniform4ivProc glUniform4ivFn;
  glUniform4uiProc glUniform4uiFn;
  glUniform4uivProc glUniform4uivFn;
  glUniformBlockBindingProc glUniformBlockBindingFn;
  glUniformMatrix2fvProc glUniformMatrix2fvFn;
  glUniformMatrix2x3fvProc glUniformMatrix2x3fvFn;
  glUniformMatrix2x4fvProc glUniformMatrix2x4fvFn;
  glUniformMatrix3fvProc glUniformMatrix3fvFn;
  glUniformMatrix3x2fvProc glUniformMatrix3x2fvFn;
  glUniformMatrix3x4fvProc glUniformMatrix3x4fvFn;
  glUniformMatrix4fvProc glUniformMatrix4fvFn;
  glUniformMatrix4x2fvProc glUniformMatrix4x2fvFn;
  glUniformMatrix4x3fvProc glUniformMatrix4x3fvFn;
  glUnmapBufferProc glUnmapBufferFn;
  glUseProgramProc glUseProgramFn;
  glValidateProgramProc glValidateProgramFn;
  glVertexAttrib1fProc glVertexAttrib1fFn;
  glVertexAttrib1fvProc glVertexAttrib1fvFn;
  glVertexAttrib2fProc glVertexAttrib2fFn;
  glVertexAttrib2fvProc glVertexAttrib2fvFn;
  glVertexAttrib3fProc glVertexAttrib3fFn;
  glVertexAttrib3fvProc glVertexAttrib3fvFn;
  glVertexAttrib4fProc glVertexAttrib4fFn;
  glVertexAttrib4fvProc glVertexAttrib4fvFn;
  glVertexAttribDivisorANGLEProc glVertexAttribDivisorANGLEFn;
  glVertexAttribI4iProc glVertexAttribI4iFn;
  glVertexAttribI4ivProc glVertexAttribI4ivFn;
  glVertexAttribI4uiProc glVertexAttribI4uiFn;
  glVertexAttribI4uivProc glVertexAttribI4uivFn;
  glVertexAttribIPointerProc glVertexAttribIPointerFn;
  glVertexAttribPointerProc glVertexAttribPointerFn;
  glViewportProc glViewportFn;
  glWaitSemaphoreEXTProc glWaitSemaphoreEXTFn;
  glWaitSyncProc glWaitSyncFn;
  glWindowRectanglesEXTProc glWindowRectanglesEXTFn;
  glAcquireTexturesANGLEContextANGLEProc glAcquireTexturesANGLEContextANGLEFn;
  glActiveTextureContextANGLEProc glActiveTextureContextANGLEFn;
  glAttachShaderContextANGLEProc glAttachShaderContextANGLEFn;
  glBeginPixelLocalStorageANGLEContextANGLEProc
      glBeginPixelLocalStorageANGLEContextANGLEFn;
  glBeginQueryContextANGLEProc glBeginQueryContextANGLEFn;
  glBeginTransformFeedbackContextANGLEProc
      glBeginTransformFeedbackContextANGLEFn;
  glBindAttribLocationContextANGLEProc glBindAttribLocationContextANGLEFn;
  glBindBufferContextANGLEProc glBindBufferContextANGLEFn;
  glBindBufferBaseContextANGLEProc glBindBufferBaseContextANGLEFn;
  glBindBufferRangeContextANGLEProc glBindBufferRangeContextANGLEFn;
  glBindFragDataLocationContextANGLEProc glBindFragDataLocationContextANGLEFn;
  glBindFragDataLocationIndexedContextANGLEProc
      glBindFragDataLocationIndexedContextANGLEFn;
  glBindFramebufferEXTContextANGLEProc glBindFramebufferEXTContextANGLEFn;
  glBindRenderbufferEXTContextANGLEProc glBindRenderbufferEXTContextANGLEFn;
  glBindSamplerContextANGLEProc glBindSamplerContextANGLEFn;
  glBindTextureContextANGLEProc glBindTextureContextANGLEFn;
  glBindTransformFeedbackContextANGLEProc glBindTransformFeedbackContextANGLEFn;
  glBindUniformLocationCHROMIUMContextANGLEProc
      glBindUniformLocationCHROMIUMContextANGLEFn;
  glBindVertexArrayOESContextANGLEProc glBindVertexArrayOESContextANGLEFn;
  glBlendBarrierKHRContextANGLEProc glBlendBarrierKHRContextANGLEFn;
  glBlendColorContextANGLEProc glBlendColorContextANGLEFn;
  glBlendEquationContextANGLEProc glBlendEquationContextANGLEFn;
  glBlendEquationiOESContextANGLEProc glBlendEquationiOESContextANGLEFn;
  glBlendEquationSeparateContextANGLEProc glBlendEquationSeparateContextANGLEFn;
  glBlendEquationSeparateiOESContextANGLEProc
      glBlendEquationSeparateiOESContextANGLEFn;
  glBlendFuncContextANGLEProc glBlendFuncContextANGLEFn;
  glBlendFunciOESContextANGLEProc glBlendFunciOESContextANGLEFn;
  glBlendFuncSeparateContextANGLEProc glBlendFuncSeparateContextANGLEFn;
  glBlendFuncSeparateiOESContextANGLEProc glBlendFuncSeparateiOESContextANGLEFn;
  glBlitFramebufferContextANGLEProc glBlitFramebufferContextANGLEFn;
  glBlobCacheCallbacksANGLEContextANGLEProc
      glBlobCacheCallbacksANGLEContextANGLEFn;
  glBufferDataContextANGLEProc glBufferDataContextANGLEFn;
  glBufferSubDataContextANGLEProc glBufferSubDataContextANGLEFn;
  glCheckFramebufferStatusEXTContextANGLEProc
      glCheckFramebufferStatusEXTContextANGLEFn;
  glClearContextANGLEProc glClearContextANGLEFn;
  glClearBufferfiContextANGLEProc glClearBufferfiContextANGLEFn;
  glClearBufferfvContextANGLEProc glClearBufferfvContextANGLEFn;
  glClearBufferivContextANGLEProc glClearBufferivContextANGLEFn;
  glClearBufferuivContextANGLEProc glClearBufferuivContextANGLEFn;
  glClearColorContextANGLEProc glClearColorContextANGLEFn;
  glClearDepthContextANGLEProc glClearDepthContextANGLEFn;
  glClearDepthfContextANGLEProc glClearDepthfContextANGLEFn;
  glClearStencilContextANGLEProc glClearStencilContextANGLEFn;
  glClearTexImageContextANGLEProc glClearTexImageContextANGLEFn;
  glClearTexSubImageContextANGLEProc glClearTexSubImageContextANGLEFn;
  glClientWaitSyncContextANGLEProc glClientWaitSyncContextANGLEFn;
  glClipControlEXTContextANGLEProc glClipControlEXTContextANGLEFn;
  glColorMaskContextANGLEProc glColorMaskContextANGLEFn;
  glColorMaskiOESContextANGLEProc glColorMaskiOESContextANGLEFn;
  glCompileShaderContextANGLEProc glCompileShaderContextANGLEFn;
  glCompressedTexImage2DContextANGLEProc glCompressedTexImage2DContextANGLEFn;
  glCompressedTexImage3DContextANGLEProc glCompressedTexImage3DContextANGLEFn;
  glCompressedTexSubImage2DContextANGLEProc
      glCompressedTexSubImage2DContextANGLEFn;
  glCompressedTexSubImage3DContextANGLEProc
      glCompressedTexSubImage3DContextANGLEFn;
  glCopyBufferSubDataContextANGLEProc glCopyBufferSubDataContextANGLEFn;
  glCopySubTextureCHROMIUMContextANGLEProc
      glCopySubTextureCHROMIUMContextANGLEFn;
  glCopyTexImage2DContextANGLEProc glCopyTexImage2DContextANGLEFn;
  glCopyTexSubImage2DContextANGLEProc glCopyTexSubImage2DContextANGLEFn;
  glCopyTexSubImage3DContextANGLEProc glCopyTexSubImage3DContextANGLEFn;
  glCopyTextureCHROMIUMContextANGLEProc glCopyTextureCHROMIUMContextANGLEFn;
  glCreateMemoryObjectsEXTContextANGLEProc
      glCreateMemoryObjectsEXTContextANGLEFn;
  glCreateProgramContextANGLEProc glCreateProgramContextANGLEFn;
  glCreateShaderContextANGLEProc glCreateShaderContextANGLEFn;
  glCullFaceContextANGLEProc glCullFaceContextANGLEFn;
  glDebugMessageCallbackKHRContextANGLEProc
      glDebugMessageCallbackKHRContextANGLEFn;
  glDebugMessageControlKHRContextANGLEProc
      glDebugMessageControlKHRContextANGLEFn;
  glDebugMessageInsertKHRContextANGLEProc glDebugMessageInsertKHRContextANGLEFn;
  glDeleteBuffersARBContextANGLEProc glDeleteBuffersARBContextANGLEFn;
  glDeleteFencesNVContextANGLEProc glDeleteFencesNVContextANGLEFn;
  glDeleteFramebuffersEXTContextANGLEProc glDeleteFramebuffersEXTContextANGLEFn;
  glDeleteMemoryObjectsEXTContextANGLEProc
      glDeleteMemoryObjectsEXTContextANGLEFn;
  glDeleteProgramContextANGLEProc glDeleteProgramContextANGLEFn;
  glDeleteQueriesContextANGLEProc glDeleteQueriesContextANGLEFn;
  glDeleteRenderbuffersEXTContextANGLEProc
      glDeleteRenderbuffersEXTContextANGLEFn;
  glDeleteSamplersContextANGLEProc glDeleteSamplersContextANGLEFn;
  glDeleteSemaphoresEXTContextANGLEProc glDeleteSemaphoresEXTContextANGLEFn;
  glDeleteShaderContextANGLEProc glDeleteShaderContextANGLEFn;
  glDeleteSyncContextANGLEProc glDeleteSyncContextANGLEFn;
  glDeleteTexturesContextANGLEProc glDeleteTexturesContextANGLEFn;
  glDeleteTransformFeedbacksContextANGLEProc
      glDeleteTransformFeedbacksContextANGLEFn;
  glDeleteVertexArraysOESContextANGLEProc glDeleteVertexArraysOESContextANGLEFn;
  glDepthFuncContextANGLEProc glDepthFuncContextANGLEFn;
  glDepthMaskContextANGLEProc glDepthMaskContextANGLEFn;
  glDepthRangeContextANGLEProc glDepthRangeContextANGLEFn;
  glDepthRangefContextANGLEProc glDepthRangefContextANGLEFn;
  glDetachShaderContextANGLEProc glDetachShaderContextANGLEFn;
  glDisableContextANGLEProc glDisableContextANGLEFn;
  glDisableiOESContextANGLEProc glDisableiOESContextANGLEFn;
  glDisableVertexAttribArrayContextANGLEProc
      glDisableVertexAttribArrayContextANGLEFn;
  glDiscardFramebufferEXTContextANGLEProc glDiscardFramebufferEXTContextANGLEFn;
  glDrawArraysContextANGLEProc glDrawArraysContextANGLEFn;
  glDrawArraysInstancedANGLEContextANGLEProc
      glDrawArraysInstancedANGLEContextANGLEFn;
  glDrawArraysInstancedBaseInstanceANGLEContextANGLEProc
      glDrawArraysInstancedBaseInstanceANGLEContextANGLEFn;
  glDrawBufferContextANGLEProc glDrawBufferContextANGLEFn;
  glDrawBuffersARBContextANGLEProc glDrawBuffersARBContextANGLEFn;
  glDrawElementsContextANGLEProc glDrawElementsContextANGLEFn;
  glDrawElementsInstancedANGLEContextANGLEProc
      glDrawElementsInstancedANGLEContextANGLEFn;
  glDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEProc
      glDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEFn;
  glDrawRangeElementsContextANGLEProc glDrawRangeElementsContextANGLEFn;
  glEGLImageTargetRenderbufferStorageOESContextANGLEProc
      glEGLImageTargetRenderbufferStorageOESContextANGLEFn;
  glEGLImageTargetTexStorageEXTContextANGLEProc
      glEGLImageTargetTexStorageEXTContextANGLEFn;
  glEGLImageTargetTexture2DOESContextANGLEProc
      glEGLImageTargetTexture2DOESContextANGLEFn;
  glEnableContextANGLEProc glEnableContextANGLEFn;
  glEnableiOESContextANGLEProc glEnableiOESContextANGLEFn;
  glEnableVertexAttribArrayContextANGLEProc
      glEnableVertexAttribArrayContextANGLEFn;
  glEndPixelLocalStorageANGLEContextANGLEProc
      glEndPixelLocalStorageANGLEContextANGLEFn;
  glEndPixelLocalStorageImplicitANGLEContextANGLEProc
      glEndPixelLocalStorageImplicitANGLEContextANGLEFn;
  glEndQueryContextANGLEProc glEndQueryContextANGLEFn;
  glEndTilingQCOMContextANGLEProc glEndTilingQCOMContextANGLEFn;
  glEndTransformFeedbackContextANGLEProc glEndTransformFeedbackContextANGLEFn;
  glFenceSyncContextANGLEProc glFenceSyncContextANGLEFn;
  glFinishContextANGLEProc glFinishContextANGLEFn;
  glFinishFenceNVContextANGLEProc glFinishFenceNVContextANGLEFn;
  glFlushContextANGLEProc glFlushContextANGLEFn;
  glFlushMappedBufferRangeContextANGLEProc
      glFlushMappedBufferRangeContextANGLEFn;
  glFramebufferMemorylessPixelLocalStorageANGLEContextANGLEProc
      glFramebufferMemorylessPixelLocalStorageANGLEContextANGLEFn;
  glFramebufferParameteriMESAContextANGLEProc
      glFramebufferParameteriMESAContextANGLEFn;
  glFramebufferPixelLocalClearValuefvANGLEContextANGLEProc
      glFramebufferPixelLocalClearValuefvANGLEContextANGLEFn;
  glFramebufferPixelLocalClearValueivANGLEContextANGLEProc
      glFramebufferPixelLocalClearValueivANGLEContextANGLEFn;
  glFramebufferPixelLocalClearValueuivANGLEContextANGLEProc
      glFramebufferPixelLocalClearValueuivANGLEContextANGLEFn;
  glFramebufferPixelLocalStorageInterruptANGLEContextANGLEProc
      glFramebufferPixelLocalStorageInterruptANGLEContextANGLEFn;
  glFramebufferPixelLocalStorageRestoreANGLEContextANGLEProc
      glFramebufferPixelLocalStorageRestoreANGLEContextANGLEFn;
  glFramebufferRenderbufferEXTContextANGLEProc
      glFramebufferRenderbufferEXTContextANGLEFn;
  glFramebufferTexture2DEXTContextANGLEProc
      glFramebufferTexture2DEXTContextANGLEFn;
  glFramebufferTexture2DMultisampleEXTContextANGLEProc
      glFramebufferTexture2DMultisampleEXTContextANGLEFn;
  glFramebufferTextureLayerContextANGLEProc
      glFramebufferTextureLayerContextANGLEFn;
  glFramebufferTextureMultiviewOVRContextANGLEProc
      glFramebufferTextureMultiviewOVRContextANGLEFn;
  glFramebufferTexturePixelLocalStorageANGLEContextANGLEProc
      glFramebufferTexturePixelLocalStorageANGLEContextANGLEFn;
  glFrontFaceContextANGLEProc glFrontFaceContextANGLEFn;
  glGenBuffersARBContextANGLEProc glGenBuffersARBContextANGLEFn;
  glGenerateMipmapEXTContextANGLEProc glGenerateMipmapEXTContextANGLEFn;
  glGenFencesNVContextANGLEProc glGenFencesNVContextANGLEFn;
  glGenFramebuffersEXTContextANGLEProc glGenFramebuffersEXTContextANGLEFn;
  glGenQueriesContextANGLEProc glGenQueriesContextANGLEFn;
  glGenRenderbuffersEXTContextANGLEProc glGenRenderbuffersEXTContextANGLEFn;
  glGenSamplersContextANGLEProc glGenSamplersContextANGLEFn;
  glGenSemaphoresEXTContextANGLEProc glGenSemaphoresEXTContextANGLEFn;
  glGenTexturesContextANGLEProc glGenTexturesContextANGLEFn;
  glGenTransformFeedbacksContextANGLEProc glGenTransformFeedbacksContextANGLEFn;
  glGenVertexArraysOESContextANGLEProc glGenVertexArraysOESContextANGLEFn;
  glGetActiveAttribContextANGLEProc glGetActiveAttribContextANGLEFn;
  glGetActiveUniformContextANGLEProc glGetActiveUniformContextANGLEFn;
  glGetActiveUniformBlockivContextANGLEProc
      glGetActiveUniformBlockivContextANGLEFn;
  glGetActiveUniformBlockivRobustANGLEContextANGLEProc
      glGetActiveUniformBlockivRobustANGLEContextANGLEFn;
  glGetActiveUniformBlockNameContextANGLEProc
      glGetActiveUniformBlockNameContextANGLEFn;
  glGetActiveUniformsivContextANGLEProc glGetActiveUniformsivContextANGLEFn;
  glGetAttachedShadersContextANGLEProc glGetAttachedShadersContextANGLEFn;
  glGetAttribLocationContextANGLEProc glGetAttribLocationContextANGLEFn;
  glGetBooleanvContextANGLEProc glGetBooleanvContextANGLEFn;
  glGetBooleanvRobustANGLEContextANGLEProc
      glGetBooleanvRobustANGLEContextANGLEFn;
  glGetBufferParameteri64vRobustANGLEContextANGLEProc
      glGetBufferParameteri64vRobustANGLEContextANGLEFn;
  glGetBufferParameterivContextANGLEProc glGetBufferParameterivContextANGLEFn;
  glGetBufferParameterivRobustANGLEContextANGLEProc
      glGetBufferParameterivRobustANGLEContextANGLEFn;
  glGetBufferPointervRobustANGLEContextANGLEProc
      glGetBufferPointervRobustANGLEContextANGLEFn;
  glGetDebugMessageLogKHRContextANGLEProc glGetDebugMessageLogKHRContextANGLEFn;
  glGetErrorContextANGLEProc glGetErrorContextANGLEFn;
  glGetFenceivNVContextANGLEProc glGetFenceivNVContextANGLEFn;
  glGetFloatvContextANGLEProc glGetFloatvContextANGLEFn;
  glGetFloatvRobustANGLEContextANGLEProc glGetFloatvRobustANGLEContextANGLEFn;
  glGetFragDataIndexContextANGLEProc glGetFragDataIndexContextANGLEFn;
  glGetFragDataLocationContextANGLEProc glGetFragDataLocationContextANGLEFn;
  glGetFramebufferAttachmentParameterivEXTContextANGLEProc
      glGetFramebufferAttachmentParameterivEXTContextANGLEFn;
  glGetFramebufferAttachmentParameterivRobustANGLEContextANGLEProc
      glGetFramebufferAttachmentParameterivRobustANGLEContextANGLEFn;
  glGetFramebufferPixelLocalStorageParameterfvRobustANGLEContextANGLEProc
      glGetFramebufferPixelLocalStorageParameterfvRobustANGLEContextANGLEFn;
  glGetFramebufferPixelLocalStorageParameterivRobustANGLEContextANGLEProc
      glGetFramebufferPixelLocalStorageParameterivRobustANGLEContextANGLEFn;
  glGetFramebufferPixelLocalStorageParameteruivRobustANGLEContextANGLEProc
      glGetFramebufferPixelLocalStorageParameteruivRobustANGLEContextANGLEFn;
  glGetGraphicsResetStatusARBContextANGLEProc
      glGetGraphicsResetStatusARBContextANGLEFn;
  glGetInteger64i_vContextANGLEProc glGetInteger64i_vContextANGLEFn;
  glGetInteger64i_vRobustANGLEContextANGLEProc
      glGetInteger64i_vRobustANGLEContextANGLEFn;
  glGetInteger64vContextANGLEProc glGetInteger64vContextANGLEFn;
  glGetInteger64vRobustANGLEContextANGLEProc
      glGetInteger64vRobustANGLEContextANGLEFn;
  glGetIntegeri_vContextANGLEProc glGetIntegeri_vContextANGLEFn;
  glGetIntegeri_vRobustANGLEContextANGLEProc
      glGetIntegeri_vRobustANGLEContextANGLEFn;
  glGetIntegervContextANGLEProc glGetIntegervContextANGLEFn;
  glGetIntegervRobustANGLEContextANGLEProc
      glGetIntegervRobustANGLEContextANGLEFn;
  glGetInternalformativContextANGLEProc glGetInternalformativContextANGLEFn;
  glGetInternalformativRobustANGLEContextANGLEProc
      glGetInternalformativRobustANGLEContextANGLEFn;
  glGetInternalformatSampleivNVContextANGLEProc
      glGetInternalformatSampleivNVContextANGLEFn;
  glGetMultisamplefvRobustANGLEContextANGLEProc
      glGetMultisamplefvRobustANGLEContextANGLEFn;
  glGetObjectLabelKHRContextANGLEProc glGetObjectLabelKHRContextANGLEFn;
  glGetObjectPtrLabelKHRContextANGLEProc glGetObjectPtrLabelKHRContextANGLEFn;
  glGetPointervKHRContextANGLEProc glGetPointervKHRContextANGLEFn;
  glGetProgramBinaryContextANGLEProc glGetProgramBinaryContextANGLEFn;
  glGetProgramInfoLogContextANGLEProc glGetProgramInfoLogContextANGLEFn;
  glGetProgramivContextANGLEProc glGetProgramivContextANGLEFn;
  glGetProgramivRobustANGLEContextANGLEProc
      glGetProgramivRobustANGLEContextANGLEFn;
  glGetQueryivContextANGLEProc glGetQueryivContextANGLEFn;
  glGetQueryivRobustANGLEContextANGLEProc glGetQueryivRobustANGLEContextANGLEFn;
  glGetQueryObjecti64vContextANGLEProc glGetQueryObjecti64vContextANGLEFn;
  glGetQueryObjecti64vRobustANGLEContextANGLEProc
      glGetQueryObjecti64vRobustANGLEContextANGLEFn;
  glGetQueryObjectivContextANGLEProc glGetQueryObjectivContextANGLEFn;
  glGetQueryObjectivRobustANGLEContextANGLEProc
      glGetQueryObjectivRobustANGLEContextANGLEFn;
  glGetQueryObjectui64vContextANGLEProc glGetQueryObjectui64vContextANGLEFn;
  glGetQueryObjectui64vRobustANGLEContextANGLEProc
      glGetQueryObjectui64vRobustANGLEContextANGLEFn;
  glGetQueryObjectuivContextANGLEProc glGetQueryObjectuivContextANGLEFn;
  glGetQueryObjectuivRobustANGLEContextANGLEProc
      glGetQueryObjectuivRobustANGLEContextANGLEFn;
  glGetRenderbufferParameterivEXTContextANGLEProc
      glGetRenderbufferParameterivEXTContextANGLEFn;
  glGetRenderbufferParameterivRobustANGLEContextANGLEProc
      glGetRenderbufferParameterivRobustANGLEContextANGLEFn;
  glGetSamplerParameterfvContextANGLEProc glGetSamplerParameterfvContextANGLEFn;
  glGetSamplerParameterfvRobustANGLEContextANGLEProc
      glGetSamplerParameterfvRobustANGLEContextANGLEFn;
  glGetSamplerParameterivContextANGLEProc glGetSamplerParameterivContextANGLEFn;
  glGetSamplerParameterivRobustANGLEContextANGLEProc
      glGetSamplerParameterivRobustANGLEContextANGLEFn;
  glGetShaderInfoLogContextANGLEProc glGetShaderInfoLogContextANGLEFn;
  glGetShaderivContextANGLEProc glGetShaderivContextANGLEFn;
  glGetShaderivRobustANGLEContextANGLEProc
      glGetShaderivRobustANGLEContextANGLEFn;
  glGetShaderPrecisionFormatContextANGLEProc
      glGetShaderPrecisionFormatContextANGLEFn;
  glGetShaderSourceContextANGLEProc glGetShaderSourceContextANGLEFn;
  glGetStringContextANGLEProc glGetStringContextANGLEFn;
  glGetStringiContextANGLEProc glGetStringiContextANGLEFn;
  glGetSyncivContextANGLEProc glGetSyncivContextANGLEFn;
  glGetTexLevelParameterfvANGLEContextANGLEProc
      glGetTexLevelParameterfvANGLEContextANGLEFn;
  glGetTexLevelParameterfvRobustANGLEContextANGLEProc
      glGetTexLevelParameterfvRobustANGLEContextANGLEFn;
  glGetTexLevelParameterivANGLEContextANGLEProc
      glGetTexLevelParameterivANGLEContextANGLEFn;
  glGetTexLevelParameterivRobustANGLEContextANGLEProc
      glGetTexLevelParameterivRobustANGLEContextANGLEFn;
  glGetTexParameterfvContextANGLEProc glGetTexParameterfvContextANGLEFn;
  glGetTexParameterfvRobustANGLEContextANGLEProc
      glGetTexParameterfvRobustANGLEContextANGLEFn;
  glGetTexParameterivContextANGLEProc glGetTexParameterivContextANGLEFn;
  glGetTexParameterivRobustANGLEContextANGLEProc
      glGetTexParameterivRobustANGLEContextANGLEFn;
  glGetTransformFeedbackVaryingContextANGLEProc
      glGetTransformFeedbackVaryingContextANGLEFn;
  glGetTranslatedShaderSourceANGLEContextANGLEProc
      glGetTranslatedShaderSourceANGLEContextANGLEFn;
  glGetUniformBlockIndexContextANGLEProc glGetUniformBlockIndexContextANGLEFn;
  glGetUniformfvContextANGLEProc glGetUniformfvContextANGLEFn;
  glGetUniformfvRobustANGLEContextANGLEProc
      glGetUniformfvRobustANGLEContextANGLEFn;
  glGetUniformIndicesContextANGLEProc glGetUniformIndicesContextANGLEFn;
  glGetUniformivContextANGLEProc glGetUniformivContextANGLEFn;
  glGetUniformivRobustANGLEContextANGLEProc
      glGetUniformivRobustANGLEContextANGLEFn;
  glGetUniformLocationContextANGLEProc glGetUniformLocationContextANGLEFn;
  glGetUniformuivContextANGLEProc glGetUniformuivContextANGLEFn;
  glGetUniformuivRobustANGLEContextANGLEProc
      glGetUniformuivRobustANGLEContextANGLEFn;
  glGetVertexAttribfvContextANGLEProc glGetVertexAttribfvContextANGLEFn;
  glGetVertexAttribfvRobustANGLEContextANGLEProc
      glGetVertexAttribfvRobustANGLEContextANGLEFn;
  glGetVertexAttribIivRobustANGLEContextANGLEProc
      glGetVertexAttribIivRobustANGLEContextANGLEFn;
  glGetVertexAttribIuivRobustANGLEContextANGLEProc
      glGetVertexAttribIuivRobustANGLEContextANGLEFn;
  glGetVertexAttribivContextANGLEProc glGetVertexAttribivContextANGLEFn;
  glGetVertexAttribivRobustANGLEContextANGLEProc
      glGetVertexAttribivRobustANGLEContextANGLEFn;
  glGetVertexAttribPointervContextANGLEProc
      glGetVertexAttribPointervContextANGLEFn;
  glGetVertexAttribPointervRobustANGLEContextANGLEProc
      glGetVertexAttribPointervRobustANGLEContextANGLEFn;
  glHintContextANGLEProc glHintContextANGLEFn;
  glImportMemoryFdEXTContextANGLEProc glImportMemoryFdEXTContextANGLEFn;
  glImportMemoryWin32HandleEXTContextANGLEProc
      glImportMemoryWin32HandleEXTContextANGLEFn;
  glImportMemoryZirconHandleANGLEContextANGLEProc
      glImportMemoryZirconHandleANGLEContextANGLEFn;
  glImportSemaphoreFdEXTContextANGLEProc glImportSemaphoreFdEXTContextANGLEFn;
  glImportSemaphoreWin32HandleEXTContextANGLEProc
      glImportSemaphoreWin32HandleEXTContextANGLEFn;
  glImportSemaphoreZirconHandleANGLEContextANGLEProc
      glImportSemaphoreZirconHandleANGLEContextANGLEFn;
  glInsertEventMarkerEXTContextANGLEProc glInsertEventMarkerEXTContextANGLEFn;
  glInvalidateFramebufferContextANGLEProc glInvalidateFramebufferContextANGLEFn;
  glInvalidateSubFramebufferContextANGLEProc
      glInvalidateSubFramebufferContextANGLEFn;
  glInvalidateTextureANGLEContextANGLEProc
      glInvalidateTextureANGLEContextANGLEFn;
  glIsBufferContextANGLEProc glIsBufferContextANGLEFn;
  glIsEnabledContextANGLEProc glIsEnabledContextANGLEFn;
  glIsEnablediOESContextANGLEProc glIsEnablediOESContextANGLEFn;
  glIsFenceNVContextANGLEProc glIsFenceNVContextANGLEFn;
  glIsFramebufferEXTContextANGLEProc glIsFramebufferEXTContextANGLEFn;
  glIsProgramContextANGLEProc glIsProgramContextANGLEFn;
  glIsQueryContextANGLEProc glIsQueryContextANGLEFn;
  glIsRenderbufferEXTContextANGLEProc glIsRenderbufferEXTContextANGLEFn;
  glIsSamplerContextANGLEProc glIsSamplerContextANGLEFn;
  glIsShaderContextANGLEProc glIsShaderContextANGLEFn;
  glIsSyncContextANGLEProc glIsSyncContextANGLEFn;
  glIsTextureContextANGLEProc glIsTextureContextANGLEFn;
  glIsTransformFeedbackContextANGLEProc glIsTransformFeedbackContextANGLEFn;
  glIsVertexArrayOESContextANGLEProc glIsVertexArrayOESContextANGLEFn;
  glLineWidthContextANGLEProc glLineWidthContextANGLEFn;
  glLinkProgramContextANGLEProc glLinkProgramContextANGLEFn;
  glMapBufferContextANGLEProc glMapBufferContextANGLEFn;
  glMapBufferRangeContextANGLEProc glMapBufferRangeContextANGLEFn;
  glMaxShaderCompilerThreadsKHRContextANGLEProc
      glMaxShaderCompilerThreadsKHRContextANGLEFn;
  glMemoryObjectParameterivEXTContextANGLEProc
      glMemoryObjectParameterivEXTContextANGLEFn;
  glMultiDrawArraysANGLEContextANGLEProc glMultiDrawArraysANGLEContextANGLEFn;
  glMultiDrawArraysInstancedANGLEContextANGLEProc
      glMultiDrawArraysInstancedANGLEContextANGLEFn;
  glMultiDrawArraysInstancedBaseInstanceANGLEContextANGLEProc
      glMultiDrawArraysInstancedBaseInstanceANGLEContextANGLEFn;
  glMultiDrawElementsANGLEContextANGLEProc
      glMultiDrawElementsANGLEContextANGLEFn;
  glMultiDrawElementsInstancedANGLEContextANGLEProc
      glMultiDrawElementsInstancedANGLEContextANGLEFn;
  glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEProc
      glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEContextANGLEFn;
  glObjectLabelKHRContextANGLEProc glObjectLabelKHRContextANGLEFn;
  glObjectPtrLabelKHRContextANGLEProc glObjectPtrLabelKHRContextANGLEFn;
  glPatchParameteriOESContextANGLEProc glPatchParameteriOESContextANGLEFn;
  glPauseTransformFeedbackContextANGLEProc
      glPauseTransformFeedbackContextANGLEFn;
  glPixelLocalStorageBarrierANGLEContextANGLEProc
      glPixelLocalStorageBarrierANGLEContextANGLEFn;
  glPixelStoreiContextANGLEProc glPixelStoreiContextANGLEFn;
  glPointParameteriContextANGLEProc glPointParameteriContextANGLEFn;
  glPolygonModeContextANGLEProc glPolygonModeContextANGLEFn;
  glPolygonModeANGLEContextANGLEProc glPolygonModeANGLEContextANGLEFn;
  glPolygonOffsetContextANGLEProc glPolygonOffsetContextANGLEFn;
  glPolygonOffsetClampEXTContextANGLEProc glPolygonOffsetClampEXTContextANGLEFn;
  glPopDebugGroupKHRContextANGLEProc glPopDebugGroupKHRContextANGLEFn;
  glPopGroupMarkerEXTContextANGLEProc glPopGroupMarkerEXTContextANGLEFn;
  glPrimitiveRestartIndexContextANGLEProc glPrimitiveRestartIndexContextANGLEFn;
  glProgramBinaryContextANGLEProc glProgramBinaryContextANGLEFn;
  glProgramParameteriContextANGLEProc glProgramParameteriContextANGLEFn;
  glProvokingVertexANGLEContextANGLEProc glProvokingVertexANGLEContextANGLEFn;
  glPushDebugGroupKHRContextANGLEProc glPushDebugGroupKHRContextANGLEFn;
  glPushGroupMarkerEXTContextANGLEProc glPushGroupMarkerEXTContextANGLEFn;
  glQueryCounterContextANGLEProc glQueryCounterContextANGLEFn;
  glReadBufferContextANGLEProc glReadBufferContextANGLEFn;
  glReadPixelsContextANGLEProc glReadPixelsContextANGLEFn;
  glReadPixelsRobustANGLEContextANGLEProc glReadPixelsRobustANGLEContextANGLEFn;
  glReleaseShaderCompilerContextANGLEProc glReleaseShaderCompilerContextANGLEFn;
  glReleaseTexturesANGLEContextANGLEProc glReleaseTexturesANGLEContextANGLEFn;
  glRenderbufferStorageEXTContextANGLEProc
      glRenderbufferStorageEXTContextANGLEFn;
  glRenderbufferStorageMultisampleContextANGLEProc
      glRenderbufferStorageMultisampleContextANGLEFn;
  glRenderbufferStorageMultisampleAdvancedAMDContextANGLEProc
      glRenderbufferStorageMultisampleAdvancedAMDContextANGLEFn;
  glRenderbufferStorageMultisampleEXTContextANGLEProc
      glRenderbufferStorageMultisampleEXTContextANGLEFn;
  glRequestExtensionANGLEContextANGLEProc glRequestExtensionANGLEContextANGLEFn;
  glResumeTransformFeedbackContextANGLEProc
      glResumeTransformFeedbackContextANGLEFn;
  glSampleCoverageContextANGLEProc glSampleCoverageContextANGLEFn;
  glSamplerParameterfContextANGLEProc glSamplerParameterfContextANGLEFn;
  glSamplerParameterfvContextANGLEProc glSamplerParameterfvContextANGLEFn;
  glSamplerParameterfvRobustANGLEContextANGLEProc
      glSamplerParameterfvRobustANGLEContextANGLEFn;
  glSamplerParameteriContextANGLEProc glSamplerParameteriContextANGLEFn;
  glSamplerParameterivContextANGLEProc glSamplerParameterivContextANGLEFn;
  glSamplerParameterivRobustANGLEContextANGLEProc
      glSamplerParameterivRobustANGLEContextANGLEFn;
  glScissorContextANGLEProc glScissorContextANGLEFn;
  glSetFenceNVContextANGLEProc glSetFenceNVContextANGLEFn;
  glShaderBinaryContextANGLEProc glShaderBinaryContextANGLEFn;
  glShaderSourceContextANGLEProc glShaderSourceContextANGLEFn;
  glSignalSemaphoreEXTContextANGLEProc glSignalSemaphoreEXTContextANGLEFn;
  glStartTilingQCOMContextANGLEProc glStartTilingQCOMContextANGLEFn;
  glStencilFuncContextANGLEProc glStencilFuncContextANGLEFn;
  glStencilFuncSeparateContextANGLEProc glStencilFuncSeparateContextANGLEFn;
  glStencilMaskContextANGLEProc glStencilMaskContextANGLEFn;
  glStencilMaskSeparateContextANGLEProc glStencilMaskSeparateContextANGLEFn;
  glStencilOpContextANGLEProc glStencilOpContextANGLEFn;
  glStencilOpSeparateContextANGLEProc glStencilOpSeparateContextANGLEFn;
  glTestFenceNVContextANGLEProc glTestFenceNVContextANGLEFn;
  glTexBufferOESContextANGLEProc glTexBufferOESContextANGLEFn;
  glTexBufferRangeOESContextANGLEProc glTexBufferRangeOESContextANGLEFn;
  glTexImage2DContextANGLEProc glTexImage2DContextANGLEFn;
  glTexImage2DExternalANGLEContextANGLEProc
      glTexImage2DExternalANGLEContextANGLEFn;
  glTexImage2DRobustANGLEContextANGLEProc glTexImage2DRobustANGLEContextANGLEFn;
  glTexImage3DContextANGLEProc glTexImage3DContextANGLEFn;
  glTexImage3DRobustANGLEContextANGLEProc glTexImage3DRobustANGLEContextANGLEFn;
  glTexParameterfContextANGLEProc glTexParameterfContextANGLEFn;
  glTexParameterfvContextANGLEProc glTexParameterfvContextANGLEFn;
  glTexParameterfvRobustANGLEContextANGLEProc
      glTexParameterfvRobustANGLEContextANGLEFn;
  glTexParameteriContextANGLEProc glTexParameteriContextANGLEFn;
  glTexParameterivContextANGLEProc glTexParameterivContextANGLEFn;
  glTexParameterivRobustANGLEContextANGLEProc
      glTexParameterivRobustANGLEContextANGLEFn;
  glTexStorage2DEXTContextANGLEProc glTexStorage2DEXTContextANGLEFn;
  glTexStorage3DContextANGLEProc glTexStorage3DContextANGLEFn;
  glTexStorageMem2DEXTContextANGLEProc glTexStorageMem2DEXTContextANGLEFn;
  glTexStorageMemFlags2DANGLEContextANGLEProc
      glTexStorageMemFlags2DANGLEContextANGLEFn;
  glTexSubImage2DContextANGLEProc glTexSubImage2DContextANGLEFn;
  glTexSubImage2DRobustANGLEContextANGLEProc
      glTexSubImage2DRobustANGLEContextANGLEFn;
  glTexSubImage3DContextANGLEProc glTexSubImage3DContextANGLEFn;
  glTexSubImage3DRobustANGLEContextANGLEProc
      glTexSubImage3DRobustANGLEContextANGLEFn;
  glTransformFeedbackVaryingsContextANGLEProc
      glTransformFeedbackVaryingsContextANGLEFn;
  glTrimMemoryANGLEContextANGLEProc glTrimMemoryANGLEContextANGLEFn;
  glUniform1fContextANGLEProc glUniform1fContextANGLEFn;
  glUniform1fvContextANGLEProc glUniform1fvContextANGLEFn;
  glUniform1iContextANGLEProc glUniform1iContextANGLEFn;
  glUniform1ivContextANGLEProc glUniform1ivContextANGLEFn;
  glUniform1uiContextANGLEProc glUniform1uiContextANGLEFn;
  glUniform1uivContextANGLEProc glUniform1uivContextANGLEFn;
  glUniform2fContextANGLEProc glUniform2fContextANGLEFn;
  glUniform2fvContextANGLEProc glUniform2fvContextANGLEFn;
  glUniform2iContextANGLEProc glUniform2iContextANGLEFn;
  glUniform2ivContextANGLEProc glUniform2ivContextANGLEFn;
  glUniform2uiContextANGLEProc glUniform2uiContextANGLEFn;
  glUniform2uivContextANGLEProc glUniform2uivContextANGLEFn;
  glUniform3fContextANGLEProc glUniform3fContextANGLEFn;
  glUniform3fvContextANGLEProc glUniform3fvContextANGLEFn;
  glUniform3iContextANGLEProc glUniform3iContextANGLEFn;
  glUniform3ivContextANGLEProc glUniform3ivContextANGLEFn;
  glUniform3uiContextANGLEProc glUniform3uiContextANGLEFn;
  glUniform3uivContextANGLEProc glUniform3uivContextANGLEFn;
  glUniform4fContextANGLEProc glUniform4fContextANGLEFn;
  glUniform4fvContextANGLEProc glUniform4fvContextANGLEFn;
  glUniform4iContextANGLEProc glUniform4iContextANGLEFn;
  glUniform4ivContextANGLEProc glUniform4ivContextANGLEFn;
  glUniform4uiContextANGLEProc glUniform4uiContextANGLEFn;
  glUniform4uivContextANGLEProc glUniform4uivContextANGLEFn;
  glUniformBlockBindingContextANGLEProc glUniformBlockBindingContextANGLEFn;
  glUniformMatrix2fvContextANGLEProc glUniformMatrix2fvContextANGLEFn;
  glUniformMatrix2x3fvContextANGLEProc glUniformMatrix2x3fvContextANGLEFn;
  glUniformMatrix2x4fvContextANGLEProc glUniformMatrix2x4fvContextANGLEFn;
  glUniformMatrix3fvContextANGLEProc glUniformMatrix3fvContextANGLEFn;
  glUniformMatrix3x2fvContextANGLEProc glUniformMatrix3x2fvContextANGLEFn;
  glUniformMatrix3x4fvContextANGLEProc glUniformMatrix3x4fvContextANGLEFn;
  glUniformMatrix4fvContextANGLEProc glUniformMatrix4fvContextANGLEFn;
  glUniformMatrix4x2fvContextANGLEProc glUniformMatrix4x2fvContextANGLEFn;
  glUniformMatrix4x3fvContextANGLEProc glUniformMatrix4x3fvContextANGLEFn;
  glUnmapBufferContextANGLEProc glUnmapBufferContextANGLEFn;
  glUseProgramContextANGLEProc glUseProgramContextANGLEFn;
  glValidateProgramContextANGLEProc glValidateProgramContextANGLEFn;
  glVertexAttrib1fContextANGLEProc glVertexAttrib1fContextANGLEFn;
  glVertexAttrib1fvContextANGLEProc glVertexAttrib1fvContextANGLEFn;
  glVertexAttrib2fContextANGLEProc glVertexAttrib2fContextANGLEFn;
  glVertexAttrib2fvContextANGLEProc glVertexAttrib2fvContextANGLEFn;
  glVertexAttrib3fContextANGLEProc glVertexAttrib3fContextANGLEFn;
  glVertexAttrib3fvContextANGLEProc glVertexAttrib3fvContextANGLEFn;
  glVertexAttrib4fContextANGLEProc glVertexAttrib4fContextANGLEFn;
  glVertexAttrib4fvContextANGLEProc glVertexAttrib4fvContextANGLEFn;
  glVertexAttribDivisorANGLEContextANGLEProc
      glVertexAttribDivisorANGLEContextANGLEFn;
  glVertexAttribI4iContextANGLEProc glVertexAttribI4iContextANGLEFn;
  glVertexAttribI4ivContextANGLEProc glVertexAttribI4ivContextANGLEFn;
  glVertexAttribI4uiContextANGLEProc glVertexAttribI4uiContextANGLEFn;
  glVertexAttribI4uivContextANGLEProc glVertexAttribI4uivContextANGLEFn;
  glVertexAttribIPointerContextANGLEProc glVertexAttribIPointerContextANGLEFn;
  glVertexAttribPointerContextANGLEProc glVertexAttribPointerContextANGLEFn;
  glViewportContextANGLEProc glViewportContextANGLEFn;
  glWaitSemaphoreEXTContextANGLEProc glWaitSemaphoreEXTContextANGLEFn;
  glWaitSyncContextANGLEProc glWaitSyncContextANGLEFn;
  glWindowRectanglesEXTContextANGLEProc glWindowRectanglesEXTContextANGLEFn;
};

class GL_EXPORT GLApi {
 public:
  GLApi();
  virtual ~GLApi();

  virtual void SetDisabledExtensions(const std::string& disabled_extensions) {}

  virtual void glAcquireTexturesANGLEFn(GLuint numTextures,
                                        const GLuint* textures,
                                        const GLenum* layouts) = 0;
  virtual void glActiveTextureFn(GLenum texture) = 0;
  virtual void glAttachShaderFn(GLuint program, GLuint shader) = 0;
  virtual void glBeginPixelLocalStorageANGLEFn(GLsizei n,
                                               const GLenum* loadops) = 0;
  virtual void glBeginQueryFn(GLenum target, GLuint id) = 0;
  virtual void glBeginTransformFeedbackFn(GLenum primitiveMode) = 0;
  virtual void glBindAttribLocationFn(GLuint program,
                                      GLuint index,
                                      const char* name) = 0;
  virtual void glBindBufferFn(GLenum target, GLuint buffer) = 0;
  virtual void glBindBufferBaseFn(GLenum target,
                                  GLuint index,
                                  GLuint buffer) = 0;
  virtual void glBindBufferRangeFn(GLenum target,
                                   GLuint index,
                                   GLuint buffer,
                                   GLintptr offset,
                                   GLsizeiptr size) = 0;
  virtual void glBindFragDataLocationFn(GLuint program,
                                        GLuint colorNumber,
                                        const char* name) = 0;
  virtual void glBindFragDataLocationIndexedFn(GLuint program,
                                               GLuint colorNumber,
                                               GLuint index,
                                               const char* name) = 0;
  virtual void glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) = 0;
  virtual void glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) = 0;
  virtual void glBindSamplerFn(GLuint unit, GLuint sampler) = 0;
  virtual void glBindTextureFn(GLenum target, GLuint texture) = 0;
  virtual void glBindTransformFeedbackFn(GLenum target, GLuint id) = 0;
  virtual void glBindUniformLocationCHROMIUMFn(GLuint program,
                                               GLint location,
                                               const char* name) = 0;
  virtual void glBindVertexArrayOESFn(GLuint array) = 0;
  virtual void glBlendBarrierKHRFn(void) = 0;
  virtual void glBlendColorFn(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) = 0;
  virtual void glBlendEquationFn(GLenum mode) = 0;
  virtual void glBlendEquationiOESFn(GLuint buf, GLenum mode) = 0;
  virtual void glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) = 0;
  virtual void glBlendEquationSeparateiOESFn(GLuint buf,
                                             GLenum modeRGB,
                                             GLenum modeAlpha) = 0;
  virtual void glBlendFuncFn(GLenum sfactor, GLenum dfactor) = 0;
  virtual void glBlendFunciOESFn(GLuint buf,
                                 GLenum sfactor,
                                 GLenum dfactor) = 0;
  virtual void glBlendFuncSeparateFn(GLenum srcRGB,
                                     GLenum dstRGB,
                                     GLenum srcAlpha,
                                     GLenum dstAlpha) = 0;
  virtual void glBlendFuncSeparateiOESFn(GLuint buf,
                                         GLenum srcRGB,
                                         GLenum dstRGB,
                                         GLenum srcAlpha,
                                         GLenum dstAlpha) = 0;
  virtual void glBlitFramebufferFn(GLint srcX0,
                                   GLint srcY0,
                                   GLint srcX1,
                                   GLint srcY1,
                                   GLint dstX0,
                                   GLint dstY0,
                                   GLint dstX1,
                                   GLint dstY1,
                                   GLbitfield mask,
                                   GLenum filter) = 0;
  virtual void glBlobCacheCallbacksANGLEFn(GLSETBLOBPROCANGLE set,
                                           GLGETBLOBPROCANGLE get,
                                           const void* userData) = 0;
  virtual void glBufferDataFn(GLenum target,
                              GLsizeiptr size,
                              const void* data,
                              GLenum usage) = 0;
  virtual void glBufferSubDataFn(GLenum target,
                                 GLintptr offset,
                                 GLsizeiptr size,
                                 const void* data) = 0;
  virtual GLenum glCheckFramebufferStatusEXTFn(GLenum target) = 0;
  virtual void glClearFn(GLbitfield mask) = 0;
  virtual void glClearBufferfiFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat depth,
                                 GLint stencil) = 0;
  virtual void glClearBufferfvFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat* value) = 0;
  virtual void glClearBufferivFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint* value) = 0;
  virtual void glClearBufferuivFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint* value) = 0;
  virtual void glClearColorFn(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) = 0;
  virtual void glClearDepthFn(GLclampd depth) = 0;
  virtual void glClearDepthfFn(GLclampf depth) = 0;
  virtual void glClearStencilFn(GLint s) = 0;
  virtual void glClearTexImageFn(GLuint texture,
                                 GLint level,
                                 GLenum format,
                                 GLenum type,
                                 const GLvoid* data) = 0;
  virtual void glClearTexSubImageFn(GLuint texture,
                                    GLint level,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLint zoffset,
                                    GLint width,
                                    GLint height,
                                    GLint depth,
                                    GLenum format,
                                    GLenum type,
                                    const GLvoid* data) = 0;
  virtual GLenum glClientWaitSyncFn(GLsync sync,
                                    GLbitfield flags,
                                    GLuint64 timeout) = 0;
  virtual void glClipControlEXTFn(GLenum origin, GLenum depth) = 0;
  virtual void glColorMaskFn(GLboolean red,
                             GLboolean green,
                             GLboolean blue,
                             GLboolean alpha) = 0;
  virtual void glColorMaskiOESFn(GLuint buf,
                                 GLboolean red,
                                 GLboolean green,
                                 GLboolean blue,
                                 GLboolean alpha) = 0;
  virtual void glCompileShaderFn(GLuint shader) = 0;
  virtual void glCompressedTexImage2DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void* data) = 0;
  virtual void glCompressedTexImage3DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void* data) = 0;
  virtual void glCompressedTexSubImage2DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void* data) = 0;
  virtual void glCompressedTexSubImage3DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void* data) = 0;
  virtual void glCopyBufferSubDataFn(GLenum readTarget,
                                     GLenum writeTarget,
                                     GLintptr readOffset,
                                     GLintptr writeOffset,
                                     GLsizeiptr size) = 0;
  virtual void glCopySubTextureCHROMIUMFn(GLuint sourceId,
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
                                          GLboolean unpackUnmultiplyAlpha) = 0;
  virtual void glCopyTexImage2DFn(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border) = 0;
  virtual void glCopyTexSubImage2DFn(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) = 0;
  virtual void glCopyTexSubImage3DFn(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) = 0;
  virtual void glCopyTextureCHROMIUMFn(GLuint sourceId,
                                       GLint sourceLevel,
                                       GLenum destTarget,
                                       GLuint destId,
                                       GLint destLevel,
                                       GLint internalFormat,
                                       GLenum destType,
                                       GLboolean unpackFlipY,
                                       GLboolean unpackPremultiplyAlpha,
                                       GLboolean unpackUnmultiplyAlpha) = 0;
  virtual void glCreateMemoryObjectsEXTFn(GLsizei n, GLuint* memoryObjects) = 0;
  virtual GLuint glCreateProgramFn(void) = 0;
  virtual GLuint glCreateShaderFn(GLenum type) = 0;
  virtual void glCullFaceFn(GLenum mode) = 0;
  virtual void glDebugMessageCallbackKHRFn(GLDEBUGPROC callback,
                                           const void* userParam) = 0;
  virtual void glDebugMessageControlKHRFn(GLenum source,
                                          GLenum type,
                                          GLenum severity,
                                          GLsizei count,
                                          const GLuint* ids,
                                          GLboolean enabled) = 0;
  virtual void glDebugMessageInsertKHRFn(GLenum source,
                                         GLenum type,
                                         GLuint id,
                                         GLenum severity,
                                         GLsizei length,
                                         const char* buf) = 0;
  virtual void glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) = 0;
  virtual void glDeleteFencesNVFn(GLsizei n, const GLuint* fences) = 0;
  virtual void glDeleteFramebuffersEXTFn(GLsizei n,
                                         const GLuint* framebuffers) = 0;
  virtual void glDeleteMemoryObjectsEXTFn(GLsizei n,
                                          const GLuint* memoryObjects) = 0;
  virtual void glDeleteProgramFn(GLuint program) = 0;
  virtual void glDeleteQueriesFn(GLsizei n, const GLuint* ids) = 0;
  virtual void glDeleteRenderbuffersEXTFn(GLsizei n,
                                          const GLuint* renderbuffers) = 0;
  virtual void glDeleteSamplersFn(GLsizei n, const GLuint* samplers) = 0;
  virtual void glDeleteSemaphoresEXTFn(GLsizei n, const GLuint* semaphores) = 0;
  virtual void glDeleteShaderFn(GLuint shader) = 0;
  virtual void glDeleteSyncFn(GLsync sync) = 0;
  virtual void glDeleteTexturesFn(GLsizei n, const GLuint* textures) = 0;
  virtual void glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) = 0;
  virtual void glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) = 0;
  virtual void glDepthFuncFn(GLenum func) = 0;
  virtual void glDepthMaskFn(GLboolean flag) = 0;
  virtual void glDepthRangeFn(GLclampd zNear, GLclampd zFar) = 0;
  virtual void glDepthRangefFn(GLclampf zNear, GLclampf zFar) = 0;
  virtual void glDetachShaderFn(GLuint program, GLuint shader) = 0;
  virtual void glDisableFn(GLenum cap) = 0;
  virtual void glDisableiOESFn(GLenum target, GLuint index) = 0;
  virtual void glDisableVertexAttribArrayFn(GLuint index) = 0;
  virtual void glDiscardFramebufferEXTFn(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) = 0;
  virtual void glDrawArraysFn(GLenum mode, GLint first, GLsizei count) = 0;
  virtual void glDrawArraysInstancedANGLEFn(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount) = 0;
  virtual void glDrawArraysInstancedBaseInstanceANGLEFn(
      GLenum mode,
      GLint first,
      GLsizei count,
      GLsizei primcount,
      GLuint baseinstance) = 0;
  virtual void glDrawBufferFn(GLenum mode) = 0;
  virtual void glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) = 0;
  virtual void glDrawElementsFn(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void* indices) = 0;
  virtual void glDrawElementsInstancedANGLEFn(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices,
                                              GLsizei primcount) = 0;
  virtual void glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
      GLenum mode,
      GLsizei count,
      GLenum type,
      const void* indices,
      GLsizei primcount,
      GLint baseVertex,
      GLuint baseInstance) = 0;
  virtual void glDrawRangeElementsFn(GLenum mode,
                                     GLuint start,
                                     GLuint end,
                                     GLsizei count,
                                     GLenum type,
                                     const void* indices) = 0;
  virtual void glEGLImageTargetRenderbufferStorageOESFn(
      GLenum target,
      GLeglImageOES image) = 0;
  virtual void glEGLImageTargetTexStorageEXTFn(GLenum target,
                                               GLeglImageOES image,
                                               const GLint* attrib_list) = 0;
  virtual void glEGLImageTargetTexture2DOESFn(GLenum target,
                                              GLeglImageOES image) = 0;
  virtual void glEnableFn(GLenum cap) = 0;
  virtual void glEnableiOESFn(GLenum target, GLuint index) = 0;
  virtual void glEnableVertexAttribArrayFn(GLuint index) = 0;
  virtual void glEndPixelLocalStorageANGLEFn(GLsizei n,
                                             const GLenum* storeops) = 0;
  virtual void glEndPixelLocalStorageImplicitANGLEFn() = 0;
  virtual void glEndQueryFn(GLenum target) = 0;
  virtual void glEndTilingQCOMFn(GLbitfield preserveMask) = 0;
  virtual void glEndTransformFeedbackFn(void) = 0;
  virtual GLsync glFenceSyncFn(GLenum condition, GLbitfield flags) = 0;
  virtual void glFinishFn(void) = 0;
  virtual void glFinishFenceNVFn(GLuint fence) = 0;
  virtual void glFlushFn(void) = 0;
  virtual void glFlushMappedBufferRangeFn(GLenum target,
                                          GLintptr offset,
                                          GLsizeiptr length) = 0;
  virtual void glFramebufferMemorylessPixelLocalStorageANGLEFn(
      GLint plane,
      GLenum internalformat,
      GLbitfield usage) = 0;
  virtual void glFramebufferParameteriMESAFn(GLenum target,
                                             GLenum pname,
                                             GLint param) = 0;
  virtual void glFramebufferPixelLocalClearValuefvANGLEFn(
      GLint plane,
      const GLfloat* value) = 0;
  virtual void glFramebufferPixelLocalClearValueivANGLEFn(
      GLint plane,
      const GLint* value) = 0;
  virtual void glFramebufferPixelLocalClearValueuivANGLEFn(
      GLint plane,
      const GLuint* value) = 0;
  virtual void glFramebufferPixelLocalStorageInterruptANGLEFn() = 0;
  virtual void glFramebufferPixelLocalStorageRestoreANGLEFn() = 0;
  virtual void glFramebufferRenderbufferEXTFn(GLenum target,
                                              GLenum attachment,
                                              GLenum renderbuffertarget,
                                              GLuint renderbuffer) = 0;
  virtual void glFramebufferTexture2DEXTFn(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level) = 0;
  virtual void glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                      GLenum attachment,
                                                      GLenum textarget,
                                                      GLuint texture,
                                                      GLint level,
                                                      GLsizei samples) = 0;
  virtual void glFramebufferTextureLayerFn(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLint layer) = 0;
  virtual void glFramebufferTextureMultiviewOVRFn(GLenum target,
                                                  GLenum attachment,
                                                  GLuint texture,
                                                  GLint level,
                                                  GLint baseViewIndex,
                                                  GLsizei numViews) = 0;
  virtual void glFramebufferTexturePixelLocalStorageANGLEFn(
      GLint plane,
      GLuint backingtexture,
      GLint level,
      GLint layer,
      GLbitfield usage) = 0;
  virtual void glFrontFaceFn(GLenum mode) = 0;
  virtual void glGenBuffersARBFn(GLsizei n, GLuint* buffers) = 0;
  virtual void glGenerateMipmapEXTFn(GLenum target) = 0;
  virtual void glGenFencesNVFn(GLsizei n, GLuint* fences) = 0;
  virtual void glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) = 0;
  virtual void glGenQueriesFn(GLsizei n, GLuint* ids) = 0;
  virtual void glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) = 0;
  virtual void glGenSamplersFn(GLsizei n, GLuint* samplers) = 0;
  virtual void glGenSemaphoresEXTFn(GLsizei n, GLuint* semaphores) = 0;
  virtual void glGenTexturesFn(GLsizei n, GLuint* textures) = 0;
  virtual void glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) = 0;
  virtual void glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) = 0;
  virtual void glGetActiveAttribFn(GLuint program,
                                   GLuint index,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLint* size,
                                   GLenum* type,
                                   char* name) = 0;
  virtual void glGetActiveUniformFn(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) = 0;
  virtual void glGetActiveUniformBlockivFn(GLuint program,
                                           GLuint uniformBlockIndex,
                                           GLenum pname,
                                           GLint* params) = 0;
  virtual void glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                                      GLuint uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) = 0;
  virtual void glGetActiveUniformBlockNameFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             char* uniformBlockName) = 0;
  virtual void glGetActiveUniformsivFn(GLuint program,
                                       GLsizei uniformCount,
                                       const GLuint* uniformIndices,
                                       GLenum pname,
                                       GLint* params) = 0;
  virtual void glGetAttachedShadersFn(GLuint program,
                                      GLsizei maxcount,
                                      GLsizei* count,
                                      GLuint* shaders) = 0;
  virtual GLint glGetAttribLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetBooleanvFn(GLenum pname, GLboolean* params) = 0;
  virtual void glGetBooleanvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLboolean* data) = 0;
  virtual void glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint64* params) = 0;
  virtual void glGetBufferParameterivFn(GLenum target,
                                        GLenum pname,
                                        GLint* params) = 0;
  virtual void glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) = 0;
  virtual void glGetBufferPointervRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                void** params) = 0;
  virtual GLuint glGetDebugMessageLogKHRFn(GLuint count,
                                           GLsizei bufSize,
                                           GLenum* sources,
                                           GLenum* types,
                                           GLuint* ids,
                                           GLenum* severities,
                                           GLsizei* lengths,
                                           char* messageLog) = 0;
  virtual GLenum glGetErrorFn(void) = 0;
  virtual void glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) = 0;
  virtual void glGetFloatvFn(GLenum pname, GLfloat* params) = 0;
  virtual void glGetFloatvRobustANGLEFn(GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLfloat* data) = 0;
  virtual GLint glGetFragDataIndexFn(GLuint program, const char* name) = 0;
  virtual GLint glGetFragDataLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                          GLenum attachment,
                                                          GLenum pname,
                                                          GLint* params) = 0;
  virtual void glGetFramebufferAttachmentParameterivRobustANGLEFn(
      GLenum target,
      GLenum attachment,
      GLenum pname,
      GLsizei bufSize,
      GLsizei* length,
      GLint* params) = 0;
  virtual void glGetFramebufferPixelLocalStorageParameterfvRobustANGLEFn(
      GLint plane,
      GLenum pname,
      GLsizei paramCount,
      GLsizei* length,
      GLfloat* params) = 0;
  virtual void glGetFramebufferPixelLocalStorageParameterivRobustANGLEFn(
      GLint plane,
      GLenum pname,
      GLsizei paramCount,
      GLsizei* length,
      GLint* params) = 0;
  virtual void glGetFramebufferPixelLocalStorageParameteruivRobustANGLEFn(
      GLint plane,
      GLenum pname,
      GLsizei paramCount,
      GLsizei* length,
      GLuint* params) = 0;
  virtual GLenum glGetGraphicsResetStatusARBFn(void) = 0;
  virtual void glGetInteger64i_vFn(GLenum target,
                                   GLuint index,
                                   GLint64* data) = 0;
  virtual void glGetInteger64i_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) = 0;
  virtual void glGetInteger64vFn(GLenum pname, GLint64* params) = 0;
  virtual void glGetInteger64vRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint64* data) = 0;
  virtual void glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) = 0;
  virtual void glGetIntegeri_vRobustANGLEFn(GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) = 0;
  virtual void glGetIntegervFn(GLenum pname, GLint* params) = 0;
  virtual void glGetIntegervRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* data) = 0;
  virtual void glGetInternalformativFn(GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLint* params) = 0;
  virtual void glGetInternalformativRobustANGLEFn(GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) = 0;
  virtual void glGetInternalformatSampleivNVFn(GLenum target,
                                               GLenum internalformat,
                                               GLsizei samples,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLint* params) = 0;
  virtual void glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLfloat* val) = 0;
  virtual void glGetObjectLabelKHRFn(GLenum identifier,
                                     GLuint name,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     char* label) = 0;
  virtual void glGetObjectPtrLabelKHRFn(void* ptr,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        char* label) = 0;
  virtual void glGetPointervKHRFn(GLenum pname, void** params) = 0;
  virtual void glGetProgramBinaryFn(GLuint program,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLenum* binaryFormat,
                                    GLvoid* binary) = 0;
  virtual void glGetProgramInfoLogFn(GLuint program,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) = 0;
  virtual void glGetProgramivFn(GLuint program,
                                GLenum pname,
                                GLint* params) = 0;
  virtual void glGetProgramivRobustANGLEFn(GLuint program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) = 0;
  virtual void glGetQueryivFn(GLenum target, GLenum pname, GLint* params) = 0;
  virtual void glGetQueryivRobustANGLEFn(GLenum target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) = 0;
  virtual void glGetQueryObjecti64vFn(GLuint id,
                                      GLenum pname,
                                      GLint64* params) = 0;
  virtual void glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint64* params) = 0;
  virtual void glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) = 0;
  virtual void glGetQueryObjectivRobustANGLEFn(GLuint id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) = 0;
  virtual void glGetQueryObjectui64vFn(GLuint id,
                                       GLenum pname,
                                       GLuint64* params) = 0;
  virtual void glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint64* params) = 0;
  virtual void glGetQueryObjectuivFn(GLuint id,
                                     GLenum pname,
                                     GLuint* params) = 0;
  virtual void glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLuint* params) = 0;
  virtual void glGetRenderbufferParameterivEXTFn(GLenum target,
                                                 GLenum pname,
                                                 GLint* params) = 0;
  virtual void glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) = 0;
  virtual void glGetSamplerParameterfvFn(GLuint sampler,
                                         GLenum pname,
                                         GLfloat* params) = 0;
  virtual void glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLfloat* params) = 0;
  virtual void glGetSamplerParameterivFn(GLuint sampler,
                                         GLenum pname,
                                         GLint* params) = 0;
  virtual void glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) = 0;
  virtual void glGetShaderInfoLogFn(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* infolog) = 0;
  virtual void glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) = 0;
  virtual void glGetShaderivRobustANGLEFn(GLuint shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) = 0;
  virtual void glGetShaderPrecisionFormatFn(GLenum shadertype,
                                            GLenum precisiontype,
                                            GLint* range,
                                            GLint* precision) = 0;
  virtual void glGetShaderSourceFn(GLuint shader,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   char* source) = 0;
  virtual const GLubyte* glGetStringFn(GLenum name) = 0;
  virtual const GLubyte* glGetStringiFn(GLenum name, GLuint index) = 0;
  virtual void glGetSyncivFn(GLsync sync,
                             GLenum pname,
                             GLsizei bufSize,
                             GLsizei* length,
                             GLint* values) = 0;
  virtual void glGetTexLevelParameterfvANGLEFn(GLenum target,
                                               GLint level,
                                               GLenum pname,
                                               GLfloat* params) = 0;
  virtual void glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) = 0;
  virtual void glGetTexLevelParameterivANGLEFn(GLenum target,
                                               GLint level,
                                               GLenum pname,
                                               GLint* params) = 0;
  virtual void glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) = 0;
  virtual void glGetTexParameterfvFn(GLenum target,
                                     GLenum pname,
                                     GLfloat* params) = 0;
  virtual void glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* params) = 0;
  virtual void glGetTexParameterivFn(GLenum target,
                                     GLenum pname,
                                     GLint* params) = 0;
  virtual void glGetTexParameterivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) = 0;
  virtual void glGetTransformFeedbackVaryingFn(GLuint program,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* size,
                                               GLenum* type,
                                               char* name) = 0;
  virtual void glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                  GLsizei bufsize,
                                                  GLsizei* length,
                                                  char* source) = 0;
  virtual GLuint glGetUniformBlockIndexFn(GLuint program,
                                          const char* uniformBlockName) = 0;
  virtual void glGetUniformfvFn(GLuint program,
                                GLint location,
                                GLfloat* params) = 0;
  virtual void glGetUniformfvRobustANGLEFn(GLuint program,
                                           GLint location,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLfloat* params) = 0;
  virtual void glGetUniformIndicesFn(GLuint program,
                                     GLsizei uniformCount,
                                     const char* const* uniformNames,
                                     GLuint* uniformIndices) = 0;
  virtual void glGetUniformivFn(GLuint program,
                                GLint location,
                                GLint* params) = 0;
  virtual void glGetUniformivRobustANGLEFn(GLuint program,
                                           GLint location,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) = 0;
  virtual GLint glGetUniformLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetUniformuivFn(GLuint program,
                                 GLint location,
                                 GLuint* params) = 0;
  virtual void glGetUniformuivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLuint* params) = 0;
  virtual void glGetVertexAttribfvFn(GLuint index,
                                     GLenum pname,
                                     GLfloat* params) = 0;
  virtual void glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* params) = 0;
  virtual void glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) = 0;
  virtual void glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) = 0;
  virtual void glGetVertexAttribivFn(GLuint index,
                                     GLenum pname,
                                     GLint* params) = 0;
  virtual void glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) = 0;
  virtual void glGetVertexAttribPointervFn(GLuint index,
                                           GLenum pname,
                                           void** pointer) = 0;
  virtual void glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** pointer) = 0;
  virtual void glHintFn(GLenum target, GLenum mode) = 0;
  virtual void glImportMemoryFdEXTFn(GLuint memory,
                                     GLuint64 size,
                                     GLenum handleType,
                                     GLint fd) = 0;
  virtual void glImportMemoryWin32HandleEXTFn(GLuint memory,
                                              GLuint64 size,
                                              GLenum handleType,
                                              void* handle) = 0;
  virtual void glImportMemoryZirconHandleANGLEFn(GLuint memory,
                                                 GLuint64 size,
                                                 GLenum handleType,
                                                 GLuint handle) = 0;
  virtual void glImportSemaphoreFdEXTFn(GLuint semaphore,
                                        GLenum handleType,
                                        GLint fd) = 0;
  virtual void glImportSemaphoreWin32HandleEXTFn(GLuint semaphore,
                                                 GLenum handleType,
                                                 void* handle) = 0;
  virtual void glImportSemaphoreZirconHandleANGLEFn(GLuint semaphore,
                                                    GLenum handleType,
                                                    GLuint handle) = 0;
  virtual void glInsertEventMarkerEXTFn(GLsizei length, const char* marker) = 0;
  virtual void glInvalidateFramebufferFn(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) = 0;
  virtual void glInvalidateSubFramebufferFn(GLenum target,
                                            GLsizei numAttachments,
                                            const GLenum* attachments,
                                            GLint x,
                                            GLint y,
                                            GLint width,
                                            GLint height) = 0;
  virtual void glInvalidateTextureANGLEFn(GLenum target) = 0;
  virtual GLboolean glIsBufferFn(GLuint buffer) = 0;
  virtual GLboolean glIsEnabledFn(GLenum cap) = 0;
  virtual GLboolean glIsEnablediOESFn(GLenum target, GLuint index) = 0;
  virtual GLboolean glIsFenceNVFn(GLuint fence) = 0;
  virtual GLboolean glIsFramebufferEXTFn(GLuint framebuffer) = 0;
  virtual GLboolean glIsProgramFn(GLuint program) = 0;
  virtual GLboolean glIsQueryFn(GLuint query) = 0;
  virtual GLboolean glIsRenderbufferEXTFn(GLuint renderbuffer) = 0;
  virtual GLboolean glIsSamplerFn(GLuint sampler) = 0;
  virtual GLboolean glIsShaderFn(GLuint shader) = 0;
  virtual GLboolean glIsSyncFn(GLsync sync) = 0;
  virtual GLboolean glIsTextureFn(GLuint texture) = 0;
  virtual GLboolean glIsTransformFeedbackFn(GLuint id) = 0;
  virtual GLboolean glIsVertexArrayOESFn(GLuint array) = 0;
  virtual void glLineWidthFn(GLfloat width) = 0;
  virtual void glLinkProgramFn(GLuint program) = 0;
  virtual void* glMapBufferFn(GLenum target, GLenum access) = 0;
  virtual void* glMapBufferRangeFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr length,
                                   GLbitfield access) = 0;
  virtual void glMaxShaderCompilerThreadsKHRFn(GLuint count) = 0;
  virtual void glMemoryObjectParameterivEXTFn(GLuint memoryObject,
                                              GLenum pname,
                                              const GLint* param) = 0;
  virtual void glMultiDrawArraysANGLEFn(GLenum mode,
                                        const GLint* firsts,
                                        const GLsizei* counts,
                                        GLsizei drawcount) = 0;
  virtual void glMultiDrawArraysInstancedANGLEFn(GLenum mode,
                                                 const GLint* firsts,
                                                 const GLsizei* counts,
                                                 const GLsizei* instanceCounts,
                                                 GLsizei drawcount) = 0;
  virtual void glMultiDrawArraysInstancedBaseInstanceANGLEFn(
      GLenum mode,
      const GLint* firsts,
      const GLsizei* counts,
      const GLsizei* instanceCounts,
      const GLuint* baseInstances,
      GLsizei drawcount) = 0;
  virtual void glMultiDrawElementsANGLEFn(GLenum mode,
                                          const GLsizei* counts,
                                          GLenum type,
                                          const GLvoid* const* indices,
                                          GLsizei drawcount) = 0;
  virtual void glMultiDrawElementsInstancedANGLEFn(
      GLenum mode,
      const GLsizei* counts,
      GLenum type,
      const GLvoid* const* indices,
      const GLsizei* instanceCounts,
      GLsizei drawcount) = 0;
  virtual void glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
      GLenum mode,
      const GLsizei* counts,
      GLenum type,
      const GLvoid* const* indices,
      const GLsizei* instanceCounts,
      const GLint* baseVertices,
      const GLuint* baseInstances,
      GLsizei drawcount) = 0;
  virtual void glObjectLabelKHRFn(GLenum identifier,
                                  GLuint name,
                                  GLsizei length,
                                  const char* label) = 0;
  virtual void glObjectPtrLabelKHRFn(void* ptr,
                                     GLsizei length,
                                     const char* label) = 0;
  virtual void glPatchParameteriOESFn(GLenum pname, GLint value) = 0;
  virtual void glPauseTransformFeedbackFn(void) = 0;
  virtual void glPixelLocalStorageBarrierANGLEFn() = 0;
  virtual void glPixelStoreiFn(GLenum pname, GLint param) = 0;
  virtual void glPointParameteriFn(GLenum pname, GLint param) = 0;
  virtual void glPolygonModeFn(GLenum face, GLenum mode) = 0;
  virtual void glPolygonModeANGLEFn(GLenum face, GLenum mode) = 0;
  virtual void glPolygonOffsetFn(GLfloat factor, GLfloat units) = 0;
  virtual void glPolygonOffsetClampEXTFn(GLfloat factor,
                                         GLfloat units,
                                         GLfloat clamp) = 0;
  virtual void glPopDebugGroupKHRFn() = 0;
  virtual void glPopGroupMarkerEXTFn(void) = 0;
  virtual void glPrimitiveRestartIndexFn(GLuint index) = 0;
  virtual void glProgramBinaryFn(GLuint program,
                                 GLenum binaryFormat,
                                 const GLvoid* binary,
                                 GLsizei length) = 0;
  virtual void glProgramParameteriFn(GLuint program,
                                     GLenum pname,
                                     GLint value) = 0;
  virtual void glProvokingVertexANGLEFn(GLenum provokeMode) = 0;
  virtual void glPushDebugGroupKHRFn(GLenum source,
                                     GLuint id,
                                     GLsizei length,
                                     const char* message) = 0;
  virtual void glPushGroupMarkerEXTFn(GLsizei length, const char* marker) = 0;
  virtual void glQueryCounterFn(GLuint id, GLenum target) = 0;
  virtual void glReadBufferFn(GLenum src) = 0;
  virtual void glReadPixelsFn(GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLenum format,
                              GLenum type,
                              void* pixels) = 0;
  virtual void glReadPixelsRobustANGLEFn(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLsizei* columns,
                                         GLsizei* rows,
                                         void* pixels) = 0;
  virtual void glReleaseShaderCompilerFn(void) = 0;
  virtual void glReleaseTexturesANGLEFn(GLuint numTextures,
                                        const GLuint* textures,
                                        GLenum* layouts) = 0;
  virtual void glRenderbufferStorageEXTFn(GLenum target,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) = 0;
  virtual void glRenderbufferStorageMultisampleFn(GLenum target,
                                                  GLsizei samples,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height) = 0;
  virtual void glRenderbufferStorageMultisampleAdvancedAMDFn(
      GLenum target,
      GLsizei samples,
      GLsizei storageSamples,
      GLenum internalformat,
      GLsizei width,
      GLsizei height) = 0;
  virtual void glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                     GLsizei samples,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height) = 0;
  virtual void glRequestExtensionANGLEFn(const char* name) = 0;
  virtual void glResumeTransformFeedbackFn(void) = 0;
  virtual void glSampleCoverageFn(GLclampf value, GLboolean invert) = 0;
  virtual void glSamplerParameterfFn(GLuint sampler,
                                     GLenum pname,
                                     GLfloat param) = 0;
  virtual void glSamplerParameterfvFn(GLuint sampler,
                                      GLenum pname,
                                      const GLfloat* params) = 0;
  virtual void glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLfloat* param) = 0;
  virtual void glSamplerParameteriFn(GLuint sampler,
                                     GLenum pname,
                                     GLint param) = 0;
  virtual void glSamplerParameterivFn(GLuint sampler,
                                      GLenum pname,
                                      const GLint* params) = 0;
  virtual void glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLint* param) = 0;
  virtual void glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) = 0;
  virtual void glSetFenceNVFn(GLuint fence, GLenum condition) = 0;
  virtual void glShaderBinaryFn(GLsizei n,
                                const GLuint* shaders,
                                GLenum binaryformat,
                                const void* binary,
                                GLsizei length) = 0;
  virtual void glShaderSourceFn(GLuint shader,
                                GLsizei count,
                                const char* const* str,
                                const GLint* length) = 0;
  virtual void glSignalSemaphoreEXTFn(GLuint semaphore,
                                      GLuint numBufferBarriers,
                                      const GLuint* buffers,
                                      GLuint numTextureBarriers,
                                      const GLuint* textures,
                                      const GLenum* dstLayouts) = 0;
  virtual void glStartTilingQCOMFn(GLuint x,
                                   GLuint y,
                                   GLuint width,
                                   GLuint height,
                                   GLbitfield preserveMask) = 0;
  virtual void glStencilFuncFn(GLenum func, GLint ref, GLuint mask) = 0;
  virtual void glStencilFuncSeparateFn(GLenum face,
                                       GLenum func,
                                       GLint ref,
                                       GLuint mask) = 0;
  virtual void glStencilMaskFn(GLuint mask) = 0;
  virtual void glStencilMaskSeparateFn(GLenum face, GLuint mask) = 0;
  virtual void glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) = 0;
  virtual void glStencilOpSeparateFn(GLenum face,
                                     GLenum fail,
                                     GLenum zfail,
                                     GLenum zpass) = 0;
  virtual GLboolean glTestFenceNVFn(GLuint fence) = 0;
  virtual void glTexBufferOESFn(GLenum target,
                                GLenum internalformat,
                                GLuint buffer) = 0;
  virtual void glTexBufferRangeOESFn(GLenum target,
                                     GLenum internalformat,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) = 0;
  virtual void glTexImage2DFn(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void* pixels) = 0;
  virtual void glTexImage2DExternalANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type) = 0;
  virtual void glTexImage2DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void* pixels) = 0;
  virtual void glTexImage3DFn(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void* pixels) = 0;
  virtual void glTexImage3DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void* pixels) = 0;
  virtual void glTexParameterfFn(GLenum target,
                                 GLenum pname,
                                 GLfloat param) = 0;
  virtual void glTexParameterfvFn(GLenum target,
                                  GLenum pname,
                                  const GLfloat* params) = 0;
  virtual void glTexParameterfvRobustANGLEFn(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLfloat* params) = 0;
  virtual void glTexParameteriFn(GLenum target, GLenum pname, GLint param) = 0;
  virtual void glTexParameterivFn(GLenum target,
                                  GLenum pname,
                                  const GLint* params) = 0;
  virtual void glTexParameterivRobustANGLEFn(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLint* params) = 0;
  virtual void glTexStorage2DEXTFn(GLenum target,
                                   GLsizei levels,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height) = 0;
  virtual void glTexStorage3DFn(GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth) = 0;
  virtual void glTexStorageMem2DEXTFn(GLenum target,
                                      GLsizei levels,
                                      GLenum internalFormat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLuint memory,
                                      GLuint64 offset) = 0;
  virtual void glTexStorageMemFlags2DANGLEFn(
      GLenum target,
      GLsizei levels,
      GLenum internalFormat,
      GLsizei width,
      GLsizei height,
      GLuint memory,
      GLuint64 offset,
      GLbitfield createFlags,
      GLbitfield usageFlags,
      const void* imageCreateInfoPNext) = 0;
  virtual void glTexSubImage2DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) = 0;
  virtual void glTexSubImage2DRobustANGLEFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            const void* pixels) = 0;
  virtual void glTexSubImage3DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) = 0;
  virtual void glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                            const void* pixels) = 0;
  virtual void glTransformFeedbackVaryingsFn(GLuint program,
                                             GLsizei count,
                                             const char* const* varyings,
                                             GLenum bufferMode) = 0;
  virtual void glTrimMemoryANGLEFn(GLenum trimLevel) = 0;
  virtual void glUniform1fFn(GLint location, GLfloat x) = 0;
  virtual void glUniform1fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform1iFn(GLint location, GLint x) = 0;
  virtual void glUniform1ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform1uiFn(GLint location, GLuint v0) = 0;
  virtual void glUniform1uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform2fFn(GLint location, GLfloat x, GLfloat y) = 0;
  virtual void glUniform2fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform2iFn(GLint location, GLint x, GLint y) = 0;
  virtual void glUniform2ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform2uiFn(GLint location, GLuint v0, GLuint v1) = 0;
  virtual void glUniform2uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform3fFn(GLint location,
                             GLfloat x,
                             GLfloat y,
                             GLfloat z) = 0;
  virtual void glUniform3fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform3iFn(GLint location, GLint x, GLint y, GLint z) = 0;
  virtual void glUniform3ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform3uiFn(GLint location,
                              GLuint v0,
                              GLuint v1,
                              GLuint v2) = 0;
  virtual void glUniform3uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform4fFn(GLint location,
                             GLfloat x,
                             GLfloat y,
                             GLfloat z,
                             GLfloat w) = 0;
  virtual void glUniform4fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform4iFn(GLint location,
                             GLint x,
                             GLint y,
                             GLint z,
                             GLint w) = 0;
  virtual void glUniform4ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform4uiFn(GLint location,
                              GLuint v0,
                              GLuint v1,
                              GLuint v2,
                              GLuint v3) = 0;
  virtual void glUniform4uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniformBlockBindingFn(GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLuint uniformBlockBinding) = 0;
  virtual void glUniformMatrix2fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix2x3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix2x4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix3fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix3x2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix3x4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix4fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix4x2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix4x3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual GLboolean glUnmapBufferFn(GLenum target) = 0;
  virtual void glUseProgramFn(GLuint program) = 0;
  virtual void glValidateProgramFn(GLuint program) = 0;
  virtual void glVertexAttrib1fFn(GLuint indx, GLfloat x) = 0;
  virtual void glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) = 0;
  virtual void glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib3fFn(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z) = 0;
  virtual void glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib4fFn(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z,
                                  GLfloat w) = 0;
  virtual void glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) = 0;
  virtual void glVertexAttribI4iFn(GLuint indx,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLint w) = 0;
  virtual void glVertexAttribI4ivFn(GLuint indx, const GLint* values) = 0;
  virtual void glVertexAttribI4uiFn(GLuint indx,
                                    GLuint x,
                                    GLuint y,
                                    GLuint z,
                                    GLuint w) = 0;
  virtual void glVertexAttribI4uivFn(GLuint indx, const GLuint* values) = 0;
  virtual void glVertexAttribIPointerFn(GLuint indx,
                                        GLint size,
                                        GLenum type,
                                        GLsizei stride,
                                        const void* ptr) = 0;
  virtual void glVertexAttribPointerFn(GLuint indx,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLsizei stride,
                                       const void* ptr) = 0;
  virtual void glViewportFn(GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height) = 0;
  virtual void glWaitSemaphoreEXTFn(GLuint semaphore,
                                    GLuint numBufferBarriers,
                                    const GLuint* buffers,
                                    GLuint numTextureBarriers,
                                    const GLuint* textures,
                                    const GLenum* srcLayouts) = 0;
  virtual void glWaitSyncFn(GLsync sync,
                            GLbitfield flags,
                            GLuint64 timeout) = 0;
  virtual void glWindowRectanglesEXTFn(GLenum mode,
                                       GLsizei n,
                                       const GLint* box) = 0;
};

}  // namespace gl

#if BINDINGS_GL_PROTOTYPES
#define glAcquireTexturesANGLE \
  ::gl::g_current_gl_context->glAcquireTexturesANGLEFn
#define glActiveTexture ::gl::g_current_gl_context->glActiveTextureFn
#define glAttachShader ::gl::g_current_gl_context->glAttachShaderFn
#define glBeginPixelLocalStorageANGLE \
  ::gl::g_current_gl_context->glBeginPixelLocalStorageANGLEFn
#define glBeginQuery ::gl::g_current_gl_context->glBeginQueryFn
#define glBeginTransformFeedback \
  ::gl::g_current_gl_context->glBeginTransformFeedbackFn
#define glBindAttribLocation ::gl::g_current_gl_context->glBindAttribLocationFn
#define glBindBuffer ::gl::g_current_gl_context->glBindBufferFn
#define glBindBufferBase ::gl::g_current_gl_context->glBindBufferBaseFn
#define glBindBufferRange ::gl::g_current_gl_context->glBindBufferRangeFn
#define glBindFragDataLocation \
  ::gl::g_current_gl_context->glBindFragDataLocationFn
#define glBindFragDataLocationIndexed \
  ::gl::g_current_gl_context->glBindFragDataLocationIndexedFn
#define glBindFramebufferEXT ::gl::g_current_gl_context->glBindFramebufferEXTFn
#define glBindRenderbufferEXT \
  ::gl::g_current_gl_context->glBindRenderbufferEXTFn
#define glBindSampler ::gl::g_current_gl_context->glBindSamplerFn
#define glBindTexture ::gl::g_current_gl_context->glBindTextureFn
#define glBindTransformFeedback \
  ::gl::g_current_gl_context->glBindTransformFeedbackFn
#define glBindUniformLocationCHROMIUM \
  ::gl::g_current_gl_context->glBindUniformLocationCHROMIUMFn
#define glBindVertexArrayOES ::gl::g_current_gl_context->glBindVertexArrayOESFn
#define glBlendBarrierKHR ::gl::g_current_gl_context->glBlendBarrierKHRFn
#define glBlendColor ::gl::g_current_gl_context->glBlendColorFn
#define glBlendEquation ::gl::g_current_gl_context->glBlendEquationFn
#define glBlendEquationiOES ::gl::g_current_gl_context->glBlendEquationiOESFn
#define glBlendEquationSeparate \
  ::gl::g_current_gl_context->glBlendEquationSeparateFn
#define glBlendEquationSeparateiOES \
  ::gl::g_current_gl_context->glBlendEquationSeparateiOESFn
#define glBlendFunc ::gl::g_current_gl_context->glBlendFuncFn
#define glBlendFunciOES ::gl::g_current_gl_context->glBlendFunciOESFn
#define glBlendFuncSeparate ::gl::g_current_gl_context->glBlendFuncSeparateFn
#define glBlendFuncSeparateiOES \
  ::gl::g_current_gl_context->glBlendFuncSeparateiOESFn
#define glBlitFramebuffer ::gl::g_current_gl_context->glBlitFramebufferFn
#define glBlobCacheCallbacksANGLE \
  ::gl::g_current_gl_context->glBlobCacheCallbacksANGLEFn
#define glBufferData ::gl::g_current_gl_context->glBufferDataFn
#define glBufferSubData ::gl::g_current_gl_context->glBufferSubDataFn
#define glCheckFramebufferStatusEXT \
  ::gl::g_current_gl_context->glCheckFramebufferStatusEXTFn
#define glClear ::gl::g_current_gl_context->glClearFn
#define glClearBufferfi ::gl::g_current_gl_context->glClearBufferfiFn
#define glClearBufferfv ::gl::g_current_gl_context->glClearBufferfvFn
#define glClearBufferiv ::gl::g_current_gl_context->glClearBufferivFn
#define glClearBufferuiv ::gl::g_current_gl_context->glClearBufferuivFn
#define glClearColor ::gl::g_current_gl_context->glClearColorFn
#define glClearDepth ::gl::g_current_gl_context->glClearDepthFn
#define glClearDepthf ::gl::g_current_gl_context->glClearDepthfFn
#define glClearStencil ::gl::g_current_gl_context->glClearStencilFn
#define glClearTexImage ::gl::g_current_gl_context->glClearTexImageFn
#define glClearTexSubImage ::gl::g_current_gl_context->glClearTexSubImageFn
#define glClientWaitSync ::gl::g_current_gl_context->glClientWaitSyncFn
#define glClipControlEXT ::gl::g_current_gl_context->glClipControlEXTFn
#define glColorMask ::gl::g_current_gl_context->glColorMaskFn
#define glColorMaskiOES ::gl::g_current_gl_context->glColorMaskiOESFn
#define glCompileShader ::gl::g_current_gl_context->glCompileShaderFn
#define glCompressedTexImage2D \
  ::gl::g_current_gl_context->glCompressedTexImage2DFn
#define glCompressedTexImage3D \
  ::gl::g_current_gl_context->glCompressedTexImage3DFn
#define glCompressedTexSubImage2D \
  ::gl::g_current_gl_context->glCompressedTexSubImage2DFn
#define glCompressedTexSubImage3D \
  ::gl::g_current_gl_context->glCompressedTexSubImage3DFn
#define glCopyBufferSubData ::gl::g_current_gl_context->glCopyBufferSubDataFn
#define glCopySubTextureCHROMIUM \
  ::gl::g_current_gl_context->glCopySubTextureCHROMIUMFn
#define glCopyTexImage2D ::gl::g_current_gl_context->glCopyTexImage2DFn
#define glCopyTexSubImage2D ::gl::g_current_gl_context->glCopyTexSubImage2DFn
#define glCopyTexSubImage3D ::gl::g_current_gl_context->glCopyTexSubImage3DFn
#define glCopyTextureCHROMIUM \
  ::gl::g_current_gl_context->glCopyTextureCHROMIUMFn
#define glCreateMemoryObjectsEXT \
  ::gl::g_current_gl_context->glCreateMemoryObjectsEXTFn
#define glCreateProgram ::gl::g_current_gl_context->glCreateProgramFn
#define glCreateShader ::gl::g_current_gl_context->glCreateShaderFn
#define glCullFace ::gl::g_current_gl_context->glCullFaceFn
#define glDebugMessageCallbackKHR \
  ::gl::g_current_gl_context->glDebugMessageCallbackKHRFn
#define glDebugMessageControlKHR \
  ::gl::g_current_gl_context->glDebugMessageControlKHRFn
#define glDebugMessageInsertKHR \
  ::gl::g_current_gl_context->glDebugMessageInsertKHRFn
#define glDeleteBuffersARB ::gl::g_current_gl_context->glDeleteBuffersARBFn
#define glDeleteFencesNV ::gl::g_current_gl_context->glDeleteFencesNVFn
#define glDeleteFramebuffersEXT \
  ::gl::g_current_gl_context->glDeleteFramebuffersEXTFn
#define glDeleteMemoryObjectsEXT \
  ::gl::g_current_gl_context->glDeleteMemoryObjectsEXTFn
#define glDeleteProgram ::gl::g_current_gl_context->glDeleteProgramFn
#define glDeleteQueries ::gl::g_current_gl_context->glDeleteQueriesFn
#define glDeleteRenderbuffersEXT \
  ::gl::g_current_gl_context->glDeleteRenderbuffersEXTFn
#define glDeleteSamplers ::gl::g_current_gl_context->glDeleteSamplersFn
#define glDeleteSemaphoresEXT \
  ::gl::g_current_gl_context->glDeleteSemaphoresEXTFn
#define glDeleteShader ::gl::g_current_gl_context->glDeleteShaderFn
#define glDeleteSync ::gl::g_current_gl_context->glDeleteSyncFn
#define glDeleteTextures ::gl::g_current_gl_context->glDeleteTexturesFn
#define glDeleteTransformFeedbacks \
  ::gl::g_current_gl_context->glDeleteTransformFeedbacksFn
#define glDeleteVertexArraysOES \
  ::gl::g_current_gl_context->glDeleteVertexArraysOESFn
#define glDepthFunc ::gl::g_current_gl_context->glDepthFuncFn
#define glDepthMask ::gl::g_current_gl_context->glDepthMaskFn
#define glDepthRange ::gl::g_current_gl_context->glDepthRangeFn
#define glDepthRangef ::gl::g_current_gl_context->glDepthRangefFn
#define glDetachShader ::gl::g_current_gl_context->glDetachShaderFn
#define glDisable ::gl::g_current_gl_context->glDisableFn
#define glDisableiOES ::gl::g_current_gl_context->glDisableiOESFn
#define glDisableVertexAttribArray \
  ::gl::g_current_gl_context->glDisableVertexAttribArrayFn
#define glDiscardFramebufferEXT \
  ::gl::g_current_gl_context->glDiscardFramebufferEXTFn
#define glDrawArrays ::gl::g_current_gl_context->glDrawArraysFn
#define glDrawArraysInstancedANGLE \
  ::gl::g_current_gl_context->glDrawArraysInstancedANGLEFn
#define glDrawArraysInstancedBaseInstanceANGLE \
  ::gl::g_current_gl_context->glDrawArraysInstancedBaseInstanceANGLEFn
#define glDrawBuffer ::gl::g_current_gl_context->glDrawBufferFn
#define glDrawBuffersARB ::gl::g_current_gl_context->glDrawBuffersARBFn
#define glDrawElements ::gl::g_current_gl_context->glDrawElementsFn
#define glDrawElementsInstancedANGLE \
  ::gl::g_current_gl_context->glDrawElementsInstancedANGLEFn
#define glDrawElementsInstancedBaseVertexBaseInstanceANGLE \
  ::gl::g_current_gl_context                               \
      ->glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn
#define glDrawRangeElements ::gl::g_current_gl_context->glDrawRangeElementsFn
#define glEGLImageTargetRenderbufferStorageOES \
  ::gl::g_current_gl_context->glEGLImageTargetRenderbufferStorageOESFn
#define glEGLImageTargetTexStorageEXT \
  ::gl::g_current_gl_context->glEGLImageTargetTexStorageEXTFn
#define glEGLImageTargetTexture2DOES \
  ::gl::g_current_gl_context->glEGLImageTargetTexture2DOESFn
#define glEnable ::gl::g_current_gl_context->glEnableFn
#define glEnableiOES ::gl::g_current_gl_context->glEnableiOESFn
#define glEnableVertexAttribArray \
  ::gl::g_current_gl_context->glEnableVertexAttribArrayFn
#define glEndPixelLocalStorageANGLE \
  ::gl::g_current_gl_context->glEndPixelLocalStorageANGLEFn
#define glEndPixelLocalStorageImplicitANGLE \
  ::gl::g_current_gl_context->glEndPixelLocalStorageImplicitANGLEFn
#define glEndQuery ::gl::g_current_gl_context->glEndQueryFn
#define glEndTilingQCOM ::gl::g_current_gl_context->glEndTilingQCOMFn
#define glEndTransformFeedback \
  ::gl::g_current_gl_context->glEndTransformFeedbackFn
#define glFenceSync ::gl::g_current_gl_context->glFenceSyncFn
#define glFinish ::gl::g_current_gl_context->glFinishFn
#define glFinishFenceNV ::gl::g_current_gl_context->glFinishFenceNVFn
#define glFlush ::gl::g_current_gl_context->glFlushFn
#define glFlushMappedBufferRange \
  ::gl::g_current_gl_context->glFlushMappedBufferRangeFn
#define glFramebufferMemorylessPixelLocalStorageANGLE \
  ::gl::g_current_gl_context->glFramebufferMemorylessPixelLocalStorageANGLEFn
#define glFramebufferParameteriMESA \
  ::gl::g_current_gl_context->glFramebufferParameteriMESAFn
#define glFramebufferPixelLocalClearValuefvANGLE \
  ::gl::g_current_gl_context->glFramebufferPixelLocalClearValuefvANGLEFn
#define glFramebufferPixelLocalClearValueivANGLE \
  ::gl::g_current_gl_context->glFramebufferPixelLocalClearValueivANGLEFn
#define glFramebufferPixelLocalClearValueuivANGLE \
  ::gl::g_current_gl_context->glFramebufferPixelLocalClearValueuivANGLEFn
#define glFramebufferPixelLocalStorageInterruptANGLE \
  ::gl::g_current_gl_context->glFramebufferPixelLocalStorageInterruptANGLEFn
#define glFramebufferPixelLocalStorageRestoreANGLE \
  ::gl::g_current_gl_context->glFramebufferPixelLocalStorageRestoreANGLEFn
#define glFramebufferRenderbufferEXT \
  ::gl::g_current_gl_context->glFramebufferRenderbufferEXTFn
#define glFramebufferTexture2DEXT \
  ::gl::g_current_gl_context->glFramebufferTexture2DEXTFn
#define glFramebufferTexture2DMultisampleEXT \
  ::gl::g_current_gl_context->glFramebufferTexture2DMultisampleEXTFn
#define glFramebufferTextureLayer \
  ::gl::g_current_gl_context->glFramebufferTextureLayerFn
#define glFramebufferTextureMultiviewOVR \
  ::gl::g_current_gl_context->glFramebufferTextureMultiviewOVRFn
#define glFramebufferTexturePixelLocalStorageANGLE \
  ::gl::g_current_gl_context->glFramebufferTexturePixelLocalStorageANGLEFn
#define glFrontFace ::gl::g_current_gl_context->glFrontFaceFn
#define glGenBuffersARB ::gl::g_current_gl_context->glGenBuffersARBFn
#define glGenerateMipmapEXT ::gl::g_current_gl_context->glGenerateMipmapEXTFn
#define glGenFencesNV ::gl::g_current_gl_context->glGenFencesNVFn
#define glGenFramebuffersEXT ::gl::g_current_gl_context->glGenFramebuffersEXTFn
#define glGenQueries ::gl::g_current_gl_context->glGenQueriesFn
#define glGenRenderbuffersEXT \
  ::gl::g_current_gl_context->glGenRenderbuffersEXTFn
#define glGenSamplers ::gl::g_current_gl_context->glGenSamplersFn
#define glGenSemaphoresEXT ::gl::g_current_gl_context->glGenSemaphoresEXTFn
#define glGenTextures ::gl::g_current_gl_context->glGenTexturesFn
#define glGenTransformFeedbacks \
  ::gl::g_current_gl_context->glGenTransformFeedbacksFn
#define glGenVertexArraysOES ::gl::g_current_gl_context->glGenVertexArraysOESFn
#define glGetActiveAttrib ::gl::g_current_gl_context->glGetActiveAttribFn
#define glGetActiveUniform ::gl::g_current_gl_context->glGetActiveUniformFn
#define glGetActiveUniformBlockiv \
  ::gl::g_current_gl_context->glGetActiveUniformBlockivFn
#define glGetActiveUniformBlockivRobustANGLE \
  ::gl::g_current_gl_context->glGetActiveUniformBlockivRobustANGLEFn
#define glGetActiveUniformBlockName \
  ::gl::g_current_gl_context->glGetActiveUniformBlockNameFn
#define glGetActiveUniformsiv \
  ::gl::g_current_gl_context->glGetActiveUniformsivFn
#define glGetAttachedShaders ::gl::g_current_gl_context->glGetAttachedShadersFn
#define glGetAttribLocation ::gl::g_current_gl_context->glGetAttribLocationFn
#define glGetBooleanv ::gl::g_current_gl_context->glGetBooleanvFn
#define glGetBooleanvRobustANGLE \
  ::gl::g_current_gl_context->glGetBooleanvRobustANGLEFn
#define glGetBufferParameteri64vRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferParameteri64vRobustANGLEFn
#define glGetBufferParameteriv \
  ::gl::g_current_gl_context->glGetBufferParameterivFn
#define glGetBufferParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferParameterivRobustANGLEFn
#define glGetBufferPointervRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferPointervRobustANGLEFn
#define glGetDebugMessageLogKHR \
  ::gl::g_current_gl_context->glGetDebugMessageLogKHRFn
#define glGetError ::gl::g_current_gl_context->glGetErrorFn
#define glGetFenceivNV ::gl::g_current_gl_context->glGetFenceivNVFn
#define glGetFloatv ::gl::g_current_gl_context->glGetFloatvFn
#define glGetFloatvRobustANGLE \
  ::gl::g_current_gl_context->glGetFloatvRobustANGLEFn
#define glGetFragDataIndex ::gl::g_current_gl_context->glGetFragDataIndexFn
#define glGetFragDataLocation \
  ::gl::g_current_gl_context->glGetFragDataLocationFn
#define glGetFramebufferAttachmentParameterivEXT \
  ::gl::g_current_gl_context->glGetFramebufferAttachmentParameterivEXTFn
#define glGetFramebufferAttachmentParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetFramebufferAttachmentParameterivRobustANGLEFn
#define glGetFramebufferPixelLocalStorageParameterfvRobustANGLE \
  ::gl::g_current_gl_context                                    \
      ->glGetFramebufferPixelLocalStorageParameterfvRobustANGLEFn
#define glGetFramebufferPixelLocalStorageParameterivRobustANGLE \
  ::gl::g_current_gl_context                                    \
      ->glGetFramebufferPixelLocalStorageParameterivRobustANGLEFn
#define glGetFramebufferPixelLocalStorageParameteruivRobustANGLE \
  ::gl::g_current_gl_context                                     \
      ->glGetFramebufferPixelLocalStorageParameteruivRobustANGLEFn
#define glGetGraphicsResetStatusARB \
  ::gl::g_current_gl_context->glGetGraphicsResetStatusARBFn
#define glGetInteger64i_v ::gl::g_current_gl_context->glGetInteger64i_vFn
#define glGetInteger64i_vRobustANGLE \
  ::gl::g_current_gl_context->glGetInteger64i_vRobustANGLEFn
#define glGetInteger64v ::gl::g_current_gl_context->glGetInteger64vFn
#define glGetInteger64vRobustANGLE \
  ::gl::g_current_gl_context->glGetInteger64vRobustANGLEFn
#define glGetIntegeri_v ::gl::g_current_gl_context->glGetIntegeri_vFn
#define glGetIntegeri_vRobustANGLE \
  ::gl::g_current_gl_context->glGetIntegeri_vRobustANGLEFn
#define glGetIntegerv ::gl::g_current_gl_context->glGetIntegervFn
#define glGetIntegervRobustANGLE \
  ::gl::g_current_gl_context->glGetIntegervRobustANGLEFn
#define glGetInternalformativ \
  ::gl::g_current_gl_context->glGetInternalformativFn
#define glGetInternalformativRobustANGLE \
  ::gl::g_current_gl_context->glGetInternalformativRobustANGLEFn
#define glGetInternalformatSampleivNV \
  ::gl::g_current_gl_context->glGetInternalformatSampleivNVFn
#define glGetMultisamplefvRobustANGLE \
  ::gl::g_current_gl_context->glGetMultisamplefvRobustANGLEFn
#define glGetObjectLabelKHR ::gl::g_current_gl_context->glGetObjectLabelKHRFn
#define glGetObjectPtrLabelKHR \
  ::gl::g_current_gl_context->glGetObjectPtrLabelKHRFn
#define glGetPointervKHR ::gl::g_current_gl_context->glGetPointervKHRFn
#define glGetProgramBinary ::gl::g_current_gl_context->glGetProgramBinaryFn
#define glGetProgramInfoLog ::gl::g_current_gl_context->glGetProgramInfoLogFn
#define glGetProgramiv ::gl::g_current_gl_context->glGetProgramivFn
#define glGetProgramivRobustANGLE \
  ::gl::g_current_gl_context->glGetProgramivRobustANGLEFn
#define glGetQueryiv ::gl::g_current_gl_context->glGetQueryivFn
#define glGetQueryivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryivRobustANGLEFn
#define glGetQueryObjecti64v ::gl::g_current_gl_context->glGetQueryObjecti64vFn
#define glGetQueryObjecti64vRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjecti64vRobustANGLEFn
#define glGetQueryObjectiv ::gl::g_current_gl_context->glGetQueryObjectivFn
#define glGetQueryObjectivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectivRobustANGLEFn
#define glGetQueryObjectui64v \
  ::gl::g_current_gl_context->glGetQueryObjectui64vFn
#define glGetQueryObjectui64vRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectui64vRobustANGLEFn
#define glGetQueryObjectuiv ::gl::g_current_gl_context->glGetQueryObjectuivFn
#define glGetQueryObjectuivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectuivRobustANGLEFn
#define glGetRenderbufferParameterivEXT \
  ::gl::g_current_gl_context->glGetRenderbufferParameterivEXTFn
#define glGetRenderbufferParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetRenderbufferParameterivRobustANGLEFn
#define glGetSamplerParameterfv \
  ::gl::g_current_gl_context->glGetSamplerParameterfvFn
#define glGetSamplerParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterfvRobustANGLEFn
#define glGetSamplerParameteriv \
  ::gl::g_current_gl_context->glGetSamplerParameterivFn
#define glGetSamplerParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterivRobustANGLEFn
#define glGetShaderInfoLog ::gl::g_current_gl_context->glGetShaderInfoLogFn
#define glGetShaderiv ::gl::g_current_gl_context->glGetShaderivFn
#define glGetShaderivRobustANGLE \
  ::gl::g_current_gl_context->glGetShaderivRobustANGLEFn
#define glGetShaderPrecisionFormat \
  ::gl::g_current_gl_context->glGetShaderPrecisionFormatFn
#define glGetShaderSource ::gl::g_current_gl_context->glGetShaderSourceFn
#define glGetString ::gl::g_current_gl_context->glGetStringFn
#define glGetStringi ::gl::g_current_gl_context->glGetStringiFn
#define glGetSynciv ::gl::g_current_gl_context->glGetSyncivFn
#define glGetTexLevelParameterfvANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterfvANGLEFn
#define glGetTexLevelParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterfvRobustANGLEFn
#define glGetTexLevelParameterivANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterivANGLEFn
#define glGetTexLevelParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterivRobustANGLEFn
#define glGetTexParameterfv ::gl::g_current_gl_context->glGetTexParameterfvFn
#define glGetTexParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterfvRobustANGLEFn
#define glGetTexParameteriv ::gl::g_current_gl_context->glGetTexParameterivFn
#define glGetTexParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterivRobustANGLEFn
#define glGetTransformFeedbackVarying \
  ::gl::g_current_gl_context->glGetTransformFeedbackVaryingFn
#define glGetTranslatedShaderSourceANGLE \
  ::gl::g_current_gl_context->glGetTranslatedShaderSourceANGLEFn
#define glGetUniformBlockIndex \
  ::gl::g_current_gl_context->glGetUniformBlockIndexFn
#define glGetUniformfv ::gl::g_current_gl_context->glGetUniformfvFn
#define glGetUniformfvRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformfvRobustANGLEFn
#define glGetUniformIndices ::gl::g_current_gl_context->glGetUniformIndicesFn
#define glGetUniformiv ::gl::g_current_gl_context->glGetUniformivFn
#define glGetUniformivRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformivRobustANGLEFn
#define glGetUniformLocation ::gl::g_current_gl_context->glGetUniformLocationFn
#define glGetUniformuiv ::gl::g_current_gl_context->glGetUniformuivFn
#define glGetUniformuivRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformuivRobustANGLEFn
#define glGetVertexAttribfv ::gl::g_current_gl_context->glGetVertexAttribfvFn
#define glGetVertexAttribfvRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribfvRobustANGLEFn
#define glGetVertexAttribIivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribIivRobustANGLEFn
#define glGetVertexAttribIuivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribIuivRobustANGLEFn
#define glGetVertexAttribiv ::gl::g_current_gl_context->glGetVertexAttribivFn
#define glGetVertexAttribivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribivRobustANGLEFn
#define glGetVertexAttribPointerv \
  ::gl::g_current_gl_context->glGetVertexAttribPointervFn
#define glGetVertexAttribPointervRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribPointervRobustANGLEFn
#define glHint ::gl::g_current_gl_context->glHintFn
#define glImportMemoryFdEXT ::gl::g_current_gl_context->glImportMemoryFdEXTFn
#define glImportMemoryWin32HandleEXT \
  ::gl::g_current_gl_context->glImportMemoryWin32HandleEXTFn
#define glImportMemoryZirconHandleANGLE \
  ::gl::g_current_gl_context->glImportMemoryZirconHandleANGLEFn
#define glImportSemaphoreFdEXT \
  ::gl::g_current_gl_context->glImportSemaphoreFdEXTFn
#define glImportSemaphoreWin32HandleEXT \
  ::gl::g_current_gl_context->glImportSemaphoreWin32HandleEXTFn
#define glImportSemaphoreZirconHandleANGLE \
  ::gl::g_current_gl_context->glImportSemaphoreZirconHandleANGLEFn
#define glInsertEventMarkerEXT \
  ::gl::g_current_gl_context->glInsertEventMarkerEXTFn
#define glInvalidateFramebuffer \
  ::gl::g_current_gl_context->glInvalidateFramebufferFn
#define glInvalidateSubFramebuffer \
  ::gl::g_current_gl_context->glInvalidateSubFramebufferFn
#define glInvalidateTextureANGLE \
  ::gl::g_current_gl_context->glInvalidateTextureANGLEFn
#define glIsBuffer ::gl::g_current_gl_context->glIsBufferFn
#define glIsEnabled ::gl::g_current_gl_context->glIsEnabledFn
#define glIsEnablediOES ::gl::g_current_gl_context->glIsEnablediOESFn
#define glIsFenceNV ::gl::g_current_gl_context->glIsFenceNVFn
#define glIsFramebufferEXT ::gl::g_current_gl_context->glIsFramebufferEXTFn
#define glIsProgram ::gl::g_current_gl_context->glIsProgramFn
#define glIsQuery ::gl::g_current_gl_context->glIsQueryFn
#define glIsRenderbufferEXT ::gl::g_current_gl_context->glIsRenderbufferEXTFn
#define glIsSampler ::gl::g_current_gl_context->glIsSamplerFn
#define glIsShader ::gl::g_current_gl_context->glIsShaderFn
#define glIsSync ::gl::g_current_gl_context->glIsSyncFn
#define glIsTexture ::gl::g_current_gl_context->glIsTextureFn
#define glIsTransformFeedback \
  ::gl::g_current_gl_context->glIsTransformFeedbackFn
#define glIsVertexArrayOES ::gl::g_current_gl_context->glIsVertexArrayOESFn
#define glLineWidth ::gl::g_current_gl_context->glLineWidthFn
#define glLinkProgram ::gl::g_current_gl_context->glLinkProgramFn
#define glMapBuffer ::gl::g_current_gl_context->glMapBufferFn
#define glMapBufferRange ::gl::g_current_gl_context->glMapBufferRangeFn
#define glMaxShaderCompilerThreadsKHR \
  ::gl::g_current_gl_context->glMaxShaderCompilerThreadsKHRFn
#define glMemoryObjectParameterivEXT \
  ::gl::g_current_gl_context->glMemoryObjectParameterivEXTFn
#define glMultiDrawArraysANGLE \
  ::gl::g_current_gl_context->glMultiDrawArraysANGLEFn
#define glMultiDrawArraysInstancedANGLE \
  ::gl::g_current_gl_context->glMultiDrawArraysInstancedANGLEFn
#define glMultiDrawArraysInstancedBaseInstanceANGLE \
  ::gl::g_current_gl_context->glMultiDrawArraysInstancedBaseInstanceANGLEFn
#define glMultiDrawElementsANGLE \
  ::gl::g_current_gl_context->glMultiDrawElementsANGLEFn
#define glMultiDrawElementsInstancedANGLE \
  ::gl::g_current_gl_context->glMultiDrawElementsInstancedANGLEFn
#define glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE \
  ::gl::g_current_gl_context                                    \
      ->glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn
#define glObjectLabelKHR ::gl::g_current_gl_context->glObjectLabelKHRFn
#define glObjectPtrLabelKHR ::gl::g_current_gl_context->glObjectPtrLabelKHRFn
#define glPatchParameteriOES ::gl::g_current_gl_context->glPatchParameteriOESFn
#define glPauseTransformFeedback \
  ::gl::g_current_gl_context->glPauseTransformFeedbackFn
#define glPixelLocalStorageBarrierANGLE \
  ::gl::g_current_gl_context->glPixelLocalStorageBarrierANGLEFn
#define glPixelStorei ::gl::g_current_gl_context->glPixelStoreiFn
#define glPointParameteri ::gl::g_current_gl_context->glPointParameteriFn
#define glPolygonMode ::gl::g_current_gl_context->glPolygonModeFn
#define glPolygonModeANGLE ::gl::g_current_gl_context->glPolygonModeANGLEFn
#define glPolygonOffset ::gl::g_current_gl_context->glPolygonOffsetFn
#define glPolygonOffsetClampEXT \
  ::gl::g_current_gl_context->glPolygonOffsetClampEXTFn
#define glPopDebugGroupKHR ::gl::g_current_gl_context->glPopDebugGroupKHRFn
#define glPopGroupMarkerEXT ::gl::g_current_gl_context->glPopGroupMarkerEXTFn
#define glPrimitiveRestartIndex \
  ::gl::g_current_gl_context->glPrimitiveRestartIndexFn
#define glProgramBinary ::gl::g_current_gl_context->glProgramBinaryFn
#define glProgramParameteri ::gl::g_current_gl_context->glProgramParameteriFn
#define glProvokingVertexANGLE \
  ::gl::g_current_gl_context->glProvokingVertexANGLEFn
#define glPushDebugGroupKHR ::gl::g_current_gl_context->glPushDebugGroupKHRFn
#define glPushGroupMarkerEXT ::gl::g_current_gl_context->glPushGroupMarkerEXTFn
#define glQueryCounter ::gl::g_current_gl_context->glQueryCounterFn
#define glReadBuffer ::gl::g_current_gl_context->glReadBufferFn
#define glReadPixels ::gl::g_current_gl_context->glReadPixelsFn
#define glReadPixelsRobustANGLE \
  ::gl::g_current_gl_context->glReadPixelsRobustANGLEFn
#define glReleaseShaderCompiler \
  ::gl::g_current_gl_context->glReleaseShaderCompilerFn
#define glReleaseTexturesANGLE \
  ::gl::g_current_gl_context->glReleaseTexturesANGLEFn
#define glRenderbufferStorageEXT \
  ::gl::g_current_gl_context->glRenderbufferStorageEXTFn
#define glRenderbufferStorageMultisample \
  ::gl::g_current_gl_context->glRenderbufferStorageMultisampleFn
#define glRenderbufferStorageMultisampleAdvancedAMD \
  ::gl::g_current_gl_context->glRenderbufferStorageMultisampleAdvancedAMDFn
#define glRenderbufferStorageMultisampleEXT \
  ::gl::g_current_gl_context->glRenderbufferStorageMultisampleEXTFn
#define glRequestExtensionANGLE \
  ::gl::g_current_gl_context->glRequestExtensionANGLEFn
#define glResumeTransformFeedback \
  ::gl::g_current_gl_context->glResumeTransformFeedbackFn
#define glSampleCoverage ::gl::g_current_gl_context->glSampleCoverageFn
#define glSamplerParameterf ::gl::g_current_gl_context->glSamplerParameterfFn
#define glSamplerParameterfv ::gl::g_current_gl_context->glSamplerParameterfvFn
#define glSamplerParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterfvRobustANGLEFn
#define glSamplerParameteri ::gl::g_current_gl_context->glSamplerParameteriFn
#define glSamplerParameteriv ::gl::g_current_gl_context->glSamplerParameterivFn
#define glSamplerParameterivRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterivRobustANGLEFn
#define glScissor ::gl::g_current_gl_context->glScissorFn
#define glSetFenceNV ::gl::g_current_gl_context->glSetFenceNVFn
#define glShaderBinary ::gl::g_current_gl_context->glShaderBinaryFn
#define glShaderSource ::gl::g_current_gl_context->glShaderSourceFn
#define glSignalSemaphoreEXT ::gl::g_current_gl_context->glSignalSemaphoreEXTFn
#define glStartTilingQCOM ::gl::g_current_gl_context->glStartTilingQCOMFn
#define glStencilFunc ::gl::g_current_gl_context->glStencilFuncFn
#define glStencilFuncSeparate \
  ::gl::g_current_gl_context->glStencilFuncSeparateFn
#define glStencilMask ::gl::g_current_gl_context->glStencilMaskFn
#define glStencilMaskSeparate \
  ::gl::g_current_gl_context->glStencilMaskSeparateFn
#define glStencilOp ::gl::g_current_gl_context->glStencilOpFn
#define glStencilOpSeparate ::gl::g_current_gl_context->glStencilOpSeparateFn
#define glTestFenceNV ::gl::g_current_gl_context->glTestFenceNVFn
#define glTexBufferOES ::gl::g_current_gl_context->glTexBufferOESFn
#define glTexBufferRangeOES ::gl::g_current_gl_context->glTexBufferRangeOESFn
#define glTexImage2D ::gl::g_current_gl_context->glTexImage2DFn
#define glTexImage2DExternalANGLE \
  ::gl::g_current_gl_context->glTexImage2DExternalANGLEFn
#define glTexImage2DRobustANGLE \
  ::gl::g_current_gl_context->glTexImage2DRobustANGLEFn
#define glTexImage3D ::gl::g_current_gl_context->glTexImage3DFn
#define glTexImage3DRobustANGLE \
  ::gl::g_current_gl_context->glTexImage3DRobustANGLEFn
#define glTexParameterf ::gl::g_current_gl_context->glTexParameterfFn
#define glTexParameterfv ::gl::g_current_gl_context->glTexParameterfvFn
#define glTexParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterfvRobustANGLEFn
#define glTexParameteri ::gl::g_current_gl_context->glTexParameteriFn
#define glTexParameteriv ::gl::g_current_gl_context->glTexParameterivFn
#define glTexParameterivRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterivRobustANGLEFn
#define glTexStorage2DEXT ::gl::g_current_gl_context->glTexStorage2DEXTFn
#define glTexStorage3D ::gl::g_current_gl_context->glTexStorage3DFn
#define glTexStorageMem2DEXT ::gl::g_current_gl_context->glTexStorageMem2DEXTFn
#define glTexStorageMemFlags2DANGLE \
  ::gl::g_current_gl_context->glTexStorageMemFlags2DANGLEFn
#define glTexSubImage2D ::gl::g_current_gl_context->glTexSubImage2DFn
#define glTexSubImage2DRobustANGLE \
  ::gl::g_current_gl_context->glTexSubImage2DRobustANGLEFn
#define glTexSubImage3D ::gl::g_current_gl_context->glTexSubImage3DFn
#define glTexSubImage3DRobustANGLE \
  ::gl::g_current_gl_context->glTexSubImage3DRobustANGLEFn
#define glTransformFeedbackVaryings \
  ::gl::g_current_gl_context->glTransformFeedbackVaryingsFn
#define glTrimMemoryANGLE ::gl::g_current_gl_context->glTrimMemoryANGLEFn
#define glUniform1f ::gl::g_current_gl_context->glUniform1fFn
#define glUniform1fv ::gl::g_current_gl_context->glUniform1fvFn
#define glUniform1i ::gl::g_current_gl_context->glUniform1iFn
#define glUniform1iv ::gl::g_current_gl_context->glUniform1ivFn
#define glUniform1ui ::gl::g_current_gl_context->glUniform1uiFn
#define glUniform1uiv ::gl::g_current_gl_context->glUniform1uivFn
#define glUniform2f ::gl::g_current_gl_context->glUniform2fFn
#define glUniform2fv ::gl::g_current_gl_context->glUniform2fvFn
#define glUniform2i ::gl::g_current_gl_context->glUniform2iFn
#define glUniform2iv ::gl::g_current_gl_context->glUniform2ivFn
#define glUniform2ui ::gl::g_current_gl_context->glUniform2uiFn
#define glUniform2uiv ::gl::g_current_gl_context->glUniform2uivFn
#define glUniform3f ::gl::g_current_gl_context->glUniform3fFn
#define glUniform3fv ::gl::g_current_gl_context->glUniform3fvFn
#define glUniform3i ::gl::g_current_gl_context->glUniform3iFn
#define glUniform3iv ::gl::g_current_gl_context->glUniform3ivFn
#define glUniform3ui ::gl::g_current_gl_context->glUniform3uiFn
#define glUniform3uiv ::gl::g_current_gl_context->glUniform3uivFn
#define glUniform4f ::gl::g_current_gl_context->glUniform4fFn
#define glUniform4fv ::gl::g_current_gl_context->glUniform4fvFn
#define glUniform4i ::gl::g_current_gl_context->glUniform4iFn
#define glUniform4iv ::gl::g_current_gl_context->glUniform4ivFn
#define glUniform4ui ::gl::g_current_gl_context->glUniform4uiFn
#define glUniform4uiv ::gl::g_current_gl_context->glUniform4uivFn
#define glUniformBlockBinding \
  ::gl::g_current_gl_context->glUniformBlockBindingFn
#define glUniformMatrix2fv ::gl::g_current_gl_context->glUniformMatrix2fvFn
#define glUniformMatrix2x3fv ::gl::g_current_gl_context->glUniformMatrix2x3fvFn
#define glUniformMatrix2x4fv ::gl::g_current_gl_context->glUniformMatrix2x4fvFn
#define glUniformMatrix3fv ::gl::g_current_gl_context->glUniformMatrix3fvFn
#define glUniformMatrix3x2fv ::gl::g_current_gl_context->glUniformMatrix3x2fvFn
#define glUniformMatrix3x4fv ::gl::g_current_gl_context->glUniformMatrix3x4fvFn
#define glUniformMatrix4fv ::gl::g_current_gl_context->glUniformMatrix4fvFn
#define glUniformMatrix4x2fv ::gl::g_current_gl_context->glUniformMatrix4x2fvFn
#define glUniformMatrix4x3fv ::gl::g_current_gl_context->glUniformMatrix4x3fvFn
#define glUnmapBuffer ::gl::g_current_gl_context->glUnmapBufferFn
#define glUseProgram ::gl::g_current_gl_context->glUseProgramFn
#define glValidateProgram ::gl::g_current_gl_context->glValidateProgramFn
#define glVertexAttrib1f ::gl::g_current_gl_context->glVertexAttrib1fFn
#define glVertexAttrib1fv ::gl::g_current_gl_context->glVertexAttrib1fvFn
#define glVertexAttrib2f ::gl::g_current_gl_context->glVertexAttrib2fFn
#define glVertexAttrib2fv ::gl::g_current_gl_context->glVertexAttrib2fvFn
#define glVertexAttrib3f ::gl::g_current_gl_context->glVertexAttrib3fFn
#define glVertexAttrib3fv ::gl::g_current_gl_context->glVertexAttrib3fvFn
#define glVertexAttrib4f ::gl::g_current_gl_context->glVertexAttrib4fFn
#define glVertexAttrib4fv ::gl::g_current_gl_context->glVertexAttrib4fvFn
#define glVertexAttribDivisorANGLE \
  ::gl::g_current_gl_context->glVertexAttribDivisorANGLEFn
#define glVertexAttribI4i ::gl::g_current_gl_context->glVertexAttribI4iFn
#define glVertexAttribI4iv ::gl::g_current_gl_context->glVertexAttribI4ivFn
#define glVertexAttribI4ui ::gl::g_current_gl_context->glVertexAttribI4uiFn
#define glVertexAttribI4uiv ::gl::g_current_gl_context->glVertexAttribI4uivFn
#define glVertexAttribIPointer \
  ::gl::g_current_gl_context->glVertexAttribIPointerFn
#define glVertexAttribPointer \
  ::gl::g_current_gl_context->glVertexAttribPointerFn
#define glViewport ::gl::g_current_gl_context->glViewportFn
#define glWaitSemaphoreEXT ::gl::g_current_gl_context->glWaitSemaphoreEXTFn
#define glWaitSync ::gl::g_current_gl_context->glWaitSyncFn
#define glWindowRectanglesEXT \
  ::gl::g_current_gl_context->glWindowRectanglesEXTFn
#endif  // BINDINGS_GL_PROTOTYPES

#endif  // UI_GL_GL_BINDINGS_AUTOGEN_GL_H_
