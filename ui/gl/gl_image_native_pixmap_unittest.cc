// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_native_pixmap.h"

#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/test/gl_image_test_template.h"

// TODO(crbug.com/969798): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_GLTexture2DToDmabuf DISABLED_GLTexture2DToDmabuf
#else
#define MAYBE_GLTexture2DToDmabuf GLTexture2DToDmabuf
#endif

// TYPED_TEST_P() and REGISTER_TYPED_TEST_SUITE_P() don't do macro expansion on
// their parameters, making the MAYBE_ technique above not work -- these macros
// are a workaround.
#define TYPED_TEST_P_WITH_EXPANSION(SuiteName, TestName) \
  TYPED_TEST_P(SuiteName, TestName)
#define REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(SuiteName, ...) \
  REGISTER_TYPED_TEST_SUITE_P(SuiteName, __VA_ARGS__)

namespace gl {

namespace {

const uint8_t kImageColor[] = {0x30, 0x40, 0x10, 0xFF};

template <gfx::BufferFormat format>
class GLImageNativePixmapTestDelegate : public GLImageTestDelegateBase {
 public:
  base::Optional<GLImplementation> GetPreferedGLImplementation()
      const override {
#if defined(OS_WIN)
    return base::Optional<GLImplementation>(kGLImplementationEGLANGLE);
#else
    return base::Optional<GLImplementation>(kGLImplementationEGLGLES2);
#endif
  }

  bool SkipTest() const override {
    const std::string dmabuf_import_ext = "EGL_MESA_image_dma_buf_export";
    std::string platform_extensions(DriverEGL::GetPlatformExtensions());
    gfx::ExtensionSet extensions(gfx::MakeExtensionSet(platform_extensions));
    if (!gfx::HasExtension(extensions, dmabuf_import_ext)) {
      LOG(WARNING) << "Skip test, missing extension " << dmabuf_import_ext;
      return true;
    }

    return false;
  }

  scoped_refptr<GLImageNativePixmap> CreateSolidColorImage(
      const gfx::Size& size,
      const uint8_t color[4]) const {
    GLuint texture_id = GLTestHelper::CreateTexture(GetTextureTarget());
    EXPECT_NE(0u, texture_id);

    std::unique_ptr<uint8_t[]> pixels(
        new uint8_t[BufferSizeForBufferFormat(size, format)]);
    GLImageTestSupport::SetBufferDataToColor(
        size.width(), size.height(),
        static_cast<int>(RowSizeForBufferFormat(size.width(), format, 0)), 0,
        format, color, pixels.get());

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

    auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
    EXPECT_TRUE(image->InitializeFromTexture(texture_id));

    glDeleteTextures(1, &texture_id);
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_2D; }

  const uint8_t* GetImageColor() const { return kImageColor; }

  int GetAdmissibleError() const { return 0; }

  gfx::BufferFormat GetBufferFormat() const { return format; }
};

template <typename GLImageTestDelegate>
class GLImageNativePixmapToDmabufTest
    : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_SUITE_P(GLImageNativePixmapToDmabufTest);

TYPED_TEST_P_WITH_EXPANSION(GLImageNativePixmapToDmabufTest,
                            MAYBE_GLTexture2DToDmabuf) {
  if (this->delegate_.SkipTest())
    return;

  const gfx::Size image_size(64, 64);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  scoped_refptr<GLImageNativePixmap> image =
      this->delegate_.CreateSolidColorImage(image_size, image_color);
  ASSERT_TRUE(image);

  gfx::NativePixmapHandle native_pixmap_handle = image->ExportHandle();

  for (auto& plane : native_pixmap_handle.planes) {
    EXPECT_TRUE(plane.fd.is_valid());
  }
}

// This test verifies that GLImageNativePixmap can be exported as dmabuf fds.
REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(GLImageNativePixmapToDmabufTest,
                                           MAYBE_GLTexture2DToDmabuf);

using GLImageTestTypes = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::RGBX_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::BGRA_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::RGBX_1010102>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::BGRX_1010102>>;

#if !defined(MEMORY_SANITIZER)
// Fails under MSAN: crbug.com/886995
INSTANTIATE_TYPED_TEST_SUITE_P(GLImageNativePixmap,
                               GLImageTest,
                               GLImageTestTypes);

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageNativePixmap,
                               GLImageOddSizeTest,
                               GLImageTestTypes);

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageNativePixmap,
                               GLImageNativePixmapToDmabufTest,
                               GLImageTestTypes);
#endif

}  // namespace

}  // namespace gl
