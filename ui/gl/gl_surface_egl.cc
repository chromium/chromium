// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/angle_platform_impl.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_display_egl_util.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/sync_control_vsync_provider.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include "base/android/build_info.h"
#endif

#if !defined(EGL_FIXED_SIZE_ANGLE)
#define EGL_FIXED_SIZE_ANGLE 0x3201
#endif

#if !defined(EGL_OPENGL_ES3_BIT)
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

// Not present egl/eglext.h yet.

#ifndef EGL_EXT_gl_colorspace_display_p3
#define EGL_EXT_gl_colorspace_display_p3 1
#define EGL_GL_COLORSPACE_DISPLAY_P3_EXT 0x3363
#endif /* EGL_EXT_gl_colorspace_display_p3 */

#ifndef EGL_EXT_gl_colorspace_display_p3_passthrough
#define EGL_EXT_gl_colorspace_display_p3_passthrough 1
#define EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT 0x3490
#endif /* EGL_EXT_gl_colorspace_display_p3_passthrough */

// From ANGLE's egl/eglext.h.

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x345E
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_d3d
#define EGL_ANGLE_platform_angle_d3d 1
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE 0x320C
#endif /* EGL_ANGLE_platform_angle_d3d */

#ifndef EGL_ANGLE_platform_angle_d3d_luid
#define EGL_ANGLE_platform_angle_d3d_luid 1
#define EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE 0x34A0
#define EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE 0x34A1
#endif /* EGL_ANGLE_platform_angle_d3d_luid */

#ifndef EGL_ANGLE_platform_angle_d3d11on12
#define EGL_ANGLE_platform_angle_d3d11on12 1
#define EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE 0x3488
#endif /* EGL_ANGLE_platform_angle_d3d11on12 */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif /* EGL_ANGLE_platform_angle_opengl */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#endif /* EGL_ANGLE_platform_angle_vulkan */

#ifndef EGL_ANGLE_platform_angle_metal
#define EGL_ANGLE_platform_angle_metal 1
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE 0x3489
#endif /* EGL_ANGLE_platform_angle_metal */

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual 1
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_surface_orientation
#define EGL_ANGLE_surface_orientation
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif /* EGL_ANGLE_surface_orientation */

#ifndef EGL_ANGLE_direct_composition
#define EGL_ANGLE_direct_composition 1
#define EGL_DIRECT_COMPOSITION_ANGLE 0x33A5
#endif /* EGL_ANGLE_direct_composition */

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

#ifndef EGL_ANGLE_display_robust_resource_initialization
#define EGL_ANGLE_display_robust_resource_initialization 1
#define EGL_DISPLAY_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_display_robust_resource_initialization */

// From ANGLE's egl/eglext.h.
#ifndef EGL_ANGLE_feature_control
#define EGL_ANGLE_feature_control 1
#define EGL_FEATURE_NAME_ANGLE 0x3460
#define EGL_FEATURE_CATEGORY_ANGLE 0x3461
#define EGL_FEATURE_DESCRIPTION_ANGLE 0x3462
#define EGL_FEATURE_BUG_ANGLE 0x3463
#define EGL_FEATURE_STATUS_ANGLE 0x3464
#define EGL_FEATURE_COUNT_ANGLE 0x3465
#define EGL_FEATURE_OVERRIDES_ENABLED_ANGLE 0x3466
#define EGL_FEATURE_OVERRIDES_DISABLED_ANGLE 0x3467
#define EGL_FEATURE_ALL_DISABLED_ANGLE 0x3469
#endif /* EGL_ANGLE_feature_control */

using ui::GetLastEGLErrorString;
using ui::PlatformEvent;

namespace gl {

bool GLSurfaceEGL::initialized_ = false;

namespace {

class EGLGpuSwitchingObserver;

EGLDisplay g_egl_display = EGL_NO_DISPLAY;
EGLDisplayPlatform g_native_display(EGL_DEFAULT_DISPLAY);

const char* g_egl_client_extensions = nullptr;
const char* g_egl_extensions = nullptr;
bool g_egl_create_context_robustness_supported = false;
bool g_egl_robustness_video_memory_purge_supported = false;
bool g_egl_create_context_bind_generates_resource_supported = false;
bool g_egl_create_context_webgl_compatability_supported = false;
bool g_egl_sync_control_supported = false;
bool g_egl_sync_control_rate_supported = false;
bool g_egl_window_fixed_size_supported = false;
bool g_egl_surfaceless_context_supported = false;
bool g_egl_surface_orientation_supported = false;
bool g_egl_context_priority_supported = false;
bool g_egl_khr_colorspace = false;
bool g_egl_ext_colorspace_display_p3 = false;
bool g_egl_ext_colorspace_display_p3_passthrough = false;
bool g_egl_flexible_surface_compatibility_supported = false;
bool g_egl_robust_resource_init_supported = false;
bool g_egl_display_texture_share_group_supported = false;
bool g_egl_display_semaphore_share_group_supported = false;
bool g_egl_create_context_client_arrays_supported = false;
bool g_egl_android_native_fence_sync_supported = false;
bool g_egl_ext_pixel_format_float_supported = false;
bool g_egl_angle_feature_control_supported = false;
bool g_egl_angle_power_preference_supported = false;
bool g_egl_angle_external_context_and_surface_supported = false;
EGLGpuSwitchingObserver* g_egl_gpu_switching_observer = nullptr;

constexpr const char kSwapEventTraceCategories[] = "gpu";

constexpr size_t kMaxTimestampsSupportable = 9;

struct TraceSwapEventsInitializer {
  TraceSwapEventsInitializer()
      : value(*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
            kSwapEventTraceCategories)) {}
  const unsigned char& value;
};

static base::LazyInstance<TraceSwapEventsInitializer>::Leaky
    g_trace_swap_enabled = LAZY_INSTANCE_INITIALIZER;

class EGLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  explicit EGLSyncControlVSyncProvider(EGLSurface surface)
      : SyncControlVSyncProvider(),
        surface_(surface) {
  }

  ~EGLSyncControlVSyncProvider() override {}

  static bool IsSupported() {
    return SyncControlVSyncProvider::IsSupported() &&
           g_egl_sync_control_supported;
  }

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    uint64_t u_system_time, u_media_stream_counter, u_swap_buffer_counter;
    bool result =
        eglGetSyncValuesCHROMIUM(g_egl_display, surface_, &u_system_time,
                                 &u_media_stream_counter,
                                 &u_swap_buffer_counter) == EGL_TRUE;
    if (result) {
      *system_time = static_cast<int64_t>(u_system_time);
      *media_stream_counter = static_cast<int64_t>(u_media_stream_counter);
      *swap_buffer_counter = static_cast<int64_t>(u_swap_buffer_counter);
    }
    return result;
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!g_egl_sync_control_rate_supported) {
      return false;
    }

    bool result = eglGetMscRateANGLE(g_egl_display, surface_, numerator,
                                     denominator) == EGL_TRUE;
    return result;
  }

  bool IsHWClock() const override { return true; }

 private:
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(EGLSyncControlVSyncProvider);
};

class EGLGpuSwitchingObserver final : public ui::GpuSwitchingObserver {
 public:
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override {
    DCHECK(GLSurfaceEGL::IsANGLEPowerPreferenceSupported());
    eglHandleGPUSwitchANGLE(g_egl_display);
  }
};

std::vector<const char*> GetAttribArrayFromStringVector(
    const std::vector<std::string>& strings) {
  std::vector<const char*> attribs;
  for (const std::string& item : strings) {
    attribs.push_back(item.c_str());
  }
  attribs.push_back(0);
  return attribs;
}

