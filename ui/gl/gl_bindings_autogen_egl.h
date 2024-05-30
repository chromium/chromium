// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef UI_GL_GL_BINDINGS_AUTOGEN_EGL_H_
#define UI_GL_GL_BINDINGS_AUTOGEN_EGL_H_

#include <string>

namespace gl {

class GLContext;

typedef void(GL_BINDING_CALL* eglAcquireExternalContextANGLEProc)(
    EGLDisplay dpy,
    EGLSurface readAndDraw);
typedef EGLBoolean(GL_BINDING_CALL* eglBindAPIProc)(EGLenum api);
typedef EGLBoolean(GL_BINDING_CALL* eglBindTexImageProc)(EGLDisplay dpy,
                                                         EGLSurface surface,
                                                         EGLint buffer);
typedef EGLBoolean(GL_BINDING_CALL* eglChooseConfigProc)(
    EGLDisplay dpy,
    const EGLint* attrib_list,
    EGLConfig* configs,
    EGLint config_size,
    EGLint* num_config);
typedef EGLint(GL_BINDING_CALL* eglClientWaitSyncProc)(EGLDisplay dpy,
                                                       EGLSync sync,
                                                       EGLint flags,
                                                       EGLTime timeout);
typedef EGLint(GL_BINDING_CALL* eglClientWaitSyncKHRProc)(EGLDisplay dpy,
                                                          EGLSyncKHR sync,
                                                          EGLint flags,
                                                          EGLTimeKHR timeout);
typedef EGLBoolean(GL_BINDING_CALL* eglCopyBuffersProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLNativePixmapType target);
typedef void*(GL_BINDING_CALL* eglCopyMetalSharedEventANGLEProc)(EGLDisplay dpy,
                                                                 EGLSync sync);
typedef EGLContext(GL_BINDING_CALL* eglCreateContextProc)(
    EGLDisplay dpy,
    EGLConfig config,
    EGLContext share_context,
    const EGLint* attrib_list);
typedef EGLImage(GL_BINDING_CALL* eglCreateImageProc)(
    EGLDisplay dpy,
    EGLContext ctx,
    EGLenum target,
    EGLClientBuffer buffer,
    const EGLAttrib* attrib_list);
typedef EGLImageKHR(GL_BINDING_CALL* eglCreateImageKHRProc)(
    EGLDisplay dpy,
    EGLContext ctx,
    EGLenum target,
    EGLClientBuffer buffer,
    const EGLint* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreatePbufferFromClientBufferProc)(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreatePbufferSurfaceProc)(
    EGLDisplay dpy,
    EGLConfig config,
    const EGLint* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreatePixmapSurfaceProc)(
    EGLDisplay dpy,
    EGLConfig config,
    EGLNativePixmapType pixmap,
    const EGLint* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreatePlatformPixmapSurfaceProc)(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreatePlatformWindowSurfaceProc)(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list);
typedef EGLStreamKHR(GL_BINDING_CALL* eglCreateStreamKHRProc)(
    EGLDisplay dpy,
    const EGLint* attrib_list);
typedef EGLBoolean(GL_BINDING_CALL* eglCreateStreamProducerD3DTextureANGLEProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list);
typedef EGLSync(GL_BINDING_CALL* eglCreateSyncProc)(
    EGLDisplay dpy,
    EGLenum type,
    const EGLAttrib* attrib_list);
typedef EGLSyncKHR(GL_BINDING_CALL* eglCreateSyncKHRProc)(
    EGLDisplay dpy,
    EGLenum type,
    const EGLint* attrib_list);
typedef EGLSurface(GL_BINDING_CALL* eglCreateWindowSurfaceProc)(
    EGLDisplay dpy,
    EGLConfig config,
    EGLNativeWindowType win,
    const EGLint* attrib_list);
typedef EGLint(GL_BINDING_CALL* eglDebugMessageControlKHRProc)(
    EGLDEBUGPROCKHR callback,
    const EGLAttrib* attrib_list);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroyContextProc)(EGLDisplay dpy,
                                                           EGLContext ctx);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroyImageProc)(EGLDisplay dpy,
                                                         EGLImage image);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroyImageKHRProc)(EGLDisplay dpy,
                                                            EGLImageKHR image);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroyStreamKHRProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroySurfaceProc)(EGLDisplay dpy,
                                                           EGLSurface surface);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroySyncProc)(EGLDisplay dpy,
                                                        EGLSync sync);
typedef EGLBoolean(GL_BINDING_CALL* eglDestroySyncKHRProc)(EGLDisplay dpy,
                                                           EGLSyncKHR sync);
typedef EGLint(GL_BINDING_CALL* eglDupNativeFenceFDANDROIDProc)(
    EGLDisplay dpy,
    EGLSyncKHR sync);
typedef EGLBoolean(GL_BINDING_CALL* eglExportDMABUFImageMESAProc)(
    EGLDisplay dpy,
    EGLImageKHR image,
    int* fds,
    EGLint* strides,
    EGLint* offsets);
typedef EGLBoolean(GL_BINDING_CALL* eglExportDMABUFImageQueryMESAProc)(
    EGLDisplay dpy,
    EGLImageKHR image,
    int* fourcc,
    int* num_planes,
    EGLuint64KHR* modifiers);
typedef EGLBoolean(GL_BINDING_CALL* eglExportVkImageANGLEProc)(
    EGLDisplay dpy,
    EGLImageKHR image,
    void* vk_image,
    void* vk_image_create_info);
typedef EGLBoolean(GL_BINDING_CALL* eglGetCompositorTimingANDROIDProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint numTimestamps,
    EGLint* names,
    EGLnsecsANDROID* values);
typedef EGLBoolean(GL_BINDING_CALL* eglGetCompositorTimingSupportedANDROIDProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp);
typedef EGLBoolean(GL_BINDING_CALL* eglGetConfigAttribProc)(EGLDisplay dpy,
                                                            EGLConfig config,
                                                            EGLint attribute,
                                                            EGLint* value);
typedef EGLBoolean(GL_BINDING_CALL* eglGetConfigsProc)(EGLDisplay dpy,
                                                       EGLConfig* configs,
                                                       EGLint config_size,
                                                       EGLint* num_config);
typedef EGLContext(GL_BINDING_CALL* eglGetCurrentContextProc)(void);
typedef EGLDisplay(GL_BINDING_CALL* eglGetCurrentDisplayProc)(void);
typedef EGLSurface(GL_BINDING_CALL* eglGetCurrentSurfaceProc)(EGLint readdraw);
typedef EGLDisplay(GL_BINDING_CALL* eglGetDisplayProc)(
    EGLNativeDisplayType display_id);
