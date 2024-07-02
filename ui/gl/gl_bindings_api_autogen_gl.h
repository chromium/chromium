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

void glAcquireTexturesANGLEFn(GLuint numTextures,
                              const GLuint* textures,
                              const GLenum* layouts) override;
void glActiveShaderProgramFn(GLuint pipeline, GLuint program) override;
void glActiveTextureFn(GLenum texture) override;
void glAttachShaderFn(GLuint program, GLuint shader) override;
void glBeginPixelLocalStorageANGLEFn(GLsizei n, const GLenum* loadops) override;
void glBeginQueryFn(GLenum target, GLuint id) override;
void glBeginTransformFeedbackFn(GLenum primitiveMode) override;
void glBindAttribLocationFn(GLuint program,
                            GLuint index,
                            const char* name) override;
void glBindBufferFn(GLenum target, GLuint buffer) override;
void glBindBufferBaseFn(GLenum target, GLuint index, GLuint buffer) override;
void glBindBufferRangeFn(GLenum target,
                         GLuint index,
                         GLuint buffer,
                         GLintptr offset,
                         GLsizeiptr size) override;
void glBindFragDataLocationFn(GLuint program,
                              GLuint colorNumber,
                              const char* name) override;
void glBindFragDataLocationIndexedFn(GLuint program,
                                     GLuint colorNumber,
                                     GLuint index,
                                     const char* name) override;
void glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) override;
void glBindImageTextureEXTFn(GLuint index,
                             GLuint texture,
                             GLint level,
                             GLboolean layered,
                             GLint layer,
                             GLenum access,
                             GLint format) override;
void glBindProgramPipelineFn(GLuint pipeline) override;
void glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) override;
void glBindSamplerFn(GLuint unit, GLuint sampler) override;
void glBindTextureFn(GLenum target, GLuint texture) override;
void glBindTransformFeedbackFn(GLenum target, GLuint id) override;
void glBindUniformLocationCHROMIUMFn(GLuint program,
                                     GLint location,
                                     const char* name) override;
void glBindVertexArrayOESFn(GLuint array) override;
void glBindVertexBufferFn(GLuint bindingindex,
                          GLuint buffer,
                          GLintptr offset,
                          GLsizei stride) override;
void glBlendBarrierKHRFn(void) override;
void glBlendColorFn(GLclampf red,
                    GLclampf green,
                    GLclampf blue,
                    GLclampf alpha) override;
void glBlendEquationFn(GLenum mode) override;
void glBlendEquationiOESFn(GLuint buf, GLenum mode) override;
void glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) override;
void glBlendEquationSeparateiOESFn(GLuint buf,
                                   GLenum modeRGB,
                                   GLenum modeAlpha) override;
void glBlendFuncFn(GLenum sfactor, GLenum dfactor) override;
void glBlendFunciOESFn(GLuint buf, GLenum sfactor, GLenum dfactor) override;
void glBlendFuncSeparateFn(GLenum srcRGB,
                           GLenum dstRGB,
                           GLenum srcAlpha,
                           GLenum dstAlpha) override;
void glBlendFuncSeparateiOESFn(GLuint buf,
                               GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha) override;
void glBlitFramebufferFn(GLint srcX0,
                         GLint srcY0,
                         GLint srcX1,
                         GLint srcY1,
                         GLint dstX0,
                         GLint dstY0,
                         GLint dstX1,
                         GLint dstY1,
                         GLbitfield mask,
                         GLenum filter) override;
void glBufferDataFn(GLenum target,
                    GLsizeiptr size,
                    const void* data,
                    GLenum usage) override;
void glBufferSubDataFn(GLenum target,
                       GLintptr offset,
                       GLsizeiptr size,
                       const void* data) override;
GLenum glCheckFramebufferStatusEXTFn(GLenum target) override;
void glClearFn(GLbitfield mask) override;
void glClearBufferfiFn(GLenum buffer,
                       GLint drawbuffer,
                       const GLfloat depth,
                       GLint stencil) override;
void glClearBufferfvFn(GLenum buffer,
                       GLint drawbuffer,
                       const GLfloat* value) override;
void glClearBufferivFn(GLenum buffer,
                       GLint drawbuffer,
                       const GLint* value) override;
void glClearBufferuivFn(GLenum buffer,
                        GLint drawbuffer,
                        const GLuint* value) override;
void glClearColorFn(GLclampf red,
                    GLclampf green,
                    GLclampf blue,
                    GLclampf alpha) override;
void glClearDepthFn(GLclampd depth) override;
void glClearDepthfFn(GLclampf depth) override;
void glClearStencilFn(GLint s) override;
void glClearTexImageFn(GLuint texture,
                       GLint level,
                       GLenum format,
                       GLenum type,
                       const GLvoid* data) override;
void glClearTexSubImageFn(GLuint texture,
                          GLint level,
                          GLint xoffset,
                          GLint yoffset,
                          GLint zoffset,
                          GLint width,
                          GLint height,
                          GLint depth,
                          GLenum format,
                          GLenum type,
                          const GLvoid* data) override;
GLenum glClientWaitSyncFn(GLsync sync,
                          GLbitfield flags,
                          GLuint64 timeout) override;
void glClipControlEXTFn(GLenum origin, GLenum depth) override;
void glColorMaskFn(GLboolean red,
                   GLboolean green,
                   GLboolean blue,
                   GLboolean alpha) override;
void glColorMaskiOESFn(GLuint buf,
                       GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha) override;
void glCompileShaderFn(GLuint shader) override;
void glCompressedTexImage2DFn(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLsizei imageSize,
                              const void* data) override;
void glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const void* data) override;
void glCompressedTexImage3DFn(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLsizei imageSize,
                              const void* data) override;
void glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const void* data) override;
void glCompressedTexSubImage2DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const void* data) override;
void glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            GLsizei dataSize,
                                            const void* data) override;
void glCompressedTexSubImage3DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const void* data) override;
void glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
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
                                            const void* data) override;
void glCopyBufferSubDataFn(GLenum readTarget,
                           GLenum writeTarget,
                           GLintptr readOffset,
                           GLintptr writeOffset,
                           GLsizeiptr size) override;
void glCopySubTextureCHROMIUMFn(GLuint sourceId,
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
                                GLboolean unpackUnmultiplyAlpha) override;
void glCopyTexImage2DFn(GLenum target,
                        GLint level,
                        GLenum internalformat,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLint border) override;
void glCopyTexSubImage2DFn(GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height) override;
void glCopyTexSubImage3DFn(GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height) override;
void glCopyTextureCHROMIUMFn(GLuint sourceId,
                             GLint sourceLevel,
                             GLenum destTarget,
                             GLuint destId,
                             GLint destLevel,
                             GLint internalFormat,
                             GLenum destType,
                             GLboolean unpackFlipY,
                             GLboolean unpackPremultiplyAlpha,
                             GLboolean unpackUnmultiplyAlpha) override;
void glCreateMemoryObjectsEXTFn(GLsizei n, GLuint* memoryObjects) override;
GLuint glCreateProgramFn(void) override;
GLuint glCreateShaderFn(GLenum type) override;
GLuint glCreateShaderProgramvFn(GLenum type,
                                GLsizei count,
                                const char* const* strings) override;
void glCullFaceFn(GLenum mode) override;
void glDebugMessageCallbackFn(GLDEBUGPROC callback,
                              const void* userParam) override;
void glDebugMessageControlFn(GLenum source,
                             GLenum type,
                             GLenum severity,
                             GLsizei count,
                             const GLuint* ids,
                             GLboolean enabled) override;
void glDebugMessageInsertFn(GLenum source,
                            GLenum type,
                            GLuint id,
                            GLenum severity,
                            GLsizei length,
                            const char* buf) override;
void glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) override;
void glDeleteFencesNVFn(GLsizei n, const GLuint* fences) override;
void glDeleteFramebuffersEXTFn(GLsizei n, const GLuint* framebuffers) override;
void glDeleteMemoryObjectsEXTFn(GLsizei n,
                                const GLuint* memoryObjects) override;
void glDeleteProgramFn(GLuint program) override;
void glDeleteProgramPipelinesFn(GLsizei n, const GLuint* pipelines) override;
void glDeleteQueriesFn(GLsizei n, const GLuint* ids) override;
void glDeleteRenderbuffersEXTFn(GLsizei n,
                                const GLuint* renderbuffers) override;
void glDeleteSamplersFn(GLsizei n, const GLuint* samplers) override;
void glDeleteSemaphoresEXTFn(GLsizei n, const GLuint* semaphores) override;
void glDeleteShaderFn(GLuint shader) override;
void glDeleteSyncFn(GLsync sync) override;
void glDeleteTexturesFn(GLsizei n, const GLuint* textures) override;
void glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) override;
void glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) override;
void glDepthFuncFn(GLenum func) override;
void glDepthMaskFn(GLboolean flag) override;
void glDepthRangeFn(GLclampd zNear, GLclampd zFar) override;
void glDepthRangefFn(GLclampf zNear, GLclampf zFar) override;
void glDetachShaderFn(GLuint program, GLuint shader) override;
void glDisableFn(GLenum cap) override;
void glDisableExtensionANGLEFn(const char* name) override;
void glDisableiOESFn(GLenum target, GLuint index) override;
void glDisableVertexAttribArrayFn(GLuint index) override;
void glDiscardFramebufferEXTFn(GLenum target,
                               GLsizei numAttachments,
                               const GLenum* attachments) override;
void glDispatchComputeFn(GLuint numGroupsX,
                         GLuint numGroupsY,
                         GLuint numGroupsZ) override;
void glDispatchComputeIndirectFn(GLintptr indirect) override;
void glDrawArraysFn(GLenum mode, GLint first, GLsizei count) override;
void glDrawArraysIndirectFn(GLenum mode, const void* indirect) override;
void glDrawArraysInstancedANGLEFn(GLenum mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei primcount) override;
void glDrawArraysInstancedBaseInstanceANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount,
                                              GLuint baseinstance) override;
void glDrawBufferFn(GLenum mode) override;
void glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) override;
void glDrawElementsFn(GLenum mode,
                      GLsizei count,
                      GLenum type,
                      const void* indices) override;
void glDrawElementsIndirectFn(GLenum mode,
                              GLenum type,
                              const void* indirect) override;
void glDrawElementsInstancedANGLEFn(GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void* indices,
                                    GLsizei primcount) override;
void glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint baseVertex,
    GLuint baseInstance) override;
void glDrawRangeElementsFn(GLenum mode,
                           GLuint start,
                           GLuint end,
                           GLsizei count,
                           GLenum type,
                           const void* indices) override;
void glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                              GLeglImageOES image) override;
void glEGLImageTargetTexture2DOESFn(GLenum target,
                                    GLeglImageOES image) override;
void glEnableFn(GLenum cap) override;
void glEnableiOESFn(GLenum target, GLuint index) override;
void glEnableVertexAttribArrayFn(GLuint index) override;
void glEndPixelLocalStorageANGLEFn(GLsizei n, const GLenum* storeops) override;
void glEndQueryFn(GLenum target) override;
void glEndTilingQCOMFn(GLbitfield preserveMask) override;
void glEndTransformFeedbackFn(void) override;
GLsync glFenceSyncFn(GLenum condition, GLbitfield flags) override;
void glFinishFn(void) override;
void glFinishFenceNVFn(GLuint fence) override;
void glFlushFn(void) override;
void glFlushMappedBufferRangeFn(GLenum target,
                                GLintptr offset,
                                GLsizeiptr length) override;
