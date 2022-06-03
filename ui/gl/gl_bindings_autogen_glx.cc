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
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

DriverGLX g_driver_glx;  // Exists in .bss

void DriverGLX::InitializeStaticBindings() {
  // Ensure struct has been zero-initialized.
  char* this_bytes = reinterpret_cast<char*>(this);
  DCHECK(this_bytes[0] == 0);
  DCHECK(memcmp(this_bytes, this_bytes + 1, sizeof(*this) - 1) == 0);

  fn.glXChooseFBConfigFn = reinterpret_cast<glXChooseFBConfigProc>(
      GetGLProcAddress("glXChooseFBConfig"));
  fn.glXChooseVisualFn = reinterpret_cast<glXChooseVisualProc>(
      GetGLProcAddress("glXChooseVisual"));
  fn.glXCopyContextFn =
      reinterpret_cast<glXCopyContextProc>(GetGLProcAddress("glXCopyContext"));
  fn.glXCreateContextFn = reinterpret_cast<glXCreateContextProc>(
      GetGLProcAddress("glXCreateContext"));
  fn.glXCreateGLXPixmapFn = reinterpret_cast<glXCreateGLXPixmapProc>(
      GetGLProcAddress("glXCreateGLXPixmap"));
  fn.glXCreateNewContextFn = reinterpret_cast<glXCreateNewContextProc>(
      GetGLProcAddress("glXCreateNewContext"));
  fn.glXCreatePbufferFn = reinterpret_cast<glXCreatePbufferProc>(
      GetGLProcAddress("glXCreatePbuffer"));
  fn.glXCreatePixmapFn = reinterpret_cast<glXCreatePixmapProc>(
      GetGLProcAddress("glXCreatePixmap"));
  fn.glXCreateWindowFn = reinterpret_cast<glXCreateWindowProc>(
      GetGLProcAddress("glXCreateWindow"));
  fn.glXDestroyContextFn = reinterpret_cast<glXDestroyContextProc>(
      GetGLProcAddress("glXDestroyContext"));
  fn.glXDestroyGLXPixmapFn = reinterpret_cast<glXDestroyGLXPixmapProc>(
      GetGLProcAddress("glXDestroyGLXPixmap"));
  fn.glXDestroyPbufferFn = reinterpret_cast<glXDestroyPbufferProc>(
      GetGLProcAddress("glXDestroyPbuffer"));
  fn.glXDestroyPixmapFn = reinterpret_cast<glXDestroyPixmapProc>(
      GetGLProcAddress("glXDestroyPixmap"));
  fn.glXDestroyWindowFn = reinterpret_cast<glXDestroyWindowProc>(
      GetGLProcAddress("glXDestroyWindow"));
  fn.glXGetClientStringFn = reinterpret_cast<glXGetClientStringProc>(
      GetGLProcAddress("glXGetClientString"));
  fn.glXGetConfigFn =
      reinterpret_cast<glXGetConfigProc>(GetGLProcAddress("glXGetConfig"));
  fn.glXGetCurrentContextFn = reinterpret_cast<glXGetCurrentContextProc>(
      GetGLProcAddress("glXGetCurrentContext"));
  fn.glXGetCurrentDisplayFn = reinterpret_cast<glXGetCurrentDisplayProc>(
      GetGLProcAddress("glXGetCurrentDisplay"));
  fn.glXGetCurrentDrawableFn = reinterpret_cast<glXGetCurrentDrawableProc>(
      GetGLProcAddress("glXGetCurrentDrawable"));
  fn.glXGetCurrentReadDrawableFn =
      reinterpret_cast<glXGetCurrentReadDrawableProc>(
          GetGLProcAddress("glXGetCurrentReadDrawable"));
  fn.glXGetFBConfigAttribFn = reinterpret_cast<glXGetFBConfigAttribProc>(
      GetGLProcAddress("glXGetFBConfigAttrib"));
  fn.glXGetFBConfigsFn = reinterpret_cast<glXGetFBConfigsProc>(
      GetGLProcAddress("glXGetFBConfigs"));
  fn.glXGetSelectedEventFn = reinterpret_cast<glXGetSelectedEventProc>(
      GetGLProcAddress("glXGetSelectedEvent"));
  fn.glXGetVisualFromFBConfigFn =
      reinterpret_cast<glXGetVisualFromFBConfigProc>(
          GetGLProcAddress("glXGetVisualFromFBConfig"));
  fn.glXIsDirectFn =
      reinterpret_cast<glXIsDirectProc>(GetGLProcAddress("glXIsDirect"));
  fn.glXMakeContextCurrentFn = reinterpret_cast<glXMakeContextCurrentProc>(
      GetGLProcAddress("glXMakeContextCurrent"));
  fn.glXMakeCurrentFn =
      reinterpret_cast<glXMakeCurrentProc>(GetGLProcAddress("glXMakeCurrent"));
  fn.glXQueryContextFn = reinterpret_cast<glXQueryContextProc>(
      GetGLProcAddress("glXQueryContext"));
  fn.glXQueryDrawableFn = reinterpret_cast<glXQueryDrawableProc>(
      GetGLProcAddress("glXQueryDrawable"));
  fn.glXQueryExtensionFn = reinterpret_cast<glXQueryExtensionProc>(
      GetGLProcAddress("glXQueryExtension"));
  fn.glXQueryExtensionsStringFn =
      reinterpret_cast<glXQueryExtensionsStringProc>(
          GetGLProcAddress("glXQueryExtensionsString"));
  fn.glXQueryServerStringFn = reinterpret_cast<glXQueryServerStringProc>(
      GetGLProcAddress("glXQueryServerString"));
  fn.glXQueryVersionFn = reinterpret_cast<glXQueryVersionProc>(
      GetGLProcAddress("glXQueryVersion"));
  fn.glXSelectEventFn =
      reinterpret_cast<glXSelectEventProc>(GetGLProcAddress("glXSelectEvent"));
  fn.glXSwapBuffersFn =
      reinterpret_cast<glXSwapBuffersProc>(GetGLProcAddress("glXSwapBuffers"));
  fn.glXUseXFontFn =
      reinterpret_cast<glXUseXFontProc>(GetGLProcAddress("glXUseXFont"));
  fn.glXWaitGLFn =
      reinterpret_cast<glXWaitGLProc>(GetGLProcAddress("glXWaitGL"));
  fn.glXWaitXFn = reinterpret_cast<glXWaitXProc>(GetGLProcAddress("glXWaitX"));
}

