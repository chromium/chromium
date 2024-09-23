// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_environment_variable_override.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display_egl_util.h"
#include "ui/gl/gl_display_manager.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/sync_control_vsync_provider.h"

#if !defined(EGL_FIXED_SIZE_ANGLE)
#define EGL_FIXED_SIZE_ANGLE 0x3201
#endif

#if !defined(EGL_OPENGL_ES3_BIT)
#define EGL_OPENGL_ES3_BIT 0x00000040
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

#ifndef EGL_ANGLE_robust_resource_initialization
#define EGL_ANGLE_robust_resource_initialization 1
#define EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_display_robust_resource_initialization */

#ifndef EGL_ANGLE_surface_orientation
#define EGL_ANGLE_surface_orientation
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif /* EGL_ANGLE_surface_orientation */

using ui::GetLastEGLErrorString;

namespace gl {

namespace {

constexpr const char kSwapEventTraceCategories[] = "gpu";

constexpr size_t kMaxTimestampsSupportable = 9;

struct TraceSwapEventsInitializer {
  TraceSwapEventsInitializer()
      : value(*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
            kSwapEventTraceCategories)) {}
  const raw_ref<const unsigned char> value;
};

static base::LazyInstance<TraceSwapEventsInitializer>::Leaky
    g_trace_swap_enabled = LAZY_INSTANCE_INITIALIZER;

class EGLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  EGLSyncControlVSyncProvider(EGLSurface surface, GLDisplayEGL* display)
      : surface_(surface), display_(display) {
    DCHECK(display_);
  }

  EGLSyncControlVSyncProvider(const EGLSyncControlVSyncProvider&) = delete;
  EGLSyncControlVSyncProvider& operator=(const EGLSyncControlVSyncProvider&) =
      delete;

  ~EGLSyncControlVSyncProvider() override {}

  static bool IsSupported(GLDisplayEGL* display) {
    DCHECK(display);
    return SyncControlVSyncProvider::IsSupported() &&
           display->ext->b_EGL_CHROMIUM_sync_control;
  }

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    uint64_t u_system_time, u_media_stream_counter, u_swap_buffer_counter;
    bool result =
        eglGetSyncValuesCHROMIUM(display_->GetDisplay(), surface_,
                                 &u_system_time, &u_media_stream_counter,
                                 &u_swap_buffer_counter) == EGL_TRUE;
    if (result) {
      *system_time = static_cast<int64_t>(u_system_time);
      *media_stream_counter = static_cast<int64_t>(u_media_stream_counter);
      *swap_buffer_counter = static_cast<int64_t>(u_swap_buffer_counter);
    }
    return result;
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!display_->ext->b_EGL_ANGLE_sync_control_rate) {
      return false;
    }

    bool result = eglGetMscRateANGLE(display_->GetDisplay(), surface_,
                                     numerator, denominator) == EGL_TRUE;
    return result;
  }

  bool IsHWClock() const override { return true; }

 private:
  EGLSurface surface_;
  raw_ptr<GLDisplayEGL> display_;
};

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