void glFramebufferMemorylessPixelLocalStorageANGLEFn(
    GLint plane,
    GLenum internalformat) override;
void glFramebufferParameteriFn(GLenum target,
                               GLenum pname,
                               GLint param) override;
void glFramebufferPixelLocalClearValuefvANGLEFn(GLint plane,
                                                const GLfloat* value) override;
void glFramebufferPixelLocalClearValueivANGLEFn(GLint plane,
                                                const GLint* value) override;
void glFramebufferPixelLocalClearValueuivANGLEFn(GLint plane,
                                                 const GLuint* value) override;
void glFramebufferPixelLocalStorageInterruptANGLEFn() override;
void glFramebufferPixelLocalStorageRestoreANGLEFn() override;
void glFramebufferRenderbufferEXTFn(GLenum target,
                                    GLenum attachment,
                                    GLenum renderbuffertarget,
                                    GLuint renderbuffer) override;
void glFramebufferTexture2DEXTFn(GLenum target,
                                 GLenum attachment,
                                 GLenum textarget,
                                 GLuint texture,
                                 GLint level) override;
void glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                            GLenum attachment,
                                            GLenum textarget,
                                            GLuint texture,
                                            GLint level,
                                            GLsizei samples) override;
void glFramebufferTextureLayerFn(GLenum target,
                                 GLenum attachment,
                                 GLuint texture,
                                 GLint level,
                                 GLint layer) override;
void glFramebufferTextureMultiviewOVRFn(GLenum target,
                                        GLenum attachment,
                                        GLuint texture,
                                        GLint level,
                                        GLint baseViewIndex,
                                        GLsizei numViews) override;
void glFramebufferTexturePixelLocalStorageANGLEFn(GLint plane,
                                                  GLuint backingtexture,
                                                  GLint level,
                                                  GLint layer) override;
void glFrontFaceFn(GLenum mode) override;
void glGenBuffersARBFn(GLsizei n, GLuint* buffers) override;
void glGenerateMipmapEXTFn(GLenum target) override;
void glGenFencesNVFn(GLsizei n, GLuint* fences) override;
void glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) override;
GLuint glGenProgramPipelinesFn(GLsizei n, GLuint* pipelines) override;
void glGenQueriesFn(GLsizei n, GLuint* ids) override;
void glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) override;
void glGenSamplersFn(GLsizei n, GLuint* samplers) override;
void glGenSemaphoresEXTFn(GLsizei n, GLuint* semaphores) override;
void glGenTexturesFn(GLsizei n, GLuint* textures) override;
void glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) override;
void glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) override;
void glGetActiveAttribFn(GLuint program,
                         GLuint index,
                         GLsizei bufsize,
                         GLsizei* length,
                         GLint* size,
                         GLenum* type,
                         char* name) override;
void glGetActiveUniformFn(GLuint program,
                          GLuint index,
                          GLsizei bufsize,
                          GLsizei* length,
                          GLint* size,
                          GLenum* type,
                          char* name) override;
void glGetActiveUniformBlockivFn(GLuint program,
                                 GLuint uniformBlockIndex,
                                 GLenum pname,
                                 GLint* params) override;
void glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) override;
void glGetActiveUniformBlockNameFn(GLuint program,
                                   GLuint uniformBlockIndex,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   char* uniformBlockName) override;
void glGetActiveUniformsivFn(GLuint program,
                             GLsizei uniformCount,
                             const GLuint* uniformIndices,
                             GLenum pname,
                             GLint* params) override;
void glGetAttachedShadersFn(GLuint program,
                            GLsizei maxcount,
                            GLsizei* count,
                            GLuint* shaders) override;
GLint glGetAttribLocationFn(GLuint program, const char* name) override;
void glGetBooleani_vFn(GLenum target, GLuint index, GLboolean* data) override;
void glGetBooleani_vRobustANGLEFn(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLboolean* data) override;
void glGetBooleanvFn(GLenum pname, GLboolean* params) override;
void glGetBooleanvRobustANGLEFn(GLenum pname,
                                GLsizei bufSize,
                                GLsizei* length,
                                GLboolean* data) override;
void glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint64* params) override;
void glGetBufferParameterivFn(GLenum target,
                              GLenum pname,
                              GLint* params) override;
void glGetBufferParameterivRobustANGLEFn(GLenum target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) override;
void glGetBufferPointervRobustANGLEFn(GLenum target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      void** params) override;
GLuint glGetDebugMessageLogFn(GLuint count,
                              GLsizei bufSize,
                              GLenum* sources,
                              GLenum* types,
                              GLuint* ids,
                              GLenum* severities,
                              GLsizei* lengths,
                              char* messageLog) override;
GLenum glGetErrorFn(void) override;
void glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) override;
void glGetFloatvFn(GLenum pname, GLfloat* params) override;
void glGetFloatvRobustANGLEFn(GLenum pname,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLfloat* data) override;
GLint glGetFragDataIndexFn(GLuint program, const char* name) override;
GLint glGetFragDataLocationFn(GLuint program, const char* name) override;
void glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum pname,
                                                GLint* params) override;
void glGetFramebufferAttachmentParameterivRobustANGLEFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) override;
void glGetFramebufferParameterivFn(GLenum target,
                                   GLenum pname,
                                   GLint* params) override;
void glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) override;
void glGetFramebufferPixelLocalStorageParameterfvANGLEFn(
    GLint plane,
    GLenum pname,
    GLfloat* params) override;