void DriverGLX::InitializeExtensionBindings() {
  std::string platform_extensions(GetPlatformExtensions());
  gfx::ExtensionSet extensions(gfx::MakeExtensionSet(platform_extensions));
  ALLOW_UNUSED_LOCAL(extensions);

  ext.b_GLX_ARB_create_context =
      gfx::HasExtension(extensions, "GLX_ARB_create_context");
  ext.b_GLX_EXT_swap_control =
      gfx::HasExtension(extensions, "GLX_EXT_swap_control");
  ext.b_GLX_EXT_texture_from_pixmap =
      gfx::HasExtension(extensions, "GLX_EXT_texture_from_pixmap");
  ext.b_GLX_MESA_copy_sub_buffer =
      gfx::HasExtension(extensions, "GLX_MESA_copy_sub_buffer");
  ext.b_GLX_MESA_swap_control =
      gfx::HasExtension(extensions, "GLX_MESA_swap_control");
  ext.b_GLX_OML_sync_control =
      gfx::HasExtension(extensions, "GLX_OML_sync_control");
  ext.b_GLX_SGIX_fbconfig = gfx::HasExtension(extensions, "GLX_SGIX_fbconfig");
  ext.b_GLX_SGI_video_sync =
      gfx::HasExtension(extensions, "GLX_SGI_video_sync");

  if (ext.b_GLX_EXT_texture_from_pixmap) {
    fn.glXBindTexImageEXTFn = reinterpret_cast<glXBindTexImageEXTProc>(
        GetGLProcAddress("glXBindTexImageEXT"));
  }

  if (ext.b_GLX_MESA_copy_sub_buffer) {
    fn.glXCopySubBufferMESAFn = reinterpret_cast<glXCopySubBufferMESAProc>(
        GetGLProcAddress("glXCopySubBufferMESA"));
  }

  if (ext.b_GLX_ARB_create_context) {
    fn.glXCreateContextAttribsARBFn =
        reinterpret_cast<glXCreateContextAttribsARBProc>(
            GetGLProcAddress("glXCreateContextAttribsARB"));
  }

  if (ext.b_GLX_SGIX_fbconfig) {
    fn.glXGetFBConfigFromVisualSGIXFn =
        reinterpret_cast<glXGetFBConfigFromVisualSGIXProc>(
            GetGLProcAddress("glXGetFBConfigFromVisualSGIX"));
  }

  if (ext.b_GLX_OML_sync_control) {
    fn.glXGetMscRateOMLFn = reinterpret_cast<glXGetMscRateOMLProc>(
        GetGLProcAddress("glXGetMscRateOML"));
  }

  if (ext.b_GLX_OML_sync_control) {
    fn.glXGetSyncValuesOMLFn = reinterpret_cast<glXGetSyncValuesOMLProc>(
        GetGLProcAddress("glXGetSyncValuesOML"));
  }

  if (ext.b_GLX_EXT_texture_from_pixmap) {
    fn.glXReleaseTexImageEXTFn = reinterpret_cast<glXReleaseTexImageEXTProc>(
        GetGLProcAddress("glXReleaseTexImageEXT"));
  }

  if (ext.b_GLX_EXT_swap_control) {
    fn.glXSwapIntervalEXTFn = reinterpret_cast<glXSwapIntervalEXTProc>(
        GetGLProcAddress("glXSwapIntervalEXT"));
  }

  if (ext.b_GLX_MESA_swap_control) {
    fn.glXSwapIntervalMESAFn = reinterpret_cast<glXSwapIntervalMESAProc>(
        GetGLProcAddress("glXSwapIntervalMESA"));
  }

  if (ext.b_GLX_SGI_video_sync) {
    fn.glXWaitVideoSyncSGIFn = reinterpret_cast<glXWaitVideoSyncSGIProc>(
        GetGLProcAddress("glXWaitVideoSyncSGI"));
  }
}

void DriverGLX::ClearBindings() {
  memset(this, 0, sizeof(*this));
}

void GLXApiBase::glXBindTexImageEXTFn(Display* dpy,
                                      GLXDrawable drawable,
                                      int buffer,
                                      int* attribList) {
  driver_->fn.glXBindTexImageEXTFn(dpy, drawable, buffer, attribList);
}

GLXFBConfig* GLXApiBase::glXChooseFBConfigFn(Display* dpy,
                                             int screen,
                                             const int* attribList,
                                             int* nitems) {
  return driver_->fn.glXChooseFBConfigFn(dpy, screen, attribList, nitems);
}

XVisualInfo* GLXApiBase::glXChooseVisualFn(Display* dpy,
                                           int screen,
                                           int* attribList) {
  return driver_->fn.glXChooseVisualFn(dpy, screen, attribList);
}

void GLXApiBase::glXCopyContextFn(Display* dpy,
                                  GLXContext src,
                                  GLXContext dst,
                                  unsigned long mask) {
  driver_->fn.glXCopyContextFn(dpy, src, dst, mask);
}

void GLXApiBase::glXCopySubBufferMESAFn(Display* dpy,
                                        GLXDrawable drawable,
                                        int x,
                                        int y,
                                        int width,
                                        int height) {
  driver_->fn.glXCopySubBufferMESAFn(dpy, drawable, x, y, width, height);
}

GLXContext GLXApiBase::glXCreateContextFn(Display* dpy,
                                          XVisualInfo* vis,
                                          GLXContext shareList,
                                          int direct) {
  return driver_->fn.glXCreateContextFn(dpy, vis, shareList, direct);
}

GLXContext GLXApiBase::glXCreateContextAttribsARBFn(Display* dpy,
                                                    GLXFBConfig config,
                                                    GLXContext share_context,
                                                    int direct,
                                                    const int* attrib_list) {
  return driver_->fn.glXCreateContextAttribsARBFn(dpy, config, share_context,
                                                  direct, attrib_list);
}

GLXPixmap GLXApiBase::glXCreateGLXPixmapFn(Display* dpy,
                                           XVisualInfo* visual,
                                           Pixmap pixmap) {
  return driver_->fn.glXCreateGLXPixmapFn(dpy, visual, pixmap);
}