EGLConfig ChooseConfig(EGLDisplay display,
                       GLSurfaceFormat format,
                       bool surfaceless,
                       bool offscreen,
                       EGLint visual_id) {
  // Choose an EGL configuration.
  // On X this is only used for PBuffer surfaces.

  std::vector<EGLint> renderable_types;
  if (!GetGlWorkarounds().disable_es3gl_context) {
    renderable_types.push_back(EGL_OPENGL_ES3_BIT);
  }
  renderable_types.push_back(EGL_OPENGL_ES2_BIT);

  EGLint alpha_size = 8;
  bool want_rgb565 = format.IsRGB565();
  EGLint buffer_size = want_rgb565 ? 16 : 32;

  // Some platforms (eg. X11) may want to set custom values for alpha and buffer
  // sizes.
  GLDisplayEglUtil::GetInstance()->ChoosePlatformCustomAlphaAndBufferSize(
      &alpha_size, &buffer_size);

  EGLint surface_type =
      (surfaceless
           ? EGL_DONT_CARE
           : (offscreen ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT | EGL_PBUFFER_BIT));

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
    if (!ValidateEglConfig(display, choose_attributes, &num_configs)) {
      // Try the next renderable_type
      continue;
    }

    auto matching_configs = base::HeapArray<EGLConfig>::Uninit(num_configs);
    if (want_rgb565 || visual_id >= 0) {
      config_size = num_configs;
      config_data = matching_configs.data();
    }

    if (!eglChooseConfig(display, choose_attributes, config_data, config_size,
                         &num_configs)) {
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
        if (eglGetConfigAttrib(display, matching_configs[i], EGL_RED_SIZE,
                               &red) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_BLUE_SIZE,
                               &blue) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_GREEN_SIZE,
                               &green) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_ALPHA_SIZE,
                               &alpha) &&
            alpha == 0 && red == 5 && green == 6 && blue == 5) {
          config = matching_configs[i];
          match_found = true;
          break;
        }
      }
      if (!match_found) {
        // To fall back to default 32 bit format, choose with
        // the right attributes again.
        if (!ValidateEglConfig(display, config_attribs_8888, &num_configs)) {
          // Try the next renderable_type
          continue;
        }
        if (!eglChooseConfig(display, config_attribs_8888, &config, 1,
                             &num_configs)) {
          LOG(ERROR) << "eglChooseConfig failed with error "
                     << GetLastEGLErrorString();
          return config;
        }
      }
    } else if (visual_id >= 0) {
      for (int i = 0; i < num_configs; i++) {
        EGLint id;
        if (eglGetConfigAttrib(display, matching_configs[i],
                               EGL_NATIVE_VISUAL_ID, &id) &&
            id == visual_id) {
          config = matching_configs[i];
          break;
        }
      }
    }
    return config;
  }

  LOG(ERROR) << "No suitable EGL configs found.";
  return nullptr;
}

}  // namespace

GLSurfaceEGL::GLSurfaceEGL(GLDisplayEGL* display) : display_(display) {
  DCHECK(display_);
}

GLSurfaceFormat GLSurfaceEGL::GetFormat() {
  return format_;
}

GLDisplay* GLSurfaceEGL::GetGLDisplay() {
  return display_;
}

EGLConfig GLSurfaceEGL::GetConfig() {
  if (!config_) {
    config_ = ChooseConfig(display_->GetDisplay(), format_, IsSurfaceless(),
                           IsOffscreen(), GetNativeVisualID());
  }
  return config_;
}

EGLint GLSurfaceEGL::GetNativeVisualID() const {
  return -1;
}

EGLDisplay GLSurfaceEGL::GetEGLDisplay() {
  return display_->GetDisplay();
}

// static
GLDisplayEGL* GLSurfaceEGL::GetGLDisplayEGL() {
  return GLDisplayManagerEGL::GetInstance()->GetDisplay(
      GpuPreference::kDefault);
}

GLSurfaceEGL::~GLSurfaceEGL() {
  // InvalidateWeakPtrs should be called from the concrete dtors.
  CHECK(!HasWeakPtrs());
}

#if BUILDFLAG(IS_ANDROID)
NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(
    GLDisplayEGL* display,
    ScopedANativeWindow scoped_window,
    std::unique_ptr<gfx::VSyncProvider> vsync_provider)
    : GLSurfaceEGL(display),
      scoped_window_(std::move(scoped_window)),
      window_(scoped_window_.a_native_window()),
      vsync_provider_external_(std::move(vsync_provider)) {}
#else
NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(
    GLDisplayEGL* display,
    EGLNativeWindowType window,
    std::unique_ptr<gfx::VSyncProvider> vsync_provider)
    : GLSurfaceEGL(display),
      window_(window),
      vsync_provider_external_(std::move(vsync_provider)) {
#if BUILDFLAG(IS_WIN)
  RECT windowRect;
  if (GetClientRect(window_, &windowRect))
    size_ = gfx::Rect(windowRect).size();
#endif
}
#endif  // BUILDFLAG(IS_ANDROID)