typedef EGLint(GL_BINDING_CALL* eglGetErrorProc)(void);
typedef EGLBoolean(GL_BINDING_CALL* eglGetFrameTimestampsANDROIDProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLuint64KHR frameId,
    EGLint numTimestamps,
    EGLint* timestamps,
    EGLnsecsANDROID* values);
typedef EGLBoolean(GL_BINDING_CALL* eglGetFrameTimestampSupportedANDROIDProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp);
typedef EGLBoolean(GL_BINDING_CALL* eglGetMscRateANGLEProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint* numerator,
    EGLint* denominator);
typedef EGLClientBuffer(GL_BINDING_CALL* eglGetNativeClientBufferANDROIDProc)(
    const struct AHardwareBuffer* ahardwarebuffer);
typedef EGLBoolean(GL_BINDING_CALL* eglGetNextFrameIdANDROIDProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLuint64KHR* frameId);
typedef EGLDisplay(GL_BINDING_CALL* eglGetPlatformDisplayProc)(
    EGLenum platform,
    void* native_display,
    const EGLAttrib* attrib_list);
typedef __eglMustCastToProperFunctionPointerType(
    GL_BINDING_CALL* eglGetProcAddressProc)(const char* procname);
typedef EGLBoolean(GL_BINDING_CALL* eglGetSyncAttribProc)(EGLDisplay dpy,
                                                          EGLSync sync,
                                                          EGLint attribute,
                                                          EGLAttrib* value);
typedef EGLBoolean(GL_BINDING_CALL* eglGetSyncAttribKHRProc)(EGLDisplay dpy,
                                                             EGLSyncKHR sync,
                                                             EGLint attribute,
                                                             EGLint* value);
typedef EGLBoolean(GL_BINDING_CALL* eglGetSyncValuesCHROMIUMProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLuint64CHROMIUM* ust,
    EGLuint64CHROMIUM* msc,
    EGLuint64CHROMIUM* sbc);
typedef void(GL_BINDING_CALL* eglHandleGPUSwitchANGLEProc)(EGLDisplay dpy);
typedef EGLBoolean(GL_BINDING_CALL* eglImageFlushExternalEXTProc)(
    EGLDisplay dpy,
    EGLImageKHR image,
    const EGLAttrib* attrib_list);
typedef EGLBoolean(GL_BINDING_CALL* eglInitializeProc)(EGLDisplay dpy,
                                                       EGLint* major,
                                                       EGLint* minor);
typedef EGLint(GL_BINDING_CALL* eglLabelObjectKHRProc)(EGLDisplay display,
                                                       EGLenum objectType,
                                                       EGLObjectKHR object,
                                                       EGLLabelKHR label);
typedef EGLBoolean(GL_BINDING_CALL* eglMakeCurrentProc)(EGLDisplay dpy,
                                                        EGLSurface draw,
                                                        EGLSurface read,
                                                        EGLContext ctx);
typedef EGLBoolean(GL_BINDING_CALL* eglPostSubBufferNVProc)(EGLDisplay dpy,
                                                            EGLSurface surface,
                                                            EGLint x,
                                                            EGLint y,
                                                            EGLint width,
                                                            EGLint height);
typedef EGLenum(GL_BINDING_CALL* eglQueryAPIProc)(void);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryContextProc)(EGLDisplay dpy,
                                                         EGLContext ctx,
                                                         EGLint attribute,
                                                         EGLint* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDebugKHRProc)(EGLint attribute,
                                                          EGLAttrib* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDeviceAttribEXTProc)(
    EGLDeviceEXT device,
    EGLint attribute,
    EGLAttrib* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDevicesEXTProc)(
    EGLint max_devices,
    EGLDeviceEXT* devices,
    EGLint* num_devices);
typedef const char*(GL_BINDING_CALL* eglQueryDeviceStringEXTProc)(
    EGLDeviceEXT device,
    EGLint name);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDisplayAttribANGLEProc)(
    EGLDisplay dpy,
    EGLint attribute,
    EGLAttrib* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDisplayAttribEXTProc)(
    EGLDisplay dpy,
    EGLint attribute,
    EGLAttrib* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDmaBufFormatsEXTProc)(
    EGLDisplay dpy,
    EGLint max_formats,
    EGLint* formats,
    EGLint* num_formats);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryDmaBufModifiersEXTProc)(
    EGLDisplay dpy,
    EGLint format,
    EGLint max_modifiers,
    EGLuint64KHR* modifiers,
    EGLBoolean* external_only,
    EGLint* num_modifiers);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryStreamKHRProc)(EGLDisplay dpy,
                                                           EGLStreamKHR stream,
                                                           EGLenum attribute,
                                                           EGLint* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQueryStreamu64KHRProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLenum attribute,
    EGLuint64KHR* value);
typedef const char*(GL_BINDING_CALL* eglQueryStringProc)(EGLDisplay dpy,
                                                         EGLint name);
typedef const char*(GL_BINDING_CALL* eglQueryStringiANGLEProc)(EGLDisplay dpy,
                                                               EGLint name,
                                                               EGLint index);
typedef EGLBoolean(GL_BINDING_CALL* eglQuerySurfaceProc)(EGLDisplay dpy,
                                                         EGLSurface surface,
                                                         EGLint attribute,
                                                         EGLint* value);
typedef EGLBoolean(GL_BINDING_CALL* eglQuerySurfacePointerANGLEProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint attribute,
    void** value);
typedef void(GL_BINDING_CALL* eglReacquireHighPowerGPUANGLEProc)(
    EGLDisplay dpy,
    EGLContext ctx);
typedef void(GL_BINDING_CALL* eglReleaseExternalContextANGLEProc)(
    EGLDisplay dpy);
typedef void(GL_BINDING_CALL* eglReleaseHighPowerGPUANGLEProc)(EGLDisplay dpy,
                                                               EGLContext ctx);
typedef EGLBoolean(GL_BINDING_CALL* eglReleaseTexImageProc)(EGLDisplay dpy,
                                                            EGLSurface surface,
                                                            EGLint buffer);
typedef EGLBoolean(GL_BINDING_CALL* eglReleaseThreadProc)(void);
typedef void(GL_BINDING_CALL* eglSetBlobCacheFuncsANDROIDProc)(
    EGLDisplay dpy,
    EGLSetBlobFuncANDROID set,
    EGLGetBlobFuncANDROID get);
typedef void(GL_BINDING_CALL* eglSetValidationEnabledANGLEProc)(
    EGLBoolean validationState);
typedef EGLBoolean(GL_BINDING_CALL* eglStreamAttribKHRProc)(EGLDisplay dpy,
                                                            EGLStreamKHR stream,
                                                            EGLenum attribute,
                                                            EGLint value);