GLXContext GLXApiBase::glXCreateNewContextFn(Display* dpy,
                                             GLXFBConfig config,
                                             int renderType,
                                             GLXContext shareList,
                                             int direct) {
  return driver_->fn.glXCreateNewContextFn(dpy, config, renderType, shareList,
                                           direct);
}

GLXPbuffer GLXApiBase::glXCreatePbufferFn(Display* dpy,
                                          GLXFBConfig config,
                                          const int* attribList) {
  return driver_->fn.glXCreatePbufferFn(dpy, config, attribList);
}

GLXPixmap GLXApiBase::glXCreatePixmapFn(Display* dpy,
                                        GLXFBConfig config,
                                        Pixmap pixmap,
                                        const int* attribList) {
  return driver_->fn.glXCreatePixmapFn(dpy, config, pixmap, attribList);
}

GLXWindow GLXApiBase::glXCreateWindowFn(Display* dpy,
                                        GLXFBConfig config,
                                        Window win,
                                        const int* attribList) {
  return driver_->fn.glXCreateWindowFn(dpy, config, win, attribList);
}

void GLXApiBase::glXDestroyContextFn(Display* dpy, GLXContext ctx) {
  driver_->fn.glXDestroyContextFn(dpy, ctx);
}

void GLXApiBase::glXDestroyGLXPixmapFn(Display* dpy, GLXPixmap pixmap) {
  driver_->fn.glXDestroyGLXPixmapFn(dpy, pixmap);
}

void GLXApiBase::glXDestroyPbufferFn(Display* dpy, GLXPbuffer pbuf) {
  driver_->fn.glXDestroyPbufferFn(dpy, pbuf);
}

void GLXApiBase::glXDestroyPixmapFn(Display* dpy, GLXPixmap pixmap) {
  driver_->fn.glXDestroyPixmapFn(dpy, pixmap);
}

void GLXApiBase::glXDestroyWindowFn(Display* dpy, GLXWindow window) {
  driver_->fn.glXDestroyWindowFn(dpy, window);
}

const char* GLXApiBase::glXGetClientStringFn(Display* dpy, int name) {
  return driver_->fn.glXGetClientStringFn(dpy, name);
}

int GLXApiBase::glXGetConfigFn(Display* dpy,
                               XVisualInfo* visual,
                               int attrib,
                               int* value) {
  return driver_->fn.glXGetConfigFn(dpy, visual, attrib, value);
}

GLXContext GLXApiBase::glXGetCurrentContextFn(void) {
  return driver_->fn.glXGetCurrentContextFn();
}

Display* GLXApiBase::glXGetCurrentDisplayFn(void) {
  return driver_->fn.glXGetCurrentDisplayFn();
}

GLXDrawable GLXApiBase::glXGetCurrentDrawableFn(void) {
  return driver_->fn.glXGetCurrentDrawableFn();
}

GLXDrawable GLXApiBase::glXGetCurrentReadDrawableFn(void) {
  return driver_->fn.glXGetCurrentReadDrawableFn();
}

int GLXApiBase::glXGetFBConfigAttribFn(Display* dpy,
                                       GLXFBConfig config,
                                       int attribute,
                                       int* value) {
  return driver_->fn.glXGetFBConfigAttribFn(dpy, config, attribute, value);
}

GLXFBConfig GLXApiBase::glXGetFBConfigFromVisualSGIXFn(
    Display* dpy,
    XVisualInfo* visualInfo) {
  return driver_->fn.glXGetFBConfigFromVisualSGIXFn(dpy, visualInfo);
}

GLXFBConfig* GLXApiBase::glXGetFBConfigsFn(Display* dpy,
                                           int screen,
                                           int* nelements) {
  return driver_->fn.glXGetFBConfigsFn(dpy, screen, nelements);
}

bool GLXApiBase::glXGetMscRateOMLFn(Display* dpy,
                                    GLXDrawable drawable,
                                    int32_t* numerator,
                                    int32_t* denominator) {
  return driver_->fn.glXGetMscRateOMLFn(dpy, drawable, numerator, denominator);
}

void GLXApiBase::glXGetSelectedEventFn(Display* dpy,
                                       GLXDrawable drawable,
                                       unsigned long* mask) {
  driver_->fn.glXGetSelectedEventFn(dpy, drawable, mask);
}

bool GLXApiBase::glXGetSyncValuesOMLFn(Display* dpy,
                                       GLXDrawable drawable,
                                       int64_t* ust,
                                       int64_t* msc,
                                       int64_t* sbc) {
  return driver_->fn.glXGetSyncValuesOMLFn(dpy, drawable, ust, msc, sbc);
}

XVisualInfo* GLXApiBase::glXGetVisualFromFBConfigFn(Display* dpy,
                                                    GLXFBConfig config) {
  return driver_->fn.glXGetVisualFromFBConfigFn(dpy, config);
}

int GLXApiBase::glXIsDirectFn(Display* dpy, GLXContext ctx) {
  return driver_->fn.glXIsDirectFn(dpy, ctx);
}

int GLXApiBase::glXMakeContextCurrentFn(Display* dpy,
                                        GLXDrawable draw,
                                        GLXDrawable read,
                                        GLXContext ctx) {
  return driver_->fn.glXMakeContextCurrentFn(dpy, draw, read, ctx);
}

int GLXApiBase::glXMakeCurrentFn(Display* dpy,
                                 GLXDrawable drawable,
                                 GLXContext ctx) {
  return driver_->fn.glXMakeCurrentFn(dpy, drawable, ctx);
}

int GLXApiBase::glXQueryContextFn(Display* dpy,
                                  GLXContext ctx,
                                  int attribute,
                                  int* value) {
  return driver_->fn.glXQueryContextFn(dpy, ctx, attribute, value);
}

void GLXApiBase::glXQueryDrawableFn(Display* dpy,
                                    GLXDrawable draw,
                                    int attribute,
                                    unsigned int* value) {
  driver_->fn.glXQueryDrawableFn(dpy, draw, attribute, value);
}

int GLXApiBase::glXQueryExtensionFn(Display* dpy, int* errorb, int* event) {
  return driver_->fn.glXQueryExtensionFn(dpy, errorb, event);
}

