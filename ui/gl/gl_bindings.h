// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_BINDINGS_H_
#define UI_GL_GL_BINDINGS_H_

#include "build/build_config.h"

// Includes the platform independent and platform dependent GL headers.

// GL headers may include inttypes.h and so we need to ensure that
// __STDC_FORMAT_MACROS is defined in order for //base/format_macros.h to
// function correctly. See comment and #error message in //base/format_macros.h
// for details.
#if defined(OS_POSIX) && !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif
#if defined(USE_GLX)
// Must be included before GL headers or they might pollute the global
// namespace with X11 macros indirectly.
#include "ui/gfx/x/x11.h"

// GL headers expect Bool and Status this to be defined but we avoid
// defining them since they clash with too much code. Instead we have
// to add them temporarily here and undef them again below.
#define Bool int
#define Status int
#endif  // USE_GLX

#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_export.h"

// The standard OpenGL native extension headers are also included.
#if defined(OS_WIN)
#include <GL/wglext.h>
#elif defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#elif defined(USE_GLX)
#include <GL/glx.h>
#include <GL/glxext.h>

// Done with these temporary macros now
#undef Bool
#undef Status
#endif

// GLES2 defines not part of Desktop GL
// Shader Precision-Specified Types
#define GL_LOW_FLOAT                                     0x8DF0
#define GL_MEDIUM_FLOAT                                  0x8DF1
#define GL_HIGH_FLOAT                                    0x8DF2
#define GL_LOW_INT                                       0x8DF3
#define GL_MEDIUM_INT                                    0x8DF4
#define GL_HIGH_INT                                      0x8DF5
#define GL_IMPLEMENTATION_COLOR_READ_TYPE                0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT              0x8B9B
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS                  0x8DFD
#define GL_MAX_VERTEX_UNIFORM_VECTORS                    0x8DFB
#define GL_MAX_VARYING_VECTORS                           0x8DFC
#define GL_SHADER_BINARY_FORMATS                         0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS                     0x8DF9
#define GL_SHADER_COMPILER                               0x8DFA
#define GL_RGB565                                        0x8D62
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES           0x8B8B
#define GL_RGB8_OES                                      0x8051
#define GL_RGBA8_OES                                     0x8058
#define GL_HALF_FLOAT_OES                                0x8D61

// GL_OES_EGL_image_external
#define GL_TEXTURE_EXTERNAL_OES                          0x8D65
#define GL_SAMPLER_EXTERNAL_OES                          0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES                  0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES              0x8D68

// GL_ANGLE_translated_shader_source
#define GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE         0x93A0

#define GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM         0x9243
#define GL_BIND_GENERATES_RESOURCE_CHROMIUM              0x9244

// GL_ANGLE_texture_usage
#define GL_TEXTURE_USAGE_ANGLE                           0x93A2
#define GL_FRAMEBUFFER_ATTACHMENT_ANGLE                  0x93A3

// GL_EXT_texture_storage
#define GL_TEXTURE_IMMUTABLE_FORMAT_EXT                  0x912F
#define GL_ALPHA8_EXT                                    0x803C
#define GL_LUMINANCE8_EXT                                0x8040
#define GL_LUMINANCE8_ALPHA8_EXT                         0x8045
#define GL_RGB10_A2_EXT                                  0x8059
#define GL_RGBA32F_EXT                                   0x8814
#define GL_RGB32F_EXT                                    0x8815
#define GL_ALPHA32F_EXT                                  0x8816
#define GL_LUMINANCE32F_EXT                              0x8818
#define GL_LUMINANCE_ALPHA32F_EXT                        0x8819
#define GL_RGBA16F_EXT                                   0x881A
#define GL_RGB16F_EXT                                    0x881B
#define GL_RG16F_EXT                                     0x822F
#define GL_R16F_EXT                                      0x822D
#define GL_ALPHA16F_EXT                                  0x881C
#define GL_LUMINANCE16F_EXT                              0x881E
#define GL_LUMINANCE_ALPHA16F_EXT                        0x881F
#define GL_R32F_EXT                                      0x822E
#define GL_RG32F_EXT                                     0x8230
#define GL_BGRA8_EXT                                     0x93A1

// GL_ANGLE_instanced_arrays
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE             0x88FE

// GL_ANGLE_client_arrays
#define GL_CLIENT_ARRAYS_ANGLE 0x93AA

// GL_ANGLE_robust_resource_initialization
#define GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x93AB

// GL_ANGLE_request_extension
#define GL_REQUESTABLE_EXTENSIONS_ANGLE 0x93A8
#define GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE 0x93A9

