// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string>

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
  // Ensure struct has been zero-initialized.
  char* this_bytes = reinterpret_cast<char*>(this);
  DCHECK(this_bytes[0] == 0);
  DCHECK(memcmp(this_bytes, this_bytes + 1, sizeof(*this) - 1) == 0);

  fn.eglBindAPIFn =
      reinterpret_cast<eglBindAPIProc>(GetGLProcAddress("eglBindAPI"));
  fn.eglBindTexImageFn = reinterpret_cast<eglBindTexImageProc>(
      GetGLProcAddress("eglBindTexImage"));
  fn.eglChooseConfigFn = reinterpret_cast<eglChooseConfigProc>(
      GetGLProcAddress("eglChooseConfig"));
  fn.eglClientWaitSyncKHRFn = reinterpret_cast<eglClientWaitSyncKHRProc>(
      GetGLProcAddress("eglClientWaitSyncKHR"));
  fn.eglCopyBuffersFn =
      reinterpret_cast<eglCopyBuffersProc>(GetGLProcAddress("eglCopyBuffers"));
  fn.eglCreateContextFn = reinterpret_cast<eglCreateContextProc>(
      GetGLProcAddress("eglCreateContext"));
  fn.eglCreatePbufferFromClientBufferFn =
      reinterpret_cast<eglCreatePbufferFromClientBufferProc>(
          GetGLProcAddress("eglCreatePbufferFromClientBuffer"));
  fn.eglCreatePbufferSurfaceFn = reinterpret_cast<eglCreatePbufferSurfaceProc>(
      GetGLProcAddress("eglCreatePbufferSurface"));
  fn.eglCreatePixmapSurfaceFn = reinterpret_cast<eglCreatePixmapSurfaceProc>(
      GetGLProcAddress("eglCreatePixmapSurface"));
  fn.eglCreateSyncKHRFn = reinterpret_cast<eglCreateSyncKHRProc>(
      GetGLProcAddress("eglCreateSyncKHR"));
  fn.eglCreateWindowSurfaceFn = reinterpret_cast<eglCreateWindowSurfaceProc>(
      GetGLProcAddress("eglCreateWindowSurface"));
  fn.eglDestroyContextFn = reinterpret_cast<eglDestroyContextProc>(
      GetGLProcAddress("eglDestroyContext"));
  fn.eglDestroySurfaceFn = reinterpret_cast<eglDestroySurfaceProc>(
      GetGLProcAddress("eglDestroySurface"));
  fn.eglDestroySyncKHRFn = reinterpret_cast<eglDestroySyncKHRProc>(
      GetGLProcAddress("eglDestroySyncKHR"));
  fn.eglDupNativeFenceFDANDROIDFn =
      reinterpret_cast<eglDupNativeFenceFDANDROIDProc>(
          GetGLProcAddress("eglDupNativeFenceFDANDROID"));
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
  fn.eglGetPlatformDisplayFn = reinterpret_cast<eglGetPlatformDisplayProc>(
      GetGLProcAddress("eglGetPlatformDisplay"));
  fn.eglGetProcAddressFn = reinterpret_cast<eglGetProcAddressProc>(
      GetGLProcAddress("eglGetProcAddress"));
  fn.eglGetSyncAttribKHRFn = reinterpret_cast<eglGetSyncAttribKHRProc>(
      GetGLProcAddress("eglGetSyncAttribKHR"));
  fn.eglInitializeFn =
      reinterpret_cast<eglInitializeProc>(GetGLProcAddress("eglInitialize"));
  fn.eglMakeCurrentFn =
      reinterpret_cast<eglMakeCurrentProc>(GetGLProcAddress("eglMakeCurrent"));
  fn.eglQueryAPIFn =
      reinterpret_cast<eglQueryAPIProc>(GetGLProcAddress("eglQueryAPI"));
  fn.eglQueryContextFn = reinterpret_cast<eglQueryContextProc>(
      GetGLProcAddress("eglQueryContext"));
  fn.eglQueryStringFn =
      reinterpret_cast<eglQueryStringProc>(GetGLProcAddress("eglQueryString"));
  fn.eglQuerySurfaceFn = reinterpret_cast<eglQuerySurfaceProc>(
      GetGLProcAddress("eglQuerySurface"));
  fn.eglReleaseTexImageFn = reinterpret_cast<eglReleaseTexImageProc>(
      GetGLProcAddress("eglReleaseTexImage"));
  fn.eglReleaseThreadFn = reinterpret_cast<eglReleaseThreadProc>(
      GetGLProcAddress("eglReleaseThread"));
  fn.eglSurfaceAttribFn = reinterpret_cast<eglSurfaceAttribProc>(
      GetGLProcAddress("eglSurfaceAttrib"));
  fn.eglSwapBuffersFn =
      reinterpret_cast<eglSwapBuffersProc>(GetGLProcAddress("eglSwapBuffers"));
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
}

void DriverEGL::InitializeClientExtensionBindings() {
  std::string client_extensions(GetClientExtensions());
  gfx::ExtensionSet extensions(gfx::MakeExtensionSet(client_extensions));
  ALLOW_UNUSED_LOCAL(extensions);

  ext.b_EGL_ANGLE_feature_control =
      gfx::HasExtension(extensions, "EGL_ANGLE_feature_control");
  ext.b_EGL_KHR_debug = gfx::HasExtension(extensions, "EGL_KHR_debug");

  if (ext.b_EGL_KHR_debug) {
    fn.eglDebugMessageControlKHRFn =
        reinterpret_cast<eglDebugMessageControlKHRProc>(
            GetGLProcAddress("eglDebugMessageControlKHR"));
  }

  if (ext.b_EGL_KHR_debug) {
    fn.eglLabelObjectKHRFn = reinterpret_cast<eglLabelObjectKHRProc>(
        GetGLProcAddress("eglLabelObjectKHR"));
  }

  if (ext.b_EGL_KHR_debug) {
    fn.eglQueryDebugKHRFn = reinterpret_cast<eglQueryDebugKHRProc>(
        GetGLProcAddress("eglQueryDebugKHR"));
  }

  if (ext.b_EGL_ANGLE_feature_control) {
    fn.eglQueryDisplayAttribANGLEFn =
        reinterpret_cast<eglQueryDisplayAttribANGLEProc>(
            GetGLProcAddress("eglQueryDisplayAttribANGLE"));
  }

  if (ext.b_EGL_ANGLE_feature_control) {
    fn.eglQueryStringiANGLEFn = reinterpret_cast<eglQueryStringiANGLEProc>(
        GetGLProcAddress("eglQueryStringiANGLE"));
  }
}