const char* GLXApiBase::glXQueryExtensionsStringFn(Display* dpy, int screen) {
  return driver_->fn.glXQueryExtensionsStringFn(dpy, screen);
}

const char* GLXApiBase::glXQueryServerStringFn(Display* dpy,
                                               int screen,
                                               int name) {
  return driver_->fn.glXQueryServerStringFn(dpy, screen, name);
}

int GLXApiBase::glXQueryVersionFn(Display* dpy, int* maj, int* min) {
  return driver_->fn.glXQueryVersionFn(dpy, maj, min);
}

void GLXApiBase::glXReleaseTexImageEXTFn(Display* dpy,
                                         GLXDrawable drawable,
                                         int buffer) {
  driver_->fn.glXReleaseTexImageEXTFn(dpy, drawable, buffer);
}

void GLXApiBase::glXSelectEventFn(Display* dpy,
                                  GLXDrawable drawable,
                                  unsigned long mask) {
  driver_->fn.glXSelectEventFn(dpy, drawable, mask);
}

void GLXApiBase::glXSwapBuffersFn(Display* dpy, GLXDrawable drawable) {
  driver_->fn.glXSwapBuffersFn(dpy, drawable);
}

void GLXApiBase::glXSwapIntervalEXTFn(Display* dpy,
                                      GLXDrawable drawable,
                                      int interval) {
  driver_->fn.glXSwapIntervalEXTFn(dpy, drawable, interval);
}

void GLXApiBase::glXSwapIntervalMESAFn(unsigned int interval) {
  driver_->fn.glXSwapIntervalMESAFn(interval);
}

void GLXApiBase::glXUseXFontFn(Font font, int first, int count, int list) {
  driver_->fn.glXUseXFontFn(font, first, count, list);
}

void GLXApiBase::glXWaitGLFn(void) {
  driver_->fn.glXWaitGLFn();
}

int GLXApiBase::glXWaitVideoSyncSGIFn(int divisor,
                                      int remainder,
                                      unsigned int* count) {
  return driver_->fn.glXWaitVideoSyncSGIFn(divisor, remainder, count);
}

void GLXApiBase::glXWaitXFn(void) {
  driver_->fn.glXWaitXFn();
}

void TraceGLXApi::glXBindTexImageEXTFn(Display* dpy,
                                       GLXDrawable drawable,
                                       int buffer,
                                       int* attribList) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXBindTexImageEXT");
  glx_api_->glXBindTexImageEXTFn(dpy, drawable, buffer, attribList);
}

GLXFBConfig* TraceGLXApi::glXChooseFBConfigFn(Display* dpy,
                                              int screen,
                                              const int* attribList,
                                              int* nitems) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXChooseFBConfig");
  return glx_api_->glXChooseFBConfigFn(dpy, screen, attribList, nitems);
}

XVisualInfo* TraceGLXApi::glXChooseVisualFn(Display* dpy,
                                            int screen,
                                            int* attribList) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXChooseVisual");
  return glx_api_->glXChooseVisualFn(dpy, screen, attribList);
}

void TraceGLXApi::glXCopyContextFn(Display* dpy,
                                   GLXContext src,
                                   GLXContext dst,
                                   unsigned long mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCopyContext");
  glx_api_->glXCopyContextFn(dpy, src, dst, mask);
}

void TraceGLXApi::glXCopySubBufferMESAFn(Display* dpy,
                                         GLXDrawable drawable,
                                         int x,
                                         int y,
                                         int width,
                                         int height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCopySubBufferMESA");
  glx_api_->glXCopySubBufferMESAFn(dpy, drawable, x, y, width, height);
}

GLXContext TraceGLXApi::glXCreateContextFn(Display* dpy,
                                           XVisualInfo* vis,
                                           GLXContext shareList,
                                           int direct) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreateContext");
  return glx_api_->glXCreateContextFn(dpy, vis, shareList, direct);
}

GLXContext TraceGLXApi::glXCreateContextAttribsARBFn(Display* dpy,
                                                     GLXFBConfig config,
                                                     GLXContext share_context,
                                                     int direct,
                                                     const int* attrib_list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLXAPI::glXCreateContextAttribsARB");
  return glx_api_->glXCreateContextAttribsARBFn(dpy, config, share_context,
                                                direct, attrib_list);
}

GLXPixmap TraceGLXApi::glXCreateGLXPixmapFn(Display* dpy,
                                            XVisualInfo* visual,
                                            Pixmap pixmap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreateGLXPixmap");
  return glx_api_->glXCreateGLXPixmapFn(dpy, visual, pixmap);
}

GLXContext TraceGLXApi::glXCreateNewContextFn(Display* dpy,
                                              GLXFBConfig config,
                                              int renderType,
                                              GLXContext shareList,
                                              int direct) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreateNewContext");
  return glx_api_->glXCreateNewContextFn(dpy, config, renderType, shareList,
                                         direct);
}

GLXPbuffer TraceGLXApi::glXCreatePbufferFn(Display* dpy,
                                           GLXFBConfig config,
                                           const int* attribList) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreatePbuffer");
  return glx_api_->glXCreatePbufferFn(dpy, config, attribList);
}

GLXPixmap TraceGLXApi::glXCreatePixmapFn(Display* dpy,
                                         GLXFBConfig config,
                                         Pixmap pixmap,
                                         const int* attribList) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreatePixmap");
  return glx_api_->glXCreatePixmapFn(dpy, config, pixmap, attribList);
}

GLXWindow TraceGLXApi::glXCreateWindowFn(Display* dpy,
                                         GLXFBConfig config,
                                         Window win,
                                         const int* attribList) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXCreateWindow");
  return glx_api_->glXCreateWindowFn(dpy, config, win, attribList);
}

void TraceGLXApi::glXDestroyContextFn(Display* dpy, GLXContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXDestroyContext");
  glx_api_->glXDestroyContextFn(dpy, ctx);
}

void TraceGLXApi::glXDestroyGLXPixmapFn(Display* dpy, GLXPixmap pixmap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXDestroyGLXPixmap");
  glx_api_->glXDestroyGLXPixmapFn(dpy, pixmap);
}

