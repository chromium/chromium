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

MOCK_METHOD2(AcquireExternalContextANGLE,
             void(EGLDisplay dpy, EGLSurface readAndDraw));
MOCK_METHOD1(BindAPI, EGLBoolean(EGLenum api));
MOCK_METHOD3(BindTexImage,
             EGLBoolean(EGLDisplay dpy, EGLSurface surface, EGLint buffer));
MOCK_METHOD5(ChooseConfig,
             EGLBoolean(EGLDisplay dpy,
                        const EGLint* attrib_list,
                        EGLConfig* configs,
                        EGLint config_size,
                        EGLint* num_config));
MOCK_METHOD4(
    ClientWaitSync,
    EGLint(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout));
MOCK_METHOD4(
    ClientWaitSyncKHR,
    EGLint(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout));
MOCK_METHOD3(CopyBuffers,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLNativePixmapType target));
MOCK_METHOD2(CopyMetalSharedEventANGLE, void*(EGLDisplay dpy, EGLSync sync));
MOCK_METHOD4(CreateContext,
             EGLContext(EGLDisplay dpy,
                        EGLConfig config,
                        EGLContext share_context,
                        const EGLint* attrib_list));
MOCK_METHOD5(CreateImage,
             EGLImage(EGLDisplay dpy,
                      EGLContext ctx,
                      EGLenum target,
                      EGLClientBuffer buffer,
                      const EGLAttrib* attrib_list));
MOCK_METHOD5(CreateImageKHR,
             EGLImageKHR(EGLDisplay dpy,
                         EGLContext ctx,
                         EGLenum target,
                         EGLClientBuffer buffer,
                         const EGLint* attrib_list));
MOCK_METHOD5(CreatePbufferFromClientBuffer,
             EGLSurface(EGLDisplay dpy,
                        EGLenum buftype,
                        void* buffer,
                        EGLConfig config,
                        const EGLint* attrib_list));
MOCK_METHOD3(CreatePbufferSurface,
             EGLSurface(EGLDisplay dpy,
                        EGLConfig config,
                        const EGLint* attrib_list));
MOCK_METHOD4(CreatePixmapSurface,
             EGLSurface(EGLDisplay dpy,
                        EGLConfig config,
                        EGLNativePixmapType pixmap,
                        const EGLint* attrib_list));
MOCK_METHOD4(CreatePlatformPixmapSurface,
             EGLSurface(EGLDisplay dpy,
                        EGLConfig config,
                        void* native_pixmap,
                        const EGLAttrib* attrib_list));
MOCK_METHOD4(CreatePlatformWindowSurface,
             EGLSurface(EGLDisplay dpy,
                        EGLConfig config,
                        void* native_window,
                        const EGLAttrib* attrib_list));
MOCK_METHOD2(CreateStreamKHR,
             EGLStreamKHR(EGLDisplay dpy, const EGLint* attrib_list));
MOCK_METHOD3(CreateStreamProducerD3DTextureANGLE,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        EGLAttrib* attrib_list));
MOCK_METHOD3(CreateSync,
             EGLSync(EGLDisplay dpy,
                     EGLenum type,
                     const EGLAttrib* attrib_list));
MOCK_METHOD3(CreateSyncKHR,
             EGLSyncKHR(EGLDisplay dpy,
                        EGLenum type,
                        const EGLint* attrib_list));
MOCK_METHOD4(CreateWindowSurface,
             EGLSurface(EGLDisplay dpy,
                        EGLConfig config,
                        EGLNativeWindowType win,
                        const EGLint* attrib_list));
MOCK_METHOD2(DebugMessageControlKHR,
             EGLint(EGLDEBUGPROCKHR callback, const EGLAttrib* attrib_list));
MOCK_METHOD2(DestroyContext, EGLBoolean(EGLDisplay dpy, EGLContext ctx));
MOCK_METHOD2(DestroyImage, EGLBoolean(EGLDisplay dpy, EGLImage image));
MOCK_METHOD2(DestroyImageKHR, EGLBoolean(EGLDisplay dpy, EGLImageKHR image));
MOCK_METHOD2(DestroyStreamKHR, EGLBoolean(EGLDisplay dpy, EGLStreamKHR stream));
MOCK_METHOD2(DestroySurface, EGLBoolean(EGLDisplay dpy, EGLSurface surface));
MOCK_METHOD2(DestroySync, EGLBoolean(EGLDisplay dpy, EGLSync sync));
MOCK_METHOD2(DestroySyncKHR, EGLBoolean(EGLDisplay dpy, EGLSyncKHR sync));
MOCK_METHOD2(DupNativeFenceFDANDROID, EGLint(EGLDisplay dpy, EGLSyncKHR sync));
MOCK_METHOD5(ExportDMABUFImageMESA,
             EGLBoolean(EGLDisplay dpy,
                        EGLImageKHR image,
                        int* fds,
                        EGLint* strides,
                        EGLint* offsets));