void DriverEGL::InitializeExtensionBindings() {
  std::string platform_extensions(GetPlatformExtensions());
  gfx::ExtensionSet extensions(gfx::MakeExtensionSet(platform_extensions));
  ALLOW_UNUSED_LOCAL(extensions);

  ext.b_EGL_ANDROID_blob_cache =
      gfx::HasExtension(extensions, "EGL_ANDROID_blob_cache");
  ext.b_EGL_ANDROID_get_frame_timestamps =
      gfx::HasExtension(extensions, "EGL_ANDROID_get_frame_timestamps");
  ext.b_EGL_ANDROID_get_native_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANDROID_get_native_client_buffer");
  ext.b_EGL_ANDROID_native_fence_sync =
      gfx::HasExtension(extensions, "EGL_ANDROID_native_fence_sync");
  ext.b_EGL_ANGLE_d3d_share_handle_client_buffer =
      gfx::HasExtension(extensions, "EGL_ANGLE_d3d_share_handle_client_buffer");
  ext.b_EGL_ANGLE_query_surface_pointer =
      gfx::HasExtension(extensions, "EGL_ANGLE_query_surface_pointer");
  ext.b_EGL_ANGLE_stream_producer_d3d_texture =
      gfx::HasExtension(extensions, "EGL_ANGLE_stream_producer_d3d_texture");
  ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle = gfx::HasExtension(
      extensions, "EGL_ANGLE_surface_d3d_texture_2d_share_handle");
  ext.b_EGL_CHROMIUM_sync_control =
      gfx::HasExtension(extensions, "EGL_CHROMIUM_sync_control");
  ext.b_EGL_EXT_image_flush_external =
      gfx::HasExtension(extensions, "EGL_EXT_image_flush_external");
  ext.b_EGL_KHR_fence_sync =
      gfx::HasExtension(extensions, "EGL_KHR_fence_sync");
  ext.b_EGL_KHR_gl_texture_2D_image =
      gfx::HasExtension(extensions, "EGL_KHR_gl_texture_2D_image");
  ext.b_EGL_KHR_image = gfx::HasExtension(extensions, "EGL_KHR_image");
  ext.b_EGL_KHR_image_base =
      gfx::HasExtension(extensions, "EGL_KHR_image_base");
  ext.b_EGL_KHR_stream = gfx::HasExtension(extensions, "EGL_KHR_stream");
  ext.b_EGL_KHR_stream_consumer_gltexture =
      gfx::HasExtension(extensions, "EGL_KHR_stream_consumer_gltexture");
  ext.b_EGL_KHR_swap_buffers_with_damage =
      gfx::HasExtension(extensions, "EGL_KHR_swap_buffers_with_damage");
  ext.b_EGL_KHR_wait_sync = gfx::HasExtension(extensions, "EGL_KHR_wait_sync");
  ext.b_EGL_MESA_image_dma_buf_export =
      gfx::HasExtension(extensions, "EGL_MESA_image_dma_buf_export");
  ext.b_EGL_NV_post_sub_buffer =
      gfx::HasExtension(extensions, "EGL_NV_post_sub_buffer");
  ext.b_EGL_NV_stream_consumer_gltexture_yuv =
      gfx::HasExtension(extensions, "EGL_NV_stream_consumer_gltexture_yuv");
  ext.b_GL_CHROMIUM_egl_android_native_fence_sync_hack = gfx::HasExtension(
      extensions, "GL_CHROMIUM_egl_android_native_fence_sync_hack");
  ext.b_GL_CHROMIUM_egl_khr_fence_sync_hack =
      gfx::HasExtension(extensions, "GL_CHROMIUM_egl_khr_fence_sync_hack");

  if (ext.b_EGL_KHR_image || ext.b_EGL_KHR_image_base ||
      ext.b_EGL_KHR_gl_texture_2D_image) {
    fn.eglCreateImageKHRFn = reinterpret_cast<eglCreateImageKHRProc>(
        GetGLProcAddress("eglCreateImageKHR"));
  }

  if (ext.b_EGL_KHR_stream) {
    fn.eglCreateStreamKHRFn = reinterpret_cast<eglCreateStreamKHRProc>(
        GetGLProcAddress("eglCreateStreamKHR"));
  }

  if (ext.b_EGL_ANGLE_stream_producer_d3d_texture) {
    fn.eglCreateStreamProducerD3DTextureANGLEFn =
        reinterpret_cast<eglCreateStreamProducerD3DTextureANGLEProc>(
            GetGLProcAddress("eglCreateStreamProducerD3DTextureANGLE"));
  }

  if (ext.b_EGL_KHR_image || ext.b_EGL_KHR_image_base) {
    fn.eglDestroyImageKHRFn = reinterpret_cast<eglDestroyImageKHRProc>(
        GetGLProcAddress("eglDestroyImageKHR"));
  }

  if (ext.b_EGL_KHR_stream) {
    fn.eglDestroyStreamKHRFn = reinterpret_cast<eglDestroyStreamKHRProc>(
        GetGLProcAddress("eglDestroyStreamKHR"));
  }

  if (ext.b_EGL_MESA_image_dma_buf_export) {
    fn.eglExportDMABUFImageMESAFn =
        reinterpret_cast<eglExportDMABUFImageMESAProc>(
            GetGLProcAddress("eglExportDMABUFImageMESA"));
  }

  if (ext.b_EGL_MESA_image_dma_buf_export) {
    fn.eglExportDMABUFImageQueryMESAFn =
        reinterpret_cast<eglExportDMABUFImageQueryMESAProc>(
            GetGLProcAddress("eglExportDMABUFImageQueryMESA"));
  }

  if (ext.b_EGL_ANDROID_get_frame_timestamps) {
    fn.eglGetCompositorTimingANDROIDFn =
        reinterpret_cast<eglGetCompositorTimingANDROIDProc>(
            GetGLProcAddress("eglGetCompositorTimingANDROID"));
  }

  if (ext.b_EGL_ANDROID_get_frame_timestamps) {
    fn.eglGetCompositorTimingSupportedANDROIDFn =
        reinterpret_cast<eglGetCompositorTimingSupportedANDROIDProc>(
            GetGLProcAddress("eglGetCompositorTimingSupportedANDROID"));
  }

  if (ext.b_EGL_ANDROID_get_frame_timestamps) {
    fn.eglGetFrameTimestampsANDROIDFn =
        reinterpret_cast<eglGetFrameTimestampsANDROIDProc>(
            GetGLProcAddress("eglGetFrameTimestampsANDROID"));
  }

  if (ext.b_EGL_ANDROID_get_frame_timestamps) {
    fn.eglGetFrameTimestampSupportedANDROIDFn =
        reinterpret_cast<eglGetFrameTimestampSupportedANDROIDProc>(
            GetGLProcAddress("eglGetFrameTimestampSupportedANDROID"));
  }

  if (ext.b_EGL_ANDROID_get_native_client_buffer) {
    fn.eglGetNativeClientBufferANDROIDFn =
        reinterpret_cast<eglGetNativeClientBufferANDROIDProc>(
            GetGLProcAddress("eglGetNativeClientBufferANDROID"));
  }

  if (ext.b_EGL_ANDROID_get_frame_timestamps) {
    fn.eglGetNextFrameIdANDROIDFn =
        reinterpret_cast<eglGetNextFrameIdANDROIDProc>(
            GetGLProcAddress("eglGetNextFrameIdANDROID"));
  }

  if (ext.b_EGL_CHROMIUM_sync_control) {
    fn.eglGetSyncValuesCHROMIUMFn =
        reinterpret_cast<eglGetSyncValuesCHROMIUMProc>(
            GetGLProcAddress("eglGetSyncValuesCHROMIUM"));
  }

  if (ext.b_EGL_EXT_image_flush_external) {
    fn.eglImageFlushExternalEXTFn =
        reinterpret_cast<eglImageFlushExternalEXTProc>(
            GetGLProcAddress("eglImageFlushExternalEXT"));
  }

  if (ext.b_EGL_NV_post_sub_buffer) {
    fn.eglPostSubBufferNVFn = reinterpret_cast<eglPostSubBufferNVProc>(
        GetGLProcAddress("eglPostSubBufferNV"));
  }

  if (ext.b_EGL_KHR_stream) {
    fn.eglQueryStreamKHRFn = reinterpret_cast<eglQueryStreamKHRProc>(
        GetGLProcAddress("eglQueryStreamKHR"));
  }

  if (ext.b_EGL_KHR_stream) {
    fn.eglQueryStreamu64KHRFn = reinterpret_cast<eglQueryStreamu64KHRProc>(
        GetGLProcAddress("eglQueryStreamu64KHR"));
  }

  if (ext.b_EGL_ANGLE_query_surface_pointer) {
    fn.eglQuerySurfacePointerANGLEFn =
        reinterpret_cast<eglQuerySurfacePointerANGLEProc>(
            GetGLProcAddress("eglQuerySurfacePointerANGLE"));
  }

  if (ext.b_EGL_ANDROID_blob_cache) {
    fn.eglSetBlobCacheFuncsANDROIDFn =
        reinterpret_cast<eglSetBlobCacheFuncsANDROIDProc>(
            GetGLProcAddress("eglSetBlobCacheFuncsANDROID"));
  }

  if (ext.b_EGL_KHR_stream) {
    fn.eglStreamAttribKHRFn = reinterpret_cast<eglStreamAttribKHRProc>(
        GetGLProcAddress("eglStreamAttribKHR"));
  }

  if (ext.b_EGL_KHR_stream_consumer_gltexture) {
    fn.eglStreamConsumerAcquireKHRFn =
        reinterpret_cast<eglStreamConsumerAcquireKHRProc>(
            GetGLProcAddress("eglStreamConsumerAcquireKHR"));
  }

  if (ext.b_EGL_NV_stream_consumer_gltexture_yuv) {
    fn.eglStreamConsumerGLTextureExternalAttribsNVFn =
        reinterpret_cast<eglStreamConsumerGLTextureExternalAttribsNVProc>(
            GetGLProcAddress("eglStreamConsumerGLTextureExternalAttribsNV"));
  }

  if (ext.b_EGL_KHR_stream_consumer_gltexture) {
    fn.eglStreamConsumerGLTextureExternalKHRFn =
        reinterpret_cast<eglStreamConsumerGLTextureExternalKHRProc>(
            GetGLProcAddress("eglStreamConsumerGLTextureExternalKHR"));
  }

  if (ext.b_EGL_KHR_stream_consumer_gltexture) {
    fn.eglStreamConsumerReleaseKHRFn =
        reinterpret_cast<eglStreamConsumerReleaseKHRProc>(
            GetGLProcAddress("eglStreamConsumerReleaseKHR"));
  }

  if (ext.b_EGL_ANGLE_stream_producer_d3d_texture) {
    fn.eglStreamPostD3DTextureANGLEFn =
        reinterpret_cast<eglStreamPostD3DTextureANGLEProc>(
            GetGLProcAddress("eglStreamPostD3DTextureANGLE"));
  }

  if (ext.b_EGL_KHR_swap_buffers_with_damage) {
    fn.eglSwapBuffersWithDamageKHRFn =
        reinterpret_cast<eglSwapBuffersWithDamageKHRProc>(
            GetGLProcAddress("eglSwapBuffersWithDamageKHR"));
  }

  if (ext.b_EGL_KHR_wait_sync) {
    fn.eglWaitSyncKHRFn = reinterpret_cast<eglWaitSyncKHRProc>(
        GetGLProcAddress("eglWaitSyncKHR"));
  }
}

