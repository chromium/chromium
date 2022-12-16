// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_

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
    GTEST_SKIP() << "Skip because GL initialization failed";

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
  EXPECT_EQ(small_image->GetSizeForTesting().ToString(),
            small_image_size.ToString());
  EXPECT_EQ(large_image->GetSizeForTesting().ToString(),
            large_image_size.ToString());
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
    GTEST_SKIP() << "Skip because GL initialization failed";

  const gfx::Size odd_image_size(17, 53);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  // Create an odd-sized solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> odd_image =
      this->delegate_.CreateSolidColorImage(odd_image_size, image_color);
  ASSERT_TRUE(odd_image);

  // Verify that image size is correct.
  EXPECT_EQ(odd_image->GetSizeForTesting().ToString(),
            odd_image_size.ToString());
}

// The GLImageTest test case verifies the behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(GLImageOddSizeTest, MAYBE_Create);

}  // namespace gl

// Avoid polluting source files that include this header.
#undef MAYBE_Create
#undef TYPED_TEST_P_WITH_EXPANSION
#undef REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION

#endif  // UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
