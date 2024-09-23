// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string>

#include "base/containers/span.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

DriverEGL g_driver_egl;  // Exists in .bss

void DriverEGL::InitializeStaticBindings() {
#if DCHECK_IS_ON()
  // Ensure struct has been zero-initialized.
  auto bytes = base::byte_span_from_ref(*this);
  for (auto byte : bytes) {
    DCHECK_EQ(0, byte);
  };
#endif

  fn.eglAcquireExternalContextANGLEFn =
      reinterpret_cast<eglAcquireExternalContextANGLEProc>(
          GetGLProcAddress("eglAcquireExternalContextANGLE"));
  fn.eglBindAPIFn =
      reinterpret_cast<eglBindAPIProc>(GetGLProcAddress("eglBindAPI"));
  fn.eglBindTexImageFn = reinterpret_cast<eglBindTexImageProc>(
      GetGLProcAddress("eglBindTexImage"));
  fn.eglChooseConfigFn = reinterpret_cast<eglChooseConfigProc>(
      GetGLProcAddress("eglChooseConfig"));
  fn.eglClientWaitSyncFn = reinterpret_cast<eglClientWaitSyncProc>(
      GetGLProcAddress("eglClientWaitSync"));
  fn.eglClientWaitSyncKHRFn = reinterpret_cast<eglClientWaitSyncKHRProc>(
      GetGLProcAddress("eglClientWaitSyncKHR"));
  fn.eglCopyBuffersFn =
      reinterpret_cast<eglCopyBuffersProc>(GetGLProcAddress("eglCopyBuffers"));
  fn.eglCopyMetalSharedEventANGLEFn =
      reinterpret_cast<eglCopyMetalSharedEventANGLEProc>(
          GetGLProcAddress("eglCopyMetalSharedEventANGLE"));
  fn.eglCreateContextFn = reinterpret_cast<eglCreateContextProc>(
      GetGLProcAddress("eglCreateContext"));
  fn.eglCreateImageFn =
      reinterpret_cast<eglCreateImageProc>(GetGLProcAddress("eglCreateImage"));
  fn.eglCreateImageKHRFn = reinterpret_cast<eglCreateImageKHRProc>(
      GetGLProcAddress("eglCreateImageKHR"));
  fn.eglCreatePbufferFromClientBufferFn =
      reinterpret_cast<eglCreatePbufferFromClientBufferProc>(
          GetGLProcAddress("eglCreatePbufferFromClientBuffer"));
  fn.eglCreatePbufferSurfaceFn = reinterpret_cast<eglCreatePbufferSurfaceProc>(
      GetGLProcAddress("eglCreatePbufferSurface"));
  fn.eglCreatePixmapSurfaceFn = reinterpret_cast<eglCreatePixmapSurfaceProc>(
      GetGLProcAddress("eglCreatePixmapSurface"));
  fn.eglCreatePlatformPixmapSurfaceFn =
      reinterpret_cast<eglCreatePlatformPixmapSurfaceProc>(
          GetGLProcAddress("eglCreatePlatformPixmapSurface"));
  fn.eglCreatePlatformWindowSurfaceFn =
      reinterpret_cast<eglCreatePlatformWindowSurfaceProc>(
          GetGLProcAddress("eglCreatePlatformWindowSurface"));
  fn.eglCreateStreamKHRFn = reinterpret_cast<eglCreateStreamKHRProc>(
      GetGLProcAddress("eglCreateStreamKHR"));
  fn.eglCreateStreamProducerD3DTextureANGLEFn =
      reinterpret_cast<eglCreateStreamProducerD3DTextureANGLEProc>(
          GetGLProcAddress("eglCreateStreamProducerD3DTextureANGLE"));
  fn.eglCreateSyncFn =
      reinterpret_cast<eglCreateSyncProc>(GetGLProcAddress("eglCreateSync"));
  fn.eglCreateSyncKHRFn = reinterpret_cast<eglCreateSyncKHRProc>(
      GetGLProcAddress("eglCreateSyncKHR"));
  fn.eglCreateWindowSurfaceFn = reinterpret_cast<eglCreateWindowSurfaceProc>(
      GetGLProcAddress("eglCreateWindowSurface"));
  fn.eglDebugMessageControlKHRFn =
      reinterpret_cast<eglDebugMessageControlKHRProc>(
          GetGLProcAddress("eglDebugMessageControlKHR"));
  fn.eglDestroyContextFn = reinterpret_cast<eglDestroyContextProc>(
      GetGLProcAddress("eglDestroyContext"));
  fn.eglDestroyImageFn = reinterpret_cast<eglDestroyImageProc>(
      GetGLProcAddress("eglDestroyImage"));
  fn.eglDestroyImageKHRFn = reinterpret_cast<eglDestroyImageKHRProc>(
      GetGLProcAddress("eglDestroyImageKHR"));
  fn.eglDestroyStreamKHRFn = reinterpret_cast<eglDestroyStreamKHRProc>(
      GetGLProcAddress("eglDestroyStreamKHR"));
  fn.eglDestroySurfaceFn = reinterpret_cast<eglDestroySurfaceProc>(
      GetGLProcAddress("eglDestroySurface"));
  fn.eglDestroySyncFn =
      reinterpret_cast<eglDestroySyncProc>(GetGLProcAddress("eglDestroySync"));
  fn.eglDestroySyncKHRFn = reinterpret_cast<eglDestroySyncKHRProc>(
      GetGLProcAddress("eglDestroySyncKHR"));
  fn.eglDupNativeFenceFDANDROIDFn =
      reinterpret_cast<eglDupNativeFenceFDANDROIDProc>(
          GetGLProcAddress("eglDupNativeFenceFDANDROID"));
  fn.eglExportDMABUFImageMESAFn =
      reinterpret_cast<eglExportDMABUFImageMESAProc>(
          GetGLProcAddress("eglExportDMABUFImageMESA"));
  fn.eglExportDMABUFImageQueryMESAFn =
      reinterpret_cast<eglExportDMABUFImageQueryMESAProc>(
          GetGLProcAddress("eglExportDMABUFImageQueryMESA"));
  fn.eglExportVkImageANGLEFn = reinterpret_cast<eglExportVkImageANGLEProc>(
      GetGLProcAddress("eglExportVkImageANGLE"));
  fn.eglGetCompositorTimingANDROIDFn =
      reinterpret_cast<eglGetCompositorTimingANDROIDProc>(
          GetGLProcAddress("eglGetCompositorTimingANDROID"));
  fn.eglGetCompositorTimingSupportedANDROIDFn =
      reinterpret_cast<eglGetCompositorTimingSupportedANDROIDProc>(
          GetGLProcAddress("eglGetCompositorTimingSupportedANDROID"));
  fn.eglGetConfigAttribFn = reinterpret_cast<eglGetConfigAttribProc>(
      GetGLProcAddress("eglGetConfigAttrib"));
  fn.eglGetConfigsFn =
      reinterpret_cast<eglGetConfigsProc>(GetGLProcAddress("eglGetConfigs"));
  fn.eglGetCurrentContextFn = reinterpret_cast<eglGetCurrentContextProc>(
      GetGLProcAddress("eglGetCurrentContext"));
  fn.eglGetCurrentDisplayFn = reinterpret_cast<eglGetCurrentDisplayProc>(
      GetGLProcAddress("eglGetCurrentDisplay"));
  fn.eglGetCurrentSurfaceFn = reinterpret_cast<eglGetCurrentSurfaceProc>(
      GetGLProcAddress("eglGetCurrentSurface"));
  fn.eglGetDisplayFn =
      reinterpret_cast<eglGetDisplayProc>(GetGLProcAddress("eglGetDisplay"));
  fn.eglGetErrorFn =
      reinterpret_cast<eglGetErrorProc>(GetGLProcAddress("eglGetError"));
  fn.eglGetFrameTimestampsANDROIDFn =
      reinterpret_cast<eglGetFrameTimestampsANDROIDProc>(
          GetGLProcAddress("eglGetFrameTimestampsANDROID"));
  fn.eglGetFrameTimestampSupportedANDROIDFn =
      reinterpret_cast<eglGetFrameTimestampSupportedANDROIDProc>(
          GetGLProcAddress("eglGetFrameTimestampSupportedANDROID"));
  fn.eglGetMscRateANGLEFn = reinterpret_cast<eglGetMscRateANGLEProc>(
      GetGLProcAddress("eglGetMscRateANGLE"));
  fn.eglGetNativeClientBufferANDROIDFn =
      reinterpret_cast<eglGetNativeClientBufferANDROIDProc>(
          GetGLProcAddress("eglGetNativeClientBufferANDROID"));
  fn.eglGetNextFrameIdANDROIDFn =
      reinterpret_cast<eglGetNextFrameIdANDROIDProc>(
          GetGLProcAddress("eglGetNextFrameIdANDROID"));
  fn.eglGetPlatformDisplayFn = reinterpret_cast<eglGetPlatformDisplayProc>(
      GetGLProcAddress("eglGetPlatformDisplay"));
  fn.eglGetProcAddressFn = reinterpret_cast<eglGetProcAddressProc>(
      GetGLProcAddress("eglGetProcAddress"));
  fn.eglGetSyncAttribFn = reinterpret_cast<eglGetSyncAttribProc>(
      GetGLProcAddress("eglGetSyncAttrib"));
  fn.eglGetSyncAttribKHRFn = reinterpret_cast<eglGetSyncAttribKHRProc>(
      GetGLProcAddress("eglGetSyncAttribKHR"));
  fn.eglGetSyncValuesCHROMIUMFn =
      reinterpret_cast<eglGetSyncValuesCHROMIUMProc>(
          GetGLProcAddress("eglGetSyncValuesCHROMIUM"));
  fn.eglHandleGPUSwitchANGLEFn = reinterpret_cast<eglHandleGPUSwitchANGLEProc>(
      GetGLProcAddress("eglHandleGPUSwitchANGLE"));
  fn.eglImageFlushExternalEXTFn =
      reinterpret_cast<eglImageFlushExternalEXTProc>(
          GetGLProcAddress("eglImageFlushExternalEXT"));
  fn.eglInitializeFn =
      reinterpret_cast<eglInitializeProc>(GetGLProcAddress("eglInitialize"));
  fn.eglLabelObjectKHRFn = reinterpret_cast<eglLabelObjectKHRProc>(
      GetGLProcAddress("eglLabelObjectKHR"));
  fn.eglMakeCurrentFn =
      reinterpret_cast<eglMakeCurrentProc>(GetGLProcAddress("eglMakeCurrent"));
  fn.eglPostSubBufferNVFn = reinterpret_cast<eglPostSubBufferNVProc>(
      GetGLProcAddress("eglPostSubBufferNV"));
  fn.eglQueryAPIFn =
      reinterpret_cast<eglQueryAPIProc>(GetGLProcAddress("eglQueryAPI"));
  fn.eglQueryContextFn = reinterpret_cast<eglQueryContextProc>(
      GetGLProcAddress("eglQueryContext"));
  fn.eglQueryDebugKHRFn = reinterpret_cast<eglQueryDebugKHRProc>(
      GetGLProcAddress("eglQueryDebugKHR"));
  fn.eglQueryDeviceAttribEXTFn = reinterpret_cast<eglQueryDeviceAttribEXTProc>(
      GetGLProcAddress("eglQueryDeviceAttribEXT"));
  fn.eglQueryDevicesEXTFn = reinterpret_cast<eglQueryDevicesEXTProc>(
      GetGLProcAddress("eglQueryDevicesEXT"));
  fn.eglQueryDeviceStringEXTFn = reinterpret_cast<eglQueryDeviceStringEXTProc>(
      GetGLProcAddress("eglQueryDeviceStringEXT"));
  fn.eglQueryDisplayAttribANGLEFn =
      reinterpret_cast<eglQueryDisplayAttribANGLEProc>(
          GetGLProcAddress("eglQueryDisplayAttribANGLE"));
  fn.eglQueryDisplayAttribEXTFn =
      reinterpret_cast<eglQueryDisplayAttribEXTProc>(
          GetGLProcAddress("eglQueryDisplayAttribEXT"));
  fn.eglQueryDmaBufFormatsEXTFn =
      reinterpret_cast<eglQueryDmaBufFormatsEXTProc>(
          GetGLProcAddress("eglQueryDmaBufFormatsEXT"));
  fn.eglQueryDmaBufModifiersEXTFn =
      reinterpret_cast<eglQueryDmaBufModifiersEXTProc>(
          GetGLProcAddress("eglQueryDmaBufModifiersEXT"));
  fn.eglQueryStreamKHRFn = reinterpret_cast<eglQueryStreamKHRProc>(
      GetGLProcAddress("eglQueryStreamKHR"));
  fn.eglQueryStreamu64KHRFn = reinterpret_cast<eglQueryStreamu64KHRProc>(
      GetGLProcAddress("eglQueryStreamu64KHR"));
  fn.eglQueryStringFn =
      reinterpret_cast<eglQueryStringProc>(GetGLProcAddress("eglQueryString"));
  fn.eglQueryStringiANGLEFn = reinterpret_cast<eglQueryStringiANGLEProc>(
      GetGLProcAddress("eglQueryStringiANGLE"));
  fn.eglQuerySurfaceFn = reinterpret_cast<eglQuerySurfaceProc>(
      GetGLProcAddress("eglQuerySurface"));
  fn.eglQuerySurfacePointerANGLEFn =
      reinterpret_cast<eglQuerySurfacePointerANGLEProc>(
          GetGLProcAddress("eglQuerySurfacePointerANGLE"));
  fn.eglReacquireHighPowerGPUANGLEFn =
      reinterpret_cast<eglReacquireHighPowerGPUANGLEProc>(
          GetGLProcAddress("eglReacquireHighPowerGPUANGLE"));
  fn.eglReleaseExternalContextANGLEFn =
      reinterpret_cast<eglReleaseExternalContextANGLEProc>(
          GetGLProcAddress("eglReleaseExternalContextANGLE"));
  fn.eglReleaseHighPowerGPUANGLEFn =
      reinterpret_cast<eglReleaseHighPowerGPUANGLEProc>(
          GetGLProcAddress("eglReleaseHighPowerGPUANGLE"));
  fn.eglReleaseTexImageFn = reinterpret_cast<eglReleaseTexImageProc>(
      GetGLProcAddress("eglReleaseTexImage"));
  fn.eglReleaseThreadFn = reinterpret_cast<eglReleaseThreadProc>(
      GetGLProcAddress("eglReleaseThread"));
  fn.eglSetBlobCacheFuncsANDROIDFn =
      reinterpret_cast<eglSetBlobCacheFuncsANDROIDProc>(
          GetGLProcAddress("eglSetBlobCacheFuncsANDROID"));
  fn.eglSetValidationEnabledANGLEFn =
      reinterpret_cast<eglSetValidationEnabledANGLEProc>(
          GetGLProcAddress("eglSetValidationEnabledANGLE"));
  fn.eglStreamAttribKHRFn = reinterpret_cast<eglStreamAttribKHRProc>(
      GetGLProcAddress("eglStreamAttribKHR"));
  fn.eglStreamConsumerAcquireKHRFn =
      reinterpret_cast<eglStreamConsumerAcquireKHRProc>(
          GetGLProcAddress("eglStreamConsumerAcquireKHR"));
  fn.eglStreamConsumerGLTextureExternalAttribsNVFn =
      reinterpret_cast<eglStreamConsumerGLTextureExternalAttribsNVProc>(
          GetGLProcAddress("eglStreamConsumerGLTextureExternalAttribsNV"));
  fn.eglStreamConsumerGLTextureExternalKHRFn =
      reinterpret_cast<eglStreamConsumerGLTextureExternalKHRProc>(
          GetGLProcAddress("eglStreamConsumerGLTextureExternalKHR"));
  fn.eglStreamConsumerReleaseKHRFn =
      reinterpret_cast<eglStreamConsumerReleaseKHRProc>(
          GetGLProcAddress("eglStreamConsumerReleaseKHR"));
  fn.eglStreamPostD3DTextureANGLEFn =
      reinterpret_cast<eglStreamPostD3DTextureANGLEProc>(
          GetGLProcAddress("eglStreamPostD3DTextureANGLE"));
  fn.eglSurfaceAttribFn = reinterpret_cast<eglSurfaceAttribProc>(
      GetGLProcAddress("eglSurfaceAttrib"));
  fn.eglSwapBuffersFn =
      reinterpret_cast<eglSwapBuffersProc>(GetGLProcAddress("eglSwapBuffers"));
  fn.eglSwapBuffersWithDamageKHRFn =
      reinterpret_cast<eglSwapBuffersWithDamageKHRProc>(
          GetGLProcAddress("eglSwapBuffersWithDamageKHR"));
  fn.eglSwapIntervalFn = reinterpret_cast<eglSwapIntervalProc>(
      GetGLProcAddress("eglSwapInterval"));
  fn.eglTerminateFn =
      reinterpret_cast<eglTerminateProc>(GetGLProcAddress("eglTerminate"));
  fn.eglWaitClientFn =
      reinterpret_cast<eglWaitClientProc>(GetGLProcAddress("eglWaitClient"));
  fn.eglWaitGLFn =
      reinterpret_cast<eglWaitGLProc>(GetGLProcAddress("eglWaitGL"));
  fn.eglWaitNativeFn =
      reinterpret_cast<eglWaitNativeProc>(GetGLProcAddress("eglWaitNative"));
  fn.eglWaitSyncFn =
      reinterpret_cast<eglWaitSyncProc>(GetGLProcAddress("eglWaitSync"));
  fn.eglWaitSyncKHRFn =
      reinterpret_cast<eglWaitSyncKHRProc>(GetGLProcAddress("eglWaitSyncKHR"));
  fn.eglWaitUntilWorkScheduledANGLEFn =
      reinterpret_cast<eglWaitUntilWorkScheduledANGLEProc>(
          GetGLProcAddress("eglWaitUntilWorkScheduledANGLE"));
}

