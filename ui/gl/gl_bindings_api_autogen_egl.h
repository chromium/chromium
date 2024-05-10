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

void eglAcquireExternalContextANGLEFn(EGLDisplay dpy,
                                      EGLSurface readAndDraw) override;
EGLBoolean eglBindAPIFn(EGLenum api) override;
EGLBoolean eglBindTexImageFn(EGLDisplay dpy,
                             EGLSurface surface,
                             EGLint buffer) override;
EGLBoolean eglChooseConfigFn(EGLDisplay dpy,
                             const EGLint* attrib_list,
                             EGLConfig* configs,
                             EGLint config_size,
                             EGLint* num_config) override;
EGLint eglClientWaitSyncFn(EGLDisplay dpy,
                           EGLSync sync,
                           EGLint flags,
                           EGLTime timeout) override;
EGLint eglClientWaitSyncKHRFn(EGLDisplay dpy,
                              EGLSyncKHR sync,
                              EGLint flags,
                              EGLTimeKHR timeout) override;
EGLBoolean eglCopyBuffersFn(EGLDisplay dpy,
                            EGLSurface surface,
                            EGLNativePixmapType target) override;
void* eglCopyMetalSharedEventANGLEFn(EGLDisplay dpy, EGLSync sync) override;
EGLContext eglCreateContextFn(EGLDisplay dpy,
                              EGLConfig config,
                              EGLContext share_context,
                              const EGLint* attrib_list) override;
EGLImage eglCreateImageFn(EGLDisplay dpy,
                          EGLContext ctx,
                          EGLenum target,
                          EGLClientBuffer buffer,
                          const EGLAttrib* attrib_list) override;
EGLImageKHR eglCreateImageKHRFn(EGLDisplay dpy,
                                EGLContext ctx,
                                EGLenum target,
                                EGLClientBuffer buffer,
                                const EGLint* attrib_list) override;
EGLSurface eglCreatePbufferFromClientBufferFn(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) override;
EGLSurface eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                     EGLConfig config,
                                     const EGLint* attrib_list) override;
EGLSurface eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                    EGLConfig config,
                                    EGLNativePixmapType pixmap,
                                    const EGLint* attrib_list) override;
EGLSurface eglCreatePlatformPixmapSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list) override;
EGLSurface eglCreatePlatformWindowSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list) override;
EGLStreamKHR eglCreateStreamKHRFn(EGLDisplay dpy,
                                  const EGLint* attrib_list) override;
EGLBoolean eglCreateStreamProducerD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) override;
EGLSync eglCreateSyncFn(EGLDisplay dpy,
                        EGLenum type,
                        const EGLAttrib* attrib_list) override;
EGLSyncKHR eglCreateSyncKHRFn(EGLDisplay dpy,
                              EGLenum type,
                              const EGLint* attrib_list) override;
EGLSurface eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                    EGLConfig config,
                                    EGLNativeWindowType win,
                                    const EGLint* attrib_list) override;
EGLint eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                   const EGLAttrib* attrib_list) override;
EGLBoolean eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) override;
EGLBoolean eglDestroyImageFn(EGLDisplay dpy, EGLImage image) override;
EGLBoolean eglDestroyImageKHRFn(EGLDisplay dpy, EGLImageKHR image) override;
EGLBoolean eglDestroyStreamKHRFn(EGLDisplay dpy, EGLStreamKHR stream) override;
EGLBoolean eglDestroySurfaceFn(EGLDisplay dpy, EGLSurface surface) override;
EGLBoolean eglDestroySyncFn(EGLDisplay dpy, EGLSync sync) override;
EGLBoolean eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) override;
EGLint eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy, EGLSyncKHR sync) override;
EGLBoolean eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                      EGLImageKHR image,
                                      int* fds,
                                      EGLint* strides,
                                      EGLint* offsets) override;
EGLBoolean eglExportDMABUFImageQueryMESAFn(EGLDisplay dpy,
                                           EGLImageKHR image,
                                           int* fourcc,
                                           int* num_planes,
                                           EGLuint64KHR* modifiers) override;