void TraceGLXApi::glXDestroyPbufferFn(Display* dpy, GLXPbuffer pbuf) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXDestroyPbuffer");
  glx_api_->glXDestroyPbufferFn(dpy, pbuf);
}

void TraceGLXApi::glXDestroyPixmapFn(Display* dpy, GLXPixmap pixmap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXDestroyPixmap");
  glx_api_->glXDestroyPixmapFn(dpy, pixmap);
}

void TraceGLXApi::glXDestroyWindowFn(Display* dpy, GLXWindow window) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXDestroyWindow");
  glx_api_->glXDestroyWindowFn(dpy, window);
}

const char* TraceGLXApi::glXGetClientStringFn(Display* dpy, int name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetClientString");
  return glx_api_->glXGetClientStringFn(dpy, name);
}

int TraceGLXApi::glXGetConfigFn(Display* dpy,
                                XVisualInfo* visual,
                                int attrib,
                                int* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetConfig");
  return glx_api_->glXGetConfigFn(dpy, visual, attrib, value);
}

GLXContext TraceGLXApi::glXGetCurrentContextFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetCurrentContext");
  return glx_api_->glXGetCurrentContextFn();
}

Display* TraceGLXApi::glXGetCurrentDisplayFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetCurrentDisplay");
  return glx_api_->glXGetCurrentDisplayFn();
}

GLXDrawable TraceGLXApi::glXGetCurrentDrawableFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetCurrentDrawable");
  return glx_api_->glXGetCurrentDrawableFn();
}

GLXDrawable TraceGLXApi::glXGetCurrentReadDrawableFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLXAPI::glXGetCurrentReadDrawable");
  return glx_api_->glXGetCurrentReadDrawableFn();
}

int TraceGLXApi::glXGetFBConfigAttribFn(Display* dpy,
                                        GLXFBConfig config,
                                        int attribute,
                                        int* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetFBConfigAttrib");
  return glx_api_->glXGetFBConfigAttribFn(dpy, config, attribute, value);
}

GLXFBConfig TraceGLXApi::glXGetFBConfigFromVisualSGIXFn(
    Display* dpy,
    XVisualInfo* visualInfo) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLXAPI::glXGetFBConfigFromVisualSGIX");
  return glx_api_->glXGetFBConfigFromVisualSGIXFn(dpy, visualInfo);
}

GLXFBConfig* TraceGLXApi::glXGetFBConfigsFn(Display* dpy,
                                            int screen,
                                            int* nelements) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetFBConfigs");
  return glx_api_->glXGetFBConfigsFn(dpy, screen, nelements);
}

bool TraceGLXApi::glXGetMscRateOMLFn(Display* dpy,
                                     GLXDrawable drawable,
                                     int32_t* numerator,
                                     int32_t* denominator) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetMscRateOML");
  return glx_api_->glXGetMscRateOMLFn(dpy, drawable, numerator, denominator);
}

void TraceGLXApi::glXGetSelectedEventFn(Display* dpy,
                                        GLXDrawable drawable,
                                        unsigned long* mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetSelectedEvent");
  glx_api_->glXGetSelectedEventFn(dpy, drawable, mask);
}

bool TraceGLXApi::glXGetSyncValuesOMLFn(Display* dpy,
                                        GLXDrawable drawable,
                                        int64_t* ust,
                                        int64_t* msc,
                                        int64_t* sbc) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetSyncValuesOML");
  return glx_api_->glXGetSyncValuesOMLFn(dpy, drawable, ust, msc, sbc);
}

XVisualInfo* TraceGLXApi::glXGetVisualFromFBConfigFn(Display* dpy,
                                                     GLXFBConfig config) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXGetVisualFromFBConfig");
  return glx_api_->glXGetVisualFromFBConfigFn(dpy, config);
}

int TraceGLXApi::glXIsDirectFn(Display* dpy, GLXContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXIsDirect");
  return glx_api_->glXIsDirectFn(dpy, ctx);
}

int TraceGLXApi::glXMakeContextCurrentFn(Display* dpy,
                                         GLXDrawable draw,
                                         GLXDrawable read,
                                         GLXContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXMakeContextCurrent");
  return glx_api_->glXMakeContextCurrentFn(dpy, draw, read, ctx);
}

int TraceGLXApi::glXMakeCurrentFn(Display* dpy,
                                  GLXDrawable drawable,
                                  GLXContext ctx) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXMakeCurrent");
  return glx_api_->glXMakeCurrentFn(dpy, drawable, ctx);
}

int TraceGLXApi::glXQueryContextFn(Display* dpy,
                                   GLXContext ctx,
                                   int attribute,
                                   int* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryContext");
  return glx_api_->glXQueryContextFn(dpy, ctx, attribute, value);
}

void TraceGLXApi::glXQueryDrawableFn(Display* dpy,
                                     GLXDrawable draw,
                                     int attribute,
                                     unsigned int* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryDrawable");
  glx_api_->glXQueryDrawableFn(dpy, draw, attribute, value);
}

int TraceGLXApi::glXQueryExtensionFn(Display* dpy, int* errorb, int* event) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryExtension");
  return glx_api_->glXQueryExtensionFn(dpy, errorb, event);
}

const char* TraceGLXApi::glXQueryExtensionsStringFn(Display* dpy, int screen) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryExtensionsString");
  return glx_api_->glXQueryExtensionsStringFn(dpy, screen);
}

const char* TraceGLXApi::glXQueryServerStringFn(Display* dpy,
                                                int screen,
                                                int name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryServerString");
  return glx_api_->glXQueryServerStringFn(dpy, screen, name);
}

int TraceGLXApi::glXQueryVersionFn(Display* dpy, int* maj, int* min) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXQueryVersion");
  return glx_api_->glXQueryVersionFn(dpy, maj, min);
}

void TraceGLXApi::glXReleaseTexImageEXTFn(Display* dpy,
                                          GLXDrawable drawable,
                                          int buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXReleaseTexImageEXT");
  glx_api_->glXReleaseTexImageEXTFn(dpy, drawable, buffer);
}

void TraceGLXApi::glXSelectEventFn(Display* dpy,
                                   GLXDrawable drawable,
                                   unsigned long mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXSelectEvent");
  glx_api_->glXSelectEventFn(dpy, drawable, mask);
}

