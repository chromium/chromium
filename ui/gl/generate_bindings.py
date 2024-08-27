#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for GL/GLES extension wrangler."""

import optparse
import os
import collections
import re
import platform
import sys
from subprocess import call
from collections import namedtuple

HEADER_PATHS = [
  '../../third_party/angle/include',
  '.',
  '../../gpu',
]

UNCONDITIONALLY_BOUND_EXTENSIONS = set([
  'GL_CHROMIUM_gles_depth_binding_hack', # crbug.com/448206
  'GL_CHROMIUM_glgetstringi_hack', # crbug.com/470396
  'GL_CHROMIUM_egl_khr_fence_sync_hack', # crbug.com/504758
  'GL_CHROMIUM_egl_android_native_fence_sync_hack', # crbug.com/775707
])

"""Function binding conditions can be specified manually by supplying a versions
array instead of the names array. Each version has the following keys:
   name: Mandatory. Name of the function. Multiple versions can have the same
         name but different conditions.
   extensions: Extra Extensions for which the function is bound. Only needed
               in some cases where the extension cannot be parsed from the
               headers.
   explicit_only: if True, only extensions in 'extensions' are considered.
   is_optional: True if the GetProcAddress can return NULL for the
                function.  This may happen for example when functions
                are added to a new version of an extension, but the
                extension string is not modified.
By default, the function gets its name from the first name in its names or
versions array. This can be overridden by supplying a 'known_as' key.

"""
GL_FUNCTIONS = [
{ 'return_type': 'void',
  'versions': [{ 'name': 'glAcquireTexturesANGLE',
                 'extensions': ['GL_ANGLE_vulkan_image'] }],
  'arguments': 'GLuint numTextures, const GLuint* textures, '
               'const GLenum* layouts', },
{ 'return_type': 'void',
  'names': ['glActiveShaderProgram'],
  'arguments': 'GLuint pipeline, GLuint program', },
{ 'return_type': 'void',
  'names': ['glActiveTexture'],
  'arguments': 'GLenum texture', },
{ 'return_type': 'void',
  'names': ['glAttachShader'],
  'arguments': 'GLuint program, GLuint shader', },
{ 'return_type': 'void',
  'versions': [{'name': 'glBeginPixelLocalStorageANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLsizei n, const GLenum* loadops', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glBeginQuery' },
               { 'name': 'glBeginQueryEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLenum target, GLuint id', },
{ 'return_type': 'void',
  'names': ['glBeginTransformFeedback'],
  'arguments': 'GLenum primitiveMode', },
{ 'return_type': 'void',
  'names': ['glBindAttribLocation'],
  'arguments': 'GLuint program, GLuint index, const char* name', },
{ 'return_type': 'void',
  'names': ['glBindBuffer'],
  'arguments': 'GLenum target, GLuint buffer', },
{ 'return_type': 'void',
  'names': ['glBindBufferBase'],
  'arguments': 'GLenum target, GLuint index, GLuint buffer', },
{ 'return_type': 'void',
  'names': ['glBindBufferRange'],
  'arguments': 'GLenum target, GLuint index, GLuint buffer, GLintptr offset, '
               'GLsizeiptr size', },
{ 'return_type': 'void',
  'known_as': 'glBindFragDataLocation',
  'versions': [{ 'name': 'glBindFragDataLocationEXT',
                 'extensions': ['GL_EXT_blend_func_extended'] }],
  'arguments': 'GLuint program, GLuint colorNumber, const char* name', },
{ 'return_type': 'void',
  'known_as': 'glBindFragDataLocationIndexed',
  'versions': [{ 'name': 'glBindFragDataLocationIndexedEXT',
                 'extensions': ['GL_EXT_blend_func_extended'] }],
  'arguments':
      'GLuint program, GLuint colorNumber, GLuint index, const char* name',
},
{ 'return_type': 'void',
  'known_as': 'glBindFramebufferEXT',
  'names': ['glBindFramebuffer'],
  'arguments': 'GLenum target, GLuint framebuffer', },
{ 'return_type': 'void',
  'known_as': 'glBindImageTextureEXT',
  'versions': [{ 'name': 'glBindImageTexture' },
               { 'name': 'glBindImageTextureEXT',
                 'extensions': ['GL_EXT_shader_image_load_store'] }],
  'arguments': 'GLuint index, GLuint texture, GLint level, GLboolean layered,'
               'GLint layer, GLenum access, GLint format', },
{ 'return_type': 'void',
  'names': ['glBindProgramPipeline'],
  'arguments': 'GLuint pipeline', },
{ 'return_type': 'void',
  'known_as': 'glBindRenderbufferEXT',
  'names': ['glBindRenderbuffer'],
  'arguments': 'GLenum target, GLuint renderbuffer', },
{ 'return_type': 'void',
  'names': ['glBindSampler'],
  'arguments': 'GLuint unit, GLuint sampler', },
{ 'return_type': 'void',
  'names': ['glBindTexture'],
  'arguments': 'GLenum target, GLuint texture', },
{ 'return_type': 'void',
  'names': ['glBindTransformFeedback'],
  'arguments': 'GLenum target, GLuint id', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glBindUniformLocationCHROMIUM',
                 'extensions': ['GL_CHROMIUM_bind_uniform_location'] }],
  'arguments': 'GLuint program, GLint location, const char* name' },
{ 'return_type': 'void',
  'known_as': 'glBindVertexArrayOES',
  'versions': [{ 'name': 'glBindVertexArray' },
               { 'name': 'glBindVertexArrayOES' }],
  'arguments': 'GLuint array' },
{ 'return_type': 'void',
  'names': ['glBindVertexBuffer'],
  'arguments': 'GLuint bindingindex, GLuint buffer, GLintptr offset, '
               'GLsizei stride', },
{ 'return_type': 'void',
  'known_as': 'glBlendBarrierKHR',
  'versions': [{ 'name': 'glBlendBarrierNV',
                 'extensions': ['GL_NV_blend_equation_advanced'] },
               { 'name': 'glBlendBarrierKHR',
                 'extensions': ['GL_KHR_blend_equation_advanced'] }],
  'arguments': 'void' },
{ 'return_type': 'void',
  'names': ['glBlendColor'],
  'arguments': 'GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha', },
{ 'return_type': 'void',
  'names': ['glBlendEquation'],
  'arguments': ' GLenum mode ', },
{ 'return_type': 'void',
  'known_as': 'glBlendEquationiOES',
  'versions': [
    { 'name': 'glBlendEquationi' },
    { 'name': 'glBlendEquationiOES', 'extensions':
      ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': ' GLuint buf, GLenum mode ', },
{ 'return_type': 'void',
  'names': ['glBlendEquationSeparate'],
  'arguments': 'GLenum modeRGB, GLenum modeAlpha', },
{ 'return_type': 'void',
  'known_as': 'glBlendEquationSeparateiOES',
  'versions': [
    { 'name': 'glBlendEquationSeparatei' },
    { 'name': 'glBlendEquationSeparateiOES', 'extensions':
      ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': 'GLuint buf, GLenum modeRGB, GLenum modeAlpha', },
{ 'return_type': 'void',
  'names': ['glBlendFunc'],
  'arguments': 'GLenum sfactor, GLenum dfactor', },
{ 'return_type': 'void',
  'known_as': 'glBlendFunciOES',
  'versions': [
    { 'name': 'glBlendFunci' },
    { 'name': 'glBlendFunciOES', 'extensions': ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': 'GLuint buf, GLenum sfactor, GLenum dfactor', },
{ 'return_type': 'void',
  'names': ['glBlendFuncSeparate'],
  'arguments':
      'GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha', },
{ 'return_type': 'void',
  'known_as': 'glBlendFuncSeparateiOES',
  'versions': [
    { 'name': 'glBlendFuncSeparatei' },
    { 'name': 'glBlendFuncSeparateiOES', 'extensions':
      ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments':
      'GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, '
      'GLenum dstAlpha', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glBlitFramebuffer'},
                {'name': 'glBlitFramebufferNV',
                 'extensions': ['GL_NV_framebuffer_blit']},
                {'name': 'glBlitFramebufferANGLE'}],
  'arguments': 'GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, '
               'GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, '
               'GLbitfield mask, GLenum filter', },
{ 'return_type': 'void',
  'names': ['glBufferData'],
  'arguments':
      'GLenum target, GLsizeiptr size, const void* data, GLenum usage', },
{ 'return_type': 'void',
  'names': ['glBufferSubData'],
  'arguments':
      'GLenum target, GLintptr offset, GLsizeiptr size, const void* data', },
{ 'return_type': 'GLenum',
  'known_as': 'glCheckFramebufferStatusEXT',
  'names': ['glCheckFramebufferStatus'],
  'arguments': 'GLenum target',
  'logging_code': """
  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringEnum(result));
""", },
{ 'return_type': 'void',
  'names': ['glClear'],
  'arguments': 'GLbitfield mask', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearBufferfi' }],
  'arguments': 'GLenum buffer, GLint drawbuffer, const GLfloat depth, '
               'GLint stencil', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearBufferfv' }],
  'arguments': 'GLenum buffer, GLint drawbuffer, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearBufferiv' }],
  'arguments': 'GLenum buffer, GLint drawbuffer, const GLint* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearBufferuiv' }],
  'arguments': 'GLenum buffer, GLint drawbuffer, const GLuint* value', },
{ 'return_type': 'void',
  'names': ['glClearColor'],
  'arguments': 'GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearDepth',
                 'extensions': ['GL_CHROMIUM_gles_depth_binding_hack'] }],
  'arguments': 'GLclampd depth', },
{ 'return_type': 'void',
  'names': ['glClearDepthf'],
  'arguments': 'GLclampf depth', },
{ 'return_type': 'void',
  'names': ['glClearStencil'],
  'arguments': 'GLint s', },
{ 'return_type': 'void',
  'known_as': 'glClearTexImage',
  'versions': [{ 'name': 'glClearTexImageEXT',
                 'extensions': ['GL_EXT_clear_texture'] }],
  'arguments':
      'GLuint texture, GLint level, GLenum format, GLenum type, '
      'const GLvoid* data', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glClearTexSubImage',
                 'extensions': ['GL_EXT_clear_texture'] },
               { 'name': 'glClearTexSubImageEXT',
                 'extensions': ['GL_EXT_clear_texture'] }],
  'arguments':
      'GLuint texture, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLint width, GLint height, GLint depth, GLenum format, '
      'GLenum type, const GLvoid* data', },
{ 'return_type': 'GLenum',
  'names': ['glClientWaitSync'],
  'arguments': 'GLsync sync, GLbitfield flags, GLuint64 timeout', },
{ 'return_type': 'void',
  'versions': [{'name': 'glClipControlEXT',
                'extensions': ['GL_EXT_clip_control']}],
  'arguments': 'GLenum origin, GLenum depth', },
{ 'return_type': 'void',
  'names': ['glColorMask'],
  'arguments':
      'GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha', },
{ 'return_type': 'void',
  'known_as': 'glColorMaskiOES',
  'versions': [
    { 'name': 'glColorMaski' },
    { 'name': 'glColorMaskiOES', 'extensions': ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments':
      'GLuint buf, GLboolean red, GLboolean green, GLboolean blue, '
      'GLboolean alpha', },
{ 'return_type': 'void',
  'names': ['glCompileShader'],
  'arguments': 'GLuint shader', },
{ 'return_type': 'void',
  'names': ['glCompressedTexImage2D'],
  'arguments':
      'GLenum target, GLint level, GLenum internalformat, GLsizei width, '
      'GLsizei height, GLint border, GLsizei imageSize, const void* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glCompressedTexImage2DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLenum internalformat, GLsizei width, '
      'GLsizei height, GLint border, GLsizei imageSize, GLsizei dataSize, '
      'const void* data', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCompressedTexImage3D' }],
  'arguments':
      'GLenum target, GLint level, GLenum internalformat, GLsizei width, '
      'GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, '
      'const void* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glCompressedTexImage3DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLenum internalformat, GLsizei width, '
      'GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, '
      'GLsizei dataSize, const void* data', },
{ 'return_type': 'void',
  'names': ['glCompressedTexSubImage2D'],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, '
      'const void* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glCompressedTexSubImage2DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, '
      'GLsizei dataSize, const void* data', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCompressedTexSubImage3D' }],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, '
      'GLenum format, GLsizei imageSize, const void* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glCompressedTexSubImage3DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, '
      'GLenum format, GLsizei imageSize, GLsizei dataSize, '
      'const void* data', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCopyBufferSubData' }],
  'arguments':
      'GLenum readTarget, GLenum writeTarget, GLintptr readOffset, '
      'GLintptr writeOffset, GLsizeiptr size', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCopySubTextureCHROMIUM',
                 'extensions': ['GL_CHROMIUM_copy_texture'], }],
  'arguments':
      'GLuint sourceId, GLint sourceLevel, GLenum destTarget, GLuint destId, '
      'GLint destLevel, GLint xoffset, GLint yoffset, GLint x, GLint y, '
      'GLsizei width, GLsizei height, GLboolean unpackFlipY, '
      'GLboolean unpackPremultiplyAlpha, GLboolean unpackUnmultiplyAlpha', },
{ 'return_type': 'void',
  'names': ['glCopyTexImage2D'],
  'arguments':
      'GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, '
      'GLsizei width, GLsizei height, GLint border', },
{ 'return_type': 'void',
  'names': ['glCopyTexSubImage2D'],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, '
      'GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCopyTexSubImage3D' }],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glCopyTextureCHROMIUM',
                 'extensions': ['GL_CHROMIUM_copy_texture'], }],
  'arguments':
      'GLuint sourceId, GLint sourceLevel, GLenum destTarget, GLuint destId, '
      'GLint destLevel, GLint internalFormat, GLenum destType, '
      'GLboolean unpackFlipY, GLboolean unpackPremultiplyAlpha, '
      'GLboolean unpackUnmultiplyAlpha', },
{ 'return_type': 'void',
  'names': [ 'glCreateMemoryObjectsEXT' ],
  'arguments': 'GLsizei n, GLuint* memoryObjects', },
{ 'return_type': 'GLuint',
  'names': ['glCreateProgram'],
  'arguments': 'void', },
{ 'return_type': 'GLuint',
  'names': ['glCreateShader'],
  'arguments': 'GLenum type', },
{ 'return_type': 'GLuint',
  'names': ['glCreateShaderProgramv'],
  'arguments': 'GLenum type, GLsizei count, const char* const* strings', },
{ 'return_type': 'void',
  'names': ['glCullFace'],
  'arguments': 'GLenum mode', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDebugMessageCallback' },
               { 'name': 'glDebugMessageCallbackKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments': 'GLDEBUGPROC callback, const void* userParam', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDebugMessageControl' },
               { 'name': 'glDebugMessageControlKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLenum source, GLenum type, GLenum severity, GLsizei count, '
    'const GLuint* ids, GLboolean enabled', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDebugMessageInsert' },
               { 'name': 'glDebugMessageInsertKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLenum source, GLenum type, GLuint id, GLenum severity, '
    'GLsizei length, const char* buf', },
{ 'return_type': 'void',
  'names': ['glDeleteBuffers'],
  'known_as': 'glDeleteBuffersARB',
  'arguments': 'GLsizei n, const GLuint* buffers', },
{ 'return_type': 'void',
  'names': ['glDeleteFencesNV'],
  'arguments': 'GLsizei n, const GLuint* fences', },
{ 'return_type': 'void',
  'known_as': 'glDeleteFramebuffersEXT',
  'names': ['glDeleteFramebuffers'],
  'arguments': 'GLsizei n, const GLuint* framebuffers', },
{ 'return_type': 'void',
  'names': [ 'glDeleteMemoryObjectsEXT' ],
  'versions': [{ 'name': 'glDeleteMemoryObjectsEXT',
                 'extensions': ['GL_EXT_memory_object'] }],
  'arguments': 'GLsizei n, const GLuint* memoryObjects', },
{ 'return_type': 'void',
  'names': ['glDeleteProgram'],
  'arguments': 'GLuint program', },
{ 'return_type': 'void',
  'names': ['glDeleteProgramPipelines'],
  'arguments': 'GLsizei n, const GLuint* pipelines', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDeleteQueries' },
               { 'name': 'glDeleteQueriesEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLsizei n, const GLuint* ids', },
{ 'return_type': 'void',
  'known_as': 'glDeleteRenderbuffersEXT',
  'names': ['glDeleteRenderbuffers'],
  'arguments': 'GLsizei n, const GLuint* renderbuffers', },
{ 'return_type': 'void',
  'names': ['glDeleteSamplers'],
  'arguments': 'GLsizei n, const GLuint* samplers', },
{ 'return_type': 'void',
  'names': ['glDeleteSemaphoresEXT'],
  'arguments': 'GLsizei n, const GLuint* semaphores', },
{ 'return_type': 'void',
  'names': ['glDeleteShader'],
  'arguments': 'GLuint shader', },
{ 'return_type': 'void',
  'names': ['glDeleteSync'],
  'arguments': 'GLsync sync', },
{ 'return_type': 'void',
  'names': ['glDeleteTextures'],
  'arguments': 'GLsizei n, const GLuint* textures', },
{ 'return_type': 'void',
  'names': ['glDeleteTransformFeedbacks'],
  'arguments': 'GLsizei n, const GLuint* ids', },
{ 'return_type': 'void',
  'known_as': 'glDeleteVertexArraysOES',
  'versions': [{ 'name': 'glDeleteVertexArrays' },
               { 'name': 'glDeleteVertexArraysOES' }],
  'arguments': 'GLsizei n, const GLuint* arrays' },
{ 'return_type': 'void',
  'names': ['glDepthFunc'],
  'arguments': 'GLenum func', },
{ 'return_type': 'void',
  'names': ['glDepthMask'],
  'arguments': 'GLboolean flag', },
{ 'return_type': 'void',
 'versions': [{ 'name': 'glDepthRange',
                'extensions': ['GL_CHROMIUM_gles_depth_binding_hack'] }],
  'arguments': 'GLclampd zNear, GLclampd zFar', },
{ 'return_type': 'void',
  'names': ['glDepthRangef'],
  'arguments': 'GLclampf zNear, GLclampf zFar', },
{ 'return_type': 'void',
  'names': ['glDetachShader'],
  'arguments': 'GLuint program, GLuint shader', },
{ 'return_type': 'void',
  'names': ['glDisable'],
  'arguments': 'GLenum cap', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDisableExtensionANGLE',
                 'extensions': ['GL_ANGLE_request_extension'] }],
  'arguments': 'const char* name', },
{ 'return_type': 'void',
  'known_as': 'glDisableiOES',
  'versions': [
    { 'name': 'glDisablei' },
    { 'name': 'glDisableiOES', 'extensions': ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': 'GLenum target, GLuint index', },
{ 'return_type': 'void',
  'names': ['glDisableVertexAttribArray'],
  'arguments': 'GLuint index', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDiscardFramebufferEXT',
                 'extensions': ['GL_EXT_discard_framebuffer'] }],
  'arguments': 'GLenum target, GLsizei numAttachments, '
      'const GLenum* attachments' },
{ 'return_type': 'void',
  'names': ['glDispatchCompute'],
  'arguments': 'GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ', },
{ 'return_type': 'void',
  'names': ['glDispatchComputeIndirect'],
  'arguments': 'GLintptr indirect', },
{ 'return_type': 'void',
  'names': ['glDrawArrays'],
  'arguments': 'GLenum mode, GLint first, GLsizei count', },
{ 'return_type': 'void',
  'names': ['glDrawArraysIndirect'],
  'arguments': 'GLenum mode, const void* indirect', },
{ 'return_type': 'void',
  'known_as': 'glDrawArraysInstancedANGLE',
  'names': ['glDrawArraysInstancedANGLE', 'glDrawArraysInstanced'],
  'arguments': 'GLenum mode, GLint first, GLsizei count, GLsizei primcount', },
{ 'return_type': 'void',
  'known_as': 'glDrawArraysInstancedBaseInstanceANGLE',
  #TODO(shrekshao): workaround when native support not available for cmd decoder
  'versions' : [{ 'name': 'glDrawArraysInstancedBaseInstanceEXT' },
                { 'name': 'glDrawArraysInstancedBaseInstanceANGLE',
                 'extensions': ['GL_ANGLE_base_vertex_base_instance'] }],
  'arguments': 'GLenum mode, GLint first, GLsizei count, GLsizei primcount, '
  'GLuint baseinstance', },
{ 'return_type': 'void',
  'names': ['glDrawBuffer'],
  'arguments': 'GLenum mode', },
{ 'return_type': 'void',
  'known_as': 'glDrawBuffersARB',
  'names': ['glDrawBuffersEXT', 'glDrawBuffers'],
  'arguments': 'GLsizei n, const GLenum* bufs', },
{ 'return_type': 'void',
  'names': ['glDrawElements'],
  'arguments':
      'GLenum mode, GLsizei count, GLenum type, const void* indices', },
{ 'return_type': 'void',
  'names': ['glDrawElementsIndirect'],
  'arguments': 'GLenum mode, GLenum type, const void* indirect', },
{ 'return_type': 'void',
  'known_as': 'glDrawElementsInstancedANGLE',
  'names': ['glDrawElementsInstancedANGLE', 'glDrawElementsInstanced'],
  'arguments':
      'GLenum mode, GLsizei count, GLenum type, const void* indices, '
      'GLsizei primcount', },
{ 'return_type': 'void',
  'known_as': 'glDrawElementsInstancedBaseVertexBaseInstanceANGLE',
  #TODO(shrekshao): workaround when native support not available for cmd decoder
  'versions' : [{ 'name': 'glDrawElementsInstancedBaseVertexBaseInstanceEXT' },
                { 'name': 'glDrawElementsInstancedBaseVertexBaseInstanceANGLE',
                 'extensions': ['GL_ANGLE_base_vertex_base_instance'] }],
  'arguments':
      'GLenum mode, GLsizei count, GLenum type, const void* indices, '
      'GLsizei primcount, GLint baseVertex, GLuint baseInstance', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glDrawRangeElements' }],
  'arguments': 'GLenum mode, GLuint start, GLuint end, GLsizei count, '
               'GLenum type, const void* indices', },
{ 'return_type': 'void',
  'names': ['glEGLImageTargetRenderbufferStorageOES'],
  'arguments': 'GLenum target, GLeglImageOES image', },
{ 'return_type': 'void',
  'names': ['glEGLImageTargetTexture2DOES'],
  'arguments': 'GLenum target, GLeglImageOES image', },
{ 'return_type': 'void',
  'names': ['glEnable'],
  'arguments': 'GLenum cap', },
{ 'return_type': 'void',
  'known_as': 'glEnableiOES',
  'versions': [
    { 'name': 'glEnablei' },
    { 'name': 'glEnableiOES', 'extensions': ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': 'GLenum target, GLuint index', },
{ 'return_type': 'void',
  'names': ['glEnableVertexAttribArray'],
  'arguments': 'GLuint index', },
{ 'return_type': 'void',
  'versions': [{'name': 'glEndPixelLocalStorageANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLsizei n, const GLenum* storeops', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glEndQuery' },
               { 'name': 'glEndQueryEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLenum target', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glEndTilingQCOM',
                 'extension': ['GL_QCOM_tiled_rendering'] }],
  'arguments': 'GLbitfield preserveMask', },
{ 'return_type': 'void',
  'names': ['glEndTransformFeedback'],
  'arguments': 'void', },
{ 'return_type': 'GLsync',
  'names': ['glFenceSync'],
  'arguments': 'GLenum condition, GLbitfield flags', },
{ 'return_type': 'void',
  'names': ['glFinish'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'names': ['glFinishFenceNV'],
  'arguments': 'GLuint fence', },
{ 'return_type': 'void',
  'names': ['glFlush'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'names': ['glFlushMappedBufferRange', 'glFlushMappedBufferRangeEXT'],
  'arguments': 'GLenum target, GLintptr offset, GLsizeiptr length', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferMemorylessPixelLocalStorageANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLint plane, GLenum internalformat', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferParameteri'},
               {'name': 'glFramebufferParameteriMESA',
                'extensions': ['GL_MESA_framebuffer_flip_y']}],
  'arguments': 'GLenum target, GLenum pname, GLint param', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferPixelLocalClearValuefvANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLint plane, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferPixelLocalClearValueivANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLint plane, const GLint* value', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferPixelLocalClearValueuivANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLint plane, const GLuint* value', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferPixelLocalStorageInterruptANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': '', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferPixelLocalStorageRestoreANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': '', },
{ 'return_type': 'void',
  'known_as': 'glFramebufferRenderbufferEXT',
  'names': ['glFramebufferRenderbuffer'],
  'arguments':
      'GLenum target, GLenum attachment, GLenum renderbuffertarget, '
      'GLuint renderbuffer', },
{ 'return_type': 'void',
  'known_as': 'glFramebufferTexture2DEXT',
  'names': ['glFramebufferTexture2D'],
  'arguments':
      'GLenum target, GLenum attachment, GLenum textarget, GLuint texture, '
      'GLint level', },
{ 'return_type': 'void',
 'versions': [{'name': 'glFramebufferTexture2DMultisampleEXT'},
              {'name': 'glFramebufferTexture2DMultisampleIMG'}],
  'arguments':
      'GLenum target, GLenum attachment, GLenum textarget, GLuint texture, '
      'GLint level, GLsizei samples', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glFramebufferTextureLayer' }],
  'arguments': 'GLenum target, GLenum attachment, GLuint texture, GLint level, '
               'GLint layer', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferTextureMultiviewOVR',
                'extensions': ['GL_OVR_multiview2']}],
  'arguments': 'GLenum target, GLenum attachment, GLuint texture, GLint level, '
               'GLint baseViewIndex, GLsizei numViews', },
{ 'return_type': 'void',
  'versions': [{'name': 'glFramebufferTexturePixelLocalStorageANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': 'GLint plane, GLuint backingtexture, GLint level, '
               'GLint layer', },
{ 'return_type': 'void',
  'names': ['glFrontFace'],
  'arguments': 'GLenum mode', },
{ 'return_type': 'void',
  'names': ['glGenBuffers'],
  'known_as': 'glGenBuffersARB',
  'arguments': 'GLsizei n, GLuint* buffers', },
{ 'return_type': 'void',
  'known_as': 'glGenerateMipmapEXT',
  'names': ['glGenerateMipmap'],
  'arguments': 'GLenum target', },
{ 'return_type': 'void',
  'names': ['glGenFencesNV'],
  'arguments': 'GLsizei n, GLuint* fences', },
{ 'return_type': 'void',
  'known_as': 'glGenFramebuffersEXT',
  'names': ['glGenFramebuffers'],
  'arguments': 'GLsizei n, GLuint* framebuffers', },
{ 'return_type': 'GLuint',
  'names': ['glGenProgramPipelines'],
  'arguments': 'GLsizei n, GLuint* pipelines' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGenQueries' },
               { 'name' : 'glGenQueriesEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLsizei n, GLuint* ids', },
{ 'return_type': 'void',
  'known_as': 'glGenRenderbuffersEXT',
  'names': ['glGenRenderbuffers'],
  'arguments': 'GLsizei n, GLuint* renderbuffers', },
{ 'return_type': 'void',
  'names': ['glGenSamplers'],
  'arguments': 'GLsizei n, GLuint* samplers', },
{ 'return_type': 'void',
  'names': ['glGenSemaphoresEXT'],
  'arguments': 'GLsizei n, GLuint* semaphores', },
{ 'return_type': 'void',
  'names': ['glGenTextures'],
  'arguments': 'GLsizei n, GLuint* textures', },
{ 'return_type': 'void',
  'names': ['glGenTransformFeedbacks'],
  'arguments': 'GLsizei n, GLuint* ids', },
{ 'return_type': 'void',
  'known_as': 'glGenVertexArraysOES',
  'versions': [{ 'name': 'glGenVertexArrays' },
               { 'name': 'glGenVertexArraysOES' }],
  'arguments': 'GLsizei n, GLuint* arrays', },
{ 'return_type': 'void',
  'names': ['glGetActiveAttrib'],
  'arguments':
      'GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, '
      'GLint* size, GLenum* type, char* name', },
{ 'return_type': 'void',
  'names': ['glGetActiveUniform'],
  'arguments':
      'GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, '
      'GLint* size, GLenum* type, char* name', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetActiveUniformBlockiv' }],
  'arguments': 'GLuint program, GLuint uniformBlockIndex, GLenum pname, '
               'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetActiveUniformBlockivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLuint uniformBlockIndex, GLenum pname, '
      'GLsizei bufSize, GLsizei* length, GLint* params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetActiveUniformBlockName' }],
  'arguments': 'GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, '
               'GLsizei* length, char* uniformBlockName', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetActiveUniformsiv' }],
  'arguments': 'GLuint program, GLsizei uniformCount, '
               'const GLuint* uniformIndices, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetAttachedShaders'],
  'arguments':
      'GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders', },
{ 'return_type': 'GLint',
  'names': ['glGetAttribLocation'],
  'arguments': 'GLuint program, const char* name', },
{ 'return_type': 'void',
  'names': ['glGetBooleani_v'],
  'arguments': 'GLenum target, GLuint index, GLboolean* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetBooleani_vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLuint index, GLsizei bufSize, GLsizei* length, '
      'GLboolean* data', },
{ 'return_type': 'void',
  'names': ['glGetBooleanv'],
  'arguments': 'GLenum pname, GLboolean* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetBooleanvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLsizei bufSize, GLsizei* length, GLboolean* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetBufferParameteri64vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint64* params', },
{ 'return_type': 'void',
  'names': ['glGetBufferParameteriv'],
  'arguments': 'GLenum target, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetBufferParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetBufferPointervRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'void** params', },
{ 'return_type': 'GLuint',
  'versions': [{ 'name': 'glGetDebugMessageLog' },
               { 'name': 'glGetDebugMessageLogKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, '
    'GLuint* ids, GLenum* severities, GLsizei* lengths, char* messageLog', },
{ 'return_type': 'GLenum',
  'names': ['glGetError'],
  'arguments': 'void',
  'logging_code': """
  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringError(result));
""", },
{ 'return_type': 'void',
  'names': ['glGetFenceivNV'],
  'arguments': 'GLuint fence, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetFloatv'],
  'arguments': 'GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFloatvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLsizei bufSize, GLsizei* length, GLfloat* data', },
{ 'return_type': 'GLint',
  'known_as': 'glGetFragDataIndex',
  'versions': [{'name': 'glGetFragDataIndexEXT',
                'extensions': ['GL_EXT_blend_func_extended']}],
  'arguments': 'GLuint program, const char* name', },
{ 'return_type': 'GLint',
  'versions': [{ 'name': 'glGetFragDataLocation' }],
  'arguments': 'GLuint program, const char* name', },
{ 'return_type': 'void',
  'known_as': 'glGetFramebufferAttachmentParameterivEXT',
  'names': ['glGetFramebufferAttachmentParameteriv'],
  'arguments': 'GLenum target, '
               'GLenum attachment, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferAttachmentParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum attachment, GLenum pname, GLsizei bufSize, '
      'GLsizei* length, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetFramebufferParameteriv'],
  'arguments': 'GLenum target, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferPixelLocalStorageParameterfvANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments':
      'GLint plane, GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferPixelLocalStorageParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory',
                               'GL_ANGLE_shader_pixel_local_storage']}],
  'arguments':
      'GLint plane, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferPixelLocalStorageParameterivANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments':
      'GLint plane, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetFramebufferPixelLocalStorageParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory',
                               'GL_ANGLE_shader_pixel_local_storage']}],
  'arguments':
      'GLint plane, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'GLenum',
  'known_as': 'glGetGraphicsResetStatusARB',
  'names': ['glGetGraphicsResetStatusKHR',
            'glGetGraphicsResetStatusEXT',
            'glGetGraphicsResetStatus'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetInteger64i_v' }],
  'arguments': 'GLenum target, GLuint index, GLint64* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetInteger64i_vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLuint index, GLsizei bufSize, GLsizei* length, '
      'GLint64* data', },
{ 'return_type': 'void',
  'names': ['glGetInteger64v'],
  'arguments': 'GLenum pname, GLint64* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetInteger64vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLsizei bufSize, GLsizei* length, GLint64* data', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetIntegeri_v' }],
  'arguments': 'GLenum target, GLuint index, GLint* data', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetIntegeri_vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLuint index, GLsizei bufSize, GLsizei* length, '
      'GLint* data', },
{ 'return_type': 'void',
  'names': ['glGetIntegerv'],
  'arguments': 'GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetIntegervRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLsizei bufSize, GLsizei* length, GLint* data', },
{ 'return_type': 'void',
  'names': ['glGetInternalformativ'],
  'arguments': 'GLenum target, GLenum internalformat, GLenum pname, '
               'GLsizei bufSize, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetInternalformativRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, '
      'GLsizei* length, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetInternalformatSampleivNV',
                'extensions': ['GL_NV_internalformat_sample_query']}],
  'arguments': 'GLenum target, GLenum internalformat, GLsizei samples, '
               'GLenum pname, GLsizei bufSize, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetMultisamplefv'],
  'arguments': 'GLenum pname, GLuint index, GLfloat* val', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetMultisamplefvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLuint index, GLsizei bufSize, GLsizei* length, '
      'GLfloat* val', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetnUniformfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetnUniformivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetnUniformuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetObjectLabel' },
               { 'name': 'glGetObjectLabelKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, '
    'char* label', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetObjectPtrLabel' },
               { 'name': 'glGetObjectPtrLabelKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments': 'void* ptr, GLsizei bufSize, GLsizei* length, char* label', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetPointerv' },
               { 'name': 'glGetPointervKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments': 'GLenum pname, void** params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetPointervRobustANGLERobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum pname, GLsizei bufSize, GLsizei* length, void** params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetProgramBinary' },
               { 'name': 'glGetProgramBinaryOES' }],
  'arguments': 'GLuint program, GLsizei bufSize, GLsizei* length, '
               'GLenum* binaryFormat, GLvoid* binary' },
{ 'return_type': 'void',
  'names': ['glGetProgramInfoLog'],
  'arguments':
      'GLuint program, GLsizei bufsize, GLsizei* length, char* infolog', },
{ 'return_type': 'void',
  'names': ['glGetProgramInterfaceiv'],
  'arguments': 'GLuint program, GLenum programInterface, GLenum pname, '
  'GLint* params'},
{ 'return_type': 'void',
  'versions': [{'name': 'glGetProgramInterfaceivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLenum programInterface, GLenum pname, '
      'GLsizei bufSize, GLsizei* length, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetProgramiv'],
  'arguments': 'GLuint program, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetProgramivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetProgramPipelineInfoLog'],
  'arguments':
      'GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* infoLog', },
{ 'return_type': 'void',
  'names': ['glGetProgramPipelineiv'],
  'arguments':
      'GLuint pipeline, GLenum pname, GLint* params', },
{ 'return_type': 'GLuint',
  'names': ['glGetProgramResourceIndex'],
  'arguments':
      'GLuint program, GLenum programInterface, const GLchar* name', },
{ 'return_type': 'void',
  'names': ['glGetProgramResourceiv'],
  'arguments': 'GLuint program, GLenum programInterface, GLuint index, '
  'GLsizei propCount, const GLenum* props, GLsizei bufSize, '
  'GLsizei* length, GLint* params'},
{ 'return_type': 'GLint',
  'names': ['glGetProgramResourceLocation'],
  'arguments': 'GLuint program, GLenum programInterface, const char* name', },
{ 'return_type': 'void',
  'names': ['glGetProgramResourceName'],
  'arguments': 'GLuint program, GLenum programInterface, GLuint index, '
  'GLsizei bufSize, GLsizei* length, GLchar* name'},
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetQueryiv' },
               { 'name': 'glGetQueryivEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLenum target, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetQueryivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'known_as': 'glGetQueryObjecti64v',
  'names': ['glGetQueryObjecti64vEXT'],
  'arguments': 'GLuint id, GLenum pname, GLint64* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetQueryObjecti64vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint id, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint64* params', },
{ 'return_type': 'void',
  'known_as': 'glGetQueryObjectiv',
  'names': ['glGetQueryObjectivEXT'],
  'arguments': 'GLuint id, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetQueryObjectivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint id, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'known_as': 'glGetQueryObjectui64v',
  'names': ['glGetQueryObjectui64vEXT'],
  'arguments': 'GLuint id, GLenum pname, GLuint64* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetQueryObjectui64vRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint id, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLuint64* params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetQueryObjectuiv' },
               { 'name': 'glGetQueryObjectuivEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLuint id, GLenum pname, GLuint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetQueryObjectuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint id, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'known_as': 'glGetRenderbufferParameterivEXT',
  'names': ['glGetRenderbufferParameteriv'],
  'arguments': 'GLenum target, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetRenderbufferParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetSamplerParameterfv'],
  'arguments': 'GLuint sampler, GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetSamplerParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetSamplerParameterIivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetSamplerParameterIuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'names': ['glGetSamplerParameteriv'],
  'arguments': 'GLuint sampler, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetSamplerParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetShaderInfoLog'],
  'arguments':
      'GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog', },
{ 'return_type': 'void',
  'names': ['glGetShaderiv'],
  'arguments': 'GLuint shader, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetShaderivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint shader, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetShaderPrecisionFormat'],
  'arguments': 'GLenum shadertype, GLenum precisiontype, '
               'GLint* range, GLint* precision', },
{ 'return_type': 'void',
  'names': ['glGetShaderSource'],
  'arguments':
      'GLuint shader, GLsizei bufsize, GLsizei* length, char* source', },
{ 'return_type': 'const GLubyte*',
  'names': ['glGetString'],
  'arguments': 'GLenum name', },
{ 'return_type': 'const GLubyte*',
  # This is needed for bootstrapping on the desktop GL core profile.
  # It won't be called unless the expected GL version is used.
  'versions': [{ 'name': 'glGetStringi',
                 'extensions': ['GL_CHROMIUM_glgetstringi_hack'] }],
  'arguments': 'GLenum name, GLuint index', },
{ 'return_type': 'void',
  'names': ['glGetSynciv'],
  'arguments':
    'GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length,'
    'GLint* values', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetTexLevelParameterfv' },
               {'name': 'glGetTexLevelParameterfvANGLE',
                'extensions': ['GL_ANGLE_get_tex_level_parameter']}],
  'arguments': 'GLenum target, GLint level, GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexLevelParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLenum pname, GLsizei bufSize, '
      'GLsizei* length, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetTexLevelParameteriv' },
               {'name': 'glGetTexLevelParameterivANGLE',
                'extensions': ['GL_ANGLE_get_tex_level_parameter']}],
  'arguments': 'GLenum target, GLint level, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexLevelParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLenum pname, GLsizei bufSize, '
      'GLsizei* length, GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetTexParameterfv'],
  'arguments': 'GLenum target, GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexParameterIivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexParameterIuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'names': ['glGetTexParameteriv'],
  'arguments': 'GLenum target, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetTexParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetTransformFeedbackVarying'],
  'arguments': 'GLuint program, GLuint index, GLsizei bufSize, '
               'GLsizei* length, GLsizei* size, GLenum* type, char* name', },
{ 'return_type': 'void',
  'names': ['glGetTranslatedShaderSourceANGLE'],
  'arguments':
      'GLuint shader, GLsizei bufsize, GLsizei* length, char* source', },
{ 'return_type': 'GLuint',
  'versions': [{ 'name': 'glGetUniformBlockIndex' }],
  'arguments': 'GLuint program, const char* uniformBlockName', },
{ 'return_type': 'void',
  'names': ['glGetUniformfv'],
  'arguments': 'GLuint program, GLint location, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetUniformfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glGetUniformIndices' }],
  'arguments': 'GLuint program, GLsizei uniformCount, '
               'const char* const* uniformNames, GLuint* uniformIndices', },
{ 'return_type': 'void',
  'names': ['glGetUniformiv'],
  'arguments': 'GLuint program, GLint location, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetUniformivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'GLint',
  'names': ['glGetUniformLocation'],
  'arguments': 'GLuint program, const char* name', },
{ 'return_type': 'void',
  'names': ['glGetUniformuiv'],
  'arguments': 'GLuint program, GLint location, GLuint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetUniformuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint program, GLint location, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'names': ['glGetVertexAttribfv'],
  'arguments': 'GLuint index, GLenum pname, GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetVertexAttribfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetVertexAttribIivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetVertexAttribIuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLuint* params', },
{ 'return_type': 'void',
  'names': ['glGetVertexAttribiv'],
  'arguments': 'GLuint index, GLenum pname, GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetVertexAttribivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'GLint* params', },
{ 'return_type': 'void',
  'names': ['glGetVertexAttribPointerv'],
  'arguments': 'GLuint index, GLenum pname, void** pointer', },
{ 'return_type': 'void',
  'versions': [{'name': 'glGetVertexAttribPointervRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint index, GLenum pname, GLsizei bufSize, GLsizei* length, '
      'void** pointer', },
{ 'return_type': 'void',
  'names': ['glHint'],
  'arguments': 'GLenum target, GLenum mode', },
{ 'return_type': 'void',
  'names': ['glImportMemoryFdEXT'],
  'arguments': 'GLuint memory, GLuint64 size, GLenum handleType, GLint fd', },
{ 'return_type': 'void',
  'names': ['glImportMemoryWin32HandleEXT'],
  'arguments': 'GLuint memory, GLuint64 size, GLenum handleType, void* handle',
  },
{ 'return_type': 'void',
  'arguments': 'GLuint memory, GLuint64 size, GLenum handleType, GLuint handle',
  'versions': [{'name': 'glImportMemoryZirconHandleANGLE',
                'extensions': ['GL_ANGLE_memory_object_fuchsia']}]},
{ 'return_type': 'void',
  'names': ['glImportSemaphoreFdEXT'],
  'arguments': 'GLuint semaphore, GLenum handleType, GLint fd', },
{ 'return_type': 'void',
  'names': ['glImportSemaphoreWin32HandleEXT'],
  'arguments': 'GLuint semaphore, GLenum handleType, void* handle', },
{ 'return_type': 'void',
  'arguments': 'GLuint semaphore, GLenum handleType, GLuint handle',
  'versions': [{'name': 'glImportSemaphoreZirconHandleANGLE',
                'extensions': ['GL_ANGLE_semaphore_fuchsia']}]},
{ 'return_type': 'void',
  'names': ['glInsertEventMarkerEXT'],
  'arguments': 'GLsizei length, const char* marker', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glInvalidateFramebuffer' }],
  'arguments': 'GLenum target, GLsizei numAttachments, '
      'const GLenum* attachments' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glInvalidateSubFramebuffer' }],
  'arguments':
      'GLenum target, GLsizei numAttachments, const GLenum* attachments, '
      'GLint x, GLint y, GLint width, GLint height', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glInvalidateTextureANGLE',
                 'extensions': ['GL_ANGLE_texture_external_update'] } ],
  'arguments': 'GLenum target', },
{ 'return_type': 'GLboolean',
  'names': ['glIsBuffer'],
  'arguments': 'GLuint buffer', },
{ 'return_type': 'GLboolean',
  'names': ['glIsEnabled'],
  'arguments': 'GLenum cap', },
{ 'return_type': 'GLboolean',
  'known_as': 'glIsEnablediOES',
  'versions': [
    { 'name': 'glIsEnabledi' },
    { 'name': 'glIsEnablediOES', 'extensions': ['GL_OES_draw_buffers_indexed'] }
  ],
  'arguments': 'GLenum target, GLuint index', },
{ 'return_type': 'GLboolean',
  'names': ['glIsFenceNV'],
  'arguments': 'GLuint fence', },
{ 'return_type': 'GLboolean',
  'known_as': 'glIsFramebufferEXT',
  'names': ['glIsFramebuffer'],
  'arguments': 'GLuint framebuffer', },
{ 'return_type': 'GLboolean',
  'names': ['glIsProgram'],
  'arguments': 'GLuint program', },
{ 'return_type': 'GLboolean',
  'names': ['glIsProgramPipeline'],
  'arguments': 'GLuint pipeline', },
{ 'return_type': 'GLboolean',
  'versions': [{ 'name': 'glIsQuery' },
               { 'name': 'glIsQueryEXT',
                 'extensions': ['GL_EXT_occlusion_query_boolean'] }],
  'arguments': 'GLuint query', },
{ 'return_type': 'GLboolean',
  'known_as': 'glIsRenderbufferEXT',
  'names': ['glIsRenderbuffer'],
  'arguments': 'GLuint renderbuffer', },
{ 'return_type': 'GLboolean',
  'names': ['glIsSampler'],
  'arguments': 'GLuint sampler', },
{ 'return_type': 'GLboolean',
  'names': ['glIsShader'],
  'arguments': 'GLuint shader', },
{ 'return_type': 'GLboolean',
  'names': ['glIsSync'],
  'arguments': 'GLsync sync', },
{ 'return_type': 'GLboolean',
  'names': ['glIsTexture'],
  'arguments': 'GLuint texture', },
{ 'return_type': 'GLboolean',
  'names': ['glIsTransformFeedback'],
  'arguments': 'GLuint id', },
{ 'return_type': 'GLboolean',
  'known_as': 'glIsVertexArrayOES',
  'versions': [{ 'name': 'glIsVertexArray' },
               { 'name': 'glIsVertexArrayOES' }],
  'arguments': 'GLuint array' },
{ 'return_type': 'void',
  'names': ['glLineWidth'],
  'arguments': 'GLfloat width', },
{ 'return_type': 'void',
  'names': ['glLinkProgram'],
  'arguments': 'GLuint program', },
{ 'return_type': 'void*',
  'known_as': 'glMapBuffer',
  'names': ['glMapBufferOES'],
  'arguments': 'GLenum target, GLenum access', },
{ 'return_type': 'void*',
  'known_as': 'glMapBufferRange',
  'versions': [{ 'name': 'glMapBufferRange' },
               { 'name': 'glMapBufferRangeEXT',
                 'extensions': ['GL_EXT_map_buffer_range'] }],
  'arguments':
      'GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access', },
{'return_type': 'void',
 'known_as': 'glMaxShaderCompilerThreadsKHR',
  'versions': [{ 'name': 'glMaxShaderCompilerThreadsKHR',
                 'extensions': ['GL_KHR_parallel_shader_compile'] }],
  'arguments': 'GLuint count', },
{ 'return_type': 'void',
  'names': ['glMemoryBarrierByRegion'],
  'arguments': 'GLbitfield barriers', },
{'return_type': 'void',
  'known_as': 'glMemoryBarrierEXT',
  'versions': [{ 'name': 'glMemoryBarrier' },
               { 'name': 'glMemoryBarrierEXT',
                 'extensions': ['GL_EXT_shader_image_load_store'] }],
  'arguments': 'GLbitfield barriers', },
{ 'return_type': 'void',
  'names': ['glMemoryObjectParameterivEXT'],
  'arguments': 'GLuint memoryObject, GLenum pname, const GLint* param'},
{ 'return_type': 'void',
  'names': ['glMinSampleShading'],
  'arguments': 'GLfloat value', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawArraysANGLE',
                 'extensions': ['GL_ANGLE_multi_draw'] }],
  'arguments': 'GLenum mode, const GLint* firsts, '
               'const GLsizei* counts, GLsizei drawcount', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawArraysInstancedANGLE',
                 'extensions': ['GL_ANGLE_multi_draw'] }],
  'arguments': 'GLenum mode, const GLint* firsts, '
               'const GLsizei* counts, const GLsizei* instanceCounts, '
               'GLsizei drawcount', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawArraysInstancedBaseInstanceANGLE',
                 'extensions': ['GL_ANGLE_base_vertex_base_instance'] }],
  'arguments': 'GLenum mode, const GLint* firsts, '
               'const GLsizei* counts, const GLsizei* instanceCounts, '
               'const GLuint* baseInstances, GLsizei drawcount', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawElementsANGLE',
                 'extensions': ['GL_ANGLE_multi_draw'] }],
  'arguments': 'GLenum mode, const GLsizei* counts, '
               'GLenum type, const GLvoid* const* indices, '
               'GLsizei drawcount', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawElementsInstancedANGLE',
                 'extensions': ['GL_ANGLE_multi_draw'] }],
  'arguments': 'GLenum mode, const GLsizei* counts, '
               'GLenum type, const GLvoid* const* indices, '
               'const GLsizei* instanceCounts, GLsizei drawcount', },
{ 'return_type': 'void',
  'versions' : [{'name': 'glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE',
                 'extensions': ['GL_ANGLE_base_vertex_base_instance'] }],
  'arguments': 'GLenum mode, '
               'const GLsizei* counts, GLenum type, '
               'const GLvoid* const* indices, const GLsizei* instanceCounts, '
               'const GLint* baseVertices, const GLuint* baseInstances, '
               'GLsizei drawcount', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glObjectLabel' },
               { 'name': 'glObjectLabelKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLenum identifier, GLuint name, GLsizei length, const char* label', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glObjectPtrLabel' },
               { 'name': 'glObjectPtrLabelKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments': 'void* ptr, GLsizei length, const char* label', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glPatchParameteri' },
               { 'name': 'glPatchParameteriOES',
                 'extensions': ['GL_OES_tessellation_shader'] }],
  'arguments': 'GLenum pname, GLint value', },
{ 'return_type': 'void',
  'names': ['glPauseTransformFeedback'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'versions': [{'name': 'glPixelLocalStorageBarrierANGLE',
                'extensions': ['GL_ANGLE_shader_pixel_local_storage']}],
  'arguments': '', },
{ 'return_type': 'void',
  'names': ['glPixelStorei'],
  'arguments': 'GLenum pname, GLint param', },
{ 'return_type': 'void',
  'names': ['glPointParameteri'],
  'arguments': 'GLenum pname, GLint param', },
{ 'return_type': 'void',
  'names': ['glPolygonMode'],
  'arguments': 'GLenum face, GLenum mode', },
{ 'return_type': 'void',
  'versions': [{'name': 'glPolygonModeANGLE',
                'extensions': ['GL_ANGLE_polygon_mode']}],
  'arguments': 'GLenum face, GLenum mode', },
{ 'return_type': 'void',
  'names': ['glPolygonOffset'],
  'arguments': 'GLfloat factor, GLfloat units', },
{ 'return_type': 'void',
  'versions': [{'name': 'glPolygonOffsetClampEXT',
                'extensions': ['GL_EXT_polygon_offset_clamp']}],
  'arguments': 'GLfloat factor, GLfloat units, GLfloat clamp', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glPopDebugGroup' },
               { 'name': 'glPopDebugGroupKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments': '', },
{ 'return_type': 'void',
  'names': ['glPopGroupMarkerEXT'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'names': ['glPrimitiveRestartIndex'],
  'arguments': 'GLuint index', },
{ 'return_type': 'void',
  'names': ['glProgramBinary', 'glProgramBinaryOES'],
  'arguments': 'GLuint program, GLenum binaryFormat, '
               'const GLvoid* binary, GLsizei length' },
{ 'return_type': 'void',
  'names': ['glProgramParameteri'],
  'arguments': 'GLuint program, GLenum pname, GLint value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1f'],
  'arguments': 'GLuint program, GLint location, GLfloat v0' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1i'],
  'arguments': 'GLuint program, GLint location, GLint v0' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1iv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1ui'],
  'arguments': 'GLuint program, GLint location, GLuint v0' },
{ 'return_type': 'void',
  'names': ['glProgramUniform1uiv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLuint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2f'],
  'arguments': 'GLuint program, GLint location, GLfloat v0, GLfloat v1' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2i'],
  'arguments': 'GLuint program, GLint location, GLint v0, GLint v1' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2iv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2ui'],
  'arguments': 'GLuint program, GLint location, GLuint v0, GLuint v1' },
{ 'return_type': 'void',
  'names': ['glProgramUniform2uiv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLuint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3f'],
  'arguments': 'GLuint program, GLint location, GLfloat v0, GLfloat v1, '
               'GLfloat v2' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3i'],
  'arguments': 'GLuint program, GLint location, GLint v0, GLint v1, GLint v2' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3iv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3ui'],
  'arguments': 'GLuint program, GLint location, GLuint v0, GLuint v1, '
               'GLuint v2' },
{ 'return_type': 'void',
  'names': ['glProgramUniform3uiv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLuint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4f'],
  'arguments': 'GLuint program, GLint location, GLfloat v0, GLfloat v1, '
               'GLfloat v2, GLfloat v3' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4i'],
  'arguments': 'GLuint program, GLint location, GLint v0, GLint v1, GLint v2, '
               'GLint v3' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4iv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4ui'],
  'arguments': 'GLuint program, GLint location, GLuint v0, GLuint v1, '
               'GLuint v2, GLuint v3' },
{ 'return_type': 'void',
  'names': ['glProgramUniform4uiv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'const GLuint* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix2fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix2x3fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix2x4fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix3fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix3x2fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix3x4fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix4fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix4x2fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'names': ['glProgramUniformMatrix4x3fv'],
  'arguments': 'GLuint program, GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value' },
{ 'return_type': 'void',
  'versions': [{'name': 'glProvokingVertexANGLE',
                'extensions': ['GL_ANGLE_provoking_vertex']}],
  'arguments': 'GLenum provokeMode', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glPushDebugGroup' },
               { 'name': 'glPushDebugGroupKHR',
                 'extensions': ['GL_KHR_debug'] }],
  'arguments':
    'GLenum source, GLuint id, GLsizei length, const char* message', },
{ 'return_type': 'void',
  'names': ['glPushGroupMarkerEXT'],
  'arguments': 'GLsizei length, const char* marker', },
{ 'return_type': 'void',
  'known_as': 'glQueryCounter',
  'names': ['glQueryCounterEXT'],
  'arguments': 'GLuint id, GLenum target', },
{ 'return_type': 'void',
  'names': ['glReadBuffer'],
  'arguments': 'GLenum src', },
{ 'return_type': 'void',
  'versions': [{'name': 'glReadnPixelsRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, '
      'GLenum type, GLsizei bufSize, GLsizei* length, GLsizei* columns, '
      'GLsizei* rows, void* data', },
{ 'return_type': 'void',
  'names': ['glReadPixels'],
  'arguments':
    'GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, '
    'GLenum type, void* pixels', },
{ 'return_type': 'void',
  'versions': [{'name': 'glReadPixelsRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, '
      'GLenum type, GLsizei bufSize, GLsizei* length, GLsizei* columns, '
      'GLsizei* rows, void* pixels', },
{ 'return_type': 'void',
  'names': ['glReleaseShaderCompiler'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glReleaseTexturesANGLE',
                 'extensions': ['GL_ANGLE_vulkan_image'] }],
  'arguments': 'GLuint numTextures, const GLuint* textures, GLenum* layouts', },
{ 'return_type': 'void',
  'known_as': 'glRenderbufferStorageEXT',
  'names': ['glRenderbufferStorage'],
  'arguments':
      'GLenum target, GLenum internalformat, GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{'name': 'glRenderbufferStorageMultisample'},
               {'name': 'glRenderbufferStorageMultisampleANGLE'},
               {'name': 'glRenderbufferStorageMultisampleEXT',
                'extensions': ['GL_EXT_framebuffer_multisample'],
                'explicit_only': True}],
  'arguments': 'GLenum target, GLsizei samples, GLenum internalformat, '
               'GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{'name': 'glRenderbufferStorageMultisampleAdvancedAMD',
                'extensions': ['GL_AMD_framebuffer_multisample_advanced'] ,
                'explicit_only': True}],
  'arguments': 'GLenum target, GLsizei samples, GLsizei storageSamples, '
               'GLenum internalformat,GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{'name': 'glRenderbufferStorageMultisampleEXT',
                'extensions': ['GL_EXT_multisampled_render_to_texture'],
                'explicit_only': True},
               {'name': 'glRenderbufferStorageMultisampleIMG'}],
  'arguments': 'GLenum target, GLsizei samples, GLenum internalformat, '
               'GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glRequestExtensionANGLE',
                 'extensions': ['GL_ANGLE_request_extension'] }],
  'arguments': 'const char* name', },
{ 'return_type': 'void',
  'names': ['glResumeTransformFeedback'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'names': ['glSampleCoverage'],
  'arguments': 'GLclampf value, GLboolean invert', },
{ 'return_type': 'void',
  'names': ['glSampleMaski'],
  'arguments': 'GLuint maskNumber, GLbitfield mask', },
{ 'return_type': 'void',
  'names': ['glSamplerParameterf'],
  'arguments': 'GLuint sampler, GLenum pname, GLfloat param', },
{ 'return_type': 'void',
  'names': ['glSamplerParameterfv'],
  'arguments': 'GLuint sampler, GLenum pname, const GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glSamplerParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, const GLfloat* param', },
{ 'return_type': 'void',
  'names': ['glSamplerParameteri'],
  'arguments': 'GLuint sampler, GLenum pname, GLint param', },
{ 'return_type': 'void',
  'versions': [{'name': 'glSamplerParameterIivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, const GLint* param', },
{ 'return_type': 'void',
  'versions': [{'name': 'glSamplerParameterIuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, const GLuint* param', },
{ 'return_type': 'void',
  'names': ['glSamplerParameteriv'],
  'arguments': 'GLuint sampler, GLenum pname, const GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glSamplerParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLuint sampler, GLenum pname, GLsizei bufSize, const GLint* param', },
{ 'return_type': 'void',
  'names': ['glScissor'],
  'arguments': 'GLint x, GLint y, GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'names': ['glSetFenceNV'],
  'arguments': 'GLuint fence, GLenum condition', },
{ 'return_type': 'void',
  'names': ['glShaderBinary'],
  'arguments': 'GLsizei n, const GLuint* shaders, GLenum binaryformat, '
               'const void* binary, GLsizei length', },
{ 'return_type': 'void',
  'names': ['glShaderSource'],
  'arguments': 'GLuint shader, GLsizei count, const char* const* str, '
               'const GLint* length', },
{ 'return_type': 'void',
  'names': ['glSignalSemaphoreEXT'],
 'arguments': 'GLuint semaphore, GLuint numBufferBarriers, '
 'const GLuint* buffers, GLuint numTextureBarriers, '
 'const GLuint* textures, const GLenum* dstLayouts', },
{ 'return_type': 'void',
  'names': ['glStartTilingQCOM'],
  'versions': [{ 'name': 'glStartTilingQCOM',
                 'extension': ['GL_QCOM_tiled_rendering'] }],
  'arguments':
      'GLuint x, GLuint y, GLuint width, GLuint height, '
      'GLbitfield preserveMask', },
{ 'return_type': 'void',
  'names': ['glStencilFunc'],
  'arguments': 'GLenum func, GLint ref, GLuint mask', },
{ 'return_type': 'void',
  'names': ['glStencilFuncSeparate'],
  'arguments': 'GLenum face, GLenum func, GLint ref, GLuint mask', },
{ 'return_type': 'void',
  'names': ['glStencilMask'],
  'arguments': 'GLuint mask', },
{ 'return_type': 'void',
  'names': ['glStencilMaskSeparate'],
  'arguments': 'GLenum face, GLuint mask', },
{ 'return_type': 'void',
  'names': ['glStencilOp'],
  'arguments': 'GLenum fail, GLenum zfail, GLenum zpass', },
{ 'return_type': 'void',
  'names': ['glStencilOpSeparate'],
  'arguments': 'GLenum face, GLenum fail, GLenum zfail, GLenum zpass', },
{ 'return_type': 'GLboolean',
  'names': ['glTestFenceNV'],
  'arguments': 'GLuint fence', },
{ 'return_type': 'void',
  'names': ['glTexBuffer', 'glTexBufferOES', 'glTexBufferEXT'],
  'arguments': 'GLenum target, GLenum internalformat, GLuint buffer', } ,
{ 'return_type': 'void',
  'names': ['glTexBufferRange', 'glTexBufferRangeOES', 'glTexBufferRangeEXT'],
  'arguments':
      'GLenum target, GLenum internalformat, GLuint buffer, '
      'GLintptr offset, GLsizeiptr size', },
{ 'return_type': 'void',
  'names': ['glTexImage2D'],
  'arguments':
      'GLenum target, GLint level, GLint internalformat, GLsizei width, '
      'GLsizei height, GLint border, GLenum format, GLenum type, '
      'const void* pixels', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glTexImage2DExternalANGLE',
                 'extensions': ['GL_ANGLE_texture_external_update'] } ],
  'arguments': 'GLenum target, GLint level, GLint internalformat, '
               'GLsizei width, GLsizei height, GLint border, GLenum format, '
               'GLenum type', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexImage2DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint internalformat, GLsizei width, '
      'GLsizei height, GLint border, GLenum format, GLenum type, '
      'GLsizei bufSize, const void* pixels', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glTexImage3D' }],
  'arguments':
      'GLenum target, GLint level, GLint internalformat, GLsizei width, '
      'GLsizei height, GLsizei depth, GLint border, GLenum format, '
      'GLenum type, const void* pixels', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexImage3DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint internalformat, GLsizei width, '
      'GLsizei height, GLsizei depth, GLint border, GLenum format, '
      'GLenum type, GLsizei bufSize, const void* pixels', },
{ 'return_type': 'void',
  'names': ['glTexParameterf'],
  'arguments': 'GLenum target, GLenum pname, GLfloat param', },
{ 'return_type': 'void',
  'names': ['glTexParameterfv'],
  'arguments': 'GLenum target, GLenum pname, const GLfloat* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexParameterfvRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, const GLfloat* params', },
{ 'return_type': 'void',
  'names': ['glTexParameteri'],
  'arguments': 'GLenum target, GLenum pname, GLint param', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexParameterIivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, const GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexParameterIuivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, const GLuint* params', },
{ 'return_type': 'void',
  'names': ['glTexParameteriv'],
  'arguments': 'GLenum target, GLenum pname, const GLint* params', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexParameterivRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLenum pname, GLsizei bufSize, const GLint* params', },
{ 'return_type': 'void',
  'known_as': 'glTexStorage2DEXT',
  'versions': [{ 'name': 'glTexStorage2D' },
               { 'name': 'glTexStorage2DEXT',
                 'extensions': ['GL_EXT_texture_storage'] }],
  'arguments': 'GLenum target, GLsizei levels, GLenum internalformat, '
               'GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'names': ['glTexStorage2DMultisample'],
  'arguments':
      'GLenum target, GLsizei samples, GLenum internalformat, '
      'GLsizei width, GLsizei height, GLboolean fixedsamplelocations', },
{ 'return_type': 'void',
  'names': ['glTexStorage3D'],
  'arguments': 'GLenum target, GLsizei levels, GLenum internalformat, '
               'GLsizei width, GLsizei height, GLsizei depth', },
{ 'return_type': 'void',
 'names': [ 'glTexStorageMem2DEXT'] ,
  'arguments': 'GLenum target, GLsizei levels, GLenum internalFormat, '
  'GLsizei width, GLsizei height, GLuint memory, GLuint64 offset', },
{ 'return_type': 'void',
 'names': [ 'glTexStorageMemFlags2DANGLE'] ,
  'versions': [{ 'name': 'glTexStorageMemFlags2DANGLE',
                 'extensions': ['GL_ANGLE_memory_object_flags'] }],
  'arguments': 'GLenum target, GLsizei levels, GLenum internalFormat, '
  'GLsizei width, GLsizei height, GLuint memory, GLuint64 offset, '
  'GLbitfield createFlags, GLbitfield usageFlags, '
  'const void* imageCreateInfoPNext', },
{ 'return_type': 'void',
  'names': ['glTexSubImage2D'],
  'arguments':
     'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
     'GLsizei width, GLsizei height, GLenum format, GLenum type, '
     'const void* pixels', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexSubImage2DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLsizei width, GLsizei height, GLenum format, GLenum type, '
      'GLsizei bufSize, const void* pixels', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glTexSubImage3D' }],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, '
      'GLenum format, GLenum type, const void* pixels', },
{ 'return_type': 'void',
  'versions': [{'name': 'glTexSubImage3DRobustANGLE',
                'extensions': ['GL_ANGLE_robust_client_memory']}],
  'arguments':
      'GLenum target, GLint level, GLint xoffset, GLint yoffset, '
      'GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, '
      'GLenum format, GLenum type, GLsizei bufSize, const void* pixels', },
{ 'return_type': 'void',
  'names': ['glTransformFeedbackVaryings'],
  'arguments': 'GLuint program, GLsizei count, const char* const* varyings, '
               'GLenum bufferMode', },
{ 'return_type': 'void',
  'names': ['glUniform1f'],
  'arguments': 'GLint location, GLfloat x', },
{ 'return_type': 'void',
  'names': ['glUniform1fv'],
  'arguments': 'GLint location, GLsizei count, const GLfloat* v', },
{ 'return_type': 'void',
  'names': ['glUniform1i'],
  'arguments': 'GLint location, GLint x', },
{ 'return_type': 'void',
  'names': ['glUniform1iv'],
  'arguments': 'GLint location, GLsizei count, const GLint* v', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform1ui' }],
  'arguments': 'GLint location, GLuint v0', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform1uiv' }],
  'arguments': 'GLint location, GLsizei count, const GLuint* v', },
{ 'return_type': 'void',
  'names': ['glUniform2f'],
  'arguments': 'GLint location, GLfloat x, GLfloat y', },
{ 'return_type': 'void',
  'names': ['glUniform2fv'],
  'arguments': 'GLint location, GLsizei count, const GLfloat* v', },
{ 'return_type': 'void',
  'names': ['glUniform2i'],
  'arguments': 'GLint location, GLint x, GLint y', },
{ 'return_type': 'void',
  'names': ['glUniform2iv'],
  'arguments': 'GLint location, GLsizei count, const GLint* v', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform2ui' }],
  'arguments': 'GLint location, GLuint v0, GLuint v1', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform2uiv' }],
  'arguments': 'GLint location, GLsizei count, const GLuint* v', },
{ 'return_type': 'void',
  'names': ['glUniform3f'],
  'arguments': 'GLint location, GLfloat x, GLfloat y, GLfloat z', },
{ 'return_type': 'void',
  'names': ['glUniform3fv'],
  'arguments': 'GLint location, GLsizei count, const GLfloat* v', },
{ 'return_type': 'void',
  'names': ['glUniform3i'],
  'arguments': 'GLint location, GLint x, GLint y, GLint z', },
{ 'return_type': 'void',
  'names': ['glUniform3iv'],
  'arguments': 'GLint location, GLsizei count, const GLint* v', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform3ui' }],
  'arguments': 'GLint location, GLuint v0, GLuint v1, GLuint v2', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform3uiv' }],
  'arguments': 'GLint location, GLsizei count, const GLuint* v', },
{ 'return_type': 'void',
  'names': ['glUniform4f'],
  'arguments': 'GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w', },
{ 'return_type': 'void',
  'names': ['glUniform4fv'],
  'arguments': 'GLint location, GLsizei count, const GLfloat* v', },
{ 'return_type': 'void',
  'names': ['glUniform4i'],
  'arguments': 'GLint location, GLint x, GLint y, GLint z, GLint w', },
{ 'return_type': 'void',
  'names': ['glUniform4iv'],
  'arguments': 'GLint location, GLsizei count, const GLint* v', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform4ui' }],
  'arguments': 'GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniform4uiv' }],
  'arguments': 'GLint location, GLsizei count, const GLuint* v', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformBlockBinding' }],
  'arguments': 'GLuint program, GLuint uniformBlockIndex, '
               'GLuint uniformBlockBinding', },
{ 'return_type': 'void',
  'names': ['glUniformMatrix2fv'],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix2x3fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix2x4fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'names': ['glUniformMatrix3fv'],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix3x2fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix3x4fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'names': ['glUniformMatrix4fv'],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix4x2fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glUniformMatrix4x3fv' }],
  'arguments': 'GLint location, GLsizei count, '
               'GLboolean transpose, const GLfloat* value', },
{ 'return_type': 'GLboolean',
  'known_as': 'glUnmapBuffer',
  'names': ['glUnmapBufferOES', 'glUnmapBuffer'],
  'arguments': 'GLenum target', },
{ 'return_type': 'void',
  'names': ['glUseProgram'],
  'arguments': 'GLuint program', },
{ 'return_type': 'void',
  'names': ['glUseProgramStages'],
  'arguments': 'GLuint pipeline, GLbitfield stages, GLuint program', },
{ 'return_type': 'void',
  'names': ['glValidateProgram'],
  'arguments': 'GLuint program', },
{ 'return_type': 'void',
  'names': ['glValidateProgramPipeline'],
  'arguments': 'GLuint pipeline', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib1f'],
  'arguments': 'GLuint indx, GLfloat x', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib1fv'],
  'arguments': 'GLuint indx, const GLfloat* values', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib2f'],
  'arguments': 'GLuint indx, GLfloat x, GLfloat y', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib2fv'],
  'arguments': 'GLuint indx, const GLfloat* values', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib3f'],
  'arguments': 'GLuint indx, GLfloat x, GLfloat y, GLfloat z', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib3fv'],
  'arguments': 'GLuint indx, const GLfloat* values', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib4f'],
  'arguments': 'GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w', },
{ 'return_type': 'void',
  'names': ['glVertexAttrib4fv'],
  'arguments': 'GLuint indx, const GLfloat* values', },
{ 'return_type': 'void',
  'names': ['glVertexAttribBinding'],
  'arguments': 'GLuint attribindex, GLuint bindingindex', },
{ 'return_type': 'void',
  'known_as': 'glVertexAttribDivisorANGLE',
  'names': ['glVertexAttribDivisorANGLE', 'glVertexAttribDivisorEXT',
            'glVertexAttribDivisor'],
  'arguments':
      'GLuint index, GLuint divisor', },
{ 'return_type': 'void',
  'names': ['glVertexAttribFormat'],
  'arguments': 'GLuint attribindex, GLint size, GLenum type, '
               'GLboolean normalized, GLuint relativeoffset', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glVertexAttribI4i' }],
  'arguments': 'GLuint indx, GLint x, GLint y, GLint z, GLint w', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glVertexAttribI4iv' }],
  'arguments': 'GLuint indx, const GLint* values', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glVertexAttribI4ui' }],
  'arguments': 'GLuint indx, GLuint x, GLuint y, GLuint z, GLuint w', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glVertexAttribI4uiv' }],
  'arguments': 'GLuint indx, const GLuint* values', },
{ 'return_type': 'void',
  'names': ['glVertexAttribIFormat'],
  'arguments': 'GLuint attribindex, GLint size, GLenum type, '
               'GLuint relativeoffset', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'glVertexAttribIPointer' }],
  'arguments': 'GLuint indx, GLint size, GLenum type, GLsizei stride, '
               'const void* ptr', },
{ 'return_type': 'void',
  'names': ['glVertexAttribPointer'],
  'arguments': 'GLuint indx, GLint size, GLenum type, GLboolean normalized, '
               'GLsizei stride, const void* ptr', },
{ 'return_type': 'void',
  'names': ['glVertexBindingDivisor'],
  'arguments': 'GLuint bindingindex, GLuint divisor', },
{ 'return_type': 'void',
  'names': ['glViewport'],
  'arguments': 'GLint x, GLint y, GLsizei width, GLsizei height', },
{ 'return_type': 'void',
  'names': ['glWaitSemaphoreEXT'],
  'arguments': 'GLuint semaphore, GLuint numBufferBarriers, '
  'const GLuint* buffers, GLuint numTextureBarriers, const GLuint* textures, '
  'const GLenum* srcLayouts', },
{ 'return_type': 'void',
  'names': ['glWaitSync'],
  'arguments':
    'GLsync sync, GLbitfield flags, GLuint64 timeout', },
{ 'return_type': 'void',
  'names': ['glWindowRectanglesEXT'],
  'arguments': 'GLenum mode, GLsizei n, const GLint* box', },
]