typedef EGLBoolean(GL_BINDING_CALL* eglStreamConsumerAcquireKHRProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream);
typedef EGLBoolean(
    GL_BINDING_CALL* eglStreamConsumerGLTextureExternalAttribsNVProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list);
typedef EGLBoolean(GL_BINDING_CALL* eglStreamConsumerGLTextureExternalKHRProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream);
typedef EGLBoolean(GL_BINDING_CALL* eglStreamConsumerReleaseKHRProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream);
typedef EGLBoolean(GL_BINDING_CALL* eglStreamPostD3DTextureANGLEProc)(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list);
typedef EGLBoolean(GL_BINDING_CALL* eglSurfaceAttribProc)(EGLDisplay dpy,
                                                          EGLSurface surface,
                                                          EGLint attribute,
                                                          EGLint value);
typedef EGLBoolean(GL_BINDING_CALL* eglSwapBuffersProc)(EGLDisplay dpy,
                                                        EGLSurface surface);
typedef EGLBoolean(GL_BINDING_CALL* eglSwapBuffersWithDamageKHRProc)(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint* rects,
    EGLint n_rects);
typedef EGLBoolean(GL_BINDING_CALL* eglSwapIntervalProc)(EGLDisplay dpy,
                                                         EGLint interval);
typedef EGLBoolean(GL_BINDING_CALL* eglTerminateProc)(EGLDisplay dpy);
typedef EGLBoolean(GL_BINDING_CALL* eglWaitClientProc)(void);
typedef EGLBoolean(GL_BINDING_CALL* eglWaitGLProc)(void);
typedef EGLBoolean(GL_BINDING_CALL* eglWaitNativeProc)(EGLint engine);
typedef EGLint(GL_BINDING_CALL* eglWaitSyncProc)(EGLDisplay dpy,
                                                 EGLSync sync,
                                                 EGLint flags);
typedef EGLint(GL_BINDING_CALL* eglWaitSyncKHRProc)(EGLDisplay dpy,
                                                    EGLSyncKHR sync,
                                                    EGLint flags);
typedef void(GL_BINDING_CALL* eglWaitUntilWorkScheduledANGLEProc)(
    EGLDisplay dpy);

struct GL_EXPORT ClientExtensionsEGL {
  bool b_EGL_ANGLE_display_power_preference;
  bool b_EGL_ANGLE_feature_control;
  bool b_EGL_ANGLE_no_error;
  bool b_EGL_ANGLE_platform_angle;
  bool b_EGL_ANGLE_platform_angle_d3d;
  bool b_EGL_ANGLE_platform_angle_device_id;
  bool b_EGL_ANGLE_platform_angle_device_type_egl_angle;
  bool b_EGL_ANGLE_platform_angle_device_type_swiftshader;
  bool b_EGL_ANGLE_platform_angle_metal;
  bool b_EGL_ANGLE_platform_angle_null;
  bool b_EGL_ANGLE_platform_angle_opengl;
  bool b_EGL_ANGLE_platform_angle_vulkan;
  bool b_EGL_EXT_device_base;
  bool b_EGL_EXT_device_enumeration;
  bool b_EGL_EXT_device_query;
  bool b_EGL_EXT_platform_device;
  bool b_EGL_KHR_debug;
  bool b_EGL_MESA_platform_surfaceless;

  void InitializeClientExtensionSettings();

 private:
  static std::string GetClientExtensions();
};

struct GL_EXPORT DisplayExtensionsEGL {
  bool b_EGL_ANDROID_blob_cache;
  bool b_EGL_ANDROID_create_native_client_buffer;
  bool b_EGL_ANDROID_front_buffer_auto_refresh;
  bool b_EGL_ANDROID_get_frame_timestamps;
  bool b_EGL_ANDROID_get_native_client_buffer;
  bool b_EGL_ANDROID_native_fence_sync;
  bool b_EGL_ANGLE_context_virtualization;
  bool b_EGL_ANGLE_create_context_backwards_compatible;
  bool b_EGL_ANGLE_create_context_client_arrays;
  bool b_EGL_ANGLE_create_context_webgl_compatibility;
  bool b_EGL_ANGLE_d3d_share_handle_client_buffer;
  bool b_EGL_ANGLE_display_semaphore_share_group;
  bool b_EGL_ANGLE_display_texture_share_group;
  bool b_EGL_ANGLE_external_context_and_surface;
  bool b_EGL_ANGLE_global_fence_sync;
  bool b_EGL_ANGLE_iosurface_client_buffer;
  bool b_EGL_ANGLE_keyed_mutex;
  bool b_EGL_ANGLE_metal_shared_event_sync;
  bool b_EGL_ANGLE_no_error;
  bool b_EGL_ANGLE_power_preference;
  bool b_EGL_ANGLE_query_surface_pointer;
  bool b_EGL_ANGLE_robust_resource_initialization;
  bool b_EGL_ANGLE_stream_producer_d3d_texture;
  bool b_EGL_ANGLE_surface_d3d_texture_2d_share_handle;
  bool b_EGL_ANGLE_surface_orientation;
  bool b_EGL_ANGLE_sync_control_rate;
  bool b_EGL_ANGLE_vulkan_image;
  bool b_EGL_ANGLE_wait_until_work_scheduled;
  bool b_EGL_ANGLE_window_fixed_size;
  bool b_EGL_ARM_implicit_external_sync;
  bool b_EGL_CHROMIUM_create_context_bind_generates_resource;
  bool b_EGL_CHROMIUM_sync_control;
  bool b_EGL_EXT_create_context_robustness;
  bool b_EGL_EXT_gl_colorspace_display_p3;
  bool b_EGL_EXT_gl_colorspace_display_p3_passthrough;
  bool b_EGL_EXT_image_dma_buf_import;
  bool b_EGL_EXT_image_dma_buf_import_modifiers;
  bool b_EGL_EXT_image_flush_external;
  bool b_EGL_EXT_pixel_format_float;
  bool b_EGL_IMG_context_priority;
  bool b_EGL_KHR_create_context;
  bool b_EGL_KHR_fence_sync;
  bool b_EGL_KHR_gl_colorspace;
  bool b_EGL_KHR_gl_texture_2D_image;
  bool b_EGL_KHR_image;
  bool b_EGL_KHR_image_base;
  bool b_EGL_KHR_no_config_context;
  bool b_EGL_KHR_stream;
  bool b_EGL_KHR_stream_consumer_gltexture;
  bool b_EGL_KHR_surfaceless_context;
  bool b_EGL_KHR_swap_buffers_with_damage;
  bool b_EGL_KHR_wait_sync;
  bool b_EGL_MESA_image_dma_buf_export;
  bool b_EGL_NOK_texture_from_pixmap;
  bool b_EGL_NV_post_sub_buffer;
  bool b_EGL_NV_robustness_video_memory_purge;
  bool b_EGL_NV_stream_consumer_gltexture_yuv;
  bool b_GL_CHROMIUM_egl_android_native_fence_sync_hack;
  bool b_GL_CHROMIUM_egl_khr_fence_sync_hack;