std::vector<std::string> GetStringVectorFromCommandLine(
    const base::CommandLine* command_line,
    const char switch_name[]) {
  std::string command_string = command_line->GetSwitchValueASCII(switch_name);
  return base::SplitString(command_string, ", ;", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

EGLDisplay GetPlatformANGLEDisplay(
    EGLDisplayPlatform native_display,
    EGLenum platform_type,
    const std::vector<std::string>& enabled_features,
    const std::vector<std::string>& disabled_features,
    const std::vector<EGLAttrib>& extra_display_attribs) {
  std::vector<EGLAttrib> display_attribs(extra_display_attribs);

  display_attribs.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
  display_attribs.push_back(static_cast<EGLAttrib>(platform_type));

  if (platform_type == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUseAdapterLuid)) {
      // If the LUID is specified, the format is <high part>,<low part>. Split
      // and add them to the EGL_ANGLE_platform_angle_d3d_luid ext attributes.
      std::string luid =
          command_line->GetSwitchValueASCII(switches::kUseAdapterLuid);
      size_t comma = luid.find(',');
      if (comma != std::string::npos) {
        int32_t high;
        uint32_t low;
        if (!base::StringToInt(luid.substr(0, comma), &high) ||
            !base::StringToUint(luid.substr(comma + 1), &low))
          return EGL_NO_DISPLAY;

        display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE);
        display_attribs.push_back(high);

        display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE);
        display_attribs.push_back(low);
      }
    }
  }

  GLDisplayEglUtil::GetInstance()->GetPlatformExtraDisplayAttribs(
      platform_type, &display_attribs);

  std::vector<const char*> enabled_features_attribs =
      GetAttribArrayFromStringVector(enabled_features);
  std::vector<const char*> disabled_features_attribs =
      GetAttribArrayFromStringVector(disabled_features);
  if (g_egl_angle_feature_control_supported) {
    if (!enabled_features_attribs.empty()) {
      display_attribs.push_back(EGL_FEATURE_OVERRIDES_ENABLED_ANGLE);
      display_attribs.push_back(
          reinterpret_cast<EGLAttrib>(enabled_features_attribs.data()));
    }
    if (!disabled_features_attribs.empty()) {
      display_attribs.push_back(EGL_FEATURE_OVERRIDES_DISABLED_ANGLE);
      display_attribs.push_back(
          reinterpret_cast<EGLAttrib>(disabled_features_attribs.data()));
    }
  }
  // TODO(dbehr) Add an attrib to Angle to pass EGL platform.

  display_attribs.push_back(EGL_NONE);

  // This is an EGL 1.5 function that we know ANGLE supports. It's used to pass
  // EGLAttribs (pointers) instead of EGLints into the display
  return eglGetPlatformDisplay(
      EGL_PLATFORM_ANGLE_ANGLE,
      reinterpret_cast<void*>(native_display.GetDisplay()),
      &display_attribs[0]);
}

EGLDisplay GetDisplayFromType(
    DisplayType display_type,
    EGLDisplayPlatform native_display,
    const std::vector<std::string>& enabled_angle_features,
    const std::vector<std::string>& disabled_angle_features,
    bool disable_all_angle_features) {
  std::vector<EGLAttrib> extra_display_attribs;
  if (disable_all_angle_features) {
    extra_display_attribs.push_back(EGL_FEATURE_ALL_DISABLED_ANGLE);
    extra_display_attribs.push_back(EGL_TRUE);
  }
  switch (display_type) {
    case DEFAULT:
    case SWIFT_SHADER:
      if (native_display.GetPlatform() != 0) {
        return eglGetPlatformDisplay(
            native_display.GetPlatform(),
            reinterpret_cast<void*>(native_display.GetDisplay()), nullptr);
      } else {
        return eglGetDisplay(native_display.GetDisplay());
      }
    case ANGLE_D3D9:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL_EGL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES_EGL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_NULL:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_VULKAN:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_VULKAN_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11on12:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE);
      extra_display_attribs.push_back(EGL_TRUE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_SWIFTSHADER:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_METAL:
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_METAL_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          native_display, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    default:
      NOTREACHED();
      return EGL_NO_DISPLAY;
  }
}

ANGLEImplementation GetANGLEImplementationFromDisplayType(
    DisplayType display_type) {
  switch (display_type) {
    case ANGLE_D3D9:
      return ANGLEImplementation::kD3D9;
    case ANGLE_D3D11:
    case ANGLE_D3D11_NULL:
    case ANGLE_D3D11on12:
      return ANGLEImplementation::kD3D11;
    case ANGLE_OPENGL:
    case ANGLE_OPENGL_NULL:
      return ANGLEImplementation::kOpenGL;
    case ANGLE_OPENGLES:
    case ANGLE_OPENGLES_NULL:
      return ANGLEImplementation::kOpenGLES;
    case ANGLE_NULL:
      return ANGLEImplementation::kNull;
    case ANGLE_VULKAN:
    case ANGLE_VULKAN_NULL:
      return ANGLEImplementation::kVulkan;
    case ANGLE_SWIFTSHADER:
      return ANGLEImplementation::kSwiftShader;
    case ANGLE_METAL:
    case ANGLE_METAL_NULL:
      return ANGLEImplementation::kMetal;
    default:
      return ANGLEImplementation::kNone;
  }
}

const char* DisplayTypeString(DisplayType display_type) {
  switch (display_type) {
    case DEFAULT:
      return "Default";
    case SWIFT_SHADER:
      return "SwiftShader";
    case ANGLE_D3D9:
      return "D3D9";
    case ANGLE_D3D11:
      return "D3D11";
    case ANGLE_D3D11_NULL:
      return "D3D11Null";
    case ANGLE_OPENGL:
      return "OpenGL";
    case ANGLE_OPENGL_NULL:
      return "OpenGLNull";
    case ANGLE_OPENGLES:
      return "OpenGLES";
    case ANGLE_OPENGLES_NULL:
      return "OpenGLESNull";
    case ANGLE_NULL:
      return "Null";
    case ANGLE_VULKAN:
      return "Vulkan";
    case ANGLE_VULKAN_NULL:
      return "VulkanNull";
    case ANGLE_D3D11on12:
      return "D3D11on12";
    case ANGLE_SWIFTSHADER:
      return "SwiftShader";
    case ANGLE_OPENGL_EGL:
      return "OpenGLEGL";
    case ANGLE_OPENGLES_EGL:
      return "OpenGLESEGL";
    case ANGLE_METAL:
      return "Metal";
    case ANGLE_METAL_NULL:
      return "MetalNull";
    default:
      NOTREACHED();
      return "Err";
  }
}

bool ValidateEglConfig(EGLDisplay display,
                       const EGLint* config_attribs,
                       EGLint* num_configs) {
  if (!eglChooseConfig(display,
                       config_attribs,
                       NULL,
                       0,
                       num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }
  if (*num_configs == 0) {
    return false;
  }
  return true;
}

EGLConfig ChooseConfig(GLSurfaceFormat format, bool surfaceless) {
  // Choose an EGL configuration.
  // On X this is only used for PBuffer surfaces.

  std::vector<EGLint> renderable_types;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableES3GLContext)) {
    renderable_types.push_back(EGL_OPENGL_ES3_BIT);
  }
  renderable_types.push_back(EGL_OPENGL_ES2_BIT);

  EGLint buffer_size = format.GetBufferSize();
  EGLint alpha_size = 8;
  bool want_rgb565 = buffer_size == 16;
  EGLint depth_size = format.GetDepthBits();
  EGLint stencil_size = format.GetStencilBits();
  EGLint samples = format.GetSamples();

  // Some platforms (eg. X11) may want to set custom values for alpha and buffer
  // sizes.
  GLDisplayEglUtil::GetInstance()->ChoosePlatformCustomAlphaAndBufferSize(
      &alpha_size, &buffer_size);

  EGLint surface_type =
      (surfaceless ? EGL_DONT_CARE : EGL_WINDOW_BIT | EGL_PBUFFER_BIT);

  for (auto renderable_type : renderable_types) {
    EGLint config_attribs_8888[] = {EGL_BUFFER_SIZE,
                                    buffer_size,
                                    EGL_ALPHA_SIZE,
                                    alpha_size,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_SAMPLES,
                                    samples,
                                    EGL_DEPTH_SIZE,
                                    depth_size,
                                    EGL_STENCIL_SIZE,
                                    stencil_size,
                                    EGL_RENDERABLE_TYPE,
                                    renderable_type,
                                    EGL_SURFACE_TYPE,
                                    surface_type,
                                    EGL_NONE};

    EGLint config_attribs_565[] = {EGL_BUFFER_SIZE,
                                   16,
                                   EGL_BLUE_SIZE,
                                   5,
                                   EGL_GREEN_SIZE,
                                   6,
                                   EGL_RED_SIZE,
                                   5,
                                   EGL_SAMPLES,
                                   samples,
                                   EGL_DEPTH_SIZE,
                                   depth_size,
                                   EGL_STENCIL_SIZE,
                                   stencil_size,
                                   EGL_RENDERABLE_TYPE,
                                   renderable_type,
                                   EGL_SURFACE_TYPE,
                                   surface_type,
                                   EGL_NONE};

    EGLint* choose_attributes = config_attribs_8888;
    if (want_rgb565) {
      choose_attributes = config_attribs_565;
    }

    EGLint num_configs;
    EGLint config_size = 1;
    EGLConfig config = nullptr;
    EGLConfig* config_data = &config;
    // Validate if there are any configs for given attribs.
    if (!ValidateEglConfig(g_egl_display, choose_attributes, &num_configs)) {
      // Try the next renderable_type
      continue;
    }

    std::unique_ptr<EGLConfig[]> matching_configs(new EGLConfig[num_configs]);
    if (want_rgb565) {
      config_size = num_configs;
      config_data = matching_configs.get();
    }

    if (!eglChooseConfig(g_egl_display, choose_attributes, config_data,
                         config_size, &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return config;
    }

    if (want_rgb565) {
      // Because of the EGL config sort order, we have to iterate
      // through all of them (it'll put higher sum(R,G,B) bits
      // first with the above attribs).
      bool match_found = false;
      for (int i = 0; i < num_configs; i++) {
        EGLint red, green, blue, alpha;
        // Read the relevant attributes of the EGLConfig.
        if (eglGetConfigAttrib(g_egl_display, matching_configs[i], EGL_RED_SIZE,
                               &red) &&
            eglGetConfigAttrib(g_egl_display, matching_configs[i],
                               EGL_BLUE_SIZE, &blue) &&
            eglGetConfigAttrib(g_egl_display, matching_configs[i],
                               EGL_GREEN_SIZE, &green) &&
            eglGetConfigAttrib(g_egl_display, matching_configs[i],
                               EGL_ALPHA_SIZE, &alpha) &&
            alpha == 0 && red == 5 && green == 6 && blue == 5) {
          config = matching_configs[i];
          match_found = true;
          break;
        }
      }
      if (!match_found) {
        // To fall back to default 32 bit format, choose with
        // the right attributes again.
        if (!ValidateEglConfig(g_egl_display, config_attribs_8888,
                               &num_configs)) {
          // Try the next renderable_type
          continue;
        }
        if (!eglChooseConfig(g_egl_display, config_attribs_8888, &config, 1,
                             &num_configs)) {
          LOG(ERROR) << "eglChooseConfig failed with error "
                     << GetLastEGLErrorString();
          return config;
        }
      }
    }
    return config;
  }

  LOG(ERROR) << "No suitable EGL configs found.";
  return nullptr;
}

