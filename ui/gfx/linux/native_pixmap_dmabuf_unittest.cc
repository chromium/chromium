// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/native_pixmap_dmabuf.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace gfx {

class NativePixmapDmaBufTest
    : public ::testing::TestWithParam<gfx::BufferFormat> {
 protected:
  gfx::NativePixmapHandle CreateMockNativePixmapHandle(
      gfx::Size image_size,
      const gfx::BufferFormat format) {
    gfx::NativePixmapHandle handle;
    handle.modifier = 1;
    const int num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
    for (int i = 0; i < num_planes; ++i) {
      // These values are arbitrarily chosen to be different from each other.
      const int stride = (i + 1) * image_size.width();
      const int offset = i * image_size.width() * image_size.height();
      const uint64_t size = stride * image_size.height();
      base::ScopedFD fd(open("/dev/zero", O_RDONLY));
      EXPECT_TRUE(fd.is_valid());

      handle.planes.emplace_back(stride, offset, size, std::move(fd));
    }

    return handle;
  }
};

INSTANTIATE_TEST_SUITE_P(ConvertTest,
                         NativePixmapDmaBufTest,
                         ::testing::Values(gfx::BufferFormat::RGBX_8888,
                                           gfx::BufferFormat::YVU_420));

// Verifies NativePixmapDmaBuf conversion from and to NativePixmapHandle.
TEST_P(NativePixmapDmaBufTest, Convert) {
  const gfx::BufferFormat format = GetParam();
  const gfx::Size image_size(128, 64);

  gfx::NativePixmapHandle handle =
      CreateMockNativePixmapHandle(image_size, format);

  gfx::NativePixmapHandle handle_clone = CloneHandleForIPC(handle);

  // NativePixmapHandle to NativePixmapDmabuf
  scoped_refptr<gfx::NativePixmap> native_pixmap_dmabuf(
      new gfx::NativePixmapDmaBuf(image_size, format, std::move(handle)));
  EXPECT_TRUE(native_pixmap_dmabuf->AreDmaBufFdsValid());
  EXPECT_EQ(native_pixmap_dmabuf->GetBufferFormatModifier(),
            handle_clone.modifier);
  // NativePixmap to NativePixmapHandle.
  const size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(
      native_pixmap_dmabuf->GetBufferFormat());
  for (size_t i = 0; i < num_planes; ++i) {
    EXPECT_EQ(native_pixmap_dmabuf->GetDmaBufPitch(i),
              handle_clone.planes[i].stride);
    EXPECT_EQ(native_pixmap_dmabuf->GetDmaBufOffset(i),
              handle_clone.planes[i].offset);
    EXPECT_EQ(native_pixmap_dmabuf->GetDmaBufPlaneSize(i),
              static_cast<size_t>(handle_clone.planes[i].size));
  }
}

}  // namespace gfx