  void InitializeExtensionSettings(EGLDisplay display);
  void UpdateConditionalExtensionSettings(EGLDisplay display);

  static std::string GetPlatformExtensions(EGLDisplay display);
};

struct ProcsEGL {
  eglAcquireExternalContextANGLEProc eglAcquireExternalContextANGLEFn;
  eglBindAPIProc eglBindAPIFn;
  eglBindTexImageProc eglBindTexImageFn;
  eglChooseConfigProc eglChooseConfigFn;
  eglClientWaitSyncProc eglClientWaitSyncFn;
  eglClientWaitSyncKHRProc eglClientWaitSyncKHRFn;
  eglCopyBuffersProc eglCopyBuffersFn;
  eglCopyMetalSharedEventANGLEProc eglCopyMetalSharedEventANGLEFn;
  eglCreateContextProc eglCreateContextFn;
  eglCreateImageProc eglCreateImageFn;
  eglCreateImageKHRProc eglCreateImageKHRFn;
  eglCreatePbufferFromClientBufferProc eglCreatePbufferFromClientBufferFn;
  eglCreatePbufferSurfaceProc eglCreatePbufferSurfaceFn;
  eglCreatePixmapSurfaceProc eglCreatePixmapSurfaceFn;
  eglCreatePlatformPixmapSurfaceProc eglCreatePlatformPixmapSurfaceFn;
  eglCreatePlatformWindowSurfaceProc eglCreatePlatformWindowSurfaceFn;
  eglCreateStreamKHRProc eglCreateStreamKHRFn;
  eglCreateStreamProducerD3DTextureANGLEProc
      eglCreateStreamProducerD3DTextureANGLEFn;
  eglCreateSyncProc eglCreateSyncFn;
  eglCreateSyncKHRProc eglCreateSyncKHRFn;
  eglCreateWindowSurfaceProc eglCreateWindowSurfaceFn;
  eglDebugMessageControlKHRProc eglDebugMessageControlKHRFn;
  eglDestroyContextProc eglDestroyContextFn;
  eglDestroyImageProc eglDestroyImageFn;
  eglDestroyImageKHRProc eglDestroyImageKHRFn;
  eglDestroyStreamKHRProc eglDestroyStreamKHRFn;
  eglDestroySurfaceProc eglDestroySurfaceFn;
  eglDestroySyncProc eglDestroySyncFn;
  eglDestroySyncKHRProc eglDestroySyncKHRFn;
  eglDupNativeFenceFDANDROIDProc eglDupNativeFenceFDANDROIDFn;
  eglExportDMABUFImageMESAProc eglExportDMABUFImageMESAFn;
  eglExportDMABUFImageQueryMESAProc eglExportDMABUFImageQueryMESAFn;
  eglExportVkImageANGLEProc eglExportVkImageANGLEFn;
  eglGetCompositorTimingANDROIDProc eglGetCompositorTimingANDROIDFn;
  eglGetCompositorTimingSupportedANDROIDProc
      eglGetCompositorTimingSupportedANDROIDFn;
  eglGetConfigAttribProc eglGetConfigAttribFn;
  eglGetConfigsProc eglGetConfigsFn;
  eglGetCurrentContextProc eglGetCurrentContextFn;
  eglGetCurrentDisplayProc eglGetCurrentDisplayFn;
  eglGetCurrentSurfaceProc eglGetCurrentSurfaceFn;
  eglGetDisplayProc eglGetDisplayFn;
  eglGetErrorProc eglGetErrorFn;
  eglGetFrameTimestampsANDROIDProc eglGetFrameTimestampsANDROIDFn;
  eglGetFrameTimestampSupportedANDROIDProc
      eglGetFrameTimestampSupportedANDROIDFn;
  eglGetMscRateANGLEProc eglGetMscRateANGLEFn;
  eglGetNativeClientBufferANDROIDProc eglGetNativeClientBufferANDROIDFn;
  eglGetNextFrameIdANDROIDProc eglGetNextFrameIdANDROIDFn;
  eglGetPlatformDisplayProc eglGetPlatformDisplayFn;
  eglGetProcAddressProc eglGetProcAddressFn;
  eglGetSyncAttribProc eglGetSyncAttribFn;
  eglGetSyncAttribKHRProc eglGetSyncAttribKHRFn;
  eglGetSyncValuesCHROMIUMProc eglGetSyncValuesCHROMIUMFn;
  eglHandleGPUSwitchANGLEProc eglHandleGPUSwitchANGLEFn;
  eglImageFlushExternalEXTProc eglImageFlushExternalEXTFn;
  eglInitializeProc eglInitializeFn;
  eglLabelObjectKHRProc eglLabelObjectKHRFn;
  eglMakeCurrentProc eglMakeCurrentFn;
  eglPostSubBufferNVProc eglPostSubBufferNVFn;
  eglQueryAPIProc eglQueryAPIFn;
  eglQueryContextProc eglQueryContextFn;
  eglQueryDebugKHRProc eglQueryDebugKHRFn;
  eglQueryDeviceAttribEXTProc eglQueryDeviceAttribEXTFn;
  eglQueryDevicesEXTProc eglQueryDevicesEXTFn;
  eglQueryDeviceStringEXTProc eglQueryDeviceStringEXTFn;
  eglQueryDisplayAttribANGLEProc eglQueryDisplayAttribANGLEFn;
  eglQueryDisplayAttribEXTProc eglQueryDisplayAttribEXTFn;
  eglQueryDmaBufFormatsEXTProc eglQueryDmaBufFormatsEXTFn;
  eglQueryDmaBufModifiersEXTProc eglQueryDmaBufModifiersEXTFn;
  eglQueryStreamKHRProc eglQueryStreamKHRFn;
  eglQueryStreamu64KHRProc eglQueryStreamu64KHRFn;
  eglQueryStringProc eglQueryStringFn;
  eglQueryStringiANGLEProc eglQueryStringiANGLEFn;
  eglQuerySurfaceProc eglQuerySurfaceFn;
  eglQuerySurfacePointerANGLEProc eglQuerySurfacePointerANGLEFn;
  eglReacquireHighPowerGPUANGLEProc eglReacquireHighPowerGPUANGLEFn;
  eglReleaseExternalContextANGLEProc eglReleaseExternalContextANGLEFn;
  eglReleaseHighPowerGPUANGLEProc eglReleaseHighPowerGPUANGLEFn;
  eglReleaseTexImageProc eglReleaseTexImageFn;
  eglReleaseThreadProc eglReleaseThreadFn;
  eglSetBlobCacheFuncsANDROIDProc eglSetBlobCacheFuncsANDROIDFn;
  eglSetValidationEnabledANGLEProc eglSetValidationEnabledANGLEFn;
  eglStreamAttribKHRProc eglStreamAttribKHRFn;
  eglStreamConsumerAcquireKHRProc eglStreamConsumerAcquireKHRFn;
  eglStreamConsumerGLTextureExternalAttribsNVProc
      eglStreamConsumerGLTextureExternalAttribsNVFn;
  eglStreamConsumerGLTextureExternalKHRProc
      eglStreamConsumerGLTextureExternalKHRFn;
  eglStreamConsumerReleaseKHRProc eglStreamConsumerReleaseKHRFn;
  eglStreamPostD3DTextureANGLEProc eglStreamPostD3DTextureANGLEFn;
  eglSurfaceAttribProc eglSurfaceAttribFn;
  eglSwapBuffersProc eglSwapBuffersFn;
  eglSwapBuffersWithDamageKHRProc eglSwapBuffersWithDamageKHRFn;
  eglSwapIntervalProc eglSwapIntervalFn;
  eglTerminateProc eglTerminateFn;
  eglWaitClientProc eglWaitClientFn;
  eglWaitGLProc eglWaitGLFn;
  eglWaitNativeProc eglWaitNativeFn;
  eglWaitSyncProc eglWaitSyncFn;
  eglWaitSyncKHRProc eglWaitSyncKHRFn;
  eglWaitUntilWorkScheduledANGLEProc eglWaitUntilWorkScheduledANGLEFn;
};