void AddInitDisplay(std::vector<DisplayType>* init_displays,
                    DisplayType display_type) {
  // Make sure to not add the same display type twice.
  if (!base::Contains(*init_displays, display_type))
    init_displays->push_back(display_type);
}

const char* GetDebugMessageTypeString(EGLint source) {
  switch (source) {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
      return "Critical";
    case EGL_DEBUG_MSG_ERROR_KHR:
      return "Error";
    case EGL_DEBUG_MSG_WARN_KHR:
      return "Warning";
    case EGL_DEBUG_MSG_INFO_KHR:
      return "Info";
    default:
      return "UNKNOWN";
  }
}

static void EGLAPIENTRY LogEGLDebugMessage(EGLenum error,
                                           const char* command,
                                           EGLint message_type,
                                           EGLLabelKHR thread_label,
                                           EGLLabelKHR object_label,
                                           const char* message) {
  std::string formatted_message = std::string("EGL Driver message (") +
                                  GetDebugMessageTypeString(message_type) +
                                  ") " + command + ": " + message;

  // Assume that all labels that have been set are strings
  if (thread_label) {
    formatted_message += " thread: ";
    formatted_message += static_cast<const char*>(thread_label);
  }
  if (object_label) {
    formatted_message += " object: ";
    formatted_message += static_cast<const char*>(object_label);
  }

  if (message_type == EGL_DEBUG_MSG_CRITICAL_KHR ||
      message_type == EGL_DEBUG_MSG_ERROR_KHR) {
    LOG(ERROR) << formatted_message;
  } else {
    DVLOG(1) << formatted_message;
  }
}

}  // namespace

void GetEGLInitDisplays(bool supports_angle_d3d,
                        bool supports_angle_opengl,
                        bool supports_angle_null,
                        bool supports_angle_vulkan,
                        bool supports_angle_swiftshader,
                        bool supports_angle_egl,
                        bool supports_angle_metal,
                        const base::CommandLine* command_line,
                        std::vector<DisplayType>* init_displays) {
  // SwiftShader does not use the platform extensions
  if (command_line->GetSwitchValueASCII(switches::kUseGL) ==
      kGLImplementationSwiftShaderForWebGLName) {
    AddInitDisplay(init_displays, SWIFT_SHADER);
    return;
  }

  std::string requested_renderer =
      command_line->GetSwitchValueASCII(switches::kUseANGLE);

  bool use_angle_default =
      !command_line->HasSwitch(switches::kUseANGLE) ||
      requested_renderer == kANGLEImplementationDefaultName;

  if (supports_angle_null &&
      requested_renderer == kANGLEImplementationNullName) {
    AddInitDisplay(init_displays, ANGLE_NULL);
    return;
  }

  // If no display has been explicitly requested and the DefaultANGLEOpenGL
  // experiment is enabled, try creating OpenGL displays first.
  // TODO(oetuaho@nvidia.com): Only enable this path on specific GPUs with a
  // blocklist entry. http://crbug.com/693090
  if (supports_angle_opengl && use_angle_default &&
      base::FeatureList::IsEnabled(features::kDefaultANGLEOpenGL)) {
    AddInitDisplay(init_displays, ANGLE_OPENGL);
    AddInitDisplay(init_displays, ANGLE_OPENGLES);
  }

  if (supports_angle_metal && use_angle_default &&
      base::FeatureList::IsEnabled(features::kDefaultANGLEMetal)) {
    AddInitDisplay(init_displays, ANGLE_METAL);
  }

  if (supports_angle_vulkan && use_angle_default &&
      features::IsDefaultANGLEVulkan()) {
    AddInitDisplay(init_displays, ANGLE_VULKAN);
  }

  if (supports_angle_d3d) {
    if (use_angle_default) {
      // Default mode for ANGLE - try D3D11, else try D3D9
      if (!command_line->HasSwitch(switches::kDisableD3D11)) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      }
      AddInitDisplay(init_displays, ANGLE_D3D9);
    } else {
      if (requested_renderer == kANGLEImplementationD3D11Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      } else if (requested_renderer == kANGLEImplementationD3D9Name) {
        AddInitDisplay(init_displays, ANGLE_D3D9);
      } else if (requested_renderer == kANGLEImplementationD3D11NULLName) {
        AddInitDisplay(init_displays, ANGLE_D3D11_NULL);
      } else if (requested_renderer == kANGLEImplementationD3D11on12Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11on12);
      }
    }
  }

  if (supports_angle_opengl) {
    if (use_angle_default && !supports_angle_d3d) {
#if defined(OS_ANDROID)
      // Don't request desktopGL on android
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#else
      AddInitDisplay(init_displays, ANGLE_OPENGL);
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#endif
    } else {
      if (requested_renderer == kANGLEImplementationOpenGLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES);
      } else if (requested_renderer == kANGLEImplementationOpenGLNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLEGLName &&
                 supports_angle_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_EGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESEGLName &&
                 supports_angle_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_EGL);
      }
    }
  }

  if (supports_angle_vulkan) {
    if (use_angle_default) {
      if (!supports_angle_d3d && !supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_VULKAN);
      }
    } else if (requested_renderer == kANGLEImplementationVulkanName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN);
    } else if (requested_renderer == kANGLEImplementationVulkanNULLName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN_NULL);
    }
  }

  if (supports_angle_swiftshader) {
    if (requested_renderer == kANGLEImplementationSwiftShaderName) {
      AddInitDisplay(init_displays, ANGLE_SWIFTSHADER);
    }
  }

  if (supports_angle_metal) {
    if (use_angle_default) {
      if (!supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_METAL);
      }
    } else if (requested_renderer == kANGLEImplementationMetalName) {
      AddInitDisplay(init_displays, ANGLE_METAL);
    } else if (requested_renderer == kANGLEImplementationMetalNULLName) {
      AddInitDisplay(init_displays, ANGLE_METAL_NULL);
    }
  }

  // If no displays are available due to missing angle extensions or invalid
  // flags, request the default display.
  if (init_displays->empty()) {
    init_displays->push_back(DEFAULT);
  }
}

GLSurfaceEGL::GLSurfaceEGL() {}

GLSurfaceFormat GLSurfaceEGL::GetFormat() {
  return format_;
}

EGLDisplay GLSurfaceEGL::GetDisplay() {
  return g_egl_display;
}

EGLConfig GLSurfaceEGL::GetConfig() {
  if (!config_) {
    config_ = ChooseConfig(format_, IsSurfaceless());
  }
  return config_;
}

// static
bool GLSurfaceEGL::InitializeOneOff(EGLDisplayPlatform native_display) {
  if (initialized_)
    return true;

  // Must be called before InitializeDisplay().
  g_driver_egl.InitializeClientExtensionBindings();

  InitializeDisplay(native_display);
  if (g_egl_display == EGL_NO_DISPLAY)
    return false;

  // Must be called after InitializeDisplay().
  g_driver_egl.InitializeExtensionBindings();

  return InitializeOneOffCommon();
}

