// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/create_gr_gl_interface.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "base/traits_bag.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace gl::init {

// This code emulates GL fences (GL_APPLE_sync or GL_ARB_sync) via
// EGL_KHR_fence_sync extension. It's used to provide Skia ways of
// synchronization on platforms that does not have GL fences but support EGL
namespace {

// If enabled, adds a delay to GL program link whose value is given by the
// feature param. Used for an ablation study.
BASE_FEATURE(kAddDelayToGLProgramLink,
             "AddDelayToGLProgramLink",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kGLProgramLinkDelayMicroseconds{
    &kAddDelayToGLProgramLink, /*name=*/"GLProgramLinkDelayMicroseconds",
    /*default_value=*/1000};

struct EGLFenceData {
  EGLSync sync;
  EGLDisplay display;
};

GLsync glFenceSyncEmulateEGL(GLenum condition, GLbitfield flags) {
  DCHECK(condition == GL_SYNC_GPU_COMMANDS_COMPLETE);
  DCHECK(flags == 0);

  // Prefer EGL_ANGLE_global_fence_sync as it guarantees synchronization with
  // past submissions from all contexts, rather than the current context.
  gl::GLContext* context = gl::GLContext::GetCurrent();
  gl::GLDisplayEGL* display = context ? context->GetGLDisplayEGL() : nullptr;
  const EGLenum syncType =
      display && display->ext->b_EGL_ANGLE_global_fence_sync
          ? EGL_SYNC_GLOBAL_FENCE_ANGLE
          : EGL_SYNC_FENCE_KHR;

  init::EGLFenceData* data = new EGLFenceData;

  data->display = eglGetCurrentDisplay();
  data->sync = eglCreateSyncKHR(data->display, syncType, nullptr);

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

  NOTREACHED_IN_MIGRATION();
  return 0;
}

void glWaitSyncEmulateEGL(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  init::EGLFenceData* data = reinterpret_cast<init::EGLFenceData*>(sync);

  DCHECK(timeout == GL_TIMEOUT_IGNORED);
  DCHECK(flags == 0);

  if (!GetDefaultDisplayEGL()->ext->b_EGL_KHR_wait_sync) {
    eglClientWaitSyncKHR(data->display, data->sync, 0, EGL_FOREVER_KHR);
    return;
  }

  EGLint result = eglWaitSyncKHR(data->display, data->sync, 0);
  DCHECK(result);
}

GLboolean glIsSyncEmulateEGL(GLsync sync) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

}  // namespace

namespace {

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_with_api(
    R (gl::GLApi::*func)(Args...),
    gl::GLApi* api) {
  return [func, api](Args... args) { return (api->*func)(args...); };
}

struct FlushHelper {
  FlushHelper() {
    TRACE_EVENT0("gpu",
                 "CreateGrGLInterface - bind_with_flush_on_mac - beforefunc");
    glFlush();
  }
  ~FlushHelper() {
    TRACE_EVENT0("gpu",
                 "CreateGrGLInterface - bind_with_flush_on_mac - afterfunc");
    glFlush();
  }
};

template <bool droppable,
          bool slow,
          bool need_flush,
          typename R,
          typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_impl(
    R(GL_BINDING_CALL* func)(Args...),
    gl::ProgressReporter* progress_reporter) {
  // Don't wrap missing functions.
  if (!func)
    return nullptr;

  constexpr bool need_wrap = droppable || slow || need_flush;
  if constexpr (need_wrap) {
    return [func, progress_reporter](Args... args) -> R {
      if constexpr (droppable) {
        if (HasInitializedNullDrawGLBindings())
          return R();
      }

      std::optional<gl::ScopedProgressReporter> scoped_reporter;
      // Not using constexpr if here to avoid unused progress_reporter warning.
      if (slow && progress_reporter)
        scoped_reporter.emplace(progress_reporter);

      std::optional<FlushHelper> flush_helper;
      if constexpr (need_flush)
        flush_helper.emplace();
      return func(args...);
    };
  } else {
    return func;
  }
}

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(GLuint, Args...)>
bind_timed_compile_function(R(GL_BINDING_CALL* func)(GLuint shader, Args...),
                            glGetShaderivProc get_shader_iv,
                            gl::ProgressReporter* progress_reporter) {
  // Don't wrap missing functions.
  if (!func) {
    return nullptr;
  }

  return [func, get_shader_iv, progress_reporter](GLuint shader,
                                                  Args... args) -> R {
    gl::ScopedProgressReporter scoped_reporter(progress_reporter);
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Gpu.GrCompileShaderUs");

    func(shader, args...);

    GLint compile_result = 0;
    get_shader_iv(shader, GL_COMPILE_STATUS, &compile_result);
  };
}

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(GLuint, Args...)> bind_timed_link_function(
    R(GL_BINDING_CALL* func)(GLuint program, Args...),
    glGetProgramivProc get_program_iv,
    gl::ProgressReporter* progress_reporter) {
  // Don't wrap missing functions.
  if (!func) {
    return nullptr;
  }

  return [func, get_program_iv, progress_reporter](GLuint program,
                                                   Args... args) -> R {
    gl::ScopedProgressReporter scoped_reporter(progress_reporter);
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Gpu.GrLinkProgramUs");

    if (base::FeatureList::IsEnabled(kAddDelayToGLProgramLink)) {
      base::PlatformThread::Sleep(
          base::Microseconds(kGLProgramLinkDelayMicroseconds.Get()));
    }

    func(program, args...);

    GLint compile_result = 0;
    get_program_iv(program, GL_LINK_STATUS, &compile_result);
  };
}

// Call can be dropped for tests that setup null draw gl bindings.
struct Droppable {};
// Call needs to be wrapped with ProgressReporter.
struct Slow {};
// Call needs to be wrapped with glFlush call, used on MacOS.
struct NeedFlush {};

#if BUILDFLAG(IS_MAC)
using SlowOnMac = Slow;
using NeedFlushOnMac = NeedFlush;
#else
using SlowOnMac = void;
using NeedFlushOnMac = void;
#endif

template <typename... Traits>
struct BindWithTraits {
  template <typename R, typename... Args>
  static GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind(
      R(GL_BINDING_CALL* func)(Args...),
      gl::ProgressReporter* progress_reporter,
      bool is_angle) {
    constexpr bool droppable =
        base::trait_helpers::HasTrait<Droppable, Traits...>();
    constexpr bool slow = base::trait_helpers::HasTrait<Slow, Traits...>();
    constexpr bool need_flush =
        base::trait_helpers::HasTrait<NeedFlush, Traits...>();

#if BUILDFLAG(IS_MAC)
    // If running on Apple silicon, regardless of the architecture, don't
    // perform this workaround. See https://crbug.com/1131312.
    if (need_flush && base::mac::GetCPUType() == base::mac::CPUType::kIntel &&
        !is_angle) {
      return bind_impl<droppable, slow, /*need_flush=*/true>(func,
                                                             progress_reporter);
    } else {
      return bind_impl<droppable, slow, /*need_flush=*/false>(
          func, progress_reporter);
    }
#else
    return bind_impl<droppable, slow, need_flush>(func, progress_reporter);
#endif
  }
};

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
    "GL_CHROMIUM_framebuffer_mixed_samples",
    "GL_EXT_direct_state_access",
    "GL_EXT_multi_draw_indirect",
    "GL_EXT_raster_multisample",
    "GL_NV_bindless_texture",
    "GL_NV_framebuffer_mixed_samples",
    "GL_NV_texture_barrier",
    "GL_OES_sample_shading",
    "GL_EXT_draw_instanced",
};

}  // anonymous namespace

