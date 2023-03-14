// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_share_group.h"

#include "base/check.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gl {

GLShareGroup::GLShareGroup() = default;

void GLShareGroup::AddContext(GLContext* context) {
  contexts_.insert(context);
}

void GLShareGroup::RemoveContext(GLContext* context) {
  contexts_.erase(context);
  if (shared_context_ == context)
    shared_context_ = nullptr;
}

void* GLShareGroup::GetHandle() {
  GLContext* context = GetContext();
  if (context)
    return context->GetHandle();

  return NULL;
}

GLContext* GLShareGroup::GetContext() {
  for (auto it = contexts_.begin(); it != contexts_.end(); ++it) {
    if ((*it)->GetHandle())
      return *it;
  }

  return NULL;
}

void GLShareGroup::SetSharedContext(GLContext* context) {
  DCHECK(contexts_.find(context) != contexts_.end());
  shared_context_ = context;
}

GLShareGroup::~GLShareGroup() {
}

}  // namespace gl
