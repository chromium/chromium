// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

#include <array>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/shader_tracking.h"

namespace gl {

// The GL state for when no context is bound
static CurrentGL* g_no_context_current_gl = nullptr;

// If the null draw bindings are currently enabled.
// TODO: Consider adding a new GLApi that no-ops these functions
static bool g_null_draw_bindings_enabled = false;

namespace {

// TODO(epenner): Could the above function be merged into GetInternalFormat and
// removed?
static inline GLenum GetTexInternalFormat(const GLVersionInfo* version,
                                          GLenum internal_format,
                                          GLenum format,
                                          GLenum type) {
  GLenum gl_internal_format = GetInternalFormat(version, internal_format);

  // g_version_info must be initialized when this function is bound.
  if (version->is_es3) {
    if (internal_format == GL_RED_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_R8_EXT;
          break;
        case GL_UNSIGNED_SHORT:
          gl_internal_format = GL_R16_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_R16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_R32F_EXT;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      return gl_internal_format;
    } else if (internal_format == GL_RG_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_RG8_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_RG16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_RG32F_EXT;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      return gl_internal_format;
    }
  }

  if (version->IsAtLeastGLES(3, 0)) {
    switch (internal_format) {
      case GL_SRGB_EXT:
        gl_internal_format = GL_SRGB8;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_internal_format = GL_SRGB8_ALPHA8;
        break;
      default:
        break;
    }
  }

  if (version->is_es2)
    return gl_internal_format;

  // For ES3, use sized float/half_float internal formats whenever posssible.
  if (type == GL_FLOAT) {
    switch (internal_format) {
      // We need to map all the unsized internal formats from ES2 clients.
      case GL_RGBA:
        gl_internal_format = GL_RGBA32F;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB32F;
        break;
      default:
        // We can't assert here because if the client context is ES3,
        // all sized internal_format will reach here.
        break;
    }
  } else if (type == GL_HALF_FLOAT_OES) {
    switch (internal_format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA16F;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB16F;
        break;
      default:
        break;
    }
  }

  return gl_internal_format;
}

static inline GLenum GetTexFormat(const GLVersionInfo* version, GLenum format) {
  GLenum gl_format = format;

  if (version->IsAtLeastGLES(3, 0)) {
    switch (format) {
      case GL_SRGB_EXT:
        gl_format = GL_RGB;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_format = GL_RGBA;
        break;
      default:
        break;
    }
  }

  return gl_format;
}

static inline GLenum GetPixelType(const GLVersionInfo* version,
                                  GLenum type,
                                  GLenum format) {
  if (!version->is_es2 && type == GL_HALF_FLOAT_OES) {
    // For ES3+, use HALF_FLOAT instead of HALF_FLOAT_OES whenever possible.
    switch (format) {
      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
      case GL_ALPHA:
        return type;
      default:
        return GL_HALF_FLOAT;
    }
  }
  return type;
}

}  // anonymous namespace

GLenum GetInternalFormat(const GLVersionInfo* version, GLenum internal_format) {
  if (version->is_es3 && version->is_mesa) {
    // Mesa bug workaround: Mipmapping does not work when using GL_BGRA_EXT
    if (internal_format == GL_BGRA_EXT)
      return GL_RGBA;
  }
  return internal_format;
}

void InitializeStaticGLBindingsGL() {
  g_no_context_current_gl = new CurrentGL;
  g_no_context_current_gl->Api = new NoContextGLApi;
}

CurrentGL*& ThreadLocalCurrentGL() {
  thread_local CurrentGL* current_gl = nullptr;
  return current_gl;
}

CurrentGL* GetThreadLocalCurrentGL() {
  // This prevents mutation of the thread local CurrentGL pointer.
  return ThreadLocalCurrentGL();
}

void SetThreadLocalCurrentGL(CurrentGL* current) {
  ThreadLocalCurrentGL() = current ? current : g_no_context_current_gl;
}

void ClearBindingsGL() {
  if (g_no_context_current_gl) {
    delete g_no_context_current_gl->Api;
    delete g_no_context_current_gl->Driver;
    delete g_no_context_current_gl->Version;
    delete g_no_context_current_gl;
    g_no_context_current_gl = nullptr;
  }
  ThreadLocalCurrentGL() = nullptr;
}

bool SetNullDrawGLBindingsEnabled(bool enabled) {
  bool old_value = g_null_draw_bindings_enabled;
  g_null_draw_bindings_enabled = enabled;
  return old_value;
}

bool GetNullDrawBindingsEnabled() {
  return g_null_draw_bindings_enabled;
}

GLApi::GLApi() = default;

GLApi::~GLApi() = default;

GLApiBase::GLApiBase() : driver_(nullptr) {}

GLApiBase::~GLApiBase() = default;

void GLApiBase::InitializeBase(DriverGL* driver) {
  driver_ = driver;
}

RealGLApi::RealGLApi()
    : logging_enabled_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGPUServiceLogging)) {}