sk_sp<GrGLInterface> CreateGrGLInterface(
    const gl::GLVersionInfo& version_info,
    gl::ProgressReporter* progress_reporter) {
  gl::ProcsGL* gl = &gl::g_current_gl_driver->fn;
  gl::GLApi* api = gl::g_current_gl_context;

  GrGLStandard standard = kGLES_GrGLStandard;

  // Depending on the advertised version and extensions, skia checks for
  // existence of entrypoints. However some of those we don't yet handle in
  // gl_bindings, so we need to fake the version to the maximum fully supported
  // by the bindings (ES 3.0), and blocklist extensions that skia handles but
  // bindings don't.
  // TODO(piman): add bindings for missing entrypoints.
  GrGLFunction<GrGLGetStringFn> get_string;
  const bool apply_version_override = version_info.IsAtLeastGLES(3, 1);

  if (apply_version_override) {
    GLVersionInfo::VersionStrings version;
    version = version_info.GetFakeVersionStrings(3, 0);

    get_string = [version](GLenum name) {
      return GetStringHook(version.gl_version, version.glsl_version, name);
    };
  } else {
    get_string = bind_with_api(&gl::GLApi::glGetStringFn, api);
  }

  auto get_stringi = bind_with_api(&gl::GLApi::glGetStringiFn, api);
  auto get_integerv = bind_with_api(&gl::GLApi::glGetIntegervFn, api);

  auto timed_compile_shader = bind_timed_compile_function(
      gl->glCompileShaderFn, gl->glGetShaderivFn, progress_reporter);

  auto timed_link_program = bind_timed_link_function(
      gl->glLinkProgramFn, gl->glGetProgramivFn, progress_reporter);

  GrGLExtensions extensions;
  if (!extensions.init(standard, get_string, get_stringi, get_integerv)) {
    LOG(ERROR) << "Failed to initialize extensions";
    return nullptr;
  }
  for (const char* extension : kBlocklistExtensions)
    extensions.remove(extension);

#define BIND_EXTENSION(skia_name, chrome_name, ...)            \
  functions->f##skia_name = BindWithTraits<__VA_ARGS__>::bind( \
      gl->gl##chrome_name##Fn, progress_reporter, version_info.is_angle)
#define BIND(fname, ...) BIND_EXTENSION(fname, fname, __VA_ARGS__)

  GrGLInterface* gl_interface = new GrGLInterface();
  GrGLInterface::Functions* functions = &gl_interface->fFunctions;
  BIND(ActiveTexture);
  BIND(AttachShader);
  BIND(BindAttribLocation);
  BIND(BindBuffer);
  BIND(BindFragDataLocation);
  BIND_EXTENSION(BindUniformLocation, BindUniformLocationCHROMIUM);
  BIND(BeginQuery);
  BIND(BindSampler);
  BIND(BindTexture, SlowOnMac);
  BIND_EXTENSION(BlendBarrier, BlendBarrierKHR);
  BIND(BlendColor);
  BIND(BlendEquation);
  BIND(BlendFunc);
  BIND(BufferData);
  BIND(BufferSubData);
  BIND(Clear, Droppable, SlowOnMac, NeedFlushOnMac);
  BIND(ClearColor);
  BIND(ClearStencil);
  BIND(ClearTexImage);
  BIND(ClearTexSubImage);
  BIND(ColorMask);
  functions->fCompileShader = timed_compile_shader;
  BIND(CompressedTexImage2D, Slow, NeedFlushOnMac);
  BIND(CompressedTexSubImage2D, Slow);
  BIND(CopyBufferSubData);
  BIND(CopyTexSubImage2D, Slow);
  BIND(CreateProgram);
  BIND(CreateShader);
  BIND(CullFace);
  BIND_EXTENSION(DeleteBuffers, DeleteBuffersARB, Slow);
  BIND(DeleteProgram, Slow);
  BIND(DeleteQueries);
  BIND(DeleteSamplers);
  BIND(DeleteShader, Slow);
  BIND(DeleteTextures, Slow, NeedFlushOnMac);
  BIND(DepthMask);
  BIND(Disable);
  BIND(DisableVertexAttribArray);
  BIND_EXTENSION(DiscardFramebuffer, DiscardFramebufferEXT);
  BIND(DrawArrays, Droppable, SlowOnMac);
  BIND(DrawBuffer);
  BIND_EXTENSION(DrawBuffers, DrawBuffersARB);
  BIND(DrawElements, Droppable, SlowOnMac);
  BIND_EXTENSION(DrawArraysInstanced, DrawArraysInstancedANGLE, Droppable,
                 SlowOnMac);
  BIND_EXTENSION(DrawArraysInstancedBaseInstance,
                 DrawArraysInstancedBaseInstanceANGLE, Droppable, SlowOnMac);
  BIND_EXTENSION(MultiDrawArraysInstancedBaseInstance,
                 MultiDrawArraysInstancedBaseInstanceANGLE, Droppable,
                 SlowOnMac);
  BIND_EXTENSION(DrawElementsInstanced, DrawElementsInstancedANGLE, Droppable,
                 SlowOnMac);
  BIND_EXTENSION(DrawElementsInstancedBaseVertexBaseInstance,
                 DrawElementsInstancedBaseVertexBaseInstanceANGLE, Droppable,
                 SlowOnMac);
  BIND_EXTENSION(MultiDrawElementsInstancedBaseVertexBaseInstance,
                 MultiDrawElementsInstancedBaseVertexBaseInstanceANGLE,
                 Droppable, SlowOnMac);

  // GL 4.0 or GL_ARB_draw_indirect or ES 3.1
  BIND(DrawArraysIndirect, Droppable, SlowOnMac);
  BIND(DrawElementsIndirect, Droppable, SlowOnMac);

  BIND(DrawRangeElements, Droppable, SlowOnMac);
  BIND(Enable);
  BIND(EnableVertexAttribArray);
  BIND(EndQuery);
  BIND(Finish, Slow);
  BIND(Flush, Slow);
  BIND(FrontFace);
  BIND_EXTENSION(GenBuffers, GenBuffersARB);
  BIND(GetBufferParameteriv);
  BIND(GetError);
  BIND(GetFloatv);
  BIND(GetIntegerv);
  BIND(GetMultisamplefv);
  BIND(GetQueryObjectiv);
  BIND(GetQueryObjectuiv);
  BIND(GetQueryObjecti64v);
  BIND(GetQueryObjectui64v);
  BIND(QueryCounter);
  BIND(GetQueryiv);
  BIND(GetProgramBinary);
  BIND(GetProgramInfoLog);
  BIND(GetProgramiv);
  BIND(GetShaderInfoLog);
  BIND(GetShaderiv);
  functions->fGetString = get_string;
  BIND(GetStringi);
  BIND(GetShaderPrecisionFormat);
  BIND(GetTexLevelParameteriv);
  BIND(GenQueries);
  BIND(GenSamplers);
  BIND(GenTextures);
  BIND(GetUniformLocation);
  BIND(IsTexture);
  BIND(LineWidth);
  functions->fLinkProgram = timed_link_program;
  BIND(MapBuffer);

  // GL 4.3 or GL_ARB_multi_draw_indirect or ES+GL_EXT_multi_draw_indirect
  // BIND(MultiDrawArraysIndirect);
  // BIND(MultiDrawElementsIndirect);

  BIND(PatchParameteri);
  BIND(PixelStorei);
  BIND(PolygonMode);

  // TODO(vasilyt): Figure out why BIND(fProgramBinary) doesn't fit in
  // GrFunction
  functions->fProgramBinary = gl->glProgramBinaryFn;

  BIND(ProgramParameteri);

  // GL_EXT_raster_multisample
  // BIND_EXTENSION(RasterSamples , RasterSamplesEXT);

  BIND(ReadBuffer);
  BIND(ReadPixels);
  BIND(SamplerParameterf);
  BIND(SamplerParameteri);
  BIND(SamplerParameteriv);
  BIND(Scissor);
  BIND(ShaderSource);
  BIND(StencilFunc);
  BIND(StencilFuncSeparate);
  BIND(StencilMask);
  BIND(StencilMaskSeparate);
  BIND(StencilOp);
  BIND(StencilOpSeparate);
  BIND(TexBuffer);
  BIND(TexBufferRange);
  BIND(TexImage2D, Slow, NeedFlushOnMac);
  BIND(TexParameterf);
  BIND(TexParameterfv);
  BIND(TexParameteri);
  BIND(TexParameteriv);
  BIND_EXTENSION(TexStorage2D, TexStorage2DEXT, Slow, NeedFlushOnMac);
  BIND(TexSubImage2D, Slow, NeedFlushOnMac);

  // GL 4.5 or GL_ARB_texture_barrier or GL_NV_texture_barrier
  // BIND(TextureBarrier);
  // BIND_EXTENSION(TextureBarrier , TextureBarrierNV);

  BIND(Uniform1f);
  BIND(Uniform1i);
  BIND(Uniform1fv);
  BIND(Uniform1iv);
  BIND(Uniform2f);
  BIND(Uniform2i);
  BIND(Uniform2fv);
  BIND(Uniform2iv);
  BIND(Uniform3f);
  BIND(Uniform3i);
  BIND(Uniform3fv);
  BIND(Uniform3iv);
  BIND(Uniform4f);
  BIND(Uniform4i);
  BIND(Uniform4fv);
  BIND(Uniform4iv);
  BIND(UniformMatrix2fv);
  BIND(UniformMatrix3fv);
  BIND(UniformMatrix4fv);
  BIND(UnmapBuffer);
  BIND(UseProgram);
  BIND(VertexAttrib1f);
  BIND(VertexAttrib2fv);
  BIND(VertexAttrib3fv);
  BIND(VertexAttrib4fv);

  BIND_EXTENSION(VertexAttribDivisor, VertexAttribDivisorANGLE);

  BIND(VertexAttribIPointer);

  BIND(VertexAttribPointer);
  BIND(Viewport);
  BIND(BindFragDataLocationIndexed);

  BIND_EXTENSION(BindVertexArray, BindVertexArrayOES);
  BIND_EXTENSION(GenVertexArrays, GenVertexArraysOES);
  BIND_EXTENSION(DeleteVertexArrays, DeleteVertexArraysOES);

  BIND(MapBufferRange);
  BIND(FlushMappedBufferRange);

  BIND_EXTENSION(GenerateMipmap, GenerateMipmapEXT);
  BIND_EXTENSION(GenFramebuffers, GenFramebuffersEXT);
  BIND_EXTENSION(GetFramebufferAttachmentParameteriv,
                 GetFramebufferAttachmentParameterivEXT);
  BIND_EXTENSION(GetRenderbufferParameteriv, GetRenderbufferParameterivEXT);
  BIND_EXTENSION(BindFramebuffer, BindFramebufferEXT, Slow, NeedFlushOnMac);
  BIND_EXTENSION(FramebufferTexture2D, FramebufferTexture2DEXT);
  BIND_EXTENSION(CheckFramebufferStatus, CheckFramebufferStatusEXT);
  BIND_EXTENSION(DeleteFramebuffers, DeleteFramebuffersEXT, Slow,
                 NeedFlushOnMac);
  BIND_EXTENSION(RenderbufferStorage, RenderbufferStorageEXT, NeedFlushOnMac);
  BIND_EXTENSION(GenRenderbuffers, GenRenderbuffersEXT);
  BIND_EXTENSION(DeleteRenderbuffers, DeleteRenderbuffersEXT, NeedFlushOnMac);
  BIND_EXTENSION(FramebufferRenderbuffer, FramebufferRenderbufferEXT);
  BIND_EXTENSION(BindRenderbuffer, BindRenderbufferEXT);

  BIND(RenderbufferStorageMultisample, NeedFlushOnMac);
  BIND_EXTENSION(FramebufferTexture2DMultisample,
                 FramebufferTexture2DMultisampleEXT);
  BIND_EXTENSION(RenderbufferStorageMultisampleES2EXT,
                 RenderbufferStorageMultisampleEXT, NeedFlushOnMac);
  BIND(BlitFramebuffer, NeedFlushOnMac);

  BIND_EXTENSION(InsertEventMarker, InsertEventMarkerEXT);
  BIND_EXTENSION(PushGroupMarker, PushGroupMarkerEXT);
  BIND_EXTENSION(PopGroupMarker, PopGroupMarkerEXT);

  // GL 4.3 or GL_ARB_invalidate_subdata
  // BIND(InvalidateBufferData);
  // BIND(InvalidateBufferSubData);
  // BIND(InvalidateTexImage);
  // BIND(InvalidateTexSubImage);

  BIND(InvalidateFramebuffer);
  BIND(InvalidateSubFramebuffer);

  // GL_NV_bindless_texture
  // BIND_EXTENSION(GetTextureHandle , GetTextureHandleNV);
  // BIND_EXTENSION(GetTextureSamplerHandle , GetTextureSamplerHandleNV);
  // BIND_EXTENSION(MakeTextureHandleResident , MakeTextureHandleResidentNV);
  // BIND_EXTENSION(MakeTextureHandleNonResident ,
  // MakeTextureHandleNonResidentNV); BIND_EXTENSION(GetImageHandle ,
  // GetImageHandleNV); BIND_EXTENSION(MakeImageHandleResident ,
  // MakeImageHandleResidentNV); BIND_EXTENSION(MakeImageHandleNonResident ,
  // MakeImageHandleNonResidentNV); BIND_EXTENSION(IsTextureHandleResident ,
  // IsTextureHandleResidentNV); BIND_EXTENSION(IsImageHandleResident ,
  // IsImageHandleResidentNV); BIND_EXTENSION(UniformHandleui64 ,
  // UniformHandleui64NV); BIND_EXTENSION(UniformHandleui64v ,
  // UniformHandleui64vNV); BIND_EXTENSION(ProgramUniformHandleui64 ,
  // ProgramUniformHandleui64NV); BIND_EXTENSION(ProgramUniformHandleui64v ,
  // ProgramUniformHandleui64vNV);

  // GL_EXT_direct_state_access
  // BIND_EXTENSION(TextureParameteri , TextureParameteriEXT);
  // BIND_EXTENSION(TextureParameteriv , TextureParameterivEXT);
  // BIND_EXTENSION(TextureParameterf , TextureParameterfEXT);
  // BIND_EXTENSION(TextureParameterfv , TextureParameterfvEXT);
  // BIND_EXTENSION(TextureImage1D , TextureImage1DEXT);
  // BIND_EXTENSION(TextureImage2D , TextureImage2DEXT);
  // BIND_EXTENSION(TextureSubImage1D , TextureSubImage1DEXT);
  // BIND_EXTENSION(TextureSubImage2D , TextureSubImage2DEXT);
  // BIND_EXTENSION(CopyTextureImage1D , CopyTextureImage1DEXT);
  // BIND_EXTENSION(CopyTextureImage2D , CopyTextureImage2DEXT);
  // BIND_EXTENSION(CopyTextureSubImage1D , CopyTextureSubImage1DEXT);
  // BIND_EXTENSION(CopyTextureSubImage2D , CopyTextureSubImage2DEXT);
  // BIND_EXTENSION(GetNamedBufferParameteriv , GetNamedBufferParameterivEXT);
  // BIND_EXTENSION(GetNamedBufferPointerv , GetNamedBufferPointervEXT);
  // BIND_EXTENSION(GetNamedBufferSubData , GetNamedBufferSubDataEXT);
  // BIND_EXTENSION(GetTextureImage , GetTextureImageEXT);
  // BIND_EXTENSION(GetTextureParameterfv , GetTextureParameterfvEXT);
  // BIND_EXTENSION(GetTextureParameteriv , GetTextureParameterivEXT);
  // BIND_EXTENSION(GetTextureLevelParameterfv , GetTextureLevelParameterfvEXT);
  // BIND_EXTENSION(GetTextureLevelParameteriv , GetTextureLevelParameterivEXT);
  // BIND_EXTENSION(MapNamedBuffer , MapNamedBufferEXT);
  // BIND_EXTENSION(NamedBufferData , NamedBufferDataEXT);
  // BIND_EXTENSION(NamedBufferSubData , NamedBufferSubDataEXT);
  // BIND_EXTENSION(ProgramUniform1f , ProgramUniform1fEXT);
  // BIND_EXTENSION(ProgramUniform2f , ProgramUniform2fEXT);
  // BIND_EXTENSION(ProgramUniform3f , ProgramUniform3fEXT);
  // BIND_EXTENSION(ProgramUniform4f , ProgramUniform4fEXT);
  // BIND_EXTENSION(ProgramUniform1i , ProgramUniform1iEXT);
  // BIND_EXTENSION(ProgramUniform2i , ProgramUniform2iEXT);
  // BIND_EXTENSION(ProgramUniform3i , ProgramUniform3iEXT);
  // BIND_EXTENSION(ProgramUniform4i , ProgramUniform4iEXT);
  // BIND_EXTENSION(ProgramUniform1fv , ProgramUniform1fvEXT);
  // BIND_EXTENSION(ProgramUniform2fv , ProgramUniform2fvEXT);
  // BIND_EXTENSION(ProgramUniform3fv , ProgramUniform3fvEXT);
  // BIND_EXTENSION(ProgramUniform4fv , ProgramUniform4fvEXT);
  // BIND_EXTENSION(ProgramUniform1iv , ProgramUniform1ivEXT);
  // BIND_EXTENSION(ProgramUniform2iv , ProgramUniform2ivEXT);
  // BIND_EXTENSION(ProgramUniform3iv , ProgramUniform3ivEXT);
  // BIND_EXTENSION(ProgramUniform4iv , ProgramUniform4ivEXT);
  // BIND_EXTENSION(ProgramUniformMatrix2fv , ProgramUniformMatrix2fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix3fv , ProgramUniformMatrix3fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix4fv , ProgramUniformMatrix4fvEXT);
  // BIND_EXTENSION(UnmapNamedBuffer , UnmapNamedBufferEXT);
  // BIND_EXTENSION(TextureImage3D , TextureImage3DEXT);
  // BIND_EXTENSION(TextureSubImage3D , TextureSubImage3DEXT);
  // BIND_EXTENSION(CopyTextureSubImage3D , CopyTextureSubImage3DEXT);
  // BIND_EXTENSION(CompressedTextureImage3D , CompressedTextureImage3DEXT);
  // BIND_EXTENSION(CompressedTextureImage2D , CompressedTextureImage2DEXT);
  // BIND_EXTENSION(CompressedTextureImage1D , CompressedTextureImage1DEXT);
  // BIND_EXTENSION(CompressedTextureSubImage3D,
  //                CompressedTextureSubImage3DEXT);
  // BIND_EXTENSION(CompressedTextureSubImage2D,
  //                CompressedTextureSubImage2DEXT);
  // BIND_EXTENSION(CompressedTextureSubImage1D,
  //                CompressedTextureSubImage1DEXT);
  // BIND_EXTENSION(GetCompressedTextureImage, GetCompressedTextureImageEXT);
  // BIND_EXTENSION(ProgramUniformMatrix2x3fv, ProgramUniformMatrix2x3fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix3x2fv, ProgramUniformMatrix3x2fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix2x4fv, ProgramUniformMatrix2x4fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix4x2fv, ProgramUniformMatrix4x2fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix3x4fv, ProgramUniformMatrix3x4fvEXT);
  // BIND_EXTENSION(ProgramUniformMatrix4x3fv, ProgramUniformMatrix4x3fvEXT);
  // BIND_EXTENSION(NamedRenderbufferStorage, NamedRenderbufferStorageEXT);
  // BIND_EXTENSION(GetNamedRenderbufferParameteriv,
  //                GetNamedRenderbufferParameterivEXT);
  // BIND_EXTENSION(NamedRenderbufferStorageMultisample,
  //                NamedRenderbufferStorageMultisampleEXT);
  // BIND_EXTENSION(CheckNamedFramebufferStatus,
  //                CheckNamedFramebufferStatusEXT);
  // BIND_EXTENSION(NamedFramebufferTexture1D, NamedFramebufferTexture1DEXT);
  // BIND_EXTENSION(NamedFramebufferTexture2D, NamedFramebufferTexture2DEXT);
  // BIND_EXTENSION(NamedFramebufferTexture3D, NamedFramebufferTexture3DEXT);
  // BIND_EXTENSION(NamedFramebufferRenderbuffer,
  //                NamedFramebufferRenderbufferEXT);
  // BIND_EXTENSION(GetNamedFramebufferAttachmentParameteriv,
  //                GetNamedFramebufferAttachmentParameterivEXT);
  // BIND_EXTENSION(GenerateTextureMipmap, GenerateTextureMipmapEXT);
  // BIND_EXTENSION(FramebufferDrawBuffer, FramebufferDrawBufferEXT);
  // BIND_EXTENSION(FramebufferDrawBuffers, FramebufferDrawBuffersEXT);
  // BIND_EXTENSION(FramebufferReadBuffer, FramebufferReadBufferEXT);
  // BIND_EXTENSION(GetFramebufferParameteriv, GetFramebufferParameterivEXT);
  // BIND_EXTENSION(NamedCopyBufferSubData, NamedCopyBufferSubDataEXT);
  // BIND_EXTENSION(VertexArrayVertexOffset, VertexArrayVertexOffsetEXT);
  // BIND_EXTENSION(VertexArrayColorOffset, VertexArrayColorOffsetEXT);
  // BIND_EXTENSION(VertexArrayEdgeFlagOffset, VertexArrayEdgeFlagOffsetEXT);
  // BIND_EXTENSION(VertexArrayIndexOffset, VertexArrayIndexOffsetEXT);
  // BIND_EXTENSION(VertexArrayNormalOffset, VertexArrayNormalOffsetEXT);
  // BIND_EXTENSION(VertexArrayTexCoordOffset, VertexArrayTexCoordOffsetEXT);
  // BIND_EXTENSION(VertexArrayMultiTexCoordOffset,
  //                VertexArrayMultiTexCoordOffsetEXT);
  // BIND_EXTENSION(VertexArrayFogCoordOffset, VertexArrayFogCoordOffsetEXT);
  // BIND_EXTENSION(VertexArraySecondaryColorOffset,
  //                VertexArraySecondaryColorOffsetEXT);
  // BIND_EXTENSION(VertexArrayVertexAttribOffset,
  //                VertexArrayVertexAttribOffsetEXT);
  // BIND_EXTENSION(VertexArrayVertexAttribIOffset,
  //                VertexArrayVertexAttribIOffsetEXT);
  // BIND_EXTENSION(EnableVertexArray, EnableVertexArrayEXT);
  // BIND_EXTENSION(DisableVertexArray, DisableVertexArrayEXT);
  // BIND_EXTENSION(EnableVertexArrayAttrib, EnableVertexArrayAttribEXT);
  // BIND_EXTENSION(DisableVertexArrayAttrib, DisableVertexArrayAttribEXT);
  // BIND_EXTENSION(GetVertexArrayIntegerv, GetVertexArrayIntegervEXT);
  // BIND_EXTENSION(GetVertexArrayPointerv, GetVertexArrayPointervEXT);
  // BIND_EXTENSION(GetVertexArrayIntegeri_v, GetVertexArrayIntegeri_vEXT);
  // BIND_EXTENSION(GetVertexArrayPointeri_v, GetVertexArrayPointeri_vEXT);
  // BIND_EXTENSION(MapNamedBufferRange, MapNamedBufferRangeEXT);
  // BIND_EXTENSION(FlushMappedNamedBufferRange,
  //                FlushMappedNamedBufferRangeEXT);
  // BIND_EXTENSION(TextureBuffer, TextureBufferEXT);

  // Some drivers report GL_KHR_debug but do not provide functions. Validate and
  // remove reported extension from the list if necessary
  // See https://crbug.com/1008125
  if (gl->glDebugMessageControlFn && gl->glDebugMessageInsertFn &&
      gl->glDebugMessageCallbackFn && gl->glGetDebugMessageLogFn &&
      gl->glPushDebugGroupFn && gl->glPopDebugGroupFn && gl->glObjectLabelFn) {
    BIND(DebugMessageControl);
    BIND(DebugMessageInsert);
    BIND(DebugMessageCallback);
    BIND(GetDebugMessageLog);
    BIND(PushDebugGroup);
    BIND(PopDebugGroup);
    BIND(ObjectLabel);
  } else {
    extensions.remove("GL_KHR_debug");
  }

  // GL_EXT_window_rectangles
  BIND_EXTENSION(WindowRectangles, WindowRectanglesEXT);

  // GL_QCOM_tiled_rendering
  BIND_EXTENSION(StartTiling, StartTilingQCOM);
  BIND_EXTENSION(EndTiling, EndTilingQCOM);

  // EGL_KHR_image / EGL_KHR_image_base
  // functions->fCreateImage = nullptr;
  // functions->fDestroyImage = nullptr;

  BIND(FenceSync);
  BIND(IsSync);
  BIND(ClientWaitSync);
  BIND(WaitSync);
  BIND(DeleteSync);

  if (!gl->glFenceSyncFn && GetDefaultDisplayEGL()->ext->b_EGL_KHR_fence_sync) {
    // Emulate APPLE_sync via egl
    extensions.add("GL_APPLE_sync");

    functions->fFenceSync = glFenceSyncEmulateEGL;
    functions->fIsSync = glIsSyncEmulateEGL;
    functions->fClientWaitSync = glClientWaitSyncEmulateEGL;
    functions->fWaitSync = glWaitSyncEmulateEGL;
    functions->fDeleteSync = glDeleteSyncEmulateEGL;
  }

  // Skia can fall back to GL_NV_fence if GLsync objects are not available.
  BIND_EXTENSION(DeleteFences, DeleteFencesNV);
  BIND_EXTENSION(FinishFence, FinishFenceNV);
  BIND_EXTENSION(GenFences, GenFencesNV);
  BIND_EXTENSION(SetFence, SetFenceNV);
  BIND_EXTENSION(TestFence, TestFenceNV);

  BIND(GetInternalformativ);

#undef BIND
#undef BIND_EXTENSION

  gl_interface->fStandard = standard;
  gl_interface->fExtensions.swap(&extensions);
  sk_sp<GrGLInterface> returned(gl_interface);
  return returned;
}

}  // namespace gl::init