bool NativeViewGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  DCHECK(!surface_);
  format_ = format;

  if (display_->GetDisplay() == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Trying to create NativeViewGLSurfaceEGL with invalid "
               << "display.";
    return false;
  }

  // We need to make sure that window_ is correctly initialized with all
  // the platform-dependant quirks, if any, before creating the surface.
  if (!InitializeNativeWindow()) {
    LOG(ERROR) << "Error trying to initialize the native window.";
    return false;
  }

  if (!GetConfig()) {
    LOG(ERROR) << "No suitable EGL configs found for initialization.";
    return false;
  }

  std::vector<EGLint> egl_window_attributes;

  if (display_->ext->b_EGL_ANGLE_window_fixed_size &&
      enable_fixed_size_angle_) {
    egl_window_attributes.push_back(EGL_FIXED_SIZE_ANGLE);
    egl_window_attributes.push_back(EGL_TRUE);
    egl_window_attributes.push_back(EGL_WIDTH);
    egl_window_attributes.push_back(size_.width());
    egl_window_attributes.push_back(EGL_HEIGHT);
    egl_window_attributes.push_back(size_.height());
  }

  if (display_->ext->b_EGL_NV_post_sub_buffer) {
    egl_window_attributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
    egl_window_attributes.push_back(EGL_TRUE);
  }

  if (display_->ext->b_EGL_ANGLE_surface_orientation) {
    EGLint attrib;
    eglGetConfigAttrib(display_->GetDisplay(), GetConfig(),
                       EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &attrib);
    surface_origin_ = (attrib == EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE)
                          ? gfx::SurfaceOrigin::kTopLeft
                          : gfx::SurfaceOrigin::kBottomLeft;
  }

  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_ANGLE);
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
  }

  // Note that COLORSPACE_LINEAR refers to the sRGB color space, but
  // without opting into sRGB blending. It is equivalent to
  // COLORSPACE_SRGB with Disable(FRAMEBUFFER_SRGB).
  if (display_->ext->b_EGL_KHR_gl_colorspace) {
    egl_window_attributes.push_back(EGL_GL_COLORSPACE_KHR);
    egl_window_attributes.push_back(EGL_GL_COLORSPACE_LINEAR_KHR);
  }

  egl_window_attributes.push_back(EGL_NONE);
  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(display_->GetDisplay(), GetConfig(),
                                    window_, &egl_window_attributes[0]);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  if (display_->ext->b_EGL_NV_post_sub_buffer) {
    EGLint surfaceVal;
    EGLBoolean retVal =
        eglQuerySurface(display_->GetDisplay(), surface_,
                        EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceVal);
    supports_post_sub_buffer_ = (surfaceVal && retVal) == EGL_TRUE;
  }

  supports_swap_buffer_with_damage_ =
      display_->ext->b_EGL_KHR_swap_buffers_with_damage;

  if (!vsync_provider_external_ &&
      EGLSyncControlVSyncProvider::IsSupported(display_)) {
    vsync_provider_internal_ =
        std::make_unique<EGLSyncControlVSyncProvider>(surface_, display_);
  }

  if (!vsync_provider_external_ && !vsync_provider_internal_)
    vsync_provider_internal_ = CreateVsyncProviderInternal();

  presentation_helper_ =
      std::make_unique<GLSurfacePresentationHelper>(GetVSyncProvider());
  return true;
}

bool NativeViewGLSurfaceEGL::SupportsSwapTimestamps() const {
  return display_->ext->b_EGL_ANDROID_get_frame_timestamps;
}