void glGetFramebufferPixelLocalStorageParameterfvRobustANGLEFn(
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params) override;
void glGetFramebufferPixelLocalStorageParameterivANGLEFn(
    GLint plane,
    GLenum pname,
    GLint* params) override;
void glGetFramebufferPixelLocalStorageParameterivRobustANGLEFn(
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) override;
GLenum glGetGraphicsResetStatusARBFn(void) override;
void glGetInteger64i_vFn(GLenum target, GLuint index, GLint64* data) override;
void glGetInteger64i_vRobustANGLEFn(GLenum target,
                                    GLuint index,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLint64* data) override;
void glGetInteger64vFn(GLenum pname, GLint64* params) override;
void glGetInteger64vRobustANGLEFn(GLenum pname,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLint64* data) override;
void glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) override;
void glGetIntegeri_vRobustANGLEFn(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLint* data) override;
void glGetIntegervFn(GLenum pname, GLint* params) override;
void glGetIntegervRobustANGLEFn(GLenum pname,
                                GLsizei bufSize,
                                GLsizei* length,
                                GLint* data) override;
void glGetInternalformativFn(GLenum target,
                             GLenum internalformat,
                             GLenum pname,
                             GLsizei bufSize,
                             GLint* params) override;
void glGetInternalformativRobustANGLEFn(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLint* params) override;
void glGetInternalformatSampleivNVFn(GLenum target,
                                     GLenum internalformat,
                                     GLsizei samples,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLint* params) override;
void glGetMultisamplefvFn(GLenum pname, GLuint index, GLfloat* val) override;
void glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLfloat* val) override;
void glGetnUniformfvRobustANGLEFn(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLfloat* params) override;
void glGetnUniformivRobustANGLEFn(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLint* params) override;
void glGetnUniformuivRobustANGLEFn(GLuint program,
                                   GLint location,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLuint* params) override;
void glGetObjectLabelFn(GLenum identifier,
                        GLuint name,
                        GLsizei bufSize,
                        GLsizei* length,
                        char* label) override;
void glGetObjectPtrLabelFn(void* ptr,
                           GLsizei bufSize,
                           GLsizei* length,
                           char* label) override;
void glGetPointervFn(GLenum pname, void** params) override;
void glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           void** params) override;
void glGetProgramBinaryFn(GLuint program,
                          GLsizei bufSize,
                          GLsizei* length,
                          GLenum* binaryFormat,
                          GLvoid* binary) override;
void glGetProgramInfoLogFn(GLuint program,
                           GLsizei bufsize,
                           GLsizei* length,
                           char* infolog) override;
void glGetProgramInterfaceivFn(GLuint program,
                               GLenum programInterface,
                               GLenum pname,
                               GLint* params) override;
void glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) override;
void glGetProgramivFn(GLuint program, GLenum pname, GLint* params) override;
void glGetProgramivRobustANGLEFn(GLuint program,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei* length,
                                 GLint* params) override;
void glGetProgramPipelineInfoLogFn(GLuint pipeline,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLchar* infoLog) override;
void glGetProgramPipelineivFn(GLuint pipeline,
                              GLenum pname,
                              GLint* params) override;
GLuint glGetProgramResourceIndexFn(GLuint program,
                                   GLenum programInterface,
                                   const GLchar* name) override;
void glGetProgramResourceivFn(GLuint program,
                              GLenum programInterface,
                              GLuint index,
                              GLsizei propCount,
                              const GLenum* props,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLint* params) override;
GLint glGetProgramResourceLocationFn(GLuint program,
                                     GLenum programInterface,
                                     const char* name) override;
void glGetProgramResourceNameFn(GLuint program,
                                GLenum programInterface,
                                GLuint index,
                                GLsizei bufSize,
                                GLsizei* length,
                                GLchar* name) override;
void glGetQueryivFn(GLenum target, GLenum pname, GLint* params) override;
void glGetQueryivRobustANGLEFn(GLenum target,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* params) override;
void glGetQueryObjecti64vFn(GLuint id, GLenum pname, GLint64* params) override;
void glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint64* params) override;
void glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) override;
void glGetQueryObjectivRobustANGLEFn(GLuint id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint* params) override;
void glGetQueryObjectui64vFn(GLuint id,
                             GLenum pname,
                             GLuint64* params) override;
void glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLuint64* params) override;
void glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) override;
void glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLuint* params) override;
void glGetRenderbufferParameterivEXTFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) override;
void glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) override;
void glGetSamplerParameterfvFn(GLuint sampler,
                               GLenum pname,
                               GLfloat* params) override;
void glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* params) override;
void glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) override;
void glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLuint* params) override;
void glGetSamplerParameterivFn(GLuint sampler,
                               GLenum pname,
                               GLint* params) override;
void glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) override;
void glGetShaderInfoLogFn(GLuint shader,
                          GLsizei bufsize,
                          GLsizei* length,
                          char* infolog) override;
void glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) override;
void glGetShaderivRobustANGLEFn(GLuint shader,
                                GLenum pname,
                                GLsizei bufSize,
                                GLsizei* length,
                                GLint* params) override;
void glGetShaderPrecisionFormatFn(GLenum shadertype,
                                  GLenum precisiontype,
                                  GLint* range,
                                  GLint* precision) override;
void glGetShaderSourceFn(GLuint shader,
                         GLsizei bufsize,
                         GLsizei* length,
                         char* source) override;
const GLubyte* glGetStringFn(GLenum name) override;
const GLubyte* glGetStringiFn(GLenum name, GLuint index) override;
void glGetSyncivFn(GLsync sync,
                   GLenum pname,
                   GLsizei bufSize,
                   GLsizei* length,
                   GLint* values) override;
