// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_egl.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/khronos/EGL/egl.h"
#include "third_party/khronos/EGL/eglext.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#ifndef EGL_CHROMIUM_create_context_bind_generates_resource
#define EGL_CHROMIUM_create_context_bind_generates_resource 1
#define EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM 0x33AD
#endif /* EGL_CHROMIUM_create_context_bind_generates_resource */

#ifndef EGL_ANGLE_create_context_webgl_compatibility
#define EGL_ANGLE_create_context_webgl_compatibility 1
#define EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE 0x33AC
#endif /* EGL_ANGLE_create_context_webgl_compatibility */

#ifndef EGL_ANGLE_display_texture_share_group
#define EGL_ANGLE_display_texture_share_group 1
#define EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE 0x33AF
#endif /* EGL_ANGLE_display_texture_share_group */

#ifndef EGL_ANGLE_display_semaphore_share_group
#define EGL_ANGLE_display_semaphore_share_group 1
#define EGL_DISPLAY_SEMAPHORE_SHARE_GROUP_ANGLE 0x348D
#endif /* EGL_ANGLE_display_semaphore_share_group */

#ifndef EGL_ANGLE_external_context_and_surface
#define EGL_ANGLE_external_context_and_surface 1
#define EGL_EXTERNAL_CONTEXT_ANGLE 0x348E
#endif /* EGL_ANGLE_external_context_and_surface */

#ifndef EGL_ANGLE_create_context_client_arrays
#define EGL_ANGLE_create_context_client_arrays 1
#define EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE 0x3452
#endif /* EGL_ANGLE_create_context_client_arrays */

#ifndef EGL_ANGLE_robust_resource_initialization
#define EGL_ANGLE_robust_resource_initialization 1
#define EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_display_robust_resource_initialization */

#ifndef EGL_ANGLE_create_context_backwards_compatible
#define EGL_ANGLE_create_context_backwards_compatible 1
#define EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE 0x3483
#endif /* EGL_ANGLE_create_context_backwards_compatible */

#ifndef EGL_CONTEXT_PRIORITY_LEVEL_IMG
#define EGL_CONTEXT_PRIORITY_LEVEL_IMG 0x3100
#define EGL_CONTEXT_PRIORITY_HIGH_IMG 0x3101
#define EGL_CONTEXT_PRIORITY_MEDIUM_IMG 0x3102
#define EGL_CONTEXT_PRIORITY_LOW_IMG 0x3103
#endif /* EGL_CONTEXT_PRIORITY_LEVEL */

#ifndef EGL_ANGLE_power_preference
#define EGL_ANGLE_power_preference 1
#define EGL_POWER_PREFERENCE_ANGLE 0x3482
#define EGL_LOW_POWER_ANGLE 0x0001
#define EGL_HIGH_POWER_ANGLE 0x0002
#endif /* EGL_ANGLE_power_preference */

#ifndef EGL_NV_robustness_video_memory_purge
#define EGL_NV_robustness_video_memory_purge 1
#define EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV 0x334C
#endif /*EGL_NV_robustness_video_memory_purge */

#ifndef EGL_ANGLE_context_virtualization
#define EGL_ANGLE_context_virtualization 1
#define EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE 0x3481
#endif /* EGL_ANGLE_context_virtualization */

using ui::GetEGLErrorString;
using ui::GetLastEGLErrorString;