void TraceGLXApi::glXSwapBuffersFn(Display* dpy, GLXDrawable drawable) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXSwapBuffers");
  glx_api_->glXSwapBuffersFn(dpy, drawable);
}

void TraceGLXApi::glXSwapIntervalEXTFn(Display* dpy,
                                       GLXDrawable drawable,
                                       int interval) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXSwapIntervalEXT");
  glx_api_->glXSwapIntervalEXTFn(dpy, drawable, interval);
}

void TraceGLXApi::glXSwapIntervalMESAFn(unsigned int interval) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXSwapIntervalMESA");
  glx_api_->glXSwapIntervalMESAFn(interval);
}

void TraceGLXApi::glXUseXFontFn(Font font, int first, int count, int list) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXUseXFont");
  glx_api_->glXUseXFontFn(font, first, count, list);
}

void TraceGLXApi::glXWaitGLFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXWaitGL");
  glx_api_->glXWaitGLFn();
}

int TraceGLXApi::glXWaitVideoSyncSGIFn(int divisor,
                                       int remainder,
                                       unsigned int* count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXWaitVideoSyncSGI");
  return glx_api_->glXWaitVideoSyncSGIFn(divisor, remainder, count);
}

void TraceGLXApi::glXWaitXFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLXAPI::glXWaitX");
  glx_api_->glXWaitXFn();
}

void LogGLXApi::glXBindTexImageEXTFn(Display* dpy,
                                     GLXDrawable drawable,
                                     int buffer,
                                     int* attribList) {
  GL_SERVICE_LOG("glXBindTexImageEXT"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << buffer << ", "
                 << static_cast<const void*>(attribList) << ")");
  glx_api_->glXBindTexImageEXTFn(dpy, drawable, buffer, attribList);
}

