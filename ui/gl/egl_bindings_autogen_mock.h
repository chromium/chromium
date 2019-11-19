// Copyright 2016 The Chromium Authors. All rights reserved.
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

static EGLBoolean GL_BINDING_CALL Mock_eglBindAPI(EGLenum api);
static EGLBoolean GL_BINDING_CALL Mock_eglBindTexImage(EGLDisplay dpy,
                                                       EGLSurface surface,
                                                       EGLint buffer);
static EGLBoolean GL_BINDING_CALL
Mock_eglChooseConfig(EGLDisplay dpy,
                     const EGLint* attrib_list,
                     EGLConfig* configs,
                     EGLint config_size,
                     EGLint* num_config);
static EGLint GL_BINDING_CALL Mock_eglClientWaitSyncKHR(EGLDisplay dpy,
                                                        EGLSyncKHR sync,
                                                        EGLint flags,
                                                        EGLTimeKHR timeout);
static EGLBoolean GL_BINDING_CALL
Mock_eglCopyBuffers(EGLDisplay dpy,
                    EGLSurface surface,
                    EGLNativePixmapType target);
static EGLContext GL_BINDING_CALL
Mock_eglCreateContext(EGLDisplay dpy,
                      EGLConfig config,
                      EGLContext share_context,
                      const EGLint* attrib_list);
static EGLImageKHR GL_BINDING_CALL
Mock_eglCreateImageKHR(EGLDisplay dpy,
                       EGLContext ctx,
                       EGLenum target,
                       EGLClientBuffer buffer,
                       const EGLint* attrib_list);
static EGLSurface GL_BINDING_CALL
Mock_eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                      EGLenum buftype,
                                      void* buffer,
                                      EGLConfig config,
                                      const EGLint* attrib_list);
static EGLSurface GL_BINDING_CALL
Mock_eglCreatePbufferSurface(EGLDisplay dpy,
                             EGLConfig config,
                             const EGLint* attrib_list);
static EGLSurface GL_BINDING_CALL
Mock_eglCreatePixmapSurface(EGLDisplay dpy,
                            EGLConfig config,
                            EGLNativePixmapType pixmap,
                            const EGLint* attrib_list);
static EGLStreamKHR GL_BINDING_CALL
Mock_eglCreateStreamKHR(EGLDisplay dpy, const EGLint* attrib_list);
static EGLBoolean GL_BINDING_CALL
Mock_eglCreateStreamProducerD3DTextureANGLE(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLAttrib* attrib_list);
static EGLSyncKHR GL_BINDING_CALL
Mock_eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint* attrib_list);
static EGLSurface GL_BINDING_CALL
Mock_eglCreateWindowSurface(EGLDisplay dpy,
                            EGLConfig config,
                            EGLNativeWindowType win,
                            const EGLint* attrib_list);
static EGLint GL_BINDING_CALL
Mock_eglDebugMessageControlKHR(EGLDEBUGPROCKHR callback,
                               const EGLAttrib* attrib_list);
static EGLBoolean GL_BINDING_CALL Mock_eglDestroyContext(EGLDisplay dpy,
                                                         EGLContext ctx);
static EGLBoolean GL_BINDING_CALL Mock_eglDestroyImageKHR(EGLDisplay dpy,
                                                          EGLImageKHR image);
static EGLBoolean GL_BINDING_CALL Mock_eglDestroyStreamKHR(EGLDisplay dpy,
                                                           EGLStreamKHR stream);
static EGLBoolean GL_BINDING_CALL Mock_eglDestroySurface(EGLDisplay dpy,
                                                         EGLSurface surface);
static EGLBoolean GL_BINDING_CALL Mock_eglDestroySyncKHR(EGLDisplay dpy,
                                                         EGLSyncKHR sync);
static EGLint GL_BINDING_CALL Mock_eglDupNativeFenceFDANDROID(EGLDisplay dpy,
                                                              EGLSyncKHR sync);
static EGLBoolean GL_BINDING_CALL
Mock_eglExportDMABUFImageMESA(EGLDisplay dpy,
                              EGLImageKHR image,
                              int* fds,
                              EGLint* strides,
                              EGLint* offsets);