void ClientExtensionsEGL::InitializeClientExtensionSettings() {
  std::string client_extensions(GetClientExtensions());
  [[maybe_unused]] gfx::ExtensionSet extensions(
      gfx::MakeExtensionSet(client_extensions));

  b_EGL_ANGLE_display_power_preference =
      gfx::HasExtension(extensions, "EGL_ANGLE_display_power_preference");
  b_EGL_ANGLE_feature_control =
      gfx::HasExtension(extensions, "EGL_ANGLE_feature_control");
  b_EGL_ANGLE_no_error = gfx::HasExtension(extensions, "EGL_ANGLE_no_error");
  b_EGL_ANGLE_platform_angle =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle");
  b_EGL_ANGLE_platform_angle_d3d =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_d3d");
  b_EGL_ANGLE_platform_angle_device_id =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_device_id");
  b_EGL_ANGLE_platform_angle_device_type_egl_angle = gfx::HasExtension(
      extensions, "EGL_ANGLE_platform_angle_device_type_egl_angle");
  b_EGL_ANGLE_platform_angle_device_type_swiftshader = gfx::HasExtension(
      extensions, "EGL_ANGLE_platform_angle_device_type_swiftshader");
  b_EGL_ANGLE_platform_angle_metal =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_metal");
  b_EGL_ANGLE_platform_angle_null =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_null");
  b_EGL_ANGLE_platform_angle_opengl =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_opengl");
  b_EGL_ANGLE_platform_angle_vulkan =
      gfx::HasExtension(extensions, "EGL_ANGLE_platform_angle_vulkan");
  b_EGL_EXT_device_base = gfx::HasExtension(extensions, "EGL_EXT_device_base");
  b_EGL_EXT_device_enumeration =
      gfx::HasExtension(extensions, "EGL_EXT_device_enumeration");
  b_EGL_EXT_device_query =
      gfx::HasExtension(extensions, "EGL_EXT_device_query");
  b_EGL_EXT_platform_device =
      gfx::HasExtension(extensions, "EGL_EXT_platform_device");
  b_EGL_KHR_debug = gfx::HasExtension(extensions, "EGL_KHR_debug");
  b_EGL_MESA_platform_surfaceless =
      gfx::HasExtension(extensions, "EGL_MESA_platform_surfaceless");
}

void DisplayExtensionsEGL::InitializeExtensionSettings(EGLDisplay display) {
  std::string platform_extensions(GetPlatformExtensions(display));
  [[maybe_unused]] gfx::ExtensionSet extensions(
      gfx::MakeExtensionSet(platform_extensions));

  b_EGL_ANDROID_blob_cache =
      gfx::HasExtension(extensions, "EGL_ANDROID_blob_cache");
  b_EGL_ANDROID_create_native_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANDROID_create_native_client_buffer");
  b_EGL_ANDROID_front_buffer_auto_refresh =
      gfx::HasExtension(extensions, "EGL_ANDROID_front_buffer_auto_refresh");
  b_EGL_ANDROID_get_frame_timestamps =
      gfx::HasExtension(extensions, "EGL_ANDROID_get_frame_timestamps");
  b_EGL_ANDROID_get_native_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANDROID_get_native_client_buffer");
  b_EGL_ANDROID_native_fence_sync =
      gfx::HasExtension(extensions, "EGL_ANDROID_native_fence_sync");
  b_EGL_ANGLE_context_virtualization =
      gfx::HasExtension(extensions, "EGL_ANGLE_context_virtualization");
  b_EGL_ANGLE_create_context_backwards_compatible = gfx::HasExtension(
      extensions, "EGL_ANGLE_create_context_backwards_compatible");
  b_EGL_ANGLE_create_context_client_arrays =
      gfx::HasExtension(extensions, "EGL_ANGLE_create_context_client_arrays");
  b_EGL_ANGLE_create_context_webgl_compatibility = gfx::HasExtension(
      extensions, "EGL_ANGLE_create_context_webgl_compatibility");
  b_EGL_ANGLE_d3d_share_handle_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANGLE_d3d_share_handle_client_buffer");
  b_EGL_ANGLE_display_semaphore_share_group =
      gfx::HasExtension(extensions, "EGL_ANGLE_display_semaphore_share_group");
  b_EGL_ANGLE_display_texture_share_group =
      gfx::HasExtension(extensions, "EGL_ANGLE_display_texture_share_group");
  b_EGL_ANGLE_external_context_and_surface =
      gfx::HasExtension(extensions, "EGL_ANGLE_external_context_and_surface");
  b_EGL_ANGLE_global_fence_sync =
      gfx::HasExtension(extensions, "EGL_ANGLE_global_fence_sync");
  b_EGL_ANGLE_iosurface_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANGLE_iosurface_client_buffer");
  b_EGL_ANGLE_keyed_mutex =
      gfx::HasExtension(extensions, "EGL_ANGLE_keyed_mutex");
  b_EGL_ANGLE_metal_shared_event_sync =
      gfx::HasExtension(extensions, "EGL_ANGLE_metal_shared_event_sync");
  b_EGL_ANGLE_no_error = gfx::HasExtension(extensions, "EGL_ANGLE_no_error");
  b_EGL_ANGLE_power_preference =
      gfx::HasExtension(extensions, "EGL_ANGLE_power_preference");
  b_EGL_ANGLE_query_surface_pointer =
      gfx::HasExtension(extensions, "EGL_ANGLE_query_surface_pointer");
  b_EGL_ANGLE_robust_resource_initialization =
      gfx::HasExtension(extensions, "EGL_ANGLE_robust_resource_initialization");
  b_EGL_ANGLE_stream_producer_d3d_texture =
      gfx::HasExtension(extensions, "EGL_ANGLE_stream_producer_d3d_texture");
  b_EGL_ANGLE_surface_d3d_texture_2d_share_handle = gfx::HasExtension(
      extensions, "EGL_ANGLE_surface_d3d_texture_2d_share_handle");
  b_EGL_ANGLE_surface_orientation =
      gfx::HasExtension(extensions, "EGL_ANGLE_surface_orientation");
  b_EGL_ANGLE_sync_control_rate =
      gfx::HasExtension(extensions, "EGL_ANGLE_sync_control_rate");
  b_EGL_ANGLE_vulkan_image =
      gfx::HasExtension(extensions, "EGL_ANGLE_vulkan_image");
  b_EGL_ANGLE_wait_until_work_scheduled =
      gfx::HasExtension(extensions, "EGL_ANGLE_wait_until_work_scheduled");
  b_EGL_ANGLE_window_fixed_size =
      gfx::HasExtension(extensions, "EGL_ANGLE_window_fixed_size");
  b_EGL_ARM_implicit_external_sync =
      gfx::HasExtension(extensions, "EGL_ARM_implicit_external_sync");
  b_EGL_CHROMIUM_create_context_bind_generates_resource = gfx::HasExtension(
      extensions, "EGL_CHROMIUM_create_context_bind_generates_resource");
  b_EGL_CHROMIUM_sync_control =
      gfx::HasExtension(extensions, "EGL_CHROMIUM_sync_control");
  b_EGL_EXT_create_context_robustness =
      gfx::HasExtension(extensions, "EGL_EXT_create_context_robustness");
  b_EGL_EXT_gl_colorspace_display_p3 =
      gfx::HasExtension(extensions, "EGL_EXT_gl_colorspace_display_p3");
  b_EGL_EXT_gl_colorspace_display_p3_passthrough = gfx::HasExtension(
      extensions, "EGL_EXT_gl_colorspace_display_p3_passthrough");
  b_EGL_EXT_image_dma_buf_import =
      gfx::HasExtension(extensions, "EGL_EXT_image_dma_buf_import");
  b_EGL_EXT_image_dma_buf_import_modifiers =
      gfx::HasExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
  b_EGL_EXT_image_flush_external =
      gfx::HasExtension(extensions, "EGL_EXT_image_flush_external");
  b_EGL_EXT_pixel_format_float =
      gfx::HasExtension(extensions, "EGL_EXT_pixel_format_float");
  b_EGL_IMG_context_priority =
      gfx::HasExtension(extensions, "EGL_IMG_context_priority");
  b_EGL_KHR_create_context =
      gfx::HasExtension(extensions, "EGL_KHR_create_context");
  b_EGL_KHR_fence_sync = gfx::HasExtension(extensions, "EGL_KHR_fence_sync");
  b_EGL_KHR_gl_colorspace =
      gfx::HasExtension(extensions, "EGL_KHR_gl_colorspace");
  b_EGL_KHR_gl_texture_2D_image =
      gfx::HasExtension(extensions, "EGL_KHR_gl_texture_2D_image");
  b_EGL_KHR_image = gfx::HasExtension(extensions, "EGL_KHR_image");
  b_EGL_KHR_image_base = gfx::HasExtension(extensions, "EGL_KHR_image_base");
  b_EGL_KHR_no_config_context =
      gfx::HasExtension(extensions, "EGL_KHR_no_config_context");
  b_EGL_KHR_stream = gfx::HasExtension(extensions, "EGL_KHR_stream");
  b_EGL_KHR_stream_consumer_gltexture =
      gfx::HasExtension(extensions, "EGL_KHR_stream_consumer_gltexture");
  b_EGL_KHR_surfaceless_context =
      gfx::HasExtension(extensions, "EGL_KHR_surfaceless_context");
  b_EGL_KHR_swap_buffers_with_damage =
      gfx::HasExtension(extensions, "EGL_KHR_swap_buffers_with_damage");
  b_EGL_KHR_wait_sync = gfx::HasExtension(extensions, "EGL_KHR_wait_sync");
  b_EGL_MESA_image_dma_buf_export =
      gfx::HasExtension(extensions, "EGL_MESA_image_dma_buf_export");
  b_EGL_NOK_texture_from_pixmap =
      gfx::HasExtension(extensions, "EGL_NOK_texture_from_pixmap");
  b_EGL_NV_post_sub_buffer =
      gfx::HasExtension(extensions, "EGL_NV_post_sub_buffer");
  b_EGL_NV_robustness_video_memory_purge =
      gfx::HasExtension(extensions, "EGL_NV_robustness_video_memory_purge");
  b_EGL_NV_stream_consumer_gltexture_yuv =
      gfx::HasExtension(extensions, "EGL_NV_stream_consumer_gltexture_yuv");
  b_GL_CHROMIUM_egl_android_native_fence_sync_hack = gfx::HasExtension(
      extensions, "GL_CHROMIUM_egl_android_native_fence_sync_hack");
  b_GL_CHROMIUM_egl_khr_fence_sync_hack =
      gfx::HasExtension(extensions, "GL_CHROMIUM_egl_khr_fence_sync_hack");
}