void glGetTexLevelParameterfvFn(GLenum target,
                                GLint level,
                                GLenum pname,
                                GLfloat* params) override;
void glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLfloat* params) override;
void glGetTexLevelParameterivFn(GLenum target,
                                GLint level,
                                GLenum pname,
                                GLint* params) override;
void glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) override;
void glGetTexParameterfvFn(GLenum target,
                           GLenum pname,
                           GLfloat* params) override;
void glGetTexParameterfvRobustANGLEFn(GLenum target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLfloat* params) override;
void glGetTexParameterIivRobustANGLEFn(GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint* params) override;
void glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLuint* params) override;
void glGetTexParameterivFn(GLenum target, GLenum pname, GLint* params) override;
void glGetTexParameterivRobustANGLEFn(GLenum target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint* params) override;
void glGetTransformFeedbackVaryingFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLsizei* size,
                                     GLenum* type,
                                     char* name) override;
void glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                        GLsizei bufsize,
                                        GLsizei* length,
                                        char* source) override;
GLuint glGetUniformBlockIndexFn(GLuint program,
                                const char* uniformBlockName) override;
void glGetUniformfvFn(GLuint program, GLint location, GLfloat* params) override;
void glGetUniformfvRobustANGLEFn(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei* length,
                                 GLfloat* params) override;
void glGetUniformIndicesFn(GLuint program,
                           GLsizei uniformCount,
                           const char* const* uniformNames,
                           GLuint* uniformIndices) override;
void glGetUniformivFn(GLuint program, GLint location, GLint* params) override;
void glGetUniformivRobustANGLEFn(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei* length,
                                 GLint* params) override;
GLint glGetUniformLocationFn(GLuint program, const char* name) override;
void glGetUniformuivFn(GLuint program, GLint location, GLuint* params) override;
void glGetUniformuivRobustANGLEFn(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLuint* params) override;
void glGetVertexAttribfvFn(GLuint index,
                           GLenum pname,
                           GLfloat* params) override;
void glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLfloat* params) override;
void glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint* params) override;
void glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLuint* params) override;
void glGetVertexAttribivFn(GLuint index, GLenum pname, GLint* params) override;
void glGetVertexAttribivRobustANGLEFn(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint* params) override;
void glGetVertexAttribPointervFn(GLuint index,
                                 GLenum pname,
                                 void** pointer) override;
void glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            void** pointer) override;
void glHintFn(GLenum target, GLenum mode) override;
void glImportMemoryFdEXTFn(GLuint memory,
                           GLuint64 size,
                           GLenum handleType,
                           GLint fd) override;
void glImportMemoryWin32HandleEXTFn(GLuint memory,
                                    GLuint64 size,
                                    GLenum handleType,
                                    void* handle) override;
void glImportMemoryZirconHandleANGLEFn(GLuint memory,
                                       GLuint64 size,
                                       GLenum handleType,
                                       GLuint handle) override;
void glImportSemaphoreFdEXTFn(GLuint semaphore,
                              GLenum handleType,
                              GLint fd) override;
void glImportSemaphoreWin32HandleEXTFn(GLuint semaphore,
                                       GLenum handleType,
                                       void* handle) override;
void glImportSemaphoreZirconHandleANGLEFn(GLuint semaphore,
                                          GLenum handleType,
                                          GLuint handle) override;
void glInsertEventMarkerEXTFn(GLsizei length, const char* marker) override;
void glInvalidateFramebufferFn(GLenum target,
                               GLsizei numAttachments,
                               const GLenum* attachments) override;
void glInvalidateSubFramebufferFn(GLenum target,
                                  GLsizei numAttachments,
                                  const GLenum* attachments,
                                  GLint x,
                                  GLint y,
                                  GLint width,
                                  GLint height) override;
void glInvalidateTextureANGLEFn(GLenum target) override;
GLboolean glIsBufferFn(GLuint buffer) override;
GLboolean glIsEnabledFn(GLenum cap) override;
GLboolean glIsEnablediOESFn(GLenum target, GLuint index) override;
GLboolean glIsFenceNVFn(GLuint fence) override;
GLboolean glIsFramebufferEXTFn(GLuint framebuffer) override;
GLboolean glIsProgramFn(GLuint program) override;
GLboolean glIsProgramPipelineFn(GLuint pipeline) override;
GLboolean glIsQueryFn(GLuint query) override;
GLboolean glIsRenderbufferEXTFn(GLuint renderbuffer) override;
GLboolean glIsSamplerFn(GLuint sampler) override;
GLboolean glIsShaderFn(GLuint shader) override;
GLboolean glIsSyncFn(GLsync sync) override;
GLboolean glIsTextureFn(GLuint texture) override;
GLboolean glIsTransformFeedbackFn(GLuint id) override;
GLboolean glIsVertexArrayOESFn(GLuint array) override;
void glLineWidthFn(GLfloat width) override;
void glLinkProgramFn(GLuint program) override;
void* glMapBufferFn(GLenum target, GLenum access) override;
void* glMapBufferRangeFn(GLenum target,
                         GLintptr offset,
                         GLsizeiptr length,
                         GLbitfield access) override;
void glMaxShaderCompilerThreadsKHRFn(GLuint count) override;
void glMemoryBarrierByRegionFn(GLbitfield barriers) override;
void glMemoryBarrierEXTFn(GLbitfield barriers) override;
void glMemoryObjectParameterivEXTFn(GLuint memoryObject,
                                    GLenum pname,
                                    const GLint* param) override;
void glMinSampleShadingFn(GLfloat value) override;
void glMultiDrawArraysANGLEFn(GLenum mode,
                              const GLint* firsts,
                              const GLsizei* counts,
                              GLsizei drawcount) override;
