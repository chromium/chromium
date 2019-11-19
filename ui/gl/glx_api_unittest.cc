// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_implementation.h"

namespace gl {

class GLXApiTest : public testing::Test {
 public:
  void SetUp() override {
    fake_extension_string_ = "";

    g_driver_glx.ClearBindings();
    g_driver_glx.fn.glXQueryExtensionsStringFn = &FakeQueryExtensionsString;
    SetGLImplementation(kGLImplementationMockGL);
    SetGLGetProcAddressProc(
        static_cast<GLGetProcAddressProc>(&FakeGLGetProcAddress));
  }

  void TearDown() override {
    g_current_glx_context = nullptr;
    api_.reset(nullptr);
    g_driver_glx.ClearBindings();

    fake_extension_string_ = "";
  }

  void InitializeAPI(const char* disabled_extensions) {
    api_ = std::make_unique<RealGLXApi>();
    g_current_glx_context = api_.get();
    api_->Initialize(&g_driver_glx);
    if (disabled_extensions) {
      SetDisabledExtensionsGLX(disabled_extensions);
    }
    g_driver_glx.InitializeExtensionBindings();
  }

  void SetFakeExtensionString(const char* fake_string) {
    fake_extension_string_ = fake_string;
  }

  const char* GetExtensions() {
    return api_->glXQueryExtensionsStringFn(reinterpret_cast<Display*>(0x1), 0);
  }

  static GLXContext FakeCreateContextAttribsARB(Display* dpy,
                                                GLXFBConfig config,
                                                GLXContext share_context,
                                                int direct,
                                                const int* attrib_list) {
    return static_cast<GLXContext>(nullptr);
  }

  static GLFunctionPointerType GL_BINDING_CALL
  FakeGLGetProcAddress(const char* proc) {
    if (!strcmp("glXCreateContextAttribsARB", proc)) {
      return reinterpret_cast<GLFunctionPointerType>(
          &FakeCreateContextAttribsARB);
    }
    return NULL;
  }

  static const char* GL_BINDING_CALL FakeQueryExtensionsString(Display* dpy,
                                                               int screen) {
    return fake_extension_string_;
  }

 protected:
  static const char* fake_extension_string_;

  std::unique_ptr<RealGLXApi> api_;
};

const char* GLXApiTest::fake_extension_string_ = "";

TEST_F(GLXApiTest, DisabledExtensionBitTest) {
  static const char* kFakeExtensions = "GLX_ARB_create_context";
  static const char* kFakeDisabledExtensions = "GLX_ARB_create_context";

  SetFakeExtensionString(kFakeExtensions);
  InitializeAPI(nullptr);

  EXPECT_TRUE(g_driver_glx.ext.b_GLX_ARB_create_context);

  InitializeAPI(kFakeDisabledExtensions);

  EXPECT_FALSE(g_driver_glx.ext.b_GLX_ARB_create_context);
}

TEST_F(GLXApiTest, DisabledExtensionStringTest) {
  static const char* kFakeExtensions = "EGL_EXT_1 EGL_EXT_2"
                                       " EGL_EXT_3 EGL_EXT_4";
  static const char* kFakeDisabledExtensions =
      "EGL_EXT_1,EGL_EXT_2,EGL_FAKE";
  static const char* kFilteredExtensions = "EGL_EXT_3 EGL_EXT_4";

  SetFakeExtensionString(kFakeExtensions);
  InitializeAPI(nullptr);

  EXPECT_STREQ(kFakeExtensions, GetExtensions());

  InitializeAPI(kFakeDisabledExtensions);

  EXPECT_STREQ(kFilteredExtensions, GetExtensions());
}

}  // namespace gl