void DriverEGL::ClearBindings() {
  auto bytes = base::byte_span_from_ref(*this);
  std::ranges::fill(bytes, 0);
}

void EGLApiBase::eglAcquireExternalContextANGLEFn(EGLDisplay dpy,
                                                  EGLSurface readAndDraw) {
  driver_->fn.eglAcquireExternalContextANGLEFn(dpy, readAndDraw);
}

EGLBoolean EGLApiBase::eglBindAPIFn(EGLenum api) {
  return driver_->fn.eglBindAPIFn(api);
}

EGLBoolean EGLApiBase::eglBindTexImageFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLint buffer) {
  return driver_->fn.eglBindTexImageFn(dpy, surface, buffer);
}

EGLBoolean EGLApiBase::eglChooseConfigFn(EGLDisplay dpy,
                                         const EGLint* attrib_list,
                                         EGLConfig* configs,
                                         EGLint config_size,
                                         EGLint* num_config) {
  return driver_->fn.eglChooseConfigFn(dpy, attrib_list, configs, config_size,
                                       num_config);
}

EGLint EGLApiBase::eglClientWaitSyncFn(EGLDisplay dpy,
                                       EGLSync sync,
                                       EGLint flags,
                                       EGLTime timeout) {
  return driver_->fn.eglClientWaitSyncFn(dpy, sync, flags, timeout);
}

EGLint EGLApiBase::eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                          EGLSyncKHR sync,
                                          EGLint flags,
                                          EGLTimeKHR timeout) {
  return driver_->fn.eglClientWaitSyncKHRFn(dpy, sync, flags, timeout);
}

EGLBoolean EGLApiBase::eglCopyBuffersFn(EGLDisplay dpy,
                                        EGLSurface surface,
                                        EGLNativePixmapType target) {
  return driver_->fn.eglCopyBuffersFn(dpy, surface, target);
}

void* EGLApiBase::eglCopyMetalSharedEventANGLEFn(EGLDisplay dpy, EGLSync sync) {
  return driver_->fn.eglCopyMetalSharedEventANGLEFn(dpy, sync);
}

EGLContext EGLApiBase::eglCreateContextFn(EGLDisplay dpy,
                                          EGLConfig config,
                                          EGLContext share_context,
                                          const EGLint* attrib_list) {
  return driver_->fn.eglCreateContextFn(dpy, config, share_context,
                                        attrib_list);
}

EGLImage EGLApiBase::eglCreateImageFn(EGLDisplay dpy,
                                      EGLContext ctx,
                                      EGLenum target,
                                      EGLClientBuffer buffer,
                                      const EGLAttrib* attrib_list) {
  return driver_->fn.eglCreateImageFn(dpy, ctx, target, buffer, attrib_list);
}

EGLImageKHR EGLApiBase::eglCreateImageKHRFn(EGLDisplay dpy,
                                            EGLContext ctx,
                                            EGLenum target,
                                            EGLClientBuffer buffer,
                                            const EGLint* attrib_list) {
  return driver_->fn.eglCreateImageKHRFn(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface EGLApiBase::eglCreatePbufferFromClientBufferFn(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) {
  return driver_->fn.eglCreatePbufferFromClientBufferFn(dpy, buftype, buffer,
                                                        config, attrib_list);
}

EGLSurface EGLApiBase::eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 const EGLint* attrib_list) {
  return driver_->fn.eglCreatePbufferSurfaceFn(dpy, config, attrib_list);
}

EGLSurface EGLApiBase::eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                                EGLConfig config,
                                                EGLNativePixmapType pixmap,
                                                const EGLint* attrib_list) {
  return driver_->fn.eglCreatePixmapSurfaceFn(dpy, config, pixmap, attrib_list);
}

EGLSurface EGLApiBase::eglCreatePlatformPixmapSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list) {
  return driver_->fn.eglCreatePlatformPixmapSurfaceFn(
      dpy, config, native_pixmap, attrib_list);
}

EGLSurface EGLApiBase::eglCreatePlatformWindowSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list) {
  return driver_->fn.eglCreatePlatformWindowSurfaceFn(
      dpy, config, native_window, attrib_list);
}

EGLStreamKHR EGLApiBase::eglCreateStreamKHRFn(EGLDisplay dpy,
                                              const EGLint* attrib_list) {
  return driver_->fn.eglCreateStreamKHRFn(dpy, attrib_list);
}

EGLBoolean EGLApiBase::eglCreateStreamProducerD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  return driver_->fn.eglCreateStreamProducerD3DTextureANGLEFn(dpy, stream,
                                                              attrib_list);
}

EGLSync EGLApiBase::eglCreateSyncFn(EGLDisplay dpy,
                                    EGLenum type,
                                    const EGLAttrib* attrib_list) {
  return driver_->fn.eglCreateSyncFn(dpy, type, attrib_list);
}

EGLSyncKHR EGLApiBase::eglCreateSyncKHRFn(EGLDisplay dpy,
                                          EGLenum type,
                                          const EGLint* attrib_list) {
  return driver_->fn.eglCreateSyncKHRFn(dpy, type, attrib_list);
}

EGLSurface EGLApiBase::eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                                EGLConfig config,
                                                EGLNativeWindowType win,
                                                const EGLint* attrib_list) {
  return driver_->fn.eglCreateWindowSurfaceFn(dpy, config, win, attrib_list);
}

EGLint EGLApiBase::eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                               const EGLAttrib* attrib_list) {
  return driver_->fn.eglDebugMessageControlKHRFn(callback, attrib_list);
}

EGLBoolean EGLApiBase::eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) {
  return driver_->fn.eglDestroyContextFn(dpy, ctx);
}

EGLBoolean EGLApiBase::eglDestroyImageFn(EGLDisplay dpy, EGLImage image) {
  return driver_->fn.eglDestroyImageFn(dpy, image);
}

EGLBoolean EGLApiBase::eglDestroyImageKHRFn(EGLDisplay dpy, EGLImageKHR image) {
  return driver_->fn.eglDestroyImageKHRFn(dpy, image);
}

EGLBoolean EGLApiBase::eglDestroyStreamKHRFn(EGLDisplay dpy,
                                             EGLStreamKHR stream) {
  return driver_->fn.eglDestroyStreamKHRFn(dpy, stream);
}

EGLBoolean EGLApiBase::eglDestroySurfaceFn(EGLDisplay dpy, EGLSurface surface) {
  return driver_->fn.eglDestroySurfaceFn(dpy, surface);
}

EGLBoolean EGLApiBase::eglDestroySyncFn(EGLDisplay dpy, EGLSync sync) {
  return driver_->fn.eglDestroySyncFn(dpy, sync);
}

EGLBoolean EGLApiBase::eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) {
  return driver_->fn.eglDestroySyncKHRFn(dpy, sync);
}

EGLint EGLApiBase::eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                                EGLSyncKHR sync) {
  return driver_->fn.eglDupNativeFenceFDANDROIDFn(dpy, sync);
}

EGLBoolean EGLApiBase::eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                                  EGLImageKHR image,
                                                  int* fds,
                                                  EGLint* strides,
                                                  EGLint* offsets) {
  return driver_->fn.eglExportDMABUFImageMESAFn(dpy, image, fds, strides,
                                                offsets);
}

EGLBoolean EGLApiBase::eglExportDMABUFImageQueryMESAFn(
    EGLDisplay dpy,
    EGLImageKHR image,
    int* fourcc,
    int* num_planes,
    EGLuint64KHR* modifiers) {
  return driver_->fn.eglExportDMABUFImageQueryMESAFn(dpy, image, fourcc,
                                                     num_planes, modifiers);
}

EGLBoolean EGLApiBase::eglExportVkImageANGLEFn(EGLDisplay dpy,
                                               EGLImageKHR image,
                                               void* vk_image,
                                               void* vk_image_create_info) {
  return driver_->fn.eglExportVkImageANGLEFn(dpy, image, vk_image,
                                             vk_image_create_info);
}

EGLBoolean EGLApiBase::eglGetCompositorTimingANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint numTimestamps,
    EGLint* names,
    EGLnsecsANDROID* values) {
  return driver_->fn.eglGetCompositorTimingANDROIDFn(
      dpy, surface, numTimestamps, names, values);
}

EGLBoolean EGLApiBase::eglGetCompositorTimingSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  return driver_->fn.eglGetCompositorTimingSupportedANDROIDFn(dpy, surface,
                                                              timestamp);
}

EGLBoolean EGLApiBase::eglGetConfigAttribFn(EGLDisplay dpy,
                                            EGLConfig config,
                                            EGLint attribute,
                                            EGLint* value) {
  return driver_->fn.eglGetConfigAttribFn(dpy, config, attribute, value);
}

EGLBoolean EGLApiBase::eglGetConfigsFn(EGLDisplay dpy,
                                       EGLConfig* configs,
                                       EGLint config_size,
                                       EGLint* num_config) {
  return driver_->fn.eglGetConfigsFn(dpy, configs, config_size, num_config);
}

EGLContext EGLApiBase::eglGetCurrentContextFn(void) {
  return driver_->fn.eglGetCurrentContextFn();
}

EGLDisplay EGLApiBase::eglGetCurrentDisplayFn(void) {
  return driver_->fn.eglGetCurrentDisplayFn();
}

EGLSurface EGLApiBase::eglGetCurrentSurfaceFn(EGLint readdraw) {
  return driver_->fn.eglGetCurrentSurfaceFn(readdraw);
}

EGLDisplay EGLApiBase::eglGetDisplayFn(EGLNativeDisplayType display_id) {
  return driver_->fn.eglGetDisplayFn(display_id);
}

EGLint EGLApiBase::eglGetErrorFn(void) {
  return driver_->fn.eglGetErrorFn();
}

EGLBoolean EGLApiBase::eglGetFrameTimestampsANDROIDFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLuint64KHR frameId,
                                                      EGLint numTimestamps,
                                                      EGLint* timestamps,
                                                      EGLnsecsANDROID* values) {
  return driver_->fn.eglGetFrameTimestampsANDROIDFn(
      dpy, surface, frameId, numTimestamps, timestamps, values);
}

EGLBoolean EGLApiBase::eglGetFrameTimestampSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  return driver_->fn.eglGetFrameTimestampSupportedANDROIDFn(dpy, surface,
                                                            timestamp);
}

EGLBoolean EGLApiBase::eglGetMscRateANGLEFn(EGLDisplay dpy,
                                            EGLSurface surface,
                                            EGLint* numerator,
                                            EGLint* denominator) {
  return driver_->fn.eglGetMscRateANGLEFn(dpy, surface, numerator, denominator);
}

EGLClientBuffer EGLApiBase::eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) {
  return driver_->fn.eglGetNativeClientBufferANDROIDFn(ahardwarebuffer);
}

EGLBoolean EGLApiBase::eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                                  EGLSurface surface,
                                                  EGLuint64KHR* frameId) {
  return driver_->fn.eglGetNextFrameIdANDROIDFn(dpy, surface, frameId);
}

EGLDisplay EGLApiBase::eglGetPlatformDisplayFn(EGLenum platform,
                                               void* native_display,
                                               const EGLAttrib* attrib_list) {
  return driver_->fn.eglGetPlatformDisplayFn(platform, native_display,
                                             attrib_list);
}