// GL_ANGLE_memory_size
#define GL_MEMORY_SIZE_ANGLE 0x93AD

// GL_EXT_occlusion_query_boolean
#define GL_ANY_SAMPLES_PASSED_EXT                        0x8C2F
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT           0x8D6A
#define GL_CURRENT_QUERY_EXT                             0x8865
#define GL_QUERY_RESULT_EXT                              0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT                    0x8867

// GL_ARB_occlusion_query
#define GL_SAMPLES_PASSED_ARB                            0x8914

// GL_CHROMIUM_command_buffer_query
#define GL_COMMANDS_ISSUED_CHROMIUM                      0x6004
#define GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM            0x6005

/* GL_CHROMIUM_get_error_query */
#define GL_GET_ERROR_QUERY_CHROMIUM                      0x6003

/* GL_CHROMIUM_command_buffer_latency_query */
#define GL_LATENCY_QUERY_CHROMIUM                        0x6007

/* GL_CHROMIUM_program_completion_query */
#define GL_PROGRAM_COMPLETION_QUERY_CHROMIUM 0x6009

/* GL_CHROMIUM_async_pixel_transfers */
#define GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM           0x6006

// GL_CHROMIUM_sync_query
#define GL_COMMANDS_COMPLETED_CHROMIUM                   0x84F7

// GL_CHROMIUM_ycrcb_420_image
#define GL_RGB_YCRCB_420_CHROMIUM                        0x78FA

// GL_CHROMIUM_ycbcr_422_image
#define GL_RGB_YCBCR_422_CHROMIUM                        0x78FB

// GL_CHROMIUM_ycbcr_420v_image
#define GL_RGB_YCBCR_420V_CHROMIUM 0x78FC

// GL_CHROMIUM_ycbcr_p010_image
#define GL_RGB_YCBCR_P010_CHROMIUM 0x78FD

// GL_CHROMIUM_schedule_overlay_plane
#define GL_OVERLAY_TRANSFORM_NONE_CHROMIUM               0x9245
#define GL_OVERLAY_TRANSFORM_FLIP_HORIZONTAL_CHROMIUM    0x9246
#define GL_OVERLAY_TRANSFORM_FLIP_VERTICAL_CHROMIUM      0x9247
#define GL_OVERLAY_TRANSFORM_ROTATE_90_CHROMIUM          0x9248
#define GL_OVERLAY_TRANSFORM_ROTATE_180_CHROMIUM         0x9249
#define GL_OVERLAY_TRANSFORM_ROTATE_270_CHROMIUM         0x924A

// GL_CHROMIUM_subscribe_uniforms
#define GL_SUBSCRIBED_VALUES_BUFFER_CHROMIUM             0x924B
#define GL_MOUSE_POSITION_CHROMIUM                       0x924C

// GL_CHROMIUM_texture_filtering_hint
#define GL_TEXTURE_FILTERING_HINT_CHROMIUM               0x8AF0

// GL_CHROMIUM_resize
#define GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM 0x8AF1
#define GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM 0x8AF2
#define GL_COLOR_SPACE_SRGB_CHROMIUM 0x8AF3
#define GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM 0x8AF4
#define GL_COLOR_SPACE_HDR10_CHROMIUM 0x8AF5

// GL_CHROMIUM_texture_storage_image
#define GL_SCANOUT_CHROMIUM 0x6000

// GL_OES_texure_3D
#define GL_SAMPLER_3D_OES                                0x8B5F

// GL_OES_depth24
#define GL_DEPTH_COMPONENT24_OES                         0x81A6

// GL_OES_depth32
#define GL_DEPTH_COMPONENT32_OES                         0x81A7

// GL_OES_packed_depth_stencil
#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES                          0x88F0
#endif

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8                              0x88F0
#endif

// GL_OES_compressed_ETC1_RGB8_texture
#define GL_ETC1_RGB8_OES                                 0x8D64

// GL_AMD_compressed_ATC_texture
#define GL_ATC_RGB_AMD                                   0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD                   0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD               0x87EE

// GL_IMG_texture_compression_pvrtc
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG               0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG               0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG              0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG              0x8C03

// GL_OES_vertex_array_object
#define GL_VERTEX_ARRAY_BINDING_OES                      0x85B5

// GL_CHROMIUM_pixel_transfer_buffer_object
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM         0x78EC
#define GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM           0x78ED
#define GL_PIXEL_PACK_TRANSFER_BUFFER_BINDING_CHROMIUM   0x78EE
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM 0x78EF

/* GL_EXT_discard_framebuffer */
#ifndef GL_EXT_discard_framebuffer
#define GL_COLOR_EXT                                     0x1800
#define GL_DEPTH_EXT                                     0x1801
#define GL_STENCIL_EXT                                   0x1802
#endif

