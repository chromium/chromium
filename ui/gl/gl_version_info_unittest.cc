// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include <array>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace gl {

TEST(GLVersionInfoTest, ParseGLVersionStringTest) {
  struct GLVersionTestData {
    const std::string_view gl_version;
    unsigned expected_gl_major;
    unsigned expected_gl_minor;
    bool expected_is_es2;
    bool expected_is_es3;
    const std::string_view expected_driver_vendor;
    const std::string_view expected_driver_version;
  };
  static constexpr std::array<GLVersionTestData, 8> kTestData = {
      {{"OpenGL ES 3.0", 3, 0, false, true, "", ""},
       {"OpenGL ES 3.2 v1.r12p0-04rel0.44f2946824bb8739781564bffe2110c9", 3, 2,
        false, true, "ARM", "12.0.04rel0"},
       {"OpenGL ES 3.0 V@84.0 AU@05.00.00.046.002 (CL@)", 3, 0, false, true, "",
        "84.0"},
       {"OpenGL ES 2.0 build 1.12@2701748", 2, 0, true, false, "", "1.12"},
       {"OpenGL ES 3.1 V6.2.4.138003", 3, 1, false, true, "", "6.2.4.138003"},
       {"OpenGL ES 3.0 Mesa 12.0.3 (git-8b8f097)", 3, 0, false, true, "Mesa",
        "12.0.3"},
       {"OpenGL ES 3.0 SwiftShader 4.1.0.7", 3, 0, false, true, "", "4.1.0.7"},
       // This is a non spec compliant string from Nexus6 on Android N.
       {"OpenGL ES 3.1V@104.0", 3, 1, false, true, "", "104.0"}}};

  gfx::ExtensionSet extensions;
  for (const auto& test_case : kTestData) {
    GLVersionInfo version_info(test_case.gl_version.data(), nullptr,
                               extensions);
    EXPECT_EQ(test_case.expected_gl_major, version_info.major_version);
    EXPECT_EQ(test_case.expected_gl_minor, version_info.minor_version);
    EXPECT_EQ(test_case.expected_is_es2, version_info.is_es2);
    EXPECT_EQ(test_case.expected_is_es3, version_info.is_es3);
    EXPECT_STREQ(test_case.expected_driver_vendor.data(),
                 version_info.driver_vendor.c_str());
    EXPECT_STREQ(test_case.expected_driver_version.data(),
                 version_info.driver_version.c_str());
  }
}

TEST(GLVersionInfoTest, DriverVendorForANGLE) {
  struct GLVersionTestData {
    const std::string_view gl_version;
    const std::string_view gl_renderer;
    unsigned expected_gl_major;
    unsigned expected_gl_minor;
    bool expected_is_es2;
    bool expected_is_es3;
    bool expected_is_d3d;
    const std::string_view expected_driver_vendor;
    const std::string_view expected_driver_version;
  };
  static constexpr std::array<GLVersionTestData, 12> kTestData = {
      {{"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (Intel Inc., Intel(R) UHD Graphics 630, OpenGL 4.1 "
        "INTEL-14.7.11)",
        2, 0, true, false, false, "INTEL", "14.7.11"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (ATI Technologies Inc., AMD Radeon Pro 560X OpenGL Engine, "
        "OpenGL 4.1 ATI-3.10.19)",
        2, 0, true, false, false, "ATI", "3.10.19"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (AMD, Metal Renderer: AMD Radeon Pro 560X, Version 10.15.7 "
        "(Build 19H114))",
        2, 0, true, false, false, "AMD", "10.15.7"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (Mesa/X.org, llvmpipe (LLVM 11.0.0 256 bits), OpenGL 4.5 (Core "
        "Profile) Mesa 20.2.4)",
        2, 0, true, false, false, "Mesa", "20.2.4"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (Apple, Apple A12Z, OpenGL 4.1 Metal - 70.12.7)", 2, 0, true,
        false, false, "Apple", "70.12.7"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA Corporation, Quadro P1000/PCIe/SSE2, OpenGL 4.5.0 "
        "NVIDIA "
        "440.100)",
        2, 0, true, false, false, "NVIDIA", "440.100"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA, Vulkan 1.1.119 (NVIDIA Quadro P1000 (0x00001CB1)), "
        "NVIDIA-440.400.0)",
        2, 0, true, false, false, "NVIDIA", "440.400.0"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA, NVIDIA Quadro P1000 Direct3D11 vs_5_0 ps_5_0, "
        "D3D11-23.21.13.9077)",
        2, 0, true, false, true, "NVIDIA", "23.21.13.9077"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA, NVIDIA Quadro P1000 Direct3D9Ex vs_3_0 ps_3_0, "
        "nvldumdx.dll-23.21.13.9077)",
        2, 0, true, false, true, "NVIDIA", "23.21.13.9077"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA Corporation, Quadro P1000/PCIe/SSE2, OpenGL 4.5.0 "
        "NVIDIA "
        "390.77)",
        2, 0, true, false, false, "NVIDIA", "390.77"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (NVIDIA, Vulkan 1.0.65 (NVIDIA Quadro P1000 (0x00001CB1)), "
        "NVIDIA-390.308.0)",
        2, 0, true, false, false, "NVIDIA", "390.308.0"},
       {"OpenGL ES 2.0.0 (ANGLE 2.1.4875 git hash: 32e78475b1c0)",
        "ANGLE (Google, Vulkan 1.1.0 (SwiftShader Device (Subzero) "
        "(0x0000C0DE)), SwiftShader driver-5.0.0)",
        2, 0, true, false, false, "Google", "5.0.0"}}};

  gfx::ExtensionSet extensions;
  for (const auto& test_case : kTestData) {
    GLVersionInfo version_info(test_case.gl_version.data(),
                               test_case.gl_renderer.data(), extensions);
    EXPECT_TRUE(version_info.is_angle);

    EXPECT_EQ(test_case.expected_gl_major, version_info.major_version);
    EXPECT_EQ(test_case.expected_gl_minor, version_info.minor_version);
    EXPECT_EQ(test_case.expected_is_es2, version_info.is_es2);
    EXPECT_EQ(test_case.expected_is_es3, version_info.is_es3);
    EXPECT_EQ(test_case.expected_is_d3d, version_info.is_d3d);
    EXPECT_STREQ(test_case.expected_driver_vendor.data(),
                 version_info.driver_vendor.c_str());
    EXPECT_STREQ(test_case.expected_driver_version.data(),
                 version_info.driver_version.c_str());
  }
}
}
