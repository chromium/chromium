// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/mock_egl.h"

void SetupAttributes() {
  // We want 'v' to be spanified because of unsafe usage (e.g. index access
  // using dynamic col).
  // Expected rewrite:
  // std::array<GLfloat, 4> v = {0.0, 0.0, 0.0, 0.0};
  GLfloat v[4] = {0.0, 0.0, 0.0, 0.0};

  // Dynamic index access to force spanification
  int col = 1;
  v[col] = 1.0;

  // Passed to external API.
  // This is a frontier call.
  // Expected rewrite:
  // glVertexAttrib4fv(1, v.data());
  glVertexAttrib4fv(1, v);
}
