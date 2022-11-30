// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_WORKAROUNDS_H_
#define UI_GL_GL_WORKAROUNDS_H_

namespace gl {

struct GLWorkarounds {
  // glClearColor does not always work on Intel 6xxx Mac drivers. See
  // crbug.com/710443.
  bool clear_to_zero_or_one_broken = false;
  // Reset texImage2D base level to workaround pixel comparison failure
  // above Mac OS 10.12.4 on Intel Mac. See crbug.com/705865.
  bool reset_teximage2d_base_level = false;
};

}  // namespace gl

#endif  // UI_GL_GL_WORKAROUNDS_H_