void DriverEGL::ClearBindings() {
  memset(this, 0, sizeof(*this));
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

EGLContext EGLApiBase::eglCreateContextFn(EGLDisplay dpy,
                                          EGLConfig config,
                                          EGLContext share_context,
                                          const EGLint* attrib_list) {
  return driver_->fn.eglCreateContextFn(dpy, config, share_context,
                                        attrib_list);
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

EGLBoolean EGLApiBase::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                    EGLint attribute,
                                                    EGLAttrib* value) {
  return driver_->fn.eglQueryDisplayAttribANGLEFn(dpy, attribute, value);
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

EGLint EGLApiBase::eglWaitSyncKHRFn(EGLDisplay dpy,
                                    EGLSyncKHR sync,
                                    EGLint flags) {
  return driver_->fn.eglWaitSyncKHRFn(dpy, sync, flags);
}

EGLBoolean TraceEGLApi::eglBindAPIFn(EGLenum api) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglBindAPI")
  return egl_api_->eglBindAPIFn(api);
}

EGLBoolean TraceEGLApi::eglBindTexImageFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglBindTexImage")
  return egl_api_->eglBindTexImageFn(dpy, surface, buffer);
}

EGLBoolean TraceEGLApi::eglChooseConfigFn(EGLDisplay dpy,
                                          const EGLint* attrib_list,
                                          EGLConfig* configs,
                                          EGLint config_size,
                                          EGLint* num_config) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglChooseConfig")
  return egl_api_->eglChooseConfigFn(dpy, attrib_list, configs, config_size,
                                     num_config);
}