class GL_EXPORT EGLApi {
 public:
  EGLApi();
  virtual ~EGLApi();

  virtual void SetDisabledExtensions(const std::string& disabled_extensions) {}

  virtual void eglAcquireExternalContextANGLEFn(EGLDisplay dpy,
                                                EGLSurface readAndDraw) = 0;
  virtual EGLBoolean eglBindAPIFn(EGLenum api) = 0;
  virtual EGLBoolean eglBindTexImageFn(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint buffer) = 0;
  virtual EGLBoolean eglChooseConfigFn(EGLDisplay dpy,
                                       const EGLint* attrib_list,
                                       EGLConfig* configs,
                                       EGLint config_size,
                                       EGLint* num_config) = 0;
  virtual EGLint eglClientWaitSyncFn(EGLDisplay dpy,
                                     EGLSync sync,
                                     EGLint flags,
                                     EGLTime timeout) = 0;
  virtual EGLint eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                        EGLSyncKHR sync,
                                        EGLint flags,
                                        EGLTimeKHR timeout) = 0;
  virtual EGLBoolean eglCopyBuffersFn(EGLDisplay dpy,
                                      EGLSurface surface,
                                      EGLNativePixmapType target) = 0;
  virtual void* eglCopyMetalSharedEventANGLEFn(EGLDisplay dpy,
                                               EGLSync sync) = 0;
  virtual EGLContext eglCreateContextFn(EGLDisplay dpy,
                                        EGLConfig config,
                                        EGLContext share_context,
                                        const EGLint* attrib_list) = 0;
  virtual EGLImage eglCreateImageFn(EGLDisplay dpy,
                                    EGLContext ctx,
                                    EGLenum target,
                                    EGLClientBuffer buffer,
                                    const EGLAttrib* attrib_list) = 0;
  virtual EGLImageKHR eglCreateImageKHRFn(EGLDisplay dpy,
                                          EGLContext ctx,
                                          EGLenum target,
                                          EGLClientBuffer buffer,
                                          const EGLint* attrib_list) = 0;
  virtual EGLSurface eglCreatePbufferFromClientBufferFn(
      EGLDisplay dpy,
      EGLenum buftype,
      void* buffer,
      EGLConfig config,
      const EGLint* attrib_list) = 0;
  virtual EGLSurface eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                               EGLConfig config,
                                               const EGLint* attrib_list) = 0;
  virtual EGLSurface eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                              EGLConfig config,
                                              EGLNativePixmapType pixmap,
                                              const EGLint* attrib_list) = 0;
  virtual EGLSurface eglCreatePlatformPixmapSurfaceFn(
      EGLDisplay dpy,
      EGLConfig config,
      void* native_pixmap,
      const EGLAttrib* attrib_list) = 0;
  virtual EGLSurface eglCreatePlatformWindowSurfaceFn(
      EGLDisplay dpy,
      EGLConfig config,
      void* native_window,
      const EGLAttrib* attrib_list) = 0;
  virtual EGLStreamKHR eglCreateStreamKHRFn(EGLDisplay dpy,
                                            const EGLint* attrib_list) = 0;
  virtual EGLBoolean eglCreateStreamProducerD3DTextureANGLEFn(
      EGLDisplay dpy,
      EGLStreamKHR stream,
      EGLAttrib* attrib_list) = 0;
  virtual EGLSync eglCreateSyncFn(EGLDisplay dpy,
                                  EGLenum type,
                                  const EGLAttrib* attrib_list) = 0;
  virtual EGLSyncKHR eglCreateSyncKHRFn(EGLDisplay dpy,
                                        EGLenum type,
                                        const EGLint* attrib_list) = 0;
  virtual EGLSurface eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                              EGLConfig config,
                                              EGLNativeWindowType win,
                                              const EGLint* attrib_list) = 0;
  virtual EGLint eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                             const EGLAttrib* attrib_list) = 0;
  virtual EGLBoolean eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) = 0;
  virtual EGLBoolean eglDestroyImageFn(EGLDisplay dpy, EGLImage image) = 0;
  virtual EGLBoolean eglDestroyImageKHRFn(EGLDisplay dpy,
                                          EGLImageKHR image) = 0;
  virtual EGLBoolean eglDestroyStreamKHRFn(EGLDisplay dpy,
                                           EGLStreamKHR stream) = 0;
  virtual EGLBoolean eglDestroySurfaceFn(EGLDisplay dpy,
                                         EGLSurface surface) = 0;
  virtual EGLBoolean eglDestroySyncFn(EGLDisplay dpy, EGLSync sync) = 0;
  virtual EGLBoolean eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) = 0;
  virtual EGLint eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                              EGLSyncKHR sync) = 0;
  virtual EGLBoolean eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                                EGLImageKHR image,
                                                int* fds,
                                                EGLint* strides,
                                                EGLint* offsets) = 0;
  virtual EGLBoolean eglExportDMABUFImageQueryMESAFn(
      EGLDisplay dpy,
      EGLImageKHR image,
      int* fourcc,
      int* num_planes,
      EGLuint64KHR* modifiers) = 0;
  virtual EGLBoolean eglExportVkImageANGLEFn(EGLDisplay dpy,
                                             EGLImageKHR image,
                                             void* vk_image,
                                             void* vk_image_create_info) = 0;
  virtual EGLBoolean eglGetCompositorTimingANDROIDFn(
      EGLDisplay dpy,
      EGLSurface surface,
      EGLint numTimestamps,
      EGLint* names,
      EGLnsecsANDROID* values) = 0;
  virtual EGLBoolean eglGetCompositorTimingSupportedANDROIDFn(
      EGLDisplay dpy,
      EGLSurface surface,
      EGLint timestamp) = 0;
  virtual EGLBoolean eglGetConfigAttribFn(EGLDisplay dpy,
                                          EGLConfig config,
                                          EGLint attribute,
                                          EGLint* value) = 0;
  virtual EGLBoolean eglGetConfigsFn(EGLDisplay dpy,
                                     EGLConfig* configs,
                                     EGLint config_size,
                                     EGLint* num_config) = 0;
  virtual EGLContext eglGetCurrentContextFn(void) = 0;
  virtual EGLDisplay eglGetCurrentDisplayFn(void) = 0;
  virtual EGLSurface eglGetCurrentSurfaceFn(EGLint readdraw) = 0;
  virtual EGLDisplay eglGetDisplayFn(EGLNativeDisplayType display_id) = 0;
  virtual EGLint eglGetErrorFn(void) = 0;
  virtual EGLBoolean eglGetFrameTimestampsANDROIDFn(
      EGLDisplay dpy,
      EGLSurface surface,
      EGLuint64KHR frameId,
      EGLint numTimestamps,
      EGLint* timestamps,
      EGLnsecsANDROID* values) = 0;
  virtual EGLBoolean eglGetFrameTimestampSupportedANDROIDFn(
      EGLDisplay dpy,
      EGLSurface surface,
      EGLint timestamp) = 0;
  virtual EGLBoolean eglGetMscRateANGLEFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint* numerator,
                                          EGLint* denominator) = 0;
  virtual EGLClientBuffer eglGetNativeClientBufferANDROIDFn(
      const struct AHardwareBuffer* ahardwarebuffer) = 0;
  virtual EGLBoolean eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLuint64KHR* frameId) = 0;
  virtual EGLDisplay eglGetPlatformDisplayFn(EGLenum platform,
                                             void* native_display,
                                             const EGLAttrib* attrib_list) = 0;
  virtual __eglMustCastToProperFunctionPointerType eglGetProcAddressFn(
      const char* procname) = 0;
  virtual EGLBoolean eglGetSyncAttribFn(EGLDisplay dpy,
                                        EGLSync sync,
                                        EGLint attribute,
                                        EGLAttrib* value) = 0;
  virtual EGLBoolean eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                           EGLSyncKHR sync,
                                           EGLint attribute,
                                           EGLint* value) = 0;
  virtual EGLBoolean eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLuint64CHROMIUM* ust,
                                                EGLuint64CHROMIUM* msc,
                                                EGLuint64CHROMIUM* sbc) = 0;
  virtual void eglHandleGPUSwitchANGLEFn(EGLDisplay dpy) = 0;
  virtual EGLBoolean eglImageFlushExternalEXTFn(
      EGLDisplay dpy,
      EGLImageKHR image,
      const EGLAttrib* attrib_list) = 0;
  virtual EGLBoolean eglInitializeFn(EGLDisplay dpy,
                                     EGLint* major,
                                     EGLint* minor) = 0;
  virtual EGLint eglLabelObjectKHRFn(EGLDisplay display,
                                     EGLenum objectType,
                                     EGLObjectKHR object,
                                     EGLLabelKHR label) = 0;
  virtual EGLBoolean eglMakeCurrentFn(EGLDisplay dpy,
                                      EGLSurface draw,
                                      EGLSurface read,
                                      EGLContext ctx) = 0;
  virtual EGLBoolean eglPostSubBufferNVFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint x,
                                          EGLint y,
                                          EGLint width,
                                          EGLint height) = 0;
  virtual EGLenum eglQueryAPIFn(void) = 0;
  virtual EGLBoolean eglQueryContextFn(EGLDisplay dpy,
                                       EGLContext ctx,
                                       EGLint attribute,
                                       EGLint* value) = 0;
  virtual EGLBoolean eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) = 0;
  virtual EGLBoolean eglQueryDeviceAttribEXTFn(EGLDeviceEXT device,
                                               EGLint attribute,
                                               EGLAttrib* value) = 0;
  virtual EGLBoolean eglQueryDevicesEXTFn(EGLint max_devices,
                                          EGLDeviceEXT* devices,
                                          EGLint* num_devices) = 0;
  virtual const char* eglQueryDeviceStringEXTFn(EGLDeviceEXT device,
                                                EGLint name) = 0;
  virtual EGLBoolean eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                  EGLint attribute,
                                                  EGLAttrib* value) = 0;
  virtual EGLBoolean eglQueryDisplayAttribEXTFn(EGLDisplay dpy,
                                                EGLint attribute,
                                                EGLAttrib* value) = 0;
  virtual EGLBoolean eglQueryDmaBufFormatsEXTFn(EGLDisplay dpy,
                                                EGLint max_formats,
                                                EGLint* formats,
                                                EGLint* num_formats) = 0;
  virtual EGLBoolean eglQueryDmaBufModifiersEXTFn(EGLDisplay dpy,
                                                  EGLint format,
                                                  EGLint max_modifiers,
                                                  EGLuint64KHR* modifiers,
                                                  EGLBoolean* external_only,
                                                  EGLint* num_modifiers) = 0;
  virtual EGLBoolean eglQueryStreamKHRFn(EGLDisplay dpy,
                                         EGLStreamKHR stream,
                                         EGLenum attribute,
                                         EGLint* value) = 0;
  virtual EGLBoolean eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLuint64KHR* value) = 0;
  virtual const char* eglQueryStringFn(EGLDisplay dpy, EGLint name) = 0;
  virtual const char* eglQueryStringiANGLEFn(EGLDisplay dpy,
                                             EGLint name,
                                             EGLint index) = 0;
  virtual EGLBoolean eglQuerySurfaceFn(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint attribute,
                                       EGLint* value) = 0;
  virtual EGLBoolean eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLint attribute,
                                                   void** value) = 0;
  virtual void eglReacquireHighPowerGPUANGLEFn(EGLDisplay dpy,
                                               EGLContext ctx) = 0;
  virtual void eglReleaseExternalContextANGLEFn(EGLDisplay dpy) = 0;
  virtual void eglReleaseHighPowerGPUANGLEFn(EGLDisplay dpy,
                                             EGLContext ctx) = 0;
  virtual EGLBoolean eglReleaseTexImageFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint buffer) = 0;
  virtual EGLBoolean eglReleaseThreadFn(void) = 0;
  virtual void eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                             EGLSetBlobFuncANDROID set,
                                             EGLGetBlobFuncANDROID get) = 0;
  virtual void eglSetValidationEnabledANGLEFn(EGLBoolean validationState) = 0;
  virtual EGLBoolean eglStreamAttribKHRFn(EGLDisplay dpy,
                                          EGLStreamKHR stream,
                                          EGLenum attribute,
                                          EGLint value) = 0;
  virtual EGLBoolean eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                   EGLStreamKHR stream) = 0;
  virtual EGLBoolean eglStreamConsumerGLTextureExternalAttribsNVFn(
      EGLDisplay dpy,
      EGLStreamKHR stream,
      EGLAttrib* attrib_list) = 0;
  virtual EGLBoolean eglStreamConsumerGLTextureExternalKHRFn(
      EGLDisplay dpy,
      EGLStreamKHR stream) = 0;
  virtual EGLBoolean eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                   EGLStreamKHR stream) = 0;
  virtual EGLBoolean eglStreamPostD3DTextureANGLEFn(
      EGLDisplay dpy,
      EGLStreamKHR stream,
      void* texture,
      const EGLAttrib* attrib_list) = 0;
  virtual EGLBoolean eglSurfaceAttribFn(EGLDisplay dpy,
                                        EGLSurface surface,
                                        EGLint attribute,
                                        EGLint value) = 0;
  virtual EGLBoolean eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) = 0;
  virtual EGLBoolean eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLint* rects,
                                                   EGLint n_rects) = 0;
  virtual EGLBoolean eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) = 0;
  virtual EGLBoolean eglTerminateFn(EGLDisplay dpy) = 0;
  virtual EGLBoolean eglWaitClientFn(void) = 0;
  virtual EGLBoolean eglWaitGLFn(void) = 0;
  virtual EGLBoolean eglWaitNativeFn(EGLint engine) = 0;
  virtual EGLint eglWaitSyncFn(EGLDisplay dpy, EGLSync sync, EGLint flags) = 0;
  virtual EGLint eglWaitSyncKHRFn(EGLDisplay dpy,
                                  EGLSyncKHR sync,
                                  EGLint flags) = 0;
  virtual void eglWaitUntilWorkScheduledANGLEFn(EGLDisplay dpy) = 0;
};

}  // namespace gl