// static
bool GLSurfaceEGL::InitializeOneOffForTesting() {
  g_driver_egl.InitializeClientExtensionBindings();
  g_egl_display = eglGetCurrentDisplay();
  g_driver_egl.InitializeExtensionBindings();
  return InitializeOneOffCommon();
}

// static
bool GLSurfaceEGL::InitializeOneOffCommon() {
  g_egl_client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  g_egl_extensions = eglQueryString(g_egl_display, EGL_EXTENSIONS);

  g_egl_create_context_robustness_supported =
      HasEGLExtension("EGL_EXT_create_context_robustness");
  g_egl_robustness_video_memory_purge_supported =
      HasEGLExtension("EGL_NV_robustness_video_memory_purge");
  g_egl_create_context_bind_generates_resource_supported =
      HasEGLExtension("EGL_CHROMIUM_create_context_bind_generates_resource");
  g_egl_create_context_webgl_compatability_supported =
      HasEGLExtension("EGL_ANGLE_create_context_webgl_compatibility");
  g_egl_sync_control_supported = HasEGLExtension("EGL_CHROMIUM_sync_control");
  g_egl_sync_control_rate_supported =
      HasEGLExtension("EGL_ANGLE_sync_control_rate");
  g_egl_window_fixed_size_supported =
      HasEGLExtension("EGL_ANGLE_window_fixed_size");
  g_egl_surface_orientation_supported =
      HasEGLExtension("EGL_ANGLE_surface_orientation");
  g_egl_khr_colorspace = HasEGLExtension("EGL_KHR_gl_colorspace");
  g_egl_ext_colorspace_display_p3 =
      HasEGLExtension("EGL_EXT_gl_colorspace_display_p3");
  g_egl_ext_colorspace_display_p3_passthrough =
      HasEGLExtension("EGL_EXT_gl_colorspace_display_p3_passthrough");
  // According to https://source.android.com/compatibility/android-cdd.html the
  // EGL_IMG_context_priority extension is mandatory for Virtual Reality High
  // Performance support, but due to a bug in Android Nougat the extension
  // isn't being reported even when it's present. As a fallback, check if other
  // related extensions that were added for VR support are present, and assume
  // that this implies context priority is also supported. See also:
  // https://github.com/googlevr/gvr-android-sdk/issues/330
  g_egl_context_priority_supported =
      HasEGLExtension("EGL_IMG_context_priority") ||
      (HasEGLExtension("EGL_ANDROID_front_buffer_auto_refresh") &&
       HasEGLExtension("EGL_ANDROID_create_native_client_buffer"));

#if defined(OS_WIN)
  // Need EGL_ANGLE_flexible_surface_compatibility to allow surfaces with and
  // without alpha to be bound to the same context.
  g_egl_flexible_surface_compatibility_supported =
      HasEGLExtension("EGL_ANGLE_flexible_surface_compatibility");
#endif

  g_egl_display_texture_share_group_supported =
      HasEGLExtension("EGL_ANGLE_display_texture_share_group");
  g_egl_display_semaphore_share_group_supported =
      HasEGLExtension("EGL_ANGLE_display_semaphore_share_group");
  g_egl_create_context_client_arrays_supported =
      HasEGLExtension("EGL_ANGLE_create_context_client_arrays");
  g_egl_robust_resource_init_supported =
      HasEGLExtension("EGL_ANGLE_robust_resource_initialization");

  // TODO(oetuaho@nvidia.com): Surfaceless is disabled on Android as a temporary
  // workaround, since code written for Android WebView takes different paths
  // based on whether GL surface objects have underlying EGL surface handles,
  // conflicting with the use of surfaceless. See https://crbug.com/382349
#if defined(OS_ANDROID)
  DCHECK(!g_egl_surfaceless_context_supported);
#else
  // Check if SurfacelessEGL is supported.
  g_egl_surfaceless_context_supported =
      HasEGLExtension("EGL_KHR_surfaceless_context");
  if (g_egl_surfaceless_context_supported) {
    // EGL_KHR_surfaceless_context is supported but ensure
    // GL_OES_surfaceless_context is also supported. We need a current context
    // to query for supported GL extensions.
    scoped_refptr<GLSurface> surface = new SurfacelessEGL(gfx::Size(1, 1));
    scoped_refptr<GLContext> context = InitializeGLContext(
        new GLContextEGL(nullptr), surface.get(), GLContextAttribs());
    if (!context || !context->MakeCurrent(surface.get()))
      g_egl_surfaceless_context_supported = false;

    // Ensure context supports GL_OES_surfaceless_context.
    if (g_egl_surfaceless_context_supported) {
      g_egl_surfaceless_context_supported =
          context->HasExtension("GL_OES_surfaceless_context");
      context->ReleaseCurrent(surface.get());
    }
  }
#endif

  // The native fence sync extension is a bit complicated. It's reported as
  // present for ChromeOS, but Android currently doesn't report this extension
  // even when it's present, and older devices and Android emulator may export
  // a useless wrapper function. See crbug.com/775707 for details. In short, if
  // the symbol is present and we're on Android N or newer and we are not on
  // Android emulator, assume that it's usable even if the extension wasn't
  // reported. TODO(https://crbug.com/1086781): Once this is fixed at the
  // Android level, update the heuristic to trust the reported extension from
  // that version onward.
  g_egl_android_native_fence_sync_supported =
      HasEGLExtension("EGL_ANDROID_native_fence_sync");
#if defined(OS_ANDROID)
  if (!g_egl_android_native_fence_sync_supported &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_NOUGAT &&
      g_driver_egl.fn.eglDupNativeFenceFDANDROIDFn &&
      base::SysInfo::GetAndroidHardwareEGL() != "swiftshader" &&
      base::SysInfo::GetAndroidHardwareEGL() != "emulation") {
    g_egl_android_native_fence_sync_supported = true;
  }
#endif

  g_egl_ext_pixel_format_float_supported =
      HasEGLExtension("EGL_EXT_pixel_format_float");

  g_egl_angle_power_preference_supported =
      HasEGLExtension("EGL_ANGLE_power_preference");

  g_egl_angle_external_context_and_surface_supported =
      HasEGLExtension("EGL_ANGLE_external_context_and_surface");

  if (g_egl_angle_power_preference_supported) {
    g_egl_gpu_switching_observer = new EGLGpuSwitchingObserver();
    ui::GpuSwitchingManager::GetInstance()->AddObserver(
        g_egl_gpu_switching_observer);
  }

  initialized_ = true;
  return true;
}

// static
bool GLSurfaceEGL::InitializeExtensionSettingsOneOff() {
  if (!initialized_)
    return false;
  g_driver_egl.UpdateConditionalExtensionBindings();
  g_egl_client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  g_egl_extensions = eglQueryString(g_egl_display, EGL_EXTENSIONS);

  return true;
}

// static
void GLSurfaceEGL::ShutdownOneOff() {
  if (g_egl_gpu_switching_observer) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(
        g_egl_gpu_switching_observer);
    delete g_egl_gpu_switching_observer;
    g_egl_gpu_switching_observer = nullptr;
  }

  angle::ResetPlatform(g_egl_display);

  if (g_egl_display != EGL_NO_DISPLAY) {
    DCHECK(g_driver_egl.fn.eglTerminateFn);
    eglTerminate(g_egl_display);
  }
  g_egl_display = EGL_NO_DISPLAY;

  g_egl_client_extensions = nullptr;
  g_egl_extensions = nullptr;
  g_egl_create_context_robustness_supported = false;
  g_egl_robustness_video_memory_purge_supported = false;
  g_egl_create_context_bind_generates_resource_supported = false;
  g_egl_create_context_webgl_compatability_supported = false;
  g_egl_sync_control_supported = false;
  g_egl_sync_control_rate_supported = false;
  g_egl_window_fixed_size_supported = false;
  g_egl_surface_orientation_supported = false;
  g_egl_surfaceless_context_supported = false;
  g_egl_robust_resource_init_supported = false;
  g_egl_display_texture_share_group_supported = false;
  g_egl_create_context_client_arrays_supported = false;
  g_egl_angle_feature_control_supported = false;

  initialized_ = false;
}

// static
EGLDisplay GLSurfaceEGL::GetHardwareDisplay() {
  return g_egl_display;
}

// static
EGLNativeDisplayType GLSurfaceEGL::GetNativeDisplay() {
  return g_native_display.GetDisplay();
}

// static
const char* GLSurfaceEGL::GetEGLClientExtensions() {
  return g_egl_client_extensions ? g_egl_client_extensions : "";
}

// static
const char* GLSurfaceEGL::GetEGLExtensions() {
  return g_egl_extensions;
}

// static
bool GLSurfaceEGL::HasEGLClientExtension(const char* name) {
  return ExtensionsContain(GetEGLClientExtensions(), name);
}

