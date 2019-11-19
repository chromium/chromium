// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/paint_info.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/base/region.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace views {
namespace {

using PaintInfos = std::vector<std::unique_ptr<PaintInfo>>;

// Device scale factors
constexpr float DSF100 = 1.f;
constexpr float DSF125 = 1.25f;
constexpr float DSF150 = 1.5f;
constexpr float DSF160 = 1.6f;
constexpr float DSF166 = 1.66f;

const std::vector<float> kDsfList = {DSF100, DSF125, DSF150, DSF160, DSF166};

constexpr gfx::Size kLayerSize(123, 456);

//  ___________
// |     1     |
// |___________|
// | 3 | 4 | 5 | <-- 2 (encapsulates 3, 4 and 5)
// |___|___|___|
// |   7   | 8 | <-- 6 (encapsulates 7 and 8)
// |_______|___|
//
// |r_0| encapsulates 1, 2 and 6.
const gfx::Rect r_0(kLayerSize);

constexpr gfx::Rect r_1(0, 0, 123, 152);

constexpr gfx::Rect r_2(0, 152, 123, 152);
constexpr gfx::Rect r_3(0, 0, 41, 152);
constexpr gfx::Rect r_4(41, 0, 41, 152);
constexpr gfx::Rect r_5(82, 0, 41, 152);

constexpr gfx::Rect r_6(0, 304, 123, 152);
constexpr gfx::Rect r_7(0, 0, 82, 152);
constexpr gfx::Rect r_8(82, 0, 41, 152);

// Verifies that the child recording bounds completely cover the parent
// recording bounds.
void VerifyChildBoundsCoversParent(const PaintInfo* parent_paint_info,
                                   const std::vector<PaintInfo*>& info_list) {
  cc::Region remaining(gfx::Rect(parent_paint_info->paint_recording_size()));
  int times_empty = 0;
  for (auto* const paint_info : info_list) {
    const gfx::Rect& child_recording_bounds =
        paint_info->paint_recording_bounds() -
        parent_paint_info->paint_recording_bounds().OffsetFromOrigin();
    EXPECT_TRUE(remaining.Contains(child_recording_bounds))
        << "Remaining: " << remaining.ToString()
        << " paint recording bounds: " << child_recording_bounds.ToString();
    remaining.Subtract(child_recording_bounds);
    times_empty += remaining.IsEmpty();
  }
  EXPECT_EQ(times_empty, 1);
}

void VerifyPixelCanvasCornerScaling(const PaintInfos& info_list) {
  // child 1, child 2 and child 6 should completely cover child 0.
  std::vector<PaintInfo*> child_info_list;
  child_info_list.push_back(info_list[1].get());
  child_info_list.push_back(info_list[2].get());
  child_info_list.push_back(info_list[6].get());
  VerifyChildBoundsCoversParent(info_list[0].get(), child_info_list);
  child_info_list.clear();

  // Child 3,4 and 5 should completely cover child 2.
  child_info_list.push_back(info_list[3].get());
  child_info_list.push_back(info_list[4].get());
  child_info_list.push_back(info_list[5].get());
  VerifyChildBoundsCoversParent(info_list[2].get(), child_info_list);
  child_info_list.clear();

  // Child 7 and 8 should completely cover child 6.
  child_info_list.push_back(info_list[7].get());
  child_info_list.push_back(info_list[8].get());
  VerifyChildBoundsCoversParent(info_list[6].get(), child_info_list);
  child_info_list.clear();
}

void VerifyPixelSizesAreSameAsDIPSize(const PaintInfos& info_list) {
  EXPECT_EQ(info_list[0]->paint_recording_size(), r_0.size());

  EXPECT_EQ(info_list[1]->paint_recording_size(), r_1.size());

  EXPECT_EQ(info_list[2]->paint_recording_size(), r_2.size());
  EXPECT_EQ(info_list[3]->paint_recording_size(), r_3.size());
  EXPECT_EQ(info_list[4]->paint_recording_size(), r_4.size());
  EXPECT_EQ(info_list[5]->paint_recording_size(), r_5.size());

  EXPECT_EQ(info_list[6]->paint_recording_size(), r_6.size());
  EXPECT_EQ(info_list[7]->paint_recording_size(), r_7.size());
  EXPECT_EQ(info_list[8]->paint_recording_size(), r_8.size());
}

}  // namespace

class PaintInfoTest : public ::testing::Test {
 public:
  PaintInfoTest() = default;

  ~PaintInfoTest() override = default;

  //  ___________
  // |     1     |
  // |___________|
  // | 3 | 4 | 5 | <-- 2 (encapsulates 3, 4 and 5)
  // |___|___|___|
  // |   7   | 8 | <-- 6 (encapsulates 7 and 8)
  // |_______|___|
  //
  // |r_0| encapsulates 1, 2 and 6.
  //
  // Returns the following arrangement of paint recording bounds for the given
  // |dsf|
  PaintInfos GetPaintInfoSetup(const ui::PaintContext& context) {
    PaintInfos info_list(9);

    info_list[0].reset(new PaintInfo(context, kLayerSize));

    info_list[1].reset(
        new PaintInfo(*info_list[0], r_1, r_0.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));

    info_list[2].reset(
        new PaintInfo(*info_list[0], r_2, r_0.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));
    info_list[3].reset(
        new PaintInfo(*info_list[2], r_3, r_2.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));
    info_list[4].reset(
        new PaintInfo(*info_list[2], r_4, r_2.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));
    info_list[5].reset(
        new PaintInfo(*info_list[2], r_5, r_2.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));

    info_list[6].reset(
        new PaintInfo(*info_list[0], r_6, r_0.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));
    info_list[7].reset(
        new PaintInfo(*info_list[6], r_7, r_6.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));
    info_list[8].reset(
        new PaintInfo(*info_list[6], r_8, r_6.size(),
                      PaintInfo::ScaleType::kScaleWithEdgeSnapping, false));

    return info_list;
  }