void NativeViewGLSurfaceEGL::SetEnableSwapTimestamps() {
  DCHECK(display_->ext->b_EGL_ANDROID_get_frame_timestamps);

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

  eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                   EGL_TRUE);

  // Check if egl composite interval is supported or not. If not then return.
  // Else check which other timestamps are supported.
  EGLint interval_name = EGL_COMPOSITE_INTERVAL_ANDROID;
  if (!eglGetCompositorTimingSupportedANDROID(display_->GetDisplay(), surface_,
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
    if (!eglGetFrameTimestampSupportedANDROID(display_->GetDisplay(), surface_,
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
      case EGL_RENDERING_COMPLETE_TIME_ANDROID:
        writes_done_index_ = static_cast<int>(supported_egl_timestamps_.size());
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

  // Recreate the presentation helper here to make sure egl_timestamp_client_
  // in |presentation_helper_| is initialized after |use_egl_timestamp_| is
  // initialized.
  presentation_helper_ =
      std::make_unique<GLSurfacePresentationHelper>(GetVSyncProvider());
}

bool NativeViewGLSurfaceEGL::InitializeNativeWindow() {
  return true;
}

void NativeViewGLSurfaceEGL::Destroy() {
  presentation_helper_ = nullptr;
  vsync_provider_internal_ = nullptr;

  if (surface_) {
    if (!eglDestroySurface(display_->GetDisplay(), surface_)) {
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
    PresentationCallback callback,
    gfx::FrameData data) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:RealSwapBuffers",
      "width", GetSize().width(),
      "height", GetSize().height());

  EGLuint64KHR new_frame_id = 0;
  bool new_frame_id_is_valid = true;
  if (use_egl_timestamps_) {
    new_frame_id_is_valid = !!eglGetNextFrameIdANDROID(display_->GetDisplay(),
                                                       surface_, &new_frame_id);
  }
  if (!new_frame_id_is_valid)
    new_frame_id = -1;

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback), new_frame_id);

  if (!eglSwapBuffers(display_->GetDisplay(), surface_)) {
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
  if (old_swap_info.frame_id_is_valid && *g_trace_swap_enabled.Get().value)
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
          display_->GetDisplay(), surface_, oldFrameId,
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
  auto epsilon = base::Microseconds(1);
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
    UNSAFE_TODO(pending_symbols++);
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
  if (!eglQuerySurface(display_->GetDisplay(), surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(display_->GetDisplay(), surface_, EGL_HEIGHT, &height)) {
    LOG(ERROR) << "eglQuerySurface failed with error "
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
  if (use_egl_timestamps_) {
    eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                     EGL_TRUE);
  }
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
  if (use_egl_timestamps_) {
    eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                     EGL_TRUE);
  }
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
    base::TimeTicks* writes_done_time,
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
  if (!eglGetCompositorTimingANDROID(GetEGLDisplay(), surface_, 1,
                                     &interval_name, &composite_interval_ns)) {
    *composite_interval =
        base::Nanoseconds(base::TimeTicks::kNanosecondsPerSecond / 60);
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
  *composite_interval = base::Nanoseconds(composite_interval_ns);

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
          display_->GetDisplay(), surface_, frame_id,
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
      *presentation_time =
          base::TimeTicks() + base::Nanoseconds(presentation_time_ns);
    }
  } else {
    *presentation_time =
        base::TimeTicks() + base::Nanoseconds(presentation_time_ns);
    *presentation_flags = presentation_flags_;
  }

  // Get the WritesDone time if available, otherwise set to a null TimeTicks.
  EGLnsecsANDROID writes_done_time_ns = egl_timestamps[writes_done_index_];
  if (writes_done_time_ns == EGL_TIMESTAMP_INVALID_ANDROID ||
      writes_done_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    *writes_done_time = base::TimeTicks();
  } else {
    *writes_done_time =
        base::TimeTicks() + base::Nanoseconds(writes_done_time_ns);
  }

  return true;
}

gfx::SwapResult NativeViewGLSurfaceEGL::SwapBuffersWithDamage(
    const std::vector<int>& rects,
    PresentationCallback callback,
    gfx::FrameData data) {
  DCHECK(supports_swap_buffer_with_damage_);

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglSwapBuffersWithDamageKHR(display_->GetDisplay(), surface_,
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
    PresentationCallback callback,
    gfx::FrameData data) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:PostSubBuffer", "width", width,
               "height", height);
  DCHECK(supports_post_sub_buffer_);
  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    // With EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE the contents are rendered
    // inverted, but the PostSubBuffer rectangle is still measured from the
    // bottom left.
    y = GetSize().height() - y - height;
  }

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglPostSubBufferNV(GetEGLDisplay(), surface_, x, y, width, height)) {
    DVLOG(1) << "eglPostSubBufferNV failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  }
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
  if (!eglSwapInterval(display_->GetDisplay(), enabled ? 1 : 0)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  InvalidateWeakPtrs();
  Destroy();
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(GLDisplayEGL* display,
                                         const gfx::Size& size)
    : GLSurfaceEGL(display), size_(size), surface_(nullptr) {
  // Some implementations of Pbuffer do not support having a 0 size. For such
  // cases use a (1, 1) surface.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool PbufferGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  if (display_->GetDisplay() == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Trying to create PbufferGLSurfaceEGL with invalid "
               << "display.";
    return false;
  }

  if (!GetConfig()) {
    LOG(ERROR) << "No suitable EGL configs found for initialization.";
    return false;
  }

  EGLSurface old_surface = surface_;

#if BUILDFLAG(IS_ANDROID)
  // This is to allow context virtualization which requires on- and offscreen
  // to use a compatible config. We expect the client to request RGB565
  // onscreen surface also for this to work (with the exception of
  // fullscreen video).
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512)
    format.SetRGB565();
#endif

  format_ = format;

  // Allocate the new pbuffer surface before freeing the old one to ensure
  // they have different addresses. If they have the same address then a
  // future call to MakeCurrent might early out because it appears the current
  // context and surface have not changed.
  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(size_.width());
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(size_.height());

  // Enable robust resource init when using SwANGLE
  if (IsSoftwareGLImplementation(GetGLImplementationParts()) &&
      display_->ext->b_EGL_ANGLE_robust_resource_initialization) {
    pbuffer_attribs.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    pbuffer_attribs.push_back(EGL_TRUE);
  }

  // Append final EGL_NONE to signal the pbuffer attributes are finished
  pbuffer_attribs.push_back(EGL_NONE);
  pbuffer_attribs.push_back(EGL_NONE);

  EGLSurface new_surface = eglCreatePbufferSurface(
      display_->GetDisplay(), GetConfig(), &pbuffer_attribs[0]);
  if (!new_surface) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (old_surface)
    eglDestroySurface(display_->GetDisplay(), old_surface);

  surface_ = new_surface;
  return true;
}

void PbufferGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(display_->GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool PbufferGLSurfaceEGL::IsOffscreen() {
  return true;
}

gfx::SwapResult PbufferGLSurfaceEGL::SwapBuffers(PresentationCallback callback,
                                                 gfx::FrameData data) {
  NOTREACHED_IN_MIGRATION()
      << "Attempted to call SwapBuffers on a PbufferGLSurfaceEGL.";
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
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#else
  if (!display_->ext->b_EGL_ANGLE_query_surface_pointer)
    return nullptr;

  if (!display_->ext->b_EGL_ANGLE_surface_d3d_texture_2d_share_handle)
    return nullptr;

  void* handle;
  if (!eglQuerySurfacePointerANGLE(display_->GetDisplay(), GetHandle(),
                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                   &handle)) {
    return nullptr;
  }

  return handle;
#endif
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  InvalidateWeakPtrs();
  Destroy();
}

SurfacelessEGL::SurfacelessEGL(GLDisplayEGL* display, const gfx::Size& size)
    : GLSurfaceEGL(display), size_(size) {}

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

gfx::SwapResult SurfacelessEGL::SwapBuffers(PresentationCallback callback,
                                            gfx::FrameData data) {
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
  return nullptr;
}

SurfacelessEGL::~SurfacelessEGL() {
  InvalidateWeakPtrs();
}

}  // namespace gl