EGLBoolean eglExportVkImageANGLEFn(EGLDisplay dpy,
                                   EGLImageKHR image,
                                   void* vk_image,
                                   void* vk_image_create_info) override;
EGLBoolean eglGetCompositorTimingANDROIDFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint numTimestamps,
                                           EGLint* names,
                                           EGLnsecsANDROID* values) override;
EGLBoolean eglGetCompositorTimingSupportedANDROIDFn(EGLDisplay dpy,
                                                    EGLSurface surface,
                                                    EGLint timestamp) override;
EGLBoolean eglGetConfigAttribFn(EGLDisplay dpy,
                                EGLConfig config,
                                EGLint attribute,
                                EGLint* value) override;
EGLBoolean eglGetConfigsFn(EGLDisplay dpy,
                           EGLConfig* configs,
                           EGLint config_size,
                           EGLint* num_config) override;
EGLContext eglGetCurrentContextFn(void) override;
EGLDisplay eglGetCurrentDisplayFn(void) override;
EGLSurface eglGetCurrentSurfaceFn(EGLint readdraw) override;
EGLDisplay eglGetDisplayFn(EGLNativeDisplayType display_id) override;
EGLint eglGetErrorFn(void) override;
EGLBoolean eglGetFrameTimestampsANDROIDFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLuint64KHR frameId,
                                          EGLint numTimestamps,
                                          EGLint* timestamps,
                                          EGLnsecsANDROID* values) override;
EGLBoolean eglGetFrameTimestampSupportedANDROIDFn(EGLDisplay dpy,
                                                  EGLSurface surface,
                                                  EGLint timestamp) override;
EGLBoolean eglGetMscRateANGLEFn(EGLDisplay dpy,
                                EGLSurface surface,
                                EGLint* numerator,
                                EGLint* denominator) override;
EGLClientBuffer eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) override;
EGLBoolean eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                      EGLSurface surface,
                                      EGLuint64KHR* frameId) override;
EGLDisplay eglGetPlatformDisplayFn(EGLenum platform,
                                   void* native_display,
                                   const EGLAttrib* attrib_list) override;
__eglMustCastToProperFunctionPointerType eglGetProcAddressFn(
    const char* procname) override;
EGLBoolean eglGetSyncAttribFn(EGLDisplay dpy,
                              EGLSync sync,
                              EGLint attribute,
                              EGLAttrib* value) override;
EGLBoolean eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                 EGLSyncKHR sync,
                                 EGLint attribute,
                                 EGLint* value) override;
EGLBoolean eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                      EGLSurface surface,
                                      EGLuint64CHROMIUM* ust,
                                      EGLuint64CHROMIUM* msc,
                                      EGLuint64CHROMIUM* sbc) override;
void eglHandleGPUSwitchANGLEFn(EGLDisplay dpy) override;
EGLBoolean eglImageFlushExternalEXTFn(EGLDisplay dpy,
                                      EGLImageKHR image,
                                      const EGLAttrib* attrib_list) override;
EGLBoolean eglInitializeFn(EGLDisplay dpy,
                           EGLint* major,
                           EGLint* minor) override;
EGLint eglLabelObjectKHRFn(EGLDisplay display,
                           EGLenum objectType,
                           EGLObjectKHR object,
                           EGLLabelKHR label) override;
EGLBoolean eglMakeCurrentFn(EGLDisplay dpy,
                            EGLSurface draw,
                            EGLSurface read,
                            EGLContext ctx) override;
EGLBoolean eglPostSubBufferNVFn(EGLDisplay dpy,
                                EGLSurface surface,
                                EGLint x,
                                EGLint y,
                                EGLint width,
                                EGLint height) override;
EGLenum eglQueryAPIFn(void) override;
EGLBoolean eglQueryContextFn(EGLDisplay dpy,
                             EGLContext ctx,
                             EGLint attribute,
                             EGLint* value) override;
