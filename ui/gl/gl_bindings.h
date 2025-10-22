// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_BINDINGS_H_
#define UI_GL_GL_BINDINGS_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

// Includes the platform independent and platform dependent GL headers.

// GL headers may include inttypes.h and so we need to ensure that
// __STDC_FORMAT_MACROS is defined in order for //base/format_macros.h to
// function correctly. See comment and #error message in //base/format_macros.h
// for details.
#if BUILDFLAG(IS_POSIX) && !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

// The GL and EGL headers below are included in a specific order with comments
// separating them to avoid the code formatter re-ordering them.

// Core GL headers must be included before the glext headers
#include <GLES3/gl32.h>

// glext headers reference the GL enums added in the core GL headers
#include <GLES2/gl2ext.h>
#include <GLES2/gl2ext_angle.h>

// Core EGL headers can be included after GL
#include <EGL/egl.h>

// eglext headers reference EGL eums added in the core EGL headers
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_export.h"

// GLClampd is only defined in the desktop headers
typedef double GLclampd;

// Desktop GL-only enable state
#define GL_PRIMITIVE_RESTART 0x8F9D

#define GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM         0x9243
#define GL_BIND_GENERATES_RESOURCE_CHROMIUM              0x9244

// GL_ARB_occlusion_query
#define GL_SAMPLES_PASSED_ARB                            0x8914

// GL_CHROMIUM_command_buffer_query
#define GL_COMMANDS_ISSUED_CHROMIUM                      0x6004

/* GL_CHROMIUM_get_error_query */
#define GL_GET_ERROR_QUERY_CHROMIUM                      0x6003

/* GL_CHROMIUM_program_completion_query */
#define GL_PROGRAM_COMPLETION_QUERY_CHROMIUM 0x6009

/* GL_CHROMIUM_async_pixel_transfers */
#define GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM           0x6006

// GL_CHROMIUM_sync_query
#define GL_COMMANDS_COMPLETED_CHROMIUM                   0x84F7

// GL_CHROMIUM_subscribe_uniforms
#define GL_SUBSCRIBED_VALUES_BUFFER_CHROMIUM             0x924B
#define GL_MOUSE_POSITION_CHROMIUM                       0x924C

// GL_CHROMIUM_pixel_transfer_buffer_object
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM         0x78EC
#define GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM           0x78ED
#define GL_PIXEL_PACK_TRANSFER_BUFFER_BINDING_CHROMIUM   0x78EE
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM 0x78EF

#ifndef GL_EXT_multisample_compatibility
#define GL_EXT_multisample_compatibility 1
#define GL_MULTISAMPLE_EXT 0x809D
#define GL_SAMPLE_ALPHA_TO_ONE_EXT 0x809F
#endif /* GL_EXT_multisample_compatibility */

#ifndef GL_CHROMIUM_nonblocking_readback
#define GL_CHROMIUM_nonblocking_readback 1
#define GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM 0x84F8
#endif /* GL_CHROMIUM_nonblocking_readback */

#ifndef GL_CHROMIUM_shared_image
#define GL_CHROMIUM_shared_image 1
#define GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM 0x8AF6
#define GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM 0x8AF7
#endif /* GL_CHROMIUM_shared_image */

#define GL_GLEXT_PROTOTYPES 1

#if BUILDFLAG(IS_WIN)
#define GL_BINDING_CALL WINAPI
#else
#define GL_BINDING_CALL
#endif

#if defined(NDEBUG) && !defined(GPU_ENABLE_SERVICE_LOGGING)
#define GL_SERVICE_LOG(args) DLOG(INFO) << args;
#define GL_SERVICE_LOG_CODE_BLOCK(code)
#else
#define GL_SERVICE_LOG(args) LOG(INFO) << args;
#define GL_SERVICE_LOG_CODE_BLOCK(code) code
#endif

#define GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT 0x8868

// Forward declare EGL types.
typedef uint64_t EGLuint64CHROMIUM;

#if !defined(BINDINGS_GL_PROTOTYPES)
#define BINDINGS_GL_PROTOTYPES 1
#endif
#include "gl_bindings_autogen_gl.h"

#if !defined(BINDINGS_EGL_PROTOTYPES)
#define BINDINGS_EGL_PROTOTYPES 1
#endif
#include "gl_bindings_autogen_egl.h"

using GLFunctionPointerType = void (*)();
#if BUILDFLAG(IS_WIN)
typedef GLFunctionPointerType(WINAPI* GLGetProcAddressProc)(const char* name);
#define STDCALL __stdcall
#else
typedef GLFunctionPointerType (*GLGetProcAddressProc)(const char* name);
#define STDCALL
#endif

namespace gl {

struct GLVersionInfo;

struct GL_EXPORT DriverGL {
  void InitializeStaticBindings(GLGetProcAddressProc get_proc_address);
  void InitializeDynamicBindings(GLGetProcAddressProc get_proc_address,
                                 const GLVersionInfo* ver,
                                 const gfx::ExtensionSet& extensions);
  void ClearBindings();

  ProcsGL fn;
  ExtensionsGL ext;
};

struct GL_EXPORT CurrentGL {
  raw_ptr<GLApi, DanglingUntriaged> Api = nullptr;
  raw_ptr<DriverGL, DanglingUntriaged> Driver = nullptr;
  raw_ptr<const GLVersionInfo, AcrossTasksDanglingUntriaged> Version = nullptr;
};

struct GL_EXPORT DriverEGL {
  void InitializeStaticBindings(GLGetProcAddressProc get_proc_address);
  void ClearBindings();

  ProcsEGL fn;
  ClientExtensionsEGL client_ext;
};

// This #define is here to support autogenerated code.
#define g_current_gl_context GetThreadLocalCurrentGL()->Api.get()
#define g_current_gl_driver GetThreadLocalCurrentGL()->Driver
#define g_current_gl_version GetThreadLocalCurrentGL()->Version.get()
GL_EXPORT CurrentGL* GetThreadLocalCurrentGL();

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern DriverEGL g_driver_egl;

}  // namespace gl

#endif  // UI_GL_GL_BINDINGS_H_