#define eglAcquireExternalContextANGLE \
  ::gl::g_current_egl_context->eglAcquireExternalContextANGLEFn
#define eglBindAPI ::gl::g_current_egl_context->eglBindAPIFn
#define eglBindTexImage ::gl::g_current_egl_context->eglBindTexImageFn
#define eglChooseConfig ::gl::g_current_egl_context->eglChooseConfigFn
#define eglClientWaitSync ::gl::g_current_egl_context->eglClientWaitSyncFn
#define eglClientWaitSyncKHR ::gl::g_current_egl_context->eglClientWaitSyncKHRFn
#define eglCopyBuffers ::gl::g_current_egl_context->eglCopyBuffersFn
#define eglCopyMetalSharedEventANGLE \
  ::gl::g_current_egl_context->eglCopyMetalSharedEventANGLEFn
#define eglCreateContext ::gl::g_current_egl_context->eglCreateContextFn
#define eglCreateImage ::gl::g_current_egl_context->eglCreateImageFn
#define eglCreateImageKHR ::gl::g_current_egl_context->eglCreateImageKHRFn
#define eglCreatePbufferFromClientBuffer \
  ::gl::g_current_egl_context->eglCreatePbufferFromClientBufferFn
#define eglCreatePbufferSurface \
  ::gl::g_current_egl_context->eglCreatePbufferSurfaceFn
