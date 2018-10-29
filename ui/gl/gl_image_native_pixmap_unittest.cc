// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_native_pixmap.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gl {

namespace {

const uint8_t kImageColor[] = {0x30, 0x40, 0x10, 0xFF};

template <gfx::BufferFormat format>
class GLImageNativePixmapTestDelegate : public GLImageTestDelegateBase {
 public:
  base::Optional<GLImplementation> GetPreferedGLImplementation()
      const override {
    return base::Optional<GLImplementation>(kGLImplementationEGLGLES2);
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

    scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
        size, gl::GLImageNativePixmap::GetInternalFormatForTesting(format)));
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

TYPED_TEST_CASE_P(GLImageNativePixmapToDmabufTest);

TYPED_TEST_P(GLImageNativePixmapToDmabufTest, GLTexture2DToDmabuf) {
  if (this->delegate_.SkipTest())
    return;

  const gfx::Size image_size(64, 64);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  scoped_refptr<GLImageNativePixmap> image =
      this->delegate_.CreateSolidColorImage(image_size, image_color);
  ASSERT_TRUE(image);

  gfx::NativePixmapHandle native_pixmap_handle = image->ExportHandle();

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(this->delegate_.GetBufferFormat());
  EXPECT_EQ(num_planes, native_pixmap_handle.planes.size());

  std::vector<base::ScopedFD> scoped_fds;
  for (auto& fd : native_pixmap_handle.fds) {
    EXPECT_TRUE(fd.auto_close);
    scoped_fds.emplace_back(fd.fd);
    EXPECT_TRUE(scoped_fds.back().is_valid());
  }
}

// This test verifies that GLImageNativePixmap can be exported as dmabuf fds.
REGISTER_TYPED_TEST_CASE_P(GLImageNativePixmapToDmabufTest,
                           GLTexture2DToDmabuf);

using GLImageTestTypes = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::RGBX_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferFormat::BGRA_8888>>;

#if !defined(MEMORY_SANITIZER)
// Fails under MSAN: crbug.com/886995
INSTANTIATE_TYPED_TEST_CASE_P(GLImageNativePixmap,
                              GLImageTest,
                              GLImageTestTypes);

INSTANTIATE_TYPED_TEST_CASE_P(GLImageNativePixmap,
                              GLImageOddSizeTest,
                              GLImageTestTypes);

INSTANTIATE_TYPED_TEST_CASE_P(GLImageNativePixmap,
                              GLImageNativePixmapToDmabufTest,
                              GLImageTestTypes);
#endif

}  // namespace

}  // namespace gl
