// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gl {
namespace {

const uint8_t kGreen[] = {0x0, 0xFF, 0x0, 0xFF};

template <gfx::BufferFormat format>
class GLImageRefCountedMemoryTestDelegate : public GLImageTestDelegateBase {
 public:
  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    DCHECK_EQ(NumberOfPlanesForLinearBufferFormat(format), 1u);
    std::vector<uint8_t> data(gfx::BufferSizeForBufferFormat(size, format));
    scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes(data));
    GLImageTestSupport::SetBufferDataToColor(
        size.width(), size.height(),
        static_cast<int>(RowSizeForBufferFormat(size.width(), format, 0)), 0,
        format, color, &bytes->data().front());
    auto image = base::MakeRefCounted<GLImageRefCountedMemory>(size);
    bool rv = image->Initialize(bytes.get(), format);
    EXPECT_TRUE(rv);
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_2D; }
  const uint8_t* GetImageColor() { return kGreen; }
  int GetAdmissibleError() const { return 0; }
};

using GLImageTestTypes = testing::Types<
    GLImageRefCountedMemoryTestDelegate<gfx::BufferFormat::RGBX_8888>,
    GLImageRefCountedMemoryTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageRefCountedMemoryTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageRefCountedMemoryTestDelegate<gfx::BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageRefCountedMemory,
                               GLImageTest,
                               GLImageTestTypes);

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageRefCountedMemory,
                               GLImageCopyTest,
                               GLImageTestTypes);

}  // namespace
}  // namespace gl