// static
bool GLSurfaceEGL::HasEGLExtension(const char* name) {
  return ExtensionsContain(GetEGLExtensions(), name);
}

// static
bool GLSurfaceEGL::IsCreateContextRobustnessSupported() {
  return g_egl_create_context_robustness_supported;
}

// static
bool GLSurfaceEGL::IsRobustnessVideoMemoryPurgeSupported() {
  return g_egl_robustness_video_memory_purge_supported;
}

bool GLSurfaceEGL::IsCreateContextBindGeneratesResourceSupported() {
  return g_egl_create_context_bind_generates_resource_supported;
}

bool GLSurfaceEGL::IsCreateContextWebGLCompatabilitySupported() {
  return g_egl_create_context_webgl_compatability_supported;
}

// static
bool GLSurfaceEGL::IsEGLSurfacelessContextSupported() {
  return g_egl_surfaceless_context_supported;
}

// static
bool GLSurfaceEGL::IsEGLContextPrioritySupported() {
  return g_egl_context_priority_supported;
}

// static
bool GLSurfaceEGL::IsEGLFlexibleSurfaceCompatibilitySupported() {
  return g_egl_flexible_surface_compatibility_supported;
}

bool GLSurfaceEGL::IsRobustResourceInitSupported() {
  return g_egl_robust_resource_init_supported;
}

bool GLSurfaceEGL::IsDisplayTextureShareGroupSupported() {
  return g_egl_display_texture_share_group_supported;
}

bool GLSurfaceEGL::IsDisplaySemaphoreShareGroupSupported() {
  return g_egl_display_semaphore_share_group_supported;
}

bool GLSurfaceEGL::IsCreateContextClientArraysSupported() {
  return g_egl_create_context_client_arrays_supported;
}

bool GLSurfaceEGL::IsAndroidNativeFenceSyncSupported() {
  return g_egl_android_native_fence_sync_supported;
}

bool GLSurfaceEGL::IsPixelFormatFloatSupported() {
  return g_egl_ext_pixel_format_float_supported;
}

bool GLSurfaceEGL::IsANGLEFeatureControlSupported() {
  return g_egl_angle_feature_control_supported;
}

bool GLSurfaceEGL::IsANGLEPowerPreferenceSupported() {
  return g_egl_angle_power_preference_supported;
}

bool GLSurfaceEGL::IsANGLEExternalContextAndSurfaceSupported() {
  return g_egl_angle_external_context_and_surface_supported;
}

GLSurfaceEGL::~GLSurfaceEGL() = default;

// InitializeDisplay is necessary because the static binding code
// needs a full Display init before it can query the Display extensions.
// static
EGLDisplay GLSurfaceEGL::InitializeDisplay(EGLDisplayPlatform native_display) {
  if (g_egl_display != EGL_NO_DISPLAY) {
    return g_egl_display;
  }

  g_native_display = native_display;

  // If EGL_EXT_client_extensions not supported this call to eglQueryString
  // will return NULL.
  g_egl_client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

  bool supports_egl_debug = HasEGLClientExtension("EGL_KHR_debug");
  if (supports_egl_debug) {
    EGLAttrib controls[] = {
        EGL_DEBUG_MSG_CRITICAL_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_ERROR_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_WARN_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_INFO_KHR,
        EGL_TRUE,
        EGL_NONE,
        EGL_NONE,
    };

    eglDebugMessageControlKHR(&LogEGLDebugMessage, controls);
  }

  bool supports_angle_d3d = false;
  bool supports_angle_opengl = false;
  bool supports_angle_null = false;
  bool supports_angle_vulkan = false;
  bool supports_angle_swiftshader = false;
  bool supports_angle_egl = false;
  bool supports_angle_metal = false;
  // Check for availability of ANGLE extensions.
  if (HasEGLClientExtension("EGL_ANGLE_platform_angle")) {
    supports_angle_d3d = HasEGLClientExtension("EGL_ANGLE_platform_angle_d3d");
    supports_angle_opengl =
        HasEGLClientExtension("EGL_ANGLE_platform_angle_opengl");
    supports_angle_null =
        HasEGLClientExtension("EGL_ANGLE_platform_angle_null");
    supports_angle_vulkan =
        HasEGLClientExtension("EGL_ANGLE_platform_angle_vulkan");
    supports_angle_swiftshader = HasEGLClientExtension(
        "EGL_ANGLE_platform_angle_device_type_swiftshader");
    supports_angle_egl =
        HasEGLClientExtension("EGL_ANGLE_platform_angle_device_type_egl_angle");
    supports_angle_metal =
        HasEGLClientExtension("EGL_ANGLE_platform_angle_metal");
  }

  bool supports_angle = supports_angle_d3d || supports_angle_opengl ||
                        supports_angle_null || supports_angle_vulkan ||
                        supports_angle_swiftshader || supports_angle_metal;

  g_egl_angle_feature_control_supported =
      HasEGLClientExtension("EGL_ANGLE_feature_control");

  std::vector<DisplayType> init_displays;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GetEGLInitDisplays(supports_angle_d3d, supports_angle_opengl,
                     supports_angle_null, supports_angle_vulkan,
                     supports_angle_swiftshader, supports_angle_egl,
                     supports_angle_metal, command_line, &init_displays);

  std::vector<std::string> enabled_angle_features =
      GetStringVectorFromCommandLine(command_line,
                                     switches::kEnableANGLEFeatures);
  std::vector<std::string> disabled_angle_features =
      GetStringVectorFromCommandLine(command_line,
                                     switches::kDisableANGLEFeatures);

  bool disable_all_angle_features =
      command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds);

  for (size_t disp_index = 0; disp_index < init_displays.size(); ++disp_index) {
    DisplayType display_type = init_displays[disp_index];
    EGLDisplay display = GetDisplayFromType(
        display_type, g_native_display, enabled_angle_features,
        disabled_angle_features, disable_all_angle_features);
    if (display == EGL_NO_DISPLAY) {
      LOG(ERROR) << "EGL display query failed with error "
                 << GetLastEGLErrorString();
    }

    // Init ANGLE platform now that we have the global display.
    if (supports_angle) {
      if (!angle::InitializePlatform(display)) {
        LOG(ERROR) << "ANGLE Platform initialization failed.";
      }

      SetANGLEImplementation(
          GetANGLEImplementationFromDisplayType(display_type));
    }

#if defined(USE_X11)
    // Unset DISPLAY env, so the vulkan can be initialized successfully, if the
    // X server doesn't support Vulkan surface.
    base::Optional<ui::ScopedUnsetDisplay> unset_display;
    if (display_type == ANGLE_VULKAN && !ui::IsVulkanSurfaceSupported())
      unset_display.emplace();
#endif  // defined(USE_X11)

    if (!eglInitialize(display, nullptr, nullptr)) {
      bool is_last = disp_index == init_displays.size() - 1;

      LOG(ERROR) << "eglInitialize " << DisplayTypeString(display_type)
                 << " failed with error " << GetLastEGLErrorString()
                 << (is_last ? "" : ", trying next display type");
      continue;
    }

    std::ostringstream display_type_string;
    auto gl_implementation = GetGLImplementation();
    display_type_string << GetGLImplementationName(gl_implementation);
    if (gl_implementation == kGLImplementationEGLANGLE) {
      display_type_string << ":" << DisplayTypeString(display_type);
    }

    static auto* egl_display_type_key = base::debug::AllocateCrashKeyString(
        "egl-display-type", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(egl_display_type_key,
                                   display_type_string.str());

    UMA_HISTOGRAM_ENUMERATION("GPU.EGLDisplayType", display_type,
                              DISPLAY_TYPE_MAX);
    g_egl_display = display;
    break;
  }

  return g_egl_display;
}

NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(
    EGLNativeWindowType window,
    std::unique_ptr<gfx::VSyncProvider> vsync_provider)
    : window_(window), vsync_provider_external_(std::move(vsync_provider)) {
#if defined(OS_ANDROID)
  if (window)
    ANativeWindow_acquire(window);
#endif

#if defined(OS_WIN)
  RECT windowRect;
  if (GetClientRect(window_, &windowRect))
    size_ = gfx::Rect(windowRect).size();
#endif
}

