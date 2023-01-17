// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_DISPLAY_INITIALIZER_H_
#define UI_GL_INIT_GL_DISPLAY_INITIALIZER_H_

#include <vector>

#include "ui/gl/gl_display.h"
#include "ui/gl/init/gl_init_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace gl::init {

GL_INIT_EXPORT void GetEGLInitDisplaysForTesting(
    bool supports_angle_d3d,
    bool supports_angle_opengl,
    bool supports_angle_null,
    bool supports_angle_vulkan,
    bool supports_angle_swiftshader,
    bool supports_angle_opengl_egl,
    bool supports_angle_metal,
    const base::CommandLine* command_line,
    std::vector<DisplayType>* init_displays);

GL_INIT_EXPORT void GetDisplayInitializationParams(
    bool* supports_angle,
    std::vector<DisplayType>* init_displays);

GL_INIT_EXPORT bool InitializeDisplay(GLDisplayEGL* display,
                                      EGLDisplayPlatform native_display);

}  // namespace gl::init

#endif  // UI_GL_INIT_GL_DISPLAY_INITIALIZER_H_