// GL_EXT_sRGB
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT     0x8210

// GL_ARB_get_program_binary
#define PROGRAM_BINARY_RETRIEVABLE_HINT                  0x8257
// GL_OES_get_program_binary
#define GL_PROGRAM_BINARY_LENGTH_OES                     0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS_OES                0x87FE
#define GL_PROGRAM_BINARY_FORMATS_OES                    0x87FF

#ifndef GL_EXT_multisampled_render_to_texture
#define GL_RENDERBUFFER_SAMPLES_EXT                      0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT        0x8D56
#define GL_MAX_SAMPLES_EXT                               0x8D57
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT    0x8D6C
#endif

#ifndef GL_IMG_multisampled_render_to_texture
#define GL_RENDERBUFFER_SAMPLES_IMG                      0x9133
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG        0x9134
#define GL_MAX_SAMPLES_IMG                               0x9135
#define GL_TEXTURE_SAMPLES_IMG                           0x9136
#endif

#ifndef GL_CHROMIUM_path_rendering
#define GL_CHROMIUM_path_rendering 1
// These match the corresponding values in NV_path_rendering
// extension, eg tokens with CHROMIUM replaced with NV.
#define GL_PATH_MODELVIEW_MATRIX_CHROMIUM 0x0BA6
#define GL_PATH_PROJECTION_MATRIX_CHROMIUM 0x0BA7
#define GL_PATH_MODELVIEW_CHROMIUM 0x1700
#define GL_PATH_PROJECTION_CHROMIUM 0x1701
#define GL_FLAT_CHROMIUM 0x1D00
#define GL_CLOSE_PATH_CHROMIUM 0x00
#define GL_MOVE_TO_CHROMIUM 0x02
#define GL_LINE_TO_CHROMIUM 0x04
#define GL_QUADRATIC_CURVE_TO_CHROMIUM 0x0A
#define GL_CUBIC_CURVE_TO_CHROMIUM 0x0C
#define GL_CONIC_CURVE_TO_CHROMIUM 0x1A
#define GL_EYE_LINEAR_CHROMIUM 0x2400
#define GL_OBJECT_LINEAR_CHROMIUM 0x2401
#define GL_CONSTANT_CHROMIUM 0x8576
#define GL_PATH_STROKE_WIDTH_CHROMIUM 0x9075
#define GL_PATH_END_CAPS_CHROMIUM 0x9076
#define GL_PATH_JOIN_STYLE_CHROMIUM 0x9079
#define GL_PATH_MITER_LIMIT_CHROMIUM 0x907a
#define GL_PATH_STROKE_BOUND_CHROMIUM 0x9086
#define GL_COUNT_UP_CHROMIUM 0x9088
#define GL_COUNT_DOWN_CHROMIUM 0x9089
#define GL_CONVEX_HULL_CHROMIUM 0x908B
#define GL_BOUNDING_BOX_CHROMIUM 0x908D
#define GL_TRANSLATE_X_CHROMIUM 0x908E
#define GL_TRANSLATE_Y_CHROMIUM 0x908F
#define GL_TRANSLATE_2D_CHROMIUM 0x9090
#define GL_TRANSLATE_3D_CHROMIUM 0x9091
#define GL_AFFINE_2D_CHROMIUM 0x9092
#define GL_AFFINE_3D_CHROMIUM 0x9094
#define GL_TRANSPOSE_AFFINE_2D_CHROMIUM 0x9096
#define GL_TRANSPOSE_AFFINE_3D_CHROMIUM 0x9098
#define GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM 0x909C
#define GL_SQUARE_CHROMIUM 0x90a3
#define GL_ROUND_CHROMIUM 0x90a4
#define GL_BEVEL_CHROMIUM 0x90a6
#define GL_MITER_REVERT_CHROMIUM 0x90a7
#define GL_PATH_STENCIL_FUNC_CHROMIUM 0x90B7
#define GL_PATH_STENCIL_REF_CHROMIUM 0x90B8
#define GL_PATH_STENCIL_VALUE_MASK_CHROMIUM 0x90B9
#endif

#ifndef GL_EXT_multisample_compatibility
#define GL_EXT_multisample_compatibility 1
#define GL_MULTISAMPLE_EXT 0x809D
#define GL_SAMPLE_ALPHA_TO_ONE_EXT 0x809F
#endif /* GL_EXT_multisample_compatibility */

#ifndef GL_CHROMIUM_framebuffer_mixed_samples
#define GL_CHROMIUM_framebuffer_mixed_samples 1
#define GL_COVERAGE_MODULATION_CHROMIUM 0x9332
#endif /* GL_CHROMIUM_framebuffer_mixed_samples */

