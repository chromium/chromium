// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/path_win.h"

#include <stddef.h>

#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/win/scoped_gdi_object.h"
#include "skia/ext/skia_utils_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"

namespace gfx {

namespace {

// Get rectangles from the |region| and convert them to SkIRect.
std::vector<SkIRect> GetRectsFromHRGN(HRGN region) {
  // Determine the size of output buffer required to receive the region.
  DWORD bytes_size = GetRegionData(region, 0, NULL);
  CHECK_NE((DWORD)0, bytes_size);

  // Fetch the Windows RECTs that comprise the region.
  std::vector<char> buffer(bytes_size);
  LPRGNDATA region_data = reinterpret_cast<LPRGNDATA>(buffer.data());
  DWORD result = GetRegionData(region, bytes_size, region_data);
  CHECK_EQ(bytes_size, result);

  // Pull out the rectangles into a SkIRect vector to return to caller.
  base::span<RECT> rects(reinterpret_cast<RECT*>(&region_data->Buffer[0]),
                         region_data->rdh.nCount);
  std::vector<SkIRect> sk_rects(rects.size());
  base::ranges::transform(rects, sk_rects.begin(), skia::RECTToSkIRect);

  return sk_rects;
}

}  // namespace

// Test that rectangle with round corners stil has round corners after
// converting from SkPath to the HRGN.
// FIXME: this test is fragile (it depends on rrect rasterization impl)
TEST(CreateHRGNFromSkPathTest, RoundCornerTest) {
  const SkIRect rects[] = {
      { 16, 0, 34, 1 },
      { 12, 1, 38, 2 },
      { 10, 2, 40, 3 },
      { 9, 3, 41, 4 },
      { 7, 4, 43, 5 },
      { 6, 5, 44, 6 },
      { 5, 6, 45, 7 },
      { 4, 7, 45, 8 },
      { 4, 8, 46, 9 },
      { 3, 9, 47, 10 },
      { 2, 10, 47, 11 },
      { 2, 11, 48, 12 },
      { 1, 12, 49, 16 },
      { 0, 16, 50, 34 },
      { 1, 34, 49, 38 },
      { 2, 38, 48, 39 },
      { 2, 39, 47, 40 },
      { 3, 40, 47, 41 },
      { 4, 41, 46, 42 },
      { 4, 42, 45, 43 },
      { 5, 43, 45, 44 },
      { 6, 44, 44, 45 },
      { 8, 45, 42, 46 },
      { 9, 46, 41, 47 },
      { 11, 47, 39, 48 },
      { 12, 48, 38, 49 },
      { 16, 49, 34, 50 },
  };

  SkPath path;
  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(50, 50), 20, 20);
  path.addRRect(rrect);
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  const std::vector<SkIRect>& region_rects = GetRectsFromHRGN(region.get());
  EXPECT_EQ(std::size(rects), region_rects.size());
  for (size_t i = 0; i < std::size(rects) && i < region_rects.size(); ++i)
    EXPECT_EQ(rects[i], region_rects[i]);
}

// Check that a path enclosing two non-adjacent areas is correctly translated
// to a non-contiguous region.
TEST(CreateHRGNFromSkPathTest, NonContiguousPath) {
  const SkIRect rects[] = {
      { 0, 0, 50, 50},
      { 100, 100, 150, 150},
  };

  SkPath path;
  for (const SkIRect& rect : rects) {
    path.addRect(SkRect::Make(rect));
  }
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  const std::vector<SkIRect>& region_rects = GetRectsFromHRGN(region.get());
  ASSERT_EQ(std::size(rects), region_rects.size());
  for (size_t i = 0; i < std::size(rects); ++i)
    EXPECT_EQ(rects[i], region_rects[i]);
}

// Check that empty region is returned for empty path.
TEST(CreateHRGNFromSkPathTest, EmptyPath) {
  SkPath path;
  base::win::ScopedRegion empty_region(::CreateRectRgn(0, 0, 0, 0));
  base::win::ScopedRegion region(CreateHRGNFromSkPath(path));
  EXPECT_TRUE(::EqualRgn(empty_region.get(), region.get()));
}

}  // namespace gfx

