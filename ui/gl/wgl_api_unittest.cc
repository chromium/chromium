// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/gl_wgl_api_implementation.h"

typedef std::pair<std::string, std::string> stringpair;

namespace gl {

class WGLApiTest : public testing::Test {
 public:
  void SetUp() override {
    GLSurfaceWGL::InitializeOneOffForTesting();
    fake_arb_extension_string_ = "";
    fake_ext_extension_string_ = "";

    g_driver_wgl.ClearBindings();
    g_driver_wgl.fn.wglGetExtensionsStringARBFn = &FakeGetExtensionsStringARB;
    g_driver_wgl.fn.wglGetExtensionsStringEXTFn = &FakeGetExtensionsStringEXT;
    SetGLImplementation(kGLImplementationDesktopGL);
    SetGLGetProcAddressProc(&FakeGLGetProcAddress);
  }

  void TearDown() override {
    g_current_wgl_context = nullptr;
    api_.reset(nullptr);
    g_driver_wgl.ClearBindings();

    fake_ext_extension_string_ = "";
    fake_arb_extension_string_ = "";
  }

  void InitializeAPI(const char* disabled_extensions) {
    api_ = std::make_unique<RealWGLApi>();
    g_current_wgl_context = api_.get();
    api_->Initialize(&g_driver_wgl);
    if (disabled_extensions) {
      SetDisabledExtensionsWGL(disabled_extensions);
    }
    g_driver_wgl.InitializeExtensionBindings();
  }

  void SetFakeEXTExtensionString(const char* fake_string) {
    fake_ext_extension_string_ = fake_string;
  }

  void SetFakeARBExtensionString(const char* fake_string) {
    fake_arb_extension_string_ = fake_string;
  }

  stringpair GetExtensions() {
    auto stringify = [](const char* str) -> std::string {
      return str ? str : "";
    };
    return stringpair(
        stringify(wglGetExtensionsStringARB(NULL)),
        stringify(wglGetExtensionsStringEXT()));
  }

  static GLFunctionPointerType WINAPI FakeGLGetProcAddress(const char* proc) {
    return NULL;
  }

  static const char* GL_BINDING_CALL FakeGetExtensionsStringARB(HDC dc) {
    return fake_arb_extension_string_;
  }

  static const char* GL_BINDING_CALL FakeGetExtensionsStringEXT() {
    return fake_ext_extension_string_;
  }

 protected:
  static const char* fake_ext_extension_string_;
  static const char* fake_arb_extension_string_;

  std::unique_ptr<RealWGLApi> api_;
};

const char* WGLApiTest::fake_ext_extension_string_ = "";
const char* WGLApiTest::fake_arb_extension_string_ = "";

TEST_F(WGLApiTest, DisabledExtensionBitTest) {
  static const char* kFakeExtensions = "WGL_ARB_extensions_string";
  static const char* kFakeDisabledExtensions = "WGL_ARB_extensions_string";

  InitializeAPI(nullptr);

  EXPECT_FALSE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);

  // NULL simulates not being able to resolve wglGetExtensionsStringARB
  SetFakeARBExtensionString(nullptr);
  SetFakeEXTExtensionString(kFakeExtensions);

  InitializeAPI(nullptr);
  EXPECT_TRUE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);

  InitializeAPI(kFakeExtensions);
  EXPECT_FALSE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);

  SetFakeARBExtensionString("");
  SetFakeEXTExtensionString(kFakeExtensions);

  InitializeAPI(nullptr);
  // We expect false here, because wglGetExtensionsStringARB
  // always takes precedence over wglGetExtensionsStringEXT
  // if it is available.
  EXPECT_FALSE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);

  SetFakeARBExtensionString(kFakeExtensions);
  SetFakeEXTExtensionString("");

  InitializeAPI(nullptr);
  EXPECT_TRUE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);

  InitializeAPI(kFakeDisabledExtensions);
  EXPECT_FALSE(g_driver_wgl.ext.b_WGL_ARB_extensions_string);
}

TEST_F(WGLApiTest, DisabledExtensionStringTest) {
  static const char* kFakeExtensions = "EGL_EXT_1 EGL_EXT_2"
                                       " EGL_EXT_3 EGL_EXT_4";
  static const char* kFakeDisabledExtensions =
      "EGL_EXT_1,EGL_EXT_2,EGL_FAKE";
  static const char* kFilteredExtensions = "EGL_EXT_3 EGL_EXT_4";

  SetFakeARBExtensionString(kFakeExtensions);
  SetFakeEXTExtensionString(kFakeExtensions);

  InitializeAPI(nullptr);
  EXPECT_EQ(stringpair(kFakeExtensions, kFakeExtensions),
            GetExtensions());

  InitializeAPI(kFakeDisabledExtensions);
  EXPECT_EQ(stringpair(kFilteredExtensions, kFilteredExtensions),
            GetExtensions());
}

}  // namespace gl
