// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SHADER_TRACKING_H_
#define UI_GL_SHADER_TRACKING_H_

#include <string>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT ShaderTracking {
 public:
  static ShaderTracking* GetInstance();

  static const size_t kMaxShaderSize = 1024;

  void GetShaders(std::string* shader0, std::string* shader1);

  void SetShaders(const char* shader0, const char* shader1);

 private:
  friend base::NoDestructor<ShaderTracking>;

  ShaderTracking() {}
  ~ShaderTracking() {}

  mutable base::Lock lock_;
  std::string shaders_[2];

  DISALLOW_COPY_AND_ASSIGN(ShaderTracking);
};

}  // namespace gl

#endif  // UI_GL_SHADER_TRACKING_H_