static EGLBoolean GL_BINDING_CALL
Mock_eglExportDMABUFImageQueryMESA(EGLDisplay dpy,
                                   EGLImageKHR image,
                                   int* fourcc,
                                   int* num_planes,
                                   EGLuint64KHR* modifiers);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetCompositorTimingANDROID(EGLDisplay dpy,
                                   EGLSurface surface,
                                   EGLint numTimestamps,
                                   EGLint* names,
                                   EGLnsecsANDROID* values);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetCompositorTimingSupportedANDROID(EGLDisplay dpy,
                                            EGLSurface surface,
                                            EGLint timestamp);
static EGLBoolean GL_BINDING_CALL Mock_eglGetConfigAttrib(EGLDisplay dpy,
                                                          EGLConfig config,
                                                          EGLint attribute,
                                                          EGLint* value);
static EGLBoolean GL_BINDING_CALL Mock_eglGetConfigs(EGLDisplay dpy,
                                                     EGLConfig* configs,
                                                     EGLint config_size,
                                                     EGLint* num_config);
static EGLContext GL_BINDING_CALL Mock_eglGetCurrentContext(void);
static EGLDisplay GL_BINDING_CALL Mock_eglGetCurrentDisplay(void);
static EGLSurface GL_BINDING_CALL Mock_eglGetCurrentSurface(EGLint readdraw);
static EGLDisplay GL_BINDING_CALL
Mock_eglGetDisplay(EGLNativeDisplayType display_id);
static EGLint GL_BINDING_CALL Mock_eglGetError(void);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetFrameTimestampSupportedANDROID(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint timestamp);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetFrameTimestampsANDROID(EGLDisplay dpy,
                                  EGLSurface surface,
                                  EGLuint64KHR frameId,
                                  EGLint numTimestamps,
                                  EGLint* timestamps,
                                  EGLnsecsANDROID* values);
static EGLClientBuffer GL_BINDING_CALL Mock_eglGetNativeClientBufferANDROID(
    const struct AHardwareBuffer* ahardwarebuffer);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetNextFrameIdANDROID(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLuint64KHR* frameId);
static EGLDisplay GL_BINDING_CALL
Mock_eglGetPlatformDisplay(EGLenum platform,
                           void* native_display,
                           const EGLAttrib* attrib_list);
static __eglMustCastToProperFunctionPointerType GL_BINDING_CALL
Mock_eglGetProcAddress(const char* procname);
static EGLBoolean GL_BINDING_CALL Mock_eglGetSyncAttribKHR(EGLDisplay dpy,
                                                           EGLSyncKHR sync,
                                                           EGLint attribute,
                                                           EGLint* value);
static EGLBoolean GL_BINDING_CALL
Mock_eglGetSyncValuesCHROMIUM(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLuint64CHROMIUM* ust,
                              EGLuint64CHROMIUM* msc,
                              EGLuint64CHROMIUM* sbc);
static EGLBoolean GL_BINDING_CALL
Mock_eglImageFlushExternalEXT(EGLDisplay dpy,
                              EGLImageKHR image,
                              const EGLAttrib* attrib_list);
static EGLBoolean GL_BINDING_CALL Mock_eglInitialize(EGLDisplay dpy,
                                                     EGLint* major,
                                                     EGLint* minor);
static EGLint GL_BINDING_CALL Mock_eglLabelObjectKHR(EGLDisplay display,
                                                     EGLenum objectType,
                                                     EGLObjectKHR object,
                                                     EGLLabelKHR label);
static EGLBoolean GL_BINDING_CALL Mock_eglMakeCurrent(EGLDisplay dpy,
                                                      EGLSurface draw,
                                                      EGLSurface read,
                                                      EGLContext ctx);
static EGLBoolean GL_BINDING_CALL Mock_eglPostSubBufferNV(EGLDisplay dpy,
                                                          EGLSurface surface,
                                                          EGLint x,
                                                          EGLint y,
                                                          EGLint width,
                                                          EGLint height);
static EGLenum GL_BINDING_CALL Mock_eglQueryAPI(void);
static EGLBoolean GL_BINDING_CALL Mock_eglQueryContext(EGLDisplay dpy,
                                                       EGLContext ctx,
                                                       EGLint attribute,
                                                       EGLint* value);