bool NativeViewGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  DCHECK(!surface_);
  format_ = format;

  if (!GetDisplay()) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  // We need to make sure that window_ is correctly initialized with all
  // the platform-dependant quirks, if any, before creating the surface.
  if (!InitializeNativeWindow()) {
    LOG(ERROR) << "Error trying to initialize the native window.";
    return false;
  }

  std::vector<EGLint> egl_window_attributes;

  if (g_egl_window_fixed_size_supported && enable_fixed_size_angle_) {
    egl_window_attributes.push_back(EGL_FIXED_SIZE_ANGLE);
    egl_window_attributes.push_back(EGL_TRUE);
    egl_window_attributes.push_back(EGL_WIDTH);
    egl_window_attributes.push_back(size_.width());
    egl_window_attributes.push_back(EGL_HEIGHT);
    egl_window_attributes.push_back(size_.height());
  }

  if (g_driver_egl.ext.b_EGL_NV_post_sub_buffer) {
    egl_window_attributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
    egl_window_attributes.push_back(EGL_TRUE);
  }

  if (g_egl_surface_orientation_supported) {
    EGLint attrib;
    eglGetConfigAttrib(GetDisplay(), GetConfig(),
                       EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &attrib);
    surface_origin_ = (attrib == EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE)
                          ? gfx::SurfaceOrigin::kTopLeft
                          : gfx::SurfaceOrigin::kBottomLeft;
  }

  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_ANGLE);
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
  }

  switch (format_.GetColorSpace()) {
    case GLSurfaceFormat::COLOR_SPACE_UNSPECIFIED:
      break;
    case GLSurfaceFormat::COLOR_SPACE_SRGB:
      // Note that COLORSPACE_LINEAR refers to the sRGB color space, but
      // without opting into sRGB blending. It is equivalent to
      // COLORSPACE_SRGB with Disable(FRAMEBUFFER_SRGB).
      if (g_egl_khr_colorspace) {
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_KHR);
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_LINEAR_KHR);
      }
      break;
    case GLSurfaceFormat::COLOR_SPACE_DISPLAY_P3:
      // Note that it is not the case that
      //   COLORSPACE_SRGB is to COLORSPACE_LINEAR_KHR
      // as
      //   COLORSPACE_DISPLAY_P3 is to COLORSPACE_DISPLAY_P3_LINEAR
      // COLORSPACE_DISPLAY_P3 is equivalent to COLORSPACE_LINEAR, except with
      // with the P3 gamut instead of the the sRGB gamut.
      // COLORSPACE_DISPLAY_P3_LINEAR has a linear transfer function, and is
      // intended for use with 16-bit formats.
      bool p3_supported = g_egl_ext_colorspace_display_p3 ||
                          g_egl_ext_colorspace_display_p3_passthrough;
      if (g_egl_khr_colorspace && p3_supported) {
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_KHR);
        // Chrome relied on incorrect Android behavior when dealing with P3 /
        // framebuffer_srgb interactions. This behavior was fixed in Q, which
        // causes invalid Chrome rendering. To achieve Android-P behavior in Q+,
        // use EGL_GL_COLORSPACE_P3_PASSTHROUGH_EXT where possible.
        if (g_egl_ext_colorspace_display_p3_passthrough) {
          egl_window_attributes.push_back(
              EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT);
        } else {
          egl_window_attributes.push_back(EGL_GL_COLORSPACE_DISPLAY_P3_EXT);
        }
      }
      break;
  }

  egl_window_attributes.push_back(EGL_NONE);
  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(
      GetDisplay(), GetConfig(), window_, &egl_window_attributes[0]);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  if (g_driver_egl.ext.b_EGL_NV_post_sub_buffer) {
    EGLint surfaceVal;
    EGLBoolean retVal = eglQuerySurface(
        GetDisplay(), surface_, EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceVal);
    supports_post_sub_buffer_ = (surfaceVal && retVal) == EGL_TRUE;
  }

  supports_swap_buffer_with_damage_ =
      g_driver_egl.ext.b_EGL_KHR_swap_buffers_with_damage;

  if (!vsync_provider_external_ && EGLSyncControlVSyncProvider::IsSupported()) {
    vsync_provider_internal_ =
        std::make_unique<EGLSyncControlVSyncProvider>(surface_);
  }

  if (!vsync_provider_external_ && !vsync_provider_internal_)
    vsync_provider_internal_ = CreateVsyncProviderInternal();

  presentation_helper_ =
      std::make_unique<GLSurfacePresentationHelper>(GetVSyncProvider());
  return true;
}

bool NativeViewGLSurfaceEGL::SupportsSwapTimestamps() const {
  return g_driver_egl.ext.b_EGL_ANDROID_get_frame_timestamps;
}

void NativeViewGLSurfaceEGL::SetEnableSwapTimestamps() {
  DCHECK(g_driver_egl.ext.b_EGL_ANDROID_get_frame_timestamps);

  // If frame timestamps are supported, set the proper attribute to enable the
  // feature and then cache the timestamps supported by the underlying
  // implementation. EGL_DISPLAY_PRESENT_TIME_ANDROID support, in particular,
  // is spotty.
  // Clear the supported timestamps here to protect against Initialize() being
  // called twice.
  supported_egl_timestamps_.clear();
  supported_event_names_.clear();
  presentation_feedback_index_ = -1;
  composition_start_index_ = -1;

  eglSurfaceAttrib(GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID, EGL_TRUE);

  // Check if egl composite interval is supported or not. If not then return.
  // Else check which other timestamps are supported.
  EGLint interval_name = EGL_COMPOSITE_INTERVAL_ANDROID;
  if (!eglGetCompositorTimingSupportedANDROID(GetDisplay(), surface_,
                                              interval_name))
    return;

  static const struct {
    EGLint egl_name;
    const char* name;
  } all_timestamps[kMaxTimestampsSupportable] = {
      {EGL_REQUESTED_PRESENT_TIME_ANDROID, "Queue"},
      {EGL_RENDERING_COMPLETE_TIME_ANDROID, "WritesDone"},
      {EGL_COMPOSITION_LATCH_TIME_ANDROID, "LatchedForDisplay"},
      {EGL_FIRST_COMPOSITION_START_TIME_ANDROID, "1stCompositeCpu"},
      {EGL_LAST_COMPOSITION_START_TIME_ANDROID, "NthCompositeCpu"},
      {EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID, "GpuCompositeDone"},
      {EGL_DISPLAY_PRESENT_TIME_ANDROID, "ScanOutStart"},
      {EGL_DEQUEUE_READY_TIME_ANDROID, "DequeueReady"},
      {EGL_READS_DONE_TIME_ANDROID, "ReadsDone"},
  };

  supported_egl_timestamps_.reserve(kMaxTimestampsSupportable);
  supported_event_names_.reserve(kMaxTimestampsSupportable);
  for (const auto& ts : all_timestamps) {
    if (!eglGetFrameTimestampSupportedANDROID(GetDisplay(), surface_,
                                              ts.egl_name))
      continue;

    // For presentation feedback, prefer the actual scan out time, but fallback
    // to SurfaceFlinger's composite time since some devices don't support
    // the former.
    switch (ts.egl_name) {
      case EGL_FIRST_COMPOSITION_START_TIME_ANDROID:
        // Value of presentation_feedback_index_ relies on the order of
        // all_timestamps.
        presentation_feedback_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        composition_start_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        presentation_flags_ = 0;
        break;
      case EGL_DISPLAY_PRESENT_TIME_ANDROID:
        presentation_feedback_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        presentation_flags_ = gfx::PresentationFeedback::kVSync |
                              gfx::PresentationFeedback::kHWCompletion;
        break;
    }

    // Stored in separate vectors so we can pass the egl timestamps
    // directly to the EGL functions.
    supported_egl_timestamps_.push_back(ts.egl_name);
    supported_event_names_.push_back(ts.name);
  }
  DCHECK_GE(presentation_feedback_index_, 0);
  DCHECK_GE(composition_start_index_, 0);

  use_egl_timestamps_ = !supported_egl_timestamps_.empty();
}

bool NativeViewGLSurfaceEGL::InitializeNativeWindow() {
  return true;
}

void NativeViewGLSurfaceEGL::Destroy() {
  presentation_helper_ = nullptr;
  vsync_provider_internal_ = nullptr;

  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool NativeViewGLSurfaceEGL::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceEGL::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:RealSwapBuffers",
      "width", GetSize().width(),
      "height", GetSize().height());

  if (!CommitAndClearPendingOverlays()) {
    DVLOG(1) << "Failed to commit pending overlay planes.";
    return gfx::SwapResult::SWAP_FAILED;
  }

  EGLuint64KHR new_frame_id = 0;
  bool new_frame_id_is_valid = true;
  if (use_egl_timestamps_) {
    new_frame_id_is_valid =
        !!eglGetNextFrameIdANDROID(GetDisplay(), surface_, &new_frame_id);
  }
  if (!new_frame_id_is_valid)
    new_frame_id = -1;

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback), new_frame_id);

  if (!eglSwapBuffers(GetDisplay(), surface_)) {
    DVLOG(1) << "eglSwapBuffers failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  } else if (use_egl_timestamps_) {
    UpdateSwapEvents(new_frame_id, new_frame_id_is_valid);
  }

  return scoped_swap_buffers.result();
}