EGLBoolean eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) override;
EGLBoolean eglQueryDeviceAttribEXTFn(EGLDeviceEXT device,
                                     EGLint attribute,
                                     EGLAttrib* value) override;
EGLBoolean eglQueryDevicesEXTFn(EGLint max_devices,
                                EGLDeviceEXT* devices,
                                EGLint* num_devices) override;
const char* eglQueryDeviceStringEXTFn(EGLDeviceEXT device,
                                      EGLint name) override;
EGLBoolean eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                        EGLint attribute,
                                        EGLAttrib* value) override;
EGLBoolean eglQueryDisplayAttribEXTFn(EGLDisplay dpy,
                                      EGLint attribute,
                                      EGLAttrib* value) override;
EGLBoolean eglQueryDmaBufFormatsEXTFn(EGLDisplay dpy,
                                      EGLint max_formats,
                                      EGLint* formats,
                                      EGLint* num_formats) override;
EGLBoolean eglQueryDmaBufModifiersEXTFn(EGLDisplay dpy,
                                        EGLint format,
                                        EGLint max_modifiers,
                                        EGLuint64KHR* modifiers,
                                        EGLBoolean* external_only,
                                        EGLint* num_modifiers) override;
EGLBoolean eglQueryStreamKHRFn(EGLDisplay dpy,
                               EGLStreamKHR stream,
                               EGLenum attribute,
                               EGLint* value) override;
EGLBoolean eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                  EGLStreamKHR stream,
                                  EGLenum attribute,
                                  EGLuint64KHR* value) override;
const char* eglQueryStringFn(EGLDisplay dpy, EGLint name) override;
const char* eglQueryStringiANGLEFn(EGLDisplay dpy,
                                   EGLint name,
                                   EGLint index) override;
EGLBoolean eglQuerySurfaceFn(EGLDisplay dpy,
                             EGLSurface surface,
                             EGLint attribute,
                             EGLint* value) override;
EGLBoolean eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLint attribute,
                                         void** value) override;
void eglReacquireHighPowerGPUANGLEFn(EGLDisplay dpy, EGLContext ctx) override;
void eglReleaseExternalContextANGLEFn(EGLDisplay dpy) override;
void eglReleaseHighPowerGPUANGLEFn(EGLDisplay dpy, EGLContext ctx) override;
EGLBoolean eglReleaseTexImageFn(EGLDisplay dpy,
                                EGLSurface surface,
                                EGLint buffer) override;
EGLBoolean eglReleaseThreadFn(void) override;
void eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                   EGLSetBlobFuncANDROID set,
                                   EGLGetBlobFuncANDROID get) override;
void eglSetValidationEnabledANGLEFn(EGLBoolean validationState) override;
EGLBoolean eglStreamAttribKHRFn(EGLDisplay dpy,
                                EGLStreamKHR stream,
                                EGLenum attribute,
                                EGLint value) override;
EGLBoolean eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                         EGLStreamKHR stream) override;
EGLBoolean eglStreamConsumerGLTextureExternalAttribsNVFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) override;
EGLBoolean eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) override;
EGLBoolean eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                         EGLStreamKHR stream) override;
EGLBoolean eglStreamPostD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) override;
EGLBoolean eglSurfaceAttribFn(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint attribute,
                              EGLint value) override;
EGLBoolean eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) override;
EGLBoolean eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLint* rects,
                                         EGLint n_rects) override;
EGLBoolean eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) override;
EGLBoolean eglTerminateFn(EGLDisplay dpy) override;
EGLBoolean eglWaitClientFn(void) override;
EGLBoolean eglWaitGLFn(void) override;
EGLBoolean eglWaitNativeFn(EGLint engine) override;
EGLint eglWaitSyncFn(EGLDisplay dpy, EGLSync sync, EGLint flags) override;
EGLint eglWaitSyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags) override;
void eglWaitUntilWorkScheduledANGLEFn(EGLDisplay dpy) override;
