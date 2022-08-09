// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringize_macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_helper.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_image_test_support.h"
#include "ui/gl/test/gl_test_helper.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/mac_util.h"
#endif

// TODO(crbug.com/969798): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_Create DISABLED_Create
#else
#define MAYBE_Create Create
#endif

// TYPED_TEST_P() and REGISTER_TYPED_TEST_SUITE_P() don't do macro expansion on
// their parameters, making the MAYBE_ technique above not work -- these macros
// are a workaround.
#define TYPED_TEST_P_WITH_EXPANSION(SuiteName, TestName) \
  TYPED_TEST_P(SuiteName, TestName)
#define REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(SuiteName, ...) \
  REGISTER_TYPED_TEST_SUITE_P(SuiteName, __VA_ARGS__)

namespace gl {

namespace internal {

void DrawTextureQuad(GLenum target, const gfx::Size& size);
}

class GLImageTestDelegateBase {
 public:
  virtual ~GLImageTestDelegateBase() {}

  virtual void DidSetUp() {}
  virtual void WillTearDown() {}

  virtual absl::optional<GLImplementationParts> GetPreferedGLImplementation()
      const;
  virtual bool SkipTest(GLDisplay* display) const;
};

template <typename GLImageTestDelegate>
class GLImageTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    auto prefered_impl = delegate_.GetPreferedGLImplementation();
    display_ = GLImageTestSupport::InitializeGL(prefered_impl);
    surface_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), GLContextAttribs());
    context_->MakeCurrent(surface_.get());
    delegate_.DidSetUp();
  }
  void TearDown() override {
    delegate_.WillTearDown();
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    surface_ = nullptr;
    GLImageTestSupport::CleanupGL(display_);
  }

 protected:
  scoped_refptr<GLSurface> surface_;
  scoped_refptr<GLContext> context_;
  GLImageTestDelegate delegate_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TYPED_TEST_SUITE_P(GLImageTest);

TYPED_TEST_P_WITH_EXPANSION(GLImageTest, MAYBE_Create) {
  if (this->delegate_.SkipTest(this->display_))
    return;

  // NOTE: On some drm devices (mediatek) the mininum width/height to add an fb
  // for a bo must be 64, and YVU_420 in i915 requires at least 128 length.
  const gfx::Size small_image_size(128, 128);
  const gfx::Size large_image_size(512, 512);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  // Create a small solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> small_image =
      this->delegate_.CreateSolidColorImage(small_image_size, image_color);
  ASSERT_TRUE(small_image);

  // Create a large solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> large_image =
      this->delegate_.CreateSolidColorImage(large_image_size, image_color);
  ASSERT_TRUE(large_image);

  // Verify that image size is correct.
  EXPECT_EQ(small_image->GetSize().ToString(), small_image_size.ToString());
  EXPECT_EQ(large_image->GetSize().ToString(), large_image_size.ToString());
}

// The GLImageTest test case verifies the behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(GLImageTest, MAYBE_Create);

template <typename GLImageTestDelegate>
class GLImageOddSizeTest : public GLImageTest<GLImageTestDelegate> {};

// This test verifies that odd-sized GLImages can be created and destroyed.
TYPED_TEST_SUITE_P(GLImageOddSizeTest);

TYPED_TEST_P_WITH_EXPANSION(GLImageOddSizeTest, MAYBE_Create) {
  if (this->delegate_.SkipTest(this->display_))
    return;

  const gfx::Size odd_image_size(17, 53);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  // Create an odd-sized solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> odd_image =
      this->delegate_.CreateSolidColorImage(odd_image_size, image_color);
  ASSERT_TRUE(odd_image);

  // Verify that image size is correct.
  EXPECT_EQ(odd_image->GetSize().ToString(), odd_image_size.ToString());
}

// The GLImageTest test case verifies the behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(GLImageOddSizeTest, MAYBE_Create);

template <typename GLImageTestDelegate>
class GLImageCopyTest : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_SUITE_P(GLImageCopyTest);

TYPED_TEST_P(GLImageCopyTest, CopyTexImage) {
  if (this->delegate_.SkipTest(this->display_))
    return;

  // CopyTexImage follows different code paths depending whether the image is
  // > 1 MiB or not. This range of sizes should cover both possibilities
  // regardless of format.
  const std::vector<gfx::Size> image_size_list{
      {256, 256},
      {512, 512},
      {1024, 1024},
  };
  const uint8_t* image_color = this->delegate_.GetImageColor();
  const uint8_t texture_color[] = {0, 0, 0xff, 0xff};

  GLuint vao = 0;
  if (GLContext::GetCurrent()->GetVersionInfo()->IsAtLeastGL(3, 3)) {
    // To avoid glGetVertexAttribiv(0, ...) failing.
    glGenVertexArraysOES(1, &vao);
    glBindVertexArrayOES(vao);
  }

  for (auto image_size : image_size_list) {
    LOG(INFO) << "Testing with size " << image_size.ToString();
    GLuint framebuffer =
        GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
    ASSERT_TRUE(framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, image_size.width(), image_size.height());

    // Create a solid color green image of preferred format. This must succeed
    // in order for a GLImage to be conformant.
    scoped_refptr<GLImage> image =
        this->delegate_.CreateSolidColorImage(image_size, image_color);
    ASSERT_TRUE(image);

    // Create a solid color blue texture of the same size as |image|.
    unsigned target = this->delegate_.GetTextureTarget();
    GLuint texture = GLTestHelper::CreateTexture(target);
    std::unique_ptr<uint8_t[]> pixels(new uint8_t[BufferSizeForBufferFormat(
        image_size, gfx::BufferFormat::RGBA_8888)]);
    GLImageTestSupport::SetBufferDataToColor(
        image_size.width(), image_size.height(),
        static_cast<int>(RowSizeForBufferFormat(
            image_size.width(), gfx::BufferFormat::RGBA_8888, 0)),
        0, gfx::BufferFormat::RGBA_8888, texture_color, pixels.get());
    glBindTexture(target, texture);
    glTexImage2D(target, 0, GL_RGBA, image_size.width(), image_size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

    // Copy |image| to |texture|.
    bool rv = image->CopyTexImage(target);
    EXPECT_TRUE(rv);

    // Draw |texture| to viewport.
    internal::DrawTextureQuad(target, image_size);

    // Read back pixels to check expectations.
    GLTestHelper::CheckPixelsWithError(
        0, 0, image_size.width(), image_size.height(),
        this->delegate_.GetAdmissibleError(), image_color);

    // Clean up.
    glDeleteTextures(1, &texture);
    glDeleteFramebuffersEXT(1, &framebuffer);
  }

  if (vao) {
    glDeleteVertexArraysOES(1, &vao);
  }
}

// The GLImageCopyTest test case verifies that the GLImage implementation
// handles CopyTexImage correctly.
REGISTER_TYPED_TEST_SUITE_P(GLImageCopyTest, CopyTexImage);

}  // namespace gl

// Avoid polluting source files that include this header.
#undef MAYBE_Create
#undef TYPED_TEST_P_WITH_EXPANSION
#undef REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION

#endif  // UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