static EGLBoolean GL_BINDING_CALL Mock_eglQueryDebugKHR(EGLint attribute,
                                                        EGLAttrib* value);
static EGLBoolean GL_BINDING_CALL
Mock_eglQueryDisplayAttribANGLE(EGLDisplay dpy,
                                EGLint attribute,
                                EGLAttrib* value);
static EGLBoolean GL_BINDING_CALL Mock_eglQueryStreamKHR(EGLDisplay dpy,
                                                         EGLStreamKHR stream,
                                                         EGLenum attribute,
                                                         EGLint* value);
static EGLBoolean GL_BINDING_CALL
Mock_eglQueryStreamu64KHR(EGLDisplay dpy,
                          EGLStreamKHR stream,
                          EGLenum attribute,
                          EGLuint64KHR* value);
static const char* GL_BINDING_CALL Mock_eglQueryString(EGLDisplay dpy,
                                                       EGLint name);
static const char* GL_BINDING_CALL Mock_eglQueryStringiANGLE(EGLDisplay dpy,
                                                             EGLint name,
                                                             EGLint index);
static EGLBoolean GL_BINDING_CALL Mock_eglQuerySurface(EGLDisplay dpy,
                                                       EGLSurface surface,
                                                       EGLint attribute,
                                                       EGLint* value);
static EGLBoolean GL_BINDING_CALL
Mock_eglQuerySurfacePointerANGLE(EGLDisplay dpy,
                                 EGLSurface surface,
                                 EGLint attribute,
                                 void** value);
static EGLBoolean GL_BINDING_CALL Mock_eglReleaseTexImage(EGLDisplay dpy,
                                                          EGLSurface surface,
                                                          EGLint buffer);
static EGLBoolean GL_BINDING_CALL Mock_eglReleaseThread(void);
static void GL_BINDING_CALL
Mock_eglSetBlobCacheFuncsANDROID(EGLDisplay dpy,
                                 EGLSetBlobFuncANDROID set,
                                 EGLGetBlobFuncANDROID get);
static EGLBoolean GL_BINDING_CALL Mock_eglStreamAttribKHR(EGLDisplay dpy,
                                                          EGLStreamKHR stream,
                                                          EGLenum attribute,
                                                          EGLint value);
static EGLBoolean GL_BINDING_CALL
Mock_eglStreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream);
static EGLBoolean GL_BINDING_CALL
Mock_eglStreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy,
                                                 EGLStreamKHR stream,
                                                 EGLAttrib* attrib_list);
static EGLBoolean GL_BINDING_CALL
Mock_eglStreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream);
static EGLBoolean GL_BINDING_CALL
Mock_eglStreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream);
static EGLBoolean GL_BINDING_CALL
Mock_eglStreamPostD3DTextureANGLE(EGLDisplay dpy,
                                  EGLStreamKHR stream,
                                  void* texture,
                                  const EGLAttrib* attrib_list);
static EGLBoolean GL_BINDING_CALL Mock_eglSurfaceAttrib(EGLDisplay dpy,
                                                        EGLSurface surface,
                                                        EGLint attribute,
                                                        EGLint value);
static EGLBoolean GL_BINDING_CALL Mock_eglSwapBuffers(EGLDisplay dpy,
                                                      EGLSurface surface);
static EGLBoolean GL_BINDING_CALL
Mock_eglSwapBuffersWithDamageKHR(EGLDisplay dpy,
                                 EGLSurface surface,
                                 EGLint* rects,
                                 EGLint n_rects);
static EGLBoolean GL_BINDING_CALL Mock_eglSwapInterval(EGLDisplay dpy,
                                                       EGLint interval);
static EGLBoolean GL_BINDING_CALL Mock_eglTerminate(EGLDisplay dpy);
static EGLBoolean GL_BINDING_CALL Mock_eglWaitClient(void);
static EGLBoolean GL_BINDING_CALL Mock_eglWaitGL(void);
static EGLBoolean GL_BINDING_CALL Mock_eglWaitNative(EGLint engine);
static EGLint GL_BINDING_CALL Mock_eglWaitSyncKHR(EGLDisplay dpy,
                                                  EGLSyncKHR sync,
                                                  EGLint flags);
