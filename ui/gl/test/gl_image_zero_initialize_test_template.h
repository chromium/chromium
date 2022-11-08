// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_IMAGE_ZERO_INITIALIZE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_ZERO_INITIALIZE_TEST_TEMPLATE_H_

#include "build/build_config.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gl {

template <typename GLImageTestDelegate>
class GLImageZeroInitializeTest : public GLImageTest<GLImageTestDelegate> {};

// This test verifies that if an uninitialized image is bound to a texture, the
// result is zero-initialized.
TYPED_TEST_SUITE_P(GLImageZeroInitializeTest);

TYPED_TEST_P(GLImageZeroInitializeTest, ZeroInitialize) {
  if (this->delegate_.SkipTest(this->display_))
    GTEST_SKIP() << "Skip ZeroInitialize because GL initialization failed";

  const gfx::Size image_size(256, 256);

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create an uninitialized image of preferred format.
  scoped_refptr<GLImage> image = this->delegate_.CreateImage(image_size);

  // Create a texture that |image| will be bound to.
  GLenum target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  glBindTexture(target, texture);

  // Bind |image| to |texture|.
  bool rv = image->BindTexImage(target);
  EXPECT_TRUE(rv);

  // Draw |texture| to viewport.
  internal::DrawTextureQuad(target, image_size);

  // Release |image| from |texture|.
  image->ReleaseTexImage(target);

  // Read back pixels to check expectations.
  const uint8_t zero_color[] = {0, 0, 0, 0};
  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            zero_color);

  // Clean up.
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
}

REGISTER_TYPED_TEST_SUITE_P(GLImageZeroInitializeTest, ZeroInitialize);

}  // namespace gl

#endif  // UI_GL_TEST_GL_IMAGE_ZERO_INITIALIZE_TEST_TEMPLATE_H_