__eglMustCastToProperFunctionPointerType EGLApiBase::eglGetProcAddressFn(
    const char* procname) {
  return driver_->fn.eglGetProcAddressFn(procname);
}

EGLBoolean EGLApiBase::eglGetSyncAttribFn(EGLDisplay dpy,
                                          EGLSync sync,
                                          EGLint attribute,
                                          EGLAttrib* value) {
  return driver_->fn.eglGetSyncAttribFn(dpy, sync, attribute, value);
}

EGLBoolean EGLApiBase::eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                             EGLSyncKHR sync,
                                             EGLint attribute,
                                             EGLint* value) {
  return driver_->fn.eglGetSyncAttribKHRFn(dpy, sync, attribute, value);
}

EGLBoolean EGLApiBase::eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                                  EGLSurface surface,
                                                  EGLuint64CHROMIUM* ust,
                                                  EGLuint64CHROMIUM* msc,
                                                  EGLuint64CHROMIUM* sbc) {
  return driver_->fn.eglGetSyncValuesCHROMIUMFn(dpy, surface, ust, msc, sbc);
}

void EGLApiBase::eglHandleGPUSwitchANGLEFn(EGLDisplay dpy) {
  driver_->fn.eglHandleGPUSwitchANGLEFn(dpy);
}

EGLBoolean EGLApiBase::eglImageFlushExternalEXTFn(
    EGLDisplay dpy,
    EGLImageKHR image,
    const EGLAttrib* attrib_list) {
  return driver_->fn.eglImageFlushExternalEXTFn(dpy, image, attrib_list);
}

EGLBoolean EGLApiBase::eglInitializeFn(EGLDisplay dpy,
                                       EGLint* major,
                                       EGLint* minor) {
  return driver_->fn.eglInitializeFn(dpy, major, minor);
}

EGLint EGLApiBase::eglLabelObjectKHRFn(EGLDisplay display,
                                       EGLenum objectType,
                                       EGLObjectKHR object,
                                       EGLLabelKHR label) {
  return driver_->fn.eglLabelObjectKHRFn(display, objectType, object, label);
}

EGLBoolean EGLApiBase::eglMakeCurrentFn(EGLDisplay dpy,
                                        EGLSurface draw,
                                        EGLSurface read,
                                        EGLContext ctx) {
  return driver_->fn.eglMakeCurrentFn(dpy, draw, read, ctx);
}

EGLBoolean EGLApiBase::eglPostSubBufferNVFn(EGLDisplay dpy,
                                            EGLSurface surface,
                                            EGLint x,
                                            EGLint y,
                                            EGLint width,
                                            EGLint height) {
  return driver_->fn.eglPostSubBufferNVFn(dpy, surface, x, y, width, height);
}

EGLenum EGLApiBase::eglQueryAPIFn(void) {
  return driver_->fn.eglQueryAPIFn();
}

EGLBoolean EGLApiBase::eglQueryContextFn(EGLDisplay dpy,
                                         EGLContext ctx,
                                         EGLint attribute,
                                         EGLint* value) {
  return driver_->fn.eglQueryContextFn(dpy, ctx, attribute, value);
}

EGLBoolean EGLApiBase::eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) {
  return driver_->fn.eglQueryDebugKHRFn(attribute, value);
}

EGLBoolean EGLApiBase::eglQueryDeviceAttribEXTFn(EGLDeviceEXT device,
                                                 EGLint attribute,
                                                 EGLAttrib* value) {
  return driver_->fn.eglQueryDeviceAttribEXTFn(device, attribute, value);
}

EGLBoolean EGLApiBase::eglQueryDevicesEXTFn(EGLint max_devices,
                                            EGLDeviceEXT* devices,
                                            EGLint* num_devices) {
  return driver_->fn.eglQueryDevicesEXTFn(max_devices, devices, num_devices);
}

const char* EGLApiBase::eglQueryDeviceStringEXTFn(EGLDeviceEXT device,
                                                  EGLint name) {
  return driver_->fn.eglQueryDeviceStringEXTFn(device, name);
}

EGLBoolean EGLApiBase::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                    EGLint attribute,
                                                    EGLAttrib* value) {
  return driver_->fn.eglQueryDisplayAttribANGLEFn(dpy, attribute, value);
}

EGLBoolean EGLApiBase::eglQueryDisplayAttribEXTFn(EGLDisplay dpy,
                                                  EGLint attribute,
                                                  EGLAttrib* value) {
  return driver_->fn.eglQueryDisplayAttribEXTFn(dpy, attribute, value);
}

EGLBoolean EGLApiBase::eglQueryDmaBufFormatsEXTFn(EGLDisplay dpy,
                                                  EGLint max_formats,
                                                  EGLint* formats,
                                                  EGLint* num_formats) {
  return driver_->fn.eglQueryDmaBufFormatsEXTFn(dpy, max_formats, formats,
                                                num_formats);
}

EGLBoolean EGLApiBase::eglQueryDmaBufModifiersEXTFn(EGLDisplay dpy,
                                                    EGLint format,
                                                    EGLint max_modifiers,
                                                    EGLuint64KHR* modifiers,
                                                    EGLBoolean* external_only,
                                                    EGLint* num_modifiers) {
  return driver_->fn.eglQueryDmaBufModifiersEXTFn(
      dpy, format, max_modifiers, modifiers, external_only, num_modifiers);
}

EGLBoolean EGLApiBase::eglQueryStreamKHRFn(EGLDisplay dpy,
                                           EGLStreamKHR stream,
                                           EGLenum attribute,
                                           EGLint* value) {
  return driver_->fn.eglQueryStreamKHRFn(dpy, stream, attribute, value);
}

EGLBoolean EGLApiBase::eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                              EGLStreamKHR stream,
                                              EGLenum attribute,
                                              EGLuint64KHR* value) {
  return driver_->fn.eglQueryStreamu64KHRFn(dpy, stream, attribute, value);
}

const char* EGLApiBase::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  return driver_->fn.eglQueryStringFn(dpy, name);
}

const char* EGLApiBase::eglQueryStringiANGLEFn(EGLDisplay dpy,
                                               EGLint name,
                                               EGLint index) {
  return driver_->fn.eglQueryStringiANGLEFn(dpy, name, index);
}

EGLBoolean EGLApiBase::eglQuerySurfaceFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLint attribute,
                                         EGLint* value) {
  return driver_->fn.eglQuerySurfaceFn(dpy, surface, attribute, value);
}

EGLBoolean EGLApiBase::eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                     EGLSurface surface,
                                                     EGLint attribute,
                                                     void** value) {
  return driver_->fn.eglQuerySurfacePointerANGLEFn(dpy, surface, attribute,
                                                   value);
}

void EGLApiBase::eglReacquireHighPowerGPUANGLEFn(EGLDisplay dpy,
                                                 EGLContext ctx) {
  driver_->fn.eglReacquireHighPowerGPUANGLEFn(dpy, ctx);
}

void EGLApiBase::eglReleaseExternalContextANGLEFn(EGLDisplay dpy) {
  driver_->fn.eglReleaseExternalContextANGLEFn(dpy);
}

void EGLApiBase::eglReleaseHighPowerGPUANGLEFn(EGLDisplay dpy, EGLContext ctx) {
  driver_->fn.eglReleaseHighPowerGPUANGLEFn(dpy, ctx);
}

EGLBoolean EGLApiBase::eglReleaseTexImageFn(EGLDisplay dpy,
                                            EGLSurface surface,
                                            EGLint buffer) {
  return driver_->fn.eglReleaseTexImageFn(dpy, surface, buffer);
}

EGLBoolean EGLApiBase::eglReleaseThreadFn(void) {
  return driver_->fn.eglReleaseThreadFn();
}

void EGLApiBase::eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                               EGLSetBlobFuncANDROID set,
                                               EGLGetBlobFuncANDROID get) {
  driver_->fn.eglSetBlobCacheFuncsANDROIDFn(dpy, set, get);
}

void EGLApiBase::eglSetValidationEnabledANGLEFn(EGLBoolean validationState) {
  driver_->fn.eglSetValidationEnabledANGLEFn(validationState);
}

EGLBoolean EGLApiBase::eglStreamAttribKHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLint value) {
  return driver_->fn.eglStreamAttribKHRFn(dpy, stream, attribute, value);
}

EGLBoolean EGLApiBase::eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                     EGLStreamKHR stream) {
  return driver_->fn.eglStreamConsumerAcquireKHRFn(dpy, stream);
}

EGLBoolean EGLApiBase::eglStreamConsumerGLTextureExternalAttribsNVFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  return driver_->fn.eglStreamConsumerGLTextureExternalAttribsNVFn(dpy, stream,
                                                                   attrib_list);
}

EGLBoolean EGLApiBase::eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  return driver_->fn.eglStreamConsumerGLTextureExternalKHRFn(dpy, stream);
}

EGLBoolean EGLApiBase::eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                     EGLStreamKHR stream) {
  return driver_->fn.eglStreamConsumerReleaseKHRFn(dpy, stream);
}

EGLBoolean EGLApiBase::eglStreamPostD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) {
  return driver_->fn.eglStreamPostD3DTextureANGLEFn(dpy, stream, texture,
                                                    attrib_list);
}

EGLBoolean EGLApiBase::eglSurfaceAttribFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint attribute,
                                          EGLint value) {
  return driver_->fn.eglSurfaceAttribFn(dpy, surface, attribute, value);
}

EGLBoolean EGLApiBase::eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) {
  return driver_->fn.eglSwapBuffersFn(dpy, surface);
}

EGLBoolean EGLApiBase::eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                                     EGLSurface surface,
                                                     EGLint* rects,
                                                     EGLint n_rects) {
  return driver_->fn.eglSwapBuffersWithDamageKHRFn(dpy, surface, rects,
                                                   n_rects);
}

EGLBoolean EGLApiBase::eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) {
  return driver_->fn.eglSwapIntervalFn(dpy, interval);
}

EGLBoolean EGLApiBase::eglTerminateFn(EGLDisplay dpy) {
  return driver_->fn.eglTerminateFn(dpy);
}

EGLBoolean EGLApiBase::eglWaitClientFn(void) {
  return driver_->fn.eglWaitClientFn();
}

EGLBoolean EGLApiBase::eglWaitGLFn(void) {
  return driver_->fn.eglWaitGLFn();
}

EGLBoolean EGLApiBase::eglWaitNativeFn(EGLint engine) {
  return driver_->fn.eglWaitNativeFn(engine);
}

EGLint EGLApiBase::eglWaitSyncFn(EGLDisplay dpy, EGLSync sync, EGLint flags) {
  return driver_->fn.eglWaitSyncFn(dpy, sync, flags);
}

EGLint EGLApiBase::eglWaitSyncKHRFn(EGLDisplay dpy,
                                    EGLSyncKHR sync,
                                    EGLint flags) {
  return driver_->fn.eglWaitSyncKHRFn(dpy, sync, flags);
}

void EGLApiBase::eglWaitUntilWorkScheduledANGLEFn(EGLDisplay dpy) {
  driver_->fn.eglWaitUntilWorkScheduledANGLEFn(dpy);
}

void TraceEGLApi::eglAcquireExternalContextANGLEFn(EGLDisplay dpy,
                                                   EGLSurface readAndDraw) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglAcquireExternalContextANGLE");
  egl_api_->eglAcquireExternalContextANGLEFn(dpy, readAndDraw);
}

EGLBoolean TraceEGLApi::eglBindAPIFn(EGLenum api) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglBindAPI");
  return egl_api_->eglBindAPIFn(api);
}

EGLBoolean TraceEGLApi::eglBindTexImageFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglBindTexImage");
  return egl_api_->eglBindTexImageFn(dpy, surface, buffer);
}

EGLBoolean TraceEGLApi::eglChooseConfigFn(EGLDisplay dpy,
                                          const EGLint* attrib_list,
                                          EGLConfig* configs,
                                          EGLint config_size,
                                          EGLint* num_config) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglChooseConfig");
  return egl_api_->eglChooseConfigFn(dpy, attrib_list, configs, config_size,
                                     num_config);
}

EGLint TraceEGLApi::eglClientWaitSyncFn(EGLDisplay dpy,
                                        EGLSync sync,
                                        EGLint flags,
                                        EGLTime timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglClientWaitSync");
  return egl_api_->eglClientWaitSyncFn(dpy, sync, flags, timeout);
}

EGLint TraceEGLApi::eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                           EGLSyncKHR sync,
                                           EGLint flags,
                                           EGLTimeKHR timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglClientWaitSyncKHR");
  return egl_api_->eglClientWaitSyncKHRFn(dpy, sync, flags, timeout);
}

EGLBoolean TraceEGLApi::eglCopyBuffersFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLNativePixmapType target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCopyBuffers");
  return egl_api_->eglCopyBuffersFn(dpy, surface, target);
}

void* TraceEGLApi::eglCopyMetalSharedEventANGLEFn(EGLDisplay dpy,
                                                  EGLSync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglCopyMetalSharedEventANGLE");
  return egl_api_->eglCopyMetalSharedEventANGLEFn(dpy, sync);
}