namespace gl {

namespace {

// Change the specified attribute in context_attributes. This fails if
// the attribute is not already present. Returns true on success, false
// otherwise.
bool ChangeContextAttributes(std::vector<EGLint>& context_attributes,
                             EGLint attribute,
                             EGLint value) {
  auto iter = base::ranges::find(context_attributes, attribute);
  if (iter != context_attributes.end()) {
    ++iter;
    if (iter != context_attributes.end()) {
      *iter = value;
      return true;
    }
  }

  return false;
}

bool IsARMSwiftShaderPlatform() {
#if BUILDFLAG(IS_MAC)
  return base::mac::GetCPUType() == base::mac::CPUType::kArm;
#elif BUILDFLAG(IS_IOS)
  return true;
#elif BUILDFLAG(IS_WIN)
  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->GetArchitecture();
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  return windows_architecture == base::win::OSInfo::ARM64_ARCHITECTURE ||
         os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64();
#else
  // SwiftShader is not used on Android
  return false;
#endif
}

}  // namespace

GLContextEGL::GLContextEGL(GLShareGroup* share_group)
    : GLContextReal(share_group) {}

bool GLContextEGL::InitializeImpl(GLSurface* compatible_surface,
                                  const GLContextAttribs& attribs) {
  DCHECK(compatible_surface);
  DCHECK(!context_);

  gl_display_ = static_cast<GLDisplayEGL*>(compatible_surface->GetGLDisplay());
  DCHECK(gl_display_);

  EGLint context_client_major_version = attribs.client_major_es_version;
  EGLint context_client_minor_version = attribs.client_minor_es_version;

  // Always prefer to use EGL_KHR_no_config_context so that all surfaces and
  // contexts are compatible
  if (!gl_display_->ext->b_EGL_KHR_no_config_context) {
    config_ = compatible_surface->GetConfig();
    if (!config_) {
      LOG(ERROR) << "Failed to get config for surface "
                 << compatible_surface->GetHandle();
      return false;
    }
    EGLint config_renderable_type = 0;
    if (!eglGetConfigAttrib(gl_display_->GetDisplay(), config_,
                            EGL_RENDERABLE_TYPE, &config_renderable_type)) {
      LOG(ERROR) << "eglGetConfigAttrib failed with error "
                 << GetLastEGLErrorString();
      return false;
    }

    // If the requested context is ES3 but the config cannot support ES3,
    // request ES2 instead.
    if ((config_renderable_type & EGL_OPENGL_ES3_BIT) == 0 &&
        context_client_major_version >= 3) {
      context_client_major_version = 2;
      context_client_minor_version = 0;
    }
  }

  std::vector<EGLint> context_attributes;
  if (attribs.can_skip_validation &&
      GetGLImplementation() == kGLImplementationEGLANGLE) {
    context_attributes.push_back(EGL_CONTEXT_OPENGL_NO_ERROR_KHR);
    context_attributes.push_back(EGL_TRUE);
  }

  // EGL_KHR_create_context allows requesting both a major and minor context
  // version
  if (gl_display_->ext->b_EGL_KHR_create_context) {
    context_attributes.push_back(EGL_CONTEXT_MAJOR_VERSION);
    context_attributes.push_back(context_client_major_version);

    context_attributes.push_back(EGL_CONTEXT_MINOR_VERSION);
    context_attributes.push_back(context_client_minor_version);
  } else {
    context_attributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
    context_attributes.push_back(context_client_major_version);

    // Can only request 2.0 or 3.0 contexts without the EGL_KHR_create_context
    // extension, DCHECK to make sure we update the code to support devices
    // without this extension
    DCHECK(context_client_minor_version == 0);
  }

  bool is_swangle = IsSoftwareGLImplementation(GetGLImplementationParts());

  if (attribs.webgl_compatibility_context && is_swangle &&
      IsARMSwiftShaderPlatform() &&
      !features::IsSwiftShaderAllowedByCommandLine(
          base::CommandLine::ForCurrentProcess())) {
    // crbug.com/1378476: LLVM 10 is used as the JIT compiler for SwiftShader,
    // which doesn't fully support ARM. Disable Swiftshader on ARM CPUs for
    // WebGL until LLVM is upgraded.
    // Allow SwiftShader if explicitly requested by command line for testing.
    DVLOG(1) << __FUNCTION__
             << ": Software WebGL contexts are not supported on ARM CPUs.";
    return false;
  }

  if (gl_display_->ext->b_EGL_EXT_create_context_robustness || is_swangle) {
    DVLOG(1) << "EGL_EXT_create_context_robustness supported.";
    context_attributes.push_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT);
    context_attributes.push_back(
        (attribs.robust_buffer_access || is_swangle) ? EGL_TRUE : EGL_FALSE);
    if (attribs.lose_context_on_reset) {
      context_attributes.push_back(
          EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
      context_attributes.push_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);

      if (gl_display_->ext->b_EGL_NV_robustness_video_memory_purge) {
        context_attributes.push_back(
            EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV);
        context_attributes.push_back(EGL_TRUE);
      }
    }
  } else {
    // At some point we should require the presence of the robustness
    // extension and remove this code path.
    DVLOG(1) << "EGL_EXT_create_context_robustness NOT supported.";
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    LOG(ERROR) << "eglBindApi failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (gl_display_->ext->b_EGL_CHROMIUM_create_context_bind_generates_resource) {
    context_attributes.push_back(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    context_attributes.push_back(attribs.bind_generates_resource ? EGL_TRUE
                                                                 : EGL_FALSE);
  } else {
    DCHECK(attribs.bind_generates_resource);
  }

  if (gl_display_->ext->b_EGL_ANGLE_create_context_webgl_compatibility) {
    context_attributes.push_back(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    context_attributes.push_back(
        attribs.webgl_compatibility_context ? EGL_TRUE : EGL_FALSE);
  } else {
    DCHECK(!attribs.webgl_compatibility_context);
  }

  if (gl_display_->IsEGLContextPrioritySupported()) {
    // Medium priority is the default, only set the attribute if
    // a different priority is requested.
    if (attribs.context_priority == ContextPriorityLow) {
      DVLOG(1) << __FUNCTION__ << ": setting ContextPriorityLow";
      context_attributes.push_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
      context_attributes.push_back(EGL_CONTEXT_PRIORITY_LOW_IMG);
    } else if (attribs.context_priority == ContextPriorityHigh) {
      DVLOG(1) << __FUNCTION__ << ": setting ContextPriorityHigh";
      context_attributes.push_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
      context_attributes.push_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }
  }

  global_texture_share_group_ = attribs.global_texture_share_group;
  if (gl_display_->ext->b_EGL_ANGLE_display_texture_share_group) {
    context_attributes.push_back(EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE);
    context_attributes.push_back(global_texture_share_group_ ? EGL_TRUE
                                                             : EGL_FALSE);
  } else {
    DCHECK(!global_texture_share_group_);
  }

  if (gl_display_->ext->b_EGL_ANGLE_display_semaphore_share_group) {
    context_attributes.push_back(EGL_DISPLAY_SEMAPHORE_SHARE_GROUP_ANGLE);
    context_attributes.push_back(
        attribs.global_semaphore_share_group ? EGL_TRUE : EGL_FALSE);
  } else {
    DCHECK(!attribs.global_semaphore_share_group);
  }

  if (gl_display_->ext->b_EGL_ANGLE_create_context_client_arrays) {
    context_attributes.push_back(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    context_attributes.push_back(attribs.allow_client_arrays ? EGL_TRUE
                                                             : EGL_FALSE);
  } else {
    // Client arrays are allowed by default without
    // ANGLE_create_context_client_arrays to control it. Verify that's the
    // requested behavior.
    DCHECK(attribs.allow_client_arrays);
  }

  if (gl_display_->ext->b_EGL_ANGLE_robust_resource_initialization ||
      is_swangle) {
    context_attributes.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    context_attributes.push_back(
        (attribs.robust_resource_initialization || is_swangle) ? EGL_TRUE
                                                               : EGL_FALSE);
  } else {
    DCHECK(!attribs.robust_resource_initialization);
  }

  if (gl_display_->ext->b_EGL_ANGLE_create_context_backwards_compatible) {
    // Request a specific context version. The Passthrough command decoder
    // relies on the returned context being the exact version it requested.
    context_attributes.push_back(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
    context_attributes.push_back(EGL_FALSE);
  }

  if (gl_display_->ext->b_EGL_ANGLE_power_preference) {
    GpuPreference pref = attribs.gpu_preference;
    pref = GLSurface::AdjustGpuPreference(pref);
    switch (pref) {
      case GpuPreference::kDefault:
        // Don't request any GPU, let ANGLE and the native driver decide.
        break;
      case GpuPreference::kLowPower:
        context_attributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
        context_attributes.push_back(EGL_LOW_POWER_ANGLE);
        break;
      case GpuPreference::kHighPerformance:
        context_attributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
        context_attributes.push_back(EGL_HIGH_POWER_ANGLE);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  if (gl_display_->ext->b_EGL_ANGLE_external_context_and_surface) {
    if (attribs.angle_create_from_external_context) {
      context_attributes.push_back(EGL_EXTERNAL_CONTEXT_ANGLE);
      context_attributes.push_back(EGL_TRUE);
    }
  }

  angle_context_virtualization_group_number_ =
      attribs.angle_context_virtualization_group_number;
  if (gl_display_->ext->b_EGL_ANGLE_context_virtualization) {
    context_attributes.push_back(EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE);
    context_attributes.push_back(
        static_cast<EGLint>(angle_context_virtualization_group_number_));
  }

  // Append final EGL_NONE to signal the context attributes are finished
  context_attributes.push_back(EGL_NONE);
  context_attributes.push_back(EGL_NONE);

  context_ =
      eglCreateContext(gl_display_->GetDisplay(), config_,
                       share_group() ? share_group()->GetHandle() : nullptr,
                       context_attributes.data());
  if (context_) {
    return true;
  }

  // If EGL_KHR_no_config_context is in use and context creation failed,
  // it might indicate that an unsupported ES version was requested. Try
  // falling back to a lower version.
  GLint error = eglGetError();
  if (gl_display_->ext->b_EGL_KHR_no_config_context &&
      (error == EGL_BAD_MATCH || error == EGL_BAD_ATTRIBUTE)) {
    // Set up the list of versions to try: 3.1 -> 3.0 -> 2.0
    std::vector<std::pair<EGLint, EGLint>> candidate_versions;
    if (context_client_major_version == 3 &&
        context_client_minor_version == 1) {
      candidate_versions.emplace_back(3, 0);
      candidate_versions.emplace_back(2, 0);
    } else if (context_client_major_version == 3 &&
               context_client_minor_version == 0) {
      candidate_versions.emplace_back(2, 0);
    }

    for (const auto& version : candidate_versions) {
      if (!ChangeContextAttributes(context_attributes,
                                   EGL_CONTEXT_MAJOR_VERSION, version.first) ||
          !ChangeContextAttributes(context_attributes,
                                   EGL_CONTEXT_MINOR_VERSION, version.second)) {
        break;
      }

      context_ =
          eglCreateContext(gl_display_->GetDisplay(), config_,
                           share_group() ? share_group()->GetHandle() : nullptr,
                           context_attributes.data());
      // Stop searching as soon as a context is successfully created.
      if (context_) {
        return true;
      } else {
        error = eglGetError();
      }
    }
  }

  LOG(ERROR) << "eglCreateContext failed with error "
             << GetEGLErrorString(error);
  return false;
}

void GLContextEGL::Destroy() {
  ReleaseBackpressureFences();
  OnContextWillDestroy();
  if (context_) {
    if (!eglDestroyContext(gl_display_->GetDisplay(), context_)) {
      LOG(ERROR) << "eglDestroyContext failed with error "
                 << GetLastEGLErrorString();
    }

    context_ = nullptr;
  }
}

void GLContextEGL::SetVisibility(bool visibility) {
  if (gl_display_->ext->b_EGL_ANGLE_power_preference) {
    // It doesn't matter whether this context was explicitly allocated
    // with a power preference - ANGLE will take care of any default behavior.
    if (visibility) {
      eglReacquireHighPowerGPUANGLE(gl_display_->GetDisplay(), context_);
    } else {
      eglReleaseHighPowerGPUANGLE(gl_display_->GetDisplay(), context_);
    }
  }
}

GLDisplayEGL* GLContextEGL::GetGLDisplayEGL() {
  return gl_display_;
}

GLContextEGL* GLContextEGL::AsGLContextEGL() {
  return this;
}

bool GLContextEGL::CanShareTexturesWithContext(GLContext* other_context) {
  GLContextEGL* other_egl_context = other_context->AsGLContextEGL();
  if (!other_egl_context) {
    return false;
  }

  if (GLContext::CanShareTexturesWithContext(other_egl_context)) {
    return true;
  }

  // Contexts can share texture using EGL_ANGLE_display_texture_share_group
  // extension if they are on the same display and same virtualization group
  // number.
  return global_texture_share_group_ &&
         other_egl_context->global_texture_share_group_ &&
         angle_context_virtualization_group_number_ ==
             other_egl_context->angle_context_virtualization_group_number_ &&
         GetGLDisplayEGL() == other_egl_context->GetGLDisplayEGL();
}

void GLContextEGL::ReleaseBackpressureFences() {
#if BUILDFLAG(IS_APPLE)
  bool has_backpressure_fences = HasBackpressureFences();
#else
  bool has_backpressure_fences = false;
#endif

  if (has_backpressure_fences) {
    // If this context is not current, bind this context's API so that the YUV
    // converter can safely destruct
    GLContext* prev_current_context = GetRealCurrent();
    if (prev_current_context != this) {
      SetThreadLocalCurrentGL(GetCurrentGL());
    }

    EGLContext current_egl_context = eglGetCurrentContext();
    EGLSurface current_draw_surface = EGL_NO_SURFACE;
    EGLSurface current_read_surface = EGL_NO_SURFACE;
    if (context_ != current_egl_context) {
      current_draw_surface = eglGetCurrentSurface(EGL_DRAW);
      current_read_surface = eglGetCurrentSurface(EGL_READ);
      if (!eglMakeCurrent(gl_display_->GetDisplay(), EGL_NO_SURFACE,
                          EGL_NO_SURFACE, context_)) {
        LOG(ERROR) << "eglMakeCurrent failed with error "
                   << GetLastEGLErrorString();
      }
    }

#if BUILDFLAG(IS_APPLE)
    DestroyBackpressureFences();
#endif

    // Rebind the current context's API if needed.
    if (prev_current_context != this) {
      SetThreadLocalCurrentGL(prev_current_context
                                  ? prev_current_context->GetCurrentGL()
                                  : nullptr);
    }

    if (context_ != current_egl_context) {
      if (!eglMakeCurrent(gl_display_->GetDisplay(), current_draw_surface,
                          current_read_surface, current_egl_context)) {
        LOG(ERROR) << "eglMakeCurrent failed with error "
                   << GetLastEGLErrorString();
      }
    }
  }
}

bool GLContextEGL::MakeCurrentImpl(GLSurface* surface) {
  DCHECK(context_);
  if (lost_) {
    LOG(ERROR) << "Failed to make context current since it is marked as lost";
    return false;
  }
  if (IsCurrent(surface))
    return true;

  ScopedReleaseCurrent release_current;
  TRACE_EVENT2("gpu", "GLContextEGL::MakeCurrent", "context",
               static_cast<void*>(context_), "surface",
               static_cast<void*>(surface));

  if (unbind_fbo_on_makecurrent_ && GetCurrent()) {
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  }

  if (!eglMakeCurrent(gl_display_->GetDisplay(), surface->GetHandle(),
                      surface->GetHandle(), context_)) {
    LOG(ERROR) << "eglMakeCurrent failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  // Set this as soon as the context is current, since we might call into GL.
  BindGLApi();

  SetCurrent(surface);
  InitializeDynamicBindings();

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Could not make current.";
    return false;
  }

  release_current.Cancel();
  return true;
}

void GLContextEGL::SetUnbindFboOnMakeCurrent() {
  unbind_fbo_on_makecurrent_ = true;
}

void GLContextEGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  if (unbind_fbo_on_makecurrent_)
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

  SetCurrent(nullptr);
  if (!eglMakeCurrent(gl_display_->GetDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT)) {
    LOG(ERROR) << "eglMakeCurrent failed to release current with error "
               << GetLastEGLErrorString();
    lost_ = true;
  }

  DCHECK(!IsCurrent(nullptr));
}

bool GLContextEGL::IsCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (lost_)
    return false;

  bool native_context_is_current = context_ == eglGetCurrentContext();

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetRealCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    if (surface->GetHandle() != eglGetCurrentSurface(EGL_DRAW))
      return false;
  }

  if (gl_display_) {
    if (gl_display_->GetDisplay() != eglGetCurrentDisplay()) {
      return false;
    }
  }

  return true;
}

void* GLContextEGL::GetHandle() {
  return context_;
}

unsigned int GLContextEGL::CheckStickyGraphicsResetStatusImpl() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(g_current_gl_driver);
  const ExtensionsGL& ext = g_current_gl_driver->ext;
  if ((graphics_reset_status_ == GL_NO_ERROR) &&
      gl_display_->ext->b_EGL_EXT_create_context_robustness &&
      (ext.b_GL_KHR_robustness || ext.b_GL_EXT_robustness)) {
    graphics_reset_status_ = glGetGraphicsResetStatusARB();
  }
  return graphics_reset_status_;
}

GLContextEGL::~GLContextEGL() {
  Destroy();
}

}  // namespace gl