#ifndef GL_KHR_blend_equation_advanced
#define GL_KHR_blend_equation_advanced 1
#define GL_COLORBURN_KHR                  0x929A
#define GL_COLORDODGE_KHR                 0x9299
#define GL_DARKEN_KHR                     0x9297
#define GL_DIFFERENCE_KHR                 0x929E
#define GL_EXCLUSION_KHR                  0x92A0
#define GL_HARDLIGHT_KHR                  0x929B
#define GL_HSL_COLOR_KHR                  0x92AF
#define GL_HSL_HUE_KHR                    0x92AD
#define GL_HSL_LUMINOSITY_KHR             0x92B0
#define GL_HSL_SATURATION_KHR             0x92AE
#define GL_LIGHTEN_KHR                    0x9298
#define GL_MULTIPLY_KHR                   0x9294
#define GL_OVERLAY_KHR                    0x9296
#define GL_SCREEN_KHR                     0x9295
#define GL_SOFTLIGHT_KHR                  0x929C
#endif /* GL_KHR_blend_equation_advanced */

#ifndef GL_KHR_blend_equation_advanced_coherent
#define GL_KHR_blend_equation_advanced_coherent 1
#define GL_BLEND_ADVANCED_COHERENT_KHR    0x9285
#endif /* GL_KHR_blend_equation_advanced_coherent */

#ifndef GL_EXT_disjoint_timer_query
#define GL_EXT_disjoint_timer_query 1
#define GL_QUERY_COUNTER_BITS_EXT         0x8864
#define GL_TIME_ELAPSED_EXT               0x88BF
#define GL_TIMESTAMP_EXT                  0x8E28
#define GL_GPU_DISJOINT_EXT               0x8FBB
#endif

#ifndef GL_KHR_robustness
#define GL_KHR_robustness 1
#define GL_CONTEXT_ROBUST_ACCESS_KHR      0x90F3
#define GL_LOSE_CONTEXT_ON_RESET_KHR      0x8252
#define GL_GUILTY_CONTEXT_RESET_KHR       0x8253
#define GL_INNOCENT_CONTEXT_RESET_KHR     0x8254
#define GL_UNKNOWN_CONTEXT_RESET_KHR      0x8255
#define GL_RESET_NOTIFICATION_STRATEGY_KHR 0x8256
#define GL_NO_RESET_NOTIFICATION_KHR      0x8261
#define GL_CONTEXT_LOST_KHR               0x0507
#endif /* GL_KHR_robustness */

#ifndef GL_EXT_texture_norm16
#define GL_EXT_texture_norm16 1
#define GL_R16_EXT 0x822A
#define GL_RG16_EXT 0x822C
#define GL_RGBA16_EXT 0x805B
#define GL_RGB16_EXT 0x8054
#define GL_RGB16_SNORM_EXT 0x8F9A
#endif /* GL_EXT_texture_norm16 */

#ifndef GL_EXT_texture_rg
#define GL_EXT_texture_rg 1
#define GL_RED_EXT 0x1903
#define GL_RG_EXT 0x8227
#define GL_R8_EXT 0x8229
#define GL_RG8_EXT 0x822B
#endif /* GL_EXT_texture_rg */

// This is from NV_path_rendering, but the GL header is not up to date with the
// most recent version of the extension. This definition could be removed once
// glext.h r27498 or later is imported.
#ifndef GL_FRAGMENT_INPUT_NV
#define GL_FRAGMENT_INPUT_NV 0x936D
#endif

#ifndef GL_EXT_blend_func_extended
#define GL_EXT_blend_func_extended 1
#define GL_SRC_ALPHA_SATURATE_EXT 0x0308
#define GL_SRC1_ALPHA_EXT 0x8589  // OpenGL 1.5 token value
#define GL_SRC1_COLOR_EXT 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR_EXT 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA_EXT 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT 0x88FC
#endif /* GL_EXT_blend_func_extended */

#ifndef GL_EXT_window_rectangles
#define GL_EXT_window_rectangles 1
#define GL_INCLUSIVE_EXT 0x8F10
#define GL_EXCLUSIVE_EXT 0x8F11
#define GL_WINDOW_RECTANGLE_EXT 0x8F12
#define GL_WINDOW_RECTANGLE_MODE_EXT 0x8F13
#define GL_MAX_WINDOW_RECTANGLES_EXT 0x8F14
#define GL_NUM_WINDOW_RECTANGLES_EXT 0x8F15
#endif /* GL_EXT_window_rectangles */

