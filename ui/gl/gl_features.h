// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FEATURES_H_
#define UI_GL_GL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GL_EXPORT extern const base::Feature kDefaultPassthroughCommandDecoder;

}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
