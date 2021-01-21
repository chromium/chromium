// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/create_gr_gl_interface.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

#if defined(OS_APPLE)
#include "base/mac/mac_util.h"
#endif

namespace gl {
namespace init {

// This code emulates GL fences (GL_APPLE_sync or GL_ARB_sync) via
// EGL_KHR_fence_sync extension. It's used to provide Skia ways of
// synchronization on platforms that does not have GL fences but support EGL
namespace {
struct EGLFenceData {
  EGLSync sync;
  EGLDisplay display;
};

GLsync glFenceSyncEmulateEGL(GLenum condition, GLbitfield flags) {
  DCHECK(condition == GL_SYNC_GPU_COMMANDS_COMPLETE);
  DCHECK(flags == 0);

  init::EGLFenceData* data = new EGLFenceData;

  data->display = eglGetCurrentDisplay();
  data->sync = eglCreateSyncKHR(data->display, EGL_SYNC_FENCE_KHR, nullptr);

  return reinterpret_cast<GLsync>(data);
}

void glDeleteSyncEmulateEGL(GLsync sync) {
  EGLFenceData* data = reinterpret_cast<EGLFenceData*>(sync);
  eglDestroySyncKHR(data->display, data->sync);
  delete data;
}

GLenum glClientWaitSyncEmulateEGL(GLsync sync,
                                  GLbitfield flags,
                                  GLuint64 timeout) {
  init::EGLFenceData* data = reinterpret_cast<init::EGLFenceData*>(sync);

  EGLint egl_flags = 0;

  if (flags & GL_SYNC_FLUSH_COMMANDS_BIT) {
    egl_flags |= EGL_SYNC_FLUSH_COMMANDS_BIT;
  }
  EGLint result =
      eglClientWaitSyncKHR(data->display, data->sync, egl_flags, timeout);

  switch (result) {
    case EGL_CONDITION_SATISFIED:
      return GL_CONDITION_SATISFIED;
    case EGL_TIMEOUT_EXPIRED:
      return GL_TIMEOUT_EXPIRED;
    case EGL_FALSE:
      return GL_WAIT_FAILED;
  }

  NOTREACHED();
  return 0;
}

void glWaitSyncEmulateEGL(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  init::EGLFenceData* data = reinterpret_cast<init::EGLFenceData*>(sync);

  DCHECK(timeout == GL_TIMEOUT_IGNORED);
  DCHECK(flags == 0);

  if (!g_driver_egl.ext.b_EGL_KHR_wait_sync) {
    eglClientWaitSyncKHR(data->display, data->sync, 0, EGL_FOREVER_KHR);
    return;
  }

  EGLint result = eglWaitSyncKHR(data->display, data->sync, 0);
  DCHECK(result);
}

GLboolean glIsSyncEmulateEGL(GLsync sync) {
  NOTREACHED();
  return true;
}

#if defined(OS_APPLE)
std::map<GLuint, base::TimeTicks>& GetProgramCreateTimesMap() {
  static base::NoDestructor<std::map<GLuint, base::TimeTicks>> instance;
  return *instance.get();
}
#endif

}  // namespace

namespace {

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind(R (gl::GLApi::*func)(Args...),
                                                  gl::GLApi* api) {
  return [func, api](Args... args) { return (api->*func)(args...); };
}

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_slow(
    R(GL_BINDING_CALL* func)(Args...),
    gl::ProgressReporter* progress_reporter) {
  if (!progress_reporter)
    return func;
  return [func, progress_reporter](Args... args) {
    gl::ScopedProgressReporter scoped_reporter(progress_reporter);
    return func(args...);
  };
}

template <bool droppable_call, typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> maybe_drop_call(
    R(GL_BINDING_CALL* func)(Args...)) {
  // One branch is optimized away because droppable_call is set at compile time.
  if (droppable_call) {
    return [func](Args... args) {
      if (!HasInitializedNullDrawGLBindings())
        func(args...);
    };
  } else {
    return func;
  }
}

template <bool droppable_call = false, typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_slow_on_mac(
    R(GL_BINDING_CALL* func)(Args...),
    gl::ProgressReporter* progress_reporter) {
#if defined(OS_APPLE)
  if (!progress_reporter) {
    return maybe_drop_call<droppable_call>(func);
  }
  return [func, progress_reporter](Args... args) {
    gl::ScopedProgressReporter scoped_reporter(progress_reporter);
    // Conditional may be optimized out because droppable_call is set at compile
    // time.
    if (!droppable_call || !HasInitializedNullDrawGLBindings())
      return func(args...);
  };
#endif
  return maybe_drop_call<droppable_call>(func);
}

template <bool droppable_call = false, typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_with_flush_on_mac(
    R(GL_BINDING_CALL* func)(Args...),
    bool is_angle) {
#if defined(OS_APPLE)
  // If running on Apple silicon or ANGLE, regardless of the architecture,
  // disable this workaround.  See https://crbug.com/1131312.
  const bool needs_flush =
      base::mac::GetCPUType() == base::mac::CPUType::kIntel && !is_angle;
  if (needs_flush) {
    return [func](Args... args) {
      // Conditional may be optimized out because droppable_call is set at
      // compile time.
      if (!droppable_call || !HasInitializedNullDrawGLBindings()) {
        {
          TRACE_EVENT0(
              "gpu",
              "CreateGrGLInterface - bind_with_flush_on_mac - beforefunc");
          glFlush();
        }
        func(args...);
        {
          TRACE_EVENT0(
              "gpu",
              "CreateGrGLInterface - bind_with_flush_on_mac - afterfunc");
          glFlush();
        }
      }
    };
  }
#endif
  return maybe_drop_call<droppable_call>(func);
}

template <bool droppable_call = false, typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_slow_with_flush_on_mac(
    R(GL_BINDING_CALL* func)(Args...),
    gl::ProgressReporter* progress_reporter,
    bool is_angle) {
  if (!progress_reporter) {
    return bind_with_flush_on_mac<droppable_call>(func, is_angle);
  }
  return [func, progress_reporter, is_angle](Args... args) {
    gl::ScopedProgressReporter scoped_reporter(progress_reporter);
    return bind_with_flush_on_mac<droppable_call>(func, is_angle)(args...);
  };
}

const GLubyte* GetStringHook(const char* gl_version_string,
                             const char* glsl_version_string,
                             GLenum name) {
  switch (name) {
    case GL_VERSION:
      return reinterpret_cast<const GLubyte*>(gl_version_string);
    case GL_SHADING_LANGUAGE_VERSION:
      return reinterpret_cast<const GLubyte*>(glsl_version_string);
    default:
      return glGetString(name);
  }
}

const char* kBlocklistExtensions[] = {
    "GL_APPLE_framebuffer_multisample",
    "GL_ARB_ES3_1_compatibility",
    "GL_ARB_draw_indirect",
    "GL_ARB_invalidate_subdata",
    "GL_ARB_multi_draw_indirect",
    "GL_ARB_sample_shading",
    "GL_ARB_texture_barrier",
    "GL_EXT_direct_state_access",
    "GL_EXT_multi_draw_indirect",
    "GL_EXT_raster_multisample",
    "GL_NV_bindless_texture",
    "GL_NV_texture_barrier",
    "GL_OES_sample_shading",
};

}  // anonymous namespace

sk_sp<GrGLInterface> CreateGrGLInterface(
    const gl::GLVersionInfo& version_info,
    bool use_version_es2,
    gl::ProgressReporter* progress_reporter) {
  // Can't fake ES with desktop GL.
  use_version_es2 &= version_info.is_es;

  gl::ProcsGL* gl = &gl::g_current_gl_driver->fn;
  gl::GLApi* api = gl::g_current_gl_context;

  GrGLStandard standard =
      version_info.is_es ? kGLES_GrGLStandard : kGL_GrGLStandard;

  // Depending on the advertised version and extensions, skia checks for
  // existence of entrypoints. However some of those we don't yet handle in
  // gl_bindings, so we need to fake the version to the maximum fully supported
  // by the bindings (GL 4.1 or ES 3.0), and blocklist extensions that skia
  // handles but bindings don't.
  // TODO(piman): add bindings for missing entrypoints.
  GrGLFunction<GrGLGetStringFn> get_string;
  const bool apply_version_override = use_version_es2 ||
                                      version_info.IsAtLeastGL(4, 2) ||
                                      version_info.IsAtLeastGLES(3, 1);

  if (apply_version_override || version_info.IsVersionSubstituted()) {
    GLVersionInfo::VersionStrings version;
    if (version_info.IsVersionSubstituted()) {
      version = version_info.GetFakeVersionStrings(version_info.major_version,
                                                   version_info.minor_version);
    } else if (version_info.is_es) {
      if (use_version_es2)
        version = version_info.GetFakeVersionStrings(2, 0);
      else
        version = version_info.GetFakeVersionStrings(3, 0);
    } else {
      version = version_info.GetFakeVersionStrings(4, 1);
    }

    get_string = [version](GLenum name) {
      return GetStringHook(version.gl_version, version.glsl_version, name);
    };
  } else {
    get_string = bind(&gl::GLApi::glGetStringFn, api);
  }

  auto get_stringi = bind(&gl::GLApi::glGetStringiFn, api);
  auto get_integerv = bind(&gl::GLApi::glGetIntegervFn, api);

  GrGLExtensions extensions;
  if (!extensions.init(standard, get_string, get_stringi, get_integerv)) {
    LOG(ERROR) << "Failed to initialize extensions";
    return nullptr;
  }
  for (const char* extension : kBlocklistExtensions)
    extensions.remove(extension);

  GrGLInterface* interface = new GrGLInterface();
  GrGLInterface::Functions* functions = &interface->fFunctions;
  functions->fActiveTexture = gl->glActiveTextureFn;
  functions->fAttachShader = gl->glAttachShaderFn;
  functions->fBindAttribLocation = gl->glBindAttribLocationFn;
  functions->fBindBuffer = gl->glBindBufferFn;
  functions->fBindFragDataLocation = gl->glBindFragDataLocationFn;
  functions->fBindUniformLocation = gl->glBindUniformLocationCHROMIUMFn;
  functions->fBeginQuery = gl->glBeginQueryFn;
  functions->fBindSampler = gl->glBindSamplerFn;
  functions->fBindTexture =
      bind_slow_on_mac(gl->glBindTextureFn, progress_reporter);

  functions->fBlendBarrier = gl->glBlendBarrierKHRFn;

  functions->fBlendColor = gl->glBlendColorFn;
  functions->fBlendEquation = gl->glBlendEquationFn;
  functions->fBlendFunc = gl->glBlendFuncFn;
  functions->fBufferData = gl->glBufferDataFn;
  functions->fBufferSubData = gl->glBufferSubDataFn;
  functions->fClear = bind_slow_with_flush_on_mac<true>(
      gl->glClearFn, progress_reporter, version_info.is_angle);
  functions->fClearColor = gl->glClearColorFn;
  functions->fClearStencil = gl->glClearStencilFn;
  functions->fClearTexImage = gl->glClearTexImageFn;
  functions->fClearTexSubImage = gl->glClearTexSubImageFn;
  functions->fColorMask = gl->glColorMaskFn;
  functions->fCompileShader =
      bind_slow(gl->glCompileShaderFn, progress_reporter);
  functions->fCompressedTexImage2D = bind_slow_with_flush_on_mac(
      gl->glCompressedTexImage2DFn, progress_reporter, version_info.is_angle);
  functions->fCompressedTexSubImage2D =
      bind_slow(gl->glCompressedTexSubImage2DFn, progress_reporter);
  functions->fCopyTexSubImage2D =
      bind_slow(gl->glCopyTexSubImage2DFn, progress_reporter);
#if defined(OS_APPLE)
  functions->fCreateProgram = [func = gl->glCreateProgramFn]() {
    auto& program_create_times = GetProgramCreateTimesMap();
    GLuint program = func();
    program_create_times[program] = base::TimeTicks::Now();
    return program;
  };
#else
  functions->fCreateProgram = gl->glCreateProgramFn;
#endif
  functions->fCreateShader = gl->glCreateShaderFn;
  functions->fCullFace = gl->glCullFaceFn;
  functions->fDeleteBuffers =
      bind_slow(gl->glDeleteBuffersARBFn, progress_reporter);
#if defined(OS_APPLE)
  functions->fDeleteProgram = [func = gl->glDeleteProgramFn](GLuint program) {
    auto& program_create_times = GetProgramCreateTimesMap();
    program_create_times.erase(program);
    func(program);
  };
#else
  functions->fDeleteProgram =
      bind_slow(gl->glDeleteProgramFn, progress_reporter);
#endif
  functions->fDeleteQueries = gl->glDeleteQueriesFn;
  functions->fDeleteSamplers = gl->glDeleteSamplersFn;
  functions->fDeleteShader = bind_slow(gl->glDeleteShaderFn, progress_reporter);
  functions->fDeleteTextures = bind_slow_with_flush_on_mac(
      gl->glDeleteTexturesFn, progress_reporter, version_info.is_angle);
  functions->fDepthMask = gl->glDepthMaskFn;
  functions->fDisable = gl->glDisableFn;
  functions->fDisableVertexAttribArray = gl->glDisableVertexAttribArrayFn;
  functions->fDiscardFramebuffer = gl->glDiscardFramebufferEXTFn;
  functions->fDrawArrays =
      bind_slow_on_mac<true>(gl->glDrawArraysFn, progress_reporter);
  functions->fDrawBuffer = gl->glDrawBufferFn;
  functions->fDrawBuffers = gl->glDrawBuffersARBFn;
  functions->fDrawElements =
      bind_slow_on_mac<true>(gl->glDrawElementsFn, progress_reporter);

  functions->fDrawArraysInstanced = bind_slow_on_mac<true>(
      gl->glDrawArraysInstancedANGLEFn, progress_reporter);
  functions->fDrawArraysInstancedBaseInstance = bind_slow_on_mac<true>(
      gl->glDrawArraysInstancedBaseInstanceANGLEFn, progress_reporter);
  functions->fMultiDrawArraysInstancedBaseInstance = bind_slow_on_mac<true>(
      gl->glMultiDrawArraysInstancedBaseInstanceANGLEFn, progress_reporter);
  functions->fDrawElementsInstanced = bind_slow_on_mac<true>(
      gl->glDrawElementsInstancedANGLEFn, progress_reporter);
  functions->fDrawElementsInstancedBaseVertexBaseInstance =
      bind_slow_on_mac<true>(
          gl->glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn,
          progress_reporter);
  functions->fMultiDrawElementsInstancedBaseVertexBaseInstance =
      bind_slow_on_mac<true>(
          gl->glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn,
          progress_reporter);

  // GL 4.0 or GL_ARB_draw_indirect or ES 3.1
  functions->fDrawArraysIndirect =
      bind_slow_on_mac<true>(gl->glDrawArraysIndirectFn, progress_reporter);
  functions->fDrawElementsIndirect =
      bind_slow_on_mac<true>(gl->glDrawElementsIndirectFn, progress_reporter);

  functions->fDrawRangeElements =
      bind_slow_on_mac<true>(gl->glDrawRangeElementsFn, progress_reporter);
  functions->fEnable = gl->glEnableFn;
  functions->fEnableVertexAttribArray = gl->glEnableVertexAttribArrayFn;
  functions->fEndQuery = gl->glEndQueryFn;
  functions->fFinish = bind_slow(gl->glFinishFn, progress_reporter);
  functions->fFlush = bind_slow(gl->glFlushFn, progress_reporter);
  functions->fFrontFace = gl->glFrontFaceFn;
  functions->fGenBuffers = gl->glGenBuffersARBFn;
  functions->fGetBufferParameteriv = gl->glGetBufferParameterivFn;
  functions->fGetError = gl->glGetErrorFn;
  functions->fGetIntegerv = gl->glGetIntegervFn;
  functions->fGetMultisamplefv = gl->glGetMultisamplefvFn;
  functions->fGetQueryObjectiv = gl->glGetQueryObjectivFn;
  functions->fGetQueryObjectuiv = gl->glGetQueryObjectuivFn;
  functions->fGetQueryObjecti64v = gl->glGetQueryObjecti64vFn;
  functions->fGetQueryObjectui64v = gl->glGetQueryObjectui64vFn;
  functions->fQueryCounter = gl->glQueryCounterFn;
  functions->fGetQueryiv = gl->glGetQueryivFn;
  functions->fGetProgramBinary = gl->glGetProgramBinaryFn;
  functions->fGetProgramInfoLog = gl->glGetProgramInfoLogFn;
#if defined(OS_APPLE)
  functions->fGetProgramiv = [func = gl->glGetProgramivFn](
                                 GLuint program, GLenum pname, GLint* params) {
    func(program, pname, params);
    if (pname == 0x8B82 /* GR_GL_LINK_STATUS */) {
      auto& program_create_times = GetProgramCreateTimesMap();
      auto found = program_create_times.find(program);
      if (found != program_create_times.end()) {
        base::TimeDelta elapsed = base::TimeTicks::Now() - found->second;
        UMA_HISTOGRAM_TIMES("Gpu.GL.ProgramBuildTime", elapsed);
        program_create_times.erase(found);
      }
    }
  };
#else
  functions->fGetProgramiv = gl->glGetProgramivFn;
#endif
  functions->fGetShaderInfoLog = gl->glGetShaderInfoLogFn;
  functions->fGetShaderiv = gl->glGetShaderivFn;
  functions->fGetString = get_string;
  functions->fGetStringi = gl->glGetStringiFn;
  functions->fGetShaderPrecisionFormat = gl->glGetShaderPrecisionFormatFn;
  functions->fGetTexLevelParameteriv = gl->glGetTexLevelParameterivFn;
  functions->fGenQueries = gl->glGenQueriesFn;
  functions->fGenSamplers = gl->glGenSamplersFn;
  functions->fGenTextures = gl->glGenTexturesFn;
  functions->fGetUniformLocation = gl->glGetUniformLocationFn;
  functions->fIsTexture = gl->glIsTextureFn;
  functions->fLineWidth = gl->glLineWidthFn;
  functions->fLinkProgram = bind_slow(gl->glLinkProgramFn, progress_reporter);
  functions->fMapBuffer = gl->glMapBufferFn;

  // GL 4.3 or GL_ARB_multi_draw_indirect or ES+GL_EXT_multi_draw_indirect
  // functions->fMultiDrawArraysIndirect = gl->glMultiDrawArraysIndirectFn;
  // functions->fMultiDrawElementsIndirect = gl->glMultiDrawElementsIndirectFn;

  functions->fPatchParameteri = gl->glPatchParameteriFn;
  functions->fPixelStorei = gl->glPixelStoreiFn;
  functions->fPolygonMode = gl->glPolygonModeFn;
  functions->fProgramBinary = gl->glProgramBinaryFn;
  functions->fProgramParameteri = gl->glProgramParameteriFn;

  // GL_EXT_raster_multisample
  // functions->fRasterSamples = gl->glRasterSamplesEXTFn;

  functions->fReadBuffer = gl->glReadBufferFn;
  functions->fReadPixels = gl->glReadPixelsFn;
  functions->fSamplerParameteri = gl->glSamplerParameteriFn;
  functions->fSamplerParameteriv = gl->glSamplerParameterivFn;
  functions->fScissor = gl->glScissorFn;
  functions->fShaderSource = gl->glShaderSourceFn;
  functions->fStencilFunc = gl->glStencilFuncFn;
  functions->fStencilFuncSeparate = gl->glStencilFuncSeparateFn;
  functions->fStencilMask = gl->glStencilMaskFn;
  functions->fStencilMaskSeparate = gl->glStencilMaskSeparateFn;
  functions->fStencilOp = gl->glStencilOpFn;
  functions->fStencilOpSeparate = gl->glStencilOpSeparateFn;
  functions->fTexBuffer = gl->glTexBufferFn;
  functions->fTexBufferRange = gl->glTexBufferRangeFn;
  functions->fTexImage2D = bind_slow_with_flush_on_mac(
      gl->glTexImage2DFn, progress_reporter, version_info.is_angle);
  functions->fTexParameterf = gl->glTexParameterfFn;
  functions->fTexParameterfv = gl->glTexParameterfvFn;
  functions->fTexParameteri = gl->glTexParameteriFn;
  functions->fTexParameteriv = gl->glTexParameterivFn;
  functions->fTexStorage2D = bind_slow_with_flush_on_mac(
      gl->glTexStorage2DEXTFn, progress_reporter, version_info.is_angle);
  functions->fTexSubImage2D = bind_slow_with_flush_on_mac(
      gl->glTexSubImage2DFn, progress_reporter, version_info.is_angle);

  // GL 4.5 or GL_ARB_texture_barrier or GL_NV_texture_barrier
  // functions->fTextureBarrier = gl->glTextureBarrierFn;
  // functions->fTextureBarrier = gl->glTextureBarrierNVFn;

  functions->fUniform1f = gl->glUniform1fFn;
  functions->fUniform1i = gl->glUniform1iFn;
  functions->fUniform1fv = gl->glUniform1fvFn;
  functions->fUniform1iv = gl->glUniform1ivFn;
  functions->fUniform2f = gl->glUniform2fFn;
  functions->fUniform2i = gl->glUniform2iFn;
  functions->fUniform2fv = gl->glUniform2fvFn;
  functions->fUniform2iv = gl->glUniform2ivFn;
  functions->fUniform3f = gl->glUniform3fFn;
  functions->fUniform3i = gl->glUniform3iFn;
  functions->fUniform3fv = gl->glUniform3fvFn;
  functions->fUniform3iv = gl->glUniform3ivFn;
  functions->fUniform4f = gl->glUniform4fFn;
  functions->fUniform4i = gl->glUniform4iFn;
  functions->fUniform4fv = gl->glUniform4fvFn;
  functions->fUniform4iv = gl->glUniform4ivFn;
  functions->fUniformMatrix2fv = gl->glUniformMatrix2fvFn;
  functions->fUniformMatrix3fv = gl->glUniformMatrix3fvFn;
  functions->fUniformMatrix4fv = gl->glUniformMatrix4fvFn;
  functions->fUnmapBuffer = gl->glUnmapBufferFn;
  functions->fUseProgram = gl->glUseProgramFn;
  functions->fVertexAttrib1f = gl->glVertexAttrib1fFn;
  functions->fVertexAttrib2fv = gl->glVertexAttrib2fvFn;
  functions->fVertexAttrib3fv = gl->glVertexAttrib3fvFn;
  functions->fVertexAttrib4fv = gl->glVertexAttrib4fvFn;

  functions->fVertexAttribDivisor = gl->glVertexAttribDivisorANGLEFn;

  functions->fVertexAttribIPointer = gl->glVertexAttribIPointerFn;

  functions->fVertexAttribPointer = gl->glVertexAttribPointerFn;
  functions->fViewport = gl->glViewportFn;
  functions->fBindFragDataLocationIndexed = gl->glBindFragDataLocationIndexedFn;

  functions->fBindVertexArray = gl->glBindVertexArrayOESFn;
  functions->fGenVertexArrays = gl->glGenVertexArraysOESFn;
  functions->fDeleteVertexArrays = gl->glDeleteVertexArraysOESFn;

  functions->fMapBufferRange = gl->glMapBufferRangeFn;
  functions->fFlushMappedBufferRange = gl->glFlushMappedBufferRangeFn;

  functions->fGenerateMipmap = gl->glGenerateMipmapEXTFn;
  functions->fGenFramebuffers = gl->glGenFramebuffersEXTFn;
  functions->fGetFramebufferAttachmentParameteriv =
      gl->glGetFramebufferAttachmentParameterivEXTFn;
  functions->fGetRenderbufferParameteriv =
      gl->glGetRenderbufferParameterivEXTFn;
  functions->fBindFramebuffer = bind_slow_with_flush_on_mac(
      gl->glBindFramebufferEXTFn, progress_reporter, version_info.is_angle);
  functions->fFramebufferTexture2D = gl->glFramebufferTexture2DEXTFn;
  functions->fCheckFramebufferStatus = gl->glCheckFramebufferStatusEXTFn;
  functions->fDeleteFramebuffers = bind_slow_with_flush_on_mac(
      gl->glDeleteFramebuffersEXTFn, progress_reporter, version_info.is_angle);
  functions->fRenderbufferStorage = bind_with_flush_on_mac(
      gl->glRenderbufferStorageEXTFn, version_info.is_angle);
  functions->fGenRenderbuffers = gl->glGenRenderbuffersEXTFn;
  functions->fDeleteRenderbuffers = bind_with_flush_on_mac(
      gl->glDeleteRenderbuffersEXTFn, version_info.is_angle);
  functions->fFramebufferRenderbuffer = gl->glFramebufferRenderbufferEXTFn;
  functions->fBindRenderbuffer = gl->glBindRenderbufferEXTFn;
  functions->fRenderbufferStorageMultisample = bind_with_flush_on_mac(
      gl->glRenderbufferStorageMultisampleFn, version_info.is_angle);
  functions->fFramebufferTexture2DMultisample =
      gl->glFramebufferTexture2DMultisampleEXTFn;
  functions->fRenderbufferStorageMultisampleES2EXT = bind_with_flush_on_mac(
      gl->glRenderbufferStorageMultisampleEXTFn, version_info.is_angle);
  functions->fBlitFramebuffer =
      bind_with_flush_on_mac(gl->glBlitFramebufferFn, version_info.is_angle);

  functions->fMatrixLoadf = gl->glMatrixLoadfEXTFn;
  functions->fMatrixLoadIdentity = gl->glMatrixLoadIdentityEXTFn;
  functions->fPathCommands = gl->glPathCommandsNVFn;
  functions->fPathParameteri = gl->glPathParameteriNVFn;
  functions->fPathParameterf = gl->glPathParameterfNVFn;
  functions->fGenPaths = gl->glGenPathsNVFn;
  functions->fDeletePaths = gl->glDeletePathsNVFn;
  functions->fIsPath = gl->glIsPathNVFn;
  functions->fPathStencilFunc = gl->glPathStencilFuncNVFn;
  functions->fStencilFillPath = gl->glStencilFillPathNVFn;
  functions->fStencilStrokePath = gl->glStencilStrokePathNVFn;
  functions->fStencilFillPathInstanced = gl->glStencilFillPathInstancedNVFn;
  functions->fStencilStrokePathInstanced = gl->glStencilStrokePathInstancedNVFn;
  functions->fCoverFillPath = gl->glCoverFillPathNVFn;
  functions->fCoverStrokePath = gl->glCoverStrokePathNVFn;
  functions->fCoverFillPathInstanced = gl->glCoverFillPathInstancedNVFn;
  functions->fCoverStrokePathInstanced = gl->glCoverStrokePathInstancedNVFn;
  functions->fStencilThenCoverFillPath = gl->glStencilThenCoverFillPathNVFn;
  functions->fStencilThenCoverStrokePath = gl->glStencilThenCoverStrokePathNVFn;
  functions->fStencilThenCoverFillPathInstanced =
      gl->glStencilThenCoverFillPathInstancedNVFn;
  functions->fStencilThenCoverStrokePathInstanced =
      gl->glStencilThenCoverStrokePathInstancedNVFn;
  functions->fProgramPathFragmentInputGen =
      gl->glProgramPathFragmentInputGenNVFn;
  functions->fBindFragmentInputLocation =
      gl->glBindFragmentInputLocationCHROMIUMFn;

  functions->fCoverageModulation = gl->glCoverageModulationNVFn;

  functions->fInsertEventMarker = gl->glInsertEventMarkerEXTFn;
  functions->fPushGroupMarker = gl->glPushGroupMarkerEXTFn;
  functions->fPopGroupMarker = gl->glPopGroupMarkerEXTFn;

  // GL 4.3 or GL_ARB_invalidate_subdata
  // functions->fInvalidateBufferData = gl->glInvalidateBufferDataFn;
  // functions->fInvalidateBufferSubData = gl->glInvalidateBufferSubDataFn;
  // functions->fInvalidateTexImage = gl->glInvalidateTexImageFn;
  // functions->fInvalidateTexSubImage = gl->glInvalidateTexSubImageFn;

  functions->fInvalidateFramebuffer = gl->glInvalidateFramebufferFn;
  functions->fInvalidateSubFramebuffer = gl->glInvalidateSubFramebufferFn;

  functions->fGetProgramResourceLocation = gl->glGetProgramResourceLocationFn;

  // GL_NV_bindless_texture
  // functions->fGetTextureHandle = gl->glGetTextureHandleNVFn;
  // functions->fGetTextureSamplerHandle = gl->glGetTextureSamplerHandleNVFn;
  // functions->fMakeTextureHandleResident =
  //     gl->glMakeTextureHandleResidentNVFn;
  // functions->fMakeTextureHandleNonResident =
  //     gl->glMakeTextureHandleNonResidentNVFn;
  // functions->fGetImageHandle = gl->glGetImageHandleNVFn;
  // functions->fMakeImageHandleResident = gl->glMakeImageHandleResidentNVFn;
  // functions->fMakeImageHandleNonResident =
  //     gl->glMakeImageHandleNonResidentNVFn;
  // functions->fIsTextureHandleResident = gl->glIsTextureHandleResidentNVFn;
  // functions->fIsImageHandleResident = gl->glIsImageHandleResidentNVFn;
  // functions->fUniformHandleui64 = gl->glUniformHandleui64NVFn;
  // functions->fUniformHandleui64v = gl->glUniformHandleui64vNVFn;
  // functions->fProgramUniformHandleui64 = gl->glProgramUniformHandleui64NVFn;
  // functions->fProgramUniformHandleui64v =
  //     gl->glProgramUniformHandleui64vNVFn;

  // GL_EXT_direct_state_access
  // functions->fTextureParameteri = gl->glTextureParameteriEXTFn;
  // functions->fTextureParameteriv = gl->glTextureParameterivEXTFn;
  // functions->fTextureParameterf = gl->glTextureParameterfEXTFn;
  // functions->fTextureParameterfv = gl->glTextureParameterfvEXTFn;
  // functions->fTextureImage1D = gl->glTextureImage1DEXTFn;
  // functions->fTextureImage2D = gl->glTextureImage2DEXTFn;
  // functions->fTextureSubImage1D = gl->glTextureSubImage1DEXTFn;
  // functions->fTextureSubImage2D = gl->glTextureSubImage2DEXTFn;
  // functions->fCopyTextureImage1D = gl->glCopyTextureImage1DEXTFn;
  // functions->fCopyTextureImage2D = gl->glCopyTextureImage2DEXTFn;
  // functions->fCopyTextureSubImage1D = gl->glCopyTextureSubImage1DEXTFn;
  // functions->fCopyTextureSubImage2D = gl->glCopyTextureSubImage2DEXTFn;
  // functions->fGetNamedBufferParameteriv =
  //     gl->glGetNamedBufferParameterivEXTFn;
  // functions->fGetNamedBufferPointerv = gl->glGetNamedBufferPointervEXTFn;
  // functions->fGetNamedBufferSubData = gl->glGetNamedBufferSubDataEXTFn;
  // functions->fGetTextureImage = gl->glGetTextureImageEXTFn;
  // functions->fGetTextureParameterfv = gl->glGetTextureParameterfvEXTFn;
  // functions->fGetTextureParameteriv = gl->glGetTextureParameterivEXTFn;
  // functions->fGetTextureLevelParameterfv =
  //     gl->glGetTextureLevelParameterfvEXTFn;
  // functions->fGetTextureLevelParameteriv =
  //     gl->glGetTextureLevelParameterivEXTFn;
  // functions->fMapNamedBuffer = gl->glMapNamedBufferEXTFn;
  // functions->fNamedBufferData = gl->glNamedBufferDataEXTFn;
  // functions->fNamedBufferSubData = gl->glNamedBufferSubDataEXTFn;
  // functions->fProgramUniform1f = gl->glProgramUniform1fEXTFn;
  // functions->fProgramUniform2f = gl->glProgramUniform2fEXTFn;
  // functions->fProgramUniform3f = gl->glProgramUniform3fEXTFn;
  // functions->fProgramUniform4f = gl->glProgramUniform4fEXTFn;
  // functions->fProgramUniform1i = gl->glProgramUniform1iEXTFn;
  // functions->fProgramUniform2i = gl->glProgramUniform2iEXTFn;
  // functions->fProgramUniform3i = gl->glProgramUniform3iEXTFn;
  // functions->fProgramUniform4i = gl->glProgramUniform4iEXTFn;
  // functions->fProgramUniform1fv = gl->glProgramUniform1fvEXTFn;
  // functions->fProgramUniform2fv = gl->glProgramUniform2fvEXTFn;
  // functions->fProgramUniform3fv = gl->glProgramUniform3fvEXTFn;
  // functions->fProgramUniform4fv = gl->glProgramUniform4fvEXTFn;
  // functions->fProgramUniform1iv = gl->glProgramUniform1ivEXTFn;
  // functions->fProgramUniform2iv = gl->glProgramUniform2ivEXTFn;
  // functions->fProgramUniform3iv = gl->glProgramUniform3ivEXTFn;
  // functions->fProgramUniform4iv = gl->glProgramUniform4ivEXTFn;
  // functions->fProgramUniformMatrix2fv = gl->glProgramUniformMatrix2fvEXTFn;
  // functions->fProgramUniformMatrix3fv = gl->glProgramUniformMatrix3fvEXTFn;
  // functions->fProgramUniformMatrix4fv = gl->glProgramUniformMatrix4fvEXTFn;
  // functions->fUnmapNamedBuffer = gl->glUnmapNamedBufferEXTFn;
  // functions->fTextureImage3D = gl->glTextureImage3DEXTFn;
  // functions->fTextureSubImage3D = gl->glTextureSubImage3DEXTFn;
  // functions->fCopyTextureSubImage3D = gl->glCopyTextureSubImage3DEXTFn;
  // functions->fCompressedTextureImage3D = gl->glCompressedTextureImage3DEXTFn;
  // functions->fCompressedTextureImage2D = gl->glCompressedTextureImage2DEXTFn;
  // functions->fCompressedTextureImage1D = gl->glCompressedTextureImage1DEXTFn;
  // functions->fCompressedTextureSubImage3D =
  //     gl->glCompressedTextureSubImage3DEXTFn;
  // functions->fCompressedTextureSubImage2D =
  //     gl->glCompressedTextureSubImage2DEXTFn;
  // functions->fCompressedTextureSubImage1D =
  //     gl->glCompressedTextureSubImage1DEXTFn;
  // functions->fGetCompressedTextureImage =
  //     gl->glGetCompressedTextureImageEXTFn;
  // functions->fProgramUniformMatrix2x3fv =
  //     gl->glProgramUniformMatrix2x3fvEXTFn;
  // functions->fProgramUniformMatrix3x2fv =
  //     gl->glProgramUniformMatrix3x2fvEXTFn;
  // functions->fProgramUniformMatrix2x4fv =
  //     gl->glProgramUniformMatrix2x4fvEXTFn;
  // functions->fProgramUniformMatrix4x2fv =
  //     gl->glProgramUniformMatrix4x2fvEXTFn;
  // functions->fProgramUniformMatrix3x4fv =
  //     gl->glProgramUniformMatrix3x4fvEXTFn;
  // functions->fProgramUniformMatrix4x3fv =
  //     gl->glProgramUniformMatrix4x3fvEXTFn;
  // functions->fNamedRenderbufferStorage = gl->glNamedRenderbufferStorageEXTFn;
  // functions->fGetNamedRenderbufferParameteriv =
  //     gl->glGetNamedRenderbufferParameterivEXTFn;
  // functions->fNamedRenderbufferStorageMultisample =
  //     gl->glNamedRenderbufferStorageMultisampleEXTFn;
  // functions->fCheckNamedFramebufferStatus =
  //     gl->glCheckNamedFramebufferStatusEXTFn;
  // functions->fNamedFramebufferTexture1D =
  //     gl->glNamedFramebufferTexture1DEXTFn;
  // functions->fNamedFramebufferTexture2D =
  //     gl->glNamedFramebufferTexture2DEXTFn;
  // functions->fNamedFramebufferTexture3D =
  //     gl->glNamedFramebufferTexture3DEXTFn;
  // functions->fNamedFramebufferRenderbuffer =
  //     gl->glNamedFramebufferRenderbufferEXTFn;
  // functions->fGetNamedFramebufferAttachmentParameteriv =
  //     gl->glGetNamedFramebufferAttachmentParameterivEXTFn;
  // functions->fGenerateTextureMipmap = gl->glGenerateTextureMipmapEXTFn;
  // functions->fFramebufferDrawBuffer = gl->glFramebufferDrawBufferEXTFn;
  // functions->fFramebufferDrawBuffers = gl->glFramebufferDrawBuffersEXTFn;
  // functions->fFramebufferReadBuffer = gl->glFramebufferReadBufferEXTFn;
  // functions->fGetFramebufferParameteriv =
  //     gl->glGetFramebufferParameterivEXTFn;
  // functions->fNamedCopyBufferSubData = gl->glNamedCopyBufferSubDataEXTFn;
  // functions->fVertexArrayVertexOffset = gl->glVertexArrayVertexOffsetEXTFn;
  // functions->fVertexArrayColorOffset = gl->glVertexArrayColorOffsetEXTFn;
  // functions->fVertexArrayEdgeFlagOffset =
  //     gl->glVertexArrayEdgeFlagOffsetEXTFn;
  // functions->fVertexArrayIndexOffset = gl->glVertexArrayIndexOffsetEXTFn;
  // functions->fVertexArrayNormalOffset = gl->glVertexArrayNormalOffsetEXTFn;
  // functions->fVertexArrayTexCoordOffset =
  //     gl->glVertexArrayTexCoordOffsetEXTFn;
  // functions->fVertexArrayMultiTexCoordOffset =
  //     gl->glVertexArrayMultiTexCoordOffsetEXTFn;
  // functions->fVertexArrayFogCoordOffset =
  //     gl->glVertexArrayFogCoordOffsetEXTFn;
  // functions->fVertexArraySecondaryColorOffset =
  //     gl->glVertexArraySecondaryColorOffsetEXTFn;
  // functions->fVertexArrayVertexAttribOffset =
  //     gl->glVertexArrayVertexAttribOffsetEXTFn;
  // functions->fVertexArrayVertexAttribIOffset =
  //     gl->glVertexArrayVertexAttribIOffsetEXTFn;
  // functions->fEnableVertexArray = gl->glEnableVertexArrayEXTFn;
  // functions->fDisableVertexArray = gl->glDisableVertexArrayEXTFn;
  // functions->fEnableVertexArrayAttrib = gl->glEnableVertexArrayAttribEXTFn;
  // functions->fDisableVertexArrayAttrib = gl->glDisableVertexArrayAttribEXTFn;
  // functions->fGetVertexArrayIntegerv = gl->glGetVertexArrayIntegervEXTFn;
  // functions->fGetVertexArrayPointerv = gl->glGetVertexArrayPointervEXTFn;
  // functions->fGetVertexArrayIntegeri_v = gl->glGetVertexArrayIntegeri_vEXTFn;
  // functions->fGetVertexArrayPointeri_v = gl->glGetVertexArrayPointeri_vEXTFn;
  // functions->fMapNamedBufferRange = gl->glMapNamedBufferRangeEXTFn;
  // functions->fFlushMappedNamedBufferRange =
  //     gl->glFlushMappedNamedBufferRangeEXTFn;
  // functions->fTextureBuffer = gl->glTextureBufferEXTFn;

  // Some drivers report GL_KHR_debug but do not provide functions. Validate and
  // remove reported extension from the list if necessary
  // See https://crbug.com/1008125
  if (gl->glDebugMessageControlFn && gl->glDebugMessageInsertFn &&
      gl->glDebugMessageCallbackFn && gl->glGetDebugMessageLogFn &&
      gl->glPushDebugGroupFn && gl->glPopDebugGroupFn && gl->glObjectLabelFn) {
    functions->fDebugMessageControl = gl->glDebugMessageControlFn;
    functions->fDebugMessageInsert = gl->glDebugMessageInsertFn;
    functions->fDebugMessageCallback = gl->glDebugMessageCallbackFn;
    functions->fGetDebugMessageLog = gl->glGetDebugMessageLogFn;
    functions->fPushDebugGroup = gl->glPushDebugGroupFn;
    functions->fPopDebugGroup = gl->glPopDebugGroupFn;
    functions->fObjectLabel = gl->glObjectLabelFn;
  } else {
    extensions.remove("GL_KHR_debug");
  }

  // GL_EXT_window_rectangles
  functions->fWindowRectangles = gl->glWindowRectanglesEXTFn;

  // GL_QCOM_tiled_rendering
  functions->fStartTiling = gl->glStartTilingQCOMFn;
  functions->fEndTiling = gl->glEndTilingQCOMFn;

  // EGL_KHR_image / EGL_KHR_image_base
  // functions->fCreateImage = nullptr;
  // functions->fDestroyImage = nullptr;

  functions->fFenceSync = gl->glFenceSyncFn;
  functions->fIsSync = gl->glIsSyncFn;
  functions->fClientWaitSync = gl->glClientWaitSyncFn;
  functions->fWaitSync = gl->glWaitSyncFn;
  functions->fDeleteSync = gl->glDeleteSyncFn;

  if (!gl->glFenceSyncFn) {
    // NOTE: Skia uses the same function pointers without APPLE suffix
    if (extensions.has("GL_APPLE_sync")) {
      functions->fFenceSync = gl->glFenceSyncAPPLEFn;
      functions->fIsSync = gl->glIsSyncAPPLEFn;
      functions->fClientWaitSync = gl->glClientWaitSyncAPPLEFn;
      functions->fWaitSync = gl->glWaitSyncAPPLEFn;
      functions->fDeleteSync = gl->glDeleteSyncAPPLEFn;
    } else if (g_driver_egl.ext.b_EGL_KHR_fence_sync) {
      // Emulate APPLE_sync via egl
      extensions.add("GL_APPLE_sync");

      functions->fFenceSync = glFenceSyncEmulateEGL;
      functions->fIsSync = glIsSyncEmulateEGL;
      functions->fClientWaitSync = glClientWaitSyncEmulateEGL;
      functions->fWaitSync = glWaitSyncEmulateEGL;
      functions->fDeleteSync = glDeleteSyncEmulateEGL;
    }
  } else if (use_version_es2) {
    // We have gl sync, but want to Skia use ES2 that doesn't have fences.
    // To provide Skia with ways of sync to prevent it calling glFinish we set
    // GL_APPLE_sync support.
    extensions.add("GL_APPLE_sync");
  }

  // Skia can fall back to GL_NV_fence if GLsync objects are not available.
  functions->fDeleteFences = gl->glDeleteFencesNVFn;
  functions->fFinishFence = gl->glFinishFenceNVFn;
  functions->fGenFences = gl->glGenFencesNVFn;
  functions->fSetFence = gl->glSetFenceNVFn;
  functions->fTestFence = gl->glTestFenceNVFn;

  functions->fGetInternalformativ = gl->glGetInternalformativFn;

  interface->fStandard = standard;
  interface->fExtensions.swap(&extensions);
  sk_sp<GrGLInterface> returned(interface);
  return returned;
}

}  // namespace init
}  // namespace gl