EGLint TraceEGLApi::eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                           EGLSyncKHR sync,
                                           EGLint flags,
                                           EGLTimeKHR timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglClientWaitSyncKHR")
  return egl_api_->eglClientWaitSyncKHRFn(dpy, sync, flags, timeout);
}

EGLBoolean TraceEGLApi::eglCopyBuffersFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLNativePixmapType target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCopyBuffers")
  return egl_api_->eglCopyBuffersFn(dpy, surface, target);
}

EGLContext TraceEGLApi::eglCreateContextFn(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLContext share_context,
                                           const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreateContext")
  return egl_api_->eglCreateContextFn(dpy, config, share_context, attrib_list);
}

EGLImageKHR TraceEGLApi::eglCreateImageKHRFn(EGLDisplay dpy,
                                             EGLContext ctx,
                                             EGLenum target,
                                             EGLClientBuffer buffer,
                                             const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreateImageKHR")
  return egl_api_->eglCreateImageKHRFn(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePbufferFromClientBufferFn(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglCreatePbufferFromClientBuffer")
  return egl_api_->eglCreatePbufferFromClientBufferFn(dpy, buftype, buffer,
                                                      config, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePbufferSurfaceFn(EGLDisplay dpy,
                                                  EGLConfig config,
                                                  const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreatePbufferSurface")
  return egl_api_->eglCreatePbufferSurfaceFn(dpy, config, attrib_list);
}

EGLSurface TraceEGLApi::eglCreatePixmapSurfaceFn(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativePixmapType pixmap,
                                                 const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreatePixmapSurface")
  return egl_api_->eglCreatePixmapSurfaceFn(dpy, config, pixmap, attrib_list);
}

EGLStreamKHR TraceEGLApi::eglCreateStreamKHRFn(EGLDisplay dpy,
                                               const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreateStreamKHR")
  return egl_api_->eglCreateStreamKHRFn(dpy, attrib_list);
}

EGLBoolean TraceEGLApi::eglCreateStreamProducerD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::eglCreateStreamProducerD3DTextureANGLE")
  return egl_api_->eglCreateStreamProducerD3DTextureANGLEFn(dpy, stream,
                                                            attrib_list);
}

EGLSyncKHR TraceEGLApi::eglCreateSyncKHRFn(EGLDisplay dpy,
                                           EGLenum type,
                                           const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreateSyncKHR")
  return egl_api_->eglCreateSyncKHRFn(dpy, type, attrib_list);
}

EGLSurface TraceEGLApi::eglCreateWindowSurfaceFn(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLNativeWindowType win,
                                                 const EGLint* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglCreateWindowSurface")
  return egl_api_->eglCreateWindowSurfaceFn(dpy, config, win, attrib_list);
}

EGLint TraceEGLApi::eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                                const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDebugMessageControlKHR")
  return egl_api_->eglDebugMessageControlKHRFn(callback, attrib_list);
}

EGLBoolean TraceEGLApi::eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDestroyContext")
  return egl_api_->eglDestroyContextFn(dpy, ctx);
}

EGLBoolean TraceEGLApi::eglDestroyImageKHRFn(EGLDisplay dpy,
                                             EGLImageKHR image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDestroyImageKHR")
  return egl_api_->eglDestroyImageKHRFn(dpy, image);
}

EGLBoolean TraceEGLApi::eglDestroyStreamKHRFn(EGLDisplay dpy,
                                              EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDestroyStreamKHR")
  return egl_api_->eglDestroyStreamKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglDestroySurfaceFn(EGLDisplay dpy,
                                            EGLSurface surface) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDestroySurface")
  return egl_api_->eglDestroySurfaceFn(dpy, surface);
}

EGLBoolean TraceEGLApi::eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDestroySyncKHR")
  return egl_api_->eglDestroySyncKHRFn(dpy, sync);
}

EGLint TraceEGLApi::eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                                 EGLSyncKHR sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglDupNativeFenceFDANDROID")
  return egl_api_->eglDupNativeFenceFDANDROIDFn(dpy, sync);
}

EGLBoolean TraceEGLApi::eglExportDMABUFImageMESAFn(EGLDisplay dpy,
                                                   EGLImageKHR image,
                                                   int* fds,
                                                   EGLint* strides,
                                                   EGLint* offsets) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglExportDMABUFImageMESA")
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
                                "TraceGLAPI::eglExportDMABUFImageQueryMESA")
  return egl_api_->eglExportDMABUFImageQueryMESAFn(dpy, image, fourcc,
                                                   num_planes, modifiers);
}

EGLBoolean TraceEGLApi::eglGetCompositorTimingANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint numTimestamps,
    EGLint* names,
    EGLnsecsANDROID* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglGetCompositorTimingANDROID")
  return egl_api_->eglGetCompositorTimingANDROIDFn(dpy, surface, numTimestamps,
                                                   names, values);
}

EGLBoolean TraceEGLApi::eglGetCompositorTimingSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::eglGetCompositorTimingSupportedANDROID")
  return egl_api_->eglGetCompositorTimingSupportedANDROIDFn(dpy, surface,
                                                            timestamp);
}

EGLBoolean TraceEGLApi::eglGetConfigAttribFn(EGLDisplay dpy,
                                             EGLConfig config,
                                             EGLint attribute,
                                             EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetConfigAttrib")
  return egl_api_->eglGetConfigAttribFn(dpy, config, attribute, value);
}

EGLBoolean TraceEGLApi::eglGetConfigsFn(EGLDisplay dpy,
                                        EGLConfig* configs,
                                        EGLint config_size,
                                        EGLint* num_config) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetConfigs")
  return egl_api_->eglGetConfigsFn(dpy, configs, config_size, num_config);
}

EGLContext TraceEGLApi::eglGetCurrentContextFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetCurrentContext")
  return egl_api_->eglGetCurrentContextFn();
}

EGLDisplay TraceEGLApi::eglGetCurrentDisplayFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetCurrentDisplay")
  return egl_api_->eglGetCurrentDisplayFn();
}

EGLSurface TraceEGLApi::eglGetCurrentSurfaceFn(EGLint readdraw) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetCurrentSurface")
  return egl_api_->eglGetCurrentSurfaceFn(readdraw);
}

EGLDisplay TraceEGLApi::eglGetDisplayFn(EGLNativeDisplayType display_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetDisplay")
  return egl_api_->eglGetDisplayFn(display_id);
}

EGLint TraceEGLApi::eglGetErrorFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetError")
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
                                "TraceGLAPI::eglGetFrameTimestampsANDROID")
  return egl_api_->eglGetFrameTimestampsANDROIDFn(
      dpy, surface, frameId, numTimestamps, timestamps, values);
}

