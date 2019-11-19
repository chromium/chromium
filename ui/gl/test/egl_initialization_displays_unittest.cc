// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface_egl.h"

namespace {

TEST(EGLInitializationDisplaysTest, DisableD3D11) {
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gl::DisplayType> displays;

  // using --disable-d3d11 with the default --use-angle should never return
  // D3D11.
  command_line->AppendSwitch(switches::kDisableD3D11);
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_FALSE(base::Contains(displays, gl::ANGLE_D3D11));

  // Specifically requesting D3D11 should always return it if the extension is
  // available
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationD3D11Name);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_D3D11));
  EXPECT_EQ(displays.size(), 1u);

  // Specifically requesting D3D11 should not return D3D11 if the extension is
  // not available
  displays.clear();
  GetEGLInitDisplays(false, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_FALSE(base::Contains(displays, gl::ANGLE_D3D11));
}

TEST(EGLInitializationDisplaysTest, SwiftShader) {
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gl::DisplayType> displays;

  // If swiftshader is requested, only SWIFT_SHADER should be returned
  command_line->AppendSwitchASCII(switches::kUseGL,
                                  gl::kGLImplementationSwiftShaderForWebGLName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::SWIFT_SHADER));
  EXPECT_EQ(displays.size(), 1u);

  // Even if there are other flags, swiftshader should take prescedence
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationD3D11Name);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::SWIFT_SHADER));
  EXPECT_EQ(displays.size(), 1u);
}

TEST(EGLInitializationDisplaysTest, DefaultRenderers) {
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  // Default without --use-angle flag
  std::vector<gl::DisplayType> default_no_flag_displays;
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &default_no_flag_displays);
  EXPECT_FALSE(default_no_flag_displays.empty());

  // Default with --use-angle flag
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationDefaultName);
  std::vector<gl::DisplayType> default_with_flag_displays;
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &default_with_flag_displays);
  EXPECT_FALSE(default_with_flag_displays.empty());

  // Make sure the same results are returned
  EXPECT_EQ(default_no_flag_displays, default_with_flag_displays);
}

TEST(EGLInitializationDisplaysTest, NonDefaultRenderers) {
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  std::vector<gl::DisplayType> displays;

  // OpenGL
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationOpenGLName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_OPENGL));
  EXPECT_EQ(displays.size(), 1u);

  // OpenGLES
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationOpenGLESName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_OPENGLES));
  EXPECT_EQ(displays.size(), 1u);

  // Null
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationNullName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_NULL));
  EXPECT_EQ(displays.size(), 1u);

  // Vulkan
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationVulkanName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_VULKAN));
  EXPECT_EQ(displays.size(), 1u);

  // Vulkan/SwiftShader
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationSwiftShaderName);
  displays.clear();
  GetEGLInitDisplays(true, true, true, true, true, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::ANGLE_SWIFTSHADER));
  EXPECT_EQ(displays.size(), 1u);
}

TEST(EGLInitializationDisplaysTest, NoExtensions) {
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(base::CommandLine::NO_PROGRAM));

  // With no angle platform extensions, only DEFAULT should be available
  std::vector<gl::DisplayType> displays;
  GetEGLInitDisplays(false, false, false, false, false, command_line.get(),
                     &displays);
  EXPECT_TRUE(base::Contains(displays, gl::DEFAULT));
  EXPECT_EQ(displays.size(), 1u);
}

}  // namespace