void glMultiDrawArraysInstancedANGLEFn(GLenum mode,
                                       const GLint* firsts,
                                       const GLsizei* counts,
                                       const GLsizei* instanceCounts,
                                       GLsizei drawcount) override;
void glMultiDrawArraysInstancedBaseInstanceANGLEFn(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instanceCounts,
    const GLuint* baseInstances,
    GLsizei drawcount) override;
void glMultiDrawElementsANGLEFn(GLenum mode,
                                const GLsizei* counts,
                                GLenum type,
                                const GLvoid* const* indices,
                                GLsizei drawcount) override;
void glMultiDrawElementsInstancedANGLEFn(GLenum mode,
                                         const GLsizei* counts,
                                         GLenum type,
                                         const GLvoid* const* indices,
                                         const GLsizei* instanceCounts,
                                         GLsizei drawcount) override;
void glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLvoid* const* indices,
    const GLsizei* instanceCounts,
    const GLint* baseVertices,
    const GLuint* baseInstances,
    GLsizei drawcount) override;
void glObjectLabelFn(GLenum identifier,
                     GLuint name,
                     GLsizei length,
                     const char* label) override;
void glObjectPtrLabelFn(void* ptr, GLsizei length, const char* label) override;
void glPatchParameteriFn(GLenum pname, GLint value) override;
void glPauseTransformFeedbackFn(void) override;
void glPixelLocalStorageBarrierANGLEFn() override;
void glPixelStoreiFn(GLenum pname, GLint param) override;
void glPointParameteriFn(GLenum pname, GLint param) override;
void glPolygonModeFn(GLenum face, GLenum mode) override;
void glPolygonModeANGLEFn(GLenum face, GLenum mode) override;
void glPolygonOffsetFn(GLfloat factor, GLfloat units) override;
void glPolygonOffsetClampEXTFn(GLfloat factor,
                               GLfloat units,
                               GLfloat clamp) override;
void glPopDebugGroupFn() override;
void glPopGroupMarkerEXTFn(void) override;
void glPrimitiveRestartIndexFn(GLuint index) override;
void glProgramBinaryFn(GLuint program,
                       GLenum binaryFormat,
                       const GLvoid* binary,
                       GLsizei length) override;
void glProgramParameteriFn(GLuint program, GLenum pname, GLint value) override;
void glProgramUniform1fFn(GLuint program, GLint location, GLfloat v0) override;
void glProgramUniform1fvFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLfloat* value) override;
void glProgramUniform1iFn(GLuint program, GLint location, GLint v0) override;
void glProgramUniform1ivFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLint* value) override;
void glProgramUniform1uiFn(GLuint program, GLint location, GLuint v0) override;
void glProgramUniform1uivFn(GLuint program,
                            GLint location,
                            GLsizei count,
                            const GLuint* value) override;
void glProgramUniform2fFn(GLuint program,
                          GLint location,
                          GLfloat v0,
                          GLfloat v1) override;
void glProgramUniform2fvFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLfloat* value) override;
void glProgramUniform2iFn(GLuint program,
                          GLint location,
                          GLint v0,
                          GLint v1) override;
void glProgramUniform2ivFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLint* value) override;
void glProgramUniform2uiFn(GLuint program,
                           GLint location,
                           GLuint v0,
                           GLuint v1) override;
void glProgramUniform2uivFn(GLuint program,
                            GLint location,
                            GLsizei count,
                            const GLuint* value) override;
void glProgramUniform3fFn(GLuint program,
                          GLint location,
                          GLfloat v0,
                          GLfloat v1,
                          GLfloat v2) override;
void glProgramUniform3fvFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLfloat* value) override;
void glProgramUniform3iFn(GLuint program,
                          GLint location,
                          GLint v0,
                          GLint v1,
                          GLint v2) override;
void glProgramUniform3ivFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLint* value) override;
void glProgramUniform3uiFn(GLuint program,
                           GLint location,
                           GLuint v0,
                           GLuint v1,
                           GLuint v2) override;
void glProgramUniform3uivFn(GLuint program,
                            GLint location,
                            GLsizei count,
                            const GLuint* value) override;
void glProgramUniform4fFn(GLuint program,
                          GLint location,
                          GLfloat v0,
                          GLfloat v1,
                          GLfloat v2,
                          GLfloat v3) override;
void glProgramUniform4fvFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLfloat* value) override;
void glProgramUniform4iFn(GLuint program,
                          GLint location,
                          GLint v0,
                          GLint v1,
                          GLint v2,
                          GLint v3) override;
void glProgramUniform4ivFn(GLuint program,
                           GLint location,
                           GLsizei count,
                           const GLint* value) override;
void glProgramUniform4uiFn(GLuint program,
                           GLint location,
                           GLuint v0,
                           GLuint v1,
                           GLuint v2,
                           GLuint v3) override;
void glProgramUniform4uivFn(GLuint program,
                            GLint location,
                            GLsizei count,
                            const GLuint* value) override;
void glProgramUniformMatrix2fvFn(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) override;
void glProgramUniformMatrix2x3fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProgramUniformMatrix2x4fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProgramUniformMatrix3fvFn(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) override;
void glProgramUniformMatrix3x2fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProgramUniformMatrix3x4fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProgramUniformMatrix4fvFn(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) override;
void glProgramUniformMatrix4x2fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProgramUniformMatrix4x3fvFn(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat* value) override;
void glProvokingVertexANGLEFn(GLenum provokeMode) override;
void glPushDebugGroupFn(GLenum source,
                        GLuint id,
                        GLsizei length,
                        const char* message) override;