void NativeViewGLSurfaceEGL::UpdateSwapEvents(EGLuint64KHR newFrameId,
                                              bool newFrameIdIsValid) {
  // Queue info for the frame just swapped.
  swap_info_queue_.push({newFrameIdIsValid, newFrameId});

  // Make sure we have a frame old enough that all it's timstamps should
  // be available by now.
  constexpr int kFramesAgoToGetServerTimestamps = 4;
  if (swap_info_queue_.size() <= kFramesAgoToGetServerTimestamps)
    return;

  // TraceEvents if needed.
  // If we weren't able to get a valid frame id before the swap, we can't get
  // its timestamps now.
  const SwapInfo& old_swap_info = swap_info_queue_.front();
  if (old_swap_info.frame_id_is_valid && g_trace_swap_enabled.Get().value)
    TraceSwapEvents(old_swap_info.frame_id);

  swap_info_queue_.pop();
}

void NativeViewGLSurfaceEGL::TraceSwapEvents(EGLuint64KHR oldFrameId) {
  // We shouldn't be calling eglGetFrameTimestampsANDROID with more timestamps
  // than it supports.
  DCHECK_LE(supported_egl_timestamps_.size(), kMaxTimestampsSupportable);

  // Get the timestamps.
  std::vector<EGLnsecsANDROID> egl_timestamps(supported_egl_timestamps_.size(),
                                              EGL_TIMESTAMP_INVALID_ANDROID);
  if (!eglGetFrameTimestampsANDROID(
          GetDisplay(), surface_, oldFrameId,
          static_cast<EGLint>(supported_egl_timestamps_.size()),
          supported_egl_timestamps_.data(), egl_timestamps.data())) {
    TRACE_EVENT_INSTANT0("gpu", "eglGetFrameTimestamps:Failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Track supported and valid time/name pairs.
  struct TimeNamePair {
    base::TimeTicks time;
    const char* name;
  };

  std::vector<TimeNamePair> tracePairs;
  tracePairs.reserve(supported_egl_timestamps_.size());
  for (size_t i = 0; i < egl_timestamps.size(); i++) {
    // Although a timestamp of 0 is technically valid, we shouldn't expect to
    // see it in practice. 0's are more likely due to a known linux kernel bug
    // that inadvertently discards timestamp information when merging two
    // retired fences.
    if (egl_timestamps[i] == 0 ||
        egl_timestamps[i] == EGL_TIMESTAMP_INVALID_ANDROID ||
        egl_timestamps[i] == EGL_TIMESTAMP_PENDING_ANDROID) {
      continue;
    }
    // TODO(brianderson): Replace FromInternalValue usage.
    tracePairs.push_back(
        {base::TimeTicks::FromInternalValue(
             egl_timestamps[i] / base::TimeTicks::kNanosecondsPerMicrosecond),
         supported_event_names_[i]});
  }
  if (tracePairs.empty()) {
    TRACE_EVENT_INSTANT0("gpu", "TraceSwapEvents:NoValidTimestamps",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Sort the pairs so we can trace them in order.
  std::sort(tracePairs.begin(), tracePairs.end(),
            [](auto& a, auto& b) { return a.time < b.time; });

  // Trace the overall range under which the sub events will be nested.
  // Add an epsilon since the trace viewer interprets timestamp ranges
  // as closed on the left and open on the right. i.e.: [begin, end).
  // The last sub event isn't nested properly without the epsilon.
  auto epsilon = base::TimeDelta::FromMicroseconds(1);
  static const char* SwapEvents = "SwapEvents";
  const int64_t trace_id = oldFrameId;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kSwapEventTraceCategories, SwapEvents, trace_id, tracePairs.front().time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      kSwapEventTraceCategories, SwapEvents, trace_id,
      tracePairs.back().time + epsilon, "id", trace_id);

  // Trace the first event, which does not have a range before it.
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT_WITH_TIMESTAMP0(
      kSwapEventTraceCategories, tracePairs.front().name, trace_id,
      tracePairs.front().time);

  // Trace remaining events and their ranges.
  // Use the first characters to represent events still pending.
  // This helps color code the remaining events in the viewer, which makes
  // it obvious:
  //   1) when the order of events are different between frames and
  //   2) if multiple events occurred very close together.
  std::string valid_symbols(tracePairs.size(), '\0');
  for (size_t i = 0; i < valid_symbols.size(); i++)
    valid_symbols[i] = tracePairs[i].name[0];

  const char* pending_symbols = valid_symbols.c_str();
  for (size_t i = 1; i < tracePairs.size(); i++) {
    pending_symbols++;
    TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, pending_symbols, trace_id,
        tracePairs[i - 1].time);
    TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, pending_symbols, trace_id,
        tracePairs[i].time);
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, tracePairs[i].name, trace_id,
        tracePairs[i].time);
  }
}

std::unique_ptr<gfx::VSyncProvider>
NativeViewGLSurfaceEGL::CreateVsyncProviderInternal() {
  return nullptr;
}

gfx::Size NativeViewGLSurfaceEGL::GetSize() {
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(GetDisplay(), surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(GetDisplay(), surface_, EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
}

bool NativeViewGLSurfaceEGL::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha) {
  if (size == GetSize())
    return true;
  size_ = size;
  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);
  Destroy();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to resize window.";
    return false;
  }
  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in NativeViewGLSurfaceEGL::Resize";
    return false;
  }
  SetVSyncEnabled(vsync_enabled_);
  return true;
}

bool NativeViewGLSurfaceEGL::Recreate() {
  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);
  Destroy();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to create surface.";
    return false;
  }
  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in NativeViewGLSurfaceEGL::Recreate";
    return false;
  }
  SetVSyncEnabled(vsync_enabled_);
  return true;
}

EGLSurface NativeViewGLSurfaceEGL::GetHandle() {
  return surface_;
}

bool NativeViewGLSurfaceEGL::SupportsPostSubBuffer() {
  return supports_post_sub_buffer_;
}

gfx::SurfaceOrigin NativeViewGLSurfaceEGL::GetOrigin() const {
  return surface_origin_;
}

EGLTimestampClient* NativeViewGLSurfaceEGL::GetEGLTimestampClient() {
  // This api call is used by GLSurfacePresentationHelper class which is member
  // of this class NativeViewGLSurfaceEGL. Hence its guaranteed "this" pointer
  // will live longer than the GLSurfacePresentationHelper class.
  return this;
}

bool NativeViewGLSurfaceEGL::IsEGLTimestampSupported() const {
  return use_egl_timestamps_;
}

bool NativeViewGLSurfaceEGL::GetFrameTimestampInfoIfAvailable(
    base::TimeTicks* presentation_time,
    base::TimeDelta* composite_interval,
    uint32_t* presentation_flags,
    int frame_id) {
  DCHECK(presentation_time);
  DCHECK(composite_interval);
  DCHECK(presentation_flags);

  TRACE_EVENT1("gpu", "NativeViewGLSurfaceEGL:GetFrameTimestampInfoIfAvailable",
               "frame_id", frame_id);

  // Get the composite interval.
  EGLint interval_name = EGL_COMPOSITE_INTERVAL_ANDROID;
  EGLnsecsANDROID composite_interval_ns = 0;
  *presentation_flags = 0;

  // If an error is generated, we will treat it as a frame done for timestamp
  // reporting purpose.
  if (!eglGetCompositorTimingANDROID(GetDisplay(), surface_, 1, &interval_name,
                                     &composite_interval_ns)) {
    *composite_interval = base::TimeDelta::FromNanoseconds(
        base::TimeTicks::kNanosecondsPerSecond / 60);
    // If we couldn't get the correct presentation time due to some errors,
    // return the current time.
    *presentation_time = base::TimeTicks::Now();
    return true;
  }

  // If the composite interval is pending, the frame is not yet done.
  if (composite_interval_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    return false;
  }
  DCHECK_GT(composite_interval_ns, 0);
  *composite_interval = base::TimeDelta::FromNanoseconds(composite_interval_ns);

  // Get the all available timestamps for the frame. If a frame is invalid or
  // an error is generated,  we will treat it as a frame done for timestamp
  // reporting purpose.
  std::vector<EGLnsecsANDROID> egl_timestamps(supported_egl_timestamps_.size(),
                                              EGL_TIMESTAMP_INVALID_ANDROID);

  // TODO(vikassoni): File a driver bug for eglGetFrameTimestampsANDROID().
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=966638.
  // As per the spec, the driver is expected to return a valid timestamp from
  // the call eglGetFrameTimestampsANDROID() when its not
  // EGL_TIMESTAMP_PENDING_ANDROID or EGL_TIMESTAMP_INVALID_ANDROID. But
  // currently some buggy drivers an invalid timestamp 0.
  // This is currentlt handled in chrome for by setting the presentation time to
  // TimeTicks::Now() (snapped to the next vsync) instead of 0.
  if ((frame_id < 0) ||
      !eglGetFrameTimestampsANDROID(
          GetDisplay(), surface_, frame_id,
          static_cast<EGLint>(supported_egl_timestamps_.size()),
          supported_egl_timestamps_.data(), egl_timestamps.data())) {
    // If we couldn't get the correct presentation time due to some errors,
    // return the current time.
    *presentation_time = base::TimeTicks::Now();
    return true;
  }
  DCHECK_GE(presentation_feedback_index_, 0);
  DCHECK_GE(composition_start_index_, 0);

  // Get the presentation time.
  EGLnsecsANDROID presentation_time_ns =
      egl_timestamps[presentation_feedback_index_];

  // If the presentation time is pending, the frame is not yet done.
  if (presentation_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    return false;
  }
  if (presentation_time_ns == EGL_TIMESTAMP_INVALID_ANDROID) {
    presentation_time_ns = egl_timestamps[composition_start_index_];
    if (presentation_time_ns == EGL_TIMESTAMP_INVALID_ANDROID ||
        presentation_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
      *presentation_time = base::TimeTicks::Now();
    } else {
      *presentation_time = base::TimeTicks() + base::TimeDelta::FromNanoseconds(
                                                   presentation_time_ns);
    }
  } else {
    *presentation_time = base::TimeTicks() +
                         base::TimeDelta::FromNanoseconds(presentation_time_ns);
    *presentation_flags = presentation_flags_;
  }
  return true;
}