GLXFBConfig* LogGLXApi::glXChooseFBConfigFn(Display* dpy,
                                            int screen,
                                            const int* attribList,
                                            int* nitems) {
  GL_SERVICE_LOG("glXChooseFBConfig"
                 << "(" << static_cast<const void*>(dpy) << ", " << screen
                 << ", " << static_cast<const void*>(attribList) << ", "
                 << static_cast<const void*>(nitems) << ")");
  GLXFBConfig* result =
      glx_api_->glXChooseFBConfigFn(dpy, screen, attribList, nitems);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

XVisualInfo* LogGLXApi::glXChooseVisualFn(Display* dpy,
                                          int screen,
                                          int* attribList) {
  GL_SERVICE_LOG("glXChooseVisual"
                 << "(" << static_cast<const void*>(dpy) << ", " << screen
                 << ", " << static_cast<const void*>(attribList) << ")");
  XVisualInfo* result = glx_api_->glXChooseVisualFn(dpy, screen, attribList);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXCopyContextFn(Display* dpy,
                                 GLXContext src,
                                 GLXContext dst,
                                 unsigned long mask) {
  GL_SERVICE_LOG("glXCopyContext"
                 << "(" << static_cast<const void*>(dpy) << ", " << src << ", "
                 << dst << ", " << mask << ")");
  glx_api_->glXCopyContextFn(dpy, src, dst, mask);
}

void LogGLXApi::glXCopySubBufferMESAFn(Display* dpy,
                                       GLXDrawable drawable,
                                       int x,
                                       int y,
                                       int width,
                                       int height) {
  GL_SERVICE_LOG("glXCopySubBufferMESA"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << x << ", " << y << ", " << width << ", " << height
                 << ")");
  glx_api_->glXCopySubBufferMESAFn(dpy, drawable, x, y, width, height);
}

GLXContext LogGLXApi::glXCreateContextFn(Display* dpy,
                                         XVisualInfo* vis,
                                         GLXContext shareList,
                                         int direct) {
  GL_SERVICE_LOG("glXCreateContext"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(vis) << ", " << shareList << ", "
                 << direct << ")");
  GLXContext result = glx_api_->glXCreateContextFn(dpy, vis, shareList, direct);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXContext LogGLXApi::glXCreateContextAttribsARBFn(Display* dpy,
                                                   GLXFBConfig config,
                                                   GLXContext share_context,
                                                   int direct,
                                                   const int* attrib_list) {
  GL_SERVICE_LOG("glXCreateContextAttribsARB"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << share_context << ", " << direct << ", "
                 << static_cast<const void*>(attrib_list) << ")");
  GLXContext result = glx_api_->glXCreateContextAttribsARBFn(
      dpy, config, share_context, direct, attrib_list);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXPixmap LogGLXApi::glXCreateGLXPixmapFn(Display* dpy,
                                          XVisualInfo* visual,
                                          Pixmap pixmap) {
  GL_SERVICE_LOG("glXCreateGLXPixmap"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(visual) << ", " << pixmap << ")");
  GLXPixmap result = glx_api_->glXCreateGLXPixmapFn(dpy, visual, pixmap);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXContext LogGLXApi::glXCreateNewContextFn(Display* dpy,
                                            GLXFBConfig config,
                                            int renderType,
                                            GLXContext shareList,
                                            int direct) {
  GL_SERVICE_LOG("glXCreateNewContext"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << renderType << ", " << shareList << ", " << direct
                 << ")");
  GLXContext result = glx_api_->glXCreateNewContextFn(dpy, config, renderType,
                                                      shareList, direct);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXPbuffer LogGLXApi::glXCreatePbufferFn(Display* dpy,
                                         GLXFBConfig config,
                                         const int* attribList) {
  GL_SERVICE_LOG("glXCreatePbuffer"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << static_cast<const void*>(attribList) << ")");
  GLXPbuffer result = glx_api_->glXCreatePbufferFn(dpy, config, attribList);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXPixmap LogGLXApi::glXCreatePixmapFn(Display* dpy,
                                       GLXFBConfig config,
                                       Pixmap pixmap,
                                       const int* attribList) {
  GL_SERVICE_LOG("glXCreatePixmap"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << pixmap << ", "
                 << static_cast<const void*>(attribList) << ")");
  GLXPixmap result =
      glx_api_->glXCreatePixmapFn(dpy, config, pixmap, attribList);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXWindow LogGLXApi::glXCreateWindowFn(Display* dpy,
                                       GLXFBConfig config,
                                       Window win,
                                       const int* attribList) {
  GL_SERVICE_LOG("glXCreateWindow"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << win << ", " << static_cast<const void*>(attribList)
                 << ")");
  GLXWindow result = glx_api_->glXCreateWindowFn(dpy, config, win, attribList);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXDestroyContextFn(Display* dpy, GLXContext ctx) {
  GL_SERVICE_LOG("glXDestroyContext"
                 << "(" << static_cast<const void*>(dpy) << ", " << ctx << ")");
  glx_api_->glXDestroyContextFn(dpy, ctx);
}

void LogGLXApi::glXDestroyGLXPixmapFn(Display* dpy, GLXPixmap pixmap) {
  GL_SERVICE_LOG("glXDestroyGLXPixmap"
                 << "(" << static_cast<const void*>(dpy) << ", " << pixmap
                 << ")");
  glx_api_->glXDestroyGLXPixmapFn(dpy, pixmap);
}

void LogGLXApi::glXDestroyPbufferFn(Display* dpy, GLXPbuffer pbuf) {
  GL_SERVICE_LOG("glXDestroyPbuffer"
                 << "(" << static_cast<const void*>(dpy) << ", " << pbuf
                 << ")");
  glx_api_->glXDestroyPbufferFn(dpy, pbuf);
}

void LogGLXApi::glXDestroyPixmapFn(Display* dpy, GLXPixmap pixmap) {
  GL_SERVICE_LOG("glXDestroyPixmap"
                 << "(" << static_cast<const void*>(dpy) << ", " << pixmap
                 << ")");
  glx_api_->glXDestroyPixmapFn(dpy, pixmap);
}

void LogGLXApi::glXDestroyWindowFn(Display* dpy, GLXWindow window) {
  GL_SERVICE_LOG("glXDestroyWindow"
                 << "(" << static_cast<const void*>(dpy) << ", " << window
                 << ")");
  glx_api_->glXDestroyWindowFn(dpy, window);
}

const char* LogGLXApi::glXGetClientStringFn(Display* dpy, int name) {
  GL_SERVICE_LOG("glXGetClientString"
                 << "(" << static_cast<const void*>(dpy) << ", " << name
                 << ")");
  const char* result = glx_api_->glXGetClientStringFn(dpy, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXGetConfigFn(Display* dpy,
                              XVisualInfo* visual,
                              int attrib,
                              int* value) {
  GL_SERVICE_LOG("glXGetConfig"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(visual) << ", " << attrib << ", "
                 << static_cast<const void*>(value) << ")");
  int result = glx_api_->glXGetConfigFn(dpy, visual, attrib, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXContext LogGLXApi::glXGetCurrentContextFn(void) {
  GL_SERVICE_LOG("glXGetCurrentContext"
                 << "("
                 << ")");
  GLXContext result = glx_api_->glXGetCurrentContextFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

Display* LogGLXApi::glXGetCurrentDisplayFn(void) {
  GL_SERVICE_LOG("glXGetCurrentDisplay"
                 << "("
                 << ")");
  Display* result = glx_api_->glXGetCurrentDisplayFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXDrawable LogGLXApi::glXGetCurrentDrawableFn(void) {
  GL_SERVICE_LOG("glXGetCurrentDrawable"
                 << "("
                 << ")");
  GLXDrawable result = glx_api_->glXGetCurrentDrawableFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXDrawable LogGLXApi::glXGetCurrentReadDrawableFn(void) {
  GL_SERVICE_LOG("glXGetCurrentReadDrawable"
                 << "("
                 << ")");
  GLXDrawable result = glx_api_->glXGetCurrentReadDrawableFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXGetFBConfigAttribFn(Display* dpy,
                                      GLXFBConfig config,
                                      int attribute,
                                      int* value) {
  GL_SERVICE_LOG("glXGetFBConfigAttrib"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ", " << attribute << ", " << static_cast<const void*>(value)
                 << ")");
  int result = glx_api_->glXGetFBConfigAttribFn(dpy, config, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXFBConfig LogGLXApi::glXGetFBConfigFromVisualSGIXFn(Display* dpy,
                                                      XVisualInfo* visualInfo) {
  GL_SERVICE_LOG("glXGetFBConfigFromVisualSGIX"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(visualInfo) << ")");
  GLXFBConfig result =
      glx_api_->glXGetFBConfigFromVisualSGIXFn(dpy, visualInfo);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLXFBConfig* LogGLXApi::glXGetFBConfigsFn(Display* dpy,
                                          int screen,
                                          int* nelements) {
  GL_SERVICE_LOG("glXGetFBConfigs"
                 << "(" << static_cast<const void*>(dpy) << ", " << screen
                 << ", " << static_cast<const void*>(nelements) << ")");
  GLXFBConfig* result = glx_api_->glXGetFBConfigsFn(dpy, screen, nelements);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

bool LogGLXApi::glXGetMscRateOMLFn(Display* dpy,
                                   GLXDrawable drawable,
                                   int32_t* numerator,
                                   int32_t* denominator) {
  GL_SERVICE_LOG("glXGetMscRateOML"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << static_cast<const void*>(numerator) << ", "
                 << static_cast<const void*>(denominator) << ")");
  bool result =
      glx_api_->glXGetMscRateOMLFn(dpy, drawable, numerator, denominator);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXGetSelectedEventFn(Display* dpy,
                                      GLXDrawable drawable,
                                      unsigned long* mask) {
  GL_SERVICE_LOG("glXGetSelectedEvent"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << static_cast<const void*>(mask) << ")");
  glx_api_->glXGetSelectedEventFn(dpy, drawable, mask);
}

bool LogGLXApi::glXGetSyncValuesOMLFn(Display* dpy,
                                      GLXDrawable drawable,
                                      int64_t* ust,
                                      int64_t* msc,
                                      int64_t* sbc) {
  GL_SERVICE_LOG("glXGetSyncValuesOML"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << static_cast<const void*>(ust) << ", "
                 << static_cast<const void*>(msc) << ", "
                 << static_cast<const void*>(sbc) << ")");
  bool result = glx_api_->glXGetSyncValuesOMLFn(dpy, drawable, ust, msc, sbc);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

XVisualInfo* LogGLXApi::glXGetVisualFromFBConfigFn(Display* dpy,
                                                   GLXFBConfig config) {
  GL_SERVICE_LOG("glXGetVisualFromFBConfig"
                 << "(" << static_cast<const void*>(dpy) << ", " << config
                 << ")");
  XVisualInfo* result = glx_api_->glXGetVisualFromFBConfigFn(dpy, config);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXIsDirectFn(Display* dpy, GLXContext ctx) {
  GL_SERVICE_LOG("glXIsDirect"
                 << "(" << static_cast<const void*>(dpy) << ", " << ctx << ")");
  int result = glx_api_->glXIsDirectFn(dpy, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXMakeContextCurrentFn(Display* dpy,
                                       GLXDrawable draw,
                                       GLXDrawable read,
                                       GLXContext ctx) {
  GL_SERVICE_LOG("glXMakeContextCurrent"
                 << "(" << static_cast<const void*>(dpy) << ", " << draw << ", "
                 << read << ", " << ctx << ")");
  int result = glx_api_->glXMakeContextCurrentFn(dpy, draw, read, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXMakeCurrentFn(Display* dpy,
                                GLXDrawable drawable,
                                GLXContext ctx) {
  GL_SERVICE_LOG("glXMakeCurrent"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << ctx << ")");
  int result = glx_api_->glXMakeCurrentFn(dpy, drawable, ctx);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXQueryContextFn(Display* dpy,
                                 GLXContext ctx,
                                 int attribute,
                                 int* value) {
  GL_SERVICE_LOG("glXQueryContext"
                 << "(" << static_cast<const void*>(dpy) << ", " << ctx << ", "
                 << attribute << ", " << static_cast<const void*>(value)
                 << ")");
  int result = glx_api_->glXQueryContextFn(dpy, ctx, attribute, value);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXQueryDrawableFn(Display* dpy,
                                   GLXDrawable draw,
                                   int attribute,
                                   unsigned int* value) {
  GL_SERVICE_LOG("glXQueryDrawable"
                 << "(" << static_cast<const void*>(dpy) << ", " << draw << ", "
                 << attribute << ", " << static_cast<const void*>(value)
                 << ")");
  glx_api_->glXQueryDrawableFn(dpy, draw, attribute, value);
}

int LogGLXApi::glXQueryExtensionFn(Display* dpy, int* errorb, int* event) {
  GL_SERVICE_LOG("glXQueryExtension"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(errorb) << ", "
                 << static_cast<const void*>(event) << ")");
  int result = glx_api_->glXQueryExtensionFn(dpy, errorb, event);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* LogGLXApi::glXQueryExtensionsStringFn(Display* dpy, int screen) {
  GL_SERVICE_LOG("glXQueryExtensionsString"
                 << "(" << static_cast<const void*>(dpy) << ", " << screen
                 << ")");
  const char* result = glx_api_->glXQueryExtensionsStringFn(dpy, screen);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const char* LogGLXApi::glXQueryServerStringFn(Display* dpy,
                                              int screen,
                                              int name) {
  GL_SERVICE_LOG("glXQueryServerString"
                 << "(" << static_cast<const void*>(dpy) << ", " << screen
                 << ", " << name << ")");
  const char* result = glx_api_->glXQueryServerStringFn(dpy, screen, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

int LogGLXApi::glXQueryVersionFn(Display* dpy, int* maj, int* min) {
  GL_SERVICE_LOG("glXQueryVersion"
                 << "(" << static_cast<const void*>(dpy) << ", "
                 << static_cast<const void*>(maj) << ", "
                 << static_cast<const void*>(min) << ")");
  int result = glx_api_->glXQueryVersionFn(dpy, maj, min);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXReleaseTexImageEXTFn(Display* dpy,
                                        GLXDrawable drawable,
                                        int buffer) {
  GL_SERVICE_LOG("glXReleaseTexImageEXT"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << buffer << ")");
  glx_api_->glXReleaseTexImageEXTFn(dpy, drawable, buffer);
}

void LogGLXApi::glXSelectEventFn(Display* dpy,
                                 GLXDrawable drawable,
                                 unsigned long mask) {
  GL_SERVICE_LOG("glXSelectEvent"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << mask << ")");
  glx_api_->glXSelectEventFn(dpy, drawable, mask);
}

void LogGLXApi::glXSwapBuffersFn(Display* dpy, GLXDrawable drawable) {
  GL_SERVICE_LOG("glXSwapBuffers"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ")");
  glx_api_->glXSwapBuffersFn(dpy, drawable);
}

void LogGLXApi::glXSwapIntervalEXTFn(Display* dpy,
                                     GLXDrawable drawable,
                                     int interval) {
  GL_SERVICE_LOG("glXSwapIntervalEXT"
                 << "(" << static_cast<const void*>(dpy) << ", " << drawable
                 << ", " << interval << ")");
  glx_api_->glXSwapIntervalEXTFn(dpy, drawable, interval);
}

void LogGLXApi::glXSwapIntervalMESAFn(unsigned int interval) {
  GL_SERVICE_LOG("glXSwapIntervalMESA"
                 << "(" << interval << ")");
  glx_api_->glXSwapIntervalMESAFn(interval);
}

void LogGLXApi::glXUseXFontFn(Font font, int first, int count, int list) {
  GL_SERVICE_LOG("glXUseXFont"
                 << "(" << font << ", " << first << ", " << count << ", "
                 << list << ")");
  glx_api_->glXUseXFontFn(font, first, count, list);
}

void LogGLXApi::glXWaitGLFn(void) {
  GL_SERVICE_LOG("glXWaitGL"
                 << "("
                 << ")");
  glx_api_->glXWaitGLFn();
}

int LogGLXApi::glXWaitVideoSyncSGIFn(int divisor,
                                     int remainder,
                                     unsigned int* count) {
  GL_SERVICE_LOG("glXWaitVideoSyncSGI"
                 << "(" << divisor << ", " << remainder << ", "
                 << static_cast<const void*>(count) << ")");
  int result = glx_api_->glXWaitVideoSyncSGIFn(divisor, remainder, count);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void LogGLXApi::glXWaitXFn(void) {
  GL_SERVICE_LOG("glXWaitX"
                 << "("
                 << ")");
  glx_api_->glXWaitXFn();
}

}  // namespace gl
