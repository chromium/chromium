// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

namespace ui {
namespace {

scoped_refptr<gfx::NativePixmap> ReturnProtectedPixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    const gfx::NativePixmapHandle& handle) {
  return pixmap;
}

}  // namespace

class GbmSurfaceFactoryTest : public testing::Test {
 protected:
  GbmSurfaceFactoryTest() : surface_factory_(&drm_thread_proxy_) {}

  DrmThreadProxy drm_thread_proxy_;
  GbmSurfaceFactory surface_factory_;
};

TEST_F(GbmSurfaceFactoryTest,
       CreateNativePixmapFromHandleAcceptsMatchingProtectedPixmap) {
  scoped_refptr<gfx::NativePixmap> protected_pixmap =
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          gfx::Size(100, 100), viz::MultiPlaneFormat::kNV12,
          gfx::NativePixmapHandle());
  surface_factory_.SetGetProtectedNativePixmapDelegate(
      base::BindRepeating(&ReturnProtectedPixmap, protected_pixmap));

  scoped_refptr<gfx::NativePixmap> pixmap =
      surface_factory_.CreateNativePixmapFromHandle(
          gfx::kNullAcceleratedWidget, gfx::Size(100, 100),
          viz::MultiPlaneFormat::kNV12, gfx::NativePixmapHandle());
  EXPECT_EQ(pixmap, protected_pixmap);
}

TEST_F(GbmSurfaceFactoryTest,
       CreateNativePixmapFromHandleRejectsProtectedPixmapWithMismatchedSize) {
  scoped_refptr<gfx::NativePixmap> protected_pixmap =
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          gfx::Size(16, 16), viz::MultiPlaneFormat::kNV12,
          gfx::NativePixmapHandle());
  surface_factory_.SetGetProtectedNativePixmapDelegate(
      base::BindRepeating(&ReturnProtectedPixmap, protected_pixmap));

  scoped_refptr<gfx::NativePixmap> pixmap =
      surface_factory_.CreateNativePixmapFromHandle(
          gfx::kNullAcceleratedWidget, gfx::Size(100, 100),
          viz::MultiPlaneFormat::kNV12, gfx::NativePixmapHandle());
  EXPECT_FALSE(pixmap);
}

TEST_F(GbmSurfaceFactoryTest,
       CreateNativePixmapFromHandleRejectsProtectedPixmapWithMismatchedFormat) {
  scoped_refptr<gfx::NativePixmap> protected_pixmap =
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
          gfx::Size(100, 100), viz::SinglePlaneFormat::kBGRA_8888,
          gfx::NativePixmapHandle());
  surface_factory_.SetGetProtectedNativePixmapDelegate(
      base::BindRepeating(&ReturnProtectedPixmap, protected_pixmap));

  scoped_refptr<gfx::NativePixmap> pixmap =
      surface_factory_.CreateNativePixmapFromHandle(
          gfx::kNullAcceleratedWidget, gfx::Size(100, 100),
          viz::MultiPlaneFormat::kNV12, gfx::NativePixmapHandle());
  EXPECT_FALSE(pixmap);
}

}  // namespace ui
