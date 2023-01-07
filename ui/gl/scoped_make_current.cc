// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_make_current.h"

#include "base/check.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace ui {

ScopedMakeCurrent::ScopedMakeCurrent(gl::GLContext* context,
                                     gl::GLSurface* surface)
    : previous_context_(gl::GLContext::GetCurrent()),
      previous_surface_(gl::GLSurface::GetCurrent()),
      context_(context),
      surface_(surface) {
  DCHECK(context);
  DCHECK(surface);
  is_context_current_ = context->MakeCurrent(surface);
}

ScopedMakeCurrent::~ScopedMakeCurrent() {
  if (previous_context_) {
    DCHECK(previous_surface_);
    previous_context_->MakeCurrent(previous_surface_.get());
  } else {
    context_->ReleaseCurrent(surface_.get());
  }
}

}  // namespace ui
