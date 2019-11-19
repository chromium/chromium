// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

class GLApiTest : public testing::Test {
 public:
  void SetUp() override {
    fake_extension_string_ = "";
    fake_version_string_ = "";
    num_fake_extension_strings_ = 0;
    fake_extension_strings_ = nullptr;

    SetGLGetProcAddressProc(
        static_cast<GLGetProcAddressProc>(&FakeGLGetProcAddress));
  }

  static GLFunctionPointerType GL_BINDING_CALL
  FakeGLGetProcAddress(const char* proc) {
    return reinterpret_cast<GLFunctionPointerType>(0x1);
  }

  void TearDown() override {
    api_.reset(nullptr);
    driver_.reset(nullptr);

    SetGLImplementation(kGLImplementationNone);
    fake_extension_string_ = "";
    fake_version_string_ = "";
    num_fake_extension_strings_ = 0;
    fake_extension_strings_ = nullptr;
  }

  void InitializeAPI(const char* disabled_extensions) {
    driver_ = std::make_unique<DriverGL>();
    driver_->fn.glGetStringFn = &FakeGetString;
    driver_->fn.glGetStringiFn = &FakeGetStringi;
    driver_->fn.glGetIntegervFn = &FakeGetIntegervFn;

    api_ = std::make_unique<RealGLApi>();
    if (disabled_extensions) {
      api_->SetDisabledExtensions(disabled_extensions);
    }
    api_->Initialize(driver_.get());

    std::string extensions_string =
        GetGLExtensionsFromCurrentContext(api_.get());
    gfx::ExtensionSet extension_set = gfx::MakeExtensionSet(extensions_string);

    auto version = std::make_unique<GLVersionInfo>(
        reinterpret_cast<const char*>(api_->glGetStringFn(GL_VERSION)),
        reinterpret_cast<const char*>(api_->glGetStringFn(GL_RENDERER)),
        extension_set);

    driver_->InitializeDynamicBindings(version.get(), extension_set);
    api_->set_version(std::move(version));
  }

  void SetFakeExtensionString(const char* fake_string) {
    SetGLImplementation(kGLImplementationDesktopGL);
    fake_extension_string_ = fake_string;
    fake_version_string_ = "2.1";
  }

  void SetFakeExtensionStrings(const char** fake_strings, uint32_t count) {
    SetGLImplementation(kGLImplementationDesktopGL);
    num_fake_extension_strings_ = count;
    fake_extension_strings_ = fake_strings;
    fake_version_string_ = "3.0";
  }

  static const GLubyte* GL_BINDING_CALL FakeGetString(GLenum name) {
    if (name == GL_VERSION)
      return reinterpret_cast<const GLubyte*>(fake_version_string_);
    return reinterpret_cast<const GLubyte*>(fake_extension_string_);
  }

  static void GL_BINDING_CALL FakeGetIntegervFn(GLenum pname, GLint* params) {
    *params = num_fake_extension_strings_;
  }

  static const GLubyte* GL_BINDING_CALL FakeGetStringi(GLenum name,
                                                       GLuint index) {
    return (index < num_fake_extension_strings_) ?
           reinterpret_cast<const GLubyte*>(fake_extension_strings_[index]) :
           nullptr;
  }

  const char* GetExtensions() {
    return reinterpret_cast<const char*>(api_->glGetStringFn(GL_EXTENSIONS));
  }

  uint32_t GetNumExtensions() {
    GLint num_extensions = 0;
    api_->glGetIntegervFn(GL_NUM_EXTENSIONS, &num_extensions);
    return static_cast<uint32_t>(num_extensions);
  }

  const char* GetExtensioni(uint32_t index) {
    return reinterpret_cast<const char*>(api_->glGetStringiFn(GL_EXTENSIONS,
                                                              index));
  }

 protected:
  static const char* fake_extension_string_;
  static const char* fake_version_string_;

  static uint32_t num_fake_extension_strings_;
  static const char** fake_extension_strings_;

  std::unique_ptr<DriverGL> driver_;
  std::unique_ptr<RealGLApi> api_;
};

const char* GLApiTest::fake_extension_string_ = "";
const char* GLApiTest::fake_version_string_ = "";

uint32_t GLApiTest::num_fake_extension_strings_ = 0;
const char** GLApiTest::fake_extension_strings_ = nullptr;

TEST_F(GLApiTest, DisabledExtensionStringTest) {
  static const char* kFakeExtensions = "GL_EXT_1 GL_EXT_2 GL_EXT_3 GL_EXT_4";
  static const char* kFakeDisabledExtensions = "GL_EXT_1,GL_EXT_2,GL_FAKE";
  static const char* kFilteredExtensions = "GL_EXT_3 GL_EXT_4";

  SetFakeExtensionString(kFakeExtensions);
  InitializeAPI(nullptr);
  EXPECT_STREQ(kFakeExtensions, GetExtensions());

  InitializeAPI(kFakeDisabledExtensions);
  EXPECT_STREQ(kFilteredExtensions, GetExtensions());
}

TEST_F(GLApiTest, DisabledExtensionBitTest) {
  static const char* kFakeExtensions[] = {
    "GL_ARB_timer_query"
  };
  static const char* kFakeDisabledExtensions = "GL_ARB_timer_query";

  SetFakeExtensionStrings(kFakeExtensions, base::size(kFakeExtensions));
  InitializeAPI(nullptr);
  EXPECT_TRUE(driver_->ext.b_GL_ARB_timer_query);

  InitializeAPI(kFakeDisabledExtensions);
  EXPECT_FALSE(driver_->ext.b_GL_ARB_timer_query);
}

TEST_F(GLApiTest, DisabledExtensionStringIndexTest) {
  static const char* kFakeExtensions[] = {
    "GL_EXT_1",
    "GL_EXT_2",
    "GL_EXT_3",
    "GL_EXT_4"
  };
  static const char* kFakeDisabledExtensions = "GL_EXT_1,GL_EXT_2,GL_FAKE";
  static const char* kFilteredExtensions[] = {
    "GL_EXT_3",
    "GL_EXT_4"
  };

  SetFakeExtensionStrings(kFakeExtensions, base::size(kFakeExtensions));
  InitializeAPI(nullptr);

  EXPECT_EQ(base::size(kFakeExtensions), GetNumExtensions());
  for (uint32_t i = 0; i < base::size(kFakeExtensions); ++i) {
    EXPECT_STREQ(kFakeExtensions[i], GetExtensioni(i));
  }

  InitializeAPI(kFakeDisabledExtensions);
  EXPECT_EQ(base::size(kFilteredExtensions), GetNumExtensions());
  for (uint32_t i = 0; i < base::size(kFilteredExtensions); ++i) {
    EXPECT_STREQ(kFilteredExtensions[i], GetExtensioni(i));
  }
}

}  // namespace gl