gfx::SwapResult NativeViewGLSurfaceEGL::SwapBuffersWithDamage(
    const std::vector<int>& rects,
    PresentationCallback callback) {
  DCHECK(supports_swap_buffer_with_damage_);
  if (!CommitAndClearPendingOverlays()) {
    DVLOG(1) << "Failed to commit pending overlay planes.";
    return gfx::SwapResult::SWAP_FAILED;
  }

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglSwapBuffersWithDamageKHR(GetDisplay(), surface_,
                                   const_cast<EGLint*>(rects.data()),
                                   static_cast<EGLint>(rects.size() / 4))) {
    DVLOG(1) << "eglSwapBuffersWithDamageKHR failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  }
  return scoped_swap_buffers.result();
}

gfx::SwapResult NativeViewGLSurfaceEGL::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:PostSubBuffer", "width", width,
               "height", height);
  DCHECK(supports_post_sub_buffer_);
  if (!CommitAndClearPendingOverlays()) {
    DVLOG(1) << "Failed to commit pending overlay planes.";
    return gfx::SwapResult::SWAP_FAILED;
  }
  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    // With EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE the contents are rendered
    // inverted, but the PostSubBuffer rectangle is still measured from the
    // bottom left.
    y = GetSize().height() - y - height;
  }

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglPostSubBufferNV(GetDisplay(), surface_, x, y, width, height)) {
    DVLOG(1) << "eglPostSubBufferNV failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  }
  return scoped_swap_buffers.result();
}

bool NativeViewGLSurfaceEGL::SupportsCommitOverlayPlanes() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

gfx::SwapResult NativeViewGLSurfaceEGL::CommitOverlayPlanes(
    PresentationCallback callback) {
  DCHECK(SupportsCommitOverlayPlanes());
  // Here we assume that the overlays scheduled on this surface will display
  // themselves to the screen right away in |CommitAndClearPendingOverlays|,
  // rather than being queued and waiting for a "swap" signal.
  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!CommitAndClearPendingOverlays())
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  return scoped_swap_buffers.result();
}

bool NativeViewGLSurfaceEGL::OnMakeCurrent(GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  return GLSurfaceEGL::OnMakeCurrent(context);
}

gfx::VSyncProvider* NativeViewGLSurfaceEGL::GetVSyncProvider() {
  return vsync_provider_external_ ? vsync_provider_external_.get()
                                  : vsync_provider_internal_.get();
}

void NativeViewGLSurfaceEGL::SetVSyncEnabled(bool enabled) {
  DCHECK(GLContext::GetCurrent() && GLContext::GetCurrent()->IsCurrent(this));
  vsync_enabled_ = enabled;
  if (!eglSwapInterval(GetDisplay(), enabled ? 1 : 0)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

bool NativeViewGLSurfaceEGL::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
#if !defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return false;
#else
  pending_overlays_.push_back(GLSurfaceOverlay(z_order, transform, image,
                                               bounds_rect, crop_rect, true,
                                               std::move(gpu_fence)));
  return true;
#endif
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  Destroy();
#if defined(OS_ANDROID)
  if (window_)
    ANativeWindow_release(window_);
#endif
}

bool NativeViewGLSurfaceEGL::CommitAndClearPendingOverlays() {
  if (pending_overlays_.empty())
    return true;

  bool success = true;
#if defined(OS_ANDROID)
  for (auto& overlay : pending_overlays_)
    success &= overlay.ScheduleOverlayPlane(window_);
  pending_overlays_.clear();
#else
  NOTIMPLEMENTED();
#endif
  return success;
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(const gfx::Size& size)
    : size_(size),
      surface_(NULL) {
  // Some implementations of Pbuffer do not support having a 0 size. For such
  // cases use a (1, 1) surface.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool PbufferGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  EGLSurface old_surface = surface_;

#if defined(OS_ANDROID)
  // This is to allow context virtualization which requires on- and offscreen
  // to use a compatible config. We expect the client to request RGB565
  // onscreen surface also for this to work (with the exception of
  // fullscreen video).
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512)
    format.SetRGB565();
#endif

  format_ = format;

  EGLDisplay display = GetDisplay();
  if (!display) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  // Allocate the new pbuffer surface before freeing the old one to ensure
  // they have different addresses. If they have the same address then a
  // future call to MakeCurrent might early out because it appears the current
  // context and surface have not changed.
  EGLint pbuffer_attribs[] = {
      EGL_WIDTH, size_.width(), EGL_HEIGHT, size_.height(), EGL_NONE,
  };

  EGLSurface new_surface =
      eglCreatePbufferSurface(display, GetConfig(), pbuffer_attribs);
  if (!new_surface) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (old_surface)
    eglDestroySurface(display, old_surface);

  surface_ = new_surface;
  return true;
}

void PbufferGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool PbufferGLSurfaceEGL::IsOffscreen() {
  return true;
}

gfx::SwapResult PbufferGLSurfaceEGL::SwapBuffers(
    PresentationCallback callback) {
  NOTREACHED() << "Attempted to call SwapBuffers on a PbufferGLSurfaceEGL.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size PbufferGLSurfaceEGL::GetSize() {
  return size_;
}

bool PbufferGLSurfaceEGL::Resize(const gfx::Size& size,
                                 float scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 bool has_alpha) {
  if (size == size_)
    return true;

  size_ = size;

  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);

  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to resize pbuffer.";
    return false;
  }

  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in PbufferGLSurfaceEGL::Resize";
    return false;
  }

  return true;
}

EGLSurface PbufferGLSurfaceEGL::GetHandle() {
  return surface_;
}

void* PbufferGLSurfaceEGL::GetShareHandle() {
#if defined(OS_ANDROID)
  NOTREACHED();
  return NULL;
#else
  if (!g_driver_egl.ext.b_EGL_ANGLE_query_surface_pointer)
    return NULL;

  if (!g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle)
    return NULL;

  void* handle;
  if (!eglQuerySurfacePointerANGLE(g_egl_display, GetHandle(),
                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                   &handle)) {
    return NULL;
  }

  return handle;
#endif
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  Destroy();
}

SurfacelessEGL::SurfacelessEGL(const gfx::Size& size) : size_(size) {}

bool SurfacelessEGL::Initialize(GLSurfaceFormat format) {
  format_ = format;
  return true;
}

void SurfacelessEGL::Destroy() {
}

bool SurfacelessEGL::IsOffscreen() {
  return true;
}

bool SurfacelessEGL::IsSurfaceless() const {
  return true;
}

gfx::SwapResult SurfacelessEGL::SwapBuffers(PresentationCallback callback) {
  LOG(ERROR) << "Attempted to call SwapBuffers with SurfacelessEGL.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size SurfacelessEGL::GetSize() {
  return size_;
}

bool SurfacelessEGL::Resize(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  size_ = size;
  return true;
}

EGLSurface SurfacelessEGL::GetHandle() {
  return EGL_NO_SURFACE;
}

void* SurfacelessEGL::GetShareHandle() {
  return NULL;
}

SurfacelessEGL::~SurfacelessEGL() {
}

}  // namespace gl