void glPushGroupMarkerEXTFn(GLsizei length, const char* marker) override;
void glQueryCounterFn(GLuint id, GLenum target) override;
void glReadBufferFn(GLenum src) override;
void glReadnPixelsRobustANGLEFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                GLsizei* length,
                                GLsizei* columns,
                                GLsizei* rows,
                                void* data) override;
void glReadPixelsFn(GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    void* pixels) override;
void glReadPixelsRobustANGLEFn(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLsizei* columns,
                               GLsizei* rows,
                               void* pixels) override;
void glReleaseShaderCompilerFn(void) override;
void glReleaseTexturesANGLEFn(GLuint numTextures,
                              const GLuint* textures,
                              GLenum* layouts) override;
void glRenderbufferStorageEXTFn(GLenum target,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height) override;
void glRenderbufferStorageMultisampleFn(GLenum target,
                                        GLsizei samples,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height) override;
void glRenderbufferStorageMultisampleAdvancedAMDFn(GLenum target,
                                                   GLsizei samples,
                                                   GLsizei storageSamples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) override;
void glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                           GLsizei samples,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) override;
void glRequestExtensionANGLEFn(const char* name) override;
void glResumeTransformFeedbackFn(void) override;
void glSampleCoverageFn(GLclampf value, GLboolean invert) override;
void glSampleMaskiFn(GLuint maskNumber, GLbitfield mask) override;
void glSamplerParameterfFn(GLuint sampler,
                           GLenum pname,
                           GLfloat param) override;
void glSamplerParameterfvFn(GLuint sampler,
                            GLenum pname,
                            const GLfloat* params) override;
void glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat* param) override;
void glSamplerParameteriFn(GLuint sampler, GLenum pname, GLint param) override;
void glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLint* param) override;
void glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLuint* param) override;
void glSamplerParameterivFn(GLuint sampler,
                            GLenum pname,
                            const GLint* params) override;
void glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint* param) override;
void glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) override;
void glSetFenceNVFn(GLuint fence, GLenum condition) override;
void glShaderBinaryFn(GLsizei n,
                      const GLuint* shaders,
                      GLenum binaryformat,
                      const void* binary,
                      GLsizei length) override;
void glShaderSourceFn(GLuint shader,
                      GLsizei count,
                      const char* const* str,
                      const GLint* length) override;
void glSignalSemaphoreEXTFn(GLuint semaphore,
                            GLuint numBufferBarriers,
                            const GLuint* buffers,
                            GLuint numTextureBarriers,
                            const GLuint* textures,
                            const GLenum* dstLayouts) override;
void glStartTilingQCOMFn(GLuint x,
                         GLuint y,
                         GLuint width,
                         GLuint height,
                         GLbitfield preserveMask) override;
void glStencilFuncFn(GLenum func, GLint ref, GLuint mask) override;
void glStencilFuncSeparateFn(GLenum face,
                             GLenum func,
                             GLint ref,
                             GLuint mask) override;
void glStencilMaskFn(GLuint mask) override;
void glStencilMaskSeparateFn(GLenum face, GLuint mask) override;
void glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) override;
void glStencilOpSeparateFn(GLenum face,
                           GLenum fail,
                           GLenum zfail,
                           GLenum zpass) override;
GLboolean glTestFenceNVFn(GLuint fence) override;
void glTexBufferFn(GLenum target,
                   GLenum internalformat,
                   GLuint buffer) override;
void glTexBufferRangeFn(GLenum target,
                        GLenum internalformat,
                        GLuint buffer,
                        GLintptr offset,
                        GLsizeiptr size) override;
void glTexImage2DFn(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels) override;
void glTexImage2DExternalANGLEFn(GLenum target,
                                 GLint level,
                                 GLint internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLint border,
                                 GLenum format,
                                 GLenum type) override;
void glTexImage2DRobustANGLEFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void* pixels) override;
void glTexImage3DFn(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels) override;
void glTexImage3DRobustANGLEFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void* pixels) override;
void glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) override;
void glTexParameterfvFn(GLenum target,
                        GLenum pname,
                        const GLfloat* params) override;
void glTexParameterfvRobustANGLEFn(GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLfloat* params) override;
void glTexParameteriFn(GLenum target, GLenum pname, GLint param) override;
void glTexParameterIivRobustANGLEFn(GLenum target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLint* params) override;
void glTexParameterIuivRobustANGLEFn(GLenum target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     const GLuint* params) override;
void glTexParameterivFn(GLenum target,
                        GLenum pname,
                        const GLint* params) override;
void glTexParameterivRobustANGLEFn(GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint* params) override;
void glTexStorage2DEXTFn(GLenum target,
                         GLsizei levels,
                         GLenum internalformat,
                         GLsizei width,
                         GLsizei height) override;
void glTexStorage2DMultisampleFn(GLenum target,
                                 GLsizei samples,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLboolean fixedsamplelocations) override;
void glTexStorage3DFn(GLenum target,
                      GLsizei levels,
                      GLenum internalformat,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth) override;
void glTexStorageMem2DEXTFn(GLenum target,
                            GLsizei levels,
                            GLenum internalFormat,
                            GLsizei width,
                            GLsizei height,
                            GLuint memory,
                            GLuint64 offset) override;
void glTexStorageMemFlags2DANGLEFn(GLenum target,
                                   GLsizei levels,
                                   GLenum internalFormat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLuint memory,
                                   GLuint64 offset,
                                   GLbitfield createFlags,
                                   GLbitfield usageFlags,
                                   const void* imageCreateInfoPNext) override;
void glTexSubImage2DFn(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type,
                       const void* pixels) override;
void glTexSubImage2DRobustANGLEFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  const void* pixels) override;
void glTexSubImage3DFn(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint zoffset,
                       GLsizei width,
                       GLsizei height,
                       GLsizei depth,
                       GLenum format,
                       GLenum type,
                       const void* pixels) override;
void glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                  const void* pixels) override;
void glTransformFeedbackVaryingsFn(GLuint program,
                                   GLsizei count,
                                   const char* const* varyings,
                                   GLenum bufferMode) override;
void glUniform1fFn(GLint location, GLfloat x) override;
void glUniform1fvFn(GLint location, GLsizei count, const GLfloat* v) override;
void glUniform1iFn(GLint location, GLint x) override;
void glUniform1ivFn(GLint location, GLsizei count, const GLint* v) override;
void glUniform1uiFn(GLint location, GLuint v0) override;
void glUniform1uivFn(GLint location, GLsizei count, const GLuint* v) override;
void glUniform2fFn(GLint location, GLfloat x, GLfloat y) override;
void glUniform2fvFn(GLint location, GLsizei count, const GLfloat* v) override;
void glUniform2iFn(GLint location, GLint x, GLint y) override;
void glUniform2ivFn(GLint location, GLsizei count, const GLint* v) override;
void glUniform2uiFn(GLint location, GLuint v0, GLuint v1) override;
void glUniform2uivFn(GLint location, GLsizei count, const GLuint* v) override;
void glUniform3fFn(GLint location, GLfloat x, GLfloat y, GLfloat z) override;
void glUniform3fvFn(GLint location, GLsizei count, const GLfloat* v) override;
void glUniform3iFn(GLint location, GLint x, GLint y, GLint z) override;
void glUniform3ivFn(GLint location, GLsizei count, const GLint* v) override;
void glUniform3uiFn(GLint location, GLuint v0, GLuint v1, GLuint v2) override;
void glUniform3uivFn(GLint location, GLsizei count, const GLuint* v) override;
void glUniform4fFn(GLint location,
                   GLfloat x,
                   GLfloat y,
                   GLfloat z,
                   GLfloat w) override;
void glUniform4fvFn(GLint location, GLsizei count, const GLfloat* v) override;
void glUniform4iFn(GLint location, GLint x, GLint y, GLint z, GLint w) override;
void glUniform4ivFn(GLint location, GLsizei count, const GLint* v) override;
void glUniform4uiFn(GLint location,
                    GLuint v0,
                    GLuint v1,
                    GLuint v2,
                    GLuint v3) override;
void glUniform4uivFn(GLint location, GLsizei count, const GLuint* v) override;
void glUniformBlockBindingFn(GLuint program,
                             GLuint uniformBlockIndex,
                             GLuint uniformBlockBinding) override;
void glUniformMatrix2fvFn(GLint location,
                          GLsizei count,
                          GLboolean transpose,
                          const GLfloat* value) override;
void glUniformMatrix2x3fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
void glUniformMatrix2x4fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
void glUniformMatrix3fvFn(GLint location,
                          GLsizei count,
                          GLboolean transpose,
                          const GLfloat* value) override;
void glUniformMatrix3x2fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
void glUniformMatrix3x4fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
void glUniformMatrix4fvFn(GLint location,
                          GLsizei count,
                          GLboolean transpose,
                          const GLfloat* value) override;
void glUniformMatrix4x2fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
void glUniformMatrix4x3fvFn(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat* value) override;
GLboolean glUnmapBufferFn(GLenum target) override;
void glUseProgramFn(GLuint program) override;
void glUseProgramStagesFn(GLuint pipeline,
                          GLbitfield stages,
                          GLuint program) override;
void glValidateProgramFn(GLuint program) override;
void glValidateProgramPipelineFn(GLuint pipeline) override;
void glVertexAttrib1fFn(GLuint indx, GLfloat x) override;
void glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) override;
void glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) override;
void glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) override;
void glVertexAttrib3fFn(GLuint indx, GLfloat x, GLfloat y, GLfloat z) override;
void glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) override;
void glVertexAttrib4fFn(GLuint indx,
                        GLfloat x,
                        GLfloat y,
                        GLfloat z,
                        GLfloat w) override;
void glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) override;
void glVertexAttribBindingFn(GLuint attribindex, GLuint bindingindex) override;
void glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) override;
void glVertexAttribFormatFn(GLuint attribindex,
                            GLint size,
                            GLenum type,
                            GLboolean normalized,
                            GLuint relativeoffset) override;
void glVertexAttribI4iFn(GLuint indx,
                         GLint x,
                         GLint y,
                         GLint z,
                         GLint w) override;
void glVertexAttribI4ivFn(GLuint indx, const GLint* values) override;
void glVertexAttribI4uiFn(GLuint indx,
                          GLuint x,
                          GLuint y,
                          GLuint z,
                          GLuint w) override;
void glVertexAttribI4uivFn(GLuint indx, const GLuint* values) override;
void glVertexAttribIFormatFn(GLuint attribindex,
                             GLint size,
                             GLenum type,
                             GLuint relativeoffset) override;
void glVertexAttribIPointerFn(GLuint indx,
                              GLint size,
                              GLenum type,
                              GLsizei stride,
                              const void* ptr) override;
void glVertexAttribPointerFn(GLuint indx,
                             GLint size,
                             GLenum type,
                             GLboolean normalized,
                             GLsizei stride,
                             const void* ptr) override;
void glVertexBindingDivisorFn(GLuint bindingindex, GLuint divisor) override;
void glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) override;
void glWaitSemaphoreEXTFn(GLuint semaphore,
                          GLuint numBufferBarriers,
                          const GLuint* buffers,
                          GLuint numTextureBarriers,
                          const GLuint* textures,
                          const GLenum* srcLayouts) override;
void glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) override;
void glWindowRectanglesEXTFn(GLenum mode, GLsizei n, const GLint* box) override;