EGLBoolean TraceEGLApi::eglGetFrameTimestampSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::eglGetFrameTimestampSupportedANDROID")
  return egl_api_->eglGetFrameTimestampSupportedANDROIDFn(dpy, surface,
                                                          timestamp);
}

EGLClientBuffer TraceEGLApi::eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglGetNativeClientBufferANDROID")
  return egl_api_->eglGetNativeClientBufferANDROIDFn(ahardwarebuffer);
}

EGLBoolean TraceEGLApi::eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLuint64KHR* frameId) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetNextFrameIdANDROID")
  return egl_api_->eglGetNextFrameIdANDROIDFn(dpy, surface, frameId);
}

EGLDisplay TraceEGLApi::eglGetPlatformDisplayFn(EGLenum platform,
                                                void* native_display,
                                                const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetPlatformDisplay")
  return egl_api_->eglGetPlatformDisplayFn(platform, native_display,
                                           attrib_list);
}

__eglMustCastToProperFunctionPointerType TraceEGLApi::eglGetProcAddressFn(
    const char* procname) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetProcAddress")
  return egl_api_->eglGetProcAddressFn(procname);
}

EGLBoolean TraceEGLApi::eglGetSyncAttribKHRFn(EGLDisplay dpy,
                                              EGLSyncKHR sync,
                                              EGLint attribute,
                                              EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetSyncAttribKHR")
  return egl_api_->eglGetSyncAttribKHRFn(dpy, sync, attribute, value);
}

EGLBoolean TraceEGLApi::eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLuint64CHROMIUM* ust,
                                                   EGLuint64CHROMIUM* msc,
                                                   EGLuint64CHROMIUM* sbc) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglGetSyncValuesCHROMIUM")
  return egl_api_->eglGetSyncValuesCHROMIUMFn(dpy, surface, ust, msc, sbc);
}

EGLBoolean TraceEGLApi::eglImageFlushExternalEXTFn(
    EGLDisplay dpy,
    EGLImageKHR image,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglImageFlushExternalEXT")
  return egl_api_->eglImageFlushExternalEXTFn(dpy, image, attrib_list);
}

EGLBoolean TraceEGLApi::eglInitializeFn(EGLDisplay dpy,
                                        EGLint* major,
                                        EGLint* minor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglInitialize")
  return egl_api_->eglInitializeFn(dpy, major, minor);
}

EGLint TraceEGLApi::eglLabelObjectKHRFn(EGLDisplay display,
                                        EGLenum objectType,
                                        EGLObjectKHR object,
                                        EGLLabelKHR label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglLabelObjectKHR")
  return egl_api_->eglLabelObjectKHRFn(display, objectType, object, label);
}

EGLBoolean TraceEGLApi::eglMakeCurrentFn(EGLDisplay dpy,
                                         EGLSurface draw,
                                         EGLSurface read,
                                         EGLContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglMakeCurrent")
  return egl_api_->eglMakeCurrentFn(dpy, draw, read, ctx);
}

EGLBoolean TraceEGLApi::eglPostSubBufferNVFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint x,
                                             EGLint y,
                                             EGLint width,
                                             EGLint height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglPostSubBufferNV")
  return egl_api_->eglPostSubBufferNVFn(dpy, surface, x, y, width, height);
}

EGLenum TraceEGLApi::eglQueryAPIFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryAPI")
  return egl_api_->eglQueryAPIFn();
}

EGLBoolean TraceEGLApi::eglQueryContextFn(EGLDisplay dpy,
                                          EGLContext ctx,
                                          EGLint attribute,
                                          EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryContext")
  return egl_api_->eglQueryContextFn(dpy, ctx, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryDebugKHR")
  return egl_api_->eglQueryDebugKHRFn(attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
                                                     EGLint attribute,
                                                     EGLAttrib* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryDisplayAttribANGLE")
  return egl_api_->eglQueryDisplayAttribANGLEFn(dpy, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryStreamKHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryStreamKHR")
  return egl_api_->eglQueryStreamKHRFn(dpy, stream, attribute, value);
}

EGLBoolean TraceEGLApi::eglQueryStreamu64KHRFn(EGLDisplay dpy,
                                               EGLStreamKHR stream,
                                               EGLenum attribute,
                                               EGLuint64KHR* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryStreamu64KHR")
  return egl_api_->eglQueryStreamu64KHRFn(dpy, stream, attribute, value);
}

const char* TraceEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryString")
  return egl_api_->eglQueryStringFn(dpy, name);
}

const char* TraceEGLApi::eglQueryStringiANGLEFn(EGLDisplay dpy,
                                                EGLint name,
                                                EGLint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQueryStringiANGLE")
  return egl_api_->eglQueryStringiANGLEFn(dpy, name, index);
}

EGLBoolean TraceEGLApi::eglQuerySurfaceFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint attribute,
                                          EGLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglQuerySurface")
  return egl_api_->eglQuerySurfaceFn(dpy, surface, attribute, value);
}

EGLBoolean TraceEGLApi::eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint attribute,
                                                      void** value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglQuerySurfacePointerANGLE")
  return egl_api_->eglQuerySurfacePointerANGLEFn(dpy, surface, attribute,
                                                 value);
}

EGLBoolean TraceEGLApi::eglReleaseTexImageFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglReleaseTexImage")
  return egl_api_->eglReleaseTexImageFn(dpy, surface, buffer);
}

EGLBoolean TraceEGLApi::eglReleaseThreadFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglReleaseThread")
  return egl_api_->eglReleaseThreadFn();
}

void TraceEGLApi::eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                                EGLSetBlobFuncANDROID set,
                                                EGLGetBlobFuncANDROID get) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglSetBlobCacheFuncsANDROID")
  egl_api_->eglSetBlobCacheFuncsANDROIDFn(dpy, set, get);
}

EGLBoolean TraceEGLApi::eglStreamAttribKHRFn(EGLDisplay dpy,
                                             EGLStreamKHR stream,
                                             EGLenum attribute,
                                             EGLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglStreamAttribKHR")
  return egl_api_->eglStreamAttribKHRFn(dpy, stream, attribute, value);
}

EGLBoolean TraceEGLApi::eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglStreamConsumerAcquireKHR")
  return egl_api_->eglStreamConsumerAcquireKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamConsumerGLTextureExternalAttribsNVFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::eglStreamConsumerGLTextureExternalAttribsNV")
  return egl_api_->eglStreamConsumerGLTextureExternalAttribsNVFn(dpy, stream,
                                                                 attrib_list);
}

