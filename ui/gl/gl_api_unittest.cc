// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>
#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_split.h"
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
  static GLFunctionPointerType GL_BINDING_CALL
  FakeGLGetProcAddress(const char* proc) {
    return reinterpret_cast<GLFunctionPointerType>(0x1);
  }

  void SetUp() override {
    fake_extension_string_ = "";
    fake_version_string_ = "";

    SetGLGetProcAddressProc(
        static_cast<GLGetProcAddressProc>(&FakeGLGetProcAddress));
  }

  void TearDown() override {
    TerminateAPI();
    SetGLImplementation(kGLImplementationNone);
    fake_extension_string_ = "";
    fake_version_string_ = "";
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

  void TerminateAPI() {
    api_.reset();
    driver_.reset();
  }

  void SetFakeExtensionString(const char* fake_string) {
    SetGLImplementation(kGLImplementationEGLANGLE);
    fake_extension_string_ = fake_string;
    fake_version_string_ = "OpenGL ES 3.0";
  }

  static const GLubyte* GL_BINDING_CALL FakeGetString(GLenum name) {
    if (name == GL_VERSION)
      return reinterpret_cast<const GLubyte*>(fake_version_string_);
    return reinterpret_cast<const GLubyte*>(fake_extension_string_);
  }

  static void GL_BINDING_CALL FakeGetIntegervFn(GLenum pname, GLint* params) {
    std::vector<std::string> extensions =
        base::SplitString(fake_extension_string_, " ", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    *params = static_cast<GLint>(extensions.size());
  }

  static const GLubyte* GL_BINDING_CALL FakeGetStringi(GLenum name,
                                                       GLuint index) {
    // |extensions| needs to be static so we can return c_str() from
    // its elements.
    static std::vector<std::string> extensions;
    extensions =
        base::SplitString(fake_extension_string_, " ", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    GLuint count = static_cast<GLuint>(extensions.size());
    return (index < count)
               ? reinterpret_cast<const GLubyte*>(extensions[index].c_str())
               : nullptr;
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

  std::unique_ptr<DriverGL> driver_;
  std::unique_ptr<RealGLApi> api_;
};

const char* GLApiTest::fake_extension_string_ = "";
const char* GLApiTest::fake_version_string_ = "";

TEST_F(GLApiTest, DisabledExtensionStringTest) {
  static constexpr std::string_view kFakeExtensions =
      "GL_EXT_1 GL_EXT_2 GL_EXT_3 GL_EXT_4";
  static constexpr std::string_view kFakeDisabledExtensions =
      "GL_EXT_1,GL_EXT_2,GL_FAKE";
  static constexpr std::string_view kFilteredExtensions = "GL_EXT_3 GL_EXT_4";

  SetFakeExtensionString(kFakeExtensions.data());
  InitializeAPI(nullptr);
  EXPECT_STREQ(kFakeExtensions.data(), GetExtensions());
  TerminateAPI();

  InitializeAPI(kFakeDisabledExtensions.data());
  EXPECT_STREQ(kFilteredExtensions.data(), GetExtensions());
}

TEST_F(GLApiTest, DisabledExtensionBitTest) {
  static constexpr std::string_view kFakeExtensions =
      "GL_EXT_disjoint_timer_query";
  static constexpr std::string_view kFakeDisabledExtensions =
      "GL_EXT_disjoint_timer_query";

  SetFakeExtensionString(kFakeExtensions.data());
  InitializeAPI(nullptr);
  EXPECT_TRUE(driver_->ext.b_GL_EXT_disjoint_timer_query);
  TerminateAPI();

  InitializeAPI(kFakeDisabledExtensions.data());
  EXPECT_FALSE(driver_->ext.b_GL_EXT_disjoint_timer_query);
}

TEST_F(GLApiTest, DisabledExtensionStringIndexTest) {
  static constexpr std::string_view kFakeExtensions =
      "GL_EXT_1 GL_EXT_2 GL_EXT_3 GL_EXT_4";
  static constexpr std::array<std::string_view, 4> kFakeExtensionList = {
      {"GL_EXT_1", "GL_EXT_2", "GL_EXT_3", "GL_EXT_4"}};
  static constexpr std::string_view kFakeDisabledExtensions =
      "GL_EXT_1,GL_EXT_2,GL_FAKE";
  static constexpr std::array<std::string_view, 2> kFilteredExtensions = {
      {"GL_EXT_3", "GL_EXT_4"}};

  SetFakeExtensionString(kFakeExtensions.data());
  InitializeAPI(nullptr);
  EXPECT_EQ(kFakeExtensionList.size(), GetNumExtensions());
  for (size_t i = 0; i < kFakeExtensionList.size(); ++i) {
    EXPECT_STREQ(kFakeExtensionList[i].data(), GetExtensioni(i));
  }
  TerminateAPI();

  InitializeAPI(kFakeDisabledExtensions.data());
  EXPECT_EQ(std::size(kFilteredExtensions), GetNumExtensions());
  for (size_t i = 0; i < kFilteredExtensions.size(); ++i) {
    EXPECT_STREQ(kFilteredExtensions[i].data(), GetExtensioni(i));
  }
}

}  // namespace gl
