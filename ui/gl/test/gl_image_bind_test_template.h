// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_IMAGE_BIND_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_BIND_TEST_TEMPLATE_H_

#include "ui/gl/test/gl_image_test_template.h"

namespace gl {

template <typename GLImageTestDelegate>
class GLImageBindTest : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_SUITE_P(GLImageBindTest);

TYPED_TEST_P(GLImageBindTest, BindTexImage) {
  if (this->delegate_.SkipTest())
    return;

  const gfx::Size image_size(256, 256);
  const uint8_t* image_color = this->delegate_.GetImageColor();

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

  // Initialize a blue texture of the same size as |image|.
  unsigned target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  glBindTexture(target, texture);

  // Bind |image| to |texture|.
  bool rv = image->BindTexImage(target);
  EXPECT_TRUE(rv);

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
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

REGISTER_TYPED_TEST_SUITE_P(GLImageBindTest, BindTexImage);

}  // namespace gl

#endif  // UI_GL_TEST_GL_IMAGE_BIND_TEST_TEMPLATE_H_