EGL_FUNCTIONS = [
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglAcquireExternalContextANGLE',
                 'extensions': ['EGL_ANGLE_external_context_and_surface'] }],
  'arguments': 'EGLDisplay dpy, EGLSurface readAndDraw' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglBindAPI'],
  'arguments': 'EGLenum api', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglBindTexImage'],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLint buffer', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglChooseConfig'],
  'arguments': 'EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, '
               'EGLint config_size, EGLint* num_config', },
{ 'return_type': 'EGLint',
  'names': ['eglClientWaitSync'],
  'arguments': 'EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout' },
{ 'return_type': 'EGLint',
  'versions': [{ 'name': 'eglClientWaitSyncKHR',
                 'extensions': [
                   'EGL_KHR_fence_sync',
                   'GL_CHROMIUM_egl_khr_fence_sync_hack'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, '
      'EGLTimeKHR timeout' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglCopyBuffers'],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target', },
{ 'return_type': 'void*',
  'versions': [{ 'name': 'eglCopyMetalSharedEventANGLE',
                 'extensions': [
                   'EGL_ANGLE_metal_shared_event_sync',
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSync sync', },
{ 'return_type': 'EGLContext',
  'names': ['eglCreateContext'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, EGLContext share_context, '
              'const EGLint* attrib_list', },
{ 'return_type': 'EGLImage',
  'names': ['eglCreateImage'],
  'arguments':
      'EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, '
      'const EGLAttrib* attrib_list' },
{ 'return_type': 'EGLImageKHR',
  'versions': [{ 'name': 'eglCreateImageKHR',
                 'extensions':
                     ['EGL_KHR_image_base', 'EGL_KHR_gl_texture_2D_image'] }],
  'arguments':
      'EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, '
      'const EGLint* attrib_list' },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreatePbufferFromClientBuffer'],
  'arguments':
      'EGLDisplay dpy, EGLenum buftype, void* buffer, EGLConfig config, '
      'const EGLint* attrib_list', },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreatePbufferSurface'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list', },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreatePixmapSurface'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, '
               'const EGLint* attrib_list', },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreatePlatformPixmapSurface'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, void* native_pixmap, '
               'const EGLAttrib* attrib_list', },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreatePlatformWindowSurface'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, void* native_window, '
               'const EGLAttrib* attrib_list', },
{ 'return_type': 'EGLStreamKHR',
  'versions': [{ 'name': 'eglCreateStreamKHR',
                 'extensions': ['EGL_KHR_stream'] }],
  'arguments': 'EGLDisplay dpy, const EGLint* attrib_list' },
{ 'return_type': 'EGLBoolean',
    'versions': [{'name': 'eglCreateStreamProducerD3DTextureANGLE',
                  'extensions':
                      ['EGL_ANGLE_stream_producer_d3d_texture']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, EGLAttrib* attrib_list', },
{ 'return_type': 'EGLSync',
  'names': ['eglCreateSync'],
  'arguments': 'EGLDisplay dpy, EGLenum type, const EGLAttrib* attrib_list' },
{ 'return_type': 'EGLSyncKHR',
  'versions': [{ 'name': 'eglCreateSyncKHR',
                 'extensions': [
                   'EGL_KHR_fence_sync',
                   'GL_CHROMIUM_egl_khr_fence_sync_hack'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLenum type, const EGLint* attrib_list' },
{ 'return_type': 'EGLSurface',
  'names': ['eglCreateWindowSurface'],
  'arguments': 'EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, '
               'const EGLint* attrib_list', },
{ 'return_type': 'EGLint',
  'known_as': 'eglDebugMessageControlKHR',
  'versions': [{ 'name': 'eglDebugMessageControlKHR',
                 'client_extensions': ['EGL_KHR_debug'], }],
  'arguments': 'EGLDEBUGPROCKHR callback, const EGLAttrib* attrib_list', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglDestroyContext'],
  'arguments': 'EGLDisplay dpy, EGLContext ctx', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglDestroyImage'],
  'arguments': 'EGLDisplay dpy, EGLImage image' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name' : 'eglDestroyImageKHR',
                 'extensions': ['EGL_KHR_image_base'] }],
  'arguments': 'EGLDisplay dpy, EGLImageKHR image' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglDestroyStreamKHR',
                 'extensions': ['EGL_KHR_stream'] }],
  'arguments': 'EGLDisplay dpy, EGLStreamKHR stream' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglDestroySurface'],
  'arguments': 'EGLDisplay dpy, EGLSurface surface', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglDestroySync'],
  'arguments': 'EGLDisplay dpy, EGLSync sync' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglDestroySyncKHR',
                 'extensions': [
                   'EGL_KHR_fence_sync',
                   'GL_CHROMIUM_egl_khr_fence_sync_hack'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSyncKHR sync' },
{ 'return_type': 'EGLint',
  # At least some Android O devices such as Pixel implement this
  # but don't export the EGL_ANDROID_native_fence_sync extension.
  'versions': [{ 'name': 'eglDupNativeFenceFDANDROID',
                 'extensions': [
                     'EGL_ANDROID_native_fence_sync',
                     'GL_CHROMIUM_egl_android_native_fence_sync_hack']}],
  'arguments':
      'EGLDisplay dpy, EGLSyncKHR sync' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglExportDMABUFImageMESA',
                 'extensions': ['EGL_MESA_image_dma_buf_export'] }],
  'arguments': 'EGLDisplay dpy, EGLImageKHR image, int* fds, EGLint* strides, '
               'EGLint* offsets', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglExportDMABUFImageQueryMESA',
                 'extensions': ['EGL_MESA_image_dma_buf_export'] }],
  'arguments': 'EGLDisplay dpy, EGLImageKHR image, int* fourcc, '
               'int* num_planes, EGLuint64KHR* modifiers', },
{ 'return_type': 'EGLBoolean',
    'versions': [{'name': 'eglExportVkImageANGLE',
                  'extensions':
                      ['EGL_ANGLE_vulkan_image']}],
  'arguments': 'EGLDisplay dpy, EGLImageKHR image, void* vk_image, '
               'void* vk_image_create_info', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetCompositorTimingANDROID',
                 'extensions': [
                   'EGL_ANDROID_get_frame_timestamps'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLint numTimestamps, '
               'EGLint* names, EGLnsecsANDROID* values', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetCompositorTimingSupportedANDROID',
                 'extensions': [
                   'EGL_ANDROID_get_frame_timestamps'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLint timestamp', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglGetConfigAttrib'],
  'arguments':
      'EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglGetConfigs'],
  'arguments': 'EGLDisplay dpy, EGLConfig* configs, EGLint config_size, '
               'EGLint* num_config', },
{ 'return_type': 'EGLContext',
  'names': ['eglGetCurrentContext'],
  'arguments': 'void', },
{ 'return_type': 'EGLDisplay',
  'names': ['eglGetCurrentDisplay'],
  'arguments': 'void', },
{ 'return_type': 'EGLSurface',
  'names': ['eglGetCurrentSurface'],
  'arguments': 'EGLint readdraw', },
{ 'return_type': 'EGLDisplay',
  'names': ['eglGetDisplay'],
  'arguments': 'EGLNativeDisplayType display_id', },
{ 'return_type': 'EGLint',
  'names': ['eglGetError'],
  'arguments': 'void', },
 { 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetFrameTimestampsANDROID',
                 'extensions': [
                   'EGL_ANDROID_get_frame_timestamps'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLuint64KHR frameId, '
               'EGLint numTimestamps, EGLint* timestamps, '
               'EGLnsecsANDROID* values', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetFrameTimestampSupportedANDROID',
                 'extensions': [
                   'EGL_ANDROID_get_frame_timestamps'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLint timestamp', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetMscRateANGLE',
                 'extensions': [
                   'EGL_ANGLE_sync_control_rate'
                 ] }],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, '
      'EGLint* numerator, EGLint* denominator', },
{ 'return_type': 'EGLClientBuffer',
  'versions': [{ 'name': 'eglGetNativeClientBufferANDROID',
                 'extensions': ['EGL_ANDROID_get_native_client_buffer'], }],
  'arguments': 'const struct AHardwareBuffer* ahardwarebuffer', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetNextFrameIdANDROID',
                 'extensions': [
                   'EGL_ANDROID_get_frame_timestamps'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLuint64KHR* frameId', },
{ 'return_type': 'EGLDisplay',
  'names': ['eglGetPlatformDisplay'],
  'arguments': 'EGLenum platform, void* native_display, '
               'const EGLAttrib* attrib_list', },
{ 'return_type': '__eglMustCastToProperFunctionPointerType',
  'names': ['eglGetProcAddress'],
  'arguments': 'const char* procname',
  'logging_code': """
  GL_SERVICE_LOG("GL_RESULT: " << reinterpret_cast<void*>(result));
""", },
{ 'return_type': 'EGLBoolean',
  'names': ['eglGetSyncAttrib'],
  'arguments': 'EGLDisplay dpy, EGLSync sync, EGLint attribute, '
      'EGLAttrib* value' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetSyncAttribKHR',
                 'extensions': [
                   'EGL_KHR_fence_sync',
                   'GL_CHROMIUM_egl_khr_fence_sync_hack'
                 ] }],
  'arguments': 'EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, '
      'EGLint* value' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglGetSyncValuesCHROMIUM',
                 'extensions': [
                   'EGL_CHROMIUM_sync_control'
                 ] }],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, '
      'EGLuint64CHROMIUM* ust, EGLuint64CHROMIUM* msc, '
      'EGLuint64CHROMIUM* sbc', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglHandleGPUSwitchANGLE',
                 'extensions': ['EGL_ANGLE_power_preference'] }],
  'arguments': 'EGLDisplay dpy' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglImageFlushExternalEXT',
                 'extensions': ['EGL_EXT_image_flush_external'] }],
  'arguments':
      'EGLDisplay dpy, EGLImageKHR image, const EGLAttrib* attrib_list' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglInitialize'],
  'arguments': 'EGLDisplay dpy, EGLint* major, EGLint* minor', },
{ 'return_type': 'EGLint',
  'known_as': 'eglLabelObjectKHR',
  'versions': [{ 'name': 'eglLabelObjectKHR',
                 'client_extensions': ['EGL_KHR_debug'], }],
  'arguments': 'EGLDisplay display, EGLenum objectType, EGLObjectKHR object, '
    'EGLLabelKHR label', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglMakeCurrent'],
  'arguments':
      'EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglPostSubBufferNV'],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, '
    'EGLint x, EGLint y, EGLint width, EGLint height', },
{ 'return_type': 'EGLenum',
  'names': ['eglQueryAPI'],
  'arguments': 'void', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglQueryContext'],
  'arguments':
      'EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value', },
{ 'return_type': 'EGLBoolean',
  'known_as': 'eglQueryDebugKHR',
  'versions': [{ 'name': 'eglQueryDebugKHR',
                 'client_extensions': ['EGL_KHR_debug'], }],
  'arguments': 'EGLint attribute, EGLAttrib* value', },
{
  'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryDeviceAttribEXT',
                 'client_extensions': ['EGL_EXT_device_query'], }],
  'arguments': 'EGLDeviceEXT device, EGLint attribute, EGLAttrib* value' },
{ 'return_type': 'EGLBoolean',
  'known_as': 'eglQueryDevicesEXT',
  'versions': [{ 'name': 'eglQueryDevicesEXT',
                 'client_extensions': ['EGL_EXT_device_enumeration'], }],
  'arguments':
      'EGLint max_devices, EGLDeviceEXT* devices, EGLint* num_devices', },
{ 'return_type': 'const char *',
  'known_as': 'eglQueryDeviceStringEXT',
  'versions': [{ 'name': 'eglQueryDeviceStringEXT',
                 'client_extensions': ['EGL_EXT_device_query'], }],
  'arguments': 'EGLDeviceEXT device, EGLint name', },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryDisplayAttribANGLE',
                 'client_extensions': ['EGL_ANGLE_feature_control'] }],
  'arguments': 'EGLDisplay dpy, EGLint attribute, EGLAttrib* value' },
{
  'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryDisplayAttribEXT',
                 'client_extensions': ['EGL_EXT_device_query'], }],
  'arguments': 'EGLDisplay dpy, EGLint attribute, EGLAttrib* value' },
{
  'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryDmaBufFormatsEXT',
                 'extensions':
                     ['EGL_EXT_image_dma_buf_import_modifiers'], }],
  'arguments':
      'EGLDisplay dpy, EGLint max_formats, '
      'EGLint* formats, EGLint* num_formats' },
{
  'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryDmaBufModifiersEXT',
                 'extensions':
                     ['EGL_EXT_image_dma_buf_import_modifiers'], }],
  'arguments':
      'EGLDisplay dpy, EGLint format, EGLint max_modifiers, '
      'EGLuint64KHR* modifiers, EGLBoolean* external_only, '
      'EGLint* num_modifiers' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryStreamKHR',
                 'extensions': ['EGL_KHR_stream'] }],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, '
      'EGLint* value' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglQueryStreamu64KHR',
                 'extensions': ['EGL_KHR_stream'] }],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, '
      'EGLuint64KHR* value' },
{ 'return_type': 'const char*',
  'names': ['eglQueryString'],
  'arguments': 'EGLDisplay dpy, EGLint name', },
{ 'return_type': 'const char *',
  'versions': [{ 'name': 'eglQueryStringiANGLE',
                 'client_extensions': ['EGL_ANGLE_feature_control'] }],
  'arguments': 'EGLDisplay dpy, EGLint name, EGLint index' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglQuerySurface'],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglQuerySurfacePointerANGLE'],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, EGLint attribute, void** value', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglReacquireHighPowerGPUANGLE',
                 'extensions': ['EGL_ANGLE_power_preference'] }],
  'arguments': 'EGLDisplay dpy, EGLContext ctx' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglReleaseExternalContextANGLE',
                 'extensions': ['EGL_ANGLE_external_context_and_surface'] }],
  'arguments': 'EGLDisplay dpy' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglReleaseHighPowerGPUANGLE',
                 'extensions': ['EGL_ANGLE_power_preference'] }],
  'arguments': 'EGLDisplay dpy, EGLContext ctx' },
{ 'return_type': 'EGLBoolean',
  'names': ['eglReleaseTexImage'],
  'arguments': 'EGLDisplay dpy, EGLSurface surface, EGLint buffer', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglReleaseThread'],
  'arguments': 'void', },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglSetBlobCacheFuncsANDROID',
                 'extensions': ['EGL_ANDROID_blob_cache'] }],
  'arguments':
      'EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglSetValidationEnabledANGLE',
                 'extensions': ['EGL_ANGLE_no_error'] }],
  'arguments':
      'EGLBoolean validationState' },
{ 'return_type': 'EGLBoolean',
  'versions': [{ 'name': 'eglStreamAttribKHR',
                 'extensions': ['EGL_KHR_stream'] }],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value' },
{ 'return_type': 'EGLBoolean',
    'versions': [{ 'name': 'eglStreamConsumerAcquireKHR',
                   'extensions': ['EGL_KHR_stream_consumer_gltexture']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream', },
{ 'return_type': 'EGLBoolean',
    'versions': [{ 'name': 'eglStreamConsumerGLTextureExternalAttribsNV',
                   'extensions': ['EGL_NV_stream_consumer_gltexture_yuv']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, EGLAttrib* attrib_list', },
{ 'return_type': 'EGLBoolean',
    'versions': [{ 'name': 'eglStreamConsumerGLTextureExternalKHR',
                   'extensions': ['EGL_KHR_stream_consumer_gltexture']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream', },
{ 'return_type': 'EGLBoolean',
    'versions': [{ 'name': 'eglStreamConsumerReleaseKHR',
                   'extensions': ['EGL_KHR_stream_consumer_gltexture']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream', },
{ 'return_type': 'EGLBoolean',
    'versions': [{ 'name': 'eglStreamPostD3DTextureANGLE',
                   'extensions':
                       ['EGL_ANGLE_stream_producer_d3d_texture']}],
  'arguments':
      'EGLDisplay dpy, EGLStreamKHR stream, void* texture, '
      'const EGLAttrib* attrib_list', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglSurfaceAttrib'],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglSwapBuffers'],
  'arguments': 'EGLDisplay dpy, EGLSurface surface', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglSwapBuffersWithDamageKHR'],
  'arguments':
      'EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglSwapInterval'],
  'arguments': 'EGLDisplay dpy, EGLint interval', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglTerminate'],
  'arguments': 'EGLDisplay dpy', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglWaitClient'],
  'arguments': 'void', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglWaitGL'],
  'arguments': 'void', },
{ 'return_type': 'EGLBoolean',
  'names': ['eglWaitNative'],
  'arguments': 'EGLint engine', },
{ 'return_type': 'EGLint',
  'names': ['eglWaitSync'],
  'arguments': 'EGLDisplay dpy, EGLSync sync, EGLint flags' },
{ 'return_type': 'EGLint',
  'versions': [{ 'name': 'eglWaitSyncKHR',
                 'extensions': ['EGL_KHR_wait_sync'] }],
  'arguments': 'EGLDisplay dpy, EGLSyncKHR sync, EGLint flags' },
{ 'return_type': 'void',
  'versions': [{ 'name': 'eglWaitUntilWorkScheduledANGLE',
                 'extensions': ['EGL_ANGLE_wait_until_work_scheduled'] }],
  'arguments': 'EGLDisplay dpy' },
]

# EGL client extensions that may not add a function but are still queried.
EGL_CLIENT_EXTENSIONS_EXTRA = [
  'EGL_ANGLE_display_power_preference',
  'EGL_ANGLE_no_error',
  'EGL_ANGLE_platform_angle',
  'EGL_ANGLE_platform_angle_d3d',
  'EGL_ANGLE_platform_angle_device_id',
  'EGL_ANGLE_platform_angle_device_type_egl_angle',
  'EGL_ANGLE_platform_angle_device_type_swiftshader',
  'EGL_ANGLE_platform_angle_metal',
  'EGL_ANGLE_platform_angle_null',
  'EGL_ANGLE_platform_angle_opengl',
  'EGL_ANGLE_platform_angle_vulkan',
  'EGL_EXT_platform_device',
  'EGL_MESA_platform_surfaceless',
]

# EGL extensions that may not add a function but are still queried.
EGL_EXTENSIONS_EXTRA = [
  'EGL_ANDROID_create_native_client_buffer',
  'EGL_ANDROID_front_buffer_auto_refresh',
  'EGL_ANGLE_display_semaphore_share_group',
  'EGL_ANGLE_display_texture_share_group',
  'EGL_ANGLE_context_virtualization',
  'EGL_ANGLE_create_context_backwards_compatible',
  'EGL_ANGLE_create_context_client_arrays',
  'EGL_ANGLE_create_context_webgl_compatibility',
  'EGL_ANGLE_global_fence_sync',
  'EGL_ANGLE_iosurface_client_buffer',
  'EGL_ANGLE_keyed_mutex',
  'EGL_ANGLE_robust_resource_initialization',
  'EGL_ANGLE_surface_orientation',
  'EGL_ANGLE_window_fixed_size',
  'EGL_ARM_implicit_external_sync',
  'EGL_CHROMIUM_create_context_bind_generates_resource',
  'EGL_EXT_create_context_robustness',
  'EGL_EXT_gl_colorspace_display_p3',
  'EGL_EXT_gl_colorspace_display_p3_passthrough',
  'EGL_EXT_image_dma_buf_import',
  'EGL_EXT_pixel_format_float',
  'EGL_IMG_context_priority',
  'EGL_KHR_create_context',
  'EGL_KHR_gl_colorspace',
  'EGL_KHR_no_config_context',
  'EGL_KHR_surfaceless_context',
  'EGL_NV_robustness_video_memory_purge',
  'EGL_NOK_texture_from_pixmap',
]

FUNCTION_SETS = [
  [GL_FUNCTIONS, 'gl', [
      'GLES2/gl2ext.h',
      'GLES3/gl3.h',
      'GLES3/gl31.h',
      'GLES3/gl32.h',
    ], [
      "GL_ANGLE_renderability_validation",
      "GL_ANGLE_robust_resource_initialization",
      "GL_ANGLE_webgl_compatibility",
      "GL_EXT_texture_swizzle",
      "GL_EXT_texture_format_BGRA8888",
      "GL_EXT_unpack_subimage",
    ]
  ],
  [EGL_FUNCTIONS, 'egl', [
      'EGL/eglext.h',
    ],
    [
      'EGL_ANGLE_d3d_share_handle_client_buffer',
      'EGL_ANGLE_surface_d3d_texture_2d_share_handle',
    ],
  ],
]

GLES2_HEADERS_WITH_ENUMS = [
  'GLES2/gl2.h',
  'GLES2/gl2ext.h',
  'GLES2/gl2chromium.h',
  'GLES2/gl2extchromium.h',
  'GLES3/gl3.h',
  'GLES3/gl31.h',
]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

GLVersion = namedtuple('GLVersion', 'is_es major_version minor_version')

def GLVersionBindAlways(version):
  assert version.is_es
  return version.major_version <= 2


def GetStaticBinding(func):
  """If this function has a name assigned to it that should  be bound always,
  then return this name.

  This will be the case if either a function name is specified
  that depends on an extension from UNCONDITIONALLY_BOUND_EXTENSIONS,
  or if the GL version it depends on is assumed to be available (e.g. <=2.1).
  There can only be one name that satisfies this condition (or the bindings
  would be ambiguous)."""

  static_bindings = set([])

  for version in func['versions']:
    if 'extensions' in version:
      extensions = version['extensions']
      num_unconditional_extensions = len(
          extensions & UNCONDITIONALLY_BOUND_EXTENSIONS)
      if num_unconditional_extensions:
        static_bindings.add(version['name'])
    elif 'gl_versions' in version:
      versions = [v for v in version['gl_versions'] if GLVersionBindAlways(v)]
      if len(versions) == 1:
        static_bindings.add(version['name'])
    else:
        static_bindings.add(version['name'])

  # Avoid ambiguous bindings (static binding with different names)
  assert len(static_bindings) <= 1
  if len(static_bindings):
    static_name = static_bindings.pop()
    # Avoid ambiguous bindings (static and dynamic bindings with
    # different names)
    assert len([v['name'] for v in func['versions']
               if v['name'] != static_name]) == 0, func
    return static_name
  else:
    return None


def GenerateHeader(file, functions, set_name,
                   used_extensions, used_client_extensions):
  """Generates gl_bindings_autogen_x.h"""

  # Write file header.
  file.write(LICENSE_AND_HEADER +
"""

#ifndef UI_GL_GL_BINDINGS_AUTOGEN_%(name)s_H_
#define UI_GL_GL_BINDINGS_AUTOGEN_%(name)s_H_

#include <string>

namespace gl {

class GLContext;
""" % {'name': set_name.upper()})

  # Write typedefs for function pointer types. Always use the GL name for the
  # typedef.
  file.write('\n')
  for func in functions:
    file.write('typedef %s (GL_BINDING_CALL *%sProc)(%s);\n' %
        (func['return_type'], func['known_as'], func['arguments']))

  # Write declarations for booleans indicating which extensions are available.
  file.write('\n')
  if set_name == 'egl':
    file.write('struct GL_EXPORT ClientExtensionsEGL {\n')
    for extension in sorted(used_client_extensions):
      file.write('  bool b_%s;\n' % extension)
    file.write(
"""

  void InitializeClientExtensionSettings();

 private:
  static std::string GetClientExtensions();
};

struct GL_EXPORT DisplayExtensionsEGL {
""")
  else:
    assert len(used_client_extensions) == 0
    file.write("struct Extensions%s {\n" % set_name.upper())

  for extension in sorted(used_extensions):
    file.write('  bool b_%s;\n' % extension)
  if set_name == 'egl':
    file.write(
"""

  void InitializeExtensionSettings(EGLDisplay display);
  void UpdateConditionalExtensionSettings(EGLDisplay display);

  static std::string GetPlatformExtensions(EGLDisplay display);
""")
  file.write('};\n')
  file.write('\n')

  # Write Procs struct.
  file.write("struct Procs%s {\n" % set_name.upper())
  for func in functions:
    file.write('  %sProc %sFn;\n' % (func['known_as'], func['known_as']))
  file.write('};\n')
  file.write('\n')

  # Write Api class.
  file.write(
"""class GL_EXPORT %(name)sApi {
 public:
  %(name)sApi();
  virtual ~%(name)sApi();

  virtual void SetDisabledExtensions(
      const std::string& disabled_extensions) {}

""" % {'name': set_name.upper()})
  for func in functions:
    file.write('  virtual %s %sFn(%s) = 0;\n' %
      (func['return_type'], func['known_as'], func['arguments']))
  file.write('};\n')
  file.write('\n')

  file.write( '}  // namespace gl\n')

  # Write macros to invoke function pointers. Always use the GL name for the
  # macro.
  file.write('\n')
  for func in functions:
    file.write('#define %s ::gl::g_current_%s_context->%sFn\n' %
        (func['known_as'], set_name.lower(), func['known_as']))

  file.write('\n')
  file.write('#endif  // UI_GL_GL_BINDINGS_AUTOGEN_%s_H_\n' %
      set_name.upper())


def GenerateAPIHeader(file, functions, set_name):
  """Generates gl_bindings_api_autogen_x.h"""

  # Write file header.
  file.write(LICENSE_AND_HEADER +
"""

// Silence presubmit and Tricium warnings about include guards
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

""")

  # Write API declaration.
  for func in functions:
    file.write('  %s %sFn(%s) override;\n' %
      (func['return_type'], func['known_as'], func['arguments']))

  file.write('\n')


def GenerateMockHeader(file, functions, set_name):
  """Generates gl_mock_autogen_x.h"""

  # Write file header.
  file.write(LICENSE_AND_HEADER +
"""

// Silence presubmit and Tricium warnings about include guards
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

""")

  # Write API declaration.
  for func in functions:
    args = func['arguments']
    if args == 'void':
      args = ''
    arg_count = 0
    if len(args):
      arg_count = func['arguments'].count(',') + 1
    # TODO(zmo): crbug.com/456340
    # For now gmock supports at most 10 args.
    if arg_count <= 10:
      file.write('  MOCK_METHOD%d(%s, %s(%s));\n' %
          (arg_count, func['known_as'][len(set_name):], func['return_type'],
           args))
    else:
      file.write('  // TODO(zmo): crbug.com/456340\n')
      file.write('  // %s cannot be mocked because it has %d args.\n' %
          (func['known_as'], arg_count))

  file.write('\n')

def GenerateStubHeader(file, functions):
  """Generates gl_stub_autogen_gl.h"""

  # Write file header.
  file.write(LICENSE_AND_HEADER +
"""

#ifndef UI_GL_GL_STUB_AUTOGEN_GL_H_
#define UI_GL_GL_STUB_AUTOGEN_GL_H_

""")


  # Write API declaration.
  for func in functions:
    args = func['arguments']
    if args == 'void':
      args = ''
    return_type = func['return_type'];
    file.write('  %s gl%sFn(%s) override' % (return_type, func['known_as'][2:],
                                             args))
    if return_type == 'void':
      file.write(' {}\n');
    else:
      file.write(';\n');

  file.write('\n')
  file.write('#endif  //  UI_GL_GL_STUB_AUTOGEN_GL_H_')

def GenerateStubSource(file, functions):
  """Generates gl_stub_autogen_gl.cc"""

  # Write file header.
  file.write(LICENSE_AND_HEADER)
  file.write('\n#include "ui/gl/gl_stub_api_base.h"\n\n')
  file.write('namespace gl {\n\n')

  # Write API declaration.
  for func in functions:
    return_type = func['return_type'];
    if return_type == 'void':
      continue
    args = func['arguments']
    if args == 'void':
      args = ''
    file.write('%s GLStubApiBase::gl%sFn(%s) {\n' % (return_type,
                                                     func['known_as'][2:],
                                                     args))
    file.write('  return 0;\n');
    file.write('}\n\n');

  file.write('\n}  // namespace gl\n')


def GenerateSource(file, functions, set_name, used_extensions,
                   used_client_extensions, options):
  """Generates gl_bindings_autogen_x.cc"""

  set_header_name = "ui/gl/gl_" + set_name.lower() + "_api_implementation.h"
  include_list = [ 'base/containers/span.h',
                   'base/trace_event/trace_event.h',
                   'ui/gl/gl_enums.h',
                   'ui/gl/gl_bindings.h',
                   'ui/gl/gl_context.h',
                   'ui/gl/gl_implementation.h',
                   'ui/gl/gl_version_info.h',
                   set_header_name ]

  includes_string = "\n".join(["#include \"{0}\"".format(h)
                               for h in sorted(include_list)])

  # Write file header.
  file.write(LICENSE_AND_HEADER +
"""

#include <string>

%s

namespace gl {
""" % includes_string)

  file.write('\n')
  if set_name != 'gl':
    file.write('Driver%s g_driver_%s;  // Exists in .bss\n' % (
        set_name.upper(), set_name.lower()))
  file.write('\n')

  # Write stub functions that take the place of some functions before a context
  # is initialized. This is done to provide clear asserts on debug build and to
  # avoid crashing in case of a bug on release build.
  file.write('\n')
  num_dynamic = 0
  for func in functions:
    static_binding = GetStaticBinding(func)
    if static_binding:
      func['static_binding'] = static_binding
    else:
      num_dynamic = num_dynamic + 1

  print("[%s] %d static bindings, %d dynamic bindings" % (
      set_name, len(functions) - num_dynamic, num_dynamic))

  # Write function to initialize the function pointers that are always the same
  # and to initialize bindings where choice of the function depends on the
  # extension string or the GL version to point to stub functions.
  file.write('\n')
  file.write('void Driver%s::InitializeStaticBindings() {\n' %
             set_name.upper())
  file.write('#if DCHECK_IS_ON()\n')
  file.write('  // Ensure struct has been zero-initialized.\n')
  file.write('  auto bytes = base::byte_span_from_ref(*this);\n')
  file.write('  for (auto byte : bytes) {\n');
  file.write('    DCHECK_EQ(0, byte);\n')
  file.write('  };\n')
  file.write('#endif\n')
  file.write('\n')

  def BindingsAreAllStatic(api_set_name):
    return api_set_name == 'egl'

  def WriteFuncBinding(file, known_as, version_name):
    file.write(
        '  fn.%sFn = reinterpret_cast<%sProc>(GetGLProcAddress("%s"));\n' %
        (known_as, known_as, version_name))

  for func in functions:
    if 'static_binding' in func:
      WriteFuncBinding(file, func['known_as'], func['static_binding'])
    elif BindingsAreAllStatic(set_name):
      assert len(func['versions']) == 1
      version = func['versions'][0]
      WriteFuncBinding(file, func['known_as'], version['name'])

  def GetGLVersionCondition(gl_version):
    assert gl_version.is_es
    if GLVersionBindAlways(gl_version):
      return 'True'
    else:
      return 'ver->IsAtLeastGLES(%du, %du)' % (
          gl_version.major_version, gl_version.minor_version)

  def GetBindingCondition(version):
    conditions = []
    if 'gl_versions' in version:
      conditions.extend(
          sorted([GetGLVersionCondition(v) for v in version['gl_versions']]))
    if 'extensions' in version and version['extensions']:
      conditions.extend(
          sorted(['ext.b_%s' % e for e in version['extensions']]))
    return ' || '.join(conditions)

  def WriteConditionalFuncBinding(file, func):
    assert len(func['versions']) > 0
    known_as = func['known_as']
    i = 0
    first_version = True
    while i < len(func['versions']):
      version = func['versions'][i]
      cond = GetBindingCondition(version)
      if first_version:
        if cond == 'True':
          file.write('  {\n  ')
        elif cond != '':
          file.write('  if (%s) {\n  ' % cond)
        else:
          i += 1
          continue
      else:
        if cond == 'True':
          file.write('  else {\n  ')
        elif cond != '':
          file.write('  else if (%s) {\n  ' % (cond))
        else:
          i += 1
          continue

      WriteFuncBinding(file, known_as, version['name'])
      if options.validate_bindings:
        if not 'is_optional' in func or not func['is_optional']:
          file.write('DCHECK(fn.%sFn);\n' % known_as)
      file.write('}\n')
      if cond == 'True':
        break
      i += 1
      if cond != '':
        first_version = False

  # TODO(jmadill): make more robust
  def IsClientExtensionFunc(func):
    assert len(func['versions']) > 0
    if 'client_extensions' in func['versions'][0]:
      assert len(func['versions']) == 1
      return True
    return False

  file.write("}\n\n");

  if set_name == 'gl':
    file.write("""\
void DriverGL::InitializeDynamicBindings(const GLVersionInfo* ver,
                                         const gfx::ExtensionSet& extensions) {
""")
  elif set_name == 'egl':
    file.write("""\
void ClientExtensionsEGL::InitializeClientExtensionSettings() {
  std::string client_extensions(GetClientExtensions());
  [[maybe_unused]] gfx::ExtensionSet extensions(
      gfx::MakeExtensionSet(client_extensions));

""")
  else:
    file.write("""\
void Driver%s::InitializeExtensionBindings() {
  std::string platform_extensions(GetPlatformExtensions());
  [[maybe_unused]] gfx::ExtensionSet extensions(
      gfx::MakeExtensionSet(platform_extensions));

""" % (set_name.upper(),))

  def OutputExtensionSettings(extension_var, extensions, struct_qualifier):
    # Extra space at the end of the extension name is intentional,
    # it is used as a separator
    for extension in extensions:
      file.write('  %sb_%s = gfx::HasExtension(%s, "%s");\n' %
                 (struct_qualifier, extension, extension_var, extension))

  def OutputExtensionBindings(extension_funcs):
    for func in extension_funcs:
      if not 'static_binding' in func:
        file.write('\n')
        WriteConditionalFuncBinding(file, func)

  OutputExtensionSettings(
    'extensions',
    sorted(used_client_extensions),
    '' if BindingsAreAllStatic(set_name) else 'ext.')
  if not BindingsAreAllStatic(set_name):
    OutputExtensionBindings(
      [ f for f in functions if IsClientExtensionFunc(f) ])

  if set_name == 'egl':
    file.write("""\
}

void DisplayExtensionsEGL::InitializeExtensionSettings(EGLDisplay display) {
  std::string platform_extensions(GetPlatformExtensions(display));
  [[maybe_unused]] gfx::ExtensionSet extensions(
      gfx::MakeExtensionSet(platform_extensions));

""")

  OutputExtensionSettings(
    'extensions',
    sorted(used_extensions),
    '' if BindingsAreAllStatic(set_name) else 'ext.')
  if not BindingsAreAllStatic(set_name):
    OutputExtensionBindings(
      [ f for f in functions if not IsClientExtensionFunc(f) ])
  file.write('}\n')

  # Write function to clear all function pointers.
  file.write('\n')
  file.write("""void Driver%s::ClearBindings() {
  auto bytes = base::byte_span_from_ref(*this);
  std::ranges::fill(bytes, 0);
}
""" % set_name.upper())

  def MakeArgNames(arguments):
    argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2', arguments)
    argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2', argument_names)
    if argument_names == 'void' or argument_names == '':
      argument_names = ''
    return argument_names

  # Write GLApiBase functions
  for func in functions:
    function_name = func['known_as']
    return_type = func['return_type']
    arguments = func['arguments']
    file.write('\n')
    file.write('%s %sApiBase::%sFn(%s) {\n' %
        (return_type, set_name.upper(), function_name, arguments))
    argument_names = MakeArgNames(arguments)
    if return_type == 'void':
      file.write('  driver_->fn.%sFn(%s);\n' %
          (function_name, argument_names))
    else:
      file.write('  return driver_->fn.%sFn(%s);\n' %
          (function_name, argument_names))
    file.write('}\n')

  # Write TraceGLApi functions
  for func in functions:
    function_name = func['known_as']
    return_type = func['return_type']
    arguments = func['arguments']
    file.write('\n')
    file.write('%s Trace%sApi::%sFn(%s) {\n' %
        (return_type, set_name.upper(), function_name, arguments))
    argument_names = MakeArgNames(arguments)
    file.write('  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "Trace%sAPI::%s");\n' %
               (set_name.upper(), function_name))
    if return_type == 'void':
      file.write('  %s_api_->%sFn(%s);\n' %
          (set_name.lower(), function_name, argument_names))
    else:
      file.write('  return %s_api_->%sFn(%s);\n' %
          (set_name.lower(), function_name, argument_names))
    file.write('}\n')

  # Write LogGLApi functions
  for func in functions:
    return_type = func['return_type']
    arguments = func['arguments']
    file.write('\n')
    file.write('%s Log%sApi::%sFn(%s) {\n' %
        (return_type, set_name.upper(), func['known_as'], arguments))
    # Strip pointer types.
    argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2', arguments)
    argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2', argument_names)
    # Replace certain `Type varname` combinations with TYPE_varname.
    log_argument_names = re.sub(
        r'const char\* ([a-zA-Z0-9_]+)', r'CONSTCHAR_\1', arguments)
    log_argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\* ([a-zA-Z0-9_]+)',
        r'CONSTVOID_\2', log_argument_names)
    log_argument_names = re.sub(
        r'(const )?EGL[GS]etBlobFuncANDROID ([a-zA-Z0-9_]+)',
        r'FUNCPTR_\2', log_argument_names)
    log_argument_names = re.sub(
        r'(?<!E)GLboolean ([a-zA-Z0-9_]+)', r'GLboolean_\1', log_argument_names)
    log_argument_names = re.sub(
        r'GLDEBUGPROC ([a-zA-Z0-9_]+)',
        r'GLDEBUGPROC_\1', log_argument_names)
    log_argument_names = re.sub(
        r'EGLDEBUGPROCKHR ([a-zA-Z0-9_]+)',
        r'EGLDEBUGPROCKHR_\1', log_argument_names)
    log_argument_names = re.sub(
        r'(?<!E)GLenum ([a-zA-Z0-9_]+)', r'GLenum_\1', log_argument_names)
    # Strip remaining types.
    log_argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2',
        log_argument_names)
    # One more round of stripping to remove both type parts in `unsigned long`.
    log_argument_names = re.sub(
        r'(const )?[a-zA-Z0-9_]+\** ([a-zA-Z0-9_]+)', r'\2',
        log_argument_names)
    # Convert TYPE_varname log arguments to the corresponding log expression.
    log_argument_names = re.sub(
        r'CONSTVOID_([a-zA-Z0-9_]+)',
        r'static_cast<const void*>(\1)', log_argument_names)
    log_argument_names = re.sub(
        r'FUNCPTR_([a-zA-Z0-9_]+)',
        r'reinterpret_cast<const void*>(\1)', log_argument_names)
    log_argument_names = re.sub(
        r'CONSTCHAR_([a-zA-Z0-9_]+)', r'\1', log_argument_names)
    log_argument_names = re.sub(
        r'GLboolean_([a-zA-Z0-9_]+)', r'GLEnums::GetStringBool(\1)',
        log_argument_names)
    log_argument_names = re.sub(
        r'GLDEBUGPROC_([a-zA-Z0-9_]+)',
        r'reinterpret_cast<void*>(\1)', log_argument_names)
    log_argument_names = re.sub(
        r'EGLDEBUGPROCKHR_([a-zA-Z0-9_]+)',
        r'reinterpret_cast<void*>(\1)', log_argument_names)
    log_argument_names = re.sub(
        r'GLenum_([a-zA-Z0-9_]+)', r'GLEnums::GetStringEnum(\1)',
        log_argument_names)
    log_argument_names = log_argument_names.replace(',', ' << ", " <<')
    if argument_names == 'void' or argument_names == '':
      argument_names = ''
      log_argument_names = ''
    else:
      log_argument_names = " << " + log_argument_names
    function_name = func['known_as']
    if return_type == 'void':
      file.write('  GL_SERVICE_LOG("%s" << "(" %s << ")");\n' %
          (function_name, log_argument_names))
      file.write('  %s_api_->%sFn(%s);\n' %
          (set_name.lower(), function_name, argument_names))
      if 'logging_code' in func:
        file.write("%s\n" % func['logging_code'])
      if options.generate_dchecks and set_name == 'gl':
        file.write('  {\n')
        file.write('    GLenum error = %s_api_->glGetErrorFn();\n'
            % set_name.lower())
        file.write('    DCHECK(error == 0) << "OpenGL error 0x" << std::hex '
                   '<< error << std::dec;\n')
        file.write('  }\n')
    else:
      file.write('  GL_SERVICE_LOG("%s" << "(" %s << ")");\n' %
          (function_name, log_argument_names))
      file.write('  %s result = %s_api_->%sFn(%s);\n' %
          (return_type, set_name.lower(), function_name, argument_names))
      if 'logging_code' in func:
        file.write("%s\n" % func['logging_code'])
      else:
        file.write('  GL_SERVICE_LOG("GL_RESULT: " << result);\n')
      if options.generate_dchecks and set_name == 'gl':
        file.write('  {\n')
        file.write('    GLenum _error = %s_api_->glGetErrorFn();\n' %
            set_name.lower())
        file.write('    DCHECK(_error == 0) << "OpenGL error " << std::hex '
                   '<< _error << std::dec;\n')
        file.write('  }\n')
      file.write('  return result;\n')
    file.write('}\n')

  # Write NoContextGLApi functions
  if set_name.upper() == "GL":
    file.write('\n')
    file.write('namespace {\n')
    file.write('void NoContextHelper(const char* method_name) {\n')
    no_context_error = ('<< "Trying to call " << method_name << " without '
                        'current GL context"')
    file.write('  NOTREACHED_IN_MIGRATION() %s;\n' % no_context_error)
    file.write('  LOG(ERROR) %s;\n' % no_context_error)
    file.write('}\n')
    file.write('}  // namespace\n')
    for func in functions:
      function_name = func['known_as']
      return_type = func['return_type']
      arguments = func['arguments']
      file.write('\n')
      file.write('%s NoContextGLApi::%sFn(%s) {\n' %
          (return_type, function_name, arguments))
      argument_names = MakeArgNames(arguments)
      file.write('  NoContextHelper("%s");\n' % function_name)
      default_value = { 'GLenum': 'static_cast<GLenum>(0)',
                        'GLuint': '0U',
                        'GLint': '0',
                        'GLboolean': 'GL_FALSE',
                        'GLbyte': '0',
                        'GLubyte': '0',
                        'GLbutfield': '0',
                        'GLushort': '0',
                        'GLsizei': '0',
                        'GLfloat': '0.0f',
                        'GLdouble': '0.0',
                        'GLsync': 'nullptr',
                        'GLDEBUGPROC': 'NULL'}
      if return_type.endswith('*'):
        file.write('  return NULL;\n')
      elif return_type != 'void':
        file.write('  return %s;\n' % default_value[return_type])
      file.write('}\n')

  file.write('\n')
  file.write('}  // namespace gl\n')


def GetUniquelyNamedFunctions(functions):
  uniquely_named_functions = {}

  for func in functions:
    for version in func['versions']:
      uniquely_named_functions[version['name']] = ({
        'name': version['name'],
        'return_type': func['return_type'],
        'arguments': func['arguments'],
        'known_as': func['known_as']
      })
  return uniquely_named_functions


def GenerateMockBindingsHeader(file, functions):
  """Headers for functions that invoke MockGLInterface members"""

  file.write(LICENSE_AND_HEADER +
"""

// Silence presubmit and Tricium warnings about include guards
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

""")
  uniquely_named_functions = GetUniquelyNamedFunctions(functions)

  for key in sorted(uniquely_named_functions.keys()):
    func = uniquely_named_functions[key]
    file.write('static %s GL_BINDING_CALL Mock_%s(%s);\n' %
        (func['return_type'], func['name'], func['arguments']))


def GenerateMockBindingsSource(file, functions, set_name):
  """Generates functions that invoke MockGLInterface members and a
  GetGLProcAddress function that returns addresses to those functions."""

  file.write(LICENSE_AND_HEADER +
"""

#include <string.h>

#include "base/notreached.h"
#include "ui/gl/%s_mock.h"

namespace {
// This is called mainly to prevent the compiler combining the code of mock
// functions with identical contents, so that their function pointers will be
// different.
void Make%sMockFunctionUnique(const char *func_name) {
    VLOG(2) << "Calling mock " << func_name;
}
}  // namespace

namespace gl {
""" % (set_name, set_name.capitalize()))

  # Write functions that trampoline into the set MockGLInterface instance.
  uniquely_named_functions = GetUniquelyNamedFunctions(functions)
  sorted_function_names = sorted(uniquely_named_functions.keys())

  for key in sorted_function_names:
    func = uniquely_named_functions[key]
    file.write('\n')
    file.write('%s GL_BINDING_CALL Mock%sInterface::Mock_%s(%s) {\n' %
        (func['return_type'], set_name.upper(), func['name'],
         func['arguments']))
    file.write('  Make%sMockFunctionUnique("%s");\n' % (set_name.capitalize(),
                                                        func['name']))
    arg_re = r'(const |struct )*[a-zA-Z0-9]+((\s*const\s*)?\*)* ([a-zA-Z0-9]+)'
    argument_names = re.sub(arg_re, r'\4', func['arguments'])
    if argument_names == 'void':
      argument_names = ''
    function_name = func['known_as'][len(set_name):]
    if func['return_type'] == 'void':
      file.write('  interface_->%s(%s);\n' %
          (function_name, argument_names))
    else:
      file.write('  return interface_->%s(%s);\n' %
          (function_name, argument_names))
    file.write('}\n')

  # Write an 'invalid' function to catch code calling through uninitialized
  # function pointers or trying to interpret the return value of
  # GLProcAddress().
  file.write('\n')
  file.write('static void Mock%sInvalidFunction() {\n' % set_name.capitalize())
  file.write('  NOTREACHED_IN_MIGRATION();\n')
  file.write('}\n')

  # Write a function to lookup a mock GL function based on its name.
  file.write('\n')
  file.write('GLFunctionPointerType GL_BINDING_CALL ' +
             'Mock%sInterface::GetGLProcAddress(const char* name) {\n' % (
                 set_name.upper(),))
  for key in sorted_function_names:
    name = uniquely_named_functions[key]['name']
    file.write('  if (strcmp(name, "%s") == 0)\n' % name)
    file.write(
        '    return reinterpret_cast<GLFunctionPointerType>(Mock_%s);\n' %
            name)
  # Always return a non-NULL pointer like some EGL implementations do.
  file.write('  return reinterpret_cast<GLFunctionPointerType>('
             '&Mock%sInvalidFunction);\n' % set_name.capitalize())
  file.write('}\n')

  file.write('\n')
  file.write('}  // namespace gl\n')

def SamePrefix(a, b):
  return a[:a.rfind("_")] == b[:b.rfind("_")]

def GenerateEnumUtils(out_file, input_filenames):
  enum_re = re.compile(r'\#define\s+(GL_[a-zA-Z0-9_]+)\s+([0-9A-Fa-fx]+)')
  dict = {}
  for fname in input_filenames:
    lines = open(fname).readlines()
    for line in lines:
      m = enum_re.match(line)
      if m:
        name = m.group(1)
        value = m.group(2)
        if len(value) <= 10 and value.startswith('0x'):
          if not value in dict:
            dict[value] = name
          # check our own _CHROMIUM macro conflicts with khronos GL headers.
          # we allow for name duplication if they have the same prefix.
          elif dict[value] != name and ((name.endswith('_CHROMIUM') or
              dict[value].endswith('_CHROMIUM')) and
              not SamePrefix(name, dict[value])):
            raise RuntimeError("code collision: %s and %s have the same code %s"
                               %  (dict[value], name, value))

  out_file.write(LICENSE_AND_HEADER +
"""

#ifndef UI_GL_GL_ENUMS_IMPLEMENTATION_AUTOGEN_H_
#define UI_GL_GL_ENUMS_IMPLEMENTATION_AUTOGEN_H_

namespace {

struct EnumToString {
  uint32_t value;
  std::string_view name;
};

static constexpr EnumToString kEnumToStringTable[] = {
""")
  for value in sorted(dict):
    out_file.write('  { %s, "%s", },\n' % (value, dict[value]))
  out_file.write("""};

}  // namespace

#endif  //  UI_GL_GL_ENUMS_IMPLEMENTATION_AUTOGEN_H_
""")


def ParseFunctionsFromHeader(header_file, extensions, versions):
  """Parse a C extension header file and return a map from extension names to
  a list of functions.

  Args:
    header_file: Line-iterable C header file.
  Returns:
    Map of extension name => functions, Map of gl version => functions.
    Functions will only be in either one of the two maps.
  """
  version_start = re.compile(
      r'#ifndef GL_(ES_|)VERSION((?:_[0-9])+)$')
  extension_start = re.compile(
      r'#ifndef ((?:GL|EGL)_[A-Z]+_[a-zA-Z]\w+)')
  extension_function = re.compile(r'.+\s+([a-z]+\w+)\s*\(')
  typedef = re.compile(r'typedef .*')
  macro_start = re.compile(r'^#(if|ifdef|ifndef).*')
  macro_end = re.compile(r'^#endif.*')
  macro_depth = 0
  current_version = None
  current_version_depth = 0
  current_extension = None
  current_extension_depth = 0

  # Pick up all core functions here, since some of them are missing in the
  # Khronos headers.
  hdr = os.path.basename(header_file.name)
  if hdr == "gl.h":
    current_version = GLVersion(False, 1, 0)

  line_num = 1
  for line in header_file:
    version_match = version_start.match(line)
    if macro_start.match(line):
      macro_depth += 1
      if version_match:
        if current_version:
          raise RuntimeError('Nested GL version macro in %s at line %d' % (
              header_file.name, line_num))
        current_version_depth = macro_depth
        es = version_match.group(1)
        major_version, minor_version =\
            version_match.group(2).lstrip('_').split('_')
        is_es = len(es) > 0
        if (not is_es) and (major_version == '1'):
          minor_version = 0
        current_version = GLVersion(
            is_es, int(major_version), int(minor_version))
    elif macro_end.match(line):
      macro_depth -= 1
      if macro_depth < current_extension_depth:
        current_extension = None
      if macro_depth < current_version_depth:
        current_version = None

    match = extension_start.match(line)
    if match and not version_match:
      if current_version and hdr != "gl.h":
        raise RuntimeError('Nested GL version macro in %s at line %d' % (
            header_file.name, line_num))
      current_extension = match.group(1)
      current_extension_depth = macro_depth

    match = extension_function.match(line)
    if match and not typedef.match(line):
      if current_extension:
        extensions[current_extension].add(match.group(1))
      elif current_version:
        versions[current_version].add(match.group(1))
    line_num = line_num + 1


def GetDynamicFunctions(extension_headers):
  """Parse all optional functions from a list of header files.

  Args:
    extension_headers: List of header file names.
  Returns:
    Map of extension name => list of functions,
    Map of gl version => list of functions.
  """
  extensions = collections.defaultdict(lambda: set([]))
  gl_versions = collections.defaultdict(lambda: set([]))
  for header in extension_headers:
    ParseFunctionsFromHeader(open(header), extensions, gl_versions)
  return extensions, gl_versions


def GetFunctionToExtensionsMap(extensions):
  """Construct map from a function names to extensions which define the
  function.

  Args:
    extensions: Map of extension name => functions.
  Returns:
    Map of function name => extension names.
  """
  function_to_extensions = {}
  for extension, functions in extensions.items():
    for function in functions:
      if not function in function_to_extensions:
        function_to_extensions[function] = set([])
      function_to_extensions[function].add(extension)
  return function_to_extensions

def GetFunctionToGLVersionsMap(gl_versions):
  """Construct map from a function names to GL versions which define the
  function.

  Args:
    extensions: Map of gl versions => functions.
  Returns:
    Map of function name => gl versions.
  """
  function_to_gl_versions = {}
  for gl_version, functions in gl_versions.items():
    for function in functions:
      if not function in function_to_gl_versions:
        function_to_gl_versions[function] = set([])
      function_to_gl_versions[function].add(gl_version)
  return function_to_gl_versions


def LooksLikeExtensionFunction(function):
  """Heuristic to see if a function name is consistent with extension function
  naming."""
  vendor = re.match(r'\w+?([A-Z][A-Z]+)$', function)
  return vendor is not None and not vendor.group(1) in ['GL', 'API', 'DC']


def SortVersions(key):
   # Prefer functions from the core for binding
  if 'gl_versions' in key:
    return 0
  else:
    return 1

def FillExtensionsFromHeaders(functions, extension_headers, extra_extensions):
  """Determine which functions belong to extensions based on extension headers,
  and fill in this information to the functions table for functions that don't
  already have the information.

  Args:
    functions: List of (return type, function versions, arguments).
    extension_headers: List of header file names.
    extra_extensions: Extensions to add to the list.
  Returns:
    Set of used extensions.
  """
  # Parse known extensions.
  extensions, gl_versions = GetDynamicFunctions(extension_headers)
  functions_to_extensions = GetFunctionToExtensionsMap(extensions)
  functions_to_gl_versions = GetFunctionToGLVersionsMap(gl_versions)

  # Fill in the extension information.
  used_extensions = set()
  used_client_extensions = set()
  used_functions_by_version = collections.defaultdict(lambda: set([]))
  for func in functions:
    for version in func['versions']:
      name = version['name']

      # There should only be one version entry per name string.
      if len([v for v in func['versions'] if v['name'] == name]) > 1:
        raise RuntimeError(
            'Duplicate version entries with same name for %s' % name)

      # Make sure we know about all extensions and extension functions.
      extensions_from_headers = set([])
      if name in functions_to_extensions:
        extensions_from_headers = set(functions_to_extensions[name])

      explicit_extensions = set([])

      if 'client_extensions' in version:
        assert not 'extensions' in version
        version['extensions'] = version['client_extensions']

      if 'extensions' in version:
        explicit_extensions = set(version['extensions'])

      in_both = explicit_extensions.intersection(extensions_from_headers)
      if len(in_both):
        print("[%s] Specified redundant extensions for binding: %s" % (
            name, ', '.join(in_both)))
      diff = explicit_extensions - extensions_from_headers
      if len(diff):
        print("[%s] Specified extra extensions for binding: %s" % (
            name, ', '.join(diff)))

      if version.get('explicit_only', False):
        all_extensions = explicit_extensions
      else:
        all_extensions = extensions_from_headers.union(explicit_extensions)
      if len(all_extensions):
        version['extensions'] = all_extensions

      if 'extensions' in version:
        assert len(version['extensions'])
        if 'client_extensions' in version:
          used_client_extensions.update(version['extensions'])
        else:
          used_extensions.update(version['extensions'])

      if not 'extensions' in version and LooksLikeExtensionFunction(name):
        raise RuntimeError('%s looks like an extension function but does not '
            'belong to any of the known extensions.' % name)

      if name in functions_to_gl_versions:
        assert not 'gl_versions' in version
        version['gl_versions'] = functions_to_gl_versions[name]
        for v in version['gl_versions']:
          used_functions_by_version[v].add(name)

    func['versions'] = sorted(func['versions'], key=SortVersions)

  # Add extensions that do not have any functions.
  used_extensions.update(extra_extensions)

  # Print out used function count by GL(ES) version.
  for v in sorted([v for v in used_functions_by_version if v.is_es]):
    print("OpenGL ES %d.%d: %d used functions" % (
        v.major_version, v.minor_version, len(used_functions_by_version[v])))
  for v in sorted([v for v in used_functions_by_version if not v.is_es]):
    print("OpenGL %d.%d: %d used functions" % (
        v.major_version, v.minor_version, len(used_functions_by_version[v])))

  return used_extensions, used_client_extensions


def ResolveHeader(header, header_paths):
  for path in header_paths:
    result = os.path.join(path, header)
    if not os.path.isabs(path):
      result = os.path.abspath(os.path.join(SELF_LOCATION, result))
    if os.path.exists(result):
      # Always use forward slashes as path separators. Otherwise backslashes
      # may be incorrectly interpreted as escape characters.
      return result.replace(os.path.sep, '/')

  raise Exception('Header %s not found.' % header)


def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  parser.add_option('--inputs', action='store_true')
  parser.add_option('--verify-order', action='store_true')
  parser.add_option('--generate-dchecks', action='store_true',
                    help='Generates DCHECKs into the logging functions '
                         'asserting no GL errors (useful for debugging with '
                         '--enable-gpu-service-logging)')
  parser.add_option('--validate-bindings', action='store_true',
                    help='Generate DCHECKs to validate function bindings '
                         ' were correctly supplied (useful for debugging)')

  options, args = parser.parse_args(argv)

  if options.inputs:
    for [_, _, headers, _] in FUNCTION_SETS:
      for header in headers:
        print(ResolveHeader(header, HEADER_PATHS))
    return 0

  directory = SELF_LOCATION
  if len(args) >= 1:
    directory = args[0]

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  for [functions, set_name, extension_headers, extensions] in FUNCTION_SETS:
    # Function names can be specified in two ways (list of unique names or list
    # of versions with different binding conditions). Fill in the data to the
    # versions list in case it is missing, so that can be used from here on:
    for func in functions:
      assert 'versions' in func or 'names' in func, 'Function with no names'
      if 'versions' not in func:
        func['versions'] = [{'name': n} for n in func['names']]
      # Use the first version's name unless otherwise specified
      if 'known_as' not in func:
        func['known_as'] = func['versions'][0]['name']
      # Make sure that 'names' is not accidentally used instead of 'versions'
      if 'names' in func:
        del func['names']

    # Check function names in each set is sorted in alphabetical order.
    for index in range(len(functions) - 1):
      func_name = functions[index]['known_as']
      next_func_name = functions[index + 1]['known_as']
      if func_name.lower() > next_func_name.lower():
        raise Exception(
            'function %s is not in alphabetical order' % next_func_name)
    if options.verify_order:
      continue

    extension_headers = [ResolveHeader(h, HEADER_PATHS)
                         for h in extension_headers]
    used_extensions, used_client_extensions = FillExtensionsFromHeaders(
        functions, extension_headers, extensions)

    if set_name == 'egl':
      used_extensions.update(EGL_EXTENSIONS_EXTRA)
      used_client_extensions.update(EGL_CLIENT_EXTENSIONS_EXTRA)

    header_file = open(
        os.path.join(directory, 'gl_bindings_autogen_%s.h' % set_name), 'w')
    GenerateHeader(header_file, functions, set_name,
                   used_extensions, used_client_extensions)
    header_file.close()
    ClangFormat(header_file.name)

    header_file = open(
        os.path.join(directory, 'gl_bindings_api_autogen_%s.h' % set_name),
        'w')
    GenerateAPIHeader(header_file, functions, set_name)
    header_file.close()
    ClangFormat(header_file.name)

    source_file = open(
        os.path.join(directory, 'gl_bindings_autogen_%s.cc' % set_name), 'w')
    GenerateSource(source_file, functions, set_name,
                   used_extensions, used_client_extensions, options)
    source_file.close()
    ClangFormat(source_file.name)

  if not options.verify_order:
    header_file = open(
        os.path.join(directory, 'gl_mock_autogen_gl.h'), 'w')
    GenerateMockHeader(header_file, GL_FUNCTIONS, 'gl')
    header_file.close()
    ClangFormat(header_file.name)

    header_file = open(os.path.join(directory, 'gl_bindings_autogen_mock.h'),
                       'w')
    GenerateMockBindingsHeader(header_file, GL_FUNCTIONS)
    header_file.close()
    ClangFormat(header_file.name)

    source_file = open(os.path.join(directory, 'gl_bindings_autogen_mock.cc'),
                       'w')
    GenerateMockBindingsSource(source_file, GL_FUNCTIONS, 'gl')
    source_file.close()
    ClangFormat(source_file.name)

    header_file = open(
        os.path.join(directory, 'gl_mock_autogen_egl.h'), 'w')
    GenerateMockHeader(header_file, EGL_FUNCTIONS, 'egl')
    header_file.close()
    ClangFormat(header_file.name)

    header_file = open(os.path.join(directory, 'egl_bindings_autogen_mock.h'),
                       'w')
    GenerateMockBindingsHeader(header_file, EGL_FUNCTIONS)
    header_file.close()
    ClangFormat(header_file.name)

    source_file = open(os.path.join(directory, 'egl_bindings_autogen_mock.cc'),
                       'w')
    GenerateMockBindingsSource(source_file, EGL_FUNCTIONS, 'egl')
    source_file.close()
    ClangFormat(source_file.name)

    enum_header_filenames = [ResolveHeader(h, HEADER_PATHS)
                             for h in GLES2_HEADERS_WITH_ENUMS]
    header_file = open(os.path.join(directory,
                                    'gl_enums_implementation_autogen.h'),
                       'w')
    GenerateEnumUtils(header_file, enum_header_filenames)
    header_file.close()
    ClangFormat(header_file.name)

    header_file = open(
        os.path.join(directory, 'gl_stub_autogen_gl.h'), 'w')
    GenerateStubHeader(header_file, GL_FUNCTIONS)
    header_file.close()
    ClangFormat(header_file.name)

    header_file = open(
        os.path.join(directory, 'gl_stub_autogen_gl.cc'), 'w')
    GenerateStubSource(header_file, GL_FUNCTIONS)
    header_file.close()
    ClangFormat(header_file.name)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
