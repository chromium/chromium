// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_cgl.h"

#include <ostream>

#include "base/check_op.h"

namespace gl {

ScopedCGLSetCurrentContext::ScopedCGLSetCurrentContext(CGLContextObj context) {
  CGLContextObj previous_context = CGLGetCurrentContext();
  // It is possible for the previous context to have a zero reference count,
  // because making a context current does not increment the reference count.
  // In that case, do not restore the previous context.
  if (previous_context && CGLGetContextRetainCount(previous_context)) {
    previous_context_.reset(previous_context, base::scoped_policy::RETAIN);
  }
  CGLError error = CGLSetCurrentContext(context);
  DCHECK_EQ(error, kCGLNoError) << "CGLSetCurrentContext should never fail";
}

ScopedCGLSetCurrentContext::~ScopedCGLSetCurrentContext() {
  CGLError error = CGLSetCurrentContext(previous_context_);
  DCHECK_EQ(error, kCGLNoError) << "CGLSetCurrentContext should never fail";
}

}  // namespace gl