EGLBoolean TraceEGLApi::eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::eglStreamConsumerGLTextureExternalKHR")
  return egl_api_->eglStreamConsumerGLTextureExternalKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglStreamConsumerReleaseKHR")
  return egl_api_->eglStreamConsumerReleaseKHRFn(dpy, stream);
}

EGLBoolean TraceEGLApi::eglStreamPostD3DTextureANGLEFn(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglStreamPostD3DTextureANGLE")
  return egl_api_->eglStreamPostD3DTextureANGLEFn(dpy, stream, texture,
                                                  attrib_list);
}

EGLBoolean TraceEGLApi::eglSurfaceAttribFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint attribute,
                                           EGLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglSurfaceAttrib")
  return egl_api_->eglSurfaceAttribFn(dpy, surface, attribute, value);
}

EGLBoolean TraceEGLApi::eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglSwapBuffers")
  return egl_api_->eglSwapBuffersFn(dpy, surface);
}

EGLBoolean TraceEGLApi::eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint* rects,
                                                      EGLint n_rects) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::eglSwapBuffersWithDamageKHR")
  return egl_api_->eglSwapBuffersWithDamageKHRFn(dpy, surface, rects, n_rects);
}

EGLBoolean TraceEGLApi::eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglSwapInterval")
  return egl_api_->eglSwapIntervalFn(dpy, interval);
}

EGLBoolean TraceEGLApi::eglTerminateFn(EGLDisplay dpy) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglTerminate")
  return egl_api_->eglTerminateFn(dpy);
}

EGLBoolean TraceEGLApi::eglWaitClientFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglWaitClient")
  return egl_api_->eglWaitClientFn();
}

EGLBoolean TraceEGLApi::eglWaitGLFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglWaitGL")
  return egl_api_->eglWaitGLFn();
}

EGLBoolean TraceEGLApi::eglWaitNativeFn(EGLint engine) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglWaitNative")
  return egl_api_->eglWaitNativeFn(engine);
}

EGLint TraceEGLApi::eglWaitSyncKHRFn(EGLDisplay dpy,
                                     EGLSyncKHR sync,
                                     EGLint flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::eglWaitSyncKHR")
  return egl_api_->eglWaitSyncKHRFn(dpy, sync, flags);
}

