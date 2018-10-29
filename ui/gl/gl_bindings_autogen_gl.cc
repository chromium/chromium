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
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

void DriverGL::InitializeStaticBindings() {
  // Ensure struct has been zero-initialized.
  char* this_bytes = reinterpret_cast<char*>(this);
  DCHECK(this_bytes[0] == 0);
  DCHECK(memcmp(this_bytes, this_bytes + 1, sizeof(*this) - 1) == 0);

  fn.glActiveTextureFn = reinterpret_cast<glActiveTextureProc>(
      GetGLProcAddress("glActiveTexture"));
  fn.glAttachShaderFn =
      reinterpret_cast<glAttachShaderProc>(GetGLProcAddress("glAttachShader"));
  fn.glBindAttribLocationFn = reinterpret_cast<glBindAttribLocationProc>(
      GetGLProcAddress("glBindAttribLocation"));
  fn.glBindBufferFn =
      reinterpret_cast<glBindBufferProc>(GetGLProcAddress("glBindBuffer"));
  fn.glBindTextureFn =
      reinterpret_cast<glBindTextureProc>(GetGLProcAddress("glBindTexture"));
  fn.glBlendColorFn =
      reinterpret_cast<glBlendColorProc>(GetGLProcAddress("glBlendColor"));
  fn.glBlendEquationFn = reinterpret_cast<glBlendEquationProc>(
      GetGLProcAddress("glBlendEquation"));
  fn.glBlendEquationSeparateFn = reinterpret_cast<glBlendEquationSeparateProc>(
      GetGLProcAddress("glBlendEquationSeparate"));
  fn.glBlendFuncFn =
      reinterpret_cast<glBlendFuncProc>(GetGLProcAddress("glBlendFunc"));
  fn.glBlendFuncSeparateFn = reinterpret_cast<glBlendFuncSeparateProc>(
      GetGLProcAddress("glBlendFuncSeparate"));
  fn.glBufferDataFn =
      reinterpret_cast<glBufferDataProc>(GetGLProcAddress("glBufferData"));
  fn.glBufferSubDataFn = reinterpret_cast<glBufferSubDataProc>(
      GetGLProcAddress("glBufferSubData"));
  fn.glClearFn = reinterpret_cast<glClearProc>(GetGLProcAddress("glClear"));
  fn.glClearColorFn =
      reinterpret_cast<glClearColorProc>(GetGLProcAddress("glClearColor"));
  fn.glClearDepthFn =
      reinterpret_cast<glClearDepthProc>(GetGLProcAddress("glClearDepth"));
  fn.glClearStencilFn =
      reinterpret_cast<glClearStencilProc>(GetGLProcAddress("glClearStencil"));
  fn.glColorMaskFn =
      reinterpret_cast<glColorMaskProc>(GetGLProcAddress("glColorMask"));
  fn.glCompileShaderFn = reinterpret_cast<glCompileShaderProc>(
      GetGLProcAddress("glCompileShader"));
  fn.glCompressedTexImage2DFn = reinterpret_cast<glCompressedTexImage2DProc>(
      GetGLProcAddress("glCompressedTexImage2D"));
  fn.glCompressedTexSubImage2DFn =
      reinterpret_cast<glCompressedTexSubImage2DProc>(
          GetGLProcAddress("glCompressedTexSubImage2D"));
  fn.glCopyTexImage2DFn = reinterpret_cast<glCopyTexImage2DProc>(
      GetGLProcAddress("glCopyTexImage2D"));
  fn.glCopyTexSubImage2DFn = reinterpret_cast<glCopyTexSubImage2DProc>(
      GetGLProcAddress("glCopyTexSubImage2D"));
  fn.glCreateProgramFn = reinterpret_cast<glCreateProgramProc>(
      GetGLProcAddress("glCreateProgram"));
  fn.glCreateShaderFn =
      reinterpret_cast<glCreateShaderProc>(GetGLProcAddress("glCreateShader"));
  fn.glCullFaceFn =
      reinterpret_cast<glCullFaceProc>(GetGLProcAddress("glCullFace"));
  fn.glDeleteBuffersARBFn = reinterpret_cast<glDeleteBuffersARBProc>(
      GetGLProcAddress("glDeleteBuffers"));
  fn.glDeleteProgramFn = reinterpret_cast<glDeleteProgramProc>(
      GetGLProcAddress("glDeleteProgram"));
  fn.glDeleteShaderFn =
      reinterpret_cast<glDeleteShaderProc>(GetGLProcAddress("glDeleteShader"));
  fn.glDeleteTexturesFn = reinterpret_cast<glDeleteTexturesProc>(
      GetGLProcAddress("glDeleteTextures"));
  fn.glDepthFuncFn =
      reinterpret_cast<glDepthFuncProc>(GetGLProcAddress("glDepthFunc"));
  fn.glDepthMaskFn =
      reinterpret_cast<glDepthMaskProc>(GetGLProcAddress("glDepthMask"));
  fn.glDepthRangeFn =
      reinterpret_cast<glDepthRangeProc>(GetGLProcAddress("glDepthRange"));
  fn.glDetachShaderFn =
      reinterpret_cast<glDetachShaderProc>(GetGLProcAddress("glDetachShader"));
  fn.glDisableFn =
      reinterpret_cast<glDisableProc>(GetGLProcAddress("glDisable"));
  fn.glDisableVertexAttribArrayFn =
      reinterpret_cast<glDisableVertexAttribArrayProc>(
          GetGLProcAddress("glDisableVertexAttribArray"));
  fn.glDrawArraysFn =
      reinterpret_cast<glDrawArraysProc>(GetGLProcAddress("glDrawArrays"));
  fn.glDrawElementsFn =
      reinterpret_cast<glDrawElementsProc>(GetGLProcAddress("glDrawElements"));
  fn.glEnableFn = reinterpret_cast<glEnableProc>(GetGLProcAddress("glEnable"));
  fn.glEnableVertexAttribArrayFn =
      reinterpret_cast<glEnableVertexAttribArrayProc>(
          GetGLProcAddress("glEnableVertexAttribArray"));
  fn.glFinishFn = reinterpret_cast<glFinishProc>(GetGLProcAddress("glFinish"));
  fn.glFlushFn = reinterpret_cast<glFlushProc>(GetGLProcAddress("glFlush"));
  fn.glFrontFaceFn =
      reinterpret_cast<glFrontFaceProc>(GetGLProcAddress("glFrontFace"));
  fn.glGenBuffersARBFn =
      reinterpret_cast<glGenBuffersARBProc>(GetGLProcAddress("glGenBuffers"));
  fn.glGenTexturesFn =
      reinterpret_cast<glGenTexturesProc>(GetGLProcAddress("glGenTextures"));
  fn.glGetActiveAttribFn = reinterpret_cast<glGetActiveAttribProc>(
      GetGLProcAddress("glGetActiveAttrib"));
  fn.glGetActiveUniformFn = reinterpret_cast<glGetActiveUniformProc>(
      GetGLProcAddress("glGetActiveUniform"));
  fn.glGetAttachedShadersFn = reinterpret_cast<glGetAttachedShadersProc>(
      GetGLProcAddress("glGetAttachedShaders"));
  fn.glGetAttribLocationFn = reinterpret_cast<glGetAttribLocationProc>(
      GetGLProcAddress("glGetAttribLocation"));
  fn.glGetBooleanvFn =
      reinterpret_cast<glGetBooleanvProc>(GetGLProcAddress("glGetBooleanv"));
  fn.glGetBufferParameterivFn = reinterpret_cast<glGetBufferParameterivProc>(
      GetGLProcAddress("glGetBufferParameteriv"));
  fn.glGetErrorFn =
      reinterpret_cast<glGetErrorProc>(GetGLProcAddress("glGetError"));
  fn.glGetFloatvFn =
      reinterpret_cast<glGetFloatvProc>(GetGLProcAddress("glGetFloatv"));
  fn.glGetIntegervFn =
      reinterpret_cast<glGetIntegervProc>(GetGLProcAddress("glGetIntegerv"));
  fn.glGetProgramInfoLogFn = reinterpret_cast<glGetProgramInfoLogProc>(
      GetGLProcAddress("glGetProgramInfoLog"));
  fn.glGetProgramivFn =
      reinterpret_cast<glGetProgramivProc>(GetGLProcAddress("glGetProgramiv"));
  fn.glGetShaderInfoLogFn = reinterpret_cast<glGetShaderInfoLogProc>(
      GetGLProcAddress("glGetShaderInfoLog"));
  fn.glGetShaderivFn =
      reinterpret_cast<glGetShaderivProc>(GetGLProcAddress("glGetShaderiv"));
  fn.glGetShaderSourceFn = reinterpret_cast<glGetShaderSourceProc>(
      GetGLProcAddress("glGetShaderSource"));
  fn.glGetStringFn =
      reinterpret_cast<glGetStringProc>(GetGLProcAddress("glGetString"));
  fn.glGetStringiFn =
      reinterpret_cast<glGetStringiProc>(GetGLProcAddress("glGetStringi"));
  fn.glGetTexParameterfvFn = reinterpret_cast<glGetTexParameterfvProc>(
      GetGLProcAddress("glGetTexParameterfv"));
  fn.glGetTexParameterivFn = reinterpret_cast<glGetTexParameterivProc>(
      GetGLProcAddress("glGetTexParameteriv"));
  fn.glGetUniformfvFn =
      reinterpret_cast<glGetUniformfvProc>(GetGLProcAddress("glGetUniformfv"));
  fn.glGetUniformivFn =
      reinterpret_cast<glGetUniformivProc>(GetGLProcAddress("glGetUniformiv"));
  fn.glGetUniformLocationFn = reinterpret_cast<glGetUniformLocationProc>(
      GetGLProcAddress("glGetUniformLocation"));
  fn.glGetVertexAttribfvFn = reinterpret_cast<glGetVertexAttribfvProc>(
      GetGLProcAddress("glGetVertexAttribfv"));
  fn.glGetVertexAttribivFn = reinterpret_cast<glGetVertexAttribivProc>(
      GetGLProcAddress("glGetVertexAttribiv"));
  fn.glGetVertexAttribPointervFn =
      reinterpret_cast<glGetVertexAttribPointervProc>(
          GetGLProcAddress("glGetVertexAttribPointerv"));
  fn.glHintFn = reinterpret_cast<glHintProc>(GetGLProcAddress("glHint"));
  fn.glIsBufferFn =
      reinterpret_cast<glIsBufferProc>(GetGLProcAddress("glIsBuffer"));
  fn.glIsEnabledFn =
      reinterpret_cast<glIsEnabledProc>(GetGLProcAddress("glIsEnabled"));
  fn.glIsProgramFn =
      reinterpret_cast<glIsProgramProc>(GetGLProcAddress("glIsProgram"));
  fn.glIsShaderFn =
      reinterpret_cast<glIsShaderProc>(GetGLProcAddress("glIsShader"));
  fn.glIsTextureFn =
      reinterpret_cast<glIsTextureProc>(GetGLProcAddress("glIsTexture"));
  fn.glLineWidthFn =
      reinterpret_cast<glLineWidthProc>(GetGLProcAddress("glLineWidth"));
  fn.glLinkProgramFn =
      reinterpret_cast<glLinkProgramProc>(GetGLProcAddress("glLinkProgram"));
  fn.glPixelStoreiFn =
      reinterpret_cast<glPixelStoreiProc>(GetGLProcAddress("glPixelStorei"));
  fn.glPolygonOffsetFn = reinterpret_cast<glPolygonOffsetProc>(
      GetGLProcAddress("glPolygonOffset"));
  fn.glReadPixelsFn =
      reinterpret_cast<glReadPixelsProc>(GetGLProcAddress("glReadPixels"));
  fn.glSampleCoverageFn = reinterpret_cast<glSampleCoverageProc>(
      GetGLProcAddress("glSampleCoverage"));
  fn.glScissorFn =
      reinterpret_cast<glScissorProc>(GetGLProcAddress("glScissor"));
  fn.glShaderSourceFn =
      reinterpret_cast<glShaderSourceProc>(GetGLProcAddress("glShaderSource"));
  fn.glStencilFuncFn =
      reinterpret_cast<glStencilFuncProc>(GetGLProcAddress("glStencilFunc"));
  fn.glStencilFuncSeparateFn = reinterpret_cast<glStencilFuncSeparateProc>(
      GetGLProcAddress("glStencilFuncSeparate"));
  fn.glStencilMaskFn =
      reinterpret_cast<glStencilMaskProc>(GetGLProcAddress("glStencilMask"));
  fn.glStencilMaskSeparateFn = reinterpret_cast<glStencilMaskSeparateProc>(
      GetGLProcAddress("glStencilMaskSeparate"));
  fn.glStencilOpFn =
      reinterpret_cast<glStencilOpProc>(GetGLProcAddress("glStencilOp"));
  fn.glStencilOpSeparateFn = reinterpret_cast<glStencilOpSeparateProc>(
      GetGLProcAddress("glStencilOpSeparate"));
  fn.glTexImage2DFn =
      reinterpret_cast<glTexImage2DProc>(GetGLProcAddress("glTexImage2D"));
  fn.glTexParameterfFn = reinterpret_cast<glTexParameterfProc>(
      GetGLProcAddress("glTexParameterf"));
  fn.glTexParameterfvFn = reinterpret_cast<glTexParameterfvProc>(
      GetGLProcAddress("glTexParameterfv"));
  fn.glTexParameteriFn = reinterpret_cast<glTexParameteriProc>(
      GetGLProcAddress("glTexParameteri"));
  fn.glTexParameterivFn = reinterpret_cast<glTexParameterivProc>(
      GetGLProcAddress("glTexParameteriv"));
  fn.glTexSubImage2DFn = reinterpret_cast<glTexSubImage2DProc>(
      GetGLProcAddress("glTexSubImage2D"));
  fn.glUniform1fFn =
      reinterpret_cast<glUniform1fProc>(GetGLProcAddress("glUniform1f"));
  fn.glUniform1fvFn =
      reinterpret_cast<glUniform1fvProc>(GetGLProcAddress("glUniform1fv"));
  fn.glUniform1iFn =
      reinterpret_cast<glUniform1iProc>(GetGLProcAddress("glUniform1i"));
  fn.glUniform1ivFn =
      reinterpret_cast<glUniform1ivProc>(GetGLProcAddress("glUniform1iv"));
  fn.glUniform2fFn =
      reinterpret_cast<glUniform2fProc>(GetGLProcAddress("glUniform2f"));
  fn.glUniform2fvFn =
      reinterpret_cast<glUniform2fvProc>(GetGLProcAddress("glUniform2fv"));
  fn.glUniform2iFn =
      reinterpret_cast<glUniform2iProc>(GetGLProcAddress("glUniform2i"));
  fn.glUniform2ivFn =
      reinterpret_cast<glUniform2ivProc>(GetGLProcAddress("glUniform2iv"));
  fn.glUniform3fFn =
      reinterpret_cast<glUniform3fProc>(GetGLProcAddress("glUniform3f"));
  fn.glUniform3fvFn =
      reinterpret_cast<glUniform3fvProc>(GetGLProcAddress("glUniform3fv"));
  fn.glUniform3iFn =
      reinterpret_cast<glUniform3iProc>(GetGLProcAddress("glUniform3i"));
  fn.glUniform3ivFn =
      reinterpret_cast<glUniform3ivProc>(GetGLProcAddress("glUniform3iv"));
  fn.glUniform4fFn =
      reinterpret_cast<glUniform4fProc>(GetGLProcAddress("glUniform4f"));
  fn.glUniform4fvFn =
      reinterpret_cast<glUniform4fvProc>(GetGLProcAddress("glUniform4fv"));
  fn.glUniform4iFn =
      reinterpret_cast<glUniform4iProc>(GetGLProcAddress("glUniform4i"));
  fn.glUniform4ivFn =
      reinterpret_cast<glUniform4ivProc>(GetGLProcAddress("glUniform4iv"));
  fn.glUniformMatrix2fvFn = reinterpret_cast<glUniformMatrix2fvProc>(
      GetGLProcAddress("glUniformMatrix2fv"));
  fn.glUniformMatrix3fvFn = reinterpret_cast<glUniformMatrix3fvProc>(
      GetGLProcAddress("glUniformMatrix3fv"));
  fn.glUniformMatrix4fvFn = reinterpret_cast<glUniformMatrix4fvProc>(
      GetGLProcAddress("glUniformMatrix4fv"));
  fn.glUseProgramFn =
      reinterpret_cast<glUseProgramProc>(GetGLProcAddress("glUseProgram"));
  fn.glValidateProgramFn = reinterpret_cast<glValidateProgramProc>(
      GetGLProcAddress("glValidateProgram"));
  fn.glVertexAttrib1fFn = reinterpret_cast<glVertexAttrib1fProc>(
      GetGLProcAddress("glVertexAttrib1f"));
  fn.glVertexAttrib1fvFn = reinterpret_cast<glVertexAttrib1fvProc>(
      GetGLProcAddress("glVertexAttrib1fv"));
  fn.glVertexAttrib2fFn = reinterpret_cast<glVertexAttrib2fProc>(
      GetGLProcAddress("glVertexAttrib2f"));
  fn.glVertexAttrib2fvFn = reinterpret_cast<glVertexAttrib2fvProc>(
      GetGLProcAddress("glVertexAttrib2fv"));
  fn.glVertexAttrib3fFn = reinterpret_cast<glVertexAttrib3fProc>(
      GetGLProcAddress("glVertexAttrib3f"));
  fn.glVertexAttrib3fvFn = reinterpret_cast<glVertexAttrib3fvProc>(
      GetGLProcAddress("glVertexAttrib3fv"));
  fn.glVertexAttrib4fFn = reinterpret_cast<glVertexAttrib4fProc>(
      GetGLProcAddress("glVertexAttrib4f"));
  fn.glVertexAttrib4fvFn = reinterpret_cast<glVertexAttrib4fvProc>(
      GetGLProcAddress("glVertexAttrib4fv"));
  fn.glVertexAttribPointerFn = reinterpret_cast<glVertexAttribPointerProc>(
      GetGLProcAddress("glVertexAttribPointer"));
  fn.glViewportFn =
      reinterpret_cast<glViewportProc>(GetGLProcAddress("glViewport"));
}

void DriverGL::InitializeDynamicBindings(const GLVersionInfo* ver,
                                         const gfx::ExtensionSet& extensions) {
  ext.b_GL_ANGLE_framebuffer_blit =
      gfx::HasExtension(extensions, "GL_ANGLE_framebuffer_blit");
  ext.b_GL_ANGLE_framebuffer_multisample =
      gfx::HasExtension(extensions, "GL_ANGLE_framebuffer_multisample");
  ext.b_GL_ANGLE_instanced_arrays =
      gfx::HasExtension(extensions, "GL_ANGLE_instanced_arrays");
  ext.b_GL_ANGLE_multiview =
      gfx::HasExtension(extensions, "GL_ANGLE_multiview");
  ext.b_GL_ANGLE_request_extension =
      gfx::HasExtension(extensions, "GL_ANGLE_request_extension");
  ext.b_GL_ANGLE_robust_client_memory =
      gfx::HasExtension(extensions, "GL_ANGLE_robust_client_memory");
  ext.b_GL_ANGLE_translated_shader_source =
      gfx::HasExtension(extensions, "GL_ANGLE_translated_shader_source");
  ext.b_GL_APPLE_fence = gfx::HasExtension(extensions, "GL_APPLE_fence");
  ext.b_GL_APPLE_vertex_array_object =
      gfx::HasExtension(extensions, "GL_APPLE_vertex_array_object");
  ext.b_GL_ARB_blend_func_extended =
      gfx::HasExtension(extensions, "GL_ARB_blend_func_extended");
  ext.b_GL_ARB_draw_buffers =
      gfx::HasExtension(extensions, "GL_ARB_draw_buffers");
  ext.b_GL_ARB_draw_instanced =
      gfx::HasExtension(extensions, "GL_ARB_draw_instanced");
  ext.b_GL_ARB_framebuffer_object =
      gfx::HasExtension(extensions, "GL_ARB_framebuffer_object");
  ext.b_GL_ARB_get_program_binary =
      gfx::HasExtension(extensions, "GL_ARB_get_program_binary");
  ext.b_GL_ARB_instanced_arrays =
      gfx::HasExtension(extensions, "GL_ARB_instanced_arrays");
  ext.b_GL_ARB_internalformat_query =
      gfx::HasExtension(extensions, "GL_ARB_internalformat_query");
  ext.b_GL_ARB_map_buffer_range =
      gfx::HasExtension(extensions, "GL_ARB_map_buffer_range");
  ext.b_GL_ARB_occlusion_query =
      gfx::HasExtension(extensions, "GL_ARB_occlusion_query");
  ext.b_GL_ARB_program_interface_query =
      gfx::HasExtension(extensions, "GL_ARB_program_interface_query");
  ext.b_GL_ARB_robustness = gfx::HasExtension(extensions, "GL_ARB_robustness");
  ext.b_GL_ARB_sampler_objects =
      gfx::HasExtension(extensions, "GL_ARB_sampler_objects");
  ext.b_GL_ARB_shader_image_load_store =
      gfx::HasExtension(extensions, "GL_ARB_shader_image_load_store");
  ext.b_GL_ARB_sync = gfx::HasExtension(extensions, "GL_ARB_sync");
  ext.b_GL_ARB_texture_multisample =
      gfx::HasExtension(extensions, "GL_ARB_texture_multisample");
  ext.b_GL_ARB_texture_storage =
      gfx::HasExtension(extensions, "GL_ARB_texture_storage");
  ext.b_GL_ARB_timer_query =
      gfx::HasExtension(extensions, "GL_ARB_timer_query");
  ext.b_GL_ARB_transform_feedback2 =
      gfx::HasExtension(extensions, "GL_ARB_transform_feedback2");
  ext.b_GL_ARB_vertex_array_object =
      gfx::HasExtension(extensions, "GL_ARB_vertex_array_object");
  ext.b_GL_CHROMIUM_bind_uniform_location =
      gfx::HasExtension(extensions, "GL_CHROMIUM_bind_uniform_location");
  ext.b_GL_CHROMIUM_copy_texture =
      gfx::HasExtension(extensions, "GL_CHROMIUM_copy_texture");
  ext.b_GL_CHROMIUM_framebuffer_mixed_samples =
      gfx::HasExtension(extensions, "GL_CHROMIUM_framebuffer_mixed_samples");
  ext.b_GL_CHROMIUM_gles_depth_binding_hack =
      gfx::HasExtension(extensions, "GL_CHROMIUM_gles_depth_binding_hack");
  ext.b_GL_CHROMIUM_glgetstringi_hack =
      gfx::HasExtension(extensions, "GL_CHROMIUM_glgetstringi_hack");
  ext.b_GL_CHROMIUM_path_rendering =
      gfx::HasExtension(extensions, "GL_CHROMIUM_path_rendering");
  ext.b_GL_EXT_blend_func_extended =
      gfx::HasExtension(extensions, "GL_EXT_blend_func_extended");
  ext.b_GL_EXT_debug_marker =
      gfx::HasExtension(extensions, "GL_EXT_debug_marker");
  ext.b_GL_EXT_direct_state_access =
      gfx::HasExtension(extensions, "GL_EXT_direct_state_access");
  ext.b_GL_EXT_discard_framebuffer =
      gfx::HasExtension(extensions, "GL_EXT_discard_framebuffer");
  ext.b_GL_EXT_disjoint_timer_query =
      gfx::HasExtension(extensions, "GL_EXT_disjoint_timer_query");
  ext.b_GL_EXT_draw_buffers =
      gfx::HasExtension(extensions, "GL_EXT_draw_buffers");
  ext.b_GL_EXT_framebuffer_blit =
      gfx::HasExtension(extensions, "GL_EXT_framebuffer_blit");
  ext.b_GL_EXT_framebuffer_multisample =
      gfx::HasExtension(extensions, "GL_EXT_framebuffer_multisample");
  ext.b_GL_EXT_framebuffer_object =
      gfx::HasExtension(extensions, "GL_EXT_framebuffer_object");
  ext.b_GL_EXT_gpu_shader4 =
      gfx::HasExtension(extensions, "GL_EXT_gpu_shader4");
  ext.b_GL_EXT_instanced_arrays =
      gfx::HasExtension(extensions, "GL_EXT_instanced_arrays");
  ext.b_GL_EXT_map_buffer_range =
      gfx::HasExtension(extensions, "GL_EXT_map_buffer_range");
  ext.b_GL_EXT_multisampled_render_to_texture =
      gfx::HasExtension(extensions, "GL_EXT_multisampled_render_to_texture");
  ext.b_GL_EXT_occlusion_query_boolean =
      gfx::HasExtension(extensions, "GL_EXT_occlusion_query_boolean");
  ext.b_GL_EXT_robustness = gfx::HasExtension(extensions, "GL_EXT_robustness");
  ext.b_GL_EXT_shader_image_load_store =
      gfx::HasExtension(extensions, "GL_EXT_shader_image_load_store");
  ext.b_GL_EXT_texture_buffer =
      gfx::HasExtension(extensions, "GL_EXT_texture_buffer");
  ext.b_GL_EXT_texture_buffer_object =
      gfx::HasExtension(extensions, "GL_EXT_texture_buffer_object");
  ext.b_GL_EXT_texture_storage =
      gfx::HasExtension(extensions, "GL_EXT_texture_storage");
  ext.b_GL_EXT_timer_query =
      gfx::HasExtension(extensions, "GL_EXT_timer_query");
  ext.b_GL_EXT_transform_feedback =
      gfx::HasExtension(extensions, "GL_EXT_transform_feedback");
  ext.b_GL_EXT_unpack_subimage =
      gfx::HasExtension(extensions, "GL_EXT_unpack_subimage");
  ext.b_GL_EXT_window_rectangles =
      gfx::HasExtension(extensions, "GL_EXT_window_rectangles");
  ext.b_GL_IMG_multisampled_render_to_texture =
      gfx::HasExtension(extensions, "GL_IMG_multisampled_render_to_texture");
  ext.b_GL_INTEL_framebuffer_CMAA =
      gfx::HasExtension(extensions, "GL_INTEL_framebuffer_CMAA");
  ext.b_GL_KHR_blend_equation_advanced =
      gfx::HasExtension(extensions, "GL_KHR_blend_equation_advanced");
  ext.b_GL_KHR_debug = gfx::HasExtension(extensions, "GL_KHR_debug");
  ext.b_GL_KHR_parallel_shader_compile =
      gfx::HasExtension(extensions, "GL_KHR_parallel_shader_compile");
  ext.b_GL_KHR_robustness = gfx::HasExtension(extensions, "GL_KHR_robustness");
  ext.b_GL_NV_blend_equation_advanced =
      gfx::HasExtension(extensions, "GL_NV_blend_equation_advanced");
  ext.b_GL_NV_fence = gfx::HasExtension(extensions, "GL_NV_fence");
  ext.b_GL_NV_framebuffer_mixed_samples =
      gfx::HasExtension(extensions, "GL_NV_framebuffer_mixed_samples");
  ext.b_GL_NV_path_rendering =
      gfx::HasExtension(extensions, "GL_NV_path_rendering");
  ext.b_GL_OES_EGL_image = gfx::HasExtension(extensions, "GL_OES_EGL_image");
  ext.b_GL_OES_get_program_binary =
      gfx::HasExtension(extensions, "GL_OES_get_program_binary");
  ext.b_GL_OES_mapbuffer = gfx::HasExtension(extensions, "GL_OES_mapbuffer");
  ext.b_GL_OES_texture_buffer =
      gfx::HasExtension(extensions, "GL_OES_texture_buffer");
  ext.b_GL_OES_vertex_array_object =
      gfx::HasExtension(extensions, "GL_OES_vertex_array_object");

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glActiveShaderProgramFn = reinterpret_cast<glActiveShaderProgramProc>(
        GetGLProcAddress("glActiveShaderProgram"));
  }

  if (ext.b_GL_INTEL_framebuffer_CMAA) {
    fn.glApplyFramebufferAttachmentCMAAINTELFn =
        reinterpret_cast<glApplyFramebufferAttachmentCMAAINTELProc>(
            GetGLProcAddress("glApplyFramebufferAttachmentCMAAINTEL"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedbackEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBase"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBaseEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRange"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRangeEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocation"));
  } else if (ext.b_GL_EXT_gpu_shader4 || ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocationEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexed"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexedEXT"));
  }

  if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glBindFragmentInputLocationCHROMIUMFn =
        reinterpret_cast<glBindFragmentInputLocationCHROMIUMProc>(
            GetGLProcAddress("glBindFragmentInputLocationCHROMIUM"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebufferEXT"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTexture"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTextureEXT"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glBindProgramPipelineFn = reinterpret_cast<glBindProgramPipelineProc>(
        GetGLProcAddress("glBindProgramPipeline"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glBindSamplerFn =
        reinterpret_cast<glBindSamplerProc>(GetGLProcAddress("glBindSampler"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glBindTransformFeedbackFn =
        reinterpret_cast<glBindTransformFeedbackProc>(
            GetGLProcAddress("glBindTransformFeedback"));
  }

  if (ext.b_GL_CHROMIUM_bind_uniform_location) {
    fn.glBindUniformLocationCHROMIUMFn =
        reinterpret_cast<glBindUniformLocationCHROMIUMProc>(
            GetGLProcAddress("glBindUniformLocationCHROMIUM"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayAPPLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glBindVertexBufferFn = reinterpret_cast<glBindVertexBufferProc>(
        GetGLProcAddress("glBindVertexBuffer"));
  }

  if (ext.b_GL_NV_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierNV"));
  } else if (ext.b_GL_KHR_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierKHR"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_framebuffer_object) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebuffer"));
  } else if (ext.b_GL_ANGLE_framebuffer_blit) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebufferANGLE"));
  } else if (ext.b_GL_EXT_framebuffer_blit) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatus"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatusEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfiFn = reinterpret_cast<glClearBufferfiProc>(
        GetGLProcAddress("glClearBufferfi"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfvFn = reinterpret_cast<glClearBufferfvProc>(
        GetGLProcAddress("glClearBufferfv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferivFn = reinterpret_cast<glClearBufferivProc>(
        GetGLProcAddress("glClearBufferiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferuivFn = reinterpret_cast<glClearBufferuivProc>(
        GetGLProcAddress("glClearBufferuiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glClearDepthfFn =
        reinterpret_cast<glClearDepthfProc>(GetGLProcAddress("glClearDepthf"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glClientWaitSyncFn = reinterpret_cast<glClientWaitSyncProc>(
        GetGLProcAddress("glClientWaitSync"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexImage2DRobustANGLEFn =
        reinterpret_cast<glCompressedTexImage2DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexImage3DFn = reinterpret_cast<glCompressedTexImage3DProc>(
        GetGLProcAddress("glCompressedTexImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexImage3DRobustANGLEFn =
        reinterpret_cast<glCompressedTexImage3DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexImage3DRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexSubImage2DRobustANGLEFn =
        reinterpret_cast<glCompressedTexSubImage2DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexSubImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexSubImage3DFn =
        reinterpret_cast<glCompressedTexSubImage3DProc>(
            GetGLProcAddress("glCompressedTexSubImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexSubImage3DRobustANGLEFn =
        reinterpret_cast<glCompressedTexSubImage3DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexSubImage3DRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glCopyBufferSubDataFn = reinterpret_cast<glCopyBufferSubDataProc>(
        GetGLProcAddress("glCopyBufferSubData"));
  }

  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopySubTextureCHROMIUMFn =
        reinterpret_cast<glCopySubTextureCHROMIUMProc>(
            GetGLProcAddress("glCopySubTextureCHROMIUM"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCopyTexSubImage3DFn = reinterpret_cast<glCopyTexSubImage3DProc>(
        GetGLProcAddress("glCopyTexSubImage3D"));
  }

  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopyTextureCHROMIUMFn = reinterpret_cast<glCopyTextureCHROMIUMProc>(
        GetGLProcAddress("glCopyTextureCHROMIUM"));
  }

  if (ext.b_GL_NV_framebuffer_mixed_samples) {
    fn.glCoverageModulationNVFn = reinterpret_cast<glCoverageModulationNVProc>(
        GetGLProcAddress("glCoverageModulationNV"));
  } else if (ext.b_GL_CHROMIUM_framebuffer_mixed_samples) {
    fn.glCoverageModulationNVFn = reinterpret_cast<glCoverageModulationNVProc>(
        GetGLProcAddress("glCoverageModulationCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathInstancedNVFn =
        reinterpret_cast<glCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glCoverFillPathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glCoverFillPathInstancedNVFn =
        reinterpret_cast<glCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glCoverFillPathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathNVFn = reinterpret_cast<glCoverFillPathNVProc>(
        GetGLProcAddress("glCoverFillPathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glCoverFillPathNVFn = reinterpret_cast<glCoverFillPathNVProc>(
        GetGLProcAddress("glCoverFillPathCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathInstancedNVFn =
        reinterpret_cast<glCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glCoverStrokePathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glCoverStrokePathInstancedNVFn =
        reinterpret_cast<glCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glCoverStrokePathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathNVFn = reinterpret_cast<glCoverStrokePathNVProc>(
        GetGLProcAddress("glCoverStrokePathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glCoverStrokePathNVFn = reinterpret_cast<glCoverStrokePathNVProc>(
        GetGLProcAddress("glCoverStrokePathCHROMIUM"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glCreateShaderProgramvFn = reinterpret_cast<glCreateShaderProgramvProc>(
        GetGLProcAddress("glCreateShaderProgramv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallback"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallbackKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControl"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControlKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsert"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsertKHR"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glDeleteFencesAPPLEFn = reinterpret_cast<glDeleteFencesAPPLEProc>(
        GetGLProcAddress("glDeleteFencesAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glDeleteFencesNVFn = reinterpret_cast<glDeleteFencesNVProc>(
        GetGLProcAddress("glDeleteFencesNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffersEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glDeletePathsNVFn = reinterpret_cast<glDeletePathsNVProc>(
        GetGLProcAddress("glDeletePathsNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glDeletePathsNVFn = reinterpret_cast<glDeletePathsNVProc>(
        GetGLProcAddress("glDeletePathsCHROMIUM"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glDeleteProgramPipelinesFn =
        reinterpret_cast<glDeleteProgramPipelinesProc>(
            GetGLProcAddress("glDeleteProgramPipelines"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffersEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glDeleteSamplersFn = reinterpret_cast<glDeleteSamplersProc>(
        GetGLProcAddress("glDeleteSamplers"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glDeleteSyncFn =
        reinterpret_cast<glDeleteSyncProc>(GetGLProcAddress("glDeleteSync"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glDeleteTransformFeedbacksFn =
        reinterpret_cast<glDeleteTransformFeedbacksProc>(
            GetGLProcAddress("glDeleteTransformFeedbacks"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysAPPLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glDepthRangefFn =
        reinterpret_cast<glDepthRangefProc>(GetGLProcAddress("glDepthRangef"));
  }

  if (ext.b_GL_EXT_discard_framebuffer) {
    fn.glDiscardFramebufferEXTFn =
        reinterpret_cast<glDiscardFramebufferEXTProc>(
            GetGLProcAddress("glDiscardFramebufferEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glDispatchComputeFn = reinterpret_cast<glDispatchComputeProc>(
        GetGLProcAddress("glDispatchCompute"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glDispatchComputeIndirectFn =
        reinterpret_cast<glDispatchComputeIndirectProc>(
            GetGLProcAddress("glDispatchComputeIndirect"));
  }

  if (ver->IsAtLeastGLES(3u, 1u) || ver->IsAtLeastGL(4u, 0u)) {
    fn.glDrawArraysIndirectFn = reinterpret_cast<glDrawArraysIndirectProc>(
        GetGLProcAddress("glDrawArraysIndirect"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedANGLE"));
  }

  if (!ver->is_es) {
    fn.glDrawBufferFn =
        reinterpret_cast<glDrawBufferProc>(GetGLProcAddress("glDrawBuffer"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffers"));
  } else if (ext.b_GL_ARB_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersARB"));
  } else if (ext.b_GL_EXT_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersEXT"));
  }

  if (ver->IsAtLeastGLES(3u, 1u) || ver->IsAtLeastGL(4u, 0u)) {
    fn.glDrawElementsIndirectFn = reinterpret_cast<glDrawElementsIndirectProc>(
        GetGLProcAddress("glDrawElementsIndirect"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawRangeElementsFn = reinterpret_cast<glDrawRangeElementsProc>(
        GetGLProcAddress("glDrawRangeElements"));
  }

  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetRenderbufferStorageOESFn =
        reinterpret_cast<glEGLImageTargetRenderbufferStorageOESProc>(
            GetGLProcAddress("glEGLImageTargetRenderbufferStorageOES"));
  }

  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetTexture2DOESFn =
        reinterpret_cast<glEGLImageTargetTexture2DOESProc>(
            GetGLProcAddress("glEGLImageTargetTexture2DOES"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedbackEXT"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glFenceSyncFn =
        reinterpret_cast<glFenceSyncProc>(GetGLProcAddress("glFenceSync"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glFinishFenceAPPLEFn = reinterpret_cast<glFinishFenceAPPLEProc>(
        GetGLProcAddress("glFinishFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glFinishFenceNVFn = reinterpret_cast<glFinishFenceNVProc>(
        GetGLProcAddress("glFinishFenceNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_map_buffer_range) {
    fn.glFlushMappedBufferRangeFn =
        reinterpret_cast<glFlushMappedBufferRangeProc>(
            GetGLProcAddress("glFlushMappedBufferRange"));
  } else if (ext.b_GL_EXT_map_buffer_range) {
    fn.glFlushMappedBufferRangeFn =
        reinterpret_cast<glFlushMappedBufferRangeProc>(
            GetGLProcAddress("glFlushMappedBufferRangeEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glFramebufferParameteriFn =
        reinterpret_cast<glFramebufferParameteriProc>(
            GetGLProcAddress("glFramebufferParameteri"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2D"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DEXT"));
  }

  if (ext.b_GL_EXT_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleEXTFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleEXT"));
  } else if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleEXTFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleIMG"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glFramebufferTextureLayerFn =
        reinterpret_cast<glFramebufferTextureLayerProc>(
            GetGLProcAddress("glFramebufferTextureLayer"));
  }

  if (ext.b_GL_ANGLE_multiview) {
    fn.glFramebufferTextureMultiviewLayeredANGLEFn =
        reinterpret_cast<glFramebufferTextureMultiviewLayeredANGLEProc>(
            GetGLProcAddress("glFramebufferTextureMultiviewLayeredANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmap"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmapEXT"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glGenFencesAPPLEFn = reinterpret_cast<glGenFencesAPPLEProc>(
        GetGLProcAddress("glGenFencesAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glGenFencesNVFn =
        reinterpret_cast<glGenFencesNVProc>(GetGLProcAddress("glGenFencesNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffersEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glGenPathsNVFn =
        reinterpret_cast<glGenPathsNVProc>(GetGLProcAddress("glGenPathsNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glGenPathsNVFn = reinterpret_cast<glGenPathsNVProc>(
        GetGLProcAddress("glGenPathsCHROMIUM"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGenProgramPipelinesFn = reinterpret_cast<glGenProgramPipelinesProc>(
        GetGLProcAddress("glGenProgramPipelines"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffersEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGenSamplersFn =
        reinterpret_cast<glGenSamplersProc>(GetGLProcAddress("glGenSamplers"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glGenTransformFeedbacksFn =
        reinterpret_cast<glGenTransformFeedbacksProc>(
            GetGLProcAddress("glGenTransformFeedbacks"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysAPPLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockivFn =
        reinterpret_cast<glGetActiveUniformBlockivProc>(
            GetGLProcAddress("glGetActiveUniformBlockiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetActiveUniformBlockivRobustANGLEFn =
        reinterpret_cast<glGetActiveUniformBlockivRobustANGLEProc>(
            GetGLProcAddress("glGetActiveUniformBlockivRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockNameFn =
        reinterpret_cast<glGetActiveUniformBlockNameProc>(
            GetGLProcAddress("glGetActiveUniformBlockName"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformsivFn = reinterpret_cast<glGetActiveUniformsivProc>(
        GetGLProcAddress("glGetActiveUniformsiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetBooleani_vFn = reinterpret_cast<glGetBooleani_vProc>(
        GetGLProcAddress("glGetBooleani_v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleani_vRobustANGLEFn =
        reinterpret_cast<glGetBooleani_vRobustANGLEProc>(
            GetGLProcAddress("glGetBooleani_vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleanvRobustANGLEFn =
        reinterpret_cast<glGetBooleanvRobustANGLEProc>(
            GetGLProcAddress("glGetBooleanvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameteri64vRobustANGLEFn =
        reinterpret_cast<glGetBufferParameteri64vRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameteri64vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetBufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferPointervRobustANGLEFn =
        reinterpret_cast<glGetBufferPointervRobustANGLEProc>(
            GetGLProcAddress("glGetBufferPointervRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLog"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLogKHR"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glGetFenceivNVFn = reinterpret_cast<glGetFenceivNVProc>(
        GetGLProcAddress("glGetFenceivNV"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFloatvRobustANGLEFn = reinterpret_cast<glGetFloatvRobustANGLEProc>(
        GetGLProcAddress("glGetFloatvRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndex"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndexEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetFragDataLocationFn = reinterpret_cast<glGetFragDataLocationProc>(
        GetGLProcAddress("glGetFragDataLocation"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameterivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferAttachmentParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivRobustANGLEProc>(
            GetGLProcAddress(
                "glGetFramebufferAttachmentParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetFramebufferParameterivFn =
        reinterpret_cast<glGetFramebufferParameterivProc>(
            GetGLProcAddress("glGetFramebufferParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetFramebufferParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 5u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatus"));
  } else if (ext.b_GL_ARB_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusARB"));
  } else if (ext.b_GL_KHR_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusKHR"));
  } else if (ext.b_GL_EXT_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusEXT"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64i_vFn = reinterpret_cast<glGetInteger64i_vProc>(
        GetGLProcAddress("glGetInteger64i_v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64i_vRobustANGLEFn =
        reinterpret_cast<glGetInteger64i_vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64i_vRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64vFn = reinterpret_cast<glGetInteger64vProc>(
        GetGLProcAddress("glGetInteger64v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64vRobustANGLEFn =
        reinterpret_cast<glGetInteger64vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64vRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetIntegeri_vFn = reinterpret_cast<glGetIntegeri_vProc>(
        GetGLProcAddress("glGetIntegeri_v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegeri_vRobustANGLEFn =
        reinterpret_cast<glGetIntegeri_vRobustANGLEProc>(
            GetGLProcAddress("glGetIntegeri_vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegervRobustANGLEFn =
        reinterpret_cast<glGetIntegervRobustANGLEProc>(
            GetGLProcAddress("glGetIntegervRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_internalformat_query) {
    fn.glGetInternalformativFn = reinterpret_cast<glGetInternalformativProc>(
        GetGLProcAddress("glGetInternalformativ"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInternalformativRobustANGLEFn =
        reinterpret_cast<glGetInternalformativRobustANGLEProc>(
            GetGLProcAddress("glGetInternalformativRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_texture_multisample) {
    fn.glGetMultisamplefvFn = reinterpret_cast<glGetMultisamplefvProc>(
        GetGLProcAddress("glGetMultisamplefv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetMultisamplefvRobustANGLEFn =
        reinterpret_cast<glGetMultisamplefvRobustANGLEProc>(
            GetGLProcAddress("glGetMultisamplefvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformfvRobustANGLEFn =
        reinterpret_cast<glGetnUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformivRobustANGLEFn =
        reinterpret_cast<glGetnUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformuivRobustANGLEFn =
        reinterpret_cast<glGetnUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabelKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabelKHR"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetPointervFn =
        reinterpret_cast<glGetPointervProc>(GetGLProcAddress("glGetPointerv"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetPointervFn = reinterpret_cast<glGetPointervProc>(
        GetGLProcAddress("glGetPointervKHR"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetPointervRobustANGLERobustANGLEFn =
        reinterpret_cast<glGetPointervRobustANGLERobustANGLEProc>(
            GetGLProcAddress("glGetPointervRobustANGLERobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinaryOES"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramInterfaceivFn =
        reinterpret_cast<glGetProgramInterfaceivProc>(
            GetGLProcAddress("glGetProgramInterfaceiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramInterfaceivRobustANGLEFn =
        reinterpret_cast<glGetProgramInterfaceivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramInterfaceivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramivRobustANGLEFn =
        reinterpret_cast<glGetProgramivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramPipelineInfoLogFn =
        reinterpret_cast<glGetProgramPipelineInfoLogProc>(
            GetGLProcAddress("glGetProgramPipelineInfoLog"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramPipelineivFn = reinterpret_cast<glGetProgramPipelineivProc>(
        GetGLProcAddress("glGetProgramPipelineiv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramResourceIndexFn =
        reinterpret_cast<glGetProgramResourceIndexProc>(
            GetGLProcAddress("glGetProgramResourceIndex"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceivFn = reinterpret_cast<glGetProgramResourceivProc>(
        GetGLProcAddress("glGetProgramResourceiv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramResourceLocationFn =
        reinterpret_cast<glGetProgramResourceLocationProc>(
            GetGLProcAddress("glGetProgramResourceLocation"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceNameFn =
        reinterpret_cast<glGetProgramResourceNameProc>(
            GetGLProcAddress("glGetProgramResourceName"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryivRobustANGLEFn =
        reinterpret_cast<glGetQueryivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64vEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjecti64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjecti64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjecti64vRobustANGLE"));
  }

  if (!ver->is_es) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64vEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectui64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectui64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectui64vRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectuivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectuivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameterivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetRenderbufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetRenderbufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetRenderbufferParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterfvFn =
        reinterpret_cast<glGetSamplerParameterfvProc>(
            GetGLProcAddress("glGetSamplerParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterivFn =
        reinterpret_cast<glGetSamplerParameterivProc>(
            GetGLProcAddress("glGetSamplerParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetShaderivRobustANGLEFn =
        reinterpret_cast<glGetShaderivRobustANGLEProc>(
            GetGLProcAddress("glGetShaderivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glGetShaderPrecisionFormatFn =
        reinterpret_cast<glGetShaderPrecisionFormatProc>(
            GetGLProcAddress("glGetShaderPrecisionFormat"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glGetSyncivFn =
        reinterpret_cast<glGetSyncivProc>(GetGLProcAddress("glGetSynciv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterfvFn =
        reinterpret_cast<glGetTexLevelParameterfvProc>(
            GetGLProcAddress("glGetTexLevelParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterfvRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterivFn =
        reinterpret_cast<glGetTexLevelParameterivProc>(
            GetGLProcAddress("glGetTexLevelParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVarying"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVaryingEXT"));
  }

  if (ext.b_GL_ANGLE_translated_shader_source) {
    fn.glGetTranslatedShaderSourceANGLEFn =
        reinterpret_cast<glGetTranslatedShaderSourceANGLEProc>(
            GetGLProcAddress("glGetTranslatedShaderSourceANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformBlockIndexFn = reinterpret_cast<glGetUniformBlockIndexProc>(
        GetGLProcAddress("glGetUniformBlockIndex"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformfvRobustANGLEFn =
        reinterpret_cast<glGetUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetUniformfvRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformIndicesFn = reinterpret_cast<glGetUniformIndicesProc>(
        GetGLProcAddress("glGetUniformIndices"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformivRobustANGLEFn =
        reinterpret_cast<glGetUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetUniformuivFn = reinterpret_cast<glGetUniformuivProc>(
        GetGLProcAddress("glGetUniformuiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformuivRobustANGLEFn =
        reinterpret_cast<glGetUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribfvRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribfvRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIuivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIuivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribPointervRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribPointervRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribPointervRobustANGLE"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glInsertEventMarkerEXTFn = reinterpret_cast<glInsertEventMarkerEXTProc>(
        GetGLProcAddress("glInsertEventMarkerEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateFramebufferFn =
        reinterpret_cast<glInvalidateFramebufferProc>(
            GetGLProcAddress("glInvalidateFramebuffer"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateSubFramebufferFn =
        reinterpret_cast<glInvalidateSubFramebufferProc>(
            GetGLProcAddress("glInvalidateSubFramebuffer"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glIsFenceAPPLEFn = reinterpret_cast<glIsFenceAPPLEProc>(
        GetGLProcAddress("glIsFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glIsFenceNVFn =
        reinterpret_cast<glIsFenceNVProc>(GetGLProcAddress("glIsFenceNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebufferEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glIsPathNVFn =
        reinterpret_cast<glIsPathNVProc>(GetGLProcAddress("glIsPathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glIsPathNVFn =
        reinterpret_cast<glIsPathNVProc>(GetGLProcAddress("glIsPathCHROMIUM"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glIsProgramPipelineFn = reinterpret_cast<glIsProgramPipelineProc>(
        GetGLProcAddress("glIsProgramPipeline"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glIsSamplerFn =
        reinterpret_cast<glIsSamplerProc>(GetGLProcAddress("glIsSampler"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glIsSyncFn =
        reinterpret_cast<glIsSyncProc>(GetGLProcAddress("glIsSync"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glIsTransformFeedbackFn = reinterpret_cast<glIsTransformFeedbackProc>(
        GetGLProcAddress("glIsTransformFeedback"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayAPPLE"));
  }

  if (!ver->is_es) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBufferOES"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRange"));
  } else if (ext.b_GL_EXT_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRangeEXT"));
  }

  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadfEXTFn = reinterpret_cast<glMatrixLoadfEXTProc>(
        GetGLProcAddress("glMatrixLoadfEXT"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glMatrixLoadfEXTFn = reinterpret_cast<glMatrixLoadfEXTProc>(
        GetGLProcAddress("glMatrixLoadfCHROMIUM"));
  }

  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadIdentityEXTFn =
        reinterpret_cast<glMatrixLoadIdentityEXTProc>(
            GetGLProcAddress("glMatrixLoadIdentityEXT"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glMatrixLoadIdentityEXTFn =
        reinterpret_cast<glMatrixLoadIdentityEXTProc>(
            GetGLProcAddress("glMatrixLoadIdentityCHROMIUM"));
  }

  if (ext.b_GL_KHR_parallel_shader_compile) {
    fn.glMaxShaderCompilerThreadsKHRFn =
        reinterpret_cast<glMaxShaderCompilerThreadsKHRProc>(
            GetGLProcAddress("glMaxShaderCompilerThreadsKHR"));
  }

  if (ver->IsAtLeastGLES(3u, 1u) || ver->IsAtLeastGL(4u, 5u)) {
    fn.glMemoryBarrierByRegionFn =
        reinterpret_cast<glMemoryBarrierByRegionProc>(
            GetGLProcAddress("glMemoryBarrierByRegion"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrier"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrierEXT"));
  }

  if (ver->IsAtLeastGLES(3u, 2u) || ver->IsAtLeastGL(4u, 0u)) {
    fn.glMinSampleShadingFn = reinterpret_cast<glMinSampleShadingProc>(
        GetGLProcAddress("glMinSampleShading"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectLabelFn =
        reinterpret_cast<glObjectLabelProc>(GetGLProcAddress("glObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectLabelFn = reinterpret_cast<glObjectLabelProc>(
        GetGLProcAddress("glObjectLabelKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabelKHR"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathCommandsNVFn = reinterpret_cast<glPathCommandsNVProc>(
        GetGLProcAddress("glPathCommandsNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glPathCommandsNVFn = reinterpret_cast<glPathCommandsNVProc>(
        GetGLProcAddress("glPathCommandsCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameterfNVFn = reinterpret_cast<glPathParameterfNVProc>(
        GetGLProcAddress("glPathParameterfNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glPathParameterfNVFn = reinterpret_cast<glPathParameterfNVProc>(
        GetGLProcAddress("glPathParameterfCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameteriNVFn = reinterpret_cast<glPathParameteriNVProc>(
        GetGLProcAddress("glPathParameteriNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glPathParameteriNVFn = reinterpret_cast<glPathParameteriNVProc>(
        GetGLProcAddress("glPathParameteriCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathStencilFuncNVFn = reinterpret_cast<glPathStencilFuncNVProc>(
        GetGLProcAddress("glPathStencilFuncNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glPathStencilFuncNVFn = reinterpret_cast<glPathStencilFuncNVProc>(
        GetGLProcAddress("glPathStencilFuncCHROMIUM"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glPauseTransformFeedbackFn =
        reinterpret_cast<glPauseTransformFeedbackProc>(
            GetGLProcAddress("glPauseTransformFeedback"));
  }

  if (!ver->is_es) {
    fn.glPointParameteriFn = reinterpret_cast<glPointParameteriProc>(
        GetGLProcAddress("glPointParameteri"));
  }

  if (!ver->is_es) {
    fn.glPolygonModeFn =
        reinterpret_cast<glPolygonModeProc>(GetGLProcAddress("glPolygonMode"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroupKHR"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glPopGroupMarkerEXTFn = reinterpret_cast<glPopGroupMarkerEXTProc>(
        GetGLProcAddress("glPopGroupMarkerEXT"));
  }

  if (ver->IsAtLeastGL(3u, 1u)) {
    fn.glPrimitiveRestartIndexFn =
        reinterpret_cast<glPrimitiveRestartIndexProc>(
            GetGLProcAddress("glPrimitiveRestartIndex"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinaryOES"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramParameteriFn = reinterpret_cast<glProgramParameteriProc>(
        GetGLProcAddress("glProgramParameteri"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glProgramPathFragmentInputGenNVFn =
        reinterpret_cast<glProgramPathFragmentInputGenNVProc>(
            GetGLProcAddress("glProgramPathFragmentInputGenNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glProgramPathFragmentInputGenNVFn =
        reinterpret_cast<glProgramPathFragmentInputGenNVProc>(
            GetGLProcAddress("glProgramPathFragmentInputGenCHROMIUM"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1fFn = reinterpret_cast<glProgramUniform1fProc>(
        GetGLProcAddress("glProgramUniform1f"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1fvFn = reinterpret_cast<glProgramUniform1fvProc>(
        GetGLProcAddress("glProgramUniform1fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1iFn = reinterpret_cast<glProgramUniform1iProc>(
        GetGLProcAddress("glProgramUniform1i"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1ivFn = reinterpret_cast<glProgramUniform1ivProc>(
        GetGLProcAddress("glProgramUniform1iv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1uiFn = reinterpret_cast<glProgramUniform1uiProc>(
        GetGLProcAddress("glProgramUniform1ui"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform1uivFn = reinterpret_cast<glProgramUniform1uivProc>(
        GetGLProcAddress("glProgramUniform1uiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2fFn = reinterpret_cast<glProgramUniform2fProc>(
        GetGLProcAddress("glProgramUniform2f"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2fvFn = reinterpret_cast<glProgramUniform2fvProc>(
        GetGLProcAddress("glProgramUniform2fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2iFn = reinterpret_cast<glProgramUniform2iProc>(
        GetGLProcAddress("glProgramUniform2i"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2ivFn = reinterpret_cast<glProgramUniform2ivProc>(
        GetGLProcAddress("glProgramUniform2iv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2uiFn = reinterpret_cast<glProgramUniform2uiProc>(
        GetGLProcAddress("glProgramUniform2ui"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform2uivFn = reinterpret_cast<glProgramUniform2uivProc>(
        GetGLProcAddress("glProgramUniform2uiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3fFn = reinterpret_cast<glProgramUniform3fProc>(
        GetGLProcAddress("glProgramUniform3f"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3fvFn = reinterpret_cast<glProgramUniform3fvProc>(
        GetGLProcAddress("glProgramUniform3fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3iFn = reinterpret_cast<glProgramUniform3iProc>(
        GetGLProcAddress("glProgramUniform3i"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3ivFn = reinterpret_cast<glProgramUniform3ivProc>(
        GetGLProcAddress("glProgramUniform3iv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3uiFn = reinterpret_cast<glProgramUniform3uiProc>(
        GetGLProcAddress("glProgramUniform3ui"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform3uivFn = reinterpret_cast<glProgramUniform3uivProc>(
        GetGLProcAddress("glProgramUniform3uiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4fFn = reinterpret_cast<glProgramUniform4fProc>(
        GetGLProcAddress("glProgramUniform4f"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4fvFn = reinterpret_cast<glProgramUniform4fvProc>(
        GetGLProcAddress("glProgramUniform4fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4iFn = reinterpret_cast<glProgramUniform4iProc>(
        GetGLProcAddress("glProgramUniform4i"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4ivFn = reinterpret_cast<glProgramUniform4ivProc>(
        GetGLProcAddress("glProgramUniform4iv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4uiFn = reinterpret_cast<glProgramUniform4uiProc>(
        GetGLProcAddress("glProgramUniform4ui"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniform4uivFn = reinterpret_cast<glProgramUniform4uivProc>(
        GetGLProcAddress("glProgramUniform4uiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix2fvFn =
        reinterpret_cast<glProgramUniformMatrix2fvProc>(
            GetGLProcAddress("glProgramUniformMatrix2fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix2x3fvFn =
        reinterpret_cast<glProgramUniformMatrix2x3fvProc>(
            GetGLProcAddress("glProgramUniformMatrix2x3fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix2x4fvFn =
        reinterpret_cast<glProgramUniformMatrix2x4fvProc>(
            GetGLProcAddress("glProgramUniformMatrix2x4fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix3fvFn =
        reinterpret_cast<glProgramUniformMatrix3fvProc>(
            GetGLProcAddress("glProgramUniformMatrix3fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix3x2fvFn =
        reinterpret_cast<glProgramUniformMatrix3x2fvProc>(
            GetGLProcAddress("glProgramUniformMatrix3x2fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix3x4fvFn =
        reinterpret_cast<glProgramUniformMatrix3x4fvProc>(
            GetGLProcAddress("glProgramUniformMatrix3x4fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix4fvFn =
        reinterpret_cast<glProgramUniformMatrix4fvProc>(
            GetGLProcAddress("glProgramUniformMatrix4fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix4x2fvFn =
        reinterpret_cast<glProgramUniformMatrix4x2fvProc>(
            GetGLProcAddress("glProgramUniformMatrix4x2fv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glProgramUniformMatrix4x3fvFn =
        reinterpret_cast<glProgramUniformMatrix4x3fvProc>(
            GetGLProcAddress("glProgramUniformMatrix4x3fv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroupKHR"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glPushGroupMarkerEXTFn = reinterpret_cast<glPushGroupMarkerEXTProc>(
        GetGLProcAddress("glPushGroupMarkerEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounter"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounterEXT"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glReadBufferFn =
        reinterpret_cast<glReadBufferProc>(GetGLProcAddress("glReadBuffer"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadnPixelsRobustANGLEFn =
        reinterpret_cast<glReadnPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadnPixelsRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadPixelsRobustANGLEFn =
        reinterpret_cast<glReadPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadPixelsRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glReleaseShaderCompilerFn =
        reinterpret_cast<glReleaseShaderCompilerProc>(
            GetGLProcAddress("glReleaseShaderCompiler"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorage"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorageEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_framebuffer_object) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisample"));
  } else if (ext.b_GL_ANGLE_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleANGLE"));
  } else if (ext.b_GL_EXT_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleEXT"));
  }

  if (ext.b_GL_EXT_multisampled_render_to_texture) {
    fn.glRenderbufferStorageMultisampleEXTFn =
        reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleEXT"));
  } else if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glRenderbufferStorageMultisampleEXTFn =
        reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleIMG"));
  }

  if (ext.b_GL_ANGLE_request_extension) {
    fn.glRequestExtensionANGLEFn =
        reinterpret_cast<glRequestExtensionANGLEProc>(
            GetGLProcAddress("glRequestExtensionANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glResumeTransformFeedbackFn =
        reinterpret_cast<glResumeTransformFeedbackProc>(
            GetGLProcAddress("glResumeTransformFeedback"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glSampleMaskiFn =
        reinterpret_cast<glSampleMaskiProc>(GetGLProcAddress("glSampleMaski"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfFn = reinterpret_cast<glSamplerParameterfProc>(
        GetGLProcAddress("glSamplerParameterf"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfvFn = reinterpret_cast<glSamplerParameterfvProc>(
        GetGLProcAddress("glSamplerParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterfvRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameteriFn = reinterpret_cast<glSamplerParameteriProc>(
        GetGLProcAddress("glSamplerParameteri"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterivFn = reinterpret_cast<glSamplerParameterivProc>(
        GetGLProcAddress("glSamplerParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterivRobustANGLE"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glSetFenceAPPLEFn = reinterpret_cast<glSetFenceAPPLEProc>(
        GetGLProcAddress("glSetFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glSetFenceNVFn =
        reinterpret_cast<glSetFenceNVProc>(GetGLProcAddress("glSetFenceNV"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glShaderBinaryFn = reinterpret_cast<glShaderBinaryProc>(
        GetGLProcAddress("glShaderBinary"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathInstancedNVFn =
        reinterpret_cast<glStencilFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilFillPathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilFillPathInstancedNVFn =
        reinterpret_cast<glStencilFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilFillPathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathNVFn = reinterpret_cast<glStencilFillPathNVProc>(
        GetGLProcAddress("glStencilFillPathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilFillPathNVFn = reinterpret_cast<glStencilFillPathNVProc>(
        GetGLProcAddress("glStencilFillPathCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathInstancedNVFn =
        reinterpret_cast<glStencilStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilStrokePathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilStrokePathInstancedNVFn =
        reinterpret_cast<glStencilStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilStrokePathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathNVFn = reinterpret_cast<glStencilStrokePathNVProc>(
        GetGLProcAddress("glStencilStrokePathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilStrokePathNVFn = reinterpret_cast<glStencilStrokePathNVProc>(
        GetGLProcAddress("glStencilStrokePathCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilThenCoverFillPathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathNVFn =
        reinterpret_cast<glStencilThenCoverFillPathNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilThenCoverFillPathNVFn =
        reinterpret_cast<glStencilThenCoverFillPathNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathInstancedNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilThenCoverStrokePathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathInstancedCHROMIUM"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathNV"));
  } else if (ext.b_GL_CHROMIUM_path_rendering) {
    fn.glStencilThenCoverStrokePathNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathCHROMIUM"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glTestFenceAPPLEFn = reinterpret_cast<glTestFenceAPPLEProc>(
        GetGLProcAddress("glTestFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glTestFenceNVFn =
        reinterpret_cast<glTestFenceNVProc>(GetGLProcAddress("glTestFenceNV"));
  }

  if (ver->IsAtLeastGLES(3u, 2u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBuffer"));
  } else if (ext.b_GL_OES_texture_buffer) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBufferOES"));
  } else if (ext.b_GL_EXT_texture_buffer_object ||
             ext.b_GL_EXT_texture_buffer) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBufferEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRange"));
  } else if (ext.b_GL_OES_texture_buffer) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRangeOES"));
  } else if (ext.b_GL_EXT_texture_buffer) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRangeEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage2DRobustANGLEFn =
        reinterpret_cast<glTexImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexImage3DFn =
        reinterpret_cast<glTexImage3DProc>(GetGLProcAddress("glTexImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage3DRobustANGLEFn =
        reinterpret_cast<glTexImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexImage3DRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterfvRobustANGLEFn =
        reinterpret_cast<glTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glTexParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIivRobustANGLEFn =
        reinterpret_cast<glTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterivRobustANGLEFn =
        reinterpret_cast<glTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2D"));
  } else if (ext.b_GL_EXT_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2DEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glTexStorage2DMultisampleFn =
        reinterpret_cast<glTexStorage2DMultisampleProc>(
            GetGLProcAddress("glTexStorage2DMultisample"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage3DFn = reinterpret_cast<glTexStorage3DProc>(
        GetGLProcAddress("glTexStorage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage2DRobustANGLEFn =
        reinterpret_cast<glTexSubImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexSubImage3DFn = reinterpret_cast<glTexSubImage3DProc>(
        GetGLProcAddress("glTexSubImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage3DRobustANGLEFn =
        reinterpret_cast<glTexSubImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage3DRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryings"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryingsEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uiFn =
        reinterpret_cast<glUniform1uiProc>(GetGLProcAddress("glUniform1ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uivFn =
        reinterpret_cast<glUniform1uivProc>(GetGLProcAddress("glUniform1uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uiFn =
        reinterpret_cast<glUniform2uiProc>(GetGLProcAddress("glUniform2ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uivFn =
        reinterpret_cast<glUniform2uivProc>(GetGLProcAddress("glUniform2uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uiFn =
        reinterpret_cast<glUniform3uiProc>(GetGLProcAddress("glUniform3ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uivFn =
        reinterpret_cast<glUniform3uivProc>(GetGLProcAddress("glUniform3uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uiFn =
        reinterpret_cast<glUniform4uiProc>(GetGLProcAddress("glUniform4ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uivFn =
        reinterpret_cast<glUniform4uivProc>(GetGLProcAddress("glUniform4uiv"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glUniformBlockBindingFn = reinterpret_cast<glUniformBlockBindingProc>(
        GetGLProcAddress("glUniformBlockBinding"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x3fvFn = reinterpret_cast<glUniformMatrix2x3fvProc>(
        GetGLProcAddress("glUniformMatrix2x3fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x4fvFn = reinterpret_cast<glUniformMatrix2x4fvProc>(
        GetGLProcAddress("glUniformMatrix2x4fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x2fvFn = reinterpret_cast<glUniformMatrix3x2fvProc>(
        GetGLProcAddress("glUniformMatrix3x2fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x4fvFn = reinterpret_cast<glUniformMatrix3x4fvProc>(
        GetGLProcAddress("glUniformMatrix3x4fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x2fvFn = reinterpret_cast<glUniformMatrix4x2fvProc>(
        GetGLProcAddress("glUniformMatrix4x2fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x3fvFn = reinterpret_cast<glUniformMatrix4x3fvProc>(
        GetGLProcAddress("glUniformMatrix4x3fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUnmapBufferFn =
        reinterpret_cast<glUnmapBufferProc>(GetGLProcAddress("glUnmapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glUnmapBufferFn = reinterpret_cast<glUnmapBufferProc>(
        GetGLProcAddress("glUnmapBufferOES"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glUseProgramStagesFn = reinterpret_cast<glUseProgramStagesProc>(
        GetGLProcAddress("glUseProgramStages"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glValidateProgramPipelineFn =
        reinterpret_cast<glValidateProgramPipelineProc>(
            GetGLProcAddress("glValidateProgramPipeline"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glVertexAttribBindingFn = reinterpret_cast<glVertexAttribBindingProc>(
        GetGLProcAddress("glVertexAttribBinding"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisor"));
  } else if (ext.b_GL_ARB_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorANGLE"));
  } else if (ext.b_GL_EXT_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glVertexAttribFormatFn = reinterpret_cast<glVertexAttribFormatProc>(
        GetGLProcAddress("glVertexAttribFormat"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4iFn = reinterpret_cast<glVertexAttribI4iProc>(
        GetGLProcAddress("glVertexAttribI4i"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4ivFn = reinterpret_cast<glVertexAttribI4ivProc>(
        GetGLProcAddress("glVertexAttribI4iv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uiFn = reinterpret_cast<glVertexAttribI4uiProc>(
        GetGLProcAddress("glVertexAttribI4ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uivFn = reinterpret_cast<glVertexAttribI4uivProc>(
        GetGLProcAddress("glVertexAttribI4uiv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glVertexAttribIFormatFn = reinterpret_cast<glVertexAttribIFormatProc>(
        GetGLProcAddress("glVertexAttribIFormat"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribIPointerFn = reinterpret_cast<glVertexAttribIPointerProc>(
        GetGLProcAddress("glVertexAttribIPointer"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glVertexBindingDivisorFn = reinterpret_cast<glVertexBindingDivisorProc>(
        GetGLProcAddress("glVertexBindingDivisor"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glWaitSyncFn =
        reinterpret_cast<glWaitSyncProc>(GetGLProcAddress("glWaitSync"));
  }

  if (ext.b_GL_EXT_window_rectangles) {
    fn.glWindowRectanglesEXTFn = reinterpret_cast<glWindowRectanglesEXTProc>(
        GetGLProcAddress("glWindowRectanglesEXT"));
  }
}

void DriverGL::ClearBindings() {
  memset(this, 0, sizeof(*this));
}

void GLApiBase::glActiveShaderProgramFn(GLuint pipeline, GLuint program) {
  driver_->fn.glActiveShaderProgramFn(pipeline, program);
}

void GLApiBase::glActiveTextureFn(GLenum texture) {
  driver_->fn.glActiveTextureFn(texture);
}

void GLApiBase::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  driver_->fn.glApplyFramebufferAttachmentCMAAINTELFn();
}

void GLApiBase::glAttachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glAttachShaderFn(program, shader);
}

void GLApiBase::glBeginQueryFn(GLenum target, GLuint id) {
  driver_->fn.glBeginQueryFn(target, id);
}

void GLApiBase::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  driver_->fn.glBeginTransformFeedbackFn(primitiveMode);
}

void GLApiBase::glBindAttribLocationFn(GLuint program,
                                       GLuint index,
                                       const char* name) {
  driver_->fn.glBindAttribLocationFn(program, index, name);
}

void GLApiBase::glBindBufferFn(GLenum target, GLuint buffer) {
  driver_->fn.glBindBufferFn(target, buffer);
}

void GLApiBase::glBindBufferBaseFn(GLenum target, GLuint index, GLuint buffer) {
  driver_->fn.glBindBufferBaseFn(target, index, buffer);
}

void GLApiBase::glBindBufferRangeFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  driver_->fn.glBindBufferRangeFn(target, index, buffer, offset, size);
}

void GLApiBase::glBindFragDataLocationFn(GLuint program,
                                         GLuint colorNumber,
                                         const char* name) {
  driver_->fn.glBindFragDataLocationFn(program, colorNumber, name);
}

void GLApiBase::glBindFragDataLocationIndexedFn(GLuint program,
                                                GLuint colorNumber,
                                                GLuint index,
                                                const char* name) {
  driver_->fn.glBindFragDataLocationIndexedFn(program, colorNumber, index,
                                              name);
}

void GLApiBase::glBindFragmentInputLocationCHROMIUMFn(GLuint program,
                                                      GLint location,
                                                      const char* name) {
  driver_->fn.glBindFragmentInputLocationCHROMIUMFn(program, location, name);
}

void GLApiBase::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  driver_->fn.glBindFramebufferEXTFn(target, framebuffer);
}

void GLApiBase::glBindImageTextureEXTFn(GLuint index,
                                        GLuint texture,
                                        GLint level,
                                        GLboolean layered,
                                        GLint layer,
                                        GLenum access,
                                        GLint format) {
  driver_->fn.glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                      access, format);
}

void GLApiBase::glBindProgramPipelineFn(GLuint pipeline) {
  driver_->fn.glBindProgramPipelineFn(pipeline);
}

void GLApiBase::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  driver_->fn.glBindRenderbufferEXTFn(target, renderbuffer);
}

void GLApiBase::glBindSamplerFn(GLuint unit, GLuint sampler) {
  driver_->fn.glBindSamplerFn(unit, sampler);
}

void GLApiBase::glBindTextureFn(GLenum target, GLuint texture) {
  driver_->fn.glBindTextureFn(target, texture);
}

void GLApiBase::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  driver_->fn.glBindTransformFeedbackFn(target, id);
}

void GLApiBase::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                GLint location,
                                                const char* name) {
  driver_->fn.glBindUniformLocationCHROMIUMFn(program, location, name);
}

void GLApiBase::glBindVertexArrayOESFn(GLuint array) {
  driver_->fn.glBindVertexArrayOESFn(array);
}

void GLApiBase::glBindVertexBufferFn(GLuint bindingindex,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizei stride) {
  driver_->fn.glBindVertexBufferFn(bindingindex, buffer, offset, stride);
}

void GLApiBase::glBlendBarrierKHRFn(void) {
  driver_->fn.glBlendBarrierKHRFn();
}

void GLApiBase::glBlendColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glBlendColorFn(red, green, blue, alpha);
}

void GLApiBase::glBlendEquationFn(GLenum mode) {
  driver_->fn.glBlendEquationFn(mode);
}

void GLApiBase::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  driver_->fn.glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void GLApiBase::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  driver_->fn.glBlendFuncFn(sfactor, dfactor);
}

void GLApiBase::glBlendFuncSeparateFn(GLenum srcRGB,
                                      GLenum dstRGB,
                                      GLenum srcAlpha,
                                      GLenum dstAlpha) {
  driver_->fn.glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLApiBase::glBlitFramebufferFn(GLint srcX0,
                                    GLint srcY0,
                                    GLint srcX1,
                                    GLint srcY1,
                                    GLint dstX0,
                                    GLint dstY0,
                                    GLint dstX1,
                                    GLint dstY1,
                                    GLbitfield mask,
                                    GLenum filter) {
  driver_->fn.glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                  dstX1, dstY1, mask, filter);
}

void GLApiBase::glBufferDataFn(GLenum target,
                               GLsizeiptr size,
                               const void* data,
                               GLenum usage) {
  driver_->fn.glBufferDataFn(target, size, data, usage);
}

void GLApiBase::glBufferSubDataFn(GLenum target,
                                  GLintptr offset,
                                  GLsizeiptr size,
                                  const void* data) {
  driver_->fn.glBufferSubDataFn(target, offset, size, data);
}

GLenum GLApiBase::glCheckFramebufferStatusEXTFn(GLenum target) {
  return driver_->fn.glCheckFramebufferStatusEXTFn(target);
}

void GLApiBase::glClearFn(GLbitfield mask) {
  driver_->fn.glClearFn(mask);
}

void GLApiBase::glClearBufferfiFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat depth,
                                  GLint stencil) {
  driver_->fn.glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void GLApiBase::glClearBufferfvFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat* value) {
  driver_->fn.glClearBufferfvFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearBufferivFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLint* value) {
  driver_->fn.glClearBufferivFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearBufferuivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLuint* value) {
  driver_->fn.glClearBufferuivFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glClearColorFn(red, green, blue, alpha);
}

void GLApiBase::glClearDepthFn(GLclampd depth) {
  driver_->fn.glClearDepthFn(depth);
}

void GLApiBase::glClearDepthfFn(GLclampf depth) {
  driver_->fn.glClearDepthfFn(depth);
}

void GLApiBase::glClearStencilFn(GLint s) {
  driver_->fn.glClearStencilFn(s);
}

GLenum GLApiBase::glClientWaitSyncFn(GLsync sync,
                                     GLbitfield flags,
                                     GLuint64 timeout) {
  return driver_->fn.glClientWaitSyncFn(sync, flags, timeout);
}

void GLApiBase::glColorMaskFn(GLboolean red,
                              GLboolean green,
                              GLboolean blue,
                              GLboolean alpha) {
  driver_->fn.glColorMaskFn(red, green, blue, alpha);
}

void GLApiBase::glCompileShaderFn(GLuint shader) {
  driver_->fn.glCompileShaderFn(shader);
}

void GLApiBase::glCompressedTexImage2DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage2DFn(target, level, internalformat, width,
                                       height, border, imageSize, data);
}

void GLApiBase::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const void* data) {
  driver_->fn.glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                                  width, height, border,
                                                  imageSize, dataSize, data);
}

void GLApiBase::glCompressedTexImage3DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage3DFn(target, level, internalformat, width,
                                       height, depth, border, imageSize, data);
}

void GLApiBase::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const void* data) {
  driver_->fn.glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                                  width, height, depth, border,
                                                  imageSize, dataSize, data);
}

void GLApiBase::glCompressedTexSubImage2DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void GLApiBase::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLsizei imageSize,
                                                       GLsizei dataSize,
                                                       const void* data) {
  driver_->fn.glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

void GLApiBase::glCompressedTexSubImage3DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage3DFn(target, level, xoffset, yoffset,
                                          zoffset, width, height, depth, format,
                                          imageSize, data);
}

void GLApiBase::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLenum format,
                                                       GLsizei imageSize,
                                                       GLsizei dataSize,
                                                       const void* data) {
  driver_->fn.glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

void GLApiBase::glCopyBufferSubDataFn(GLenum readTarget,
                                      GLenum writeTarget,
                                      GLintptr readOffset,
                                      GLintptr writeOffset,
                                      GLsizeiptr size) {
  driver_->fn.glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                    writeOffset, size);
}

void GLApiBase::glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                           GLint sourceLevel,
                                           GLenum destTarget,
                                           GLuint destId,
                                           GLint destLevel,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLboolean unpackFlipY,
                                           GLboolean unpackPremultiplyAlpha,
                                           GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

void GLApiBase::glCopyTexImage2DFn(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border) {
  driver_->fn.glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                                 height, border);
}

void GLApiBase::glCopyTexSubImage2DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y,
                                    width, height);
}

void GLApiBase::glCopyTexSubImage3DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x,
                                    y, width, height);
}

void GLApiBase::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                        GLint sourceLevel,
                                        GLenum destTarget,
                                        GLuint destId,
                                        GLint destLevel,
                                        GLint internalFormat,
                                        GLenum destType,
                                        GLboolean unpackFlipY,
                                        GLboolean unpackPremultiplyAlpha,
                                        GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void GLApiBase::glCoverageModulationNVFn(GLenum components) {
  driver_->fn.glCoverageModulationNVFn(components);
}

void GLApiBase::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                             GLenum pathNameType,
                                             const void* paths,
                                             GLuint pathBase,
                                             GLenum coverMode,
                                             GLenum transformType,
                                             const GLfloat* transformValues) {
  driver_->fn.glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                           pathBase, coverMode, transformType,
                                           transformValues);
}

void GLApiBase::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  driver_->fn.glCoverFillPathNVFn(path, coverMode);
}

void GLApiBase::glCoverStrokePathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum coverMode,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, coverMode, transformType,
                                             transformValues);
}

void GLApiBase::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  driver_->fn.glCoverStrokePathNVFn(name, coverMode);
}

GLuint GLApiBase::glCreateProgramFn(void) {
  return driver_->fn.glCreateProgramFn();
}

GLuint GLApiBase::glCreateShaderFn(GLenum type) {
  return driver_->fn.glCreateShaderFn(type);
}

GLuint GLApiBase::glCreateShaderProgramvFn(GLenum type,
                                           GLsizei count,
                                           const char* const* strings) {
  return driver_->fn.glCreateShaderProgramvFn(type, count, strings);
}

void GLApiBase::glCullFaceFn(GLenum mode) {
  driver_->fn.glCullFaceFn(mode);
}

void GLApiBase::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                         const void* userParam) {
  driver_->fn.glDebugMessageCallbackFn(callback, userParam);
}

void GLApiBase::glDebugMessageControlFn(GLenum source,
                                        GLenum type,
                                        GLenum severity,
                                        GLsizei count,
                                        const GLuint* ids,
                                        GLboolean enabled) {
  driver_->fn.glDebugMessageControlFn(source, type, severity, count, ids,
                                      enabled);
}

void GLApiBase::glDebugMessageInsertFn(GLenum source,
                                       GLenum type,
                                       GLuint id,
                                       GLenum severity,
                                       GLsizei length,
                                       const char* buf) {
  driver_->fn.glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

void GLApiBase::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  driver_->fn.glDeleteBuffersARBFn(n, buffers);
}

void GLApiBase::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesAPPLEFn(n, fences);
}

void GLApiBase::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesNVFn(n, fences);
}

void GLApiBase::glDeleteFramebuffersEXTFn(GLsizei n,
                                          const GLuint* framebuffers) {
  driver_->fn.glDeleteFramebuffersEXTFn(n, framebuffers);
}

void GLApiBase::glDeletePathsNVFn(GLuint path, GLsizei range) {
  driver_->fn.glDeletePathsNVFn(path, range);
}

void GLApiBase::glDeleteProgramFn(GLuint program) {
  driver_->fn.glDeleteProgramFn(program);
}

void GLApiBase::glDeleteProgramPipelinesFn(GLsizei n, const GLuint* pipelines) {
  driver_->fn.glDeleteProgramPipelinesFn(n, pipelines);
}

void GLApiBase::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteQueriesFn(n, ids);
}

void GLApiBase::glDeleteRenderbuffersEXTFn(GLsizei n,
                                           const GLuint* renderbuffers) {
  driver_->fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void GLApiBase::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  driver_->fn.glDeleteSamplersFn(n, samplers);
}

void GLApiBase::glDeleteShaderFn(GLuint shader) {
  driver_->fn.glDeleteShaderFn(shader);
}

void GLApiBase::glDeleteSyncFn(GLsync sync) {
  driver_->fn.glDeleteSyncFn(sync);
}

void GLApiBase::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  driver_->fn.glDeleteTexturesFn(n, textures);
}

void GLApiBase::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteTransformFeedbacksFn(n, ids);
}

void GLApiBase::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  driver_->fn.glDeleteVertexArraysOESFn(n, arrays);
}

void GLApiBase::glDepthFuncFn(GLenum func) {
  driver_->fn.glDepthFuncFn(func);
}

void GLApiBase::glDepthMaskFn(GLboolean flag) {
  driver_->fn.glDepthMaskFn(flag);
}

void GLApiBase::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  driver_->fn.glDepthRangeFn(zNear, zFar);
}

void GLApiBase::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  driver_->fn.glDepthRangefFn(zNear, zFar);
}

void GLApiBase::glDetachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glDetachShaderFn(program, shader);
}

void GLApiBase::glDisableFn(GLenum cap) {
  driver_->fn.glDisableFn(cap);
}

void GLApiBase::glDisableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glDisableVertexAttribArrayFn(index);
}

void GLApiBase::glDiscardFramebufferEXTFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void GLApiBase::glDispatchComputeFn(GLuint numGroupsX,
                                    GLuint numGroupsY,
                                    GLuint numGroupsZ) {
  driver_->fn.glDispatchComputeFn(numGroupsX, numGroupsY, numGroupsZ);
}

void GLApiBase::glDispatchComputeIndirectFn(GLintptr indirect) {
  driver_->fn.glDispatchComputeIndirectFn(indirect);
}

void GLApiBase::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  driver_->fn.glDrawArraysFn(mode, first, count);
}

void GLApiBase::glDrawArraysIndirectFn(GLenum mode, const void* indirect) {
  driver_->fn.glDrawArraysIndirectFn(mode, indirect);
}

void GLApiBase::glDrawArraysInstancedANGLEFn(GLenum mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei primcount) {
  driver_->fn.glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void GLApiBase::glDrawBufferFn(GLenum mode) {
  driver_->fn.glDrawBufferFn(mode);
}

void GLApiBase::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  driver_->fn.glDrawBuffersARBFn(n, bufs);
}

void GLApiBase::glDrawElementsFn(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void* indices) {
  driver_->fn.glDrawElementsFn(mode, count, type, indices);
}

void GLApiBase::glDrawElementsIndirectFn(GLenum mode,
                                         GLenum type,
                                         const void* indirect) {
  driver_->fn.glDrawElementsIndirectFn(mode, type, indirect);
}

void GLApiBase::glDrawElementsInstancedANGLEFn(GLenum mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void* indices,
                                               GLsizei primcount) {
  driver_->fn.glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                             primcount);
}

void GLApiBase::glDrawRangeElementsFn(GLenum mode,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  driver_->fn.glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

void GLApiBase::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                         GLeglImageOES image) {
  driver_->fn.glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void GLApiBase::glEGLImageTargetTexture2DOESFn(GLenum target,
                                               GLeglImageOES image) {
  driver_->fn.glEGLImageTargetTexture2DOESFn(target, image);
}

void GLApiBase::glEnableFn(GLenum cap) {
  driver_->fn.glEnableFn(cap);
}

void GLApiBase::glEnableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glEnableVertexAttribArrayFn(index);
}

void GLApiBase::glEndQueryFn(GLenum target) {
  driver_->fn.glEndQueryFn(target);
}

void GLApiBase::glEndTransformFeedbackFn(void) {
  driver_->fn.glEndTransformFeedbackFn();
}

GLsync GLApiBase::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  return driver_->fn.glFenceSyncFn(condition, flags);
}

void GLApiBase::glFinishFn(void) {
  driver_->fn.glFinishFn();
}

void GLApiBase::glFinishFenceAPPLEFn(GLuint fence) {
  driver_->fn.glFinishFenceAPPLEFn(fence);
}

void GLApiBase::glFinishFenceNVFn(GLuint fence) {
  driver_->fn.glFinishFenceNVFn(fence);
}

void GLApiBase::glFlushFn(void) {
  driver_->fn.glFlushFn();
}

void GLApiBase::glFlushMappedBufferRangeFn(GLenum target,
                                           GLintptr offset,
                                           GLsizeiptr length) {
  driver_->fn.glFlushMappedBufferRangeFn(target, offset, length);
}

void GLApiBase::glFramebufferParameteriFn(GLenum target,
                                          GLenum pname,
                                          GLint param) {
  driver_->fn.glFramebufferParameteriFn(target, pname, param);
}

void GLApiBase::glFramebufferRenderbufferEXTFn(GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer) {
  driver_->fn.glFramebufferRenderbufferEXTFn(target, attachment,
                                             renderbuffertarget, renderbuffer);
}

void GLApiBase::glFramebufferTexture2DEXTFn(GLenum target,
                                            GLenum attachment,
                                            GLenum textarget,
                                            GLuint texture,
                                            GLint level) {
  driver_->fn.glFramebufferTexture2DEXTFn(target, attachment, textarget,
                                          texture, level);
}

void GLApiBase::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                       GLenum attachment,
                                                       GLenum textarget,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLsizei samples) {
  driver_->fn.glFramebufferTexture2DMultisampleEXTFn(
      target, attachment, textarget, texture, level, samples);
}

void GLApiBase::glFramebufferTextureLayerFn(GLenum target,
                                            GLenum attachment,
                                            GLuint texture,
                                            GLint level,
                                            GLint layer) {
  driver_->fn.glFramebufferTextureLayerFn(target, attachment, texture, level,
                                          layer);
}

void GLApiBase::glFramebufferTextureMultiviewLayeredANGLEFn(GLenum target,
                                                            GLenum attachment,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLint baseViewIndex,
                                                            GLsizei numViews) {
  driver_->fn.glFramebufferTextureMultiviewLayeredANGLEFn(
      target, attachment, texture, level, baseViewIndex, numViews);
}

void GLApiBase::glFrontFaceFn(GLenum mode) {
  driver_->fn.glFrontFaceFn(mode);
}

void GLApiBase::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  driver_->fn.glGenBuffersARBFn(n, buffers);
}

void GLApiBase::glGenerateMipmapEXTFn(GLenum target) {
  driver_->fn.glGenerateMipmapEXTFn(target);
}

void GLApiBase::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesAPPLEFn(n, fences);
}

void GLApiBase::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesNVFn(n, fences);
}

void GLApiBase::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  driver_->fn.glGenFramebuffersEXTFn(n, framebuffers);
}

GLuint GLApiBase::glGenPathsNVFn(GLsizei range) {
  return driver_->fn.glGenPathsNVFn(range);
}

GLuint GLApiBase::glGenProgramPipelinesFn(GLsizei n, GLuint* pipelines) {
  return driver_->fn.glGenProgramPipelinesFn(n, pipelines);
}

void GLApiBase::glGenQueriesFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenQueriesFn(n, ids);
}

void GLApiBase::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  driver_->fn.glGenRenderbuffersEXTFn(n, renderbuffers);
}

void GLApiBase::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  driver_->fn.glGenSamplersFn(n, samplers);
}

void GLApiBase::glGenTexturesFn(GLsizei n, GLuint* textures) {
  driver_->fn.glGenTexturesFn(n, textures);
}

void GLApiBase::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenTransformFeedbacksFn(n, ids);
}

void GLApiBase::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  driver_->fn.glGenVertexArraysOESFn(n, arrays);
}

void GLApiBase::glGetActiveAttribFn(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) {
  driver_->fn.glGetActiveAttribFn(program, index, bufsize, length, size, type,
                                  name);
}

void GLApiBase::glGetActiveUniformFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  driver_->fn.glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                   name);
}

void GLApiBase::glGetActiveUniformBlockivFn(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLint* params) {
  driver_->fn.glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                          params);
}

void GLApiBase::glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                                       GLuint uniformBlockIndex,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  driver_->fn.glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void GLApiBase::glGetActiveUniformBlockNameFn(GLuint program,
                                              GLuint uniformBlockIndex,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              char* uniformBlockName) {
  driver_->fn.glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                            length, uniformBlockName);
}

void GLApiBase::glGetActiveUniformsivFn(GLuint program,
                                        GLsizei uniformCount,
                                        const GLuint* uniformIndices,
                                        GLenum pname,
                                        GLint* params) {
  driver_->fn.glGetActiveUniformsivFn(program, uniformCount, uniformIndices,
                                      pname, params);
}

void GLApiBase::glGetAttachedShadersFn(GLuint program,
                                       GLsizei maxcount,
                                       GLsizei* count,
                                       GLuint* shaders) {
  driver_->fn.glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint GLApiBase::glGetAttribLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetAttribLocationFn(program, name);
}

void GLApiBase::glGetBooleani_vFn(GLenum target,
                                  GLuint index,
                                  GLboolean* data) {
  driver_->fn.glGetBooleani_vFn(target, index, data);
}

void GLApiBase::glGetBooleani_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLboolean* data) {
  driver_->fn.glGetBooleani_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

void GLApiBase::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  driver_->fn.glGetBooleanvFn(pname, params);
}

void GLApiBase::glGetBooleanvRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLboolean* data) {
  driver_->fn.glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint64* params) {
  driver_->fn.glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

void GLApiBase::glGetBufferParameterivFn(GLenum target,
                                         GLenum pname,
                                         GLint* params) {
  driver_->fn.glGetBufferParameterivFn(target, pname, params);
}

void GLApiBase::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  driver_->fn.glGetBufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                  length, params);
}

void GLApiBase::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 void** params) {
  driver_->fn.glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

GLuint GLApiBase::glGetDebugMessageLogFn(GLuint count,
                                         GLsizei bufSize,
                                         GLenum* sources,
                                         GLenum* types,
                                         GLuint* ids,
                                         GLenum* severities,
                                         GLsizei* lengths,
                                         char* messageLog) {
  return driver_->fn.glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                            severities, lengths, messageLog);
}

GLenum GLApiBase::glGetErrorFn(void) {
  return driver_->fn.glGetErrorFn();
}

void GLApiBase::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  driver_->fn.glGetFenceivNVFn(fence, pname, params);
}

void GLApiBase::glGetFloatvFn(GLenum pname, GLfloat* params) {
  driver_->fn.glGetFloatvFn(pname, params);
}

void GLApiBase::glGetFloatvRobustANGLEFn(GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLfloat* data) {
  driver_->fn.glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

GLint GLApiBase::glGetFragDataIndexFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataIndexFn(program, name);
}

GLint GLApiBase::glGetFragDataLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataLocationFn(program, name);
}

void GLApiBase::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                           GLenum attachment,
                                                           GLenum pname,
                                                           GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivEXTFn(target, attachment,
                                                         pname, params);
}

void GLApiBase::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

void GLApiBase::glGetFramebufferParameterivFn(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  driver_->fn.glGetFramebufferParameterivFn(target, pname, params);
}

void GLApiBase::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  driver_->fn.glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                       length, params);
}

GLenum GLApiBase::glGetGraphicsResetStatusARBFn(void) {
  return driver_->fn.glGetGraphicsResetStatusARBFn();
}

void GLApiBase::glGetInteger64i_vFn(GLenum target,
                                    GLuint index,
                                    GLint64* data) {
  driver_->fn.glGetInteger64i_vFn(target, index, data);
}

void GLApiBase::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint64* data) {
  driver_->fn.glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length,
                                             data);
}

void GLApiBase::glGetInteger64vFn(GLenum pname, GLint64* params) {
  driver_->fn.glGetInteger64vFn(pname, params);
}

void GLApiBase::glGetInteger64vRobustANGLEFn(GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint64* data) {
  driver_->fn.glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  driver_->fn.glGetIntegeri_vFn(target, index, data);
}

void GLApiBase::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* data) {
  driver_->fn.glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

void GLApiBase::glGetIntegervFn(GLenum pname, GLint* params) {
  driver_->fn.glGetIntegervFn(pname, params);
}

void GLApiBase::glGetIntegervRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* data) {
  driver_->fn.glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetInternalformativFn(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLint* params) {
  driver_->fn.glGetInternalformativFn(target, internalformat, pname, bufSize,
                                      params);
}

void GLApiBase::glGetInternalformativRobustANGLEFn(GLenum target,
                                                   GLenum internalformat,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  driver_->fn.glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                                 bufSize, length, params);
}

void GLApiBase::glGetMultisamplefvFn(GLenum pname, GLuint index, GLfloat* val) {
  driver_->fn.glGetMultisamplefvFn(pname, index, val);
}

void GLApiBase::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* val) {
  driver_->fn.glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length,
                                              val);
}

void GLApiBase::glGetnUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  driver_->fn.glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetnUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  driver_->fn.glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetnUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  driver_->fn.glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                            params);
}

void GLApiBase::glGetObjectLabelFn(GLenum identifier,
                                   GLuint name,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   char* label) {
  driver_->fn.glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

void GLApiBase::glGetObjectPtrLabelFn(void* ptr,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      char* label) {
  driver_->fn.glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

void GLApiBase::glGetPointervFn(GLenum pname, void** params) {
  driver_->fn.glGetPointervFn(pname, params);
}

void GLApiBase::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  driver_->fn.glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                    params);
}

void GLApiBase::glGetProgramBinaryFn(GLuint program,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLenum* binaryFormat,
                                     GLvoid* binary) {
  driver_->fn.glGetProgramBinaryFn(program, bufSize, length, binaryFormat,
                                   binary);
}

void GLApiBase::glGetProgramInfoLogFn(GLuint program,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  driver_->fn.glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void GLApiBase::glGetProgramInterfaceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetProgramInterfaceivFn(program, programInterface, pname,
                                        params);
}

void GLApiBase::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                     GLenum programInterface,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetProgramInterfaceivRobustANGLEFn(
      program, programInterface, pname, bufSize, length, params);
}

void GLApiBase::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  driver_->fn.glGetProgramivFn(program, pname, params);
}

void GLApiBase::glGetProgramivRobustANGLEFn(GLuint program,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetProgramivRobustANGLEFn(program, pname, bufSize, length,
                                          params);
}

void GLApiBase::glGetProgramPipelineInfoLogFn(GLuint pipeline,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLchar* infoLog) {
  driver_->fn.glGetProgramPipelineInfoLogFn(pipeline, bufSize, length, infoLog);
}

void GLApiBase::glGetProgramPipelineivFn(GLuint pipeline,
                                         GLenum pname,
                                         GLint* params) {
  driver_->fn.glGetProgramPipelineivFn(pipeline, pname, params);
}

void GLApiBase::glGetProgramResourceIndexFn(GLuint program,
                                            GLenum programInterface,
                                            const GLchar* name) {
  driver_->fn.glGetProgramResourceIndexFn(program, programInterface, name);
}

void GLApiBase::glGetProgramResourceivFn(GLuint program,
                                         GLenum programInterface,
                                         GLuint index,
                                         GLsizei propCount,
                                         const GLenum* props,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) {
  driver_->fn.glGetProgramResourceivFn(program, programInterface, index,
                                       propCount, props, bufSize, length,
                                       params);
}

GLint GLApiBase::glGetProgramResourceLocationFn(GLuint program,
                                                GLenum programInterface,
                                                const char* name) {
  return driver_->fn.glGetProgramResourceLocationFn(program, programInterface,
                                                    name);
}

void GLApiBase::glGetProgramResourceNameFn(GLuint program,
                                           GLenum programInterface,
                                           GLuint index,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLchar* name) {
  driver_->fn.glGetProgramResourceNameFn(program, programInterface, index,
                                         bufSize, length, name);
}

void GLApiBase::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryivFn(target, pname, params);
}

void GLApiBase::glGetQueryivRobustANGLEFn(GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  driver_->fn.glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

void GLApiBase::glGetQueryObjecti64vFn(GLuint id,
                                       GLenum pname,
                                       GLint64* params) {
  driver_->fn.glGetQueryObjecti64vFn(id, pname, params);
}

void GLApiBase::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* params) {
  driver_->fn.glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryObjectivFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  driver_->fn.glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

void GLApiBase::glGetQueryObjectui64vFn(GLuint id,
                                        GLenum pname,
                                        GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) {
  driver_->fn.glGetQueryObjectuivFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLuint* params) {
  driver_->fn.glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                  GLenum pname,
                                                  GLint* params) {
  driver_->fn.glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void GLApiBase::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  driver_->fn.glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

void GLApiBase::glGetSamplerParameterfvFn(GLuint sampler,
                                          GLenum pname,
                                          GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvFn(sampler, pname, params);
}

void GLApiBase::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

void GLApiBase::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                    length, params);
}

void GLApiBase::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint* params) {
  driver_->fn.glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                     length, params);
}

void GLApiBase::glGetSamplerParameterivFn(GLuint sampler,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetSamplerParameterivFn(sampler, pname, params);
}

void GLApiBase::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

void GLApiBase::glGetShaderInfoLogFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) {
  driver_->fn.glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void GLApiBase::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  driver_->fn.glGetShaderivFn(shader, pname, params);
}

void GLApiBase::glGetShaderivRobustANGLEFn(GLuint shader,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  driver_->fn.glGetShaderivRobustANGLEFn(shader, pname, bufSize, length,
                                         params);
}

void GLApiBase::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                             GLenum precisiontype,
                                             GLint* range,
                                             GLint* precision) {
  driver_->fn.glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                           precision);
}

void GLApiBase::glGetShaderSourceFn(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* source) {
  driver_->fn.glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* GLApiBase::glGetStringFn(GLenum name) {
  return driver_->fn.glGetStringFn(name);
}

const GLubyte* GLApiBase::glGetStringiFn(GLenum name, GLuint index) {
  return driver_->fn.glGetStringiFn(name, index);
}

void GLApiBase::glGetSyncivFn(GLsync sync,
                              GLenum pname,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLint* values) {
  driver_->fn.glGetSyncivFn(sync, pname, bufSize, length, values);
}

void GLApiBase::glGetTexLevelParameterfvFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvFn(target, level, pname, params);
}

void GLApiBase::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

void GLApiBase::glGetTexLevelParameterivFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLint* params) {
  driver_->fn.glGetTexLevelParameterivFn(target, level, pname, params);
}

void GLApiBase::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetTexLevelParameterivRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

void GLApiBase::glGetTexParameterfvFn(GLenum target,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetTexParameterfvFn(target, pname, params);
}

void GLApiBase::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetTexParameterivFn(GLenum target,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetTexParameterivFn(target, pname, params);
}

void GLApiBase::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetTransformFeedbackVaryingFn(GLuint program,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLsizei* size,
                                                GLenum* type,
                                                char* name) {
  driver_->fn.glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                              size, type, name);
}

void GLApiBase::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   char* source) {
  driver_->fn.glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length,
                                                 source);
}

GLuint GLApiBase::glGetUniformBlockIndexFn(GLuint program,
                                           const char* uniformBlockName) {
  return driver_->fn.glGetUniformBlockIndexFn(program, uniformBlockName);
}

void GLApiBase::glGetUniformfvFn(GLuint program,
                                 GLint location,
                                 GLfloat* params) {
  driver_->fn.glGetUniformfvFn(program, location, params);
}

void GLApiBase::glGetUniformfvRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLfloat* params) {
  driver_->fn.glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

void GLApiBase::glGetUniformIndicesFn(GLuint program,
                                      GLsizei uniformCount,
                                      const char* const* uniformNames,
                                      GLuint* uniformIndices) {
  driver_->fn.glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                    uniformIndices);
}

void GLApiBase::glGetUniformivFn(GLuint program,
                                 GLint location,
                                 GLint* params) {
  driver_->fn.glGetUniformivFn(program, location, params);
}

void GLApiBase::glGetUniformivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

GLint GLApiBase::glGetUniformLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetUniformLocationFn(program, name);
}

void GLApiBase::glGetUniformuivFn(GLuint program,
                                  GLint location,
                                  GLuint* params) {
  driver_->fn.glGetUniformuivFn(program, location, params);
}

void GLApiBase::glGetUniformuivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLuint* params) {
  driver_->fn.glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetVertexAttribfvFn(GLuint index,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetVertexAttribfvFn(index, pname, params);
}

void GLApiBase::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetVertexAttribivFn(GLuint index,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetVertexAttribivFn(index, pname, params);
}

void GLApiBase::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetVertexAttribPointervFn(GLuint index,
                                            GLenum pname,
                                            void** pointer) {
  driver_->fn.glGetVertexAttribPointervFn(index, pname, pointer);
}

void GLApiBase::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** pointer) {
  driver_->fn.glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize,
                                                     length, pointer);
}

void GLApiBase::glHintFn(GLenum target, GLenum mode) {
  driver_->fn.glHintFn(target, mode);
}

void GLApiBase::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glInsertEventMarkerEXTFn(length, marker);
}

void GLApiBase::glInvalidateFramebufferFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glInvalidateFramebufferFn(target, numAttachments, attachments);
}

void GLApiBase::glInvalidateSubFramebufferFn(GLenum target,
                                             GLsizei numAttachments,
                                             const GLenum* attachments,
                                             GLint x,
                                             GLint y,
                                             GLint width,
                                             GLint height) {
  driver_->fn.glInvalidateSubFramebufferFn(target, numAttachments, attachments,
                                           x, y, width, height);
}

GLboolean GLApiBase::glIsBufferFn(GLuint buffer) {
  return driver_->fn.glIsBufferFn(buffer);
}

GLboolean GLApiBase::glIsEnabledFn(GLenum cap) {
  return driver_->fn.glIsEnabledFn(cap);
}

GLboolean GLApiBase::glIsFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glIsFenceAPPLEFn(fence);
}

GLboolean GLApiBase::glIsFenceNVFn(GLuint fence) {
  return driver_->fn.glIsFenceNVFn(fence);
}

GLboolean GLApiBase::glIsFramebufferEXTFn(GLuint framebuffer) {
  return driver_->fn.glIsFramebufferEXTFn(framebuffer);
}

GLboolean GLApiBase::glIsPathNVFn(GLuint path) {
  return driver_->fn.glIsPathNVFn(path);
}

GLboolean GLApiBase::glIsProgramFn(GLuint program) {
  return driver_->fn.glIsProgramFn(program);
}

GLboolean GLApiBase::glIsProgramPipelineFn(GLuint pipeline) {
  return driver_->fn.glIsProgramPipelineFn(pipeline);
}

GLboolean GLApiBase::glIsQueryFn(GLuint query) {
  return driver_->fn.glIsQueryFn(query);
}

GLboolean GLApiBase::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  return driver_->fn.glIsRenderbufferEXTFn(renderbuffer);
}

GLboolean GLApiBase::glIsSamplerFn(GLuint sampler) {
  return driver_->fn.glIsSamplerFn(sampler);
}

GLboolean GLApiBase::glIsShaderFn(GLuint shader) {
  return driver_->fn.glIsShaderFn(shader);
}

GLboolean GLApiBase::glIsSyncFn(GLsync sync) {
  return driver_->fn.glIsSyncFn(sync);
}

GLboolean GLApiBase::glIsTextureFn(GLuint texture) {
  return driver_->fn.glIsTextureFn(texture);
}

GLboolean GLApiBase::glIsTransformFeedbackFn(GLuint id) {
  return driver_->fn.glIsTransformFeedbackFn(id);
}

GLboolean GLApiBase::glIsVertexArrayOESFn(GLuint array) {
  return driver_->fn.glIsVertexArrayOESFn(array);
}

void GLApiBase::glLineWidthFn(GLfloat width) {
  driver_->fn.glLineWidthFn(width);
}

void GLApiBase::glLinkProgramFn(GLuint program) {
  driver_->fn.glLinkProgramFn(program);
}

void* GLApiBase::glMapBufferFn(GLenum target, GLenum access) {
  return driver_->fn.glMapBufferFn(target, access);
}

void* GLApiBase::glMapBufferRangeFn(GLenum target,
                                    GLintptr offset,
                                    GLsizeiptr length,
                                    GLbitfield access) {
  return driver_->fn.glMapBufferRangeFn(target, offset, length, access);
}

void GLApiBase::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  driver_->fn.glMatrixLoadfEXTFn(matrixMode, m);
}

void GLApiBase::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  driver_->fn.glMatrixLoadIdentityEXTFn(matrixMode);
}

void GLApiBase::glMaxShaderCompilerThreadsKHRFn(GLuint count) {
  driver_->fn.glMaxShaderCompilerThreadsKHRFn(count);
}

void GLApiBase::glMemoryBarrierByRegionFn(GLbitfield barriers) {
  driver_->fn.glMemoryBarrierByRegionFn(barriers);
}

void GLApiBase::glMemoryBarrierEXTFn(GLbitfield barriers) {
  driver_->fn.glMemoryBarrierEXTFn(barriers);
}

void GLApiBase::glMinSampleShadingFn(GLfloat value) {
  driver_->fn.glMinSampleShadingFn(value);
}

void GLApiBase::glObjectLabelFn(GLenum identifier,
                                GLuint name,
                                GLsizei length,
                                const char* label) {
  driver_->fn.glObjectLabelFn(identifier, name, length, label);
}

void GLApiBase::glObjectPtrLabelFn(void* ptr,
                                   GLsizei length,
                                   const char* label) {
  driver_->fn.glObjectPtrLabelFn(ptr, length, label);
}

void GLApiBase::glPathCommandsNVFn(GLuint path,
                                   GLsizei numCommands,
                                   const GLubyte* commands,
                                   GLsizei numCoords,
                                   GLenum coordType,
                                   const GLvoid* coords) {
  driver_->fn.glPathCommandsNVFn(path, numCommands, commands, numCoords,
                                 coordType, coords);
}

void GLApiBase::glPathParameterfNVFn(GLuint path, GLenum pname, GLfloat value) {
  driver_->fn.glPathParameterfNVFn(path, pname, value);
}

void GLApiBase::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  driver_->fn.glPathParameteriNVFn(path, pname, value);
}

void GLApiBase::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glPathStencilFuncNVFn(func, ref, mask);
}

void GLApiBase::glPauseTransformFeedbackFn(void) {
  driver_->fn.glPauseTransformFeedbackFn();
}

void GLApiBase::glPixelStoreiFn(GLenum pname, GLint param) {
  driver_->fn.glPixelStoreiFn(pname, param);
}

void GLApiBase::glPointParameteriFn(GLenum pname, GLint param) {
  driver_->fn.glPointParameteriFn(pname, param);
}

void GLApiBase::glPolygonModeFn(GLenum face, GLenum mode) {
  driver_->fn.glPolygonModeFn(face, mode);
}

void GLApiBase::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  driver_->fn.glPolygonOffsetFn(factor, units);
}

void GLApiBase::glPopDebugGroupFn() {
  driver_->fn.glPopDebugGroupFn();
}

void GLApiBase::glPopGroupMarkerEXTFn(void) {
  driver_->fn.glPopGroupMarkerEXTFn();
}

void GLApiBase::glPrimitiveRestartIndexFn(GLuint index) {
  driver_->fn.glPrimitiveRestartIndexFn(index);
}

void GLApiBase::glProgramBinaryFn(GLuint program,
                                  GLenum binaryFormat,
                                  const GLvoid* binary,
                                  GLsizei length) {
  driver_->fn.glProgramBinaryFn(program, binaryFormat, binary, length);
}

void GLApiBase::glProgramParameteriFn(GLuint program,
                                      GLenum pname,
                                      GLint value) {
  driver_->fn.glProgramParameteriFn(program, pname, value);
}

void GLApiBase::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                  GLint location,
                                                  GLenum genMode,
                                                  GLint components,
                                                  const GLfloat* coeffs) {
  driver_->fn.glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                                components, coeffs);
}

void GLApiBase::glProgramUniform1fFn(GLuint program,
                                     GLint location,
                                     GLfloat v0) {
  driver_->fn.glProgramUniform1fFn(program, location, v0);
}

void GLApiBase::glProgramUniform1fvFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLfloat* value) {
  driver_->fn.glProgramUniform1fvFn(program, location, count, value);
}

void GLApiBase::glProgramUniform1iFn(GLuint program, GLint location, GLint v0) {
  driver_->fn.glProgramUniform1iFn(program, location, v0);
}

void GLApiBase::glProgramUniform1ivFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLint* value) {
  driver_->fn.glProgramUniform1ivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform1uiFn(GLuint program,
                                      GLint location,
                                      GLuint v0) {
  driver_->fn.glProgramUniform1uiFn(program, location, v0);
}

void GLApiBase::glProgramUniform1uivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLuint* value) {
  driver_->fn.glProgramUniform1uivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform2fFn(GLuint program,
                                     GLint location,
                                     GLfloat v0,
                                     GLfloat v1) {
  driver_->fn.glProgramUniform2fFn(program, location, v0, v1);
}

void GLApiBase::glProgramUniform2fvFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLfloat* value) {
  driver_->fn.glProgramUniform2fvFn(program, location, count, value);
}

void GLApiBase::glProgramUniform2iFn(GLuint program,
                                     GLint location,
                                     GLint v0,
                                     GLint v1) {
  driver_->fn.glProgramUniform2iFn(program, location, v0, v1);
}

void GLApiBase::glProgramUniform2ivFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLint* value) {
  driver_->fn.glProgramUniform2ivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform2uiFn(GLuint program,
                                      GLint location,
                                      GLuint v0,
                                      GLuint v1) {
  driver_->fn.glProgramUniform2uiFn(program, location, v0, v1);
}

void GLApiBase::glProgramUniform2uivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLuint* value) {
  driver_->fn.glProgramUniform2uivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform3fFn(GLuint program,
                                     GLint location,
                                     GLfloat v0,
                                     GLfloat v1,
                                     GLfloat v2) {
  driver_->fn.glProgramUniform3fFn(program, location, v0, v1, v2);
}

void GLApiBase::glProgramUniform3fvFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLfloat* value) {
  driver_->fn.glProgramUniform3fvFn(program, location, count, value);
}

void GLApiBase::glProgramUniform3iFn(GLuint program,
                                     GLint location,
                                     GLint v0,
                                     GLint v1,
                                     GLint v2) {
  driver_->fn.glProgramUniform3iFn(program, location, v0, v1, v2);
}

void GLApiBase::glProgramUniform3ivFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLint* value) {
  driver_->fn.glProgramUniform3ivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform3uiFn(GLuint program,
                                      GLint location,
                                      GLuint v0,
                                      GLuint v1,
                                      GLuint v2) {
  driver_->fn.glProgramUniform3uiFn(program, location, v0, v1, v2);
}

void GLApiBase::glProgramUniform3uivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLuint* value) {
  driver_->fn.glProgramUniform3uivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform4fFn(GLuint program,
                                     GLint location,
                                     GLfloat v0,
                                     GLfloat v1,
                                     GLfloat v2,
                                     GLfloat v3) {
  driver_->fn.glProgramUniform4fFn(program, location, v0, v1, v2, v3);
}

void GLApiBase::glProgramUniform4fvFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLfloat* value) {
  driver_->fn.glProgramUniform4fvFn(program, location, count, value);
}

void GLApiBase::glProgramUniform4iFn(GLuint program,
                                     GLint location,
                                     GLint v0,
                                     GLint v1,
                                     GLint v2,
                                     GLint v3) {
  driver_->fn.glProgramUniform4iFn(program, location, v0, v1, v2, v3);
}

void GLApiBase::glProgramUniform4ivFn(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLint* value) {
  driver_->fn.glProgramUniform4ivFn(program, location, count, value);
}

void GLApiBase::glProgramUniform4uiFn(GLuint program,
                                      GLint location,
                                      GLuint v0,
                                      GLuint v1,
                                      GLuint v2,
                                      GLuint v3) {
  driver_->fn.glProgramUniform4uiFn(program, location, v0, v1, v2, v3);
}

void GLApiBase::glProgramUniform4uivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLuint* value) {
  driver_->fn.glProgramUniform4uivFn(program, location, count, value);
}

void GLApiBase::glProgramUniformMatrix2fvFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix2fvFn(program, location, count, transpose,
                                          value);
}

void GLApiBase::glProgramUniformMatrix2x3fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix2x3fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glProgramUniformMatrix2x4fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix2x4fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glProgramUniformMatrix3fvFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix3fvFn(program, location, count, transpose,
                                          value);
}

void GLApiBase::glProgramUniformMatrix3x2fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix3x2fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glProgramUniformMatrix3x4fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix3x4fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glProgramUniformMatrix4fvFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix4fvFn(program, location, count, transpose,
                                          value);
}

void GLApiBase::glProgramUniformMatrix4x2fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix4x2fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glProgramUniformMatrix4x3fvFn(GLuint program,
                                              GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat* value) {
  driver_->fn.glProgramUniformMatrix4x3fvFn(program, location, count, transpose,
                                            value);
}

void GLApiBase::glPushDebugGroupFn(GLenum source,
                                   GLuint id,
                                   GLsizei length,
                                   const char* message) {
  driver_->fn.glPushDebugGroupFn(source, id, length, message);
}

void GLApiBase::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glPushGroupMarkerEXTFn(length, marker);
}

void GLApiBase::glQueryCounterFn(GLuint id, GLenum target) {
  driver_->fn.glQueryCounterFn(id, target);
}

void GLApiBase::glReadBufferFn(GLenum src) {
  driver_->fn.glReadBufferFn(src);
}

void GLApiBase::glReadnPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* data) {
  driver_->fn.glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                         bufSize, length, columns, rows, data);
}

void GLApiBase::glReadPixelsFn(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               void* pixels) {
  driver_->fn.glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void GLApiBase::glReadPixelsRobustANGLEFn(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLsizei* columns,
                                          GLsizei* rows,
                                          void* pixels) {
  driver_->fn.glReadPixelsRobustANGLEFn(x, y, width, height, format, type,
                                        bufSize, length, columns, rows, pixels);
}

void GLApiBase::glReleaseShaderCompilerFn(void) {
  driver_->fn.glReleaseShaderCompilerFn();
}

void GLApiBase::glRenderbufferStorageEXTFn(GLenum target,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) {
  driver_->fn.glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleFn(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleFn(target, samples,
                                                 internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, internalformat, width, height);
}

void GLApiBase::glRequestExtensionANGLEFn(const char* name) {
  driver_->fn.glRequestExtensionANGLEFn(name);
}

void GLApiBase::glResumeTransformFeedbackFn(void) {
  driver_->fn.glResumeTransformFeedbackFn();
}

void GLApiBase::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  driver_->fn.glSampleCoverageFn(value, invert);
}

void GLApiBase::glSampleMaskiFn(GLuint maskNumber, GLbitfield mask) {
  driver_->fn.glSampleMaskiFn(maskNumber, mask);
}

void GLApiBase::glSamplerParameterfFn(GLuint sampler,
                                      GLenum pname,
                                      GLfloat param) {
  driver_->fn.glSamplerParameterfFn(sampler, pname, param);
}

void GLApiBase::glSamplerParameterfvFn(GLuint sampler,
                                       GLenum pname,
                                       const GLfloat* params) {
  driver_->fn.glSamplerParameterfvFn(sampler, pname, params);
}

void GLApiBase::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLfloat* param) {
  driver_->fn.glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

void GLApiBase::glSamplerParameteriFn(GLuint sampler,
                                      GLenum pname,
                                      GLint param) {
  driver_->fn.glSamplerParameteriFn(sampler, pname, param);
}

void GLApiBase::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  driver_->fn.glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 param);
}

void GLApiBase::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLuint* param) {
  driver_->fn.glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  param);
}

void GLApiBase::glSamplerParameterivFn(GLuint sampler,
                                       GLenum pname,
                                       const GLint* params) {
  driver_->fn.glSamplerParameterivFn(sampler, pname, params);
}

void GLApiBase::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLint* param) {
  driver_->fn.glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

void GLApiBase::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glScissorFn(x, y, width, height);
}

void GLApiBase::glSetFenceAPPLEFn(GLuint fence) {
  driver_->fn.glSetFenceAPPLEFn(fence);
}

void GLApiBase::glSetFenceNVFn(GLuint fence, GLenum condition) {
  driver_->fn.glSetFenceNVFn(fence, condition);
}

void GLApiBase::glShaderBinaryFn(GLsizei n,
                                 const GLuint* shaders,
                                 GLenum binaryformat,
                                 const void* binary,
                                 GLsizei length) {
  driver_->fn.glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void GLApiBase::glShaderSourceFn(GLuint shader,
                                 GLsizei count,
                                 const char* const* str,
                                 const GLint* length) {
  driver_->fn.glShaderSourceFn(shader, count, str, length);
}

void GLApiBase::glStencilFillPathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, fillMode, mask,
                                             transformType, transformValues);
}

void GLApiBase::glStencilFillPathNVFn(GLuint path,
                                      GLenum fillMode,
                                      GLuint mask) {
  driver_->fn.glStencilFillPathNVFn(path, fillMode, mask);
}

void GLApiBase::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glStencilFuncFn(func, ref, mask);
}

void GLApiBase::glStencilFuncSeparateFn(GLenum face,
                                        GLenum func,
                                        GLint ref,
                                        GLuint mask) {
  driver_->fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

void GLApiBase::glStencilMaskFn(GLuint mask) {
  driver_->fn.glStencilMaskFn(mask);
}

void GLApiBase::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  driver_->fn.glStencilMaskSeparateFn(face, mask);
}

void GLApiBase::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  driver_->fn.glStencilOpFn(fail, zfail, zpass);
}

void GLApiBase::glStencilOpSeparateFn(GLenum face,
                                      GLenum fail,
                                      GLenum zfail,
                                      GLenum zpass) {
  driver_->fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void GLApiBase::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                               pathBase, ref, mask,
                                               transformType, transformValues);
}

void GLApiBase::glStencilStrokePathNVFn(GLuint path,
                                        GLint reference,
                                        GLuint mask) {
  driver_->fn.glStencilStrokePathNVFn(path, reference, mask);
}

void GLApiBase::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

void GLApiBase::glStencilThenCoverFillPathNVFn(GLuint path,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum coverMode) {
  driver_->fn.glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

void GLApiBase::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

void GLApiBase::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                 GLint reference,
                                                 GLuint mask,
                                                 GLenum coverMode) {
  driver_->fn.glStencilThenCoverStrokePathNVFn(path, reference, mask,
                                               coverMode);
}

GLboolean GLApiBase::glTestFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glTestFenceAPPLEFn(fence);
}

GLboolean GLApiBase::glTestFenceNVFn(GLuint fence) {
  return driver_->fn.glTestFenceNVFn(fence);
}

void GLApiBase::glTexBufferFn(GLenum target,
                              GLenum internalformat,
                              GLuint buffer) {
  driver_->fn.glTexBufferFn(target, internalformat, buffer);
}

void GLApiBase::glTexBufferRangeFn(GLenum target,
                                   GLenum internalformat,
                                   GLuint buffer,
                                   GLintptr offset,
                                   GLsizeiptr size) {
  driver_->fn.glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

void GLApiBase::glTexImage2DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage2DFn(target, level, internalformat, width, height,
                             border, format, type, pixels);
}

void GLApiBase::glTexImage2DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                        height, border, format, type, bufSize,
                                        pixels);
}

void GLApiBase::glTexImage3DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage3DFn(target, level, internalformat, width, height,
                             depth, border, format, type, pixels);
}

void GLApiBase::glTexImage3DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                        height, depth, border, format, type,
                                        bufSize, pixels);
}

void GLApiBase::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  driver_->fn.glTexParameterfFn(target, pname, param);
}

void GLApiBase::glTexParameterfvFn(GLenum target,
                                   GLenum pname,
                                   const GLfloat* params) {
  driver_->fn.glTexParameterfvFn(target, pname, params);
}

void GLApiBase::glTexParameterfvRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLfloat* params) {
  driver_->fn.glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  driver_->fn.glTexParameteriFn(target, pname, param);
}

void GLApiBase::glTexParameterIivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  driver_->fn.glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLuint* params) {
  driver_->fn.glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameterivFn(GLenum target,
                                   GLenum pname,
                                   const GLint* params) {
  driver_->fn.glTexParameterivFn(target, pname, params);
}

void GLApiBase::glTexParameterivRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint* params) {
  driver_->fn.glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexStorage2DEXTFn(GLenum target,
                                    GLsizei levels,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height) {
  driver_->fn.glTexStorage2DEXTFn(target, levels, internalformat, width,
                                  height);
}

void GLApiBase::glTexStorage2DMultisampleFn(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLboolean fixedsamplelocations) {
  driver_->fn.glTexStorage2DMultisampleFn(target, samples, internalformat,
                                          width, height, fixedsamplelocations);
}

void GLApiBase::glTexStorage3DFn(GLenum target,
                                 GLsizei levels,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth) {
  driver_->fn.glTexStorage3DFn(target, levels, internalformat, width, height,
                               depth);
}

void GLApiBase::glTexSubImage2DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                                format, type, pixels);
}

void GLApiBase::glTexSubImage2DRobustANGLEFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             const void* pixels) {
  driver_->fn.glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset,
                                           width, height, format, type, bufSize,
                                           pixels);
}

void GLApiBase::glTexSubImage3DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, type, pixels);
}

void GLApiBase::glTexSubImage3DRobustANGLEFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             const void* pixels) {
  driver_->fn.glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                           zoffset, width, height, depth,
                                           format, type, bufSize, pixels);
}

void GLApiBase::glTransformFeedbackVaryingsFn(GLuint program,
                                              GLsizei count,
                                              const char* const* varyings,
                                              GLenum bufferMode) {
  driver_->fn.glTransformFeedbackVaryingsFn(program, count, varyings,
                                            bufferMode);
}

void GLApiBase::glUniform1fFn(GLint location, GLfloat x) {
  driver_->fn.glUniform1fFn(location, x);
}

void GLApiBase::glUniform1fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform1fvFn(location, count, v);
}

void GLApiBase::glUniform1iFn(GLint location, GLint x) {
  driver_->fn.glUniform1iFn(location, x);
}

void GLApiBase::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform1ivFn(location, count, v);
}

void GLApiBase::glUniform1uiFn(GLint location, GLuint v0) {
  driver_->fn.glUniform1uiFn(location, v0);
}

void GLApiBase::glUniform1uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform1uivFn(location, count, v);
}

void GLApiBase::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  driver_->fn.glUniform2fFn(location, x, y);
}

void GLApiBase::glUniform2fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform2fvFn(location, count, v);
}

void GLApiBase::glUniform2iFn(GLint location, GLint x, GLint y) {
  driver_->fn.glUniform2iFn(location, x, y);
}

void GLApiBase::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform2ivFn(location, count, v);
}

void GLApiBase::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  driver_->fn.glUniform2uiFn(location, v0, v1);
}

void GLApiBase::glUniform2uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform2uivFn(location, count, v);
}

void GLApiBase::glUniform3fFn(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  driver_->fn.glUniform3fFn(location, x, y, z);
}

void GLApiBase::glUniform3fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform3fvFn(location, count, v);
}

void GLApiBase::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  driver_->fn.glUniform3iFn(location, x, y, z);
}

void GLApiBase::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform3ivFn(location, count, v);
}

void GLApiBase::glUniform3uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2) {
  driver_->fn.glUniform3uiFn(location, v0, v1, v2);
}

void GLApiBase::glUniform3uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform3uivFn(location, count, v);
}

void GLApiBase::glUniform4fFn(GLint location,
                              GLfloat x,
                              GLfloat y,
                              GLfloat z,
                              GLfloat w) {
  driver_->fn.glUniform4fFn(location, x, y, z, w);
}

void GLApiBase::glUniform4fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform4fvFn(location, count, v);
}

void GLApiBase::glUniform4iFn(GLint location,
                              GLint x,
                              GLint y,
                              GLint z,
                              GLint w) {
  driver_->fn.glUniform4iFn(location, x, y, z, w);
}

void GLApiBase::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform4ivFn(location, count, v);
}

void GLApiBase::glUniform4uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2,
                               GLuint v3) {
  driver_->fn.glUniform4uiFn(location, v0, v1, v2, v3);
}

void GLApiBase::glUniform4uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform4uivFn(location, count, v);
}

void GLApiBase::glUniformBlockBindingFn(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) {
  driver_->fn.glUniformBlockBindingFn(program, uniformBlockIndex,
                                      uniformBlockBinding);
}

void GLApiBase::glUniformMatrix2fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix2x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x3fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix2x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix3fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x3fvFn(location, count, transpose, value);
}

GLboolean GLApiBase::glUnmapBufferFn(GLenum target) {
  return driver_->fn.glUnmapBufferFn(target);
}

void GLApiBase::glUseProgramFn(GLuint program) {
  driver_->fn.glUseProgramFn(program);
}

void GLApiBase::glUseProgramStagesFn(GLuint pipeline,
                                     GLbitfield stages,
                                     GLuint program) {
  driver_->fn.glUseProgramStagesFn(pipeline, stages, program);
}

void GLApiBase::glValidateProgramFn(GLuint program) {
  driver_->fn.glValidateProgramFn(program);
}

void GLApiBase::glValidateProgramPipelineFn(GLuint pipeline) {
  driver_->fn.glValidateProgramPipelineFn(pipeline);
}

void GLApiBase::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  driver_->fn.glVertexAttrib1fFn(indx, x);
}

void GLApiBase::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib1fvFn(indx, values);
}

void GLApiBase::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  driver_->fn.glVertexAttrib2fFn(indx, x, y);
}

void GLApiBase::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib2fvFn(indx, values);
}

void GLApiBase::glVertexAttrib3fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  driver_->fn.glVertexAttrib3fFn(indx, x, y, z);
}

void GLApiBase::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib3fvFn(indx, values);
}

void GLApiBase::glVertexAttrib4fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  driver_->fn.glVertexAttrib4fFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib4fvFn(indx, values);
}

void GLApiBase::glVertexAttribBindingFn(GLuint attribindex,
                                        GLuint bindingindex) {
  driver_->fn.glVertexAttribBindingFn(attribindex, bindingindex);
}

void GLApiBase::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  driver_->fn.glVertexAttribDivisorANGLEFn(index, divisor);
}

void GLApiBase::glVertexAttribFormatFn(GLuint attribindex,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLuint relativeoffset) {
  driver_->fn.glVertexAttribFormatFn(attribindex, size, type, normalized,
                                     relativeoffset);
}

void GLApiBase::glVertexAttribI4iFn(GLuint indx,
                                    GLint x,
                                    GLint y,
                                    GLint z,
                                    GLint w) {
  driver_->fn.glVertexAttribI4iFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  driver_->fn.glVertexAttribI4ivFn(indx, values);
}

void GLApiBase::glVertexAttribI4uiFn(GLuint indx,
                                     GLuint x,
                                     GLuint y,
                                     GLuint z,
                                     GLuint w) {
  driver_->fn.glVertexAttribI4uiFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  driver_->fn.glVertexAttribI4uivFn(indx, values);
}

void GLApiBase::glVertexAttribIFormatFn(GLuint attribindex,
                                        GLint size,
                                        GLenum type,
                                        GLuint relativeoffset) {
  driver_->fn.glVertexAttribIFormatFn(attribindex, size, type, relativeoffset);
}

void GLApiBase::glVertexAttribIPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLsizei stride,
                                         const void* ptr) {
  driver_->fn.glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

void GLApiBase::glVertexAttribPointerFn(GLuint indx,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLsizei stride,
                                        const void* ptr) {
  driver_->fn.glVertexAttribPointerFn(indx, size, type, normalized, stride,
                                      ptr);
}

void GLApiBase::glVertexBindingDivisorFn(GLuint bindingindex, GLuint divisor) {
  driver_->fn.glVertexBindingDivisorFn(bindingindex, divisor);
}

void GLApiBase::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glViewportFn(x, y, width, height);
}

void GLApiBase::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  driver_->fn.glWaitSyncFn(sync, flags, timeout);
}

void GLApiBase::glWindowRectanglesEXTFn(GLenum mode,
                                        GLsizei n,
                                        const GLint* box) {
  driver_->fn.glWindowRectanglesEXTFn(mode, n, box);
}

void TraceGLApi::glActiveShaderProgramFn(GLuint pipeline, GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glActiveShaderProgram")
  gl_api_->glActiveShaderProgramFn(pipeline, program);
}

void TraceGLApi::glActiveTextureFn(GLenum texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glActiveTexture")
  gl_api_->glActiveTextureFn(texture);
}

void TraceGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glApplyFramebufferAttachmentCMAAINTEL")
  gl_api_->glApplyFramebufferAttachmentCMAAINTELFn();
}

void TraceGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glAttachShader")
  gl_api_->glAttachShaderFn(program, shader);
}

void TraceGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginQuery")
  gl_api_->glBeginQueryFn(target, id);
}

void TraceGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginTransformFeedback")
  gl_api_->glBeginTransformFeedbackFn(primitiveMode);
}

void TraceGLApi::glBindAttribLocationFn(GLuint program,
                                        GLuint index,
                                        const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindAttribLocation")
  gl_api_->glBindAttribLocationFn(program, index, name);
}

void TraceGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBuffer")
  gl_api_->glBindBufferFn(target, buffer);
}

void TraceGLApi::glBindBufferBaseFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferBase")
  gl_api_->glBindBufferBaseFn(target, index, buffer);
}

void TraceGLApi::glBindBufferRangeFn(GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferRange")
  gl_api_->glBindBufferRangeFn(target, index, buffer, offset, size);
}

void TraceGLApi::glBindFragDataLocationFn(GLuint program,
                                          GLuint colorNumber,
                                          const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFragDataLocation")
  gl_api_->glBindFragDataLocationFn(program, colorNumber, name);
}

void TraceGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                 GLuint colorNumber,
                                                 GLuint index,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindFragDataLocationIndexed")
  gl_api_->glBindFragDataLocationIndexedFn(program, colorNumber, index, name);
}

void TraceGLApi::glBindFragmentInputLocationCHROMIUMFn(GLuint program,
                                                       GLint location,
                                                       const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glBindFragmentInputLocationCHROMIUM")
  gl_api_->glBindFragmentInputLocationCHROMIUMFn(program, location, name);
}

void TraceGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFramebufferEXT")
  gl_api_->glBindFramebufferEXTFn(target, framebuffer);
}

void TraceGLApi::glBindImageTextureEXTFn(GLuint index,
                                         GLuint texture,
                                         GLint level,
                                         GLboolean layered,
                                         GLint layer,
                                         GLenum access,
                                         GLint format) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindImageTextureEXT")
  gl_api_->glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                   access, format);
}

void TraceGLApi::glBindProgramPipelineFn(GLuint pipeline) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindProgramPipeline")
  gl_api_->glBindProgramPipelineFn(pipeline);
}

void TraceGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindRenderbufferEXT")
  gl_api_->glBindRenderbufferEXTFn(target, renderbuffer);
}

void TraceGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindSampler")
  gl_api_->glBindSamplerFn(unit, sampler);
}

void TraceGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTexture")
  gl_api_->glBindTextureFn(target, texture);
}

void TraceGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTransformFeedback")
  gl_api_->glBindTransformFeedbackFn(target, id);
}

void TraceGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                 GLint location,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindUniformLocationCHROMIUM")
  gl_api_->glBindUniformLocationCHROMIUMFn(program, location, name);
}

void TraceGLApi::glBindVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindVertexArrayOES")
  gl_api_->glBindVertexArrayOESFn(array);
}

void TraceGLApi::glBindVertexBufferFn(GLuint bindingindex,
                                      GLuint buffer,
                                      GLintptr offset,
                                      GLsizei stride) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindVertexBuffer")
  gl_api_->glBindVertexBufferFn(bindingindex, buffer, offset, stride);
}

void TraceGLApi::glBlendBarrierKHRFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendBarrierKHR")
  gl_api_->glBlendBarrierKHRFn();
}

void TraceGLApi::glBlendColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendColor")
  gl_api_->glBlendColorFn(red, green, blue, alpha);
}

void TraceGLApi::glBlendEquationFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquation")
  gl_api_->glBlendEquationFn(mode);
}

void TraceGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquationSeparate")
  gl_api_->glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void TraceGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFunc")
  gl_api_->glBlendFuncFn(sfactor, dfactor);
}

void TraceGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                       GLenum dstRGB,
                                       GLenum srcAlpha,
                                       GLenum dstAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFuncSeparate")
  gl_api_->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void TraceGLApi::glBlitFramebufferFn(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlitFramebuffer")
  gl_api_->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

void TraceGLApi::glBufferDataFn(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferData")
  gl_api_->glBufferDataFn(target, size, data, usage);
}

void TraceGLApi::glBufferSubDataFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferSubData")
  gl_api_->glBufferSubDataFn(target, offset, size, data);
}

GLenum TraceGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCheckFramebufferStatusEXT")
  return gl_api_->glCheckFramebufferStatusEXTFn(target);
}

void TraceGLApi::glClearFn(GLbitfield mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClear")
  gl_api_->glClearFn(mask);
}

void TraceGLApi::glClearBufferfiFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat depth,
                                   GLint stencil) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfi")
  gl_api_->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void TraceGLApi::glClearBufferfvFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfv")
  gl_api_->glClearBufferfvFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearBufferivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferiv")
  gl_api_->glClearBufferivFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearBufferuivFn(GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferuiv")
  gl_api_->glClearBufferuivFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearColor")
  gl_api_->glClearColorFn(red, green, blue, alpha);
}

void TraceGLApi::glClearDepthFn(GLclampd depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepth")
  gl_api_->glClearDepthFn(depth);
}

void TraceGLApi::glClearDepthfFn(GLclampf depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepthf")
  gl_api_->glClearDepthfFn(depth);
}

void TraceGLApi::glClearStencilFn(GLint s) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearStencil")
  gl_api_->glClearStencilFn(s);
}

GLenum TraceGLApi::glClientWaitSyncFn(GLsync sync,
                                      GLbitfield flags,
                                      GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClientWaitSync")
  return gl_api_->glClientWaitSyncFn(sync, flags, timeout);
}

void TraceGLApi::glColorMaskFn(GLboolean red,
                               GLboolean green,
                               GLboolean blue,
                               GLboolean alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glColorMask")
  gl_api_->glColorMaskFn(red, green, blue, alpha);
}

void TraceGLApi::glCompileShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompileShader")
  gl_api_->glCompileShaderFn(shader);
}

void TraceGLApi::glCompressedTexImage2DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage2D")
  gl_api_->glCompressedTexImage2DFn(target, level, internalformat, width,
                                    height, border, imageSize, data);
}

void TraceGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedTexImage2DRobustANGLE")
  gl_api_->glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                               width, height, border, imageSize,
                                               dataSize, data);
}

void TraceGLApi::glCompressedTexImage3DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage3D")
  gl_api_->glCompressedTexImage3DFn(target, level, internalformat, width,
                                    height, depth, border, imageSize, data);
}

void TraceGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedTexImage3DRobustANGLE")
  gl_api_->glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                               width, height, depth, border,
                                               imageSize, dataSize, data);
}

void TraceGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage2D")
  gl_api_->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

void TraceGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glCompressedTexSubImage2DRobustANGLE")
  gl_api_->glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

void TraceGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage3D")
  gl_api_->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, imageSize,
                                       data);
}

void TraceGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLint zoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glCompressedTexSubImage3DRobustANGLE")
  gl_api_->glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

void TraceGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                       GLenum writeTarget,
                                       GLintptr readOffset,
                                       GLintptr writeOffset,
                                       GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyBufferSubData")
  gl_api_->glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                 writeOffset, size);
}

void TraceGLApi::glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                            GLint sourceLevel,
                                            GLenum destTarget,
                                            GLuint destId,
                                            GLint destLevel,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLboolean unpackFlipY,
                                            GLboolean unpackPremultiplyAlpha,
                                            GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopySubTextureCHROMIUM")
  gl_api_->glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

void TraceGLApi::glCopyTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexImage2D")
  gl_api_->glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                              height, border);
}

void TraceGLApi::glCopyTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage2D")
  gl_api_->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

void TraceGLApi::glCopyTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage3D")
  gl_api_->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                                 width, height);
}

void TraceGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                         GLint sourceLevel,
                                         GLenum destTarget,
                                         GLuint destId,
                                         GLint destLevel,
                                         GLint internalFormat,
                                         GLenum destType,
                                         GLboolean unpackFlipY,
                                         GLboolean unpackPremultiplyAlpha,
                                         GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTextureCHROMIUM")
  gl_api_->glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void TraceGLApi::glCoverageModulationNVFn(GLenum components) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverageModulationNV")
  gl_api_->glCoverageModulationNVFn(components);
}

void TraceGLApi::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathInstancedNV")
  gl_api_->glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths, pathBase,
                                        coverMode, transformType,
                                        transformValues);
}

void TraceGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathNV")
  gl_api_->glCoverFillPathNVFn(path, coverMode);
}

void TraceGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCoverStrokePathInstancedNV")
  gl_api_->glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, coverMode, transformType,
                                          transformValues);
}

void TraceGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverStrokePathNV")
  gl_api_->glCoverStrokePathNVFn(name, coverMode);
}

GLuint TraceGLApi::glCreateProgramFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateProgram")
  return gl_api_->glCreateProgramFn();
}

GLuint TraceGLApi::glCreateShaderFn(GLenum type) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateShader")
  return gl_api_->glCreateShaderFn(type);
}

GLuint TraceGLApi::glCreateShaderProgramvFn(GLenum type,
                                            GLsizei count,
                                            const char* const* strings) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateShaderProgramv")
  return gl_api_->glCreateShaderProgramvFn(type, count, strings);
}

void TraceGLApi::glCullFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCullFace")
  gl_api_->glCullFaceFn(mode);
}

void TraceGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                          const void* userParam) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageCallback")
  gl_api_->glDebugMessageCallbackFn(callback, userParam);
}

void TraceGLApi::glDebugMessageControlFn(GLenum source,
                                         GLenum type,
                                         GLenum severity,
                                         GLsizei count,
                                         const GLuint* ids,
                                         GLboolean enabled) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageControl")
  gl_api_->glDebugMessageControlFn(source, type, severity, count, ids, enabled);
}

void TraceGLApi::glDebugMessageInsertFn(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const char* buf) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageInsert")
  gl_api_->glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

void TraceGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteBuffersARB")
  gl_api_->glDeleteBuffersARBFn(n, buffers);
}

void TraceGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesAPPLE")
  gl_api_->glDeleteFencesAPPLEFn(n, fences);
}

void TraceGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesNV")
  gl_api_->glDeleteFencesNVFn(n, fences);
}

void TraceGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                           const GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFramebuffersEXT")
  gl_api_->glDeleteFramebuffersEXTFn(n, framebuffers);
}

void TraceGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeletePathsNV")
  gl_api_->glDeletePathsNVFn(path, range);
}

void TraceGLApi::glDeleteProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteProgram")
  gl_api_->glDeleteProgramFn(program);
}

void TraceGLApi::glDeleteProgramPipelinesFn(GLsizei n,
                                            const GLuint* pipelines) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteProgramPipelines")
  gl_api_->glDeleteProgramPipelinesFn(n, pipelines);
}

void TraceGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteQueries")
  gl_api_->glDeleteQueriesFn(n, ids);
}

void TraceGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                            const GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteRenderbuffersEXT")
  gl_api_->glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void TraceGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSamplers")
  gl_api_->glDeleteSamplersFn(n, samplers);
}

void TraceGLApi::glDeleteShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteShader")
  gl_api_->glDeleteShaderFn(shader);
}

void TraceGLApi::glDeleteSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSync")
  gl_api_->glDeleteSyncFn(sync);
}

void TraceGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTextures")
  gl_api_->glDeleteTexturesFn(n, textures);
}

void TraceGLApi::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTransformFeedbacks")
  gl_api_->glDeleteTransformFeedbacksFn(n, ids);
}

void TraceGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteVertexArraysOES")
  gl_api_->glDeleteVertexArraysOESFn(n, arrays);
}

void TraceGLApi::glDepthFuncFn(GLenum func) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthFunc")
  gl_api_->glDepthFuncFn(func);
}

void TraceGLApi::glDepthMaskFn(GLboolean flag) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthMask")
  gl_api_->glDepthMaskFn(flag);
}

void TraceGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRange")
  gl_api_->glDepthRangeFn(zNear, zFar);
}

void TraceGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRangef")
  gl_api_->glDepthRangefFn(zNear, zFar);
}

void TraceGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDetachShader")
  gl_api_->glDetachShaderFn(program, shader);
}

void TraceGLApi::glDisableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisable")
  gl_api_->glDisableFn(cap);
}

void TraceGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisableVertexAttribArray")
  gl_api_->glDisableVertexAttribArrayFn(index);
}

void TraceGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDiscardFramebufferEXT")
  gl_api_->glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void TraceGLApi::glDispatchComputeFn(GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDispatchCompute")
  gl_api_->glDispatchComputeFn(numGroupsX, numGroupsY, numGroupsZ);
}

void TraceGLApi::glDispatchComputeIndirectFn(GLintptr indirect) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDispatchComputeIndirect")
  gl_api_->glDispatchComputeIndirectFn(indirect);
}

void TraceGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArrays")
  gl_api_->glDrawArraysFn(mode, first, count);
}

void TraceGLApi::glDrawArraysIndirectFn(GLenum mode, const void* indirect) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArraysIndirect")
  gl_api_->glDrawArraysIndirectFn(mode, indirect);
}

void TraceGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArraysInstancedANGLE")
  gl_api_->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void TraceGLApi::glDrawBufferFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffer")
  gl_api_->glDrawBufferFn(mode);
}

void TraceGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffersARB")
  gl_api_->glDrawBuffersARBFn(n, bufs);
}

void TraceGLApi::glDrawElementsFn(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawElements")
  gl_api_->glDrawElementsFn(mode, count, type, indices);
}

void TraceGLApi::glDrawElementsIndirectFn(GLenum mode,
                                          GLenum type,
                                          const void* indirect) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawElementsIndirect")
  gl_api_->glDrawElementsIndirectFn(mode, type, indirect);
}

void TraceGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices,
                                                GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glDrawElementsInstancedANGLE")
  gl_api_->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                          primcount);
}

void TraceGLApi::glDrawRangeElementsFn(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawRangeElements")
  gl_api_->glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

void TraceGLApi::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                          GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glEGLImageTargetRenderbufferStorageOES")
  gl_api_->glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void TraceGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glEGLImageTargetTexture2DOES")
  gl_api_->glEGLImageTargetTexture2DOESFn(target, image);
}

void TraceGLApi::glEnableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnable")
  gl_api_->glEnableFn(cap);
}

void TraceGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnableVertexAttribArray")
  gl_api_->glEnableVertexAttribArrayFn(index);
}

void TraceGLApi::glEndQueryFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndQuery")
  gl_api_->glEndQueryFn(target);
}

void TraceGLApi::glEndTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndTransformFeedback")
  gl_api_->glEndTransformFeedbackFn();
}

GLsync TraceGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFenceSync")
  return gl_api_->glFenceSyncFn(condition, flags);
}

void TraceGLApi::glFinishFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinish")
  gl_api_->glFinishFn();
}

void TraceGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceAPPLE")
  gl_api_->glFinishFenceAPPLEFn(fence);
}

void TraceGLApi::glFinishFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceNV")
  gl_api_->glFinishFenceNVFn(fence);
}

void TraceGLApi::glFlushFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlush")
  gl_api_->glFlushFn();
}

void TraceGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                            GLintptr offset,
                                            GLsizeiptr length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlushMappedBufferRange")
  gl_api_->glFlushMappedBufferRangeFn(target, offset, length);
}

void TraceGLApi::glFramebufferParameteriFn(GLenum target,
                                           GLenum pname,
                                           GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferParameteri")
  gl_api_->glFramebufferParameteriFn(target, pname, param);
}

void TraceGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glFramebufferRenderbufferEXT")
  gl_api_->glFramebufferRenderbufferEXTFn(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

void TraceGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTexture2DEXT")
  gl_api_->glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                       level);
}

void TraceGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glFramebufferTexture2DMultisampleEXT")
  gl_api_->glFramebufferTexture2DMultisampleEXTFn(target, attachment, textarget,
                                                  texture, level, samples);
}

void TraceGLApi::glFramebufferTextureLayerFn(GLenum target,
                                             GLenum attachment,
                                             GLuint texture,
                                             GLint level,
                                             GLint layer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTextureLayer")
  gl_api_->glFramebufferTextureLayerFn(target, attachment, texture, level,
                                       layer);
}

void TraceGLApi::glFramebufferTextureMultiviewLayeredANGLEFn(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glFramebufferTextureMultiviewLayeredANGLE")
  gl_api_->glFramebufferTextureMultiviewLayeredANGLEFn(
      target, attachment, texture, level, baseViewIndex, numViews);
}

void TraceGLApi::glFrontFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFrontFace")
  gl_api_->glFrontFaceFn(mode);
}

void TraceGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenBuffersARB")
  gl_api_->glGenBuffersARBFn(n, buffers);
}

void TraceGLApi::glGenerateMipmapEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenerateMipmapEXT")
  gl_api_->glGenerateMipmapEXTFn(target);
}

void TraceGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesAPPLE")
  gl_api_->glGenFencesAPPLEFn(n, fences);
}

void TraceGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesNV")
  gl_api_->glGenFencesNVFn(n, fences);
}

void TraceGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFramebuffersEXT")
  gl_api_->glGenFramebuffersEXTFn(n, framebuffers);
}

GLuint TraceGLApi::glGenPathsNVFn(GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenPathsNV")
  return gl_api_->glGenPathsNVFn(range);
}

GLuint TraceGLApi::glGenProgramPipelinesFn(GLsizei n, GLuint* pipelines) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenProgramPipelines")
  return gl_api_->glGenProgramPipelinesFn(n, pipelines);
}

void TraceGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenQueries")
  gl_api_->glGenQueriesFn(n, ids);
}

void TraceGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenRenderbuffersEXT")
  gl_api_->glGenRenderbuffersEXTFn(n, renderbuffers);
}

void TraceGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenSamplers")
  gl_api_->glGenSamplersFn(n, samplers);
}

void TraceGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTextures")
  gl_api_->glGenTexturesFn(n, textures);
}

void TraceGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTransformFeedbacks")
  gl_api_->glGenTransformFeedbacksFn(n, ids);
}

void TraceGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenVertexArraysOES")
  gl_api_->glGenVertexArraysOESFn(n, arrays);
}

void TraceGLApi::glGetActiveAttribFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveAttrib")
  gl_api_->glGetActiveAttribFn(program, index, bufsize, length, size, type,
                               name);
}

void TraceGLApi::glGetActiveUniformFn(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniform")
  gl_api_->glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                name);
}

void TraceGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLenum pname,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformBlockiv")
  gl_api_->glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                       params);
}

void TraceGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetActiveUniformBlockivRobustANGLE")
  gl_api_->glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void TraceGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                               GLuint uniformBlockIndex,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetActiveUniformBlockName")
  gl_api_->glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                         length, uniformBlockName);
}

void TraceGLApi::glGetActiveUniformsivFn(GLuint program,
                                         GLsizei uniformCount,
                                         const GLuint* uniformIndices,
                                         GLenum pname,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformsiv")
  gl_api_->glGetActiveUniformsivFn(program, uniformCount, uniformIndices, pname,
                                   params);
}

void TraceGLApi::glGetAttachedShadersFn(GLuint program,
                                        GLsizei maxcount,
                                        GLsizei* count,
                                        GLuint* shaders) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttachedShaders")
  gl_api_->glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint TraceGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttribLocation")
  return gl_api_->glGetAttribLocationFn(program, name);
}

void TraceGLApi::glGetBooleani_vFn(GLenum target,
                                   GLuint index,
                                   GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleani_v")
  gl_api_->glGetBooleani_vFn(target, index, data);
}

void TraceGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleani_vRobustANGLE")
  gl_api_->glGetBooleani_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanv")
  gl_api_->glGetBooleanvFn(pname, params);
}

void TraceGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanvRobustANGLE")
  gl_api_->glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetBufferParameteri64vRobustANGLE")
  gl_api_->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

void TraceGLApi::glGetBufferParameterivFn(GLenum target,
                                          GLenum pname,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBufferParameteriv")
  gl_api_->glGetBufferParameterivFn(target, pname, params);
}

void TraceGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferParameterivRobustANGLE")
  gl_api_->glGetBufferParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void TraceGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferPointervRobustANGLE")
  gl_api_->glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

GLuint TraceGLApi::glGetDebugMessageLogFn(GLuint count,
                                          GLsizei bufSize,
                                          GLenum* sources,
                                          GLenum* types,
                                          GLuint* ids,
                                          GLenum* severities,
                                          GLsizei* lengths,
                                          char* messageLog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetDebugMessageLog")
  return gl_api_->glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                         severities, lengths, messageLog);
}

GLenum TraceGLApi::glGetErrorFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetError")
  return gl_api_->glGetErrorFn();
}

void TraceGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFenceivNV")
  gl_api_->glGetFenceivNVFn(fence, pname, params);
}

void TraceGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatv")
  gl_api_->glGetFloatvFn(pname, params);
}

void TraceGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatvRobustANGLE")
  gl_api_->glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

GLint TraceGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataIndex")
  return gl_api_->glGetFragDataIndexFn(program, name);
}

GLint TraceGLApi::glGetFragDataLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataLocation")
  return gl_api_->glGetFragDataLocationFn(program, name);
}

void TraceGLApi::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivEXT")
  gl_api_->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                      params);
}

void TraceGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivRobustANGLE")
  gl_api_->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

void TraceGLApi::glGetFramebufferParameterivFn(GLenum target,
                                               GLenum pname,
                                               GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetFramebufferParameteriv")
  gl_api_->glGetFramebufferParameterivFn(target, pname, params);
}

void TraceGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferParameterivRobustANGLE")
  gl_api_->glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

GLenum TraceGLApi::glGetGraphicsResetStatusARBFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetGraphicsResetStatusARB")
  return gl_api_->glGetGraphicsResetStatusARBFn();
}

void TraceGLApi::glGetInteger64i_vFn(GLenum target,
                                     GLuint index,
                                     GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64i_v")
  gl_api_->glGetInteger64i_vFn(target, index, data);
}

void TraceGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInteger64i_vRobustANGLE")
  gl_api_->glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64v")
  gl_api_->glGetInteger64vFn(pname, params);
}

void TraceGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64vRobustANGLE")
  gl_api_->glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_v")
  gl_api_->glGetIntegeri_vFn(target, index, data);
}

void TraceGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_vRobustANGLE")
  gl_api_->glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegerv")
  gl_api_->glGetIntegervFn(pname, params);
}

void TraceGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegervRobustANGLE")
  gl_api_->glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetInternalformativFn(GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInternalformativ")
  gl_api_->glGetInternalformativFn(target, internalformat, pname, bufSize,
                                   params);
}

void TraceGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                    GLenum internalformat,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInternalformativRobustANGLE")
  gl_api_->glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                              bufSize, length, params);
}

void TraceGLApi::glGetMultisamplefvFn(GLenum pname,
                                      GLuint index,
                                      GLfloat* val) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetMultisamplefv")
  gl_api_->glGetMultisamplefvFn(pname, index, val);
}

void TraceGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* val) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetMultisamplefvRobustANGLE")
  gl_api_->glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length, val);
}

void TraceGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformfvRobustANGLE")
  gl_api_->glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformivRobustANGLE")
  gl_api_->glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetnUniformuivRobustANGLE")
  gl_api_->glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                         params);
}

void TraceGLApi::glGetObjectLabelFn(GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectLabel")
  gl_api_->glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

void TraceGLApi::glGetObjectPtrLabelFn(void* ptr,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectPtrLabel")
  gl_api_->glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

void TraceGLApi::glGetPointervFn(GLenum pname, void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetPointerv")
  gl_api_->glGetPointervFn(pname, params);
}

void TraceGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetPointervRobustANGLERobustANGLE")
  gl_api_->glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                 params);
}

void TraceGLApi::glGetProgramBinaryFn(GLuint program,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLenum* binaryFormat,
                                      GLvoid* binary) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramBinary")
  gl_api_->glGetProgramBinaryFn(program, bufSize, length, binaryFormat, binary);
}

void TraceGLApi::glGetProgramInfoLogFn(GLuint program,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInfoLog")
  gl_api_->glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void TraceGLApi::glGetProgramInterfaceivFn(GLuint program,
                                           GLenum programInterface,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInterfaceiv")
  gl_api_->glGetProgramInterfaceivFn(program, programInterface, pname, params);
}

void TraceGLApi::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                      GLenum programInterface,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetProgramInterfaceivRobustANGLE")
  gl_api_->glGetProgramInterfaceivRobustANGLEFn(program, programInterface,
                                                pname, bufSize, length, params);
}

void TraceGLApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramiv")
  gl_api_->glGetProgramivFn(program, pname, params);
}

void TraceGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramivRobustANGLE")
  gl_api_->glGetProgramivRobustANGLEFn(program, pname, bufSize, length, params);
}

void TraceGLApi::glGetProgramPipelineInfoLogFn(GLuint pipeline,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLchar* infoLog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetProgramPipelineInfoLog")
  gl_api_->glGetProgramPipelineInfoLogFn(pipeline, bufSize, length, infoLog);
}

void TraceGLApi::glGetProgramPipelineivFn(GLuint pipeline,
                                          GLenum pname,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramPipelineiv")
  gl_api_->glGetProgramPipelineivFn(pipeline, pname, params);
}

void TraceGLApi::glGetProgramResourceIndexFn(GLuint program,
                                             GLenum programInterface,
                                             const GLchar* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceIndex")
  gl_api_->glGetProgramResourceIndexFn(program, programInterface, name);
}

void TraceGLApi::glGetProgramResourceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei propCount,
                                          const GLenum* props,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceiv")
  gl_api_->glGetProgramResourceivFn(program, programInterface, index, propCount,
                                    props, bufSize, length, params);
}

GLint TraceGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                 GLenum programInterface,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetProgramResourceLocation")
  return gl_api_->glGetProgramResourceLocationFn(program, programInterface,
                                                 name);
}

void TraceGLApi::glGetProgramResourceNameFn(GLuint program,
                                            GLenum programInterface,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLchar* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceName")
  gl_api_->glGetProgramResourceNameFn(program, programInterface, index, bufSize,
                                      length, name);
}

void TraceGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryiv")
  gl_api_->glGetQueryivFn(target, pname, params);
}

void TraceGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryivRobustANGLE")
  gl_api_->glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

void TraceGLApi::glGetQueryObjecti64vFn(GLuint id,
                                        GLenum pname,
                                        GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjecti64v")
  gl_api_->glGetQueryObjecti64vFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjecti64vRobustANGLE")
  gl_api_->glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectiv")
  gl_api_->glGetQueryObjectivFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectivRobustANGLE")
  gl_api_->glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length, params);
}

void TraceGLApi::glGetQueryObjectui64vFn(GLuint id,
                                         GLenum pname,
                                         GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectui64v")
  gl_api_->glGetQueryObjectui64vFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectui64vRobustANGLE")
  gl_api_->glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetQueryObjectuivFn(GLuint id,
                                       GLenum pname,
                                       GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectuiv")
  gl_api_->glGetQueryObjectuivFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectuivRobustANGLE")
  gl_api_->glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length, params);
}

void TraceGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetRenderbufferParameterivEXT")
  gl_api_->glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void TraceGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetRenderbufferParameterivRobustANGLE")
  gl_api_->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                     length, params);
}

void TraceGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameterfv")
  gl_api_->glGetSamplerParameterfvFn(sampler, pname, params);
}

void TraceGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterfvRobustANGLE")
  gl_api_->glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void TraceGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIivRobustANGLE")
  gl_api_->glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIuivRobustANGLE")
  gl_api_->glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  length, params);
}

void TraceGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameteriv")
  gl_api_->glGetSamplerParameterivFn(sampler, pname, params);
}

void TraceGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterivRobustANGLE")
  gl_api_->glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void TraceGLApi::glGetShaderInfoLogFn(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderInfoLog")
  gl_api_->glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void TraceGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderiv")
  gl_api_->glGetShaderivFn(shader, pname, params);
}

void TraceGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderivRobustANGLE")
  gl_api_->glGetShaderivRobustANGLEFn(shader, pname, bufSize, length, params);
}

void TraceGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                              GLenum precisiontype,
                                              GLint* range,
                                              GLint* precision) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderPrecisionFormat")
  gl_api_->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                        precision);
}

void TraceGLApi::glGetShaderSourceFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderSource")
  gl_api_->glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* TraceGLApi::glGetStringFn(GLenum name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetString")
  return gl_api_->glGetStringFn(name);
}

const GLubyte* TraceGLApi::glGetStringiFn(GLenum name, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetStringi")
  return gl_api_->glGetStringiFn(name, index);
}

void TraceGLApi::glGetSyncivFn(GLsync sync,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSynciv")
  gl_api_->glGetSyncivFn(sync, pname, bufSize, length, values);
}

void TraceGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameterfv")
  gl_api_->glGetTexLevelParameterfvFn(target, level, pname, params);
}

void TraceGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterfvRobustANGLE")
  gl_api_->glGetTexLevelParameterfvRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetTexLevelParameterivFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameteriv")
  gl_api_->glGetTexLevelParameterivFn(target, level, pname, params);
}

void TraceGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterivRobustANGLE")
  gl_api_->glGetTexLevelParameterivRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetTexParameterfvFn(GLenum target,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameterfv")
  gl_api_->glGetTexParameterfvFn(target, pname, params);
}

void TraceGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterfvRobustANGLE")
  gl_api_->glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIivRobustANGLE")
  gl_api_->glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIuivRobustANGLE")
  gl_api_->glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetTexParameterivFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameteriv")
  gl_api_->glGetTexParameterivFn(target, pname, params);
}

void TraceGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterivRobustANGLE")
  gl_api_->glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLsizei* size,
                                                 GLenum* type,
                                                 char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTransformFeedbackVarying")
  gl_api_->glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                           size, type, name);
}

void TraceGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTranslatedShaderSourceANGLE")
  gl_api_->glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length, source);
}

GLuint TraceGLApi::glGetUniformBlockIndexFn(GLuint program,
                                            const char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformBlockIndex")
  return gl_api_->glGetUniformBlockIndexFn(program, uniformBlockName);
}

void TraceGLApi::glGetUniformfvFn(GLuint program,
                                  GLint location,
                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfv")
  gl_api_->glGetUniformfvFn(program, location, params);
}

void TraceGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfvRobustANGLE")
  gl_api_->glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

void TraceGLApi::glGetUniformIndicesFn(GLuint program,
                                       GLsizei uniformCount,
                                       const char* const* uniformNames,
                                       GLuint* uniformIndices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformIndices")
  gl_api_->glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                 uniformIndices);
}

void TraceGLApi::glGetUniformivFn(GLuint program,
                                  GLint location,
                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformiv")
  gl_api_->glGetUniformivFn(program, location, params);
}

void TraceGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformivRobustANGLE")
  gl_api_->glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

GLint TraceGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformLocation")
  return gl_api_->glGetUniformLocationFn(program, name);
}

void TraceGLApi::glGetUniformuivFn(GLuint program,
                                   GLint location,
                                   GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuiv")
  gl_api_->glGetUniformuivFn(program, location, params);
}

void TraceGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuivRobustANGLE")
  gl_api_->glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetVertexAttribfvFn(GLuint index,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribfv")
  gl_api_->glGetVertexAttribfvFn(index, pname, params);
}

void TraceGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribfvRobustANGLE")
  gl_api_->glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIivRobustANGLE")
  gl_api_->glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIuivRobustANGLE")
  gl_api_->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetVertexAttribivFn(GLuint index,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribiv")
  gl_api_->glGetVertexAttribivFn(index, pname, params);
}

void TraceGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribivRobustANGLE")
  gl_api_->glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetVertexAttribPointervFn(GLuint index,
                                             GLenum pname,
                                             void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribPointerv")
  gl_api_->glGetVertexAttribPointervFn(index, pname, pointer);
}

void TraceGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetVertexAttribPointervRobustANGLE")
  gl_api_->glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize, length,
                                                  pointer);
}

void TraceGLApi::glHintFn(GLenum target, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glHint")
  gl_api_->glHintFn(target, mode);
}

void TraceGLApi::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInsertEventMarkerEXT")
  gl_api_->glInsertEventMarkerEXTFn(length, marker);
}

void TraceGLApi::glInvalidateFramebufferFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateFramebuffer")
  gl_api_->glInvalidateFramebufferFn(target, numAttachments, attachments);
}

void TraceGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments,
                                              GLint x,
                                              GLint y,
                                              GLint width,
                                              GLint height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateSubFramebuffer")
  gl_api_->glInvalidateSubFramebufferFn(target, numAttachments, attachments, x,
                                        y, width, height);
}

GLboolean TraceGLApi::glIsBufferFn(GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsBuffer")
  return gl_api_->glIsBufferFn(buffer);
}

GLboolean TraceGLApi::glIsEnabledFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsEnabled")
  return gl_api_->glIsEnabledFn(cap);
}

GLboolean TraceGLApi::glIsFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceAPPLE")
  return gl_api_->glIsFenceAPPLEFn(fence);
}

GLboolean TraceGLApi::glIsFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceNV")
  return gl_api_->glIsFenceNVFn(fence);
}

GLboolean TraceGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFramebufferEXT")
  return gl_api_->glIsFramebufferEXTFn(framebuffer);
}

GLboolean TraceGLApi::glIsPathNVFn(GLuint path) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsPathNV")
  return gl_api_->glIsPathNVFn(path);
}

GLboolean TraceGLApi::glIsProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsProgram")
  return gl_api_->glIsProgramFn(program);
}

GLboolean TraceGLApi::glIsProgramPipelineFn(GLuint pipeline) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsProgramPipeline")
  return gl_api_->glIsProgramPipelineFn(pipeline);
}

GLboolean TraceGLApi::glIsQueryFn(GLuint query) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsQuery")
  return gl_api_->glIsQueryFn(query);
}

GLboolean TraceGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsRenderbufferEXT")
  return gl_api_->glIsRenderbufferEXTFn(renderbuffer);
}

GLboolean TraceGLApi::glIsSamplerFn(GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSampler")
  return gl_api_->glIsSamplerFn(sampler);
}

GLboolean TraceGLApi::glIsShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsShader")
  return gl_api_->glIsShaderFn(shader);
}

GLboolean TraceGLApi::glIsSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSync")
  return gl_api_->glIsSyncFn(sync);
}

GLboolean TraceGLApi::glIsTextureFn(GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTexture")
  return gl_api_->glIsTextureFn(texture);
}

GLboolean TraceGLApi::glIsTransformFeedbackFn(GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTransformFeedback")
  return gl_api_->glIsTransformFeedbackFn(id);
}

GLboolean TraceGLApi::glIsVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsVertexArrayOES")
  return gl_api_->glIsVertexArrayOESFn(array);
}

void TraceGLApi::glLineWidthFn(GLfloat width) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLineWidth")
  gl_api_->glLineWidthFn(width);
}

void TraceGLApi::glLinkProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLinkProgram")
  gl_api_->glLinkProgramFn(program);
}

void* TraceGLApi::glMapBufferFn(GLenum target, GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBuffer")
  return gl_api_->glMapBufferFn(target, access);
}

void* TraceGLApi::glMapBufferRangeFn(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBufferRange")
  return gl_api_->glMapBufferRangeFn(target, offset, length, access);
}

void TraceGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadfEXT")
  gl_api_->glMatrixLoadfEXTFn(matrixMode, m);
}

void TraceGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadIdentityEXT")
  gl_api_->glMatrixLoadIdentityEXTFn(matrixMode);
}

void TraceGLApi::glMaxShaderCompilerThreadsKHRFn(GLuint count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glMaxShaderCompilerThreadsKHR")
  gl_api_->glMaxShaderCompilerThreadsKHRFn(count);
}

void TraceGLApi::glMemoryBarrierByRegionFn(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMemoryBarrierByRegion")
  gl_api_->glMemoryBarrierByRegionFn(barriers);
}

void TraceGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMemoryBarrierEXT")
  gl_api_->glMemoryBarrierEXTFn(barriers);
}

void TraceGLApi::glMinSampleShadingFn(GLfloat value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMinSampleShading")
  gl_api_->glMinSampleShadingFn(value);
}

void TraceGLApi::glObjectLabelFn(GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectLabel")
  gl_api_->glObjectLabelFn(identifier, name, length, label);
}

void TraceGLApi::glObjectPtrLabelFn(void* ptr,
                                    GLsizei length,
                                    const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectPtrLabel")
  gl_api_->glObjectPtrLabelFn(ptr, length, label);
}

void TraceGLApi::glPathCommandsNVFn(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathCommandsNV")
  gl_api_->glPathCommandsNVFn(path, numCommands, commands, numCoords, coordType,
                              coords);
}

void TraceGLApi::glPathParameterfNVFn(GLuint path,
                                      GLenum pname,
                                      GLfloat value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameterfNV")
  gl_api_->glPathParameterfNVFn(path, pname, value);
}

void TraceGLApi::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameteriNV")
  gl_api_->glPathParameteriNVFn(path, pname, value);
}

void TraceGLApi::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathStencilFuncNV")
  gl_api_->glPathStencilFuncNVFn(func, ref, mask);
}

void TraceGLApi::glPauseTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPauseTransformFeedback")
  gl_api_->glPauseTransformFeedbackFn();
}

void TraceGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPixelStorei")
  gl_api_->glPixelStoreiFn(pname, param);
}

void TraceGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPointParameteri")
  gl_api_->glPointParameteriFn(pname, param);
}

void TraceGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPolygonMode")
  gl_api_->glPolygonModeFn(face, mode);
}

void TraceGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPolygonOffset")
  gl_api_->glPolygonOffsetFn(factor, units);
}

void TraceGLApi::glPopDebugGroupFn() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopDebugGroup")
  gl_api_->glPopDebugGroupFn();
}

void TraceGLApi::glPopGroupMarkerEXTFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopGroupMarkerEXT")
  gl_api_->glPopGroupMarkerEXTFn();
}

void TraceGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPrimitiveRestartIndex")
  gl_api_->glPrimitiveRestartIndexFn(index);
}

void TraceGLApi::glProgramBinaryFn(GLuint program,
                                   GLenum binaryFormat,
                                   const GLvoid* binary,
                                   GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramBinary")
  gl_api_->glProgramBinaryFn(program, binaryFormat, binary, length);
}

void TraceGLApi::glProgramParameteriFn(GLuint program,
                                       GLenum pname,
                                       GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramParameteri")
  gl_api_->glProgramParameteriFn(program, pname, value);
}

void TraceGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramPathFragmentInputGenNV")
  gl_api_->glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                             components, coeffs);
}

void TraceGLApi::glProgramUniform1fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1f")
  gl_api_->glProgramUniform1fFn(program, location, v0);
}

void TraceGLApi::glProgramUniform1fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1fv")
  gl_api_->glProgramUniform1fvFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform1iFn(GLuint program,
                                      GLint location,
                                      GLint v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1i")
  gl_api_->glProgramUniform1iFn(program, location, v0);
}

void TraceGLApi::glProgramUniform1ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1iv")
  gl_api_->glProgramUniform1ivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform1uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1ui")
  gl_api_->glProgramUniform1uiFn(program, location, v0);
}

void TraceGLApi::glProgramUniform1uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform1uiv")
  gl_api_->glProgramUniform1uivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform2fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2f")
  gl_api_->glProgramUniform2fFn(program, location, v0, v1);
}

void TraceGLApi::glProgramUniform2fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2fv")
  gl_api_->glProgramUniform2fvFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform2iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2i")
  gl_api_->glProgramUniform2iFn(program, location, v0, v1);
}

void TraceGLApi::glProgramUniform2ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2iv")
  gl_api_->glProgramUniform2ivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform2uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2ui")
  gl_api_->glProgramUniform2uiFn(program, location, v0, v1);
}

void TraceGLApi::glProgramUniform2uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform2uiv")
  gl_api_->glProgramUniform2uivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform3fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1,
                                      GLfloat v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3f")
  gl_api_->glProgramUniform3fFn(program, location, v0, v1, v2);
}

void TraceGLApi::glProgramUniform3fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3fv")
  gl_api_->glProgramUniform3fvFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform3iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1,
                                      GLint v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3i")
  gl_api_->glProgramUniform3iFn(program, location, v0, v1, v2);
}

void TraceGLApi::glProgramUniform3ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3iv")
  gl_api_->glProgramUniform3ivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform3uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1,
                                       GLuint v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3ui")
  gl_api_->glProgramUniform3uiFn(program, location, v0, v1, v2);
}

void TraceGLApi::glProgramUniform3uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform3uiv")
  gl_api_->glProgramUniform3uivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform4fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1,
                                      GLfloat v2,
                                      GLfloat v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4f")
  gl_api_->glProgramUniform4fFn(program, location, v0, v1, v2, v3);
}

void TraceGLApi::glProgramUniform4fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4fv")
  gl_api_->glProgramUniform4fvFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform4iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1,
                                      GLint v2,
                                      GLint v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4i")
  gl_api_->glProgramUniform4iFn(program, location, v0, v1, v2, v3);
}

void TraceGLApi::glProgramUniform4ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4iv")
  gl_api_->glProgramUniform4ivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniform4uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1,
                                       GLuint v2,
                                       GLuint v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4ui")
  gl_api_->glProgramUniform4uiFn(program, location, v0, v1, v2, v3);
}

void TraceGLApi::glProgramUniform4uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniform4uiv")
  gl_api_->glProgramUniform4uivFn(program, location, count, value);
}

void TraceGLApi::glProgramUniformMatrix2fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniformMatrix2fv")
  gl_api_->glProgramUniformMatrix2fvFn(program, location, count, transpose,
                                       value);
}

void TraceGLApi::glProgramUniformMatrix2x3fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix2x3fv")
  gl_api_->glProgramUniformMatrix2x3fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glProgramUniformMatrix2x4fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix2x4fv")
  gl_api_->glProgramUniformMatrix2x4fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glProgramUniformMatrix3fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniformMatrix3fv")
  gl_api_->glProgramUniformMatrix3fvFn(program, location, count, transpose,
                                       value);
}

void TraceGLApi::glProgramUniformMatrix3x2fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix3x2fv")
  gl_api_->glProgramUniformMatrix3x2fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glProgramUniformMatrix3x4fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix3x4fv")
  gl_api_->glProgramUniformMatrix3x4fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glProgramUniformMatrix4fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramUniformMatrix4fv")
  gl_api_->glProgramUniformMatrix4fvFn(program, location, count, transpose,
                                       value);
}

void TraceGLApi::glProgramUniformMatrix4x2fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix4x2fv")
  gl_api_->glProgramUniformMatrix4x2fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glProgramUniformMatrix4x3fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramUniformMatrix4x3fv")
  gl_api_->glProgramUniformMatrix4x3fvFn(program, location, count, transpose,
                                         value);
}

void TraceGLApi::glPushDebugGroupFn(GLenum source,
                                    GLuint id,
                                    GLsizei length,
                                    const char* message) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushDebugGroup")
  gl_api_->glPushDebugGroupFn(source, id, length, message);
}

void TraceGLApi::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushGroupMarkerEXT")
  gl_api_->glPushGroupMarkerEXTFn(length, marker);
}

void TraceGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glQueryCounter")
  gl_api_->glQueryCounterFn(id, target);
}

void TraceGLApi::glReadBufferFn(GLenum src) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadBuffer")
  gl_api_->glReadBufferFn(src);
}

void TraceGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLsizei* columns,
                                            GLsizei* rows,
                                            void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadnPixelsRobustANGLE")
  gl_api_->glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                      bufSize, length, columns, rows, data);
}

void TraceGLApi::glReadPixelsFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixels")
  gl_api_->glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void TraceGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixelsRobustANGLE")
  gl_api_->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufSize,
                                     length, columns, rows, pixels);
}

void TraceGLApi::glReleaseShaderCompilerFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReleaseShaderCompiler")
  gl_api_->glReleaseShaderCompilerFn();
}

void TraceGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glRenderbufferStorageEXT")
  gl_api_->glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glRenderbufferStorageMultisample")
  gl_api_->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                              width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glRenderbufferStorageMultisampleEXT")
  gl_api_->glRenderbufferStorageMultisampleEXTFn(target, samples,
                                                 internalformat, width, height);
}

void TraceGLApi::glRequestExtensionANGLEFn(const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glRequestExtensionANGLE")
  gl_api_->glRequestExtensionANGLEFn(name);
}

void TraceGLApi::glResumeTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glResumeTransformFeedback")
  gl_api_->glResumeTransformFeedbackFn();
}

void TraceGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSampleCoverage")
  gl_api_->glSampleCoverageFn(value, invert);
}

void TraceGLApi::glSampleMaskiFn(GLuint maskNumber, GLbitfield mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSampleMaski")
  gl_api_->glSampleMaskiFn(maskNumber, mask);
}

void TraceGLApi::glSamplerParameterfFn(GLuint sampler,
                                       GLenum pname,
                                       GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterf")
  gl_api_->glSamplerParameterfFn(sampler, pname, param);
}

void TraceGLApi::glSamplerParameterfvFn(GLuint sampler,
                                        GLenum pname,
                                        const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterfv")
  gl_api_->glSamplerParameterfvFn(sampler, pname, params);
}

void TraceGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterfvRobustANGLE")
  gl_api_->glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameteriFn(GLuint sampler,
                                       GLenum pname,
                                       GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteri")
  gl_api_->glSamplerParameteriFn(sampler, pname, param);
}

void TraceGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIivRobustANGLE")
  gl_api_->glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIuivRobustANGLE")
  gl_api_->glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameterivFn(GLuint sampler,
                                        GLenum pname,
                                        const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteriv")
  gl_api_->glSamplerParameterivFn(sampler, pname, params);
}

void TraceGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterivRobustANGLE")
  gl_api_->glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glScissor")
  gl_api_->glScissorFn(x, y, width, height);
}

void TraceGLApi::glSetFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceAPPLE")
  gl_api_->glSetFenceAPPLEFn(fence);
}

void TraceGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceNV")
  gl_api_->glSetFenceNVFn(fence, condition);
}

void TraceGLApi::glShaderBinaryFn(GLsizei n,
                                  const GLuint* shaders,
                                  GLenum binaryformat,
                                  const void* binary,
                                  GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderBinary")
  gl_api_->glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void TraceGLApi::glShaderSourceFn(GLuint shader,
                                  GLsizei count,
                                  const char* const* str,
                                  const GLint* length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderSource")
  gl_api_->glShaderSourceFn(shader, count, str, length);
}

void TraceGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilFillPathInstancedNV")
  gl_api_->glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, fillMode, mask,
                                          transformType, transformValues);
}

void TraceGLApi::glStencilFillPathNVFn(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFillPathNV")
  gl_api_->glStencilFillPathNVFn(path, fillMode, mask);
}

void TraceGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFunc")
  gl_api_->glStencilFuncFn(func, ref, mask);
}

void TraceGLApi::glStencilFuncSeparateFn(GLenum face,
                                         GLenum func,
                                         GLint ref,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFuncSeparate")
  gl_api_->glStencilFuncSeparateFn(face, func, ref, mask);
}

void TraceGLApi::glStencilMaskFn(GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMask")
  gl_api_->glStencilMaskFn(mask);
}

void TraceGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMaskSeparate")
  gl_api_->glStencilMaskSeparateFn(face, mask);
}

void TraceGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOp")
  gl_api_->glStencilOpFn(fail, zfail, zpass);
}

void TraceGLApi::glStencilOpSeparateFn(GLenum face,
                                       GLenum fail,
                                       GLenum zfail,
                                       GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOpSeparate")
  gl_api_->glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void TraceGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilStrokePathInstancedNV")
  gl_api_->glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                            pathBase, ref, mask, transformType,
                                            transformValues);
}

void TraceGLApi::glStencilStrokePathNVFn(GLuint path,
                                         GLint reference,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilStrokePathNV")
  gl_api_->glStencilStrokePathNVFn(path, reference, mask);
}

void TraceGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverFillPathInstancedNV")
  gl_api_->glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

void TraceGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverFillPathNV")
  gl_api_->glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

void TraceGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverStrokePathInstancedNV")
  gl_api_->glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

void TraceGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverStrokePathNV")
  gl_api_->glStencilThenCoverStrokePathNVFn(path, reference, mask, coverMode);
}

GLboolean TraceGLApi::glTestFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceAPPLE")
  return gl_api_->glTestFenceAPPLEFn(fence);
}

GLboolean TraceGLApi::glTestFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceNV")
  return gl_api_->glTestFenceNVFn(fence);
}

void TraceGLApi::glTexBufferFn(GLenum target,
                               GLenum internalformat,
                               GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexBuffer")
  gl_api_->glTexBufferFn(target, internalformat, buffer);
}

void TraceGLApi::glTexBufferRangeFn(GLenum target,
                                    GLenum internalformat,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexBufferRange")
  gl_api_->glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

void TraceGLApi::glTexImage2DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2D")
  gl_api_->glTexImage2DFn(target, level, internalformat, width, height, border,
                          format, type, pixels);
}

void TraceGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2DRobustANGLE")
  gl_api_->glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                     height, border, format, type, bufSize,
                                     pixels);
}

void TraceGLApi::glTexImage3DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3D")
  gl_api_->glTexImage3DFn(target, level, internalformat, width, height, depth,
                          border, format, type, pixels);
}

void TraceGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3DRobustANGLE")
  gl_api_->glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                     height, depth, border, format, type,
                                     bufSize, pixels);
}

void TraceGLApi::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterf")
  gl_api_->glTexParameterfFn(target, pname, param);
}

void TraceGLApi::glTexParameterfvFn(GLenum target,
                                    GLenum pname,
                                    const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterfv")
  gl_api_->glTexParameterfvFn(target, pname, params);
}

void TraceGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterfvRobustANGLE")
  gl_api_->glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteri")
  gl_api_->glTexParameteriFn(target, pname, param);
}

void TraceGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIivRobustANGLE")
  gl_api_->glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIuivRobustANGLE")
  gl_api_->glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameterivFn(GLenum target,
                                    GLenum pname,
                                    const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteriv")
  gl_api_->glTexParameterivFn(target, pname, params);
}

void TraceGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterivRobustANGLE")
  gl_api_->glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexStorage2DEXTFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage2DEXT")
  gl_api_->glTexStorage2DEXTFn(target, levels, internalformat, width, height);
}

void TraceGLApi::glTexStorage2DMultisampleFn(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLboolean fixedsamplelocations) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage2DMultisample")
  gl_api_->glTexStorage2DMultisampleFn(target, samples, internalformat, width,
                                       height, fixedsamplelocations);
}

void TraceGLApi::glTexStorage3DFn(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage3D")
  gl_api_->glTexStorage3DFn(target, levels, internalformat, width, height,
                            depth);
}

void TraceGLApi::glTexSubImage2DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2D")
  gl_api_->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                             format, type, pixels);
}

void TraceGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2DRobustANGLE")
  gl_api_->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                        height, format, type, bufSize, pixels);
}

void TraceGLApi::glTexSubImage3DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3D")
  gl_api_->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, pixels);
}

void TraceGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLint zoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3DRobustANGLE")
  gl_api_->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                        zoffset, width, height, depth, format,
                                        type, bufSize, pixels);
}

void TraceGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                               GLsizei count,
                                               const char* const* varyings,
                                               GLenum bufferMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTransformFeedbackVaryings")
  gl_api_->glTransformFeedbackVaryingsFn(program, count, varyings, bufferMode);
}

void TraceGLApi::glUniform1fFn(GLint location, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1f")
  gl_api_->glUniform1fFn(location, x);
}

void TraceGLApi::glUniform1fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1fv")
  gl_api_->glUniform1fvFn(location, count, v);
}

void TraceGLApi::glUniform1iFn(GLint location, GLint x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1i")
  gl_api_->glUniform1iFn(location, x);
}

void TraceGLApi::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1iv")
  gl_api_->glUniform1ivFn(location, count, v);
}

void TraceGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1ui")
  gl_api_->glUniform1uiFn(location, v0);
}

void TraceGLApi::glUniform1uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1uiv")
  gl_api_->glUniform1uivFn(location, count, v);
}

void TraceGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2f")
  gl_api_->glUniform2fFn(location, x, y);
}

void TraceGLApi::glUniform2fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2fv")
  gl_api_->glUniform2fvFn(location, count, v);
}

void TraceGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2i")
  gl_api_->glUniform2iFn(location, x, y);
}

void TraceGLApi::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2iv")
  gl_api_->glUniform2ivFn(location, count, v);
}

void TraceGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2ui")
  gl_api_->glUniform2uiFn(location, v0, v1);
}

void TraceGLApi::glUniform2uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2uiv")
  gl_api_->glUniform2uivFn(location, count, v);
}

void TraceGLApi::glUniform3fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3f")
  gl_api_->glUniform3fFn(location, x, y, z);
}

void TraceGLApi::glUniform3fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3fv")
  gl_api_->glUniform3fvFn(location, count, v);
}

void TraceGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3i")
  gl_api_->glUniform3iFn(location, x, y, z);
}

void TraceGLApi::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3iv")
  gl_api_->glUniform3ivFn(location, count, v);
}

void TraceGLApi::glUniform3uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3ui")
  gl_api_->glUniform3uiFn(location, v0, v1, v2);
}

void TraceGLApi::glUniform3uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3uiv")
  gl_api_->glUniform3uivFn(location, count, v);
}

void TraceGLApi::glUniform4fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z,
                               GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4f")
  gl_api_->glUniform4fFn(location, x, y, z, w);
}

void TraceGLApi::glUniform4fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4fv")
  gl_api_->glUniform4fvFn(location, count, v);
}

void TraceGLApi::glUniform4iFn(GLint location,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4i")
  gl_api_->glUniform4iFn(location, x, y, z, w);
}

void TraceGLApi::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4iv")
  gl_api_->glUniform4ivFn(location, count, v);
}

void TraceGLApi::glUniform4uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4ui")
  gl_api_->glUniform4uiFn(location, v0, v1, v2, v3);
}

void TraceGLApi::glUniform4uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4uiv")
  gl_api_->glUniform4uivFn(location, count, v);
}

void TraceGLApi::glUniformBlockBindingFn(GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLuint uniformBlockBinding) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformBlockBinding")
  gl_api_->glUniformBlockBindingFn(program, uniformBlockIndex,
                                   uniformBlockBinding);
}

void TraceGLApi::glUniformMatrix2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2fv")
  gl_api_->glUniformMatrix2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix2x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x3fv")
  gl_api_->glUniformMatrix2x3fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix2x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x4fv")
  gl_api_->glUniformMatrix2x4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3fv")
  gl_api_->glUniformMatrix3fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x2fv")
  gl_api_->glUniformMatrix3x2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x4fv")
  gl_api_->glUniformMatrix3x4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4fv")
  gl_api_->glUniformMatrix4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x2fv")
  gl_api_->glUniformMatrix4x2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x3fv")
  gl_api_->glUniformMatrix4x3fvFn(location, count, transpose, value);
}

GLboolean TraceGLApi::glUnmapBufferFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUnmapBuffer")
  return gl_api_->glUnmapBufferFn(target);
}

void TraceGLApi::glUseProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUseProgram")
  gl_api_->glUseProgramFn(program);
}

void TraceGLApi::glUseProgramStagesFn(GLuint pipeline,
                                      GLbitfield stages,
                                      GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUseProgramStages")
  gl_api_->glUseProgramStagesFn(pipeline, stages, program);
}

void TraceGLApi::glValidateProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glValidateProgram")
  gl_api_->glValidateProgramFn(program);
}

void TraceGLApi::glValidateProgramPipelineFn(GLuint pipeline) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glValidateProgramPipeline")
  gl_api_->glValidateProgramPipelineFn(pipeline);
}

void TraceGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1f")
  gl_api_->glVertexAttrib1fFn(indx, x);
}

void TraceGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1fv")
  gl_api_->glVertexAttrib1fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2f")
  gl_api_->glVertexAttrib2fFn(indx, x, y);
}

void TraceGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2fv")
  gl_api_->glVertexAttrib2fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib3fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3f")
  gl_api_->glVertexAttrib3fFn(indx, x, y, z);
}

void TraceGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3fv")
  gl_api_->glVertexAttrib3fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib4fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4f")
  gl_api_->glVertexAttrib4fFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4fv")
  gl_api_->glVertexAttrib4fvFn(indx, values);
}

void TraceGLApi::glVertexAttribBindingFn(GLuint attribindex,
                                         GLuint bindingindex) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribBinding")
  gl_api_->glVertexAttribBindingFn(attribindex, bindingindex);
}

void TraceGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribDivisorANGLE")
  gl_api_->glVertexAttribDivisorANGLEFn(index, divisor);
}

void TraceGLApi::glVertexAttribFormatFn(GLuint attribindex,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLuint relativeoffset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribFormat")
  gl_api_->glVertexAttribFormatFn(attribindex, size, type, normalized,
                                  relativeoffset);
}

void TraceGLApi::glVertexAttribI4iFn(GLuint indx,
                                     GLint x,
                                     GLint y,
                                     GLint z,
                                     GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4i")
  gl_api_->glVertexAttribI4iFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4iv")
  gl_api_->glVertexAttribI4ivFn(indx, values);
}

void TraceGLApi::glVertexAttribI4uiFn(GLuint indx,
                                      GLuint x,
                                      GLuint y,
                                      GLuint z,
                                      GLuint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4ui")
  gl_api_->glVertexAttribI4uiFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4uiv")
  gl_api_->glVertexAttribI4uivFn(indx, values);
}

void TraceGLApi::glVertexAttribIFormatFn(GLuint attribindex,
                                         GLint size,
                                         GLenum type,
                                         GLuint relativeoffset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribIFormat")
  gl_api_->glVertexAttribIFormatFn(attribindex, size, type, relativeoffset);
}

void TraceGLApi::glVertexAttribIPointerFn(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribIPointer")
  gl_api_->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

void TraceGLApi::glVertexAttribPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLboolean normalized,
                                         GLsizei stride,
                                         const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribPointer")
  gl_api_->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
}

void TraceGLApi::glVertexBindingDivisorFn(GLuint bindingindex, GLuint divisor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexBindingDivisor")
  gl_api_->glVertexBindingDivisorFn(bindingindex, divisor);
}

void TraceGLApi::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glViewport")
  gl_api_->glViewportFn(x, y, width, height);
}

void TraceGLApi::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glWaitSync")
  gl_api_->glWaitSyncFn(sync, flags, timeout);
}

void TraceGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                         GLsizei n,
                                         const GLint* box) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glWindowRectanglesEXT")
  gl_api_->glWindowRectanglesEXTFn(mode, n, box);
}

void DebugGLApi::glActiveShaderProgramFn(GLuint pipeline, GLuint program) {
  GL_SERVICE_LOG("glActiveShaderProgram"
                 << "(" << pipeline << ", " << program << ")");
  gl_api_->glActiveShaderProgramFn(pipeline, program);
}

void DebugGLApi::glActiveTextureFn(GLenum texture) {
  GL_SERVICE_LOG("glActiveTexture"
                 << "(" << GLEnums::GetStringEnum(texture) << ")");
  gl_api_->glActiveTextureFn(texture);
}

void DebugGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  GL_SERVICE_LOG("glApplyFramebufferAttachmentCMAAINTEL"
                 << "("
                 << ")");
  gl_api_->glApplyFramebufferAttachmentCMAAINTELFn();
}

void DebugGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  GL_SERVICE_LOG("glAttachShader"
                 << "(" << program << ", " << shader << ")");
  gl_api_->glAttachShaderFn(program, shader);
}

void DebugGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  GL_SERVICE_LOG("glBeginQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  gl_api_->glBeginQueryFn(target, id);
}

void DebugGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  GL_SERVICE_LOG("glBeginTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(primitiveMode) << ")");
  gl_api_->glBeginTransformFeedbackFn(primitiveMode);
}

void DebugGLApi::glBindAttribLocationFn(GLuint program,
                                        GLuint index,
                                        const char* name) {
  GL_SERVICE_LOG("glBindAttribLocation"
                 << "(" << program << ", " << index << ", " << name << ")");
  gl_api_->glBindAttribLocationFn(program, index, name);
}

void DebugGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  GL_SERVICE_LOG("glBindBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << buffer
                 << ")");
  gl_api_->glBindBufferFn(target, buffer);
}

void DebugGLApi::glBindBufferBaseFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer) {
  GL_SERVICE_LOG("glBindBufferBase"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ")");
  gl_api_->glBindBufferBaseFn(target, index, buffer);
}

void DebugGLApi::glBindBufferRangeFn(GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) {
  GL_SERVICE_LOG("glBindBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ", " << offset << ", " << size << ")");
  gl_api_->glBindBufferRangeFn(target, index, buffer, offset, size);
}

void DebugGLApi::glBindFragDataLocationFn(GLuint program,
                                          GLuint colorNumber,
                                          const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocation"
                 << "(" << program << ", " << colorNumber << ", " << name
                 << ")");
  gl_api_->glBindFragDataLocationFn(program, colorNumber, name);
}

void DebugGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                 GLuint colorNumber,
                                                 GLuint index,
                                                 const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocationIndexed"
                 << "(" << program << ", " << colorNumber << ", " << index
                 << ", " << name << ")");
  gl_api_->glBindFragDataLocationIndexedFn(program, colorNumber, index, name);
}

void DebugGLApi::glBindFragmentInputLocationCHROMIUMFn(GLuint program,
                                                       GLint location,
                                                       const char* name) {
  GL_SERVICE_LOG("glBindFragmentInputLocationCHROMIUM"
                 << "(" << program << ", " << location << ", " << name << ")");
  gl_api_->glBindFragmentInputLocationCHROMIUMFn(program, location, name);
}

void DebugGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  GL_SERVICE_LOG("glBindFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << framebuffer
                 << ")");
  gl_api_->glBindFramebufferEXTFn(target, framebuffer);
}

void DebugGLApi::glBindImageTextureEXTFn(GLuint index,
                                         GLuint texture,
                                         GLint level,
                                         GLboolean layered,
                                         GLint layer,
                                         GLenum access,
                                         GLint format) {
  GL_SERVICE_LOG("glBindImageTextureEXT"
                 << "(" << index << ", " << texture << ", " << level << ", "
                 << GLEnums::GetStringBool(layered) << ", " << layer << ", "
                 << GLEnums::GetStringEnum(access) << ", " << format << ")");
  gl_api_->glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                   access, format);
}

void DebugGLApi::glBindProgramPipelineFn(GLuint pipeline) {
  GL_SERVICE_LOG("glBindProgramPipeline"
                 << "(" << pipeline << ")");
  gl_api_->glBindProgramPipelineFn(pipeline);
}

void DebugGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  GL_SERVICE_LOG("glBindRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << renderbuffer << ")");
  gl_api_->glBindRenderbufferEXTFn(target, renderbuffer);
}

void DebugGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  GL_SERVICE_LOG("glBindSampler"
                 << "(" << unit << ", " << sampler << ")");
  gl_api_->glBindSamplerFn(unit, sampler);
}

void DebugGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  GL_SERVICE_LOG("glBindTexture"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << texture
                 << ")");
  gl_api_->glBindTextureFn(target, texture);
}

void DebugGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  GL_SERVICE_LOG("glBindTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  gl_api_->glBindTransformFeedbackFn(target, id);
}

void DebugGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                 GLint location,
                                                 const char* name) {
  GL_SERVICE_LOG("glBindUniformLocationCHROMIUM"
                 << "(" << program << ", " << location << ", " << name << ")");
  gl_api_->glBindUniformLocationCHROMIUMFn(program, location, name);
}

void DebugGLApi::glBindVertexArrayOESFn(GLuint array) {
  GL_SERVICE_LOG("glBindVertexArrayOES"
                 << "(" << array << ")");
  gl_api_->glBindVertexArrayOESFn(array);
}

void DebugGLApi::glBindVertexBufferFn(GLuint bindingindex,
                                      GLuint buffer,
                                      GLintptr offset,
                                      GLsizei stride) {
  GL_SERVICE_LOG("glBindVertexBuffer"
                 << "(" << bindingindex << ", " << buffer << ", " << offset
                 << ", " << stride << ")");
  gl_api_->glBindVertexBufferFn(bindingindex, buffer, offset, stride);
}

void DebugGLApi::glBlendBarrierKHRFn(void) {
  GL_SERVICE_LOG("glBlendBarrierKHR"
                 << "("
                 << ")");
  gl_api_->glBlendBarrierKHRFn();
}

void DebugGLApi::glBlendColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  GL_SERVICE_LOG("glBlendColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  gl_api_->glBlendColorFn(red, green, blue, alpha);
}

void DebugGLApi::glBlendEquationFn(GLenum mode) {
  GL_SERVICE_LOG("glBlendEquation"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glBlendEquationFn(mode);
}

void DebugGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  GL_SERVICE_LOG("glBlendEquationSeparate"
                 << "(" << GLEnums::GetStringEnum(modeRGB) << ", "
                 << GLEnums::GetStringEnum(modeAlpha) << ")");
  gl_api_->glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void DebugGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  GL_SERVICE_LOG("glBlendFunc"
                 << "(" << GLEnums::GetStringEnum(sfactor) << ", "
                 << GLEnums::GetStringEnum(dfactor) << ")");
  gl_api_->glBlendFuncFn(sfactor, dfactor);
}

void DebugGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                       GLenum dstRGB,
                                       GLenum srcAlpha,
                                       GLenum dstAlpha) {
  GL_SERVICE_LOG("glBlendFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(srcRGB) << ", "
                 << GLEnums::GetStringEnum(dstRGB) << ", "
                 << GLEnums::GetStringEnum(srcAlpha) << ", "
                 << GLEnums::GetStringEnum(dstAlpha) << ")");
  gl_api_->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void DebugGLApi::glBlitFramebufferFn(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) {
  GL_SERVICE_LOG("glBlitFramebuffer"
                 << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", "
                 << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1
                 << ", " << dstY1 << ", " << mask << ", "
                 << GLEnums::GetStringEnum(filter) << ")");
  gl_api_->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

void DebugGLApi::glBufferDataFn(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  GL_SERVICE_LOG("glBufferData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << size
                 << ", " << static_cast<const void*>(data) << ", "
                 << GLEnums::GetStringEnum(usage) << ")");
  gl_api_->glBufferDataFn(target, size, data, usage);
}

void DebugGLApi::glBufferSubDataFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void* data) {
  GL_SERVICE_LOG("glBufferSubData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << size << ", " << static_cast<const void*>(data)
                 << ")");
  gl_api_->glBufferSubDataFn(target, offset, size, data);
}

GLenum DebugGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  GL_SERVICE_LOG("glCheckFramebufferStatusEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  GLenum result = gl_api_->glCheckFramebufferStatusEXTFn(target);

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringEnum(result));

  return result;
}

void DebugGLApi::glClearFn(GLbitfield mask) {
  GL_SERVICE_LOG("glClear"
                 << "(" << mask << ")");
  gl_api_->glClearFn(mask);
}

void DebugGLApi::glClearBufferfiFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat depth,
                                   GLint stencil) {
  GL_SERVICE_LOG("glClearBufferfi"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << depth << ", " << stencil << ")");
  gl_api_->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void DebugGLApi::glClearBufferfvFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat* value) {
  GL_SERVICE_LOG("glClearBufferfv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferfvFn(buffer, drawbuffer, value);
}

void DebugGLApi::glClearBufferivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint* value) {
  GL_SERVICE_LOG("glClearBufferiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferivFn(buffer, drawbuffer, value);
}

void DebugGLApi::glClearBufferuivFn(GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint* value) {
  GL_SERVICE_LOG("glClearBufferuiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferuivFn(buffer, drawbuffer, value);
}

void DebugGLApi::glClearColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  GL_SERVICE_LOG("glClearColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  gl_api_->glClearColorFn(red, green, blue, alpha);
}

void DebugGLApi::glClearDepthFn(GLclampd depth) {
  GL_SERVICE_LOG("glClearDepth"
                 << "(" << depth << ")");
  gl_api_->glClearDepthFn(depth);
}

void DebugGLApi::glClearDepthfFn(GLclampf depth) {
  GL_SERVICE_LOG("glClearDepthf"
                 << "(" << depth << ")");
  gl_api_->glClearDepthfFn(depth);
}

void DebugGLApi::glClearStencilFn(GLint s) {
  GL_SERVICE_LOG("glClearStencil"
                 << "(" << s << ")");
  gl_api_->glClearStencilFn(s);
}

GLenum DebugGLApi::glClientWaitSyncFn(GLsync sync,
                                      GLbitfield flags,
                                      GLuint64 timeout) {
  GL_SERVICE_LOG("glClientWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  GLenum result = gl_api_->glClientWaitSyncFn(sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glColorMaskFn(GLboolean red,
                               GLboolean green,
                               GLboolean blue,
                               GLboolean alpha) {
  GL_SERVICE_LOG("glColorMask"
                 << "(" << GLEnums::GetStringBool(red) << ", "
                 << GLEnums::GetStringBool(green) << ", "
                 << GLEnums::GetStringBool(blue) << ", "
                 << GLEnums::GetStringBool(alpha) << ")");
  gl_api_->glColorMaskFn(red, green, blue, alpha);
}

void DebugGLApi::glCompileShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glCompileShader"
                 << "(" << shader << ")");
  gl_api_->glCompileShaderFn(shader);
}

void DebugGLApi::glCompressedTexImage2DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << border << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage2DFn(target, level, internalformat, width,
                                    height, border, imageSize, data);
}

void DebugGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << border << ", "
                 << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                               width, height, border, imageSize,
                                               dataSize, data);
}

void DebugGLApi::glCompressedTexImage3DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ", " << border
                 << ", " << imageSize << ", " << static_cast<const void*>(data)
                 << ")");
  gl_api_->glCompressedTexImage3DFn(target, level, internalformat, width,
                                    height, depth, border, imageSize, data);
}

void DebugGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ", " << border
                 << ", " << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                               width, height, depth, border,
                                               imageSize, dataSize, data);
}

void DebugGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

void DebugGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

void DebugGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", " << imageSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, imageSize,
                                       data);
}

void DebugGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLint zoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", " << imageSize << ", "
                 << dataSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

void DebugGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                       GLenum writeTarget,
                                       GLintptr readOffset,
                                       GLintptr writeOffset,
                                       GLsizeiptr size) {
  GL_SERVICE_LOG("glCopyBufferSubData"
                 << "(" << GLEnums::GetStringEnum(readTarget) << ", "
                 << GLEnums::GetStringEnum(writeTarget) << ", " << readOffset
                 << ", " << writeOffset << ", " << size << ")");
  gl_api_->glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                 writeOffset, size);
}

void DebugGLApi::glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                            GLint sourceLevel,
                                            GLenum destTarget,
                                            GLuint destId,
                                            GLint destLevel,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLboolean unpackFlipY,
                                            GLboolean unpackPremultiplyAlpha,
                                            GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopySubTextureCHROMIUM"
                 << "(" << sourceId << ", " << sourceLevel << ", "
                 << GLEnums::GetStringEnum(destTarget) << ", " << destId << ", "
                 << destLevel << ", " << xoffset << ", " << yoffset << ", " << x
                 << ", " << y << ", " << width << ", " << height << ", "
                 << GLEnums::GetStringBool(unpackFlipY) << ", "
                 << GLEnums::GetStringBool(unpackPremultiplyAlpha) << ", "
                 << GLEnums::GetStringBool(unpackUnmultiplyAlpha) << ")");
  gl_api_->glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

void DebugGLApi::glCopyTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border) {
  GL_SERVICE_LOG("glCopyTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", " << x
                 << ", " << y << ", " << width << ", " << height << ", "
                 << border << ")");
  gl_api_->glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                              height, border);
}

void DebugGLApi::glCopyTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y
                 << ", " << width << ", " << height << ")");
  gl_api_->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

void DebugGLApi::glCopyTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                                 width, height);
}

void DebugGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                         GLint sourceLevel,
                                         GLenum destTarget,
                                         GLuint destId,
                                         GLint destLevel,
                                         GLint internalFormat,
                                         GLenum destType,
                                         GLboolean unpackFlipY,
                                         GLboolean unpackPremultiplyAlpha,
                                         GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopyTextureCHROMIUM"
                 << "(" << sourceId << ", " << sourceLevel << ", "
                 << GLEnums::GetStringEnum(destTarget) << ", " << destId << ", "
                 << destLevel << ", " << internalFormat << ", "
                 << GLEnums::GetStringEnum(destType) << ", "
                 << GLEnums::GetStringBool(unpackFlipY) << ", "
                 << GLEnums::GetStringBool(unpackPremultiplyAlpha) << ", "
                 << GLEnums::GetStringBool(unpackUnmultiplyAlpha) << ")");
  gl_api_->glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void DebugGLApi::glCoverageModulationNVFn(GLenum components) {
  GL_SERVICE_LOG("glCoverageModulationNV"
                 << "(" << GLEnums::GetStringEnum(components) << ")");
  gl_api_->glCoverageModulationNVFn(components);
}

void DebugGLApi::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths, pathBase,
                                        coverMode, transformType,
                                        transformValues);
}

void DebugGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  GL_SERVICE_LOG("glCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glCoverFillPathNVFn(path, coverMode);
}

void DebugGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverStrokePathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, coverMode, transformType,
                                          transformValues);
}

void DebugGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  GL_SERVICE_LOG("glCoverStrokePathNV"
                 << "(" << name << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glCoverStrokePathNVFn(name, coverMode);
}

GLuint DebugGLApi::glCreateProgramFn(void) {
  GL_SERVICE_LOG("glCreateProgram"
                 << "("
                 << ")");
  GLuint result = gl_api_->glCreateProgramFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLuint DebugGLApi::glCreateShaderFn(GLenum type) {
  GL_SERVICE_LOG("glCreateShader"
                 << "(" << GLEnums::GetStringEnum(type) << ")");
  GLuint result = gl_api_->glCreateShaderFn(type);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLuint DebugGLApi::glCreateShaderProgramvFn(GLenum type,
                                            GLsizei count,
                                            const char* const* strings) {
  GL_SERVICE_LOG("glCreateShaderProgramv"
                 << "(" << GLEnums::GetStringEnum(type) << ", " << count << ", "
                 << static_cast<const void*>(strings) << ")");
  GLuint result = gl_api_->glCreateShaderProgramvFn(type, count, strings);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glCullFaceFn(GLenum mode) {
  GL_SERVICE_LOG("glCullFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glCullFaceFn(mode);
}

void DebugGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                          const void* userParam) {
  GL_SERVICE_LOG("glDebugMessageCallback"
                 << "(" << reinterpret_cast<void*>(callback) << ", "
                 << static_cast<const void*>(userParam) << ")");
  gl_api_->glDebugMessageCallbackFn(callback, userParam);
}

void DebugGLApi::glDebugMessageControlFn(GLenum source,
                                         GLenum type,
                                         GLenum severity,
                                         GLsizei count,
                                         const GLuint* ids,
                                         GLboolean enabled) {
  GL_SERVICE_LOG("glDebugMessageControl"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << count << ", "
                 << static_cast<const void*>(ids) << ", "
                 << GLEnums::GetStringBool(enabled) << ")");
  gl_api_->glDebugMessageControlFn(source, type, severity, count, ids, enabled);
}

void DebugGLApi::glDebugMessageInsertFn(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const char* buf) {
  GL_SERVICE_LOG("glDebugMessageInsert"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << id << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << length << ", "
                 << buf << ")");
  gl_api_->glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

void DebugGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  GL_SERVICE_LOG("glDeleteBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  gl_api_->glDeleteBuffersARBFn(n, buffers);
}

void DebugGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glDeleteFencesAPPLEFn(n, fences);
}

void DebugGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glDeleteFencesNVFn(n, fences);
}

void DebugGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                           const GLuint* framebuffers) {
  GL_SERVICE_LOG("glDeleteFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  gl_api_->glDeleteFramebuffersEXTFn(n, framebuffers);
}

void DebugGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  GL_SERVICE_LOG("glDeletePathsNV"
                 << "(" << path << ", " << range << ")");
  gl_api_->glDeletePathsNVFn(path, range);
}

void DebugGLApi::glDeleteProgramFn(GLuint program) {
  GL_SERVICE_LOG("glDeleteProgram"
                 << "(" << program << ")");
  gl_api_->glDeleteProgramFn(program);
}

void DebugGLApi::glDeleteProgramPipelinesFn(GLsizei n,
                                            const GLuint* pipelines) {
  GL_SERVICE_LOG("glDeleteProgramPipelines"
                 << "(" << n << ", " << static_cast<const void*>(pipelines)
                 << ")");
  gl_api_->glDeleteProgramPipelinesFn(n, pipelines);
}

void DebugGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glDeleteQueriesFn(n, ids);
}

void DebugGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                            const GLuint* renderbuffers) {
  GL_SERVICE_LOG("glDeleteRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  gl_api_->glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void DebugGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  GL_SERVICE_LOG("glDeleteSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  gl_api_->glDeleteSamplersFn(n, samplers);
}

void DebugGLApi::glDeleteShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glDeleteShader"
                 << "(" << shader << ")");
  gl_api_->glDeleteShaderFn(shader);
}

void DebugGLApi::glDeleteSyncFn(GLsync sync) {
  GL_SERVICE_LOG("glDeleteSync"
                 << "(" << sync << ")");
  gl_api_->glDeleteSyncFn(sync);
}

void DebugGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  GL_SERVICE_LOG("glDeleteTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  gl_api_->glDeleteTexturesFn(n, textures);
}

void DebugGLApi::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glDeleteTransformFeedbacksFn(n, ids);
}

void DebugGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  GL_SERVICE_LOG("glDeleteVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  gl_api_->glDeleteVertexArraysOESFn(n, arrays);
}

void DebugGLApi::glDepthFuncFn(GLenum func) {
  GL_SERVICE_LOG("glDepthFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ")");
  gl_api_->glDepthFuncFn(func);
}

void DebugGLApi::glDepthMaskFn(GLboolean flag) {
  GL_SERVICE_LOG("glDepthMask"
                 << "(" << GLEnums::GetStringBool(flag) << ")");
  gl_api_->glDepthMaskFn(flag);
}

void DebugGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  GL_SERVICE_LOG("glDepthRange"
                 << "(" << zNear << ", " << zFar << ")");
  gl_api_->glDepthRangeFn(zNear, zFar);
}

void DebugGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  GL_SERVICE_LOG("glDepthRangef"
                 << "(" << zNear << ", " << zFar << ")");
  gl_api_->glDepthRangefFn(zNear, zFar);
}

void DebugGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  GL_SERVICE_LOG("glDetachShader"
                 << "(" << program << ", " << shader << ")");
  gl_api_->glDetachShaderFn(program, shader);
}

void DebugGLApi::glDisableFn(GLenum cap) {
  GL_SERVICE_LOG("glDisable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  gl_api_->glDisableFn(cap);
}

void DebugGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  GL_SERVICE_LOG("glDisableVertexAttribArray"
                 << "(" << index << ")");
  gl_api_->glDisableVertexAttribArrayFn(index);
}

void DebugGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  GL_SERVICE_LOG("glDiscardFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  gl_api_->glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void DebugGLApi::glDispatchComputeFn(GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ) {
  GL_SERVICE_LOG("glDispatchCompute"
                 << "(" << numGroupsX << ", " << numGroupsY << ", "
                 << numGroupsZ << ")");
  gl_api_->glDispatchComputeFn(numGroupsX, numGroupsY, numGroupsZ);
}

void DebugGLApi::glDispatchComputeIndirectFn(GLintptr indirect) {
  GL_SERVICE_LOG("glDispatchComputeIndirect"
                 << "(" << indirect << ")");
  gl_api_->glDispatchComputeIndirectFn(indirect);
}

void DebugGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  GL_SERVICE_LOG("glDrawArrays"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ")");
  gl_api_->glDrawArraysFn(mode, first, count);
}

void DebugGLApi::glDrawArraysIndirectFn(GLenum mode, const void* indirect) {
  GL_SERVICE_LOG("glDrawArraysIndirect"
                 << "(" << GLEnums::GetStringEnum(mode) << ", "
                 << static_cast<const void*>(indirect) << ")");
  gl_api_->glDrawArraysIndirectFn(mode, indirect);
}

void DebugGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount) {
  GL_SERVICE_LOG("glDrawArraysInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ", " << primcount << ")");
  gl_api_->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void DebugGLApi::glDrawBufferFn(GLenum mode) {
  GL_SERVICE_LOG("glDrawBuffer"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glDrawBufferFn(mode);
}

void DebugGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  GL_SERVICE_LOG("glDrawBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(bufs) << ")");
  gl_api_->glDrawBuffersARBFn(n, bufs);
}

void DebugGLApi::glDrawElementsFn(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices) {
  GL_SERVICE_LOG("glDrawElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ")");
  gl_api_->glDrawElementsFn(mode, count, type, indices);
}

void DebugGLApi::glDrawElementsIndirectFn(GLenum mode,
                                          GLenum type,
                                          const void* indirect) {
  GL_SERVICE_LOG("glDrawElementsIndirect"
                 << "(" << GLEnums::GetStringEnum(mode) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indirect) << ")");
  gl_api_->glDrawElementsIndirectFn(mode, type, indirect);
}

void DebugGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices,
                                                GLsizei primcount) {
  GL_SERVICE_LOG("glDrawElementsInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ", " << primcount
                 << ")");
  gl_api_->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                          primcount);
}

void DebugGLApi::glDrawRangeElementsFn(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  GL_SERVICE_LOG("glDrawRangeElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << start << ", "
                 << end << ", " << count << ", " << GLEnums::GetStringEnum(type)
                 << ", " << static_cast<const void*>(indices) << ")");
  gl_api_->glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

void DebugGLApi::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                          GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetRenderbufferStorageOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  gl_api_->glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void DebugGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetTexture2DOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  gl_api_->glEGLImageTargetTexture2DOESFn(target, image);
}

void DebugGLApi::glEnableFn(GLenum cap) {
  GL_SERVICE_LOG("glEnable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  gl_api_->glEnableFn(cap);
}

void DebugGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  GL_SERVICE_LOG("glEnableVertexAttribArray"
                 << "(" << index << ")");
  gl_api_->glEnableVertexAttribArrayFn(index);
}

void DebugGLApi::glEndQueryFn(GLenum target) {
  GL_SERVICE_LOG("glEndQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glEndQueryFn(target);
}

void DebugGLApi::glEndTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glEndTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glEndTransformFeedbackFn();
}

GLsync DebugGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  GL_SERVICE_LOG("glFenceSync"
                 << "(" << GLEnums::GetStringEnum(condition) << ", " << flags
                 << ")");
  GLsync result = gl_api_->glFenceSyncFn(condition, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glFinishFn(void) {
  GL_SERVICE_LOG("glFinish"
                 << "("
                 << ")");
  gl_api_->glFinishFn();
}

void DebugGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceAPPLE"
                 << "(" << fence << ")");
  gl_api_->glFinishFenceAPPLEFn(fence);
}

void DebugGLApi::glFinishFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceNV"
                 << "(" << fence << ")");
  gl_api_->glFinishFenceNVFn(fence);
}

void DebugGLApi::glFlushFn(void) {
  GL_SERVICE_LOG("glFlush"
                 << "("
                 << ")");
  gl_api_->glFlushFn();
}

void DebugGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                            GLintptr offset,
                                            GLsizeiptr length) {
  GL_SERVICE_LOG("glFlushMappedBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ")");
  gl_api_->glFlushMappedBufferRangeFn(target, offset, length);
}

void DebugGLApi::glFramebufferParameteriFn(GLenum target,
                                           GLenum pname,
                                           GLint param) {
  GL_SERVICE_LOG("glFramebufferParameteri"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  gl_api_->glFramebufferParameteriFn(target, pname, param);
}

void DebugGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  GL_SERVICE_LOG("glFramebufferRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(renderbuffertarget) << ", "
                 << renderbuffer << ")");
  gl_api_->glFramebufferRenderbufferEXTFn(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

void DebugGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  GL_SERVICE_LOG("glFramebufferTexture2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ")");
  gl_api_->glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                       level);
}

void DebugGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  GL_SERVICE_LOG("glFramebufferTexture2DMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ", " << samples << ")");
  gl_api_->glFramebufferTexture2DMultisampleEXTFn(target, attachment, textarget,
                                                  texture, level, samples);
}

void DebugGLApi::glFramebufferTextureLayerFn(GLenum target,
                                             GLenum attachment,
                                             GLuint texture,
                                             GLint level,
                                             GLint layer) {
  GL_SERVICE_LOG("glFramebufferTextureLayer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", " << texture
                 << ", " << level << ", " << layer << ")");
  gl_api_->glFramebufferTextureLayerFn(target, attachment, texture, level,
                                       layer);
}

void DebugGLApi::glFramebufferTextureMultiviewLayeredANGLEFn(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews) {
  GL_SERVICE_LOG("glFramebufferTextureMultiviewLayeredANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", " << texture
                 << ", " << level << ", " << baseViewIndex << ", " << numViews
                 << ")");
  gl_api_->glFramebufferTextureMultiviewLayeredANGLEFn(
      target, attachment, texture, level, baseViewIndex, numViews);
}

void DebugGLApi::glFrontFaceFn(GLenum mode) {
  GL_SERVICE_LOG("glFrontFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glFrontFaceFn(mode);
}

void DebugGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  GL_SERVICE_LOG("glGenBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  gl_api_->glGenBuffersARBFn(n, buffers);
}

void DebugGLApi::glGenerateMipmapEXTFn(GLenum target) {
  GL_SERVICE_LOG("glGenerateMipmapEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glGenerateMipmapEXTFn(target);
}

void DebugGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glGenFencesAPPLEFn(n, fences);
}

void DebugGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glGenFencesNVFn(n, fences);
}

void DebugGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  GL_SERVICE_LOG("glGenFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  gl_api_->glGenFramebuffersEXTFn(n, framebuffers);
}

GLuint DebugGLApi::glGenPathsNVFn(GLsizei range) {
  GL_SERVICE_LOG("glGenPathsNV"
                 << "(" << range << ")");
  GLuint result = gl_api_->glGenPathsNVFn(range);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLuint DebugGLApi::glGenProgramPipelinesFn(GLsizei n, GLuint* pipelines) {
  GL_SERVICE_LOG("glGenProgramPipelines"
                 << "(" << n << ", " << static_cast<const void*>(pipelines)
                 << ")");
  GLuint result = gl_api_->glGenProgramPipelinesFn(n, pipelines);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  GL_SERVICE_LOG("glGenQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glGenQueriesFn(n, ids);
}

void DebugGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  GL_SERVICE_LOG("glGenRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  gl_api_->glGenRenderbuffersEXTFn(n, renderbuffers);
}

void DebugGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  GL_SERVICE_LOG("glGenSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  gl_api_->glGenSamplersFn(n, samplers);
}

void DebugGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  GL_SERVICE_LOG("glGenTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  gl_api_->glGenTexturesFn(n, textures);
}

void DebugGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  GL_SERVICE_LOG("glGenTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glGenTransformFeedbacksFn(n, ids);
}

void DebugGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  GL_SERVICE_LOG("glGenVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  gl_api_->glGenVertexArraysOESFn(n, arrays);
}

void DebugGLApi::glGetActiveAttribFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  GL_SERVICE_LOG("glGetActiveAttrib"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetActiveAttribFn(program, index, bufsize, length, size, type,
                               name);
}

void DebugGLApi::glGetActiveUniformFn(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  GL_SERVICE_LOG("glGetActiveUniform"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                name);
}

void DebugGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLenum pname,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockiv"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                       params);
}

void DebugGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockivRobustANGLE"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void DebugGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                               GLuint uniformBlockIndex,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               char* uniformBlockName) {
  GL_SERVICE_LOG("glGetActiveUniformBlockName"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(uniformBlockName) << ")");
  gl_api_->glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                         length, uniformBlockName);
}

void DebugGLApi::glGetActiveUniformsivFn(GLuint program,
                                         GLsizei uniformCount,
                                         const GLuint* uniformIndices,
                                         GLenum pname,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformsiv"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformIndices) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformsivFn(program, uniformCount, uniformIndices, pname,
                                   params);
}

void DebugGLApi::glGetAttachedShadersFn(GLuint program,
                                        GLsizei maxcount,
                                        GLsizei* count,
                                        GLuint* shaders) {
  GL_SERVICE_LOG("glGetAttachedShaders"
                 << "(" << program << ", " << maxcount << ", "
                 << static_cast<const void*>(count) << ", "
                 << static_cast<const void*>(shaders) << ")");
  gl_api_->glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint DebugGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetAttribLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetAttribLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetBooleani_vFn(GLenum target,
                                   GLuint index,
                                   GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleani_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetBooleani_vFn(target, index, data);
}

void DebugGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleani_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetBooleani_vRobustANGLEFn(target, index, bufSize, length, data);
}

void DebugGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  GL_SERVICE_LOG("glGetBooleanv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBooleanvFn(pname, params);
}

void DebugGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleanvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

void DebugGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  GL_SERVICE_LOG("glGetBufferParameteri64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

void DebugGLApi::glGetBufferParameterivFn(GLenum target,
                                          GLenum pname,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameterivFn(target, pname, params);
}

void DebugGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void DebugGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  void** params) {
  GL_SERVICE_LOG("glGetBufferPointervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", " << params << ")");
  gl_api_->glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

GLuint DebugGLApi::glGetDebugMessageLogFn(GLuint count,
                                          GLsizei bufSize,
                                          GLenum* sources,
                                          GLenum* types,
                                          GLuint* ids,
                                          GLenum* severities,
                                          GLsizei* lengths,
                                          char* messageLog) {
  GL_SERVICE_LOG("glGetDebugMessageLog"
                 << "(" << count << ", " << bufSize << ", "
                 << static_cast<const void*>(sources) << ", "
                 << static_cast<const void*>(types) << ", "
                 << static_cast<const void*>(ids) << ", "
                 << static_cast<const void*>(severities) << ", "
                 << static_cast<const void*>(lengths) << ", "
                 << static_cast<const void*>(messageLog) << ")");
  GLuint result = gl_api_->glGetDebugMessageLogFn(
      count, bufSize, sources, types, ids, severities, lengths, messageLog);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLenum DebugGLApi::glGetErrorFn(void) {
  GL_SERVICE_LOG("glGetError"
                 << "("
                 << ")");
  GLenum result = gl_api_->glGetErrorFn();

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringError(result));

  return result;
}

void DebugGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetFenceivNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetFenceivNVFn(fence, pname, params);
}

void DebugGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  GL_SERVICE_LOG("glGetFloatv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFloatvFn(pname, params);
}

void DebugGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* data) {
  GL_SERVICE_LOG("glGetFloatvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

GLint DebugGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetFragDataIndex"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetFragDataIndexFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLint DebugGLApi::glGetFragDataLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetFragDataLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetFragDataLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                      params);
}

void DebugGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

void DebugGLApi::glGetFramebufferParameterivFn(GLenum target,
                                               GLenum pname,
                                               GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferParameterivFn(target, pname, params);
}

void DebugGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

GLenum DebugGLApi::glGetGraphicsResetStatusARBFn(void) {
  GL_SERVICE_LOG("glGetGraphicsResetStatusARB"
                 << "("
                 << ")");
  GLenum result = gl_api_->glGetGraphicsResetStatusARBFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetInteger64i_vFn(GLenum target,
                                     GLuint index,
                                     GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64i_vFn(target, index, data);
}

void DebugGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length, data);
}

void DebugGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  GL_SERVICE_LOG("glGetInteger64v"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInteger64vFn(pname, params);
}

void DebugGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

void DebugGLApi::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegeri_vFn(target, index, data);
}

void DebugGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length, data);
}

void DebugGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetIntegerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetIntegervFn(pname, params);
}

void DebugGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) {
  GL_SERVICE_LOG("glGetIntegervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

void DebugGLApi::glGetInternalformativFn(GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativ"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInternalformativFn(target, internalformat, pname, bufSize,
                                   params);
}

void DebugGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                    GLenum internalformat,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                              bufSize, length, params);
}

void DebugGLApi::glGetMultisamplefvFn(GLenum pname,
                                      GLuint index,
                                      GLfloat* val) {
  GL_SERVICE_LOG("glGetMultisamplefv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << index
                 << ", " << static_cast<const void*>(val) << ")");
  gl_api_->glGetMultisamplefvFn(pname, index, val);
}

void DebugGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* val) {
  GL_SERVICE_LOG("glGetMultisamplefvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(val) << ")");
  gl_api_->glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length, val);
}

void DebugGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* params) {
  GL_SERVICE_LOG("glGetnUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void DebugGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  GL_SERVICE_LOG("glGetnUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void DebugGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLuint* params) {
  GL_SERVICE_LOG("glGetnUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                         params);
}

void DebugGLApi::glGetObjectLabelFn(GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    char* label) {
  GL_SERVICE_LOG("glGetObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(label) << ")");
  gl_api_->glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

void DebugGLApi::glGetObjectPtrLabelFn(void* ptr,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       char* label) {
  GL_SERVICE_LOG("glGetObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(label) << ")");
  gl_api_->glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

void DebugGLApi::glGetPointervFn(GLenum pname, void** params) {
  GL_SERVICE_LOG("glGetPointerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << params
                 << ")");
  gl_api_->glGetPointervFn(pname, params);
}

void DebugGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** params) {
  GL_SERVICE_LOG("glGetPointervRobustANGLERobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", " << params
                 << ")");
  gl_api_->glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                 params);
}

void DebugGLApi::glGetProgramBinaryFn(GLuint program,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLenum* binaryFormat,
                                      GLvoid* binary) {
  GL_SERVICE_LOG("glGetProgramBinary"
                 << "(" << program << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ")");
  gl_api_->glGetProgramBinaryFn(program, bufSize, length, binaryFormat, binary);
}

void DebugGLApi::glGetProgramInfoLogFn(GLuint program,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  GL_SERVICE_LOG("glGetProgramInfoLog"
                 << "(" << program << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  gl_api_->glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void DebugGLApi::glGetProgramInterfaceivFn(GLuint program,
                                           GLenum programInterface,
                                           GLenum pname,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramInterfaceivFn(program, programInterface, pname, params);
}

void DebugGLApi::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                      GLenum programInterface,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceivRobustANGLE"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramInterfaceivRobustANGLEFn(program, programInterface,
                                                pname, bufSize, length, params);
}

void DebugGLApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetProgramiv"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramivFn(program, pname, params);
}

void DebugGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetProgramivRobustANGLE"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramivRobustANGLEFn(program, pname, bufSize, length, params);
}

void DebugGLApi::glGetProgramPipelineInfoLogFn(GLuint pipeline,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLchar* infoLog) {
  GL_SERVICE_LOG("glGetProgramPipelineInfoLog"
                 << "(" << pipeline << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infoLog) << ")");
  gl_api_->glGetProgramPipelineInfoLogFn(pipeline, bufSize, length, infoLog);
}

void DebugGLApi::glGetProgramPipelineivFn(GLuint pipeline,
                                          GLenum pname,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetProgramPipelineiv"
                 << "(" << pipeline << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramPipelineivFn(pipeline, pname, params);
}

void DebugGLApi::glGetProgramResourceIndexFn(GLuint program,
                                             GLenum programInterface,
                                             const GLchar* name) {
  GL_SERVICE_LOG("glGetProgramResourceIndex"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetProgramResourceIndexFn(program, programInterface, name);
}

void DebugGLApi::glGetProgramResourceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei propCount,
                                          const GLenum* props,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetProgramResourceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << propCount << ", " << static_cast<const void*>(props)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramResourceivFn(program, programInterface, index, propCount,
                                    props, bufSize, length, params);
}

GLint DebugGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                 GLenum programInterface,
                                                 const char* name) {
  GL_SERVICE_LOG("glGetProgramResourceLocation"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << name
                 << ")");
  GLint result =
      gl_api_->glGetProgramResourceLocationFn(program, programInterface, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetProgramResourceNameFn(GLuint program,
                                            GLenum programInterface,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLchar* name) {
  GL_SERVICE_LOG("glGetProgramResourceName"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(name) << ")");
  gl_api_->glGetProgramResourceNameFn(program, programInterface, index, bufSize,
                                      length, name);
}

void DebugGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetQueryiv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryivFn(target, pname, params);
}

void DebugGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetQueryivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

void DebugGLApi::glGetQueryObjecti64vFn(GLuint id,
                                        GLenum pname,
                                        GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjecti64vFn(id, pname, params);
}

void DebugGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                             params);
}

void DebugGLApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectivFn(id, pname, params);
}

void DebugGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length, params);
}

void DebugGLApi::glGetQueryObjectui64vFn(GLuint id,
                                         GLenum pname,
                                         GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectui64vFn(id, pname, params);
}

void DebugGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

void DebugGLApi::glGetQueryObjectuivFn(GLuint id,
                                       GLenum pname,
                                       GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectuivFn(id, pname, params);
}

void DebugGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length, params);
}

void DebugGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void DebugGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                     length, params);
}

void DebugGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterfvFn(sampler, pname, params);
}

void DebugGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void DebugGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 length, params);
}

void DebugGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  length, params);
}

void DebugGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                           GLenum pname,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterivFn(sampler, pname, params);
}

void DebugGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void DebugGLApi::glGetShaderInfoLogFn(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  GL_SERVICE_LOG("glGetShaderInfoLog"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  gl_api_->glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void DebugGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetShaderiv"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetShaderivFn(shader, pname, params);
}

void DebugGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  GL_SERVICE_LOG("glGetShaderivRobustANGLE"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetShaderivRobustANGLEFn(shader, pname, bufSize, length, params);
}

void DebugGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                              GLenum precisiontype,
                                              GLint* range,
                                              GLint* precision) {
  GL_SERVICE_LOG("glGetShaderPrecisionFormat"
                 << "(" << GLEnums::GetStringEnum(shadertype) << ", "
                 << GLEnums::GetStringEnum(precisiontype) << ", "
                 << static_cast<const void*>(range) << ", "
                 << static_cast<const void*>(precision) << ")");
  gl_api_->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                        precision);
}

void DebugGLApi::glGetShaderSourceFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* source) {
  GL_SERVICE_LOG("glGetShaderSource"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  gl_api_->glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* DebugGLApi::glGetStringFn(GLenum name) {
  GL_SERVICE_LOG("glGetString"
                 << "(" << GLEnums::GetStringEnum(name) << ")");
  const GLubyte* result = gl_api_->glGetStringFn(name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

const GLubyte* DebugGLApi::glGetStringiFn(GLenum name, GLuint index) {
  GL_SERVICE_LOG("glGetStringi"
                 << "(" << GLEnums::GetStringEnum(name) << ", " << index
                 << ")");
  const GLubyte* result = gl_api_->glGetStringiFn(name, index);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetSyncivFn(GLsync sync,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* values) {
  GL_SERVICE_LOG("glGetSynciv"
                 << "(" << sync << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(values) << ")");
  gl_api_->glGetSyncivFn(sync, pname, bufSize, length, values);
}

void DebugGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterfvFn(target, level, pname, params);
}

void DebugGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterfvRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void DebugGLApi::glGetTexLevelParameterivFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterivFn(target, level, pname, params);
}

void DebugGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterivRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void DebugGLApi::glGetTexParameterfvFn(GLenum target,
                                       GLenum pname,
                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterfvFn(target, pname, params);
}

void DebugGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void DebugGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                             params);
}

void DebugGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  GL_SERVICE_LOG("glGetTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                              params);
}

void DebugGLApi::glGetTexParameterivFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) {
  GL_SERVICE_LOG("glGetTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterivFn(target, pname, params);
}

void DebugGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void DebugGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLsizei* size,
                                                 GLenum* type,
                                                 char* name) {
  GL_SERVICE_LOG("glGetTransformFeedbackVarying"
                 << "(" << program << ", " << index << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                           size, type, name);
}

void DebugGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  GL_SERVICE_LOG("glGetTranslatedShaderSourceANGLE"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  gl_api_->glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length, source);
}

GLuint DebugGLApi::glGetUniformBlockIndexFn(GLuint program,
                                            const char* uniformBlockName) {
  GL_SERVICE_LOG("glGetUniformBlockIndex"
                 << "(" << program << ", " << uniformBlockName << ")");
  GLuint result = gl_api_->glGetUniformBlockIndexFn(program, uniformBlockName);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetUniformfvFn(GLuint program,
                                  GLint location,
                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformfvFn(program, location, params);
}

void DebugGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

void DebugGLApi::glGetUniformIndicesFn(GLuint program,
                                       GLsizei uniformCount,
                                       const char* const* uniformNames,
                                       GLuint* uniformIndices) {
  GL_SERVICE_LOG("glGetUniformIndices"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformNames) << ", "
                 << static_cast<const void*>(uniformIndices) << ")");
  gl_api_->glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                 uniformIndices);
}

void DebugGLApi::glGetUniformivFn(GLuint program,
                                  GLint location,
                                  GLint* params) {
  GL_SERVICE_LOG("glGetUniformiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformivFn(program, location, params);
}

void DebugGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

GLint DebugGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetUniformLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetUniformLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glGetUniformuivFn(GLuint program,
                                   GLint location,
                                   GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformuivFn(program, location, params);
}

void DebugGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void DebugGLApi::glGetVertexAttribfvFn(GLuint index,
                                       GLenum pname,
                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribfvFn(index, pname, params);
}

void DebugGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfvRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void DebugGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                             params);
}

void DebugGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIuivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                              params);
}

void DebugGLApi::glGetVertexAttribivFn(GLuint index,
                                       GLenum pname,
                                       GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribiv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribivFn(index, pname, params);
}

void DebugGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void DebugGLApi::glGetVertexAttribPointervFn(GLuint index,
                                             GLenum pname,
                                             void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointerv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << pointer << ")");
  gl_api_->glGetVertexAttribPointervFn(index, pname, pointer);
}

void DebugGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointervRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << pointer << ")");
  gl_api_->glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize, length,
                                                  pointer);
}

void DebugGLApi::glHintFn(GLenum target, GLenum mode) {
  GL_SERVICE_LOG("glHint"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glHintFn(target, mode);
}

void DebugGLApi::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  GL_SERVICE_LOG("glInsertEventMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  gl_api_->glInsertEventMarkerEXTFn(length, marker);
}

void DebugGLApi::glInvalidateFramebufferFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  GL_SERVICE_LOG("glInvalidateFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  gl_api_->glInvalidateFramebufferFn(target, numAttachments, attachments);
}

void DebugGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments,
                                              GLint x,
                                              GLint y,
                                              GLint width,
                                              GLint height) {
  GL_SERVICE_LOG("glInvalidateSubFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ", " << x << ", "
                 << y << ", " << width << ", " << height << ")");
  gl_api_->glInvalidateSubFramebufferFn(target, numAttachments, attachments, x,
                                        y, width, height);
}

GLboolean DebugGLApi::glIsBufferFn(GLuint buffer) {
  GL_SERVICE_LOG("glIsBuffer"
                 << "(" << buffer << ")");
  GLboolean result = gl_api_->glIsBufferFn(buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsEnabledFn(GLenum cap) {
  GL_SERVICE_LOG("glIsEnabled"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  GLboolean result = gl_api_->glIsEnabledFn(cap);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceAPPLE"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glIsFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceNV"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glIsFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  GL_SERVICE_LOG("glIsFramebufferEXT"
                 << "(" << framebuffer << ")");
  GLboolean result = gl_api_->glIsFramebufferEXTFn(framebuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsPathNVFn(GLuint path) {
  GL_SERVICE_LOG("glIsPathNV"
                 << "(" << path << ")");
  GLboolean result = gl_api_->glIsPathNVFn(path);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsProgramFn(GLuint program) {
  GL_SERVICE_LOG("glIsProgram"
                 << "(" << program << ")");
  GLboolean result = gl_api_->glIsProgramFn(program);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsProgramPipelineFn(GLuint pipeline) {
  GL_SERVICE_LOG("glIsProgramPipeline"
                 << "(" << pipeline << ")");
  GLboolean result = gl_api_->glIsProgramPipelineFn(pipeline);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsQueryFn(GLuint query) {
  GL_SERVICE_LOG("glIsQuery"
                 << "(" << query << ")");
  GLboolean result = gl_api_->glIsQueryFn(query);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  GL_SERVICE_LOG("glIsRenderbufferEXT"
                 << "(" << renderbuffer << ")");
  GLboolean result = gl_api_->glIsRenderbufferEXTFn(renderbuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsSamplerFn(GLuint sampler) {
  GL_SERVICE_LOG("glIsSampler"
                 << "(" << sampler << ")");
  GLboolean result = gl_api_->glIsSamplerFn(sampler);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glIsShader"
                 << "(" << shader << ")");
  GLboolean result = gl_api_->glIsShaderFn(shader);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsSyncFn(GLsync sync) {
  GL_SERVICE_LOG("glIsSync"
                 << "(" << sync << ")");
  GLboolean result = gl_api_->glIsSyncFn(sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsTextureFn(GLuint texture) {
  GL_SERVICE_LOG("glIsTexture"
                 << "(" << texture << ")");
  GLboolean result = gl_api_->glIsTextureFn(texture);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsTransformFeedbackFn(GLuint id) {
  GL_SERVICE_LOG("glIsTransformFeedback"
                 << "(" << id << ")");
  GLboolean result = gl_api_->glIsTransformFeedbackFn(id);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glIsVertexArrayOESFn(GLuint array) {
  GL_SERVICE_LOG("glIsVertexArrayOES"
                 << "(" << array << ")");
  GLboolean result = gl_api_->glIsVertexArrayOESFn(array);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glLineWidthFn(GLfloat width) {
  GL_SERVICE_LOG("glLineWidth"
                 << "(" << width << ")");
  gl_api_->glLineWidthFn(width);
}

void DebugGLApi::glLinkProgramFn(GLuint program) {
  GL_SERVICE_LOG("glLinkProgram"
                 << "(" << program << ")");
  gl_api_->glLinkProgramFn(program);
}

void* DebugGLApi::glMapBufferFn(GLenum target, GLenum access) {
  GL_SERVICE_LOG("glMapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(access) << ")");
  void* result = gl_api_->glMapBufferFn(target, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void* DebugGLApi::glMapBufferRangeFn(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  GL_SERVICE_LOG("glMapBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ", " << access << ")");
  void* result = gl_api_->glMapBufferRangeFn(target, offset, length, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  GL_SERVICE_LOG("glMatrixLoadfEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ", "
                 << static_cast<const void*>(m) << ")");
  gl_api_->glMatrixLoadfEXTFn(matrixMode, m);
}

void DebugGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  GL_SERVICE_LOG("glMatrixLoadIdentityEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ")");
  gl_api_->glMatrixLoadIdentityEXTFn(matrixMode);
}

void DebugGLApi::glMaxShaderCompilerThreadsKHRFn(GLuint count) {
  GL_SERVICE_LOG("glMaxShaderCompilerThreadsKHR"
                 << "(" << count << ")");
  gl_api_->glMaxShaderCompilerThreadsKHRFn(count);
}

void DebugGLApi::glMemoryBarrierByRegionFn(GLbitfield barriers) {
  GL_SERVICE_LOG("glMemoryBarrierByRegion"
                 << "(" << barriers << ")");
  gl_api_->glMemoryBarrierByRegionFn(barriers);
}

void DebugGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  GL_SERVICE_LOG("glMemoryBarrierEXT"
                 << "(" << barriers << ")");
  gl_api_->glMemoryBarrierEXTFn(barriers);
}

void DebugGLApi::glMinSampleShadingFn(GLfloat value) {
  GL_SERVICE_LOG("glMinSampleShading"
                 << "(" << value << ")");
  gl_api_->glMinSampleShadingFn(value);
}

void DebugGLApi::glObjectLabelFn(GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const char* label) {
  GL_SERVICE_LOG("glObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << length << ", " << label << ")");
  gl_api_->glObjectLabelFn(identifier, name, length, label);
}

void DebugGLApi::glObjectPtrLabelFn(void* ptr,
                                    GLsizei length,
                                    const char* label) {
  GL_SERVICE_LOG("glObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << length
                 << ", " << label << ")");
  gl_api_->glObjectPtrLabelFn(ptr, length, label);
}

void DebugGLApi::glPathCommandsNVFn(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords) {
  GL_SERVICE_LOG("glPathCommandsNV"
                 << "(" << path << ", " << numCommands << ", "
                 << static_cast<const void*>(commands) << ", " << numCoords
                 << ", " << GLEnums::GetStringEnum(coordType) << ", "
                 << static_cast<const void*>(coords) << ")");
  gl_api_->glPathCommandsNVFn(path, numCommands, commands, numCoords, coordType,
                              coords);
}

void DebugGLApi::glPathParameterfNVFn(GLuint path,
                                      GLenum pname,
                                      GLfloat value) {
  GL_SERVICE_LOG("glPathParameterfNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  gl_api_->glPathParameterfNVFn(path, pname, value);
}

void DebugGLApi::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  GL_SERVICE_LOG("glPathParameteriNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  gl_api_->glPathParameteriNVFn(path, pname, value);
}

void DebugGLApi::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  GL_SERVICE_LOG("glPathStencilFuncNV"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  gl_api_->glPathStencilFuncNVFn(func, ref, mask);
}

void DebugGLApi::glPauseTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glPauseTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glPauseTransformFeedbackFn();
}

void DebugGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPixelStorei"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  gl_api_->glPixelStoreiFn(pname, param);
}

void DebugGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPointParameteri"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  gl_api_->glPointParameteriFn(pname, param);
}

void DebugGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  GL_SERVICE_LOG("glPolygonMode"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glPolygonModeFn(face, mode);
}

void DebugGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  GL_SERVICE_LOG("glPolygonOffset"
                 << "(" << factor << ", " << units << ")");
  gl_api_->glPolygonOffsetFn(factor, units);
}

void DebugGLApi::glPopDebugGroupFn() {
  GL_SERVICE_LOG("glPopDebugGroup"
                 << "("
                 << ")");
  gl_api_->glPopDebugGroupFn();
}

void DebugGLApi::glPopGroupMarkerEXTFn(void) {
  GL_SERVICE_LOG("glPopGroupMarkerEXT"
                 << "("
                 << ")");
  gl_api_->glPopGroupMarkerEXTFn();
}

void DebugGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  GL_SERVICE_LOG("glPrimitiveRestartIndex"
                 << "(" << index << ")");
  gl_api_->glPrimitiveRestartIndexFn(index);
}

void DebugGLApi::glProgramBinaryFn(GLuint program,
                                   GLenum binaryFormat,
                                   const GLvoid* binary,
                                   GLsizei length) {
  GL_SERVICE_LOG("glProgramBinary"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  gl_api_->glProgramBinaryFn(program, binaryFormat, binary, length);
}

void DebugGLApi::glProgramParameteriFn(GLuint program,
                                       GLenum pname,
                                       GLint value) {
  GL_SERVICE_LOG("glProgramParameteri"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << value << ")");
  gl_api_->glProgramParameteriFn(program, pname, value);
}

void DebugGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs) {
  GL_SERVICE_LOG("glProgramPathFragmentInputGenNV"
                 << "(" << program << ", " << location << ", "
                 << GLEnums::GetStringEnum(genMode) << ", " << components
                 << ", " << static_cast<const void*>(coeffs) << ")");
  gl_api_->glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                             components, coeffs);
}

void DebugGLApi::glProgramUniform1fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0) {
  GL_SERVICE_LOG("glProgramUniform1f"
                 << "(" << program << ", " << location << ", " << v0 << ")");
  gl_api_->glProgramUniform1fFn(program, location, v0);
}

void DebugGLApi::glProgramUniform1fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniform1fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform1fvFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform1iFn(GLuint program,
                                      GLint location,
                                      GLint v0) {
  GL_SERVICE_LOG("glProgramUniform1i"
                 << "(" << program << ", " << location << ", " << v0 << ")");
  gl_api_->glProgramUniform1iFn(program, location, v0);
}

void DebugGLApi::glProgramUniform1ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  GL_SERVICE_LOG("glProgramUniform1iv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform1ivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform1uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0) {
  GL_SERVICE_LOG("glProgramUniform1ui"
                 << "(" << program << ", " << location << ", " << v0 << ")");
  gl_api_->glProgramUniform1uiFn(program, location, v0);
}

void DebugGLApi::glProgramUniform1uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  GL_SERVICE_LOG("glProgramUniform1uiv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform1uivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform2fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1) {
  GL_SERVICE_LOG("glProgramUniform2f"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ")");
  gl_api_->glProgramUniform2fFn(program, location, v0, v1);
}

void DebugGLApi::glProgramUniform2fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniform2fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform2fvFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform2iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1) {
  GL_SERVICE_LOG("glProgramUniform2i"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ")");
  gl_api_->glProgramUniform2iFn(program, location, v0, v1);
}

void DebugGLApi::glProgramUniform2ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  GL_SERVICE_LOG("glProgramUniform2iv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform2ivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform2uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1) {
  GL_SERVICE_LOG("glProgramUniform2ui"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ")");
  gl_api_->glProgramUniform2uiFn(program, location, v0, v1);
}

void DebugGLApi::glProgramUniform2uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  GL_SERVICE_LOG("glProgramUniform2uiv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform2uivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform3fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1,
                                      GLfloat v2) {
  GL_SERVICE_LOG("glProgramUniform3f"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ")");
  gl_api_->glProgramUniform3fFn(program, location, v0, v1, v2);
}

void DebugGLApi::glProgramUniform3fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniform3fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform3fvFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform3iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1,
                                      GLint v2) {
  GL_SERVICE_LOG("glProgramUniform3i"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ")");
  gl_api_->glProgramUniform3iFn(program, location, v0, v1, v2);
}

void DebugGLApi::glProgramUniform3ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  GL_SERVICE_LOG("glProgramUniform3iv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform3ivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform3uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1,
                                       GLuint v2) {
  GL_SERVICE_LOG("glProgramUniform3ui"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ")");
  gl_api_->glProgramUniform3uiFn(program, location, v0, v1, v2);
}

void DebugGLApi::glProgramUniform3uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  GL_SERVICE_LOG("glProgramUniform3uiv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform3uivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform4fFn(GLuint program,
                                      GLint location,
                                      GLfloat v0,
                                      GLfloat v1,
                                      GLfloat v2,
                                      GLfloat v3) {
  GL_SERVICE_LOG("glProgramUniform4f"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ", " << v3 << ")");
  gl_api_->glProgramUniform4fFn(program, location, v0, v1, v2, v3);
}

void DebugGLApi::glProgramUniform4fvFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniform4fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform4fvFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform4iFn(GLuint program,
                                      GLint location,
                                      GLint v0,
                                      GLint v1,
                                      GLint v2,
                                      GLint v3) {
  GL_SERVICE_LOG("glProgramUniform4i"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ", " << v3 << ")");
  gl_api_->glProgramUniform4iFn(program, location, v0, v1, v2, v3);
}

void DebugGLApi::glProgramUniform4ivFn(GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       const GLint* value) {
  GL_SERVICE_LOG("glProgramUniform4iv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform4ivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniform4uiFn(GLuint program,
                                       GLint location,
                                       GLuint v0,
                                       GLuint v1,
                                       GLuint v2,
                                       GLuint v3) {
  GL_SERVICE_LOG("glProgramUniform4ui"
                 << "(" << program << ", " << location << ", " << v0 << ", "
                 << v1 << ", " << v2 << ", " << v3 << ")");
  gl_api_->glProgramUniform4uiFn(program, location, v0, v1, v2, v3);
}

void DebugGLApi::glProgramUniform4uivFn(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        const GLuint* value) {
  GL_SERVICE_LOG("glProgramUniform4uiv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniform4uivFn(program, location, count, value);
}

void DebugGLApi::glProgramUniformMatrix2fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix2fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix2fvFn(program, location, count, transpose,
                                       value);
}

void DebugGLApi::glProgramUniformMatrix2x3fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix2x3fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix2x3fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glProgramUniformMatrix2x4fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix2x4fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix2x4fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glProgramUniformMatrix3fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix3fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix3fvFn(program, location, count, transpose,
                                       value);
}

void DebugGLApi::glProgramUniformMatrix3x2fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix3x2fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix3x2fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glProgramUniformMatrix3x4fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix3x4fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix3x4fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glProgramUniformMatrix4fvFn(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix4fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix4fvFn(program, location, count, transpose,
                                       value);
}

void DebugGLApi::glProgramUniformMatrix4x2fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix4x2fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix4x2fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glProgramUniformMatrix4x3fvFn(GLuint program,
                                               GLint location,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat* value) {
  GL_SERVICE_LOG("glProgramUniformMatrix4x3fv"
                 << "(" << program << ", " << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glProgramUniformMatrix4x3fvFn(program, location, count, transpose,
                                         value);
}

void DebugGLApi::glPushDebugGroupFn(GLenum source,
                                    GLuint id,
                                    GLsizei length,
                                    const char* message) {
  GL_SERVICE_LOG("glPushDebugGroup"
                 << "(" << GLEnums::GetStringEnum(source) << ", " << id << ", "
                 << length << ", " << message << ")");
  gl_api_->glPushDebugGroupFn(source, id, length, message);
}

void DebugGLApi::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  GL_SERVICE_LOG("glPushGroupMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  gl_api_->glPushGroupMarkerEXTFn(length, marker);
}

void DebugGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  GL_SERVICE_LOG("glQueryCounter"
                 << "(" << id << ", " << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glQueryCounterFn(id, target);
}

void DebugGLApi::glReadBufferFn(GLenum src) {
  GL_SERVICE_LOG("glReadBuffer"
                 << "(" << GLEnums::GetStringEnum(src) << ")");
  gl_api_->glReadBufferFn(src);
}

void DebugGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLsizei* columns,
                                            GLsizei* rows,
                                            void* data) {
  GL_SERVICE_LOG("glReadnPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(columns) << ", "
                 << static_cast<const void*>(rows) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                      bufSize, length, columns, rows, data);
}

void DebugGLApi::glReadPixelsFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                void* pixels) {
  GL_SERVICE_LOG("glReadPixels"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void DebugGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* pixels) {
  GL_SERVICE_LOG("glReadPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(columns) << ", "
                 << static_cast<const void*>(rows) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufSize,
                                     length, columns, rows, pixels);
}

void DebugGLApi::glReleaseShaderCompilerFn(void) {
  GL_SERVICE_LOG("glReleaseShaderCompiler"
                 << "("
                 << ")");
  gl_api_->glReleaseShaderCompilerFn();
}

void DebugGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << width
                 << ", " << height << ")");
  gl_api_->glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void DebugGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisample"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                              width, height);
}

void DebugGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glRenderbufferStorageMultisampleEXTFn(target, samples,
                                                 internalformat, width, height);
}

void DebugGLApi::glRequestExtensionANGLEFn(const char* name) {
  GL_SERVICE_LOG("glRequestExtensionANGLE"
                 << "(" << name << ")");
  gl_api_->glRequestExtensionANGLEFn(name);
}

void DebugGLApi::glResumeTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glResumeTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glResumeTransformFeedbackFn();
}

void DebugGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  GL_SERVICE_LOG("glSampleCoverage"
                 << "(" << value << ", " << GLEnums::GetStringBool(invert)
                 << ")");
  gl_api_->glSampleCoverageFn(value, invert);
}

void DebugGLApi::glSampleMaskiFn(GLuint maskNumber, GLbitfield mask) {
  GL_SERVICE_LOG("glSampleMaski"
                 << "(" << maskNumber << ", " << mask << ")");
  gl_api_->glSampleMaskiFn(maskNumber, mask);
}

void DebugGLApi::glSamplerParameterfFn(GLuint sampler,
                                       GLenum pname,
                                       GLfloat param) {
  GL_SERVICE_LOG("glSamplerParameterf"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  gl_api_->glSamplerParameterfFn(sampler, pname, param);
}

void DebugGLApi::glSamplerParameterfvFn(GLuint sampler,
                                        GLenum pname,
                                        const GLfloat* params) {
  GL_SERVICE_LOG("glSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glSamplerParameterfvFn(sampler, pname, params);
}

void DebugGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* param) {
  GL_SERVICE_LOG("glSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

void DebugGLApi::glSamplerParameteriFn(GLuint sampler,
                                       GLenum pname,
                                       GLint param) {
  GL_SERVICE_LOG("glSamplerParameteri"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  gl_api_->glSamplerParameteriFn(sampler, pname, param);
}

void DebugGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize, param);
}

void DebugGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* param) {
  GL_SERVICE_LOG("glSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize, param);
}

void DebugGLApi::glSamplerParameterivFn(GLuint sampler,
                                        GLenum pname,
                                        const GLint* params) {
  GL_SERVICE_LOG("glSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glSamplerParameterivFn(sampler, pname, params);
}

void DebugGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

void DebugGLApi::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  GL_SERVICE_LOG("glScissor"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glScissorFn(x, y, width, height);
}

void DebugGLApi::glSetFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glSetFenceAPPLE"
                 << "(" << fence << ")");
  gl_api_->glSetFenceAPPLEFn(fence);
}

void DebugGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  GL_SERVICE_LOG("glSetFenceNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(condition)
                 << ")");
  gl_api_->glSetFenceNVFn(fence, condition);
}

void DebugGLApi::glShaderBinaryFn(GLsizei n,
                                  const GLuint* shaders,
                                  GLenum binaryformat,
                                  const void* binary,
                                  GLsizei length) {
  GL_SERVICE_LOG("glShaderBinary"
                 << "(" << n << ", " << static_cast<const void*>(shaders)
                 << ", " << GLEnums::GetStringEnum(binaryformat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  gl_api_->glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void DebugGLApi::glShaderSourceFn(GLuint shader,
                                  GLsizei count,
                                  const char* const* str,
                                  const GLint* length) {
  GL_SERVICE_LOG("glShaderSource"
                 << "(" << shader << ", " << count << ", "
                 << static_cast<const void*>(str) << ", "
                 << static_cast<const void*>(length) << ")");
  gl_api_->glShaderSourceFn(shader, count, str, length);

  GL_SERVICE_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (str[ii]) {
        if (length && length[ii] >= 0) {
          std::string source(str[ii], length[ii]);
          GL_SERVICE_LOG("  " << ii << ": ---\n" << source << "\n---");
        } else {
          GL_SERVICE_LOG("  " << ii << ": ---\n" << str[ii] << "\n---");
        }
      } else {
        GL_SERVICE_LOG("  " << ii << ": NULL");
      }
    }
  });
}

void DebugGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, fillMode, mask,
                                          transformType, transformValues);
}

void DebugGLApi::glStencilFillPathNVFn(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask) {
  GL_SERVICE_LOG("glStencilFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ")");
  gl_api_->glStencilFillPathNVFn(path, fillMode, mask);
}

void DebugGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  GL_SERVICE_LOG("glStencilFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  gl_api_->glStencilFuncFn(func, ref, mask);
}

void DebugGLApi::glStencilFuncSeparateFn(GLenum face,
                                         GLenum func,
                                         GLint ref,
                                         GLuint mask) {
  GL_SERVICE_LOG("glStencilFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(func) << ", " << ref << ", " << mask
                 << ")");
  gl_api_->glStencilFuncSeparateFn(face, func, ref, mask);
}

void DebugGLApi::glStencilMaskFn(GLuint mask) {
  GL_SERVICE_LOG("glStencilMask"
                 << "(" << mask << ")");
  gl_api_->glStencilMaskFn(mask);
}

void DebugGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  GL_SERVICE_LOG("glStencilMaskSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", " << mask << ")");
  gl_api_->glStencilMaskSeparateFn(face, mask);
}

void DebugGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  GL_SERVICE_LOG("glStencilOp"
                 << "(" << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  gl_api_->glStencilOpFn(fail, zfail, zpass);
}

void DebugGLApi::glStencilOpSeparateFn(GLenum face,
                                       GLenum fail,
                                       GLenum zfail,
                                       GLenum zpass) {
  GL_SERVICE_LOG("glStencilOpSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  gl_api_->glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void DebugGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                            pathBase, ref, mask, transformType,
                                            transformValues);
}

void DebugGLApi::glStencilStrokePathNVFn(GLuint path,
                                         GLint reference,
                                         GLuint mask) {
  GL_SERVICE_LOG("glStencilStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ")");
  gl_api_->glStencilStrokePathNVFn(path, reference, mask);
}

void DebugGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

void DebugGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

void DebugGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilThenCoverStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode) << ", "
      << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

void DebugGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ")");
  gl_api_->glStencilThenCoverStrokePathNVFn(path, reference, mask, coverMode);
}

GLboolean DebugGLApi::glTestFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceAPPLE"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glTestFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

GLboolean DebugGLApi::glTestFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceNV"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glTestFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glTexBufferFn(GLenum target,
                               GLenum internalformat,
                               GLuint buffer) {
  GL_SERVICE_LOG("glTexBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << buffer
                 << ")");
  gl_api_->glTexBufferFn(target, internalformat, buffer);
}

void DebugGLApi::glTexBufferRangeFn(GLenum target,
                                    GLenum internalformat,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  GL_SERVICE_LOG("glTexBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << buffer
                 << ", " << offset << ", " << size << ")");
  gl_api_->glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

void DebugGLApi::glTexImage2DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  GL_SERVICE_LOG("glTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage2DFn(target, level, internalformat, width, height, border,
                          format, type, pixels);
}

void DebugGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  GL_SERVICE_LOG("glTexImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", " << bufSize
                 << ", " << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                     height, border, format, type, bufSize,
                                     pixels);
}

void DebugGLApi::glTexImage3DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  GL_SERVICE_LOG("glTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage3DFn(target, level, internalformat, width, height, depth,
                          border, format, type, pixels);
}

void DebugGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  GL_SERVICE_LOG("glTexImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                     height, depth, border, format, type,
                                     bufSize, pixels);
}

void DebugGLApi::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  GL_SERVICE_LOG("glTexParameterf"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  gl_api_->glTexParameterfFn(target, pname, param);
}

void DebugGLApi::glTexParameterfvFn(GLenum target,
                                    GLenum pname,
                                    const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterfvFn(target, pname, params);
}

void DebugGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

void DebugGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  GL_SERVICE_LOG("glTexParameteri"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  gl_api_->glTexParameteriFn(target, pname, param);
}

void DebugGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLint* params) {
  GL_SERVICE_LOG("glTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

void DebugGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLuint* params) {
  GL_SERVICE_LOG("glTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

void DebugGLApi::glTexParameterivFn(GLenum target,
                                    GLenum pname,
                                    const GLint* params) {
  GL_SERVICE_LOG("glTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterivFn(target, pname, params);
}

void DebugGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  GL_SERVICE_LOG("glTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

void DebugGLApi::glTexStorage2DEXTFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  GL_SERVICE_LOG("glTexStorage2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glTexStorage2DEXTFn(target, levels, internalformat, width, height);
}

void DebugGLApi::glTexStorage2DMultisampleFn(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLboolean fixedsamplelocations) {
  GL_SERVICE_LOG("glTexStorage2DMultisample"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", "
                 << GLEnums::GetStringBool(fixedsamplelocations) << ")");
  gl_api_->glTexStorage2DMultisampleFn(target, samples, internalformat, width,
                                       height, fixedsamplelocations);
}

void DebugGLApi::glTexStorage3DFn(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth) {
  GL_SERVICE_LOG("glTexStorage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ")");
  gl_api_->glTexStorage3DFn(target, levels, internalformat, width, height,
                            depth);
}

void DebugGLApi::glTexSubImage2DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                             format, type, pixels);
}

void DebugGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                        height, format, type, bufSize, pixels);
}

void DebugGLApi::glTexSubImage3DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, pixels);
}

void DebugGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLint zoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                        zoffset, width, height, depth, format,
                                        type, bufSize, pixels);
}

void DebugGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                               GLsizei count,
                                               const char* const* varyings,
                                               GLenum bufferMode) {
  GL_SERVICE_LOG("glTransformFeedbackVaryings"
                 << "(" << program << ", " << count << ", "
                 << static_cast<const void*>(varyings) << ", "
                 << GLEnums::GetStringEnum(bufferMode) << ")");
  gl_api_->glTransformFeedbackVaryingsFn(program, count, varyings, bufferMode);
}

void DebugGLApi::glUniform1fFn(GLint location, GLfloat x) {
  GL_SERVICE_LOG("glUniform1f"
                 << "(" << location << ", " << x << ")");
  gl_api_->glUniform1fFn(location, x);
}

void DebugGLApi::glUniform1fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform1fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1fvFn(location, count, v);
}

void DebugGLApi::glUniform1iFn(GLint location, GLint x) {
  GL_SERVICE_LOG("glUniform1i"
                 << "(" << location << ", " << x << ")");
  gl_api_->glUniform1iFn(location, x);
}

void DebugGLApi::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform1iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1ivFn(location, count, v);
}

void DebugGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  GL_SERVICE_LOG("glUniform1ui"
                 << "(" << location << ", " << v0 << ")");
  gl_api_->glUniform1uiFn(location, v0);
}

void DebugGLApi::glUniform1uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform1uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1uivFn(location, count, v);
}

void DebugGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  GL_SERVICE_LOG("glUniform2f"
                 << "(" << location << ", " << x << ", " << y << ")");
  gl_api_->glUniform2fFn(location, x, y);
}

void DebugGLApi::glUniform2fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform2fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2fvFn(location, count, v);
}

void DebugGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  GL_SERVICE_LOG("glUniform2i"
                 << "(" << location << ", " << x << ", " << y << ")");
  gl_api_->glUniform2iFn(location, x, y);
}

void DebugGLApi::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform2iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2ivFn(location, count, v);
}

void DebugGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  GL_SERVICE_LOG("glUniform2ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ")");
  gl_api_->glUniform2uiFn(location, v0, v1);
}

void DebugGLApi::glUniform2uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform2uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2uivFn(location, count, v);
}

void DebugGLApi::glUniform3fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z) {
  GL_SERVICE_LOG("glUniform3f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  gl_api_->glUniform3fFn(location, x, y, z);
}

void DebugGLApi::glUniform3fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform3fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3fvFn(location, count, v);
}

void DebugGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  GL_SERVICE_LOG("glUniform3i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  gl_api_->glUniform3iFn(location, x, y, z);
}

void DebugGLApi::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform3iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3ivFn(location, count, v);
}

void DebugGLApi::glUniform3uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2) {
  GL_SERVICE_LOG("glUniform3ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ")");
  gl_api_->glUniform3uiFn(location, v0, v1, v2);
}

void DebugGLApi::glUniform3uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform3uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3uivFn(location, count, v);
}

void DebugGLApi::glUniform4fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z,
                               GLfloat w) {
  GL_SERVICE_LOG("glUniform4f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  gl_api_->glUniform4fFn(location, x, y, z, w);
}

void DebugGLApi::glUniform4fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform4fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4fvFn(location, count, v);
}

void DebugGLApi::glUniform4iFn(GLint location,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLint w) {
  GL_SERVICE_LOG("glUniform4i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  gl_api_->glUniform4iFn(location, x, y, z, w);
}

void DebugGLApi::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform4iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4ivFn(location, count, v);
}

void DebugGLApi::glUniform4uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3) {
  GL_SERVICE_LOG("glUniform4ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ", " << v3 << ")");
  gl_api_->glUniform4uiFn(location, v0, v1, v2, v3);
}

void DebugGLApi::glUniform4uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform4uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4uivFn(location, count, v);
}

void DebugGLApi::glUniformBlockBindingFn(GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLuint uniformBlockBinding) {
  GL_SERVICE_LOG("glUniformBlockBinding"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << uniformBlockBinding << ")");
  gl_api_->glUniformBlockBindingFn(program, uniformBlockIndex,
                                   uniformBlockBinding);
}

void DebugGLApi::glUniformMatrix2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix2x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2x3fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix2x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2x4fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix3x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3x2fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix3x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3x4fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix4x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4x2fvFn(location, count, transpose, value);
}

void DebugGLApi::glUniformMatrix4x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4x3fvFn(location, count, transpose, value);
}

GLboolean DebugGLApi::glUnmapBufferFn(GLenum target) {
  GL_SERVICE_LOG("glUnmapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  GLboolean result = gl_api_->glUnmapBufferFn(target);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

void DebugGLApi::glUseProgramFn(GLuint program) {
  GL_SERVICE_LOG("glUseProgram"
                 << "(" << program << ")");
  gl_api_->glUseProgramFn(program);
}

void DebugGLApi::glUseProgramStagesFn(GLuint pipeline,
                                      GLbitfield stages,
                                      GLuint program) {
  GL_SERVICE_LOG("glUseProgramStages"
                 << "(" << pipeline << ", " << stages << ", " << program
                 << ")");
  gl_api_->glUseProgramStagesFn(pipeline, stages, program);
}

void DebugGLApi::glValidateProgramFn(GLuint program) {
  GL_SERVICE_LOG("glValidateProgram"
                 << "(" << program << ")");
  gl_api_->glValidateProgramFn(program);
}

void DebugGLApi::glValidateProgramPipelineFn(GLuint pipeline) {
  GL_SERVICE_LOG("glValidateProgramPipeline"
                 << "(" << pipeline << ")");
  gl_api_->glValidateProgramPipelineFn(pipeline);
}

void DebugGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  GL_SERVICE_LOG("glVertexAttrib1f"
                 << "(" << indx << ", " << x << ")");
  gl_api_->glVertexAttrib1fFn(indx, x);
}

void DebugGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib1fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib1fvFn(indx, values);
}

void DebugGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  GL_SERVICE_LOG("glVertexAttrib2f"
                 << "(" << indx << ", " << x << ", " << y << ")");
  gl_api_->glVertexAttrib2fFn(indx, x, y);
}

void DebugGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib2fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib2fvFn(indx, values);
}

void DebugGLApi::glVertexAttrib3fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  GL_SERVICE_LOG("glVertexAttrib3f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ")");
  gl_api_->glVertexAttrib3fFn(indx, x, y, z);
}

void DebugGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib3fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib3fvFn(indx, values);
}

void DebugGLApi::glVertexAttrib4fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  GL_SERVICE_LOG("glVertexAttrib4f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttrib4fFn(indx, x, y, z, w);
}

void DebugGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib4fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib4fvFn(indx, values);
}

void DebugGLApi::glVertexAttribBindingFn(GLuint attribindex,
                                         GLuint bindingindex) {
  GL_SERVICE_LOG("glVertexAttribBinding"
                 << "(" << attribindex << ", " << bindingindex << ")");
  gl_api_->glVertexAttribBindingFn(attribindex, bindingindex);
}

void DebugGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  GL_SERVICE_LOG("glVertexAttribDivisorANGLE"
                 << "(" << index << ", " << divisor << ")");
  gl_api_->glVertexAttribDivisorANGLEFn(index, divisor);
}

void DebugGLApi::glVertexAttribFormatFn(GLuint attribindex,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLuint relativeoffset) {
  GL_SERVICE_LOG("glVertexAttribFormat"
                 << "(" << attribindex << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringBool(normalized) << ", " << relativeoffset
                 << ")");
  gl_api_->glVertexAttribFormatFn(attribindex, size, type, normalized,
                                  relativeoffset);
}

void DebugGLApi::glVertexAttribI4iFn(GLuint indx,
                                     GLint x,
                                     GLint y,
                                     GLint z,
                                     GLint w) {
  GL_SERVICE_LOG("glVertexAttribI4i"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttribI4iFn(indx, x, y, z, w);
}

void DebugGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  GL_SERVICE_LOG("glVertexAttribI4iv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttribI4ivFn(indx, values);
}

void DebugGLApi::glVertexAttribI4uiFn(GLuint indx,
                                      GLuint x,
                                      GLuint y,
                                      GLuint z,
                                      GLuint w) {
  GL_SERVICE_LOG("glVertexAttribI4ui"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttribI4uiFn(indx, x, y, z, w);
}

void DebugGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  GL_SERVICE_LOG("glVertexAttribI4uiv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttribI4uivFn(indx, values);
}

void DebugGLApi::glVertexAttribIFormatFn(GLuint attribindex,
                                         GLint size,
                                         GLenum type,
                                         GLuint relativeoffset) {
  GL_SERVICE_LOG("glVertexAttribIFormat"
                 << "(" << attribindex << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", " << relativeoffset
                 << ")");
  gl_api_->glVertexAttribIFormatFn(attribindex, size, type, relativeoffset);
}

void DebugGLApi::glVertexAttribIPointerFn(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribIPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  gl_api_->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

void DebugGLApi::glVertexAttribPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLboolean normalized,
                                         GLsizei stride,
                                         const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringBool(normalized) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  gl_api_->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
}

void DebugGLApi::glVertexBindingDivisorFn(GLuint bindingindex, GLuint divisor) {
  GL_SERVICE_LOG("glVertexBindingDivisor"
                 << "(" << bindingindex << ", " << divisor << ")");
  gl_api_->glVertexBindingDivisorFn(bindingindex, divisor);
}

void DebugGLApi::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  GL_SERVICE_LOG("glViewport"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glViewportFn(x, y, width, height);
}

void DebugGLApi::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  GL_SERVICE_LOG("glWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  gl_api_->glWaitSyncFn(sync, flags, timeout);
}

void DebugGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                         GLsizei n,
                                         const GLint* box) {
  GL_SERVICE_LOG("glWindowRectanglesEXT"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << n << ", "
                 << static_cast<const void*>(box) << ")");
  gl_api_->glWindowRectanglesEXTFn(mode, n, box);
}

namespace {
void NoContextHelper(const char* method_name) {
  NOTREACHED() << "Trying to call " << method_name
               << " without current GL context";
  LOG(ERROR) << "Trying to call " << method_name
             << " without current GL context";
}
}  // namespace

void NoContextGLApi::glActiveShaderProgramFn(GLuint pipeline, GLuint program) {
  NoContextHelper("glActiveShaderProgram");
}

void NoContextGLApi::glActiveTextureFn(GLenum texture) {
  NoContextHelper("glActiveTexture");
}

void NoContextGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  NoContextHelper("glApplyFramebufferAttachmentCMAAINTEL");
}

void NoContextGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  NoContextHelper("glAttachShader");
}

void NoContextGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  NoContextHelper("glBeginQuery");
}

void NoContextGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  NoContextHelper("glBeginTransformFeedback");
}

void NoContextGLApi::glBindAttribLocationFn(GLuint program,
                                            GLuint index,
                                            const char* name) {
  NoContextHelper("glBindAttribLocation");
}

void NoContextGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  NoContextHelper("glBindBuffer");
}

void NoContextGLApi::glBindBufferBaseFn(GLenum target,
                                        GLuint index,
                                        GLuint buffer) {
  NoContextHelper("glBindBufferBase");
}

void NoContextGLApi::glBindBufferRangeFn(GLenum target,
                                         GLuint index,
                                         GLuint buffer,
                                         GLintptr offset,
                                         GLsizeiptr size) {
  NoContextHelper("glBindBufferRange");
}

void NoContextGLApi::glBindFragDataLocationFn(GLuint program,
                                              GLuint colorNumber,
                                              const char* name) {
  NoContextHelper("glBindFragDataLocation");
}

void NoContextGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                     GLuint colorNumber,
                                                     GLuint index,
                                                     const char* name) {
  NoContextHelper("glBindFragDataLocationIndexed");
}

void NoContextGLApi::glBindFragmentInputLocationCHROMIUMFn(GLuint program,
                                                           GLint location,
                                                           const char* name) {
  NoContextHelper("glBindFragmentInputLocationCHROMIUM");
}

void NoContextGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  NoContextHelper("glBindFramebufferEXT");
}

void NoContextGLApi::glBindImageTextureEXTFn(GLuint index,
                                             GLuint texture,
                                             GLint level,
                                             GLboolean layered,
                                             GLint layer,
                                             GLenum access,
                                             GLint format) {
  NoContextHelper("glBindImageTextureEXT");
}

void NoContextGLApi::glBindProgramPipelineFn(GLuint pipeline) {
  NoContextHelper("glBindProgramPipeline");
}

void NoContextGLApi::glBindRenderbufferEXTFn(GLenum target,
                                             GLuint renderbuffer) {
  NoContextHelper("glBindRenderbufferEXT");
}

void NoContextGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  NoContextHelper("glBindSampler");
}

void NoContextGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  NoContextHelper("glBindTexture");
}

void NoContextGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  NoContextHelper("glBindTransformFeedback");
}

void NoContextGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                     GLint location,
                                                     const char* name) {
  NoContextHelper("glBindUniformLocationCHROMIUM");
}

void NoContextGLApi::glBindVertexArrayOESFn(GLuint array) {
  NoContextHelper("glBindVertexArrayOES");
}

void NoContextGLApi::glBindVertexBufferFn(GLuint bindingindex,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizei stride) {
  NoContextHelper("glBindVertexBuffer");
}

void NoContextGLApi::glBlendBarrierKHRFn(void) {
  NoContextHelper("glBlendBarrierKHR");
}

void NoContextGLApi::glBlendColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NoContextHelper("glBlendColor");
}

void NoContextGLApi::glBlendEquationFn(GLenum mode) {
  NoContextHelper("glBlendEquation");
}

void NoContextGLApi::glBlendEquationSeparateFn(GLenum modeRGB,
                                               GLenum modeAlpha) {
  NoContextHelper("glBlendEquationSeparate");
}

void NoContextGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  NoContextHelper("glBlendFunc");
}

void NoContextGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                           GLenum dstRGB,
                                           GLenum srcAlpha,
                                           GLenum dstAlpha) {
  NoContextHelper("glBlendFuncSeparate");
}

void NoContextGLApi::glBlitFramebufferFn(GLint srcX0,
                                         GLint srcY0,
                                         GLint srcX1,
                                         GLint srcY1,
                                         GLint dstX0,
                                         GLint dstY0,
                                         GLint dstX1,
                                         GLint dstY1,
                                         GLbitfield mask,
                                         GLenum filter) {
  NoContextHelper("glBlitFramebuffer");
}

void NoContextGLApi::glBufferDataFn(GLenum target,
                                    GLsizeiptr size,
                                    const void* data,
                                    GLenum usage) {
  NoContextHelper("glBufferData");
}

void NoContextGLApi::glBufferSubDataFn(GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr size,
                                       const void* data) {
  NoContextHelper("glBufferSubData");
}

GLenum NoContextGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  NoContextHelper("glCheckFramebufferStatusEXT");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glClearFn(GLbitfield mask) {
  NoContextHelper("glClear");
}

void NoContextGLApi::glClearBufferfiFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat depth,
                                       GLint stencil) {
  NoContextHelper("glClearBufferfi");
}

void NoContextGLApi::glClearBufferfvFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat* value) {
  NoContextHelper("glClearBufferfv");
}

void NoContextGLApi::glClearBufferivFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLint* value) {
  NoContextHelper("glClearBufferiv");
}

void NoContextGLApi::glClearBufferuivFn(GLenum buffer,
                                        GLint drawbuffer,
                                        const GLuint* value) {
  NoContextHelper("glClearBufferuiv");
}

void NoContextGLApi::glClearColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NoContextHelper("glClearColor");
}

void NoContextGLApi::glClearDepthFn(GLclampd depth) {
  NoContextHelper("glClearDepth");
}

void NoContextGLApi::glClearDepthfFn(GLclampf depth) {
  NoContextHelper("glClearDepthf");
}

void NoContextGLApi::glClearStencilFn(GLint s) {
  NoContextHelper("glClearStencil");
}

GLenum NoContextGLApi::glClientWaitSyncFn(GLsync sync,
                                          GLbitfield flags,
                                          GLuint64 timeout) {
  NoContextHelper("glClientWaitSync");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glColorMaskFn(GLboolean red,
                                   GLboolean green,
                                   GLboolean blue,
                                   GLboolean alpha) {
  NoContextHelper("glColorMask");
}

void NoContextGLApi::glCompileShaderFn(GLuint shader) {
  NoContextHelper("glCompileShader");
}

void NoContextGLApi::glCompressedTexImage2DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NoContextHelper("glCompressedTexImage2D");
}

void NoContextGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         GLsizei dataSize,
                                                         const void* data) {
  NoContextHelper("glCompressedTexImage2DRobustANGLE");
}

void NoContextGLApi::glCompressedTexImage3DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NoContextHelper("glCompressedTexImage3D");
}

void NoContextGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         GLsizei dataSize,
                                                         const void* data) {
  NoContextHelper("glCompressedTexImage3DRobustANGLE");
}

void NoContextGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NoContextHelper("glCompressedTexSubImage2D");
}

void NoContextGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLenum format,
                                                            GLsizei imageSize,
                                                            GLsizei dataSize,
                                                            const void* data) {
  NoContextHelper("glCompressedTexSubImage2DRobustANGLE");
}

void NoContextGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NoContextHelper("glCompressedTexSubImage3D");
}

void NoContextGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLint zoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLsizei depth,
                                                            GLenum format,
                                                            GLsizei imageSize,
                                                            GLsizei dataSize,
                                                            const void* data) {
  NoContextHelper("glCompressedTexSubImage3DRobustANGLE");
}

void NoContextGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                           GLenum writeTarget,
                                           GLintptr readOffset,
                                           GLintptr writeOffset,
                                           GLsizeiptr size) {
  NoContextHelper("glCopyBufferSubData");
}

void NoContextGLApi::glCopySubTextureCHROMIUMFn(
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha) {
  NoContextHelper("glCopySubTextureCHROMIUM");
}

void NoContextGLApi::glCopyTexImage2DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border) {
  NoContextHelper("glCopyTexImage2D");
}

void NoContextGLApi::glCopyTexSubImage2DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NoContextHelper("glCopyTexSubImage2D");
}

void NoContextGLApi::glCopyTexSubImage3DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NoContextHelper("glCopyTexSubImage3D");
}

void NoContextGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                             GLint sourceLevel,
                                             GLenum destTarget,
                                             GLuint destId,
                                             GLint destLevel,
                                             GLint internalFormat,
                                             GLenum destType,
                                             GLboolean unpackFlipY,
                                             GLboolean unpackPremultiplyAlpha,
                                             GLboolean unpackUnmultiplyAlpha) {
  NoContextHelper("glCopyTextureCHROMIUM");
}

void NoContextGLApi::glCoverageModulationNVFn(GLenum components) {
  NoContextHelper("glCoverageModulationNV");
}

void NoContextGLApi::glCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glCoverFillPathInstancedNV");
}

void NoContextGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  NoContextHelper("glCoverFillPathNV");
}

void NoContextGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glCoverStrokePathInstancedNV");
}

void NoContextGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  NoContextHelper("glCoverStrokePathNV");
}

GLuint NoContextGLApi::glCreateProgramFn(void) {
  NoContextHelper("glCreateProgram");
  return 0U;
}

GLuint NoContextGLApi::glCreateShaderFn(GLenum type) {
  NoContextHelper("glCreateShader");
  return 0U;
}

GLuint NoContextGLApi::glCreateShaderProgramvFn(GLenum type,
                                                GLsizei count,
                                                const char* const* strings) {
  NoContextHelper("glCreateShaderProgramv");
  return 0U;
}

void NoContextGLApi::glCullFaceFn(GLenum mode) {
  NoContextHelper("glCullFace");
}

void NoContextGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                              const void* userParam) {
  NoContextHelper("glDebugMessageCallback");
}

void NoContextGLApi::glDebugMessageControlFn(GLenum source,
                                             GLenum type,
                                             GLenum severity,
                                             GLsizei count,
                                             const GLuint* ids,
                                             GLboolean enabled) {
  NoContextHelper("glDebugMessageControl");
}

void NoContextGLApi::glDebugMessageInsertFn(GLenum source,
                                            GLenum type,
                                            GLuint id,
                                            GLenum severity,
                                            GLsizei length,
                                            const char* buf) {
  NoContextHelper("glDebugMessageInsert");
}

void NoContextGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  NoContextHelper("glDeleteBuffersARB");
}

void NoContextGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  NoContextHelper("glDeleteFencesAPPLE");
}

void NoContextGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  NoContextHelper("glDeleteFencesNV");
}

void NoContextGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                               const GLuint* framebuffers) {
  NoContextHelper("glDeleteFramebuffersEXT");
}

void NoContextGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  NoContextHelper("glDeletePathsNV");
}

void NoContextGLApi::glDeleteProgramFn(GLuint program) {
  NoContextHelper("glDeleteProgram");
}

void NoContextGLApi::glDeleteProgramPipelinesFn(GLsizei n,
                                                const GLuint* pipelines) {
  NoContextHelper("glDeleteProgramPipelines");
}

void NoContextGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  NoContextHelper("glDeleteQueries");
}

void NoContextGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                                const GLuint* renderbuffers) {
  NoContextHelper("glDeleteRenderbuffersEXT");
}

void NoContextGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  NoContextHelper("glDeleteSamplers");
}

void NoContextGLApi::glDeleteShaderFn(GLuint shader) {
  NoContextHelper("glDeleteShader");
}

void NoContextGLApi::glDeleteSyncFn(GLsync sync) {
  NoContextHelper("glDeleteSync");
}

void NoContextGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  NoContextHelper("glDeleteTextures");
}

void NoContextGLApi::glDeleteTransformFeedbacksFn(GLsizei n,
                                                  const GLuint* ids) {
  NoContextHelper("glDeleteTransformFeedbacks");
}

void NoContextGLApi::glDeleteVertexArraysOESFn(GLsizei n,
                                               const GLuint* arrays) {
  NoContextHelper("glDeleteVertexArraysOES");
}

void NoContextGLApi::glDepthFuncFn(GLenum func) {
  NoContextHelper("glDepthFunc");
}

void NoContextGLApi::glDepthMaskFn(GLboolean flag) {
  NoContextHelper("glDepthMask");
}

void NoContextGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  NoContextHelper("glDepthRange");
}

void NoContextGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  NoContextHelper("glDepthRangef");
}

void NoContextGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  NoContextHelper("glDetachShader");
}

void NoContextGLApi::glDisableFn(GLenum cap) {
  NoContextHelper("glDisable");
}

void NoContextGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  NoContextHelper("glDisableVertexAttribArray");
}

void NoContextGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NoContextHelper("glDiscardFramebufferEXT");
}

void NoContextGLApi::glDispatchComputeFn(GLuint numGroupsX,
                                         GLuint numGroupsY,
                                         GLuint numGroupsZ) {
  NoContextHelper("glDispatchCompute");
}

void NoContextGLApi::glDispatchComputeIndirectFn(GLintptr indirect) {
  NoContextHelper("glDispatchComputeIndirect");
}

void NoContextGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  NoContextHelper("glDrawArrays");
}

void NoContextGLApi::glDrawArraysIndirectFn(GLenum mode, const void* indirect) {
  NoContextHelper("glDrawArraysIndirect");
}

void NoContextGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                                  GLint first,
                                                  GLsizei count,
                                                  GLsizei primcount) {
  NoContextHelper("glDrawArraysInstancedANGLE");
}

void NoContextGLApi::glDrawBufferFn(GLenum mode) {
  NoContextHelper("glDrawBuffer");
}

void NoContextGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  NoContextHelper("glDrawBuffersARB");
}

void NoContextGLApi::glDrawElementsFn(GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  NoContextHelper("glDrawElements");
}

void NoContextGLApi::glDrawElementsIndirectFn(GLenum mode,
                                              GLenum type,
                                              const void* indirect) {
  NoContextHelper("glDrawElementsIndirect");
}

void NoContextGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                    GLsizei count,
                                                    GLenum type,
                                                    const void* indices,
                                                    GLsizei primcount) {
  NoContextHelper("glDrawElementsInstancedANGLE");
}

void NoContextGLApi::glDrawRangeElementsFn(GLenum mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           GLenum type,
                                           const void* indices) {
  NoContextHelper("glDrawRangeElements");
}

void NoContextGLApi::glEGLImageTargetRenderbufferStorageOESFn(
    GLenum target,
    GLeglImageOES image) {
  NoContextHelper("glEGLImageTargetRenderbufferStorageOES");
}

void NoContextGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                    GLeglImageOES image) {
  NoContextHelper("glEGLImageTargetTexture2DOES");
}

void NoContextGLApi::glEnableFn(GLenum cap) {
  NoContextHelper("glEnable");
}

void NoContextGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  NoContextHelper("glEnableVertexAttribArray");
}

void NoContextGLApi::glEndQueryFn(GLenum target) {
  NoContextHelper("glEndQuery");
}

void NoContextGLApi::glEndTransformFeedbackFn(void) {
  NoContextHelper("glEndTransformFeedback");
}

GLsync NoContextGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  NoContextHelper("glFenceSync");
  return NULL;
}

void NoContextGLApi::glFinishFn(void) {
  NoContextHelper("glFinish");
}

void NoContextGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glFinishFenceAPPLE");
}

void NoContextGLApi::glFinishFenceNVFn(GLuint fence) {
  NoContextHelper("glFinishFenceNV");
}

void NoContextGLApi::glFlushFn(void) {
  NoContextHelper("glFlush");
}

void NoContextGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                                GLintptr offset,
                                                GLsizeiptr length) {
  NoContextHelper("glFlushMappedBufferRange");
}

void NoContextGLApi::glFramebufferParameteriFn(GLenum target,
                                               GLenum pname,
                                               GLint param) {
  NoContextHelper("glFramebufferParameteri");
}

void NoContextGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                    GLenum attachment,
                                                    GLenum renderbuffertarget,
                                                    GLuint renderbuffer) {
  NoContextHelper("glFramebufferRenderbufferEXT");
}

void NoContextGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                                 GLenum attachment,
                                                 GLenum textarget,
                                                 GLuint texture,
                                                 GLint level) {
  NoContextHelper("glFramebufferTexture2DEXT");
}

void NoContextGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum textarget,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLsizei samples) {
  NoContextHelper("glFramebufferTexture2DMultisampleEXT");
}

void NoContextGLApi::glFramebufferTextureLayerFn(GLenum target,
                                                 GLenum attachment,
                                                 GLuint texture,
                                                 GLint level,
                                                 GLint layer) {
  NoContextHelper("glFramebufferTextureLayer");
}

void NoContextGLApi::glFramebufferTextureMultiviewLayeredANGLEFn(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews) {
  NoContextHelper("glFramebufferTextureMultiviewLayeredANGLE");
}

void NoContextGLApi::glFrontFaceFn(GLenum mode) {
  NoContextHelper("glFrontFace");
}

void NoContextGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  NoContextHelper("glGenBuffersARB");
}

void NoContextGLApi::glGenerateMipmapEXTFn(GLenum target) {
  NoContextHelper("glGenerateMipmapEXT");
}

void NoContextGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  NoContextHelper("glGenFencesAPPLE");
}

void NoContextGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  NoContextHelper("glGenFencesNV");
}

void NoContextGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  NoContextHelper("glGenFramebuffersEXT");
}

GLuint NoContextGLApi::glGenPathsNVFn(GLsizei range) {
  NoContextHelper("glGenPathsNV");
  return 0U;
}

GLuint NoContextGLApi::glGenProgramPipelinesFn(GLsizei n, GLuint* pipelines) {
  NoContextHelper("glGenProgramPipelines");
  return 0U;
}

void NoContextGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  NoContextHelper("glGenQueries");
}

void NoContextGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  NoContextHelper("glGenRenderbuffersEXT");
}

void NoContextGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  NoContextHelper("glGenSamplers");
}

void NoContextGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  NoContextHelper("glGenTextures");
}

void NoContextGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  NoContextHelper("glGenTransformFeedbacks");
}

void NoContextGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  NoContextHelper("glGenVertexArraysOES");
}

void NoContextGLApi::glGetActiveAttribFn(GLuint program,
                                         GLuint index,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         GLint* size,
                                         GLenum* type,
                                         char* name) {
  NoContextHelper("glGetActiveAttrib");
}

void NoContextGLApi::glGetActiveUniformFn(GLuint program,
                                          GLuint index,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* size,
                                          GLenum* type,
                                          char* name) {
  NoContextHelper("glGetActiveUniform");
}

void NoContextGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                                 GLuint uniformBlockIndex,
                                                 GLenum pname,
                                                 GLint* params) {
  NoContextHelper("glGetActiveUniformBlockiv");
}

void NoContextGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetActiveUniformBlockivRobustANGLE");
}

void NoContextGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                                   GLuint uniformBlockIndex,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   char* uniformBlockName) {
  NoContextHelper("glGetActiveUniformBlockName");
}

void NoContextGLApi::glGetActiveUniformsivFn(GLuint program,
                                             GLsizei uniformCount,
                                             const GLuint* uniformIndices,
                                             GLenum pname,
                                             GLint* params) {
  NoContextHelper("glGetActiveUniformsiv");
}

void NoContextGLApi::glGetAttachedShadersFn(GLuint program,
                                            GLsizei maxcount,
                                            GLsizei* count,
                                            GLuint* shaders) {
  NoContextHelper("glGetAttachedShaders");
}

GLint NoContextGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  NoContextHelper("glGetAttribLocation");
  return 0;
}

void NoContextGLApi::glGetBooleani_vFn(GLenum target,
                                       GLuint index,
                                       GLboolean* data) {
  NoContextHelper("glGetBooleani_v");
}

void NoContextGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLboolean* data) {
  NoContextHelper("glGetBooleani_vRobustANGLE");
}

void NoContextGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  NoContextHelper("glGetBooleanv");
}

void NoContextGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLboolean* data) {
  NoContextHelper("glGetBooleanvRobustANGLE");
}

void NoContextGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint64* params) {
  NoContextHelper("glGetBufferParameteri64vRobustANGLE");
}

void NoContextGLApi::glGetBufferParameterivFn(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  NoContextHelper("glGetBufferParameteriv");
}

void NoContextGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  NoContextHelper("glGetBufferParameterivRobustANGLE");
}

void NoContextGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  NoContextHelper("glGetBufferPointervRobustANGLE");
}

GLuint NoContextGLApi::glGetDebugMessageLogFn(GLuint count,
                                              GLsizei bufSize,
                                              GLenum* sources,
                                              GLenum* types,
                                              GLuint* ids,
                                              GLenum* severities,
                                              GLsizei* lengths,
                                              char* messageLog) {
  NoContextHelper("glGetDebugMessageLog");
  return 0U;
}

GLenum NoContextGLApi::glGetErrorFn(void) {
  NoContextHelper("glGetError");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetFenceivNVFn(GLuint fence,
                                      GLenum pname,
                                      GLint* params) {
  NoContextHelper("glGetFenceivNV");
}

void NoContextGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  NoContextHelper("glGetFloatv");
}

void NoContextGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* data) {
  NoContextHelper("glGetFloatvRobustANGLE");
}

GLint NoContextGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  NoContextHelper("glGetFragDataIndex");
  return 0;
}

GLint NoContextGLApi::glGetFragDataLocationFn(GLuint program,
                                              const char* name) {
  NoContextHelper("glGetFragDataLocation");
  return 0;
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivEXTFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params) {
  NoContextHelper("glGetFramebufferAttachmentParameterivEXT");
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetFramebufferAttachmentParameterivRobustANGLE");
}

void NoContextGLApi::glGetFramebufferParameterivFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  NoContextHelper("glGetFramebufferParameteriv");
}

void NoContextGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* params) {
  NoContextHelper("glGetFramebufferParameterivRobustANGLE");
}

GLenum NoContextGLApi::glGetGraphicsResetStatusARBFn(void) {
  NoContextHelper("glGetGraphicsResetStatusARB");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetInteger64i_vFn(GLenum target,
                                         GLuint index,
                                         GLint64* data) {
  NoContextHelper("glGetInteger64i_v");
}

void NoContextGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint64* data) {
  NoContextHelper("glGetInteger64i_vRobustANGLE");
}

void NoContextGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  NoContextHelper("glGetInteger64v");
}

void NoContextGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* data) {
  NoContextHelper("glGetInteger64vRobustANGLE");
}

void NoContextGLApi::glGetIntegeri_vFn(GLenum target,
                                       GLuint index,
                                       GLint* data) {
  NoContextHelper("glGetIntegeri_v");
}

void NoContextGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* data) {
  NoContextHelper("glGetIntegeri_vRobustANGLE");
}

void NoContextGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  NoContextHelper("glGetIntegerv");
}

void NoContextGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* data) {
  NoContextHelper("glGetIntegervRobustANGLE");
}

void NoContextGLApi::glGetInternalformativFn(GLenum target,
                                             GLenum internalformat,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLint* params) {
  NoContextHelper("glGetInternalformativ");
}

void NoContextGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                        GLenum internalformat,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) {
  NoContextHelper("glGetInternalformativRobustANGLE");
}

void NoContextGLApi::glGetMultisamplefvFn(GLenum pname,
                                          GLuint index,
                                          GLfloat* val) {
  NoContextHelper("glGetMultisamplefv");
}

void NoContextGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* val) {
  NoContextHelper("glGetMultisamplefvRobustANGLE");
}

void NoContextGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  NoContextHelper("glGetnUniformfvRobustANGLE");
}

void NoContextGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  NoContextHelper("glGetnUniformivRobustANGLE");
}

void NoContextGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                                   GLint location,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  NoContextHelper("glGetnUniformuivRobustANGLE");
}

void NoContextGLApi::glGetObjectLabelFn(GLenum identifier,
                                        GLuint name,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        char* label) {
  NoContextHelper("glGetObjectLabel");
}

void NoContextGLApi::glGetObjectPtrLabelFn(void* ptr,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           char* label) {
  NoContextHelper("glGetObjectPtrLabel");
}

void NoContextGLApi::glGetPointervFn(GLenum pname, void** params) {
  NoContextHelper("glGetPointerv");
}

void NoContextGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           void** params) {
  NoContextHelper("glGetPointervRobustANGLERobustANGLE");
}

void NoContextGLApi::glGetProgramBinaryFn(GLuint program,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLenum* binaryFormat,
                                          GLvoid* binary) {
  NoContextHelper("glGetProgramBinary");
}

void NoContextGLApi::glGetProgramInfoLogFn(GLuint program,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           char* infolog) {
  NoContextHelper("glGetProgramInfoLog");
}

void NoContextGLApi::glGetProgramInterfaceivFn(GLuint program,
                                               GLenum programInterface,
                                               GLenum pname,
                                               GLint* params) {
  NoContextHelper("glGetProgramInterfaceiv");
}

void NoContextGLApi::glGetProgramInterfaceivRobustANGLEFn(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetProgramInterfaceivRobustANGLE");
}

void NoContextGLApi::glGetProgramivFn(GLuint program,
                                      GLenum pname,
                                      GLint* params) {
  NoContextHelper("glGetProgramiv");
}

void NoContextGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NoContextHelper("glGetProgramivRobustANGLE");
}

void NoContextGLApi::glGetProgramPipelineInfoLogFn(GLuint pipeline,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLchar* infoLog) {
  NoContextHelper("glGetProgramPipelineInfoLog");
}

void NoContextGLApi::glGetProgramPipelineivFn(GLuint pipeline,
                                              GLenum pname,
                                              GLint* params) {
  NoContextHelper("glGetProgramPipelineiv");
}

void NoContextGLApi::glGetProgramResourceIndexFn(GLuint program,
                                                 GLenum programInterface,
                                                 const GLchar* name) {
  NoContextHelper("glGetProgramResourceIndex");
}

void NoContextGLApi::glGetProgramResourceivFn(GLuint program,
                                              GLenum programInterface,
                                              GLuint index,
                                              GLsizei propCount,
                                              const GLenum* props,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  NoContextHelper("glGetProgramResourceiv");
}

GLint NoContextGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                     GLenum programInterface,
                                                     const char* name) {
  NoContextHelper("glGetProgramResourceLocation");
  return 0;
}

void NoContextGLApi::glGetProgramResourceNameFn(GLuint program,
                                                GLenum programInterface,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLchar* name) {
  NoContextHelper("glGetProgramResourceName");
}

void NoContextGLApi::glGetQueryivFn(GLenum target,
                                    GLenum pname,
                                    GLint* params) {
  NoContextHelper("glGetQueryiv");
}

void NoContextGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) {
  NoContextHelper("glGetQueryivRobustANGLE");
}

void NoContextGLApi::glGetQueryObjecti64vFn(GLuint id,
                                            GLenum pname,
                                            GLint64* params) {
  NoContextHelper("glGetQueryObjecti64v");
}

void NoContextGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  NoContextHelper("glGetQueryObjecti64vRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectivFn(GLuint id,
                                          GLenum pname,
                                          GLint* params) {
  NoContextHelper("glGetQueryObjectiv");
}

void NoContextGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  NoContextHelper("glGetQueryObjectivRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectui64vFn(GLuint id,
                                             GLenum pname,
                                             GLuint64* params) {
  NoContextHelper("glGetQueryObjectui64v");
}

void NoContextGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint64* params) {
  NoContextHelper("glGetQueryObjectui64vRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectuivFn(GLuint id,
                                           GLenum pname,
                                           GLuint* params) {
  NoContextHelper("glGetQueryObjectuiv");
}

void NoContextGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLuint* params) {
  NoContextHelper("glGetQueryObjectuivRobustANGLE");
}

void NoContextGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                       GLenum pname,
                                                       GLint* params) {
  NoContextHelper("glGetRenderbufferParameterivEXT");
}

void NoContextGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLint* params) {
  NoContextHelper("glGetRenderbufferParameterivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                               GLenum pname,
                                               GLfloat* params) {
  NoContextHelper("glGetSamplerParameterfv");
}

void NoContextGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* params) {
  NoContextHelper("glGetSamplerParameterfvRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NoContextHelper("glGetSamplerParameterIivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLuint* params) {
  NoContextHelper("glGetSamplerParameterIuivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                               GLenum pname,
                                               GLint* params) {
  NoContextHelper("glGetSamplerParameteriv");
}

void NoContextGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  NoContextHelper("glGetSamplerParameterivRobustANGLE");
}

void NoContextGLApi::glGetShaderInfoLogFn(GLuint shader,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          char* infolog) {
  NoContextHelper("glGetShaderInfoLog");
}

void NoContextGLApi::glGetShaderivFn(GLuint shader,
                                     GLenum pname,
                                     GLint* params) {
  NoContextHelper("glGetShaderiv");
}

void NoContextGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  NoContextHelper("glGetShaderivRobustANGLE");
}

void NoContextGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                                  GLenum precisiontype,
                                                  GLint* range,
                                                  GLint* precision) {
  NoContextHelper("glGetShaderPrecisionFormat");
}

void NoContextGLApi::glGetShaderSourceFn(GLuint shader,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         char* source) {
  NoContextHelper("glGetShaderSource");
}

const GLubyte* NoContextGLApi::glGetStringFn(GLenum name) {
  NoContextHelper("glGetString");
  return NULL;
}

const GLubyte* NoContextGLApi::glGetStringiFn(GLenum name, GLuint index) {
  NoContextHelper("glGetStringi");
  return NULL;
}

void NoContextGLApi::glGetSyncivFn(GLsync sync,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLint* values) {
  NoContextHelper("glGetSynciv");
}

void NoContextGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLfloat* params) {
  NoContextHelper("glGetTexLevelParameterfv");
}

void NoContextGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLfloat* params) {
  NoContextHelper("glGetTexLevelParameterfvRobustANGLE");
}

void NoContextGLApi::glGetTexLevelParameterivFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLint* params) {
  NoContextHelper("glGetTexLevelParameteriv");
}

void NoContextGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NoContextHelper("glGetTexLevelParameterivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterfvFn(GLenum target,
                                           GLenum pname,
                                           GLfloat* params) {
  NoContextHelper("glGetTexParameterfv");
}

void NoContextGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NoContextHelper("glGetTexParameterfvRobustANGLE");
}

void NoContextGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NoContextHelper("glGetTexParameterIivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NoContextHelper("glGetTexParameterIuivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterivFn(GLenum target,
                                           GLenum pname,
                                           GLint* params) {
  NoContextHelper("glGetTexParameteriv");
}

void NoContextGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NoContextHelper("glGetTexParameterivRobustANGLE");
}

void NoContextGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLsizei* size,
                                                     GLenum* type,
                                                     char* name) {
  NoContextHelper("glGetTransformFeedbackVarying");
}

void NoContextGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        char* source) {
  NoContextHelper("glGetTranslatedShaderSourceANGLE");
}

GLuint NoContextGLApi::glGetUniformBlockIndexFn(GLuint program,
                                                const char* uniformBlockName) {
  NoContextHelper("glGetUniformBlockIndex");
  return 0U;
}

void NoContextGLApi::glGetUniformfvFn(GLuint program,
                                      GLint location,
                                      GLfloat* params) {
  NoContextHelper("glGetUniformfv");
}

void NoContextGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  NoContextHelper("glGetUniformfvRobustANGLE");
}

void NoContextGLApi::glGetUniformIndicesFn(GLuint program,
                                           GLsizei uniformCount,
                                           const char* const* uniformNames,
                                           GLuint* uniformIndices) {
  NoContextHelper("glGetUniformIndices");
}

void NoContextGLApi::glGetUniformivFn(GLuint program,
                                      GLint location,
                                      GLint* params) {
  NoContextHelper("glGetUniformiv");
}

void NoContextGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NoContextHelper("glGetUniformivRobustANGLE");
}

GLint NoContextGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  NoContextHelper("glGetUniformLocation");
  return 0;
}

void NoContextGLApi::glGetUniformuivFn(GLuint program,
                                       GLint location,
                                       GLuint* params) {
  NoContextHelper("glGetUniformuiv");
}

void NoContextGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  NoContextHelper("glGetUniformuivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribfvFn(GLuint index,
                                           GLenum pname,
                                           GLfloat* params) {
  NoContextHelper("glGetVertexAttribfv");
}

void NoContextGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NoContextHelper("glGetVertexAttribfvRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NoContextHelper("glGetVertexAttribIivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NoContextHelper("glGetVertexAttribIuivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribivFn(GLuint index,
                                           GLenum pname,
                                           GLint* params) {
  NoContextHelper("glGetVertexAttribiv");
}

void NoContextGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NoContextHelper("glGetVertexAttribivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribPointervFn(GLuint index,
                                                 GLenum pname,
                                                 void** pointer) {
  NoContextHelper("glGetVertexAttribPointerv");
}

void NoContextGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            void** pointer) {
  NoContextHelper("glGetVertexAttribPointervRobustANGLE");
}

void NoContextGLApi::glHintFn(GLenum target, GLenum mode) {
  NoContextHelper("glHint");
}

void NoContextGLApi::glInsertEventMarkerEXTFn(GLsizei length,
                                              const char* marker) {
  NoContextHelper("glInsertEventMarkerEXT");
}

void NoContextGLApi::glInvalidateFramebufferFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NoContextHelper("glInvalidateFramebuffer");
}

void NoContextGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum* attachments,
                                                  GLint x,
                                                  GLint y,
                                                  GLint width,
                                                  GLint height) {
  NoContextHelper("glInvalidateSubFramebuffer");
}

GLboolean NoContextGLApi::glIsBufferFn(GLuint buffer) {
  NoContextHelper("glIsBuffer");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsEnabledFn(GLenum cap) {
  NoContextHelper("glIsEnabled");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glIsFenceAPPLE");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceNVFn(GLuint fence) {
  NoContextHelper("glIsFenceNV");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  NoContextHelper("glIsFramebufferEXT");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsPathNVFn(GLuint path) {
  NoContextHelper("glIsPathNV");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsProgramFn(GLuint program) {
  NoContextHelper("glIsProgram");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsProgramPipelineFn(GLuint pipeline) {
  NoContextHelper("glIsProgramPipeline");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsQueryFn(GLuint query) {
  NoContextHelper("glIsQuery");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  NoContextHelper("glIsRenderbufferEXT");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSamplerFn(GLuint sampler) {
  NoContextHelper("glIsSampler");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsShaderFn(GLuint shader) {
  NoContextHelper("glIsShader");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSyncFn(GLsync sync) {
  NoContextHelper("glIsSync");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTextureFn(GLuint texture) {
  NoContextHelper("glIsTexture");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTransformFeedbackFn(GLuint id) {
  NoContextHelper("glIsTransformFeedback");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsVertexArrayOESFn(GLuint array) {
  NoContextHelper("glIsVertexArrayOES");
  return GL_FALSE;
}

void NoContextGLApi::glLineWidthFn(GLfloat width) {
  NoContextHelper("glLineWidth");
}

void NoContextGLApi::glLinkProgramFn(GLuint program) {
  NoContextHelper("glLinkProgram");
}

void* NoContextGLApi::glMapBufferFn(GLenum target, GLenum access) {
  NoContextHelper("glMapBuffer");
  return NULL;
}

void* NoContextGLApi::glMapBufferRangeFn(GLenum target,
                                         GLintptr offset,
                                         GLsizeiptr length,
                                         GLbitfield access) {
  NoContextHelper("glMapBufferRange");
  return NULL;
}

void NoContextGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  NoContextHelper("glMatrixLoadfEXT");
}

void NoContextGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  NoContextHelper("glMatrixLoadIdentityEXT");
}

void NoContextGLApi::glMaxShaderCompilerThreadsKHRFn(GLuint count) {
  NoContextHelper("glMaxShaderCompilerThreadsKHR");
}

void NoContextGLApi::glMemoryBarrierByRegionFn(GLbitfield barriers) {
  NoContextHelper("glMemoryBarrierByRegion");
}

void NoContextGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  NoContextHelper("glMemoryBarrierEXT");
}

void NoContextGLApi::glMinSampleShadingFn(GLfloat value) {
  NoContextHelper("glMinSampleShading");
}

void NoContextGLApi::glObjectLabelFn(GLenum identifier,
                                     GLuint name,
                                     GLsizei length,
                                     const char* label) {
  NoContextHelper("glObjectLabel");
}

void NoContextGLApi::glObjectPtrLabelFn(void* ptr,
                                        GLsizei length,
                                        const char* label) {
  NoContextHelper("glObjectPtrLabel");
}

void NoContextGLApi::glPathCommandsNVFn(GLuint path,
                                        GLsizei numCommands,
                                        const GLubyte* commands,
                                        GLsizei numCoords,
                                        GLenum coordType,
                                        const GLvoid* coords) {
  NoContextHelper("glPathCommandsNV");
}

void NoContextGLApi::glPathParameterfNVFn(GLuint path,
                                          GLenum pname,
                                          GLfloat value) {
  NoContextHelper("glPathParameterfNV");
}

void NoContextGLApi::glPathParameteriNVFn(GLuint path,
                                          GLenum pname,
                                          GLint value) {
  NoContextHelper("glPathParameteriNV");
}

void NoContextGLApi::glPathStencilFuncNVFn(GLenum func,
                                           GLint ref,
                                           GLuint mask) {
  NoContextHelper("glPathStencilFuncNV");
}

void NoContextGLApi::glPauseTransformFeedbackFn(void) {
  NoContextHelper("glPauseTransformFeedback");
}

void NoContextGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  NoContextHelper("glPixelStorei");
}

void NoContextGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  NoContextHelper("glPointParameteri");
}

void NoContextGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  NoContextHelper("glPolygonMode");
}

void NoContextGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  NoContextHelper("glPolygonOffset");
}

void NoContextGLApi::glPopDebugGroupFn() {
  NoContextHelper("glPopDebugGroup");
}

void NoContextGLApi::glPopGroupMarkerEXTFn(void) {
  NoContextHelper("glPopGroupMarkerEXT");
}

void NoContextGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  NoContextHelper("glPrimitiveRestartIndex");
}

void NoContextGLApi::glProgramBinaryFn(GLuint program,
                                       GLenum binaryFormat,
                                       const GLvoid* binary,
                                       GLsizei length) {
  NoContextHelper("glProgramBinary");
}

void NoContextGLApi::glProgramParameteriFn(GLuint program,
                                           GLenum pname,
                                           GLint value) {
  NoContextHelper("glProgramParameteri");
}

void NoContextGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                       GLint location,
                                                       GLenum genMode,
                                                       GLint components,
                                                       const GLfloat* coeffs) {
  NoContextHelper("glProgramPathFragmentInputGenNV");
}

void NoContextGLApi::glProgramUniform1fFn(GLuint program,
                                          GLint location,
                                          GLfloat v0) {
  NoContextHelper("glProgramUniform1f");
}

void NoContextGLApi::glProgramUniform1fvFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLfloat* value) {
  NoContextHelper("glProgramUniform1fv");
}

void NoContextGLApi::glProgramUniform1iFn(GLuint program,
                                          GLint location,
                                          GLint v0) {
  NoContextHelper("glProgramUniform1i");
}

void NoContextGLApi::glProgramUniform1ivFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLint* value) {
  NoContextHelper("glProgramUniform1iv");
}

void NoContextGLApi::glProgramUniform1uiFn(GLuint program,
                                           GLint location,
                                           GLuint v0) {
  NoContextHelper("glProgramUniform1ui");
}

void NoContextGLApi::glProgramUniform1uivFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            const GLuint* value) {
  NoContextHelper("glProgramUniform1uiv");
}

void NoContextGLApi::glProgramUniform2fFn(GLuint program,
                                          GLint location,
                                          GLfloat v0,
                                          GLfloat v1) {
  NoContextHelper("glProgramUniform2f");
}

void NoContextGLApi::glProgramUniform2fvFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLfloat* value) {
  NoContextHelper("glProgramUniform2fv");
}

void NoContextGLApi::glProgramUniform2iFn(GLuint program,
                                          GLint location,
                                          GLint v0,
                                          GLint v1) {
  NoContextHelper("glProgramUniform2i");
}

void NoContextGLApi::glProgramUniform2ivFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLint* value) {
  NoContextHelper("glProgramUniform2iv");
}

void NoContextGLApi::glProgramUniform2uiFn(GLuint program,
                                           GLint location,
                                           GLuint v0,
                                           GLuint v1) {
  NoContextHelper("glProgramUniform2ui");
}

void NoContextGLApi::glProgramUniform2uivFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            const GLuint* value) {
  NoContextHelper("glProgramUniform2uiv");
}

void NoContextGLApi::glProgramUniform3fFn(GLuint program,
                                          GLint location,
                                          GLfloat v0,
                                          GLfloat v1,
                                          GLfloat v2) {
  NoContextHelper("glProgramUniform3f");
}

void NoContextGLApi::glProgramUniform3fvFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLfloat* value) {
  NoContextHelper("glProgramUniform3fv");
}

void NoContextGLApi::glProgramUniform3iFn(GLuint program,
                                          GLint location,
                                          GLint v0,
                                          GLint v1,
                                          GLint v2) {
  NoContextHelper("glProgramUniform3i");
}

void NoContextGLApi::glProgramUniform3ivFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLint* value) {
  NoContextHelper("glProgramUniform3iv");
}

void NoContextGLApi::glProgramUniform3uiFn(GLuint program,
                                           GLint location,
                                           GLuint v0,
                                           GLuint v1,
                                           GLuint v2) {
  NoContextHelper("glProgramUniform3ui");
}

void NoContextGLApi::glProgramUniform3uivFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            const GLuint* value) {
  NoContextHelper("glProgramUniform3uiv");
}

void NoContextGLApi::glProgramUniform4fFn(GLuint program,
                                          GLint location,
                                          GLfloat v0,
                                          GLfloat v1,
                                          GLfloat v2,
                                          GLfloat v3) {
  NoContextHelper("glProgramUniform4f");
}

void NoContextGLApi::glProgramUniform4fvFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLfloat* value) {
  NoContextHelper("glProgramUniform4fv");
}

void NoContextGLApi::glProgramUniform4iFn(GLuint program,
                                          GLint location,
                                          GLint v0,
                                          GLint v1,
                                          GLint v2,
                                          GLint v3) {
  NoContextHelper("glProgramUniform4i");
}

void NoContextGLApi::glProgramUniform4ivFn(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           const GLint* value) {
  NoContextHelper("glProgramUniform4iv");
}

void NoContextGLApi::glProgramUniform4uiFn(GLuint program,
                                           GLint location,
                                           GLuint v0,
                                           GLuint v1,
                                           GLuint v2,
                                           GLuint v3) {
  NoContextHelper("glProgramUniform4ui");
}

void NoContextGLApi::glProgramUniform4uivFn(GLuint program,
                                            GLint location,
                                            GLsizei count,
                                            const GLuint* value) {
  NoContextHelper("glProgramUniform4uiv");
}

void NoContextGLApi::glProgramUniformMatrix2fvFn(GLuint program,
                                                 GLint location,
                                                 GLsizei count,
                                                 GLboolean transpose,
                                                 const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix2fv");
}

void NoContextGLApi::glProgramUniformMatrix2x3fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix2x3fv");
}

void NoContextGLApi::glProgramUniformMatrix2x4fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix2x4fv");
}

void NoContextGLApi::glProgramUniformMatrix3fvFn(GLuint program,
                                                 GLint location,
                                                 GLsizei count,
                                                 GLboolean transpose,
                                                 const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix3fv");
}

void NoContextGLApi::glProgramUniformMatrix3x2fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix3x2fv");
}

void NoContextGLApi::glProgramUniformMatrix3x4fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix3x4fv");
}

void NoContextGLApi::glProgramUniformMatrix4fvFn(GLuint program,
                                                 GLint location,
                                                 GLsizei count,
                                                 GLboolean transpose,
                                                 const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix4fv");
}

void NoContextGLApi::glProgramUniformMatrix4x2fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix4x2fv");
}

void NoContextGLApi::glProgramUniformMatrix4x3fvFn(GLuint program,
                                                   GLint location,
                                                   GLsizei count,
                                                   GLboolean transpose,
                                                   const GLfloat* value) {
  NoContextHelper("glProgramUniformMatrix4x3fv");
}

void NoContextGLApi::glPushDebugGroupFn(GLenum source,
                                        GLuint id,
                                        GLsizei length,
                                        const char* message) {
  NoContextHelper("glPushDebugGroup");
}

void NoContextGLApi::glPushGroupMarkerEXTFn(GLsizei length,
                                            const char* marker) {
  NoContextHelper("glPushGroupMarkerEXT");
}

void NoContextGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  NoContextHelper("glQueryCounter");
}

void NoContextGLApi::glReadBufferFn(GLenum src) {
  NoContextHelper("glReadBuffer");
}

void NoContextGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLsizei* columns,
                                                GLsizei* rows,
                                                void* data) {
  NoContextHelper("glReadnPixelsRobustANGLE");
}

void NoContextGLApi::glReadPixelsFn(GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    void* pixels) {
  NoContextHelper("glReadPixels");
}

void NoContextGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* columns,
                                               GLsizei* rows,
                                               void* pixels) {
  NoContextHelper("glReadPixelsRobustANGLE");
}

void NoContextGLApi::glReleaseShaderCompilerFn(void) {
  NoContextHelper("glReleaseShaderCompiler");
}

void NoContextGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height) {
  NoContextHelper("glRenderbufferStorageEXT");
}

void NoContextGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height) {
  NoContextHelper("glRenderbufferStorageMultisample");
}

void NoContextGLApi::glRenderbufferStorageMultisampleEXTFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NoContextHelper("glRenderbufferStorageMultisampleEXT");
}

void NoContextGLApi::glRequestExtensionANGLEFn(const char* name) {
  NoContextHelper("glRequestExtensionANGLE");
}

void NoContextGLApi::glResumeTransformFeedbackFn(void) {
  NoContextHelper("glResumeTransformFeedback");
}

void NoContextGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  NoContextHelper("glSampleCoverage");
}

void NoContextGLApi::glSampleMaskiFn(GLuint maskNumber, GLbitfield mask) {
  NoContextHelper("glSampleMaski");
}

void NoContextGLApi::glSamplerParameterfFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat param) {
  NoContextHelper("glSamplerParameterf");
}

void NoContextGLApi::glSamplerParameterfvFn(GLuint sampler,
                                            GLenum pname,
                                            const GLfloat* params) {
  NoContextHelper("glSamplerParameterfv");
}

void NoContextGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLfloat* param) {
  NoContextHelper("glSamplerParameterfvRobustANGLE");
}

void NoContextGLApi::glSamplerParameteriFn(GLuint sampler,
                                           GLenum pname,
                                           GLint param) {
  NoContextHelper("glSamplerParameteri");
}

void NoContextGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint* param) {
  NoContextHelper("glSamplerParameterIivRobustANGLE");
}

void NoContextGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLuint* param) {
  NoContextHelper("glSamplerParameterIuivRobustANGLE");
}

void NoContextGLApi::glSamplerParameterivFn(GLuint sampler,
                                            GLenum pname,
                                            const GLint* params) {
  NoContextHelper("glSamplerParameteriv");
}

void NoContextGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLint* param) {
  NoContextHelper("glSamplerParameterivRobustANGLE");
}

void NoContextGLApi::glScissorFn(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  NoContextHelper("glScissor");
}

void NoContextGLApi::glSetFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glSetFenceAPPLE");
}

void NoContextGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  NoContextHelper("glSetFenceNV");
}

void NoContextGLApi::glShaderBinaryFn(GLsizei n,
                                      const GLuint* shaders,
                                      GLenum binaryformat,
                                      const void* binary,
                                      GLsizei length) {
  NoContextHelper("glShaderBinary");
}

void NoContextGLApi::glShaderSourceFn(GLuint shader,
                                      GLsizei count,
                                      const char* const* str,
                                      const GLint* length) {
  NoContextHelper("glShaderSource");
}

void NoContextGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilFillPathInstancedNV");
}

void NoContextGLApi::glStencilFillPathNVFn(GLuint path,
                                           GLenum fillMode,
                                           GLuint mask) {
  NoContextHelper("glStencilFillPathNV");
}

void NoContextGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  NoContextHelper("glStencilFunc");
}

void NoContextGLApi::glStencilFuncSeparateFn(GLenum face,
                                             GLenum func,
                                             GLint ref,
                                             GLuint mask) {
  NoContextHelper("glStencilFuncSeparate");
}

void NoContextGLApi::glStencilMaskFn(GLuint mask) {
  NoContextHelper("glStencilMask");
}

void NoContextGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  NoContextHelper("glStencilMaskSeparate");
}

void NoContextGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  NoContextHelper("glStencilOp");
}

void NoContextGLApi::glStencilOpSeparateFn(GLenum face,
                                           GLenum fail,
                                           GLenum zfail,
                                           GLenum zpass) {
  NoContextHelper("glStencilOpSeparate");
}

void NoContextGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilStrokePathInstancedNV");
}

void NoContextGLApi::glStencilStrokePathNVFn(GLuint path,
                                             GLint reference,
                                             GLuint mask) {
  NoContextHelper("glStencilStrokePathNV");
}

void NoContextGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilThenCoverFillPathInstancedNV");
}

void NoContextGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum coverMode) {
  NoContextHelper("glStencilThenCoverFillPathNV");
}

void NoContextGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilThenCoverStrokePathInstancedNV");
}

void NoContextGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum coverMode) {
  NoContextHelper("glStencilThenCoverStrokePathNV");
}

GLboolean NoContextGLApi::glTestFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glTestFenceAPPLE");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glTestFenceNVFn(GLuint fence) {
  NoContextHelper("glTestFenceNV");
  return GL_FALSE;
}

void NoContextGLApi::glTexBufferFn(GLenum target,
                                   GLenum internalformat,
                                   GLuint buffer) {
  NoContextHelper("glTexBuffer");
}

void NoContextGLApi::glTexBufferRangeFn(GLenum target,
                                        GLenum internalformat,
                                        GLuint buffer,
                                        GLintptr offset,
                                        GLsizeiptr size) {
  NoContextHelper("glTexBufferRange");
}

void NoContextGLApi::glTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NoContextHelper("glTexImage2D");
}

void NoContextGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NoContextHelper("glTexImage2DRobustANGLE");
}

void NoContextGLApi::glTexImage3DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NoContextHelper("glTexImage3D");
}

void NoContextGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NoContextHelper("glTexImage3DRobustANGLE");
}

void NoContextGLApi::glTexParameterfFn(GLenum target,
                                       GLenum pname,
                                       GLfloat param) {
  NoContextHelper("glTexParameterf");
}

void NoContextGLApi::glTexParameterfvFn(GLenum target,
                                        GLenum pname,
                                        const GLfloat* params) {
  NoContextHelper("glTexParameterfv");
}

void NoContextGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* params) {
  NoContextHelper("glTexParameterfvRobustANGLE");
}

void NoContextGLApi::glTexParameteriFn(GLenum target,
                                       GLenum pname,
                                       GLint param) {
  NoContextHelper("glTexParameteri");
}

void NoContextGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* params) {
  NoContextHelper("glTexParameterIivRobustANGLE");
}

void NoContextGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* params) {
  NoContextHelper("glTexParameterIuivRobustANGLE");
}

void NoContextGLApi::glTexParameterivFn(GLenum target,
                                        GLenum pname,
                                        const GLint* params) {
  NoContextHelper("glTexParameteriv");
}

void NoContextGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* params) {
  NoContextHelper("glTexParameterivRobustANGLE");
}

void NoContextGLApi::glTexStorage2DEXTFn(GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height) {
  NoContextHelper("glTexStorage2DEXT");
}

void NoContextGLApi::glTexStorage2DMultisampleFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLboolean fixedsamplelocations) {
  NoContextHelper("glTexStorage2DMultisample");
}

void NoContextGLApi::glTexStorage3DFn(GLenum target,
                                      GLsizei levels,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  NoContextHelper("glTexStorage3D");
}

void NoContextGLApi::glTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NoContextHelper("glTexSubImage2D");
}

void NoContextGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLenum format,
                                                  GLenum type,
                                                  GLsizei bufSize,
                                                  const void* pixels) {
  NoContextHelper("glTexSubImage2DRobustANGLE");
}

void NoContextGLApi::glTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NoContextHelper("glTexSubImage3D");
}

void NoContextGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint zoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLenum format,
                                                  GLenum type,
                                                  GLsizei bufSize,
                                                  const void* pixels) {
  NoContextHelper("glTexSubImage3DRobustANGLE");
}

void NoContextGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                                   GLsizei count,
                                                   const char* const* varyings,
                                                   GLenum bufferMode) {
  NoContextHelper("glTransformFeedbackVaryings");
}

void NoContextGLApi::glUniform1fFn(GLint location, GLfloat x) {
  NoContextHelper("glUniform1f");
}

void NoContextGLApi::glUniform1fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform1fv");
}

void NoContextGLApi::glUniform1iFn(GLint location, GLint x) {
  NoContextHelper("glUniform1i");
}

void NoContextGLApi::glUniform1ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform1iv");
}

void NoContextGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  NoContextHelper("glUniform1ui");
}

void NoContextGLApi::glUniform1uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform1uiv");
}

void NoContextGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  NoContextHelper("glUniform2f");
}

void NoContextGLApi::glUniform2fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform2fv");
}

void NoContextGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  NoContextHelper("glUniform2i");
}

void NoContextGLApi::glUniform2ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform2iv");
}

void NoContextGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  NoContextHelper("glUniform2ui");
}

void NoContextGLApi::glUniform2uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform2uiv");
}

void NoContextGLApi::glUniform3fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  NoContextHelper("glUniform3f");
}

void NoContextGLApi::glUniform3fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform3fv");
}

void NoContextGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  NoContextHelper("glUniform3i");
}

void NoContextGLApi::glUniform3ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform3iv");
}

void NoContextGLApi::glUniform3uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2) {
  NoContextHelper("glUniform3ui");
}

void NoContextGLApi::glUniform3uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform3uiv");
}

void NoContextGLApi::glUniform4fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  NoContextHelper("glUniform4f");
}

void NoContextGLApi::glUniform4fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform4fv");
}

void NoContextGLApi::glUniform4iFn(GLint location,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLint w) {
  NoContextHelper("glUniform4i");
}

void NoContextGLApi::glUniform4ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform4iv");
}

void NoContextGLApi::glUniform4uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2,
                                    GLuint v3) {
  NoContextHelper("glUniform4ui");
}

void NoContextGLApi::glUniform4uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform4uiv");
}

void NoContextGLApi::glUniformBlockBindingFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLuint uniformBlockBinding) {
  NoContextHelper("glUniformBlockBinding");
}

void NoContextGLApi::glUniformMatrix2fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix2fv");
}

void NoContextGLApi::glUniformMatrix2x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix2x3fv");
}

void NoContextGLApi::glUniformMatrix2x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix2x4fv");
}

void NoContextGLApi::glUniformMatrix3fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix3fv");
}

void NoContextGLApi::glUniformMatrix3x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix3x2fv");
}

void NoContextGLApi::glUniformMatrix3x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix3x4fv");
}

void NoContextGLApi::glUniformMatrix4fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix4fv");
}

void NoContextGLApi::glUniformMatrix4x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix4x2fv");
}

void NoContextGLApi::glUniformMatrix4x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix4x3fv");
}

GLboolean NoContextGLApi::glUnmapBufferFn(GLenum target) {
  NoContextHelper("glUnmapBuffer");
  return GL_FALSE;
}

void NoContextGLApi::glUseProgramFn(GLuint program) {
  NoContextHelper("glUseProgram");
}

void NoContextGLApi::glUseProgramStagesFn(GLuint pipeline,
                                          GLbitfield stages,
                                          GLuint program) {
  NoContextHelper("glUseProgramStages");
}

void NoContextGLApi::glValidateProgramFn(GLuint program) {
  NoContextHelper("glValidateProgram");
}

void NoContextGLApi::glValidateProgramPipelineFn(GLuint pipeline) {
  NoContextHelper("glValidateProgramPipeline");
}

void NoContextGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  NoContextHelper("glVertexAttrib1f");
}

void NoContextGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib1fv");
}

void NoContextGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  NoContextHelper("glVertexAttrib2f");
}

void NoContextGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib2fv");
}

void NoContextGLApi::glVertexAttrib3fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z) {
  NoContextHelper("glVertexAttrib3f");
}

void NoContextGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib3fv");
}

void NoContextGLApi::glVertexAttrib4fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z,
                                        GLfloat w) {
  NoContextHelper("glVertexAttrib4f");
}

void NoContextGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib4fv");
}

void NoContextGLApi::glVertexAttribBindingFn(GLuint attribindex,
                                             GLuint bindingindex) {
  NoContextHelper("glVertexAttribBinding");
}

void NoContextGLApi::glVertexAttribDivisorANGLEFn(GLuint index,
                                                  GLuint divisor) {
  NoContextHelper("glVertexAttribDivisorANGLE");
}

void NoContextGLApi::glVertexAttribFormatFn(GLuint attribindex,
                                            GLint size,
                                            GLenum type,
                                            GLboolean normalized,
                                            GLuint relativeoffset) {
  NoContextHelper("glVertexAttribFormat");
}

void NoContextGLApi::glVertexAttribI4iFn(GLuint indx,
                                         GLint x,
                                         GLint y,
                                         GLint z,
                                         GLint w) {
  NoContextHelper("glVertexAttribI4i");
}

void NoContextGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  NoContextHelper("glVertexAttribI4iv");
}

void NoContextGLApi::glVertexAttribI4uiFn(GLuint indx,
                                          GLuint x,
                                          GLuint y,
                                          GLuint z,
                                          GLuint w) {
  NoContextHelper("glVertexAttribI4ui");
}

void NoContextGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  NoContextHelper("glVertexAttribI4uiv");
}

void NoContextGLApi::glVertexAttribIFormatFn(GLuint attribindex,
                                             GLint size,
                                             GLenum type,
                                             GLuint relativeoffset) {
  NoContextHelper("glVertexAttribIFormat");
}

void NoContextGLApi::glVertexAttribIPointerFn(GLuint indx,
                                              GLint size,
                                              GLenum type,
                                              GLsizei stride,
                                              const void* ptr) {
  NoContextHelper("glVertexAttribIPointer");
}

void NoContextGLApi::glVertexAttribPointerFn(GLuint indx,
                                             GLint size,
                                             GLenum type,
                                             GLboolean normalized,
                                             GLsizei stride,
                                             const void* ptr) {
  NoContextHelper("glVertexAttribPointer");
}

void NoContextGLApi::glVertexBindingDivisorFn(GLuint bindingindex,
                                              GLuint divisor) {
  NoContextHelper("glVertexBindingDivisor");
}

void NoContextGLApi::glViewportFn(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height) {
  NoContextHelper("glViewport");
}

void NoContextGLApi::glWaitSyncFn(GLsync sync,
                                  GLbitfield flags,
                                  GLuint64 timeout) {
  NoContextHelper("glWaitSync");
}

void NoContextGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                             GLsizei n,
                                             const GLint* box) {
  NoContextHelper("glWindowRectanglesEXT");
}

}  // namespace gl