MOCK_METHOD5(ExportDMABUFImageQueryMESA,
             EGLBoolean(EGLDisplay dpy,
                        EGLImageKHR image,
                        int* fourcc,
                        int* num_planes,
                        EGLuint64KHR* modifiers));
MOCK_METHOD4(ExportVkImageANGLE,
             EGLBoolean(EGLDisplay dpy,
                        EGLImageKHR image,
                        void* vk_image,
                        void* vk_image_create_info));
MOCK_METHOD5(GetCompositorTimingANDROID,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint numTimestamps,
                        EGLint* names,
                        EGLnsecsANDROID* values));
MOCK_METHOD3(GetCompositorTimingSupportedANDROID,
             EGLBoolean(EGLDisplay dpy, EGLSurface surface, EGLint timestamp));
MOCK_METHOD4(GetConfigAttrib,
             EGLBoolean(EGLDisplay dpy,
                        EGLConfig config,
                        EGLint attribute,
                        EGLint* value));
MOCK_METHOD4(GetConfigs,
             EGLBoolean(EGLDisplay dpy,
                        EGLConfig* configs,
                        EGLint config_size,
                        EGLint* num_config));
MOCK_METHOD0(GetCurrentContext, EGLContext());
MOCK_METHOD0(GetCurrentDisplay, EGLDisplay());
MOCK_METHOD1(GetCurrentSurface, EGLSurface(EGLint readdraw));
MOCK_METHOD1(GetDisplay, EGLDisplay(EGLNativeDisplayType display_id));
MOCK_METHOD0(GetError, EGLint());
MOCK_METHOD6(GetFrameTimestampsANDROID,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLuint64KHR frameId,
                        EGLint numTimestamps,
                        EGLint* timestamps,
                        EGLnsecsANDROID* values));
MOCK_METHOD3(GetFrameTimestampSupportedANDROID,
             EGLBoolean(EGLDisplay dpy, EGLSurface surface, EGLint timestamp));
MOCK_METHOD4(GetMscRateANGLE,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint* numerator,
                        EGLint* denominator));
MOCK_METHOD1(GetNativeClientBufferANDROID,
             EGLClientBuffer(const struct AHardwareBuffer* ahardwarebuffer));
MOCK_METHOD3(GetNextFrameIdANDROID,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLuint64KHR* frameId));
MOCK_METHOD3(GetPlatformDisplay,
             EGLDisplay(EGLenum platform,
                        void* native_display,
                        const EGLAttrib* attrib_list));
MOCK_METHOD1(GetProcAddress,
             __eglMustCastToProperFunctionPointerType(const char* procname));
MOCK_METHOD4(GetSyncAttrib,
             EGLBoolean(EGLDisplay dpy,
                        EGLSync sync,
                        EGLint attribute,
                        EGLAttrib* value));
MOCK_METHOD4(GetSyncAttribKHR,
             EGLBoolean(EGLDisplay dpy,
                        EGLSyncKHR sync,
                        EGLint attribute,
                        EGLint* value));
MOCK_METHOD5(GetSyncValuesCHROMIUM,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLuint64CHROMIUM* ust,
                        EGLuint64CHROMIUM* msc,
                        EGLuint64CHROMIUM* sbc));
MOCK_METHOD1(HandleGPUSwitchANGLE, void(EGLDisplay dpy));
MOCK_METHOD3(ImageFlushExternalEXT,
             EGLBoolean(EGLDisplay dpy,
                        EGLImageKHR image,
                        const EGLAttrib* attrib_list));
MOCK_METHOD3(Initialize,
             EGLBoolean(EGLDisplay dpy, EGLint* major, EGLint* minor));
MOCK_METHOD4(LabelObjectKHR,
             EGLint(EGLDisplay display,
                    EGLenum objectType,
                    EGLObjectKHR object,
                    EGLLabelKHR label));
MOCK_METHOD4(MakeCurrent,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface draw,
                        EGLSurface read,
                        EGLContext ctx));
MOCK_METHOD6(PostSubBufferNV,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint x,
                        EGLint y,
                        EGLint width,
                        EGLint height));
MOCK_METHOD0(QueryAPI, EGLenum());
MOCK_METHOD4(QueryContext,
             EGLBoolean(EGLDisplay dpy,
                        EGLContext ctx,
                        EGLint attribute,
                        EGLint* value));
MOCK_METHOD2(QueryDebugKHR, EGLBoolean(EGLint attribute, EGLAttrib* value));
MOCK_METHOD3(QueryDeviceAttribEXT,
             EGLBoolean(EGLDeviceEXT device,
                        EGLint attribute,
                        EGLAttrib* value));