  void VerifyInvalidationRects(float dsf, bool pixel_canvas_enabled) {
    std::vector<gfx::Rect> invalidation_rects = {
        gfx::Rect(0, 0, 123, 41),     // Intersects with 0 & 1.
        gfx::Rect(0, 76, 60, 152),    // Intersects 0, 1, 2, 3 & 4.
        gfx::Rect(41, 152, 41, 152),  // Intersects with 0, 2 & 4.
        gfx::Rect(80, 320, 4, 4),     // Intersects with 0, 6, 7 & 8.
        gfx::Rect(40, 151, 43, 154),  // Intersects all
        gfx::Rect(82, 304, 1, 1),     // Intersects with 0, 6 & 8.
        gfx::Rect(81, 303, 2, 2)      // Intersects with 0, 2, 4, 5, 6, 7, 8
    };

    std::vector<std::vector<int>> repaint_indices = {
        std::vector<int>{0, 1},
        std::vector<int>{0, 1, 2, 3, 4},
        std::vector<int>{0, 2, 4},
        std::vector<int>{0, 6, 7, 8},
        std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8},
        std::vector<int>{0, 6, 8},
        std::vector<int>{0, 2, 4, 5, 6, 7, 8}};

    PaintInfos info_list;

    EXPECT_EQ(repaint_indices.size(), invalidation_rects.size());
    for (size_t i = 0; i < invalidation_rects.size(); i++) {
      ui::PaintContext context(nullptr, dsf, invalidation_rects[i],
                               pixel_canvas_enabled);
      info_list = GetPaintInfoSetup(context);
      for (int repaint_index : repaint_indices[i]) {
        EXPECT_TRUE(info_list[repaint_index]->context().IsRectInvalid(
            gfx::Rect(info_list[repaint_index]->paint_recording_size())));
      }
      info_list.clear();
    }
  }
};

TEST_F(PaintInfoTest, CornerScalingPixelCanvasEnabled) {
  PaintInfos info_list;
  for (float dsf : kDsfList) {
    ui::PaintContext context(nullptr, dsf, gfx::Rect(), true);
    info_list = GetPaintInfoSetup(context);
    VerifyPixelCanvasCornerScaling(info_list);
    info_list.clear();
  }

  // More accurate testing for 1.25 dsf
  ui::PaintContext context(nullptr, DSF125, gfx::Rect(), true);
  info_list = GetPaintInfoSetup(context);
  VerifyPixelCanvasCornerScaling(info_list);
  EXPECT_EQ(info_list[0]->paint_recording_size(), gfx::Size(154, 570));

  EXPECT_EQ(info_list[1]->paint_recording_size(), gfx::Size(154, 190));

  EXPECT_EQ(info_list[2]->paint_recording_bounds(),
            gfx::Rect(0, 190, 154, 190));
  EXPECT_EQ(info_list[3]->paint_recording_size(), gfx::Size(51, 190));
  EXPECT_EQ(info_list[4]->paint_recording_bounds(),
            gfx::Rect(51, 190, 52, 190));
  EXPECT_EQ(info_list[5]->paint_recording_bounds(),
            gfx::Rect(103, 190, 51, 190));

  EXPECT_EQ(info_list[6]->paint_recording_bounds(),
            gfx::Rect(0, 380, 154, 190));
  EXPECT_EQ(info_list[7]->paint_recording_size(), gfx::Size(103, 190));
  EXPECT_EQ(info_list[8]->paint_recording_bounds(),
            gfx::Rect(103, 380, 51, 190));
}

TEST_F(PaintInfoTest, ScalingWithPixelCanvasDisabled) {
  for (float dsf : kDsfList) {
    ui::PaintContext context(nullptr, dsf, gfx::Rect(), false);
    PaintInfos info_list = GetPaintInfoSetup(context);
    VerifyPixelCanvasCornerScaling(info_list);
    VerifyPixelSizesAreSameAsDIPSize(info_list);
    info_list.clear();
  }
}

TEST_F(PaintInfoTest, Invalidation) {
  for (float dsf : kDsfList) {
    VerifyInvalidationRects(dsf, false);
    VerifyInvalidationRects(dsf, true);
  }
}

// Make sure the PaintInfo used for view's layer uses the
// corderedbounds.
TEST_F(PaintInfoTest, LayerPaintInfo) {
  const gfx::Rect kViewBounds(15, 20, 7, 13);
  struct TestData {
    const float dsf;
    const gfx::Size size;
  };
  const TestData kTestData[6]{
      {1.0f, {7, 13}},    // rounded    enclosing (if these scaling is appleid)
      {1.25f, {9, 16}},   // 9x16       10x17
      {1.5f, {10, 20}},   // 11x20      11x20
      {1.6f, {11, 21}},   // 11x21      12x21
      {1.75f, {13, 23}},  // 12x23      13x23
      {2.f, {14, 26}},    // 14x26      14x26
  };
  for (const TestData& data : kTestData) {
    SCOPED_TRACE(testing::Message() << "dsf:" << data.dsf);
    ui::PaintContext context(nullptr, data.dsf, gfx::Rect(), true);
    PaintInfo parent_paint_info(context, gfx::Size());
    PaintInfo paint_info = PaintInfo::CreateChildPaintInfo(
        parent_paint_info, kViewBounds, gfx::Size(),
        PaintInfo::ScaleType::kScaleWithEdgeSnapping, true);
    EXPECT_EQ(gfx::Rect(data.size), paint_info.paint_recording_bounds());
  }
}

}  // namespace views
