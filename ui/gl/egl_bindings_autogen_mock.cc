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
#include "ui/gl/egl_mock.h"

namespace {
// This is called mainly to prevent the compiler combining the code of mock
// functions with identical contents, so that their function pointers will be
// different.
void MakeEglMockFunctionUnique(const char* func_name) {
  VLOG(2) << "Calling mock " << func_name;
}
}  // namespace

namespace gl {

void GL_BINDING_CALL
MockEGLInterface::Mock_eglAcquireExternalContextANGLE(EGLDisplay dpy,
                                                      EGLSurface readAndDraw) {
  MakeEglMockFunctionUnique("eglAcquireExternalContextANGLE");
  interface_->AcquireExternalContextANGLE(dpy, readAndDraw);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglBindAPI(EGLenum api) {
  MakeEglMockFunctionUnique("eglBindAPI");
  return interface_->BindAPI(api);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglBindTexImage(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint buffer) {
  MakeEglMockFunctionUnique("eglBindTexImage");
  return interface_->BindTexImage(dpy, surface, buffer);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglChooseConfig(EGLDisplay dpy,
                                       const EGLint* attrib_list,
                                       EGLConfig* configs,
                                       EGLint config_size,
                                       EGLint* num_config) {
  MakeEglMockFunctionUnique("eglChooseConfig");
  return interface_->ChooseConfig(dpy, attrib_list, configs, config_size,
                                  num_config);
}

EGLint GL_BINDING_CALL
MockEGLInterface::Mock_eglClientWaitSync(EGLDisplay dpy,
                                         EGLSync sync,
                                         EGLint flags,
                                         EGLTime timeout) {
  MakeEglMockFunctionUnique("eglClientWaitSync");
  return interface_->ClientWaitSync(dpy, sync, flags, timeout);
}

EGLint GL_BINDING_CALL
MockEGLInterface::Mock_eglClientWaitSyncKHR(EGLDisplay dpy,
                                            EGLSyncKHR sync,
                                            EGLint flags,
                                            EGLTimeKHR timeout) {
  MakeEglMockFunctionUnique("eglClientWaitSyncKHR");
  return interface_->ClientWaitSyncKHR(dpy, sync, flags, timeout);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglCopyBuffers(EGLDisplay dpy,
                                      EGLSurface surface,
                                      EGLNativePixmapType target) {
  MakeEglMockFunctionUnique("eglCopyBuffers");
  return interface_->CopyBuffers(dpy, surface, target);
}

void* GL_BINDING_CALL
MockEGLInterface::Mock_eglCopyMetalSharedEventANGLE(EGLDisplay dpy,
                                                    EGLSync sync) {
  MakeEglMockFunctionUnique("eglCopyMetalSharedEventANGLE");
  return interface_->CopyMetalSharedEventANGLE(dpy, sync);
}

EGLContext GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateContext(EGLDisplay dpy,
                                        EGLConfig config,
                                        EGLContext share_context,
                                        const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateContext");
  return interface_->CreateContext(dpy, config, share_context, attrib_list);
}

EGLImage GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateImage(EGLDisplay dpy,
                                      EGLContext ctx,
                                      EGLenum target,
                                      EGLClientBuffer buffer,
                                      const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateImage");
  return interface_->CreateImage(dpy, ctx, target, buffer, attrib_list);
}

EGLImageKHR GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateImageKHR(EGLDisplay dpy,
                                         EGLContext ctx,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateImageKHR");
  return interface_->CreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreatePbufferFromClientBuffer(
    EGLDisplay dpy,
    EGLenum buftype,
    void* buffer,
    EGLConfig config,
    const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreatePbufferFromClientBuffer");
  return interface_->CreatePbufferFromClientBuffer(dpy, buftype, buffer, config,
                                                   attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreatePbufferSurface(EGLDisplay dpy,
                                               EGLConfig config,
                                               const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreatePbufferSurface");
  return interface_->CreatePbufferSurface(dpy, config, attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreatePixmapSurface(EGLDisplay dpy,
                                              EGLConfig config,
                                              EGLNativePixmapType pixmap,
                                              const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreatePixmapSurface");
  return interface_->CreatePixmapSurface(dpy, config, pixmap, attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreatePlatformPixmapSurface(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_pixmap,
    const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglCreatePlatformPixmapSurface");
  return interface_->CreatePlatformPixmapSurface(dpy, config, native_pixmap,
                                                 attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreatePlatformWindowSurface(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglCreatePlatformWindowSurface");
  return interface_->CreatePlatformWindowSurface(dpy, config, native_window,
                                                 attrib_list);
}

EGLStreamKHR GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateStreamKHR(EGLDisplay dpy,
                                          const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateStreamKHR");
  return interface_->CreateStreamKHR(dpy, attrib_list);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateStreamProducerD3DTextureANGLE(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateStreamProducerD3DTextureANGLE");
  return interface_->CreateStreamProducerD3DTextureANGLE(dpy, stream,
                                                         attrib_list);
}

EGLSync GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateSync(EGLDisplay dpy,
                                     EGLenum type,
                                     const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateSync");
  return interface_->CreateSync(dpy, type, attrib_list);
}

EGLSyncKHR GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateSyncKHR(EGLDisplay dpy,
                                        EGLenum type,
                                        const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateSyncKHR");
  return interface_->CreateSyncKHR(dpy, type, attrib_list);
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglCreateWindowSurface(EGLDisplay dpy,
                                              EGLConfig config,
                                              EGLNativeWindowType win,
                                              const EGLint* attrib_list) {
  MakeEglMockFunctionUnique("eglCreateWindowSurface");
  return interface_->CreateWindowSurface(dpy, config, win, attrib_list);
}

EGLint GL_BINDING_CALL
MockEGLInterface::Mock_eglDebugMessageControlKHR(EGLDEBUGPROCKHR callback,
                                                 const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglDebugMessageControlKHR");
  return interface_->DebugMessageControlKHR(callback, attrib_list);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
  MakeEglMockFunctionUnique("eglDestroyContext");
  return interface_->DestroyContext(dpy, ctx);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroyImage(EGLDisplay dpy, EGLImage image) {
  MakeEglMockFunctionUnique("eglDestroyImage");
  return interface_->DestroyImage(dpy, image);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image) {
  MakeEglMockFunctionUnique("eglDestroyImageKHR");
  return interface_->DestroyImageKHR(dpy, image);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroyStreamKHR(EGLDisplay dpy,
                                           EGLStreamKHR stream) {
  MakeEglMockFunctionUnique("eglDestroyStreamKHR");
  return interface_->DestroyStreamKHR(dpy, stream);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
  MakeEglMockFunctionUnique("eglDestroySurface");
  return interface_->DestroySurface(dpy, surface);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglDestroySync(EGLDisplay dpy,
                                                                 EGLSync sync) {
  MakeEglMockFunctionUnique("eglDestroySync");
  return interface_->DestroySync(dpy, sync);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync) {
  MakeEglMockFunctionUnique("eglDestroySyncKHR");
  return interface_->DestroySyncKHR(dpy, sync);
}

EGLint GL_BINDING_CALL
MockEGLInterface::Mock_eglDupNativeFenceFDANDROID(EGLDisplay dpy,
                                                  EGLSyncKHR sync) {
  MakeEglMockFunctionUnique("eglDupNativeFenceFDANDROID");
  return interface_->DupNativeFenceFDANDROID(dpy, sync);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglExportDMABUFImageMESA(EGLDisplay dpy,
                                                EGLImageKHR image,
                                                int* fds,
                                                EGLint* strides,
                                                EGLint* offsets) {
  MakeEglMockFunctionUnique("eglExportDMABUFImageMESA");
  return interface_->ExportDMABUFImageMESA(dpy, image, fds, strides, offsets);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglExportDMABUFImageQueryMESA(EGLDisplay dpy,
                                                     EGLImageKHR image,
                                                     int* fourcc,
                                                     int* num_planes,
                                                     EGLuint64KHR* modifiers) {
  MakeEglMockFunctionUnique("eglExportDMABUFImageQueryMESA");
  return interface_->ExportDMABUFImageQueryMESA(dpy, image, fourcc, num_planes,
                                                modifiers);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglExportVkImageANGLE(EGLDisplay dpy,
                                             EGLImageKHR image,
                                             void* vk_image,
                                             void* vk_image_create_info) {
  MakeEglMockFunctionUnique("eglExportVkImageANGLE");
  return interface_->ExportVkImageANGLE(dpy, image, vk_image,
                                        vk_image_create_info);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetCompositorTimingANDROID(EGLDisplay dpy,
                                                     EGLSurface surface,
                                                     EGLint numTimestamps,
                                                     EGLint* names,
                                                     EGLnsecsANDROID* values) {
  MakeEglMockFunctionUnique("eglGetCompositorTimingANDROID");
  return interface_->GetCompositorTimingANDROID(dpy, surface, numTimestamps,
                                                names, values);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetCompositorTimingSupportedANDROID(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint timestamp) {
  MakeEglMockFunctionUnique("eglGetCompositorTimingSupportedANDROID");
  return interface_->GetCompositorTimingSupportedANDROID(dpy, surface,
                                                         timestamp);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetConfigAttrib(EGLDisplay dpy,
                                          EGLConfig config,
                                          EGLint attribute,
                                          EGLint* value) {
  MakeEglMockFunctionUnique("eglGetConfigAttrib");
  return interface_->GetConfigAttrib(dpy, config, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetConfigs(EGLDisplay dpy,
                                     EGLConfig* configs,
                                     EGLint config_size,
                                     EGLint* num_config) {
  MakeEglMockFunctionUnique("eglGetConfigs");
  return interface_->GetConfigs(dpy, configs, config_size, num_config);
}

EGLContext GL_BINDING_CALL MockEGLInterface::Mock_eglGetCurrentContext(void) {
  MakeEglMockFunctionUnique("eglGetCurrentContext");
  return interface_->GetCurrentContext();
}

EGLDisplay GL_BINDING_CALL MockEGLInterface::Mock_eglGetCurrentDisplay(void) {
  MakeEglMockFunctionUnique("eglGetCurrentDisplay");
  return interface_->GetCurrentDisplay();
}

EGLSurface GL_BINDING_CALL
MockEGLInterface::Mock_eglGetCurrentSurface(EGLint readdraw) {
  MakeEglMockFunctionUnique("eglGetCurrentSurface");
  return interface_->GetCurrentSurface(readdraw);
}

EGLDisplay GL_BINDING_CALL
MockEGLInterface::Mock_eglGetDisplay(EGLNativeDisplayType display_id) {
  MakeEglMockFunctionUnique("eglGetDisplay");
  return interface_->GetDisplay(display_id);
}

EGLint GL_BINDING_CALL MockEGLInterface::Mock_eglGetError(void) {
  MakeEglMockFunctionUnique("eglGetError");
  return interface_->GetError();
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetFrameTimestampSupportedANDROID(EGLDisplay dpy,
                                                            EGLSurface surface,
                                                            EGLint timestamp) {
  MakeEglMockFunctionUnique("eglGetFrameTimestampSupportedANDROID");
  return interface_->GetFrameTimestampSupportedANDROID(dpy, surface, timestamp);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetFrameTimestampsANDROID(EGLDisplay dpy,
                                                    EGLSurface surface,
                                                    EGLuint64KHR frameId,
                                                    EGLint numTimestamps,
                                                    EGLint* timestamps,
                                                    EGLnsecsANDROID* values) {
  MakeEglMockFunctionUnique("eglGetFrameTimestampsANDROID");
  return interface_->GetFrameTimestampsANDROID(
      dpy, surface, frameId, numTimestamps, timestamps, values);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetMscRateANGLE(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint* numerator,
                                          EGLint* denominator) {
  MakeEglMockFunctionUnique("eglGetMscRateANGLE");
  return interface_->GetMscRateANGLE(dpy, surface, numerator, denominator);
}

EGLClientBuffer GL_BINDING_CALL
MockEGLInterface::Mock_eglGetNativeClientBufferANDROID(
    const struct AHardwareBuffer* ahardwarebuffer) {
  MakeEglMockFunctionUnique("eglGetNativeClientBufferANDROID");
  return interface_->GetNativeClientBufferANDROID(ahardwarebuffer);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetNextFrameIdANDROID(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLuint64KHR* frameId) {
  MakeEglMockFunctionUnique("eglGetNextFrameIdANDROID");
  return interface_->GetNextFrameIdANDROID(dpy, surface, frameId);
}

EGLDisplay GL_BINDING_CALL
MockEGLInterface::Mock_eglGetPlatformDisplay(EGLenum platform,
                                             void* native_display,
                                             const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglGetPlatformDisplay");
  return interface_->GetPlatformDisplay(platform, native_display, attrib_list);
}

__eglMustCastToProperFunctionPointerType GL_BINDING_CALL
MockEGLInterface::Mock_eglGetProcAddress(const char* procname) {
  MakeEglMockFunctionUnique("eglGetProcAddress");
  return interface_->GetProcAddress(procname);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetSyncAttrib(EGLDisplay dpy,
                                        EGLSync sync,
                                        EGLint attribute,
                                        EGLAttrib* value) {
  MakeEglMockFunctionUnique("eglGetSyncAttrib");
  return interface_->GetSyncAttrib(dpy, sync, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetSyncAttribKHR(EGLDisplay dpy,
                                           EGLSyncKHR sync,
                                           EGLint attribute,
                                           EGLint* value) {
  MakeEglMockFunctionUnique("eglGetSyncAttribKHR");
  return interface_->GetSyncAttribKHR(dpy, sync, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglGetSyncValuesCHROMIUM(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLuint64CHROMIUM* ust,
                                                EGLuint64CHROMIUM* msc,
                                                EGLuint64CHROMIUM* sbc) {
  MakeEglMockFunctionUnique("eglGetSyncValuesCHROMIUM");
  return interface_->GetSyncValuesCHROMIUM(dpy, surface, ust, msc, sbc);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglHandleGPUSwitchANGLE(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglHandleGPUSwitchANGLE");
  interface_->HandleGPUSwitchANGLE(dpy);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglImageFlushExternalEXT(EGLDisplay dpy,
                                                EGLImageKHR image,
                                                const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglImageFlushExternalEXT");
  return interface_->ImageFlushExternalEXT(dpy, image, attrib_list);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglInitialize(EGLDisplay dpy,
                                                                EGLint* major,
                                                                EGLint* minor) {
  MakeEglMockFunctionUnique("eglInitialize");
  return interface_->Initialize(dpy, major, minor);
}

EGLint GL_BINDING_CALL
MockEGLInterface::Mock_eglLabelObjectKHR(EGLDisplay display,
                                         EGLenum objectType,
                                         EGLObjectKHR object,
                                         EGLLabelKHR label) {
  MakeEglMockFunctionUnique("eglLabelObjectKHR");
  return interface_->LabelObjectKHR(display, objectType, object, label);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglLockVulkanQueueANGLE(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglLockVulkanQueueANGLE");
  interface_->LockVulkanQueueANGLE(dpy);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglMakeCurrent(EGLDisplay dpy,
                                      EGLSurface draw,
                                      EGLSurface read,
                                      EGLContext ctx) {
  MakeEglMockFunctionUnique("eglMakeCurrent");
  return interface_->MakeCurrent(dpy, draw, read, ctx);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglPostSubBufferNV(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint x,
                                          EGLint y,
                                          EGLint width,
                                          EGLint height) {
  MakeEglMockFunctionUnique("eglPostSubBufferNV");
  return interface_->PostSubBufferNV(dpy, surface, x, y, width, height);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglPresentationTimeANDROID(EGLDisplay dpy,
                                                  EGLSurface surface,
                                                  EGLnsecsANDROID time) {
  MakeEglMockFunctionUnique("eglPresentationTimeANDROID");
  return interface_->PresentationTimeANDROID(dpy, surface, time);
}

EGLenum GL_BINDING_CALL MockEGLInterface::Mock_eglQueryAPI(void) {
  MakeEglMockFunctionUnique("eglQueryAPI");
  return interface_->QueryAPI();
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryContext(EGLDisplay dpy,
                                       EGLContext ctx,
                                       EGLint attribute,
                                       EGLint* value) {
  MakeEglMockFunctionUnique("eglQueryContext");
  return interface_->QueryContext(dpy, ctx, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDebugKHR(EGLint attribute, EGLAttrib* value) {
  MakeEglMockFunctionUnique("eglQueryDebugKHR");
  return interface_->QueryDebugKHR(attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDeviceAttribEXT(EGLDeviceEXT device,
                                               EGLint attribute,
                                               EGLAttrib* value) {
  MakeEglMockFunctionUnique("eglQueryDeviceAttribEXT");
  return interface_->QueryDeviceAttribEXT(device, attribute, value);
}

const char* GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDeviceStringEXT(EGLDeviceEXT device,
                                               EGLint name) {
  MakeEglMockFunctionUnique("eglQueryDeviceStringEXT");
  return interface_->QueryDeviceStringEXT(device, name);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDevicesEXT(EGLint max_devices,
                                          EGLDeviceEXT* devices,
                                          EGLint* num_devices) {
  MakeEglMockFunctionUnique("eglQueryDevicesEXT");
  return interface_->QueryDevicesEXT(max_devices, devices, num_devices);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDisplayAttribANGLE(EGLDisplay dpy,
                                                  EGLint attribute,
                                                  EGLAttrib* value) {
  MakeEglMockFunctionUnique("eglQueryDisplayAttribANGLE");
  return interface_->QueryDisplayAttribANGLE(dpy, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDisplayAttribEXT(EGLDisplay dpy,
                                                EGLint attribute,
                                                EGLAttrib* value) {
  MakeEglMockFunctionUnique("eglQueryDisplayAttribEXT");
  return interface_->QueryDisplayAttribEXT(dpy, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDmaBufFormatsEXT(EGLDisplay dpy,
                                                EGLint max_formats,
                                                EGLint* formats,
                                                EGLint* num_formats) {
  MakeEglMockFunctionUnique("eglQueryDmaBufFormatsEXT");
  return interface_->QueryDmaBufFormatsEXT(dpy, max_formats, formats,
                                           num_formats);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryDmaBufModifiersEXT(EGLDisplay dpy,
                                                  EGLint format,
                                                  EGLint max_modifiers,
                                                  EGLuint64KHR* modifiers,
                                                  EGLBoolean* external_only,
                                                  EGLint* num_modifiers) {
  MakeEglMockFunctionUnique("eglQueryDmaBufModifiersEXT");
  return interface_->QueryDmaBufModifiersEXT(
      dpy, format, max_modifiers, modifiers, external_only, num_modifiers);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryStreamKHR(EGLDisplay dpy,
                                         EGLStreamKHR stream,
                                         EGLenum attribute,
                                         EGLint* value) {
  MakeEglMockFunctionUnique("eglQueryStreamKHR");
  return interface_->QueryStreamKHR(dpy, stream, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryStreamu64KHR(EGLDisplay dpy,
                                            EGLStreamKHR stream,
                                            EGLenum attribute,
                                            EGLuint64KHR* value) {
  MakeEglMockFunctionUnique("eglQueryStreamu64KHR");
  return interface_->QueryStreamu64KHR(dpy, stream, attribute, value);
}

const char* GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryString(EGLDisplay dpy, EGLint name) {
  MakeEglMockFunctionUnique("eglQueryString");
  return interface_->QueryString(dpy, name);
}

const char* GL_BINDING_CALL
MockEGLInterface::Mock_eglQueryStringiANGLE(EGLDisplay dpy,
                                            EGLint name,
                                            EGLint index) {
  MakeEglMockFunctionUnique("eglQueryStringiANGLE");
  return interface_->QueryStringiANGLE(dpy, name, index);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQuerySurface(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint attribute,
                                       EGLint* value) {
  MakeEglMockFunctionUnique("eglQuerySurface");
  return interface_->QuerySurface(dpy, surface, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglQuerySurfacePointerANGLE(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLint attribute,
                                                   void** value) {
  MakeEglMockFunctionUnique("eglQuerySurfacePointerANGLE");
  return interface_->QuerySurfacePointerANGLE(dpy, surface, attribute, value);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglReacquireHighPowerGPUANGLE(EGLDisplay dpy,
                                                     EGLContext ctx) {
  MakeEglMockFunctionUnique("eglReacquireHighPowerGPUANGLE");
  interface_->ReacquireHighPowerGPUANGLE(dpy, ctx);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglReleaseExternalContextANGLE(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglReleaseExternalContextANGLE");
  interface_->ReleaseExternalContextANGLE(dpy);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglReleaseHighPowerGPUANGLE(EGLDisplay dpy,
                                                   EGLContext ctx) {
  MakeEglMockFunctionUnique("eglReleaseHighPowerGPUANGLE");
  interface_->ReleaseHighPowerGPUANGLE(dpy, ctx);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglReleaseTexImage(EGLDisplay dpy,
                                          EGLSurface surface,
                                          EGLint buffer) {
  MakeEglMockFunctionUnique("eglReleaseTexImage");
  return interface_->ReleaseTexImage(dpy, surface, buffer);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglReleaseThread(void) {
  MakeEglMockFunctionUnique("eglReleaseThread");
  return interface_->ReleaseThread();
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglSetBlobCacheFuncsANDROID(EGLDisplay dpy,
                                                   EGLSetBlobFuncANDROID set,
                                                   EGLGetBlobFuncANDROID get) {
  MakeEglMockFunctionUnique("eglSetBlobCacheFuncsANDROID");
  interface_->SetBlobCacheFuncsANDROID(dpy, set, get);
}

void GL_BINDING_CALL MockEGLInterface::Mock_eglSetValidationEnabledANGLE(
    EGLBoolean validationState) {
  MakeEglMockFunctionUnique("eglSetValidationEnabledANGLE");
  interface_->SetValidationEnabledANGLE(validationState);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglStreamAttribKHR(EGLDisplay dpy,
                                          EGLStreamKHR stream,
                                          EGLenum attribute,
                                          EGLint value) {
  MakeEglMockFunctionUnique("eglStreamAttribKHR");
  return interface_->StreamAttribKHR(dpy, stream, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglStreamConsumerAcquireKHR(EGLDisplay dpy,
                                                   EGLStreamKHR stream) {
  MakeEglMockFunctionUnique("eglStreamConsumerAcquireKHR");
  return interface_->StreamConsumerAcquireKHR(dpy, stream);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglStreamConsumerGLTextureExternalAttribsNV(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglStreamConsumerGLTextureExternalAttribsNV");
  return interface_->StreamConsumerGLTextureExternalAttribsNV(dpy, stream,
                                                              attrib_list);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglStreamConsumerGLTextureExternalKHR(
    EGLDisplay dpy,
    EGLStreamKHR stream) {
  MakeEglMockFunctionUnique("eglStreamConsumerGLTextureExternalKHR");
  return interface_->StreamConsumerGLTextureExternalKHR(dpy, stream);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglStreamConsumerReleaseKHR(EGLDisplay dpy,
                                                   EGLStreamKHR stream) {
  MakeEglMockFunctionUnique("eglStreamConsumerReleaseKHR");
  return interface_->StreamConsumerReleaseKHR(dpy, stream);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglStreamPostD3DTextureANGLE(
    EGLDisplay dpy,
    EGLStreamKHR stream,
    void* texture,
    const EGLAttrib* attrib_list) {
  MakeEglMockFunctionUnique("eglStreamPostD3DTextureANGLE");
  return interface_->StreamPostD3DTextureANGLE(dpy, stream, texture,
                                               attrib_list);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglSurfaceAttrib(EGLDisplay dpy,
                                        EGLSurface surface,
                                        EGLint attribute,
                                        EGLint value) {
  MakeEglMockFunctionUnique("eglSurfaceAttrib");
  return interface_->SurfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  MakeEglMockFunctionUnique("eglSwapBuffers");
  return interface_->SwapBuffers(dpy, surface);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglSwapBuffersWithDamageKHR(EGLDisplay dpy,
                                                   EGLSurface surface,
                                                   EGLint* rects,
                                                   EGLint n_rects) {
  MakeEglMockFunctionUnique("eglSwapBuffersWithDamageKHR");
  return interface_->SwapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
}

EGLBoolean GL_BINDING_CALL
MockEGLInterface::Mock_eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  MakeEglMockFunctionUnique("eglSwapInterval");
  return interface_->SwapInterval(dpy, interval);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglTerminate(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglTerminate");
  return interface_->Terminate(dpy);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglUnlockVulkanQueueANGLE(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglUnlockVulkanQueueANGLE");
  interface_->UnlockVulkanQueueANGLE(dpy);
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglWaitClient(void) {
  MakeEglMockFunctionUnique("eglWaitClient");
  return interface_->WaitClient();
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglWaitGL(void) {
  MakeEglMockFunctionUnique("eglWaitGL");
  return interface_->WaitGL();
}

EGLBoolean GL_BINDING_CALL MockEGLInterface::Mock_eglWaitNative(EGLint engine) {
  MakeEglMockFunctionUnique("eglWaitNative");
  return interface_->WaitNative(engine);
}

EGLint GL_BINDING_CALL MockEGLInterface::Mock_eglWaitSync(EGLDisplay dpy,
                                                          EGLSync sync,
                                                          EGLint flags) {
  MakeEglMockFunctionUnique("eglWaitSync");
  return interface_->WaitSync(dpy, sync, flags);
}

EGLint GL_BINDING_CALL MockEGLInterface::Mock_eglWaitSyncKHR(EGLDisplay dpy,
                                                             EGLSyncKHR sync,
                                                             EGLint flags) {
  MakeEglMockFunctionUnique("eglWaitSyncKHR");
  return interface_->WaitSyncKHR(dpy, sync, flags);
}

void GL_BINDING_CALL
MockEGLInterface::Mock_eglWaitUntilWorkScheduledANGLE(EGLDisplay dpy) {
  MakeEglMockFunctionUnique("eglWaitUntilWorkScheduledANGLE");
  interface_->WaitUntilWorkScheduledANGLE(dpy);
}

static void MockEglInvalidFunction() {
  NOTREACHED();
}

GLFunctionPointerType GL_BINDING_CALL
MockEGLInterface::GetGLProcAddress(const char* name) {
  std::string_view name_view(name);
  if (name_view == "eglAcquireExternalContextANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglAcquireExternalContextANGLE);
  }
  if (name_view == "eglBindAPI") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglBindAPI);
  }
  if (name_view == "eglBindTexImage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglBindTexImage);
  }
  if (name_view == "eglChooseConfig") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglChooseConfig);
  }
  if (name_view == "eglClientWaitSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglClientWaitSync);
  }
  if (name_view == "eglClientWaitSyncKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglClientWaitSyncKHR);
  }
  if (name_view == "eglCopyBuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCopyBuffers);
  }
  if (name_view == "eglCopyMetalSharedEventANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCopyMetalSharedEventANGLE);
  }
  if (name_view == "eglCreateContext") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateContext);
  }
  if (name_view == "eglCreateImage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateImage);
  }
  if (name_view == "eglCreateImageKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateImageKHR);
  }
  if (name_view == "eglCreatePbufferFromClientBuffer") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCreatePbufferFromClientBuffer);
  }
  if (name_view == "eglCreatePbufferSurface") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCreatePbufferSurface);
  }
  if (name_view == "eglCreatePixmapSurface") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreatePixmapSurface);
  }
  if (name_view == "eglCreatePlatformPixmapSurface") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCreatePlatformPixmapSurface);
  }
  if (name_view == "eglCreatePlatformWindowSurface") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCreatePlatformWindowSurface);
  }
  if (name_view == "eglCreateStreamKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateStreamKHR);
  }
  if (name_view == "eglCreateStreamProducerD3DTextureANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglCreateStreamProducerD3DTextureANGLE);
  }
  if (name_view == "eglCreateSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateSync);
  }
  if (name_view == "eglCreateSyncKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateSyncKHR);
  }
  if (name_view == "eglCreateWindowSurface") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglCreateWindowSurface);
  }
  if (name_view == "eglDebugMessageControlKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglDebugMessageControlKHR);
  }
  if (name_view == "eglDestroyContext") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroyContext);
  }
  if (name_view == "eglDestroyImage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroyImage);
  }
  if (name_view == "eglDestroyImageKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroyImageKHR);
  }
  if (name_view == "eglDestroyStreamKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroyStreamKHR);
  }
  if (name_view == "eglDestroySurface") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroySurface);
  }
  if (name_view == "eglDestroySync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroySync);
  }
  if (name_view == "eglDestroySyncKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglDestroySyncKHR);
  }
  if (name_view == "eglDupNativeFenceFDANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglDupNativeFenceFDANDROID);
  }
  if (name_view == "eglExportDMABUFImageMESA") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglExportDMABUFImageMESA);
  }
  if (name_view == "eglExportDMABUFImageQueryMESA") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglExportDMABUFImageQueryMESA);
  }
  if (name_view == "eglExportVkImageANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglExportVkImageANGLE);
  }
  if (name_view == "eglGetCompositorTimingANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetCompositorTimingANDROID);
  }
  if (name_view == "eglGetCompositorTimingSupportedANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetCompositorTimingSupportedANDROID);
  }
  if (name_view == "eglGetConfigAttrib") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetConfigAttrib);
  }
  if (name_view == "eglGetConfigs") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetConfigs);
  }
  if (name_view == "eglGetCurrentContext") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetCurrentContext);
  }
  if (name_view == "eglGetCurrentDisplay") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetCurrentDisplay);
  }
  if (name_view == "eglGetCurrentSurface") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetCurrentSurface);
  }
  if (name_view == "eglGetDisplay") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetDisplay);
  }
  if (name_view == "eglGetError") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetError);
  }
  if (name_view == "eglGetFrameTimestampSupportedANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetFrameTimestampSupportedANDROID);
  }
  if (name_view == "eglGetFrameTimestampsANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetFrameTimestampsANDROID);
  }
  if (name_view == "eglGetMscRateANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetMscRateANGLE);
  }
  if (name_view == "eglGetNativeClientBufferANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetNativeClientBufferANDROID);
  }
  if (name_view == "eglGetNextFrameIdANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetNextFrameIdANDROID);
  }
  if (name_view == "eglGetPlatformDisplay") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetPlatformDisplay);
  }
  if (name_view == "eglGetProcAddress") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetProcAddress);
  }
  if (name_view == "eglGetSyncAttrib") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetSyncAttrib);
  }
  if (name_view == "eglGetSyncAttribKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglGetSyncAttribKHR);
  }
  if (name_view == "eglGetSyncValuesCHROMIUM") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglGetSyncValuesCHROMIUM);
  }
  if (name_view == "eglHandleGPUSwitchANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglHandleGPUSwitchANGLE);
  }
  if (name_view == "eglImageFlushExternalEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglImageFlushExternalEXT);
  }
  if (name_view == "eglInitialize") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglInitialize);
  }
  if (name_view == "eglLabelObjectKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglLabelObjectKHR);
  }
  if (name_view == "eglLockVulkanQueueANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglLockVulkanQueueANGLE);
  }
  if (name_view == "eglMakeCurrent") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglMakeCurrent);
  }
  if (name_view == "eglPostSubBufferNV") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglPostSubBufferNV);
  }
  if (name_view == "eglPresentationTimeANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglPresentationTimeANDROID);
  }
  if (name_view == "eglQueryAPI") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryAPI);
  }
  if (name_view == "eglQueryContext") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryContext);
  }
  if (name_view == "eglQueryDebugKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryDebugKHR);
  }
  if (name_view == "eglQueryDeviceAttribEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDeviceAttribEXT);
  }
  if (name_view == "eglQueryDeviceStringEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDeviceStringEXT);
  }
  if (name_view == "eglQueryDevicesEXT") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryDevicesEXT);
  }
  if (name_view == "eglQueryDisplayAttribANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDisplayAttribANGLE);
  }
  if (name_view == "eglQueryDisplayAttribEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDisplayAttribEXT);
  }
  if (name_view == "eglQueryDmaBufFormatsEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDmaBufFormatsEXT);
  }
  if (name_view == "eglQueryDmaBufModifiersEXT") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQueryDmaBufModifiersEXT);
  }
  if (name_view == "eglQueryStreamKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryStreamKHR);
  }
  if (name_view == "eglQueryStreamu64KHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryStreamu64KHR);
  }
  if (name_view == "eglQueryString") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryString);
  }
  if (name_view == "eglQueryStringiANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQueryStringiANGLE);
  }
  if (name_view == "eglQuerySurface") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglQuerySurface);
  }
  if (name_view == "eglQuerySurfacePointerANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglQuerySurfacePointerANGLE);
  }
  if (name_view == "eglReacquireHighPowerGPUANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglReacquireHighPowerGPUANGLE);
  }
  if (name_view == "eglReleaseExternalContextANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglReleaseExternalContextANGLE);
  }
  if (name_view == "eglReleaseHighPowerGPUANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglReleaseHighPowerGPUANGLE);
  }
  if (name_view == "eglReleaseTexImage") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglReleaseTexImage);
  }
  if (name_view == "eglReleaseThread") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglReleaseThread);
  }
  if (name_view == "eglSetBlobCacheFuncsANDROID") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglSetBlobCacheFuncsANDROID);
  }
  if (name_view == "eglSetValidationEnabledANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglSetValidationEnabledANGLE);
  }
  if (name_view == "eglStreamAttribKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglStreamAttribKHR);
  }
  if (name_view == "eglStreamConsumerAcquireKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglStreamConsumerAcquireKHR);
  }
  if (name_view == "eglStreamConsumerGLTextureExternalAttribsNV") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglStreamConsumerGLTextureExternalAttribsNV);
  }
  if (name_view == "eglStreamConsumerGLTextureExternalKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglStreamConsumerGLTextureExternalKHR);
  }
  if (name_view == "eglStreamConsumerReleaseKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglStreamConsumerReleaseKHR);
  }
  if (name_view == "eglStreamPostD3DTextureANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglStreamPostD3DTextureANGLE);
  }
  if (name_view == "eglSurfaceAttrib") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglSurfaceAttrib);
  }
  if (name_view == "eglSwapBuffers") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglSwapBuffers);
  }
  if (name_view == "eglSwapBuffersWithDamageKHR") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglSwapBuffersWithDamageKHR);
  }
  if (name_view == "eglSwapInterval") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglSwapInterval);
  }
  if (name_view == "eglTerminate") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglTerminate);
  }
  if (name_view == "eglUnlockVulkanQueueANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglUnlockVulkanQueueANGLE);
  }
  if (name_view == "eglWaitClient") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglWaitClient);
  }
  if (name_view == "eglWaitGL") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglWaitGL);
  }
  if (name_view == "eglWaitNative") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglWaitNative);
  }
  if (name_view == "eglWaitSync") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglWaitSync);
  }
  if (name_view == "eglWaitSyncKHR") {
    return reinterpret_cast<GLFunctionPointerType>(Mock_eglWaitSyncKHR);
  }
  if (name_view == "eglWaitUntilWorkScheduledANGLE") {
    return reinterpret_cast<GLFunctionPointerType>(
        Mock_eglWaitUntilWorkScheduledANGLE);
  }
  return reinterpret_cast<GLFunctionPointerType>(&MockEglInvalidFunction);
}

}  // namespace gl