#define eglCreatePixmapSurface \
  ::gl::g_current_egl_context->eglCreatePixmapSurfaceFn
#define eglCreatePlatformPixmapSurface \
  ::gl::g_current_egl_context->eglCreatePlatformPixmapSurfaceFn
#define eglCreatePlatformWindowSurface \
  ::gl::g_current_egl_context->eglCreatePlatformWindowSurfaceFn
#define eglCreateStreamKHR ::gl::g_current_egl_context->eglCreateStreamKHRFn
#define eglCreateStreamProducerD3DTextureANGLE \
  ::gl::g_current_egl_context->eglCreateStreamProducerD3DTextureANGLEFn
#define eglCreateSync ::gl::g_current_egl_context->eglCreateSyncFn
#define eglCreateSyncKHR ::gl::g_current_egl_context->eglCreateSyncKHRFn
#define eglCreateWindowSurface \
  ::gl::g_current_egl_context->eglCreateWindowSurfaceFn
#define eglDebugMessageControlKHR \
  ::gl::g_current_egl_context->eglDebugMessageControlKHRFn
#define eglDestroyContext ::gl::g_current_egl_context->eglDestroyContextFn
#define eglDestroyImage ::gl::g_current_egl_context->eglDestroyImageFn
#define eglDestroyImageKHR ::gl::g_current_egl_context->eglDestroyImageKHRFn
#define eglDestroyStreamKHR ::gl::g_current_egl_context->eglDestroyStreamKHRFn
#define eglDestroySurface ::gl::g_current_egl_context->eglDestroySurfaceFn
#define eglDestroySync ::gl::g_current_egl_context->eglDestroySyncFn
#define eglDestroySyncKHR ::gl::g_current_egl_context->eglDestroySyncKHRFn
#define eglDupNativeFenceFDANDROID \
  ::gl::g_current_egl_context->eglDupNativeFenceFDANDROIDFn
#define eglExportDMABUFImageMESA \
  ::gl::g_current_egl_context->eglExportDMABUFImageMESAFn
#define eglExportDMABUFImageQueryMESA \
  ::gl::g_current_egl_context->eglExportDMABUFImageQueryMESAFn
#define eglExportVkImageANGLE \
  ::gl::g_current_egl_context->eglExportVkImageANGLEFn
#define eglGetCompositorTimingANDROID \
  ::gl::g_current_egl_context->eglGetCompositorTimingANDROIDFn
#define eglGetCompositorTimingSupportedANDROID \
  ::gl::g_current_egl_context->eglGetCompositorTimingSupportedANDROIDFn
#define eglGetConfigAttrib ::gl::g_current_egl_context->eglGetConfigAttribFn
#define eglGetConfigs ::gl::g_current_egl_context->eglGetConfigsFn
#define eglGetCurrentContext ::gl::g_current_egl_context->eglGetCurrentContextFn
#define eglGetCurrentDisplay ::gl::g_current_egl_context->eglGetCurrentDisplayFn
#define eglGetCurrentSurface ::gl::g_current_egl_context->eglGetCurrentSurfaceFn
#define eglGetDisplay ::gl::g_current_egl_context->eglGetDisplayFn
#define eglGetError ::gl::g_current_egl_context->eglGetErrorFn
#define eglGetFrameTimestampsANDROID \
  ::gl::g_current_egl_context->eglGetFrameTimestampsANDROIDFn