#ifndef GL_CHROMIUM_nonblocking_readback
#define GL_CHROMIUM_nonblocking_readback 1
#define GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM 0x84F8
#endif /* GL_CHROMIUM_nonblocking_readback */

#ifndef GL_MESA_framebuffer_flip_y
#define GL_MESA_framebuffer_flip_y 1
#define GL_FRAMEBUFFER_FLIP_Y_MESA 0x8BBB
#endif /* GL_MESA_framebuffer_flip_y */

#ifndef GL_KHR_parallel_shader_compile
#define GL_KHR_parallel_shader_compile 1
#define GL_MAX_SHADER_COMPILER_THREADS_KHR 0x91B0
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif /* GL_KHR_parallel_shader_compile */

#ifndef GL_CHROMIUM_shared_image
#define GL_CHROMIUM_shared_image 1
#define GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM 0x8AF6
#define GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM 0x8AF7
#endif /* GL_CHROMIUM_shared_image */

#ifndef GL_NV_internalformat_sample_query
#define GL_MULTISAMPLES_NV 0x9371
#define GL_SUPERSAMPLE_SCALE_X_NV 0x9372
#define GL_SUPERSAMPLE_SCALE_Y_NV 0x9373
#define GL_CONFORMANT_NV 0x9374
#endif /* GL_NV_internalformat_sample_query */

#define GL_GLEXT_PROTOTYPES 1

#if defined(OS_WIN)
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

// OVR_multiview2 constants.
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR 0x9630
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR 0x9632
#define GL_MAX_VIEWS_OVR 0x9631
#define GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR 0x9633

#define GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT 0x8868

// Forward declare EGL types.
typedef uint64_t EGLuint64CHROMIUM;

#include "gl_bindings_autogen_gl.h"

#if defined(USE_EGL)
#include "gl_bindings_autogen_egl.h"
#endif

#if defined(OS_WIN)
#include "gl_bindings_autogen_wgl.h"
#endif

#if defined(USE_GLX)
#include "gl_bindings_autogen_glx.h"
#endif

namespace gl {

struct GLVersionInfo;

struct GL_EXPORT DriverGL {
  void InitializeStaticBindings();
  void InitializeDynamicBindings(const GLVersionInfo* ver,
                                 const gfx::ExtensionSet& extensions);
  void ClearBindings();

  ProcsGL fn;
  ExtensionsGL ext;

  DriverGL() {
    // InitializeStaticBindings() requires that fn is null-initialized.
    ClearBindings();
  }
};

struct GL_EXPORT CurrentGL {
  GLApi* Api = nullptr;
  DriverGL* Driver = nullptr;
  const GLVersionInfo* Version = nullptr;
};

#if defined(OS_WIN)
struct GL_EXPORT DriverWGL {
  void InitializeStaticBindings();
  void InitializeExtensionBindings();
  void ClearBindings();

  ProcsWGL fn;
  ExtensionsWGL ext;

 private:
  static std::string GetPlatformExtensions();
};
#endif

#if defined(USE_EGL)
struct GL_EXPORT DriverEGL {
  void InitializeStaticBindings();
  void InitializeClientExtensionBindings();
  void InitializeExtensionBindings();
  void ClearBindings();
  void UpdateConditionalExtensionBindings();

  ProcsEGL fn;
  ExtensionsEGL ext;

  static std::string GetPlatformExtensions();
  static std::string GetClientExtensions();
};
#endif

#if defined(USE_GLX)
struct GL_EXPORT DriverGLX {
  void InitializeStaticBindings();
  void InitializeExtensionBindings();
  void ClearBindings();

  ProcsGLX fn;
  ExtensionsGLX ext;

 private:
  static std::string GetPlatformExtensions();
};
#endif

// This #define is here to support autogenerated code.
#define g_current_gl_context g_current_gl_context_tls->Get()->Api
#define g_current_gl_driver g_current_gl_context_tls->Get()->Driver
#define g_current_gl_version g_current_gl_context_tls->Get()->Version
GL_EXPORT extern base::ThreadLocalPointer<CurrentGL>* g_current_gl_context_tls;

#if defined(USE_EGL)
GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern DriverEGL g_driver_egl;
#endif

#if defined(OS_WIN)
GL_EXPORT extern WGLApi* g_current_wgl_context;
GL_EXPORT extern DriverWGL g_driver_wgl;
#endif

#if defined(USE_GLX)
GL_EXPORT extern GLXApi* g_current_glx_context;
GL_EXPORT extern DriverGLX g_driver_glx;
#endif

}  // namespace gl

#endif  // UI_GL_GL_BINDINGS_H_