MOCK_METHOD3(QueryDevicesEXT,
             EGLBoolean(EGLint max_devices,
                        EGLDeviceEXT* devices,
                        EGLint* num_devices));
MOCK_METHOD2(QueryDeviceStringEXT,
             const char*(EGLDeviceEXT device, EGLint name));
MOCK_METHOD3(QueryDisplayAttribANGLE,
             EGLBoolean(EGLDisplay dpy, EGLint attribute, EGLAttrib* value));
MOCK_METHOD3(QueryDisplayAttribEXT,
             EGLBoolean(EGLDisplay dpy, EGLint attribute, EGLAttrib* value));
MOCK_METHOD4(QueryDmaBufFormatsEXT,
             EGLBoolean(EGLDisplay dpy,
                        EGLint max_formats,
                        EGLint* formats,
                        EGLint* num_formats));
MOCK_METHOD6(QueryDmaBufModifiersEXT,
             EGLBoolean(EGLDisplay dpy,
                        EGLint format,
                        EGLint max_modifiers,
                        EGLuint64KHR* modifiers,
                        EGLBoolean* external_only,
                        EGLint* num_modifiers));
MOCK_METHOD4(QueryStreamKHR,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        EGLenum attribute,
                        EGLint* value));
MOCK_METHOD4(QueryStreamu64KHR,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        EGLenum attribute,
                        EGLuint64KHR* value));
MOCK_METHOD2(QueryString, const char*(EGLDisplay dpy, EGLint name));
MOCK_METHOD3(QueryStringiANGLE,
             const char*(EGLDisplay dpy, EGLint name, EGLint index));
MOCK_METHOD4(QuerySurface,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint attribute,
                        EGLint* value));
MOCK_METHOD4(QuerySurfacePointerANGLE,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint attribute,
                        void** value));
MOCK_METHOD2(ReacquireHighPowerGPUANGLE, void(EGLDisplay dpy, EGLContext ctx));
MOCK_METHOD1(ReleaseExternalContextANGLE, void(EGLDisplay dpy));
MOCK_METHOD2(ReleaseHighPowerGPUANGLE, void(EGLDisplay dpy, EGLContext ctx));
MOCK_METHOD3(ReleaseTexImage,
             EGLBoolean(EGLDisplay dpy, EGLSurface surface, EGLint buffer));
MOCK_METHOD0(ReleaseThread, EGLBoolean());
MOCK_METHOD3(SetBlobCacheFuncsANDROID,
             void(EGLDisplay dpy,
                  EGLSetBlobFuncANDROID set,
                  EGLGetBlobFuncANDROID get));
MOCK_METHOD1(SetValidationEnabledANGLE, void(EGLBoolean validationState));
MOCK_METHOD4(StreamAttribKHR,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        EGLenum attribute,
                        EGLint value));
MOCK_METHOD2(StreamConsumerAcquireKHR,
             EGLBoolean(EGLDisplay dpy, EGLStreamKHR stream));
MOCK_METHOD3(StreamConsumerGLTextureExternalAttribsNV,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        EGLAttrib* attrib_list));
MOCK_METHOD2(StreamConsumerGLTextureExternalKHR,
             EGLBoolean(EGLDisplay dpy, EGLStreamKHR stream));
MOCK_METHOD2(StreamConsumerReleaseKHR,
             EGLBoolean(EGLDisplay dpy, EGLStreamKHR stream));
MOCK_METHOD4(StreamPostD3DTextureANGLE,
             EGLBoolean(EGLDisplay dpy,
                        EGLStreamKHR stream,
                        void* texture,
                        const EGLAttrib* attrib_list));
MOCK_METHOD4(SurfaceAttrib,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint attribute,
                        EGLint value));
MOCK_METHOD2(SwapBuffers, EGLBoolean(EGLDisplay dpy, EGLSurface surface));
MOCK_METHOD4(SwapBuffersWithDamageKHR,
             EGLBoolean(EGLDisplay dpy,
                        EGLSurface surface,
                        EGLint* rects,
                        EGLint n_rects));
MOCK_METHOD2(SwapInterval, EGLBoolean(EGLDisplay dpy, EGLint interval));
MOCK_METHOD1(Terminate, EGLBoolean(EGLDisplay dpy));
MOCK_METHOD0(WaitClient, EGLBoolean());
MOCK_METHOD0(WaitGL, EGLBoolean());
MOCK_METHOD1(WaitNative, EGLBoolean(EGLint engine));
MOCK_METHOD3(WaitSync, EGLint(EGLDisplay dpy, EGLSync sync, EGLint flags));
MOCK_METHOD3(WaitSyncKHR,
             EGLint(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags));
MOCK_METHOD1(WaitUntilWorkScheduledANGLE, void(EGLDisplay dpy));