EGLContext TraceEGLApi::eglCreateContextFn(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLContext share_context,
                                           const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateContext");
  return egl_api_->eglCreateContextFn(dpy, config, share_context, attrib_list);
}

EGLImage TraceEGLApi::eglCreateImageFn(EGLDisplay dpy,
                                       EGLContext ctx,
                                       EGLenum target,
                                       EGLClientBuffer buffer,
                                       const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateImage");
  return egl_api_->eglCreateImageFn(dpy, ctx, target, buffer, attrib_list);
}

EGLImageKHR TraceEGLApi::eglCreateImageKHRFn(EGLDisplay dpy,
                                             EGLContext ctx,
                                             EGLenum target,
                                             EGLClientBuffer buffer,
                                             const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateImageKHR");
  return egl_api_->eglCreateImageKHRFn(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePbufferFromClientBufferFn(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglCreatePbufferFromClientBuffer");
  return egl_api_->eglCreatePbufferFromClientBufferFn(dpy, buftype, buffer,
                                                      config, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                                  EGLConfig config,
                                                  const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreatePbufferSurface");
  return egl_api_->eglCreatePbufferSurfaceFn(dpy, config, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativePixmapType pixmap,
                                                 const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreatePixmapSurface");
  return egl_api_->eglCreatePixmapSurfaceFn(dpy, config, pixmap, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePlatformPixmapSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglCreatePlatformPixmapSurface");
  return egl_api_->eglCreatePlatformPixmapSurfaceFn(dpy, config, native_pixmap,
                                                    attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePlatformWindowSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglCreatePlatformWindowSurface");
  return egl_api_->eglCreatePlatformWindowSurfaceFn(dpy, config, native_window,
                                                    attrib_list);
}

EGLStreamKHR TraceEGLApi::eglCreateStreamKHRFn(EGLDisplay dpy,
                                               const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateStreamKHR");
  return egl_api_->eglCreateStreamKHRFn(dpy, attrib_list);
}

EGLBoolean TraceEGLApi::eglCreateStreamProducerD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglCreateStreamProducerD3DTextureANGLE");
  return egl_api_->eglCreateStreamProducerD3DTextureANGLEFn(dpy, stream,
                                                            attrib_list);
}

EGLSync TraceEGLApi::eglCreateSyncFn(EGLDisplay dpy,
                                     EGLenum type,
                                     const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateSync");
  return egl_api_->eglCreateSyncFn(dpy, type, attrib_list);
}

EGLSyncKHR TraceEGLApi::eglCreateSyncKHRFn(EGLDisplay dpy,
                                           EGLenum type,
                                           const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateSyncKHR");
  return egl_api_->eglCreateSyncKHRFn(dpy, type, attrib_list);
}

EGLSurface TraceEGLApi::eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativeWindowType win,
                                                 const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglCreateWindowSurface");
  return egl_api_->eglCreateWindowSurfaceFn(dpy, config, win, attrib_list);
}

EGLint TraceEGLApi::eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                                const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglDebugMessageControlKHR");
  return egl_api_->eglDebugMessageControlKHRFn(callback, attrib_list);
}

EGLBoolean TraceEGLApi::eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroyContext");
  return egl_api_->eglDestroyContextFn(dpy, ctx);
}

EGLBoolean TraceEGLApi::eglDestroyImageFn(EGLDisplay dpy, EGLImage image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroyImage");
  return egl_api_->eglDestroyImageFn(dpy, image);
}

EGLBoolean TraceEGLApi::eglDestroyImageKHRFn(EGLDisplay dpy,
                                             EGLImageKHR image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroyImageKHR");
  return egl_api_->eglDestroyImageKHRFn(dpy, image);
}

EGLBoolean TraceEGLApi::eglDestroyStreamKHRFn(EGLDisplay dpy,
                                              EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroyStreamKHR");
  return egl_api_->eglDestroyStreamKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglDestroySurfaceFn(EGLDisplay dpy,
                                            EGLSurface surface) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroySurface");
  return egl_api_->eglDestroySurfaceFn(dpy, surface);
}

EGLBoolean TraceEGLApi::eglDestroySyncFn(EGLDisplay dpy, EGLSync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroySync");
  return egl_api_->eglDestroySyncFn(dpy, sync);
}

EGLBoolean TraceEGLApi::eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglDestroySyncKHR");
  return egl_api_->eglDestroySyncKHRFn(dpy, sync);
}

EGLint TraceEGLApi::eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                                 EGLSyncKHR sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglDupNativeFenceFDANDROID");
  return egl_api_->eglDupNativeFenceFDANDROIDFn(dpy, sync);
}

EGLBoolean TraceEGLApi::eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                                   EGLImageKHR image,
                                                   int* fds,
                                                   EGLint* strides,
                                                   EGLint* offsets) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglExportDMABUFImageMESA");
  return egl_api_->eglExportDMABUFImageMESAFn(dpy, image, fds, strides,
                                              offsets);
}

EGLBoolean TraceEGLApi::eglExportDMABUFImageQueryMESAFn(
    EGLDisplay dpy,
    EGLImageKHR image,
    int* fourcc,
    int* num_planes,
    EGLuint64KHR* modifiers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglExportDMABUFImageQueryMESA");
  return egl_api_->eglExportDMABUFImageQueryMESAFn(dpy, image, fourcc,
                                                   num_planes, modifiers);
}

EGLBoolean TraceEGLApi::eglExportVkImageANGLEFn(EGLDisplay dpy,
                                                EGLImageKHR image,
                                                void* vk_image,
                                                void* vk_image_create_info) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglExportVkImageANGLE");
  return egl_api_->eglExportVkImageANGLEFn(dpy, image, vk_image,
                                           vk_image_create_info);
}

EGLBoolean TraceEGLApi::eglGetCompositorTimingANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint numTimestamps,
    EGLint* names,
    EGLnsecsANDROID* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglGetCompositorTimingANDROID");
  return egl_api_->eglGetCompositorTimingANDROIDFn(dpy, surface, numTimestamps,
                                                   names, values);
}

EGLBoolean TraceEGLApi::eglGetCompositorTimingSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglGetCompositorTimingSupportedANDROID");
  return egl_api_->eglGetCompositorTimingSupportedANDROIDFn(dpy, surface,
                                                            timestamp);
}

EGLBoolean TraceEGLApi::eglGetConfigAttribFn(EGLDisplay dpy,
                                             EGLConfig config,
                                             EGLint attribute,
                                             EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetConfigAttrib");
  return egl_api_->eglGetConfigAttribFn(dpy, config, attribute, value);
}

EGLBoolean TraceEGLApi::eglGetConfigsFn(EGLDisplay dpy,
                                        EGLConfig* configs,
                                        EGLint config_size,
                                        EGLint* num_config) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetConfigs");
  return egl_api_->eglGetConfigsFn(dpy, configs, config_size, num_config);
}

EGLContext TraceEGLApi::eglGetCurrentContextFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetCurrentContext");
  return egl_api_->eglGetCurrentContextFn();
}

EGLDisplay TraceEGLApi::eglGetCurrentDisplayFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetCurrentDisplay");
  return egl_api_->eglGetCurrentDisplayFn();
}

EGLSurface TraceEGLApi::eglGetCurrentSurfaceFn(EGLint readdraw) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetCurrentSurface");
  return egl_api_->eglGetCurrentSurfaceFn(readdraw);
}

EGLDisplay TraceEGLApi::eglGetDisplayFn(EGLNativeDisplayType display_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetDisplay");
  return egl_api_->eglGetDisplayFn(display_id);
}

EGLint TraceEGLApi::eglGetErrorFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetError");
  return egl_api_->eglGetErrorFn();
}

EGLBoolean TraceEGLApi::eglGetFrameTimestampsANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLuint64KHR frameId,
    EGLint numTimestamps,
    EGLint* timestamps,
    EGLnsecsANDROID* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglGetFrameTimestampsANDROID");
  return egl_api_->eglGetFrameTimestampsANDROIDFn(
      dpy, surface, frameId, numTimestamps, timestamps, values);
}

EGLBoolean TraceEGLApi::eglGetFrameTimestampSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglGetFrameTimestampSupportedANDROID");
  return egl_api_->eglGetFrameTimestampSupportedANDROIDFn(dpy, surface,
                                                          timestamp);
}

EGLBoolean TraceEGLApi::eglGetMscRateANGLEFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint* numerator,
                                             EGLint* denominator) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetMscRateANGLE");
  return egl_api_->eglGetMscRateANGLEFn(dpy, surface, numerator, denominator);
}

EGLClientBuffer TraceEGLApi::eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglGetNativeClientBufferANDROID");
  return egl_api_->eglGetNativeClientBufferANDROIDFn(ahardwarebuffer);
}

EGLBoolean TraceEGLApi::eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLuint64KHR* frameId) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetNextFrameIdANDROID");
  return egl_api_->eglGetNextFrameIdANDROIDFn(dpy, surface, frameId);
}

EGLDisplay TraceEGLApi::eglGetPlatformDisplayFn(EGLenum platform,
                                                void* native_display,
                                                const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetPlatformDisplay");
  return egl_api_->eglGetPlatformDisplayFn(platform, native_display,
                                           attrib_list);
}

__eglMustCastToProperFunctionPointerType TraceEGLApi::eglGetProcAddressFn(
    const char* procname) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetProcAddress");
  return egl_api_->eglGetProcAddressFn(procname);
}

EGLBoolean TraceEGLApi::eglGetSyncAttribFn(EGLDisplay dpy,
                                           EGLSync sync,
                                           EGLint attribute,
                                           EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetSyncAttrib");
  return egl_api_->eglGetSyncAttribFn(dpy, sync, attribute, value);
}

EGLBoolean TraceEGLApi::eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                              EGLSyncKHR sync,
                                              EGLint attribute,
                                              EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetSyncAttribKHR");
  return egl_api_->eglGetSyncAttribKHRFn(dpy, sync, attribute, value);
}

EGLBoolean TraceEGLApi::eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLuint64CHROMIUM* ust,
                                                   EGLuint64CHROMIUM* msc,
                                                   EGLuint64CHROMIUM* sbc) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglGetSyncValuesCHROMIUM");
  return egl_api_->eglGetSyncValuesCHROMIUMFn(dpy, surface, ust, msc, sbc);
}

void TraceEGLApi::eglHandleGPUSwitchANGLEFn(EGLDisplay dpy) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglHandleGPUSwitchANGLE");
  egl_api_->eglHandleGPUSwitchANGLEFn(dpy);
}

EGLBoolean TraceEGLApi::eglImageFlushExternalEXTFn(
    EGLDisplay dpy,
    EGLImageKHR image,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglImageFlushExternalEXT");
  return egl_api_->eglImageFlushExternalEXTFn(dpy, image, attrib_list);
}

EGLBoolean TraceEGLApi::eglInitializeFn(EGLDisplay dpy,
                                        EGLint* major,
                                        EGLint* minor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglInitialize");
  return egl_api_->eglInitializeFn(dpy, major, minor);
}

EGLint TraceEGLApi::eglLabelObjectKHRFn(EGLDisplay display,
                                        EGLenum objectType,
                                        EGLObjectKHR object,
                                        EGLLabelKHR label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglLabelObjectKHR");
  return egl_api_->eglLabelObjectKHRFn(display, objectType, object, label);
}

EGLBoolean TraceEGLApi::eglMakeCurrentFn(EGLDisplay dpy,
                                         EGLSurface draw,
                                         EGLSurface read,
                                         EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglMakeCurrent");
  return egl_api_->eglMakeCurrentFn(dpy, draw, read, ctx);
}

EGLBoolean TraceEGLApi::eglPostSubBufferNVFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint x,
                                             EGLint y,
                                             EGLint width,
                                             EGLint height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglPostSubBufferNV");
  return egl_api_->eglPostSubBufferNVFn(dpy, surface, x, y, width, height);
}

EGLenum TraceEGLApi::eglQueryAPIFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryAPI");
  return egl_api_->eglQueryAPIFn();
}