EGLBoolean DebugEGLApi::eglBindAPIFn(EGLenum api) {
  GL_SERVICE_LOG("eglBindAPI"
                 << "(" << api << ")");
  EGLBoolean result = egl_api_->eglBindAPIFn(api);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglBindTexImageFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint buffer) {
  GL_SERVICE_LOG("eglBindTexImage"
                 << "(" << dpy << ", " << surface << ", " << buffer << ")");
  EGLBoolean result = egl_api_->eglBindTexImageFn(dpy, surface, buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglChooseConfigFn(EGLDisplay dpy,
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

EGLint DebugEGLApi::eglClientWaitSyncKHRFn(EGLDisplay dpy,
                                           EGLSyncKHR sync,
                                           EGLint flags,
                                           EGLTimeKHR timeout) {
  GL_SERVICE_LOG("eglClientWaitSyncKHR"
                 << "(" << dpy << ", " << sync << ", " << flags << ", "
                 << timeout << ")");
  EGLint result = egl_api_->eglClientWaitSyncKHRFn(dpy, sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglCopyBuffersFn(EGLDisplay dpy,
                                         EGLSurface surface,
                                         EGLNativePixmapType target) {
  GL_SERVICE_LOG("eglCopyBuffers"
                 << "(" << dpy << ", " << surface << ", " << target << ")");
  EGLBoolean result = egl_api_->eglCopyBuffersFn(dpy, surface, target);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLContext DebugEGLApi::eglCreateContextFn(EGLDisplay dpy,
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

EGLImageKHR DebugEGLApi::eglCreateImageKHRFn(EGLDisplay dpy,
                                             EGLContext ctx,
                                             EGLenum target,
                                             EGLClientBuffer buffer,
                                             const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateImageKHR"
                 << "(" << dpy << ", " << ctx << ", " << target << ", "
                 << buffer << ", " << static_cast<const void*>(attrib_list)
                 << ")");
  EGLImageKHR result =
      egl_api_->eglCreateImageKHRFn(dpy, ctx, target, buffer, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface DebugEGLApi::eglCreatePbufferFromClientBufferFn(
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

EGLSurface DebugEGLApi::eglCreatePbufferSurfaceFn(EGLDisplay dpy,
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

EGLSurface DebugEGLApi::eglCreatePixmapSurfaceFn(EGLDisplay dpy,
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

EGLStreamKHR DebugEGLApi::eglCreateStreamKHRFn(EGLDisplay dpy,
                                               const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateStreamKHR"
                 << "(" << dpy << ", " << static_cast<const void*>(attrib_list)
                 << ")");
  EGLStreamKHR result = egl_api_->eglCreateStreamKHRFn(dpy, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglCreateStreamProducerD3DTextureANGLEFn(
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

EGLSyncKHR DebugEGLApi::eglCreateSyncKHRFn(EGLDisplay dpy,
                                           EGLenum type,
                                           const EGLint* attrib_list) {
  GL_SERVICE_LOG("eglCreateSyncKHR"
                 << "(" << dpy << ", " << type << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLSyncKHR result = egl_api_->eglCreateSyncKHRFn(dpy, type, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface DebugEGLApi::eglCreateWindowSurfaceFn(EGLDisplay dpy,
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

EGLint DebugEGLApi::eglDebugMessageControlKHRFn(EGLDEBUGPROCKHR callback,
                                                const EGLAttrib* attrib_list) {
  GL_SERVICE_LOG("eglDebugMessageControlKHR"
                 << "(" << reinterpret_cast<void*>(callback) << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  EGLint result = egl_api_->eglDebugMessageControlKHRFn(callback, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglDestroyContextFn(EGLDisplay dpy, EGLContext ctx) {
  GL_SERVICE_LOG("eglDestroyContext"
                 << "(" << dpy << ", " << ctx << ")");
  EGLBoolean result = egl_api_->eglDestroyContextFn(dpy, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglDestroyImageKHRFn(EGLDisplay dpy,
                                             EGLImageKHR image) {
  GL_SERVICE_LOG("eglDestroyImageKHR"
                 << "(" << dpy << ", " << image << ")");
  EGLBoolean result = egl_api_->eglDestroyImageKHRFn(dpy, image);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglDestroyStreamKHRFn(EGLDisplay dpy,
                                              EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglDestroyStreamKHR"
                 << "(" << dpy << ", " << stream << ")");
  EGLBoolean result = egl_api_->eglDestroyStreamKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglDestroySurfaceFn(EGLDisplay dpy,
                                            EGLSurface surface) {
  GL_SERVICE_LOG("eglDestroySurface"
                 << "(" << dpy << ", " << surface << ")");
  EGLBoolean result = egl_api_->eglDestroySurfaceFn(dpy, surface);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglDestroySyncKHRFn(EGLDisplay dpy, EGLSyncKHR sync) {
  GL_SERVICE_LOG("eglDestroySyncKHR"
                 << "(" << dpy << ", " << sync << ")");
  EGLBoolean result = egl_api_->eglDestroySyncKHRFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint DebugEGLApi::eglDupNativeFenceFDANDROIDFn(EGLDisplay dpy,
                                                 EGLSyncKHR sync) {
  GL_SERVICE_LOG("eglDupNativeFenceFDANDROID"
                 << "(" << dpy << ", " << sync << ")");
  EGLint result = egl_api_->eglDupNativeFenceFDANDROIDFn(dpy, sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglExportDMABUFImageMESAFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglExportDMABUFImageQueryMESAFn(
    EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglGetCompositorTimingANDROIDFn(
    EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglGetCompositorTimingSupportedANDROIDFn(
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

EGLBoolean DebugEGLApi::eglGetConfigAttribFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglGetConfigsFn(EGLDisplay dpy,
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

EGLContext DebugEGLApi::eglGetCurrentContextFn(void) {
  GL_SERVICE_LOG("eglGetCurrentContext"
                 << "("
                 << ")");
  EGLContext result = egl_api_->eglGetCurrentContextFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLDisplay DebugEGLApi::eglGetCurrentDisplayFn(void) {
  GL_SERVICE_LOG("eglGetCurrentDisplay"
                 << "("
                 << ")");
  EGLDisplay result = egl_api_->eglGetCurrentDisplayFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLSurface DebugEGLApi::eglGetCurrentSurfaceFn(EGLint readdraw) {
  GL_SERVICE_LOG("eglGetCurrentSurface"
                 << "(" << readdraw << ")");
  EGLSurface result = egl_api_->eglGetCurrentSurfaceFn(readdraw);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLDisplay DebugEGLApi::eglGetDisplayFn(EGLNativeDisplayType display_id) {
  GL_SERVICE_LOG("eglGetDisplay"
                 << "(" << display_id << ")");
  EGLDisplay result = egl_api_->eglGetDisplayFn(display_id);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint DebugEGLApi::eglGetErrorFn(void) {
  GL_SERVICE_LOG("eglGetError"
                 << "("
                 << ")");
  EGLint result = egl_api_->eglGetErrorFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglGetFrameTimestampsANDROIDFn(
    EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglGetFrameTimestampSupportedANDROIDFn(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  GL_SERVICE_LOG("eglGetFrameTimestampSupportedANDROID"
                 << "(" << dpy << ", " << surface << ", " << timestamp << ")");
  EGLBoolean result =
      egl_api_->eglGetFrameTimestampSupportedANDROIDFn(dpy, surface, timestamp);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLClientBuffer DebugEGLApi::eglGetNativeClientBufferANDROIDFn(
    const struct AHardwareBuffer* ahardwarebuffer) {
  GL_SERVICE_LOG("eglGetNativeClientBufferANDROID"
                 << "(" << static_cast<const void*>(ahardwarebuffer) << ")");
  EGLClientBuffer result =
      egl_api_->eglGetNativeClientBufferANDROIDFn(ahardwarebuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglGetNextFrameIdANDROIDFn(EGLDisplay dpy,
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

EGLDisplay DebugEGLApi::eglGetPlatformDisplayFn(EGLenum platform,
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

__eglMustCastToProperFunctionPointerType DebugEGLApi::eglGetProcAddressFn(
    const char* procname) {
  GL_SERVICE_LOG("eglGetProcAddress"
                 << "(" << procname << ")");
  __eglMustCastToProperFunctionPointerType result =
      egl_api_->eglGetProcAddressFn(procname);

  GL_SERVICE_LOG("GL_RESULT: " << reinterpret_cast<void*>(result));

  return result;
}

EGLBoolean DebugEGLApi::eglGetSyncAttribKHRFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglGetSyncValuesCHROMIUMFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglImageFlushExternalEXTFn(
    EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglInitializeFn(EGLDisplay dpy,
                                        EGLint* major,
                                        EGLint* minor) {
  GL_SERVICE_LOG("eglInitialize"
                 << "(" << dpy << ", " << static_cast<const void*>(major)
                 << ", " << static_cast<const void*>(minor) << ")");
  EGLBoolean result = egl_api_->eglInitializeFn(dpy, major, minor);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint DebugEGLApi::eglLabelObjectKHRFn(EGLDisplay display,
                                        EGLenum objectType,
                                        EGLObjectKHR object,
                                        EGLLabelKHR label) {
  GL_SERVICE_LOG("eglLabelObjectKHR"
                 << "(" << display << ", " << objectType << ", " << object
                 << ", " << label << ")");
  EGLint result =
      egl_api_->eglLabelObjectKHRFn(display, objectType, object, label);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglMakeCurrentFn(EGLDisplay dpy,
                                         EGLSurface draw,
                                         EGLSurface read,
                                         EGLContext ctx) {
  GL_SERVICE_LOG("eglMakeCurrent"
                 << "(" << dpy << ", " << draw << ", " << read << ", " << ctx
                 << ")");
  EGLBoolean result = egl_api_->eglMakeCurrentFn(dpy, draw, read, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglPostSubBufferNVFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint x,
                                             EGLint y,
                                             EGLint width,
                                             EGLint height) {
  GL_SERVICE_LOG("eglPostSubBufferNV"
                 << "(" << dpy << ", " << surface << ", " << x << ", " << y
                 << ", " << width << ", " << height << ")");
  EGLBoolean result =
      egl_api_->eglPostSubBufferNVFn(dpy, surface, x, y, width, height);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLenum DebugEGLApi::eglQueryAPIFn(void) {
  GL_SERVICE_LOG("eglQueryAPI"
                 << "("
                 << ")");
  EGLenum result = egl_api_->eglQueryAPIFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQueryContextFn(EGLDisplay dpy,
                                          EGLContext ctx,
                                          EGLint attribute,
                                          EGLint* value) {
  GL_SERVICE_LOG("eglQueryContext"
                 << "(" << dpy << ", " << ctx << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result = egl_api_->eglQueryContextFn(dpy, ctx, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQueryDebugKHRFn(EGLint attribute, EGLAttrib* value) {
  GL_SERVICE_LOG("eglQueryDebugKHR"
                 << "(" << attribute << ", " << static_cast<const void*>(value)
                 << ")");
  EGLBoolean result = egl_api_->eglQueryDebugKHRFn(attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQueryDisplayAttribANGLEFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglQueryStreamKHRFn(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLint* value) {
  GL_SERVICE_LOG("eglQueryStreamKHR"
                 << "(" << dpy << ", " << stream << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQueryStreamKHRFn(dpy, stream, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQueryStreamu64KHRFn(EGLDisplay dpy,
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

const char* DebugEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  GL_SERVICE_LOG("eglQueryString"
                 << "(" << dpy << ", " << name << ")");
  const char* result = egl_api_->eglQueryStringFn(dpy, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* DebugEGLApi::eglQueryStringiANGLEFn(EGLDisplay dpy,
                                                EGLint name,
                                                EGLint index) {
  GL_SERVICE_LOG("eglQueryStringiANGLE"
                 << "(" << dpy << ", " << name << ", " << index << ")");
  const char* result = egl_api_->eglQueryStringiANGLEFn(dpy, name, index);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQuerySurfaceFn(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint attribute,
                                          EGLint* value) {
  GL_SERVICE_LOG("eglQuerySurface"
                 << "(" << dpy << ", " << surface << ", " << attribute << ", "
                 << static_cast<const void*>(value) << ")");
  EGLBoolean result =
      egl_api_->eglQuerySurfaceFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglQuerySurfacePointerANGLEFn(EGLDisplay dpy,
                                                      EGLSurface surface,
                                                      EGLint attribute,
                                                      void** value) {
  GL_SERVICE_LOG("eglQuerySurfacePointerANGLE"
                 << "(" << dpy << ", " << surface << ", " << attribute << ", "
                 << value << ")");
  EGLBoolean result =
      egl_api_->eglQuerySurfacePointerANGLEFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglReleaseTexImageFn(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLint buffer) {
  GL_SERVICE_LOG("eglReleaseTexImage"
                 << "(" << dpy << ", " << surface << ", " << buffer << ")");
  EGLBoolean result = egl_api_->eglReleaseTexImageFn(dpy, surface, buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglReleaseThreadFn(void) {
  GL_SERVICE_LOG("eglReleaseThread"
                 << "("
                 << ")");
  EGLBoolean result = egl_api_->eglReleaseThreadFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugEGLApi::eglSetBlobCacheFuncsANDROIDFn(EGLDisplay dpy,
                                                EGLSetBlobFuncANDROID set,
                                                EGLGetBlobFuncANDROID get) {
  GL_SERVICE_LOG("eglSetBlobCacheFuncsANDROID"
                 << "(" << dpy << ", " << reinterpret_cast<const void*>(set)
                 << ", " << reinterpret_cast<const void*>(get) << ")");
  egl_api_->eglSetBlobCacheFuncsANDROIDFn(dpy, set, get);
}

EGLBoolean DebugEGLApi::eglStreamAttribKHRFn(EGLDisplay dpy,
                                             EGLStreamKHR stream,
                                             EGLenum attribute,
                                             EGLint value) {
  GL_SERVICE_LOG("eglStreamAttribKHR"
                 << "(" << dpy << ", " << stream << ", " << attribute << ", "
                 << value << ")");
  EGLBoolean result =
      egl_api_->eglStreamAttribKHRFn(dpy, stream, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglStreamConsumerAcquireKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerAcquireKHR"
                 << "(" << dpy << ", " << stream << ")");
  EGLBoolean result = egl_api_->eglStreamConsumerAcquireKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglStreamConsumerGLTextureExternalAttribsNVFn(
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

EGLBoolean DebugEGLApi::eglStreamConsumerGLTextureExternalKHRFn(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerGLTextureExternalKHR"
                 << "(" << dpy << ", " << stream << ")");
  EGLBoolean result =
      egl_api_->eglStreamConsumerGLTextureExternalKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglStreamConsumerReleaseKHRFn(EGLDisplay dpy,
                                                      EGLStreamKHR stream) {
  GL_SERVICE_LOG("eglStreamConsumerReleaseKHR"
                 << "(" << dpy << ", " << stream << ")");
  EGLBoolean result = egl_api_->eglStreamConsumerReleaseKHRFn(dpy, stream);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglStreamPostD3DTextureANGLEFn(
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

EGLBoolean DebugEGLApi::eglSurfaceAttribFn(EGLDisplay dpy,
                                           EGLSurface surface,
                                           EGLint attribute,
                                           EGLint value) {
  GL_SERVICE_LOG("eglSurfaceAttrib"
                 << "(" << dpy << ", " << surface << ", " << attribute << ", "
                 << value << ")");
  EGLBoolean result =
      egl_api_->eglSurfaceAttribFn(dpy, surface, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglSwapBuffersFn(EGLDisplay dpy, EGLSurface surface) {
  GL_SERVICE_LOG("eglSwapBuffers"
                 << "(" << dpy << ", " << surface << ")");
  EGLBoolean result = egl_api_->eglSwapBuffersFn(dpy, surface);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglSwapBuffersWithDamageKHRFn(EGLDisplay dpy,
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

EGLBoolean DebugEGLApi::eglSwapIntervalFn(EGLDisplay dpy, EGLint interval) {
  GL_SERVICE_LOG("eglSwapInterval"
                 << "(" << dpy << ", " << interval << ")");
  EGLBoolean result = egl_api_->eglSwapIntervalFn(dpy, interval);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglTerminateFn(EGLDisplay dpy) {
  GL_SERVICE_LOG("eglTerminate"
                 << "(" << dpy << ")");
  EGLBoolean result = egl_api_->eglTerminateFn(dpy);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglWaitClientFn(void) {
  GL_SERVICE_LOG("eglWaitClient"
                 << "("
                 << ")");
  EGLBoolean result = egl_api_->eglWaitClientFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglWaitGLFn(void) {
  GL_SERVICE_LOG("eglWaitGL"
                 << "("
                 << ")");
  EGLBoolean result = egl_api_->eglWaitGLFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLBoolean DebugEGLApi::eglWaitNativeFn(EGLint engine) {
  GL_SERVICE_LOG("eglWaitNative"
                 << "(" << engine << ")");
  EGLBoolean result = egl_api_->eglWaitNativeFn(engine);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

EGLint DebugEGLApi::eglWaitSyncKHRFn(EGLDisplay dpy,
                                     EGLSyncKHR sync,
                                     EGLint flags) {
  GL_SERVICE_LOG("eglWaitSyncKHR"
                 << "(" << dpy << ", " << sync << ", " << flags << ")");
  EGLint result = egl_api_->eglWaitSyncKHRFn(dpy, sync, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

}  // namespace gl
