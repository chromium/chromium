// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the GLContextWGL and PbufferGLContext classes.

#include "ui/gl/gl_context_wgl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_wgl.h"

namespace gl {

GLContextWGL::GLContextWGL(GLShareGroup* share_group)
    : GLContextReal(share_group), context_(nullptr) {
}

bool GLContextWGL::Initialize(GLSurface* compatible_surface,
                              const GLContextAttribs& attribs) {
  // webgl_compatibility_context and disabling bind_generates_resource are not
  // supported.
  DCHECK(!attribs.webgl_compatibility_context &&
         attribs.bind_generates_resource);

  // Get the handle of another initialized context in the share group _before_
  // setting context_. Otherwise this context will be considered initialized
  // and could potentially be returned by GetHandle.
  HGLRC share_handle = static_cast<HGLRC>(share_group()->GetHandle());

  HDC device_context = static_cast<HDC>(compatible_surface->GetHandle());
  bool has_wgl_create_context_arb =
      strstr(wglGetExtensionsStringARB(device_context),
             "WGL_ARB_create_context") != nullptr;
  bool create_core_profile = has_wgl_create_context_arb &&
                             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kDisableES3GLContext);

  if (create_core_profile) {
    std::pair<int, int> attempt_versions[] = {
        {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2},
    };

    for (const auto& version : attempt_versions) {
      const int attribs[] = {
          WGL_CONTEXT_MAJOR_VERSION_ARB,
          version.first,
          WGL_CONTEXT_MINOR_VERSION_ARB,
          version.second,
          WGL_CONTEXT_PROFILE_MASK_ARB,
          WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
          0,
          0,
      };

      context_ =
          wglCreateContextAttribsARB(device_context, share_handle, attribs);
      if (context_) {
        break;
      }
    }
  }

  if (!context_) {
    context_ = wglCreateContext(device_context);
  }
  if (!context_) {
    LOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (share_handle) {
    if (!wglShareLists(share_handle, context_)) {
      LOG(ERROR) << "Could not share GL contexts.";
      Destroy();
      return false;
    }
  }

  return true;
}

void GLContextWGL::Destroy() {
  if (context_) {
    wglDeleteContext(context_);
    context_ = nullptr;
  }
}

bool GLContextWGL::MakeCurrentImpl(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  ScopedReleaseCurrent release_current;
  TRACE_EVENT0("gpu", "GLContextWGL::MakeCurrent");

  if (!wglMakeCurrent(static_cast<HDC>(surface->GetHandle()), context_)) {
    LOG(ERROR) << "Unable to make gl context current.";
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

void GLContextWGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(nullptr);
  wglMakeCurrent(nullptr, nullptr);
}

bool GLContextWGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current =
      wglGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetRealCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    if (wglGetCurrentDC() != surface->GetHandle())
      return false;
  }

  return true;
}

void* GLContextWGL::GetHandle() {
  return context_;
}

GLContextWGL::~GLContextWGL() {
  Destroy();
}

}  // namespace gl