EGLBoolean TraceEGLApi::eglQueryContextFn(EGLDisplay dpy,
                                          EGLContext ctx,
                                          EGLint attribute,
                                          EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryContext");
  return egl_api_->eglQueryContextFn(dpy, ctx, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDebugKHR");
  return egl_api_->eglQueryDebugKHRFn(attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDeviceAttribEXTFn(EGLDeviceEXT device,
                                                  EGLint attribute,
                                                  EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDeviceAttribEXT");
  return egl_api_->eglQueryDeviceAttribEXTFn(device, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDevicesEXTFn(EGLint max_devices,
                                             EGLDeviceEXT* devices,
                                             EGLint* num_devices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDevicesEXT");
  return egl_api_->eglQueryDevicesEXTFn(max_devices, devices, num_devices);
}

const char* TraceEGLApi::eglQueryDeviceStringEXTFn(EGLDeviceEXT device,
                                                   EGLint name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDeviceStringEXT");
  return egl_api_->eglQueryDeviceStringEXTFn(device, name);
}

EGLBoolean TraceEGLApi::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                     EGLint attribute,
                                                     EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglQueryDisplayAttribANGLE");
  return egl_api_->eglQueryDisplayAttribANGLEFn(dpy, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDisplayAttribEXTFn(EGLDisplay dpy,
                                                   EGLint attribute,
                                                   EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDisplayAttribEXT");
  return egl_api_->eglQueryDisplayAttribEXTFn(dpy, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDmaBufFormatsEXTFn(EGLDisplay dpy,
                                                   EGLint max_formats,
                                                   EGLint* formats,
                                                   EGLint* num_formats) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryDmaBufFormatsEXT");
  return egl_api_->eglQueryDmaBufFormatsEXTFn(dpy, max_formats, formats,
                                              num_formats);
}

EGLBoolean TraceEGLApi::eglQueryDmaBufModifiersEXTFn(EGLDisplay dpy,
                                                     EGLint format,
                                                     EGLint max_modifiers,
                                                     EGLuint64KHR* modifiers,
                                                     EGLBoolean* external_only,
                                                     EGLint* num_modifiers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglQueryDmaBufModifiersEXT");
  return egl_api_->eglQueryDmaBufModifiersEXTFn(
      dpy, format, max_modifiers, modifiers, external_only, num_modifiers);
}

EGLBoolean TraceEGLApi::eglQueryStreamKHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryStreamKHR");
  return egl_api_->eglQueryStreamKHRFn(dpy, stream, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                               EGLStreamKHR stream,
                                               EGLenum attribute,
                                               EGLuint64KHR* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryStreamu64KHR");
  return egl_api_->eglQueryStreamu64KHRFn(dpy, stream, attribute, value);
}

const char* TraceEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryString");
  return egl_api_->eglQueryStringFn(dpy, name);
}

const char* TraceEGLApi::eglQueryStringiANGLEFn(EGLDisplay dpy,
                                                EGLint name,
                                                EGLint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQueryStringiANGLE");
  return egl_api_->eglQueryStringiANGLEFn(dpy, name, index);
}

EGLBoolean TraceEGLApi::eglQuerySurfaceFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint attribute,
                                          EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglQuerySurface");
  return egl_api_->eglQuerySurfaceFn(dpy, surface, attribute, value);
}

EGLBoolean TraceEGLApi::eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint attribute,
                                                      void** value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglQuerySurfacePointerANGLE");
  return egl_api_->eglQuerySurfacePointerANGLEFn(dpy, surface, attribute,
                                                 value);
}

void TraceEGLApi::eglReacquireHighPowerGPUANGLEFn(EGLDisplay dpy,
                                                  EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglReacquireHighPowerGPUANGLE");
  egl_api_->eglReacquireHighPowerGPUANGLEFn(dpy, ctx);
}

void TraceEGLApi::eglReleaseExternalContextANGLEFn(EGLDisplay dpy) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglReleaseExternalContextANGLE");
  egl_api_->eglReleaseExternalContextANGLEFn(dpy);
}

void TraceEGLApi::eglReleaseHighPowerGPUANGLEFn(EGLDisplay dpy,
                                                EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglReleaseHighPowerGPUANGLE");
  egl_api_->eglReleaseHighPowerGPUANGLEFn(dpy, ctx);
}

EGLBoolean TraceEGLApi::eglReleaseTexImageFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglReleaseTexImage");
  return egl_api_->eglReleaseTexImageFn(dpy, surface, buffer);
}

EGLBoolean TraceEGLApi::eglReleaseThreadFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglReleaseThread");
  return egl_api_->eglReleaseThreadFn();
}

void TraceEGLApi::eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                                EGLSetBlobFuncANDROID set,
                                                EGLGetBlobFuncANDROID get) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglSetBlobCacheFuncsANDROID");
  egl_api_->eglSetBlobCacheFuncsANDROIDFn(dpy, set, get);
}

void TraceEGLApi::eglSetValidationEnabledANGLEFn(EGLBoolean validationState) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglSetValidationEnabledANGLE");
  egl_api_->eglSetValidationEnabledANGLEFn(validationState);
}

EGLBoolean TraceEGLApi::eglStreamAttribKHRFn(EGLDisplay dpy,
                                             EGLStreamKHR stream,
                                             EGLenum attribute,
                                             EGLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglStreamAttribKHR");
  return egl_api_->eglStreamAttribKHRFn(dpy, stream, attribute, value);
}

EGLBoolean TraceEGLApi::eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglStreamConsumerAcquireKHR");
  return egl_api_->eglStreamConsumerAcquireKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamConsumerGLTextureExternalAttribsNVFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglStreamConsumerGLTextureExternalAttribsNV");
  return egl_api_->eglStreamConsumerGLTextureExternalAttribsNVFn(dpy, stream,
                                                                 attrib_list);
}

EGLBoolean TraceEGLApi::eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceEGLAPI::eglStreamConsumerGLTextureExternalKHR");
  return egl_api_->eglStreamConsumerGLTextureExternalKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglStreamConsumerReleaseKHR");
  return egl_api_->eglStreamConsumerReleaseKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamPostD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglStreamPostD3DTextureANGLE");
  return egl_api_->eglStreamPostD3DTextureANGLEFn(dpy, stream, texture,
                                                  attrib_list);
}

EGLBoolean TraceEGLApi::eglSurfaceAttribFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint attribute,
                                           EGLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglSurfaceAttrib");
  return egl_api_->eglSurfaceAttribFn(dpy, surface, attribute, value);
}

EGLBoolean TraceEGLApi::eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglSwapBuffers");
  return egl_api_->eglSwapBuffersFn(dpy, surface);
}

EGLBoolean TraceEGLApi::eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint* rects,
                                                      EGLint n_rects) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglSwapBuffersWithDamageKHR");
  return egl_api_->eglSwapBuffersWithDamageKHRFn(dpy, surface, rects, n_rects);
}

EGLBoolean TraceEGLApi::eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglSwapInterval");
  return egl_api_->eglSwapIntervalFn(dpy, interval);
}

EGLBoolean TraceEGLApi::eglTerminateFn(EGLDisplay dpy) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglTerminate");
  return egl_api_->eglTerminateFn(dpy);
}

EGLBoolean TraceEGLApi::eglWaitClientFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglWaitClient");
  return egl_api_->eglWaitClientFn();
}

EGLBoolean TraceEGLApi::eglWaitGLFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglWaitGL");
  return egl_api_->eglWaitGLFn();
}

EGLBoolean TraceEGLApi::eglWaitNativeFn(EGLint engine) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglWaitNative");
  return egl_api_->eglWaitNativeFn(engine);
}

EGLint TraceEGLApi::eglWaitSyncFn(EGLDisplay dpy, EGLSync sync, EGLint flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglWaitSync");
  return egl_api_->eglWaitSyncFn(dpy, sync, flags);
}

EGLint TraceEGLApi::eglWaitSyncKHRFn(EGLDisplay dpy,
                                     EGLSyncKHR sync,
                                     EGLint flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceEGLAPI::eglWaitSyncKHR");
  return egl_api_->eglWaitSyncKHRFn(dpy, sync, flags);
}

void TraceEGLApi::eglWaitUntilWorkScheduledANGLEFn(EGLDisplay dpy) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceEGLAPI::eglWaitUntilWorkScheduledANGLE");
  egl_api_->eglWaitUntilWorkScheduledANGLEFn(dpy);
}

void LogEGLApi::eglAcquireExternalContextANGLEFn(EGLDisplay dpy,
                                                 EGLSurface readAndDraw) {
  GL_SERVICE_LOG("eglAcquireExternalContextANGLE" << "(" << dpy << ", "
                                                  << readAndDraw << ")");
  egl_api_->eglAcquireExternalContextANGLEFn(dpy, readAndDraw);
}