#define eglGetFrameTimestampSupportedANDROID \
  ::gl::g_current_egl_context->eglGetFrameTimestampSupportedANDROIDFn
#define eglGetMscRateANGLE ::gl::g_current_egl_context->eglGetMscRateANGLEFn
#define eglGetNativeClientBufferANDROID \
  ::gl::g_current_egl_context->eglGetNativeClientBufferANDROIDFn
#define eglGetNextFrameIdANDROID \
  ::gl::g_current_egl_context->eglGetNextFrameIdANDROIDFn
#define eglGetPlatformDisplay \
  ::gl::g_current_egl_context->eglGetPlatformDisplayFn
#define eglGetProcAddress ::gl::g_current_egl_context->eglGetProcAddressFn
#define eglGetSyncAttrib ::gl::g_current_egl_context->eglGetSyncAttribFn
#define eglGetSyncAttribKHR ::gl::g_current_egl_context->eglGetSyncAttribKHRFn
#define eglGetSyncValuesCHROMIUM \
  ::gl::g_current_egl_context->eglGetSyncValuesCHROMIUMFn
#define eglHandleGPUSwitchANGLE \
  ::gl::g_current_egl_context->eglHandleGPUSwitchANGLEFn
#define eglImageFlushExternalEXT \
  ::gl::g_current_egl_context->eglImageFlushExternalEXTFn
#define eglInitialize ::gl::g_current_egl_context->eglInitializeFn
#define eglLabelObjectKHR ::gl::g_current_egl_context->eglLabelObjectKHRFn
#define eglMakeCurrent ::gl::g_current_egl_context->eglMakeCurrentFn
#define eglPostSubBufferNV ::gl::g_current_egl_context->eglPostSubBufferNVFn
#define eglQueryAPI ::gl::g_current_egl_context->eglQueryAPIFn
#define eglQueryContext ::gl::g_current_egl_context->eglQueryContextFn
#define eglQueryDebugKHR ::gl::g_current_egl_context->eglQueryDebugKHRFn
#define eglQueryDeviceAttribEXT \
  ::gl::g_current_egl_context->eglQueryDeviceAttribEXTFn
#define eglQueryDevicesEXT ::gl::g_current_egl_context->eglQueryDevicesEXTFn
#define eglQueryDeviceStringEXT \
  ::gl::g_current_egl_context->eglQueryDeviceStringEXTFn
#define eglQueryDisplayAttribANGLE \
  ::gl::g_current_egl_context->eglQueryDisplayAttribANGLEFn
#define eglQueryDisplayAttribEXT \
  ::gl::g_current_egl_context->eglQueryDisplayAttribEXTFn
#define eglQueryDmaBufFormatsEXT \
  ::gl::g_current_egl_context->eglQueryDmaBufFormatsEXTFn
#define eglQueryDmaBufModifiersEXT \
  ::gl::g_current_egl_context->eglQueryDmaBufModifiersEXTFn
#define eglQueryStreamKHR ::gl::g_current_egl_context->eglQueryStreamKHRFn
#define eglQueryStreamu64KHR ::gl::g_current_egl_context->eglQueryStreamu64KHRFn
#define eglQueryString ::gl::g_current_egl_context->eglQueryStringFn
#define eglQueryStringiANGLE ::gl::g_current_egl_context->eglQueryStringiANGLEFn
#define eglQuerySurface ::gl::g_current_egl_context->eglQuerySurfaceFn
#define eglQuerySurfacePointerANGLE \
  ::gl::g_current_egl_context->eglQuerySurfacePointerANGLEFn
#define eglReacquireHighPowerGPUANGLE \
  ::gl::g_current_egl_context->eglReacquireHighPowerGPUANGLEFn
#define eglReleaseExternalContextANGLE \
  ::gl::g_current_egl_context->eglReleaseExternalContextANGLEFn
#define eglReleaseHighPowerGPUANGLE \
  ::gl::g_current_egl_context->eglReleaseHighPowerGPUANGLEFn
#define eglReleaseTexImage ::gl::g_current_egl_context->eglReleaseTexImageFn
#define eglReleaseThread ::gl::g_current_egl_context->eglReleaseThreadFn
#define eglSetBlobCacheFuncsANDROID \
  ::gl::g_current_egl_context->eglSetBlobCacheFuncsANDROIDFn
#define eglSetValidationEnabledANGLE \
  ::gl::g_current_egl_context->eglSetValidationEnabledANGLEFn
#define eglStreamAttribKHR ::gl::g_current_egl_context->eglStreamAttribKHRFn
#define eglStreamConsumerAcquireKHR \
  ::gl::g_current_egl_context->eglStreamConsumerAcquireKHRFn
#define eglStreamConsumerGLTextureExternalAttribsNV \
  ::gl::g_current_egl_context->eglStreamConsumerGLTextureExternalAttribsNVFn
#define eglStreamConsumerGLTextureExternalKHR \
  ::gl::g_current_egl_context->eglStreamConsumerGLTextureExternalKHRFn
#define eglStreamConsumerReleaseKHR \
  ::gl::g_current_egl_context->eglStreamConsumerReleaseKHRFn
#define eglStreamPostD3DTextureANGLE \
  ::gl::g_current_egl_context->eglStreamPostD3DTextureANGLEFn
#define eglSurfaceAttrib ::gl::g_current_egl_context->eglSurfaceAttribFn
#define eglSwapBuffers ::gl::g_current_egl_context->eglSwapBuffersFn
#define eglSwapBuffersWithDamageKHR \
  ::gl::g_current_egl_context->eglSwapBuffersWithDamageKHRFn
#define eglSwapInterval ::gl::g_current_egl_context->eglSwapIntervalFn
#define eglTerminate ::gl::g_current_egl_context->eglTerminateFn
#define eglWaitClient ::gl::g_current_egl_context->eglWaitClientFn
#define eglWaitGL ::gl::g_current_egl_context->eglWaitGLFn
#define eglWaitNative ::gl::g_current_egl_context->eglWaitNativeFn
#define eglWaitSync ::gl::g_current_egl_context->eglWaitSyncFn
#define eglWaitSyncKHR ::gl::g_current_egl_context->eglWaitSyncKHRFn
#define eglWaitUntilWorkScheduledANGLE \
  ::gl::g_current_egl_context->eglWaitUntilWorkScheduledANGLEFn

#endif  // UI_GL_GL_BINDINGS_AUTOGEN_EGL_H_