RealGLApi::~RealGLApi() = default;

void RealGLApi::Initialize(DriverGL* driver) {
  InitializeBase(driver);
}

void RealGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  if (pname == GL_NUM_EXTENSIONS && disabled_exts_.size()) {
    InitializeFilteredExtensionsIfNeeded();
    *params = static_cast<GLint>(filtered_exts_.size());
  } else {
    GLApiBase::glGetIntegervFn(pname, params);
  }
}

const GLubyte* RealGLApi::glGetStringFn(GLenum name) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
    InitializeFilteredExtensionsIfNeeded();
    return reinterpret_cast<const GLubyte*>(filtered_exts_str_.c_str());
  }
  return GLApiBase::glGetStringFn(name);
}

const GLubyte* RealGLApi::glGetStringiFn(GLenum name, GLuint index) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
    InitializeFilteredExtensionsIfNeeded();
    if (index >= filtered_exts_.size()) {
      return nullptr;
    }
    return reinterpret_cast<const GLubyte*>(filtered_exts_[index].c_str());
  }
  return GLApiBase::glGetStringiFn(name, index);
}

void RealGLApi::glTexImage2DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  GLenum gl_internal_format =
      GetTexInternalFormat(version_.get(), internalformat, format, type);
  GLenum gl_format = GetTexFormat(version_.get(), format);
  GLenum gl_type = GetPixelType(version_.get(), type, format);

  GLApiBase::glTexImage2DFn(target, level, gl_internal_format, width, height,
                            border, gl_format, gl_type, pixels);
}

void RealGLApi::glTexSubImage2DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  GLenum gl_format = GetTexFormat(version_.get(), format);
  GLenum gl_type = GetPixelType(version_.get(), type, format);
  GLApiBase::glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                               gl_format, gl_type, pixels);
}

void RealGLApi::glTexStorage2DEXTFn(GLenum target,
                                    GLsizei levels,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glTexStorage2DEXTFn(target, levels, gl_internal_format, width,
                                 height);
}

void RealGLApi::glTexStorageMem2DEXTFn(GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLuint memory,
                                       GLuint64 offset) {
  internalformat = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glTexStorageMem2DEXTFn(target, levels, internalformat, width,
                                    height, memory, offset);
}

void RealGLApi::glTexStorageMemFlags2DANGLEFn(
    GLenum target,
    GLsizei levels,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLuint memory,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void* imageCreateInfoPNext) {
  internalformat = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glTexStorageMemFlags2DANGLEFn(
      target, levels, internalformat, width, height, memory, offset,
      createFlags, usageFlags, imageCreateInfoPNext);
}

void RealGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageEXTFn(target, gl_internal_format, width,
                                        height);
}

// The ANGLE and IMG variants of glRenderbufferStorageMultisample currently do
// not support BGRA render buffers so only the EXT one is customized. If
// GL_CHROMIUM_renderbuffer_format_BGRA8888 support is added to ANGLE then the
// ANGLE version should also be customized.
void RealGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageMultisampleEXTFn(
      target, samples, gl_internal_format, width, height);
}

void RealGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageMultisampleFn(
      target, samples, gl_internal_format, width, height);
}

void RealGLApi::glReadPixelsFn(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               void* pixels) {
  GLenum gl_type = GetPixelType(version_.get(), type, format);
  GLApiBase::glReadPixelsFn(x, y, width, height, format, gl_type, pixels);
}

void RealGLApi::glClearFn(GLbitfield mask) {
  if (!g_null_draw_bindings_enabled) {
    GLApiBase::glClearFn(mask);
  } else if (logging_enabled_) {
    LOG(WARNING) << "Skipped glClear()";
  }
}

void RealGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  if (!g_null_draw_bindings_enabled) {
    GLApiBase::glDrawArraysFn(mode, first, count);
  } else if (logging_enabled_) {
    LOG(WARNING) << "Skipped glDrawArrays()";
  }
}

void RealGLApi::glDrawElementsFn(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void* indices) {
  if (!g_null_draw_bindings_enabled) {
    GLApiBase::glDrawElementsFn(mode, count, type, indices);
  } else if (logging_enabled_) {
    LOG(WARNING) << "Skipped glDrawElements()";
  }
}

void RealGLApi::glClearDepthFn(GLclampd depth) {
  // OpenGL ES only has glClearDepthf, forward the parameters from glClearDepth.
  // Many mock tests expect only glClearDepth is called so don't make the
  // interception when testing with mocks.
  if (GetGLImplementation() != kGLImplementationMockGL) {
    DCHECK(driver_->fn.glClearDepthfFn);
    GLApiBase::glClearDepthfFn(static_cast<GLclampf>(depth));
  } else {
    DCHECK(driver_->fn.glClearDepthFn);
    GLApiBase::glClearDepthFn(depth);
  }
}

void RealGLApi::glDepthRangeFn(GLclampd z_near, GLclampd z_far) {
  // OpenGL ES only has glDepthRangef, forward the parameters from glDepthRange.
  // Many mock tests expect only glDepthRange is called so don't make the
  // interception when testing with mocks.
  if (GetGLImplementation() != kGLImplementationMockGL) {
    DCHECK(driver_->fn.glDepthRangefFn);
    GLApiBase::glDepthRangefFn(static_cast<GLclampf>(z_near),
                               static_cast<GLclampf>(z_far));
  } else {
    DCHECK(driver_->fn.glDepthRangeFn);
    GLApiBase::glDepthRangeFn(z_near, z_far);
  }
}

void RealGLApi::glUseProgramFn(GLuint program) {
  ShaderTracking* shader_tracking = ShaderTracking::GetInstance();
  if (shader_tracking) {
    std::array<std::vector<char>, 2> buffers;
    std::array<char*, 2> strings = {};
    if (program) {
      // The following only works with ANGLE backend because ANGLE makes sure
      // a program's shaders are not actually deleted and source can still be
      // queried even if glDeleteShaders() has been called on them.

      // Also, in theory, different shaders can be attached to the program
      // after the last link, but for now, ignore such corner case patterns.
      GLsizei count = 0;
      std::array<GLuint, 2> shaders = {};
      glGetAttachedShadersFn(program, 2, &count, shaders.data());
      for (GLsizei ii = 0; ii < std::min(2, count); ++ii) {
        buffers[ii].resize(ShaderTracking::kMaxShaderSize);
        glGetShaderSourceFn(shaders[ii], ShaderTracking::kMaxShaderSize,
                            nullptr, buffers[ii].data());
        strings[ii] = buffers[ii].data();
      }
    }
    shader_tracking->SetShaders(strings[0], strings[1]);
  }
  GLApiBase::glUseProgramFn(program);
}

void RealGLApi::InitializeFilteredExtensionsIfNeeded() {
  DCHECK(disabled_exts_.size());
  if (filtered_exts_.size())
    return;
  DCHECK(filtered_exts_str_.empty());
  filtered_exts_str_ = FilterGLExtensionList(
      reinterpret_cast<const char*>(GLApiBase::glGetStringFn(GL_EXTENSIONS)),
      disabled_exts_);
  filtered_exts_ = base::SplitString(
      filtered_exts_str_, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void RealGLApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  ClearCachedGLExtensions();
  disabled_exts_.clear();
  if (disabled_extensions.empty())
    return;
  disabled_exts_ =
      base::SplitString(disabled_extensions, ", ;", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  DCHECK(disabled_exts_.size());
}

void RealGLApi::ClearCachedGLExtensions() {
  filtered_exts_.clear();
  filtered_exts_str_.clear();
}

void RealGLApi::set_version(std::unique_ptr<GLVersionInfo> version) {
  version_ = std::move(version);
}

TraceGLApi::~TraceGLApi() = default;

LogGLApi::LogGLApi(GLApi* gl_api) : gl_api_(gl_api) {}

LogGLApi::~LogGLApi() = default;

NoContextGLApi::NoContextGLApi() = default;

NoContextGLApi::~NoContextGLApi() = default;

}  // namespace gl