EGLBoolean LogEGLApi::eglBindAPIFn(EGLenum api) {
  GL_SERVICE_LOG("eglBindAPI" << "(" << api << ")");
  EGLBoolean result = egl_api_->eglBindAPIFn(api);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglBindTexImageFn(EGLDisplay dpy,
                                        EGLSurface surface,
                                        EGLint buffer) {
  GL_SERVICE_LOG("eglBindTexImage" << "(" << dpy << ", " << surface << ", "
                                   << buffer << ")");
  EGLBoolean result = egl_api_->eglBindTexImageFn(dpy, surface, buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglChooseConfigFn(EGLDisplay dpy,
                                        const EGLint* attrib_list,
                                        EGLConfig* configs,
                                        EGLint config_size,
                                        EGLint* num_config) {
  GL_SERVICE_LOG("eglChooseConfig"
                 << "(" << dpy << ", " << static_cast<const void*>(attrib_list)
                 << ", " << static_cast<const void*>(configs) << ", "
                 << config_size << ", " << static_cast<const void*>(num_config)
                 << ")");
  EGLBoolean result = egl_api_->eglChooseConfigFn(dpy, attrib_list, configs,
                                                  config_size, num_config);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglClientWaitSyncFn(EGLDisplay dpy,
                                      EGLSync sync,
                                      EGLint flags,
                                      EGLTime timeout) {
  GL_SERVICE_LOG("eglClientWaitSync" << "(" << dpy << ", " << sync << ", "
                                     << flags << ", " << timeout << ")");
  EGLint result = egl_api_->eglClientWaitSyncFn(dpy, sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                         EGLSyncKHR sync,
                                         EGLint flags,
                                         EGLTimeKHR timeout) {
  GL_SERVICE_LOG("eglClientWaitSyncKHR" << "(" << dpy << ", " << sync << ", "
                                        << flags << ", " << timeout << ")");
  EGLint result = egl_api_->eglClientWaitSyncKHRFn(dpy, sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglCopyBuffersFn(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLNativePixmapType target) {
  GL_SERVICE_LOG("eglCopyBuffers" << "(" << dpy << ", " << surface << ", "
                                  << target << ")");
  EGLBoolean result = egl_api_->eglCopyBuffersFn(dpy, surface, target);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void* LogEGLApi::eglCopyMetalSharedEventANGLEFn(EGLDisplay dpy, EGLSync sync) {
  GL_SERVICE_LOG("eglCopyMetalSharedEventANGLE" << "(" << dpy << ", " << sync
                                                << ")");
  void* result = egl_api_->eglCopyMetalSharedEventANGLEFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLContext LogEGLApi::eglCreateContextFn(EGLDisplay dpy,
                                         EGLConfig config,
                                         EGLContext share_context,
                                         const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateContext"
                 << "(" << dpy << ", " << config << ", " << share_context
                 << ", " << static_cast<const void*>(attrib_list) << ")");
  EGLContext result =
      egl_api_->eglCreateContextFn(dpy, config, share_context, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLImage LogEGLApi::eglCreateImageFn(EGLDisplay dpy,
                                     EGLContext ctx,
                                     EGLenum target,
                                     EGLClientBuffer buffer,
                                     const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglCreateImage" << "(" << dpy << ", " << ctx << ", " << target
                                  << ", " << buffer << ", "
                                  << static_cast<const void*>(attrib_list)
                                  << ")");
  EGLImage result =
      egl_api_->eglCreateImageFn(dpy, ctx, target, buffer, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLImageKHR LogEGLApi::eglCreateImageKHRFn(EGLDisplay dpy,
                                           EGLContext ctx,
                                           EGLenum target,
                                           EGLClientBuffer buffer,
                                           const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateImageKHR" << "(" << dpy << ", " << ctx << ", "
                                     << target << ", " << buffer << ", "
                                     << static_cast<const void*>(attrib_list)
                                     << ")");
  EGLImageKHR result =
      egl_api_->eglCreateImageKHRFn(dpy, ctx, target, buffer, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreatePbufferFromClientBufferFn(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreatePbufferFromClientBuffer"
                 << "(" << dpy << ", " << buftype << ", "
                 << static_cast<const void*>(buffer) << ", " << config << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result = egl_api_->eglCreatePbufferFromClientBufferFn(
      dpy, buftype, buffer, config, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                                EGLConfig config,
                                                const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreatePbufferSurface"
                 << "(" << dpy << ", " << config << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result =
      egl_api_->eglCreatePbufferSurfaceFn(dpy, config, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                               EGLConfig config,
                                               EGLNativePixmapType pixmap,
                                               const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreatePixmapSurface"
                 << "(" << dpy << ", " << config << ", " << pixmap << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result =
      egl_api_->eglCreatePixmapSurfaceFn(dpy, config, pixmap, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreatePlatformPixmapSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglCreatePlatformPixmapSurface"
                 << "(" << dpy << ", " << config << ", "
                 << static_cast<const void*>(native_pixmap) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result = egl_api_->eglCreatePlatformPixmapSurfaceFn(
      dpy, config, native_pixmap, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreatePlatformWindowSurfaceFn(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglCreatePlatformWindowSurface"
                 << "(" << dpy << ", " << config << ", "
                 << static_cast<const void*>(native_window) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result = egl_api_->eglCreatePlatformWindowSurfaceFn(
      dpy, config, native_window, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLStreamKHR LogEGLApi::eglCreateStreamKHRFn(EGLDisplay dpy,
                                             const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateStreamKHR" << "(" << dpy << ", "
                                      << static_cast<const void*>(attrib_list)
                                      << ")");
  EGLStreamKHR result = egl_api_->eglCreateStreamKHRFn(dpy, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglCreateStreamProducerD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglCreateStreamProducerD3DTextureANGLE"
                 << "(" << dpy << ", " << stream << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLBoolean result = egl_api_->eglCreateStreamProducerD3DTextureANGLEFn(
      dpy, stream, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSync LogEGLApi::eglCreateSyncFn(EGLDisplay dpy,
                                   EGLenum type,
                                   const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglCreateSync" << "(" << dpy << ", " << type << ", "
                                 << static_cast<const void*>(attrib_list)
                                 << ")");
  EGLSync result = egl_api_->eglCreateSyncFn(dpy, type, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSyncKHR LogEGLApi::eglCreateSyncKHRFn(EGLDisplay dpy,
                                         EGLenum type,
                                         const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateSyncKHR" << "(" << dpy << ", " << type << ", "
                                    << static_cast<const void*>(attrib_list)
                                    << ")");
  EGLSyncKHR result = egl_api_->eglCreateSyncKHRFn(dpy, type, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                               EGLConfig config,
                                               EGLNativeWindowType win,
                                               const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateWindowSurface"
                 << "(" << dpy << ", " << config << ", " << win << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSurface result =
      egl_api_->eglCreateWindowSurfaceFn(dpy, config, win, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                              const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglDebugMessageControlKHR"
                 << "(" << reinterpret_cast<void*>(callback) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLint result = egl_api_->eglDebugMessageControlKHRFn(callback, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) {
  GL_SERVICE_LOG("eglDestroyContext" << "(" << dpy << ", " << ctx << ")");
  EGLBoolean result = egl_api_->eglDestroyContextFn(dpy, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroyImageFn(EGLDisplay dpy, EGLImage image) {
  GL_SERVICE_LOG("eglDestroyImage" << "(" << dpy << ", " << image << ")");
  EGLBoolean result = egl_api_->eglDestroyImageFn(dpy, image);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroyImageKHRFn(EGLDisplay dpy, EGLImageKHR image) {
  GL_SERVICE_LOG("eglDestroyImageKHR" << "(" << dpy << ", " << image << ")");
  EGLBoolean result = egl_api_->eglDestroyImageKHRFn(dpy, image);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroyStreamKHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglDestroyStreamKHR" << "(" << dpy << ", " << stream << ")");
  EGLBoolean result = egl_api_->eglDestroyStreamKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroySurfaceFn(EGLDisplay dpy, EGLSurface surface) {
  GL_SERVICE_LOG("eglDestroySurface" << "(" << dpy << ", " << surface << ")");
  EGLBoolean result = egl_api_->eglDestroySurfaceFn(dpy, surface);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroySyncFn(EGLDisplay dpy, EGLSync sync) {
  GL_SERVICE_LOG("eglDestroySync" << "(" << dpy << ", " << sync << ")");
  EGLBoolean result = egl_api_->eglDestroySyncFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) {
  GL_SERVICE_LOG("eglDestroySyncKHR" << "(" << dpy << ", " << sync << ")");
  EGLBoolean result = egl_api_->eglDestroySyncKHRFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                               EGLSyncKHR sync) {
  GL_SERVICE_LOG("eglDupNativeFenceFDANDROID" << "(" << dpy << ", " << sync
                                              << ")");
  EGLint result = egl_api_->eglDupNativeFenceFDANDROIDFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                                 EGLImageKHR image,
                                                 int* fds,
                                                 EGLint* strides,
                                                 EGLint* offsets) {
  GL_SERVICE_LOG("eglExportDMABUFImageMESA"
                 << "(" << dpy << ", " << image << ", "
                 << static_cast<const void*>(fds) << ", "
                 << static_cast<const void*>(strides) << ", "
                 << static_cast<const void*>(offsets) << ")");
  EGLBoolean result =
      egl_api_->eglExportDMABUFImageMESAFn(dpy, image, fds, strides, offsets);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglExportDMABUFImageQueryMESAFn(EGLDisplay dpy,
                                                      EGLImageKHR image,
                                                      int* fourcc,
                                                      int* num_planes,
                                                      EGLuint64KHR* modifiers) {
  GL_SERVICE_LOG("eglExportDMABUFImageQueryMESA"
                 << "(" << dpy << ", " << image << ", "
                 << static_cast<const void*>(fourcc) << ", "
                 << static_cast<const void*>(num_planes) << ", "
                 << static_cast<const void*>(modifiers) << ")");
  EGLBoolean result = egl_api_->eglExportDMABUFImageQueryMESAFn(
      dpy, image, fourcc, num_planes, modifiers);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglExportVkImageANGLEFn(EGLDisplay dpy,
                                              EGLImageKHR image,
                                              void* vk_image,
                                              void* vk_image_create_info) {
  GL_SERVICE_LOG("eglExportVkImageANGLE"
                 << "(" << dpy << ", " << image << ", "
                 << static_cast<const void*>(vk_image) << ", "
                 << static_cast<const void*>(vk_image_create_info) << ")");
  EGLBoolean result = egl_api_->eglExportVkImageANGLEFn(dpy, image, vk_image,
                                                        vk_image_create_info);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetCompositorTimingANDROIDFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint numTimestamps,
                                                      EGLint* names,
                                                      EGLnsecsANDROID* values) {
  GL_SERVICE_LOG("eglGetCompositorTimingANDROID"
                 << "(" << dpy << ", " << surface << ", " << numTimestamps
                 << ", " << static_cast<const void*>(names) << ", "
                 << static_cast<const void*>(values) << ")");
  EGLBoolean result = egl_api_->eglGetCompositorTimingANDROIDFn(
      dpy, surface, numTimestamps, names, values);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetCompositorTimingSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  GL_SERVICE_LOG("eglGetCompositorTimingSupportedANDROID"
                 << "(" << dpy << ", " << surface << ", " << timestamp << ")");
  EGLBoolean result = egl_api_->eglGetCompositorTimingSupportedANDROIDFn(
      dpy, surface, timestamp);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetConfigAttribFn(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLint attribute,
                                           EGLint* value) {
  GL_SERVICE_LOG("eglGetConfigAttrib"
                 << "(" << dpy << ", " << config << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglGetConfigAttribFn(dpy, config, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetConfigsFn(EGLDisplay dpy,
                                      EGLConfig* configs,
                                      EGLint config_size,
                                      EGLint* num_config) {
  GL_SERVICE_LOG("eglGetConfigs"
                 << "(" << dpy << ", " << static_cast<const void*>(configs)
                 << ", " << config_size << ", "
                 << static_cast<const void*>(num_config) << ")");
  EGLBoolean result =
      egl_api_->eglGetConfigsFn(dpy, configs, config_size, num_config);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLContext LogEGLApi::eglGetCurrentContextFn(void) {
  GL_SERVICE_LOG("eglGetCurrentContext" << "(" << ")");
  EGLContext result = egl_api_->eglGetCurrentContextFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLDisplay LogEGLApi::eglGetCurrentDisplayFn(void) {
  GL_SERVICE_LOG("eglGetCurrentDisplay" << "(" << ")");
  EGLDisplay result = egl_api_->eglGetCurrentDisplayFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface LogEGLApi::eglGetCurrentSurfaceFn(EGLint readdraw) {
  GL_SERVICE_LOG("eglGetCurrentSurface" << "(" << readdraw << ")");
  EGLSurface result = egl_api_->eglGetCurrentSurfaceFn(readdraw);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLDisplay LogEGLApi::eglGetDisplayFn(EGLNativeDisplayType display_id) {
  GL_SERVICE_LOG("eglGetDisplay" << "(" << display_id << ")");
  EGLDisplay result = egl_api_->eglGetDisplayFn(display_id);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglGetErrorFn(void) {
  GL_SERVICE_LOG("eglGetError" << "(" << ")");
  EGLint result = egl_api_->eglGetErrorFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetFrameTimestampsANDROIDFn(EGLDisplay dpy,
                                                     EGLSurface surface,
                                                     EGLuint64KHR frameId,
                                                     EGLint numTimestamps,
                                                     EGLint* timestamps,
                                                     EGLnsecsANDROID* values) {
  GL_SERVICE_LOG("eglGetFrameTimestampsANDROID"
                 << "(" << dpy << ", " << surface << ", " << frameId << ", "
                 << numTimestamps << ", "
                 << static_cast<const void*>(timestamps) << ", "
                 << static_cast<const void*>(values) << ")");
  EGLBoolean result = egl_api_->eglGetFrameTimestampsANDROIDFn(
      dpy, surface, frameId, numTimestamps, timestamps, values);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetFrameTimestampSupportedANDROIDFn(EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLint timestamp) {
  GL_SERVICE_LOG("eglGetFrameTimestampSupportedANDROID"
                 << "(" << dpy << ", " << surface << ", " << timestamp << ")");
  EGLBoolean result =
      egl_api_->eglGetFrameTimestampSupportedANDROIDFn(dpy, surface, timestamp);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetMscRateANGLEFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint* numerator,
                                           EGLint* denominator) {
  GL_SERVICE_LOG("eglGetMscRateANGLE"
                 << "(" << dpy << ", " << surface << ", "
                 << static_cast<const void*>(numerator) << ", "
                 << static_cast<const void*>(denominator) << ")");
  EGLBoolean result =
      egl_api_->eglGetMscRateANGLEFn(dpy, surface, numerator, denominator);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLClientBuffer LogEGLApi::eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) {
  GL_SERVICE_LOG("eglGetNativeClientBufferANDROID"
                 << "(" << static_cast<const void*>(ahardwarebuffer) << ")");
  EGLClientBuffer result =
      egl_api_->eglGetNativeClientBufferANDROIDFn(ahardwarebuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLuint64KHR* frameId) {
  GL_SERVICE_LOG("eglGetNextFrameIdANDROID"
                 << "(" << dpy << ", " << surface << ", "
                 << static_cast<const void*>(frameId) << ")");
  EGLBoolean result =
      egl_api_->eglGetNextFrameIdANDROIDFn(dpy, surface, frameId);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLDisplay LogEGLApi::eglGetPlatformDisplayFn(EGLenum platform,
                                              void* native_display,
                                              const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglGetPlatformDisplay"
                 << "(" << platform << ", "
                 << static_cast<const void*>(native_display) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLDisplay result =
      egl_api_->eglGetPlatformDisplayFn(platform, native_display, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

__eglMustCastToProperFunctionPointerType LogEGLApi::eglGetProcAddressFn(
    const char* procname) {
  GL_SERVICE_LOG("eglGetProcAddress" << "(" << procname << ")");
  __eglMustCastToProperFunctionPointerType result =
      egl_api_->eglGetProcAddressFn(procname);

  GL_SERVICE_LOG("GL_RESULT: " << reinterpret_cast<void*>(result));

  return result;
}

EGLBoolean LogEGLApi::eglGetSyncAttribFn(EGLDisplay dpy,
                                         EGLSync sync,
                                         EGLint attribute,
                                         EGLAttrib* value) {
  GL_SERVICE_LOG("eglGetSyncAttrib" << "(" << dpy << ", " << sync << ", "
                                    << attribute << ", "
                                    << static_cast<const void*>(value) << ")");
  EGLBoolean result = egl_api_->eglGetSyncAttribFn(dpy, sync, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                            EGLSyncKHR sync,
                                            EGLint attribute,
                                            EGLint* value) {
  GL_SERVICE_LOG("eglGetSyncAttribKHR"
                 << "(" << dpy << ", " << sync << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglGetSyncAttribKHRFn(dpy, sync, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLuint64CHROMIUM* ust,
                                                 EGLuint64CHROMIUM* msc,
                                                 EGLuint64CHROMIUM* sbc) {
  GL_SERVICE_LOG("eglGetSyncValuesCHROMIUM"
                 << "(" << dpy << ", " << surface << ", "
                 << static_cast<const void*>(ust) << ", "
                 << static_cast<const void*>(msc) << ", "
                 << static_cast<const void*>(sbc) << ")");
  EGLBoolean result =
      egl_api_->eglGetSyncValuesCHROMIUMFn(dpy, surface, ust, msc, sbc);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogEGLApi::eglHandleGPUSwitchANGLEFn(EGLDisplay dpy) {
  GL_SERVICE_LOG("eglHandleGPUSwitchANGLE" << "(" << dpy << ")");
  egl_api_->eglHandleGPUSwitchANGLEFn(dpy);
}

EGLBoolean LogEGLApi::eglImageFlushExternalEXTFn(EGLDisplay dpy,
                                                 EGLImageKHR image,
                                                 const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglImageFlushExternalEXT"
                 << "(" << dpy << ", " << image << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLBoolean result =
      egl_api_->eglImageFlushExternalEXTFn(dpy, image, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglInitializeFn(EGLDisplay dpy,
                                      EGLint* major,
                                      EGLint* minor) {
  GL_SERVICE_LOG("eglInitialize" << "(" << dpy << ", "
                                 << static_cast<const void*>(major) << ", "
                                 << static_cast<const void*>(minor) << ")");
  EGLBoolean result = egl_api_->eglInitializeFn(dpy, major, minor);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglLabelObjectKHRFn(EGLDisplay display,
                                      EGLenum objectType,
                                      EGLObjectKHR object,
                                      EGLLabelKHR label) {
  GL_SERVICE_LOG("eglLabelObjectKHR" << "(" << display << ", " << objectType
                                     << ", " << object << ", " << label << ")");
  EGLint result =
      egl_api_->eglLabelObjectKHRFn(display, objectType, object, label);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglMakeCurrentFn(EGLDisplay dpy,
                                       EGLSurface draw,
                                       EGLSurface read,
                                       EGLContext ctx) {
  GL_SERVICE_LOG("eglMakeCurrent" << "(" << dpy << ", " << draw << ", " << read
                                  << ", " << ctx << ")");
  EGLBoolean result = egl_api_->eglMakeCurrentFn(dpy, draw, read, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglPostSubBufferNVFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint x,
                                           EGLint y,
                                           EGLint width,
                                           EGLint height) {
  GL_SERVICE_LOG("eglPostSubBufferNV" << "(" << dpy << ", " << surface << ", "
                                      << x << ", " << y << ", " << width << ", "
                                      << height << ")");
  EGLBoolean result =
      egl_api_->eglPostSubBufferNVFn(dpy, surface, x, y, width, height);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLenum LogEGLApi::eglQueryAPIFn(void) {
  GL_SERVICE_LOG("eglQueryAPI" << "(" << ")");
  EGLenum result = egl_api_->eglQueryAPIFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryContextFn(EGLDisplay dpy,
                                        EGLContext ctx,
                                        EGLint attribute,
                                        EGLint* value) {
  GL_SERVICE_LOG("eglQueryContext" << "(" << dpy << ", " << ctx << ", "
                                   << attribute << ", "
                                   << static_cast<const void*>(value) << ")");
  EGLBoolean result = egl_api_->eglQueryContextFn(dpy, ctx, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) {
  GL_SERVICE_LOG("eglQueryDebugKHR" << "(" << attribute << ", "
                                    << static_cast<const void*>(value) << ")");
  EGLBoolean result = egl_api_->eglQueryDebugKHRFn(attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDeviceAttribEXTFn(EGLDeviceEXT device,
                                                EGLint attribute,
                                                EGLAttrib* value) {
  GL_SERVICE_LOG("eglQueryDeviceAttribEXT"
                 << "(" << device << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryDeviceAttribEXTFn(device, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDevicesEXTFn(EGLint max_devices,
                                           EGLDeviceEXT* devices,
                                           EGLint* num_devices) {
  GL_SERVICE_LOG("eglQueryDevicesEXT"
                 << "(" << max_devices << ", "
                 << static_cast<const void*>(devices) << ", "
                 << static_cast<const void*>(num_devices) << ")");
  EGLBoolean result =
      egl_api_->eglQueryDevicesEXTFn(max_devices, devices, num_devices);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* LogEGLApi::eglQueryDeviceStringEXTFn(EGLDeviceEXT device,
                                                 EGLint name) {
  GL_SERVICE_LOG("eglQueryDeviceStringEXT" << "(" << device << ", " << name
                                           << ")");
  const char* result = egl_api_->eglQueryDeviceStringEXTFn(device, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                   EGLint attribute,
                                                   EGLAttrib* value) {
  GL_SERVICE_LOG("eglQueryDisplayAttribANGLE"
                 << "(" << dpy << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryDisplayAttribANGLEFn(dpy, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDisplayAttribEXTFn(EGLDisplay dpy,
                                                 EGLint attribute,
                                                 EGLAttrib* value) {
  GL_SERVICE_LOG("eglQueryDisplayAttribEXT"
                 << "(" << dpy << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryDisplayAttribEXTFn(dpy, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDmaBufFormatsEXTFn(EGLDisplay dpy,
                                                 EGLint max_formats,
                                                 EGLint* formats,
                                                 EGLint* num_formats) {
  GL_SERVICE_LOG("eglQueryDmaBufFormatsEXT"
                 << "(" << dpy << ", " << max_formats << ", "
                 << static_cast<const void*>(formats) << ", "
                 << static_cast<const void*>(num_formats) << ")");
  EGLBoolean result = egl_api_->eglQueryDmaBufFormatsEXTFn(
      dpy, max_formats, formats, num_formats);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryDmaBufModifiersEXTFn(EGLDisplay dpy,
                                                   EGLint format,
                                                   EGLint max_modifiers,
                                                   EGLuint64KHR* modifiers,
                                                   EGLBoolean* external_only,
                                                   EGLint* num_modifiers) {
  GL_SERVICE_LOG("eglQueryDmaBufModifiersEXT"
                 << "(" << dpy << ", " << format << ", " << max_modifiers
                 << ", " << static_cast<const void*>(modifiers) << ", "
                 << static_cast<const void*>(external_only) << ", "
                 << static_cast<const void*>(num_modifiers) << ")");
  EGLBoolean result = egl_api_->eglQueryDmaBufModifiersEXTFn(
      dpy, format, max_modifiers, modifiers, external_only, num_modifiers);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryStreamKHRFn(EGLDisplay dpy,
                                          EGLStreamKHR stream,
                                          EGLenum attribute,
                                          EGLint* value) {
  GL_SERVICE_LOG("eglQueryStreamKHR" << "(" << dpy << ", " << stream << ", "
                                     << attribute << ", "
                                     << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryStreamKHRFn(dpy, stream, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                             EGLStreamKHR stream,
                                             EGLenum attribute,
                                             EGLuint64KHR* value) {
  GL_SERVICE_LOG("eglQueryStreamu64KHR"
                 << "(" << dpy << ", " << stream << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryStreamu64KHRFn(dpy, stream, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* LogEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  GL_SERVICE_LOG("eglQueryString" << "(" << dpy << ", " << name << ")");
  const char* result = egl_api_->eglQueryStringFn(dpy, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* LogEGLApi::eglQueryStringiANGLEFn(EGLDisplay dpy,
                                              EGLint name,
                                              EGLint index) {
  GL_SERVICE_LOG("eglQueryStringiANGLE" << "(" << dpy << ", " << name << ", "
                                        << index << ")");
  const char* result = egl_api_->eglQueryStringiANGLEFn(dpy, name, index);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQuerySurfaceFn(EGLDisplay dpy,
                                        EGLSurface surface,
                                        EGLint attribute,
                                        EGLint* value) {
  GL_SERVICE_LOG("eglQuerySurface" << "(" << dpy << ", " << surface << ", "
                                   << attribute << ", "
                                   << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQuerySurfaceFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                    EGLSurface surface,
                                                    EGLint attribute,
                                                    void** value) {
  GL_SERVICE_LOG("eglQuerySurfacePointerANGLE" << "(" << dpy << ", " << surface
                                               << ", " << attribute << ", "
                                               << value << ")");
  EGLBoolean result =
      egl_api_->eglQuerySurfacePointerANGLEFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogEGLApi::eglReacquireHighPowerGPUANGLEFn(EGLDisplay dpy,
                                                EGLContext ctx) {
  GL_SERVICE_LOG("eglReacquireHighPowerGPUANGLE" << "(" << dpy << ", " << ctx
                                                 << ")");
  egl_api_->eglReacquireHighPowerGPUANGLEFn(dpy, ctx);
}

void LogEGLApi::eglReleaseExternalContextANGLEFn(EGLDisplay dpy) {
  GL_SERVICE_LOG("eglReleaseExternalContextANGLE" << "(" << dpy << ")");
  egl_api_->eglReleaseExternalContextANGLEFn(dpy);
}

void LogEGLApi::eglReleaseHighPowerGPUANGLEFn(EGLDisplay dpy, EGLContext ctx) {
  GL_SERVICE_LOG("eglReleaseHighPowerGPUANGLE" << "(" << dpy << ", " << ctx
                                               << ")");
  egl_api_->eglReleaseHighPowerGPUANGLEFn(dpy, ctx);
}

EGLBoolean LogEGLApi::eglReleaseTexImageFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint buffer) {
  GL_SERVICE_LOG("eglReleaseTexImage" << "(" << dpy << ", " << surface << ", "
                                      << buffer << ")");
  EGLBoolean result = egl_api_->eglReleaseTexImageFn(dpy, surface, buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglReleaseThreadFn(void) {
  GL_SERVICE_LOG("eglReleaseThread" << "(" << ")");
  EGLBoolean result = egl_api_->eglReleaseThreadFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogEGLApi::eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                              EGLSetBlobFuncANDROID set,
                                              EGLGetBlobFuncANDROID get) {
  GL_SERVICE_LOG("eglSetBlobCacheFuncsANDROID"
                 << "(" << dpy << ", " << reinterpret_cast<const void*>(set)
                 << ", " << reinterpret_cast<const void*>(get) << ")");
  egl_api_->eglSetBlobCacheFuncsANDROIDFn(dpy, set, get);
}

void LogEGLApi::eglSetValidationEnabledANGLEFn(EGLBoolean validationState) {
  GL_SERVICE_LOG("eglSetValidationEnabledANGLE" << "(" << validationState
                                                << ")");
  egl_api_->eglSetValidationEnabledANGLEFn(validationState);
}

EGLBoolean LogEGLApi::eglStreamAttribKHRFn(EGLDisplay dpy,
                                           EGLStreamKHR stream,
                                           EGLenum attribute,
                                           EGLint value) {
  GL_SERVICE_LOG("eglStreamAttribKHR" << "(" << dpy << ", " << stream << ", "
                                      << attribute << ", " << value << ")");
  EGLBoolean result =
      egl_api_->eglStreamAttribKHRFn(dpy, stream, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                    EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerAcquireKHR" << "(" << dpy << ", " << stream
                                               << ")");
  EGLBoolean result = egl_api_->eglStreamConsumerAcquireKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglStreamConsumerGLTextureExternalAttribsNVFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglStreamConsumerGLTextureExternalAttribsNV"
                 << "(" << dpy << ", " << stream << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLBoolean result = egl_api_->eglStreamConsumerGLTextureExternalAttribsNVFn(
      dpy, stream, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerGLTextureExternalKHR" << "(" << dpy << ", "
                                                         << stream << ")");
  EGLBoolean result =
      egl_api_->eglStreamConsumerGLTextureExternalKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                    EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerReleaseKHR" << "(" << dpy << ", " << stream
                                               << ")");
  EGLBoolean result = egl_api_->eglStreamConsumerReleaseKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglStreamPostD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglStreamPostD3DTextureANGLE"
                 << "(" << dpy << ", " << stream << ", "
                 << static_cast<const void*>(texture) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLBoolean result = egl_api_->eglStreamPostD3DTextureANGLEFn(
      dpy, stream, texture, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglSurfaceAttribFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLint attribute,
                                         EGLint value) {
  GL_SERVICE_LOG("eglSurfaceAttrib" << "(" << dpy << ", " << surface << ", "
                                    << attribute << ", " << value << ")");
  EGLBoolean result =
      egl_api_->eglSurfaceAttribFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) {
  GL_SERVICE_LOG("eglSwapBuffers" << "(" << dpy << ", " << surface << ")");
  EGLBoolean result = egl_api_->eglSwapBuffersFn(dpy, surface);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                                    EGLSurface surface,
                                                    EGLint* rects,
                                                    EGLint n_rects) {
  GL_SERVICE_LOG("eglSwapBuffersWithDamageKHR"
                 << "(" << dpy << ", " << surface << ", "
                 << static_cast<const void*>(rects) << ", " << n_rects << ")");
  EGLBoolean result =
      egl_api_->eglSwapBuffersWithDamageKHRFn(dpy, surface, rects, n_rects);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) {
  GL_SERVICE_LOG("eglSwapInterval" << "(" << dpy << ", " << interval << ")");
  EGLBoolean result = egl_api_->eglSwapIntervalFn(dpy, interval);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglTerminateFn(EGLDisplay dpy) {
  GL_SERVICE_LOG("eglTerminate" << "(" << dpy << ")");
  EGLBoolean result = egl_api_->eglTerminateFn(dpy);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglWaitClientFn(void) {
  GL_SERVICE_LOG("eglWaitClient" << "(" << ")");
  EGLBoolean result = egl_api_->eglWaitClientFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglWaitGLFn(void) {
  GL_SERVICE_LOG("eglWaitGL" << "(" << ")");
  EGLBoolean result = egl_api_->eglWaitGLFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean LogEGLApi::eglWaitNativeFn(EGLint engine) {
  GL_SERVICE_LOG("eglWaitNative" << "(" << engine << ")");
  EGLBoolean result = egl_api_->eglWaitNativeFn(engine);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglWaitSyncFn(EGLDisplay dpy, EGLSync sync, EGLint flags) {
  GL_SERVICE_LOG("eglWaitSync" << "(" << dpy << ", " << sync << ", " << flags
                               << ")");
  EGLint result = egl_api_->eglWaitSyncFn(dpy, sync, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint LogEGLApi::eglWaitSyncKHRFn(EGLDisplay dpy,
                                   EGLSyncKHR sync,
                                   EGLint flags) {
  GL_SERVICE_LOG("eglWaitSyncKHR" << "(" << dpy << ", " << sync << ", " << flags
                                  << ")");
  EGLint result = egl_api_->eglWaitSyncKHRFn(dpy, sync, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogEGLApi::eglWaitUntilWorkScheduledANGLEFn(EGLDisplay dpy) {
  GL_SERVICE_LOG("eglWaitUntilWorkScheduledANGLE" << "(" << dpy << ")");
  egl_api_->eglWaitUntilWorkScheduledANGLEFn(dpy);
}

}  // namespace gl
