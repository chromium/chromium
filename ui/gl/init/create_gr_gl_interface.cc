// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

namespace gl {
namespace init {

namespace {

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind(R (gl::GLApi::*func)(Args...),
                                                  gl::GLApi* api) {
  return [func, api](Args... args) { return (api->*func)(args...); };
}

class ScopedProgressReporter {
 public:
  ScopedProgressReporter(gl::ProgressReporter* progress_reporter)
      : progress_reporter_(progress_reporter) {}
  ~ScopedProgressReporter() { progress_reporter_->ReportProgress(); }

 private:
  gl::ProgressReporter* progress_reporter_;
};

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> bind_slow(
    R(GL_BINDING_CALL* func)(Args...),
    gl::ProgressReporter* progress_reporter) {
  if (!progress_reporter)
    return func;
  return [func, progress_reporter](Args... args) {
    ScopedProgressReporter scoped_reporter(progress_reporter);
    return func(args...);
  };
};

const GLubyte* GetStringHook(const char* version_string, GLenum name) {
  switch (name) {
    case GL_VERSION:
      return reinterpret_cast<const GLubyte*>(version_string);
    default:
      return glGetString(name);
  }
}

const char* kBlacklistExtensions[] = {
    "GL_APPLE_framebuffer_multisample",
    "GL_APPLE_sync",
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
  // by the bindings (GL 4.1 or ES 3.0), and blacklist extensions that skia
  // handles but bindings don't.
  // TODO(piman): add bindings for missing entrypoints.
  GrGLFunction<GrGLGetStringFn> get_string;
  const bool apply_version_override = use_version_es2 ||
                                      version_info.IsAtLeastGL(4, 2) ||
                                      version_info.IsAtLeastGLES(3, 1);
  if (apply_version_override) {
    const char* fake_version = nullptr;
    if (use_version_es2) {
      fake_version = "OpenGL ES 2.0";
    } else {
      fake_version = version_info.is_es ? "OpenGL ES 3.0" : "4.1";
    }
    get_string = [fake_version](GLenum name) {
      return GetStringHook(fake_version, name);
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
  for (const char* extension : kBlacklistExtensions)
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
  functions->fBindTexture = gl->glBindTextureFn;

  functions->fBlendBarrier = gl->glBlendBarrierKHRFn;

  functions->fBlendColor = gl->glBlendColorFn;
  functions->fBlendEquation = gl->glBlendEquationFn;
  functions->fBlendFunc = gl->glBlendFuncFn;
  functions->fBufferData = gl->glBufferDataFn;
  functions->fBufferSubData = gl->glBufferSubDataFn;
  functions->fClear = gl->glClearFn;
  functions->fClearColor = gl->glClearColorFn;
  functions->fClearStencil = gl->glClearStencilFn;

  // Not used
  // functions->fClearTexImage = nullptr;
  // functions->fClearTexSubImage = nullptr;

  functions->fColorMask = gl->glColorMaskFn;
  functions->fCompileShader =
      bind_slow(gl->glCompileShaderFn, progress_reporter);
  functions->fCompressedTexImage2D =
      bind_slow(gl->glCompressedTexImage2DFn, progress_reporter);
  functions->fCompressedTexSubImage2D =
      bind_slow(gl->glCompressedTexSubImage2DFn, progress_reporter);
  functions->fCopyTexSubImage2D =
      bind_slow(gl->glCopyTexSubImage2DFn, progress_reporter);
  functions->fCreateProgram = gl->glCreateProgramFn;
  functions->fCreateShader = gl->glCreateShaderFn;
  functions->fCullFace = gl->glCullFaceFn;
  functions->fDeleteBuffers =
      bind_slow(gl->glDeleteBuffersARBFn, progress_reporter);
  functions->fDeleteProgram =
      bind_slow(gl->glDeleteProgramFn, progress_reporter);
  functions->fDeleteQueries = gl->glDeleteQueriesFn;
  functions->fDeleteSamplers = gl->glDeleteSamplersFn;
  functions->fDeleteShader = bind_slow(gl->glDeleteShaderFn, progress_reporter);
  functions->fDeleteTextures = gl->glDeleteTexturesFn;
  functions->fDepthMask = gl->glDepthMaskFn;
  functions->fDisable = gl->glDisableFn;
  functions->fDisableVertexAttribArray = gl->glDisableVertexAttribArrayFn;
  functions->fDiscardFramebuffer = gl->glDiscardFramebufferEXTFn;
  functions->fDrawArrays = gl->glDrawArraysFn;
  functions->fDrawBuffer = gl->glDrawBufferFn;
  functions->fDrawBuffers = gl->glDrawBuffersARBFn;
  functions->fDrawElements = gl->glDrawElementsFn;

  functions->fDrawArraysInstanced = gl->glDrawArraysInstancedANGLEFn;
  functions->fDrawElementsInstanced = gl->glDrawElementsInstancedANGLEFn;

  // GL 4.0 or GL_ARB_draw_indirect or ES 3.1
  functions->fDrawArraysIndirect = gl->glDrawArraysIndirectFn;
  functions->fDrawElementsIndirect = gl->glDrawElementsIndirectFn;

  functions->fDrawRangeElements = gl->glDrawRangeElementsFn;
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
  functions->fGetProgramiv = gl->glGetProgramivFn;
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
  functions->fTexImage2D = bind_slow(gl->glTexImage2DFn, progress_reporter);
  functions->fTexParameteri = gl->glTexParameteriFn;
  functions->fTexParameteriv = gl->glTexParameterivFn;
  functions->fTexStorage2D = gl->glTexStorage2DEXTFn;
  functions->fTexSubImage2D = gl->glTexSubImage2DFn;

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
  functions->fBindFramebuffer = gl->glBindFramebufferEXTFn;
  functions->fFramebufferTexture2D = gl->glFramebufferTexture2DEXTFn;
  functions->fCheckFramebufferStatus = gl->glCheckFramebufferStatusEXTFn;
  functions->fDeleteFramebuffers =
      bind_slow(gl->glDeleteFramebuffersEXTFn, progress_reporter);
  functions->fRenderbufferStorage = gl->glRenderbufferStorageEXTFn;
  functions->fGenRenderbuffers = gl->glGenRenderbuffersEXTFn;
  functions->fDeleteRenderbuffers = gl->glDeleteRenderbuffersEXTFn;
  functions->fFramebufferRenderbuffer = gl->glFramebufferRenderbufferEXTFn;
  functions->fBindRenderbuffer = gl->glBindRenderbufferEXTFn;
  functions->fRenderbufferStorageMultisample =
      gl->glRenderbufferStorageMultisampleFn;
  functions->fFramebufferTexture2DMultisample =
      gl->glFramebufferTexture2DMultisampleEXTFn;
  functions->fRenderbufferStorageMultisampleES2EXT =
      gl->glRenderbufferStorageMultisampleEXTFn;
  functions->fBlitFramebuffer = gl->glBlitFramebufferFn;

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

  functions->fDebugMessageControl = gl->glDebugMessageControlFn;
  functions->fDebugMessageInsert = gl->glDebugMessageInsertFn;
  functions->fDebugMessageCallback = gl->glDebugMessageCallbackFn;
  functions->fGetDebugMessageLog = gl->glGetDebugMessageLogFn;
  functions->fPushDebugGroup = gl->glPushDebugGroupFn;
  functions->fPopDebugGroup = gl->glPopDebugGroupFn;
  functions->fObjectLabel = gl->glObjectLabelFn;

  // GL_EXT_window_rectangles
  functions->fWindowRectangles = gl->glWindowRectanglesEXTFn;

  // EGL_KHR_image / EGL_KHR_image_base
  // functions->fCreateImage = nullptr;
  // functions->fDestroyImage = nullptr;

  // GL 4.0 or GL_ARB_sample_shading or ES+GL_OES_sample_shading
  functions->fMinSampleShading = gl->glMinSampleShadingFn;

  functions->fFenceSync = gl->glFenceSyncFn;
  functions->fIsSync = gl->glIsSyncFn;
  functions->fClientWaitSync = gl->glClientWaitSyncFn;
  functions->fWaitSync = gl->glWaitSyncFn;
  functions->fDeleteSync = gl->glDeleteSyncFn;

  functions->fGetInternalformativ = gl->glGetInternalformativFn;

  interface->fStandard = standard;
  interface->fExtensions.swap(&extensions);
  sk_sp<GrGLInterface> returned(interface);
  return returned;
}

}  // namespace init
}  // namespace gl
