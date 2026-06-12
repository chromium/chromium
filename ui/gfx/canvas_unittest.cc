// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include <algorithm>
#include <array>
#include <limits>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_elider.h"

namespace gfx {

class CanvasTest : public testing::Test {
 protected:
  int GetStringWidth(const char *text) {
    return Canvas::GetStringWidth(base::UTF8ToUTF16(text), font_list_);
  }

  gfx::Size SizeStringInt(const char *text, int width, int line_height) {
    std::u16string text16 = base::UTF8ToUTF16(text);
    int height = 0;
    int flags =
        (text16.find('\n') != std::u16string::npos) ? Canvas::MULTI_LINE : 0;
    Canvas::SizeStringInt(text16, font_list_, &width, &height, line_height,
                          flags);
    return gfx::Size(width, height);
  }

 private:
  FontList font_list_;
};

TEST_F(CanvasTest, StringWidth) {
  EXPECT_GT(GetStringWidth("Test"), 0);
}

TEST_F(CanvasTest, StringWidthEmptyString) {
  EXPECT_EQ(0, GetStringWidth(""));
}

// Strings with less than 4 characters get cached. Check for consistency.
TEST_F(CanvasTest, StringWidthRepeatedCalls) {
  base::test::ScopedFeatureList feature_list(features::kStringWidthCache);

  // The cache is global so it can have values at the start of the test.
  Canvas::GetStringWidthCacheForTesting().Clear();

  int width_1 = GetStringWidth(gfx::kEllipsis);

  // Verify that the cache entry was created.
  EXPECT_EQ(Canvas::GetStringWidthCacheForTesting().size(), 1U);

  int width_2 = GetStringWidth(gfx::kEllipsis);

  // Verify that the repeated call didn't add a new cache entry.
  EXPECT_EQ(Canvas::GetStringWidthCacheForTesting().size(), 1U);

  EXPECT_EQ(width_1, width_2);
}

TEST_F(CanvasTest, StringSizeEmptyString) {
  gfx::Size size = SizeStringInt("", 0, 0);
  EXPECT_EQ(0, size.width());
  EXPECT_GT(size.height(), 0);
}

// Verifies GetClipBounds() returns the correct value.
TEST_F(CanvasTest, ClipRectWithScaling) {
  Canvas canvas(gfx::Size(200, 100), 2.25, true);
  canvas.ClipRect(gfx::RectF(100, 0, 20, 1.7f));
  gfx::Rect clip_rect;
  ASSERT_TRUE(canvas.GetClipBounds(&clip_rect));
  // Use Contains() rather than Equals() as skia may extend the rect in certain
  // directions. None-the-less the clip must contain the region we damaged.
  EXPECT_TRUE(clip_rect.Contains(gfx::Rect(100, 0, 20, 2)));
}

TEST_F(CanvasTest, StringSizeWithLineHeight) {
  gfx::Size one_line_size = SizeStringInt("Q", 0, 0);
  gfx::Size four_line_size = SizeStringInt("Q\nQ\nQ\nQ", 1000000, 1000);
  EXPECT_EQ(one_line_size.width(), four_line_size.width());
  EXPECT_EQ(3 * 1000 + one_line_size.height(), four_line_size.height());
}

namespace {

class IdMutatingMockPlatformFont : public PlatformFont {
 public:
  explicit IdMutatingMockPlatformFont(sk_sp<SkTypeface> typeface)
      : typeface_(typeface) {
    // Lock in the initial Skia ID during simulated platform construction
    set_typeface_unique_id(typeface_ ? typeface_->uniqueID() : 0);
  }

  void SimulateBackendIdMutation(sk_sp<SkTypeface> new_typeface) {
    // Simulate Skia purging and allocating a new SkTypeface with a new ID
    typeface_ = new_typeface;
  }

  // Minimal virtual overrides
  Font DeriveFont(int size_delta,
                  int style,
                  Font::Weight weight) const override {
    return Font();
  }
  int GetHeight() override { return 10; }
  Font::Weight GetWeight() const override { return Font::Weight::NORMAL; }
  int GetBaseline() override { return 10; }
  int GetCapHeight() override { return 10; }
  int GetExpectedTextWidth(int length) override { return length * 5; }
  int GetStyle() const override { return Font::NORMAL; }
  const std::string& GetFontName() const override {
    static const std::string name = "Mock";
    return name;
  }
  std::string GetActualFontName() const override { return "Mock"; }
  std::vector<std::string> GetActualFontNames() const override {
    return {"Mock"};
  }
  int GetFontSize() const override { return 12; }
  const FontRenderParams& GetFontRenderParams() override {
    static FontRenderParams params;
    return params;
  }
  sk_sp<SkTypeface> GetNativeSkTypeface() const override { return typeface_; }

#if BUILDFLAG(IS_APPLE)
  CTFontRef GetCTFont() const override { return nullptr; }
#endif

 private:
  ~IdMutatingMockPlatformFont() override = default;
  sk_sp<SkTypeface> typeface_;
};

}  // namespace

TEST_F(CanvasTest, StringWidthCacheIdMutationResilience) {
  base::test::ScopedFeatureList feature_list(features::kStringWidthCache);
  auto& cache = Canvas::GetStringWidthCacheForTesting();
  cache.Clear();

  // Create different typefaces to simulate ID change.
#if BUILDFLAG(IS_WIN)
  std::string font1 = "Arial";
  std::string font2 = "Courier New";
#else
  std::string font1 = "sans-serif";
  std::string font2 = "monospace";
#endif

  std::array<sk_sp<SkTypeface>, 3> tfs;
  tfs[0] = skia::MakeTypefaceFromName(font1.c_str(), SkFontStyle());
  tfs[1] = skia::MakeTypefaceFromName(font2.c_str(), SkFontStyle());
  tfs[2] = skia::MakeTypefaceFromName("serif", SkFontStyle());

  ASSERT_TRUE(tfs[0]);
  ASSERT_TRUE(tfs[1]);
  ASSERT_TRUE(tfs[2]);

  // Sort by unique ID to guarantee tf_low < tf_mid < tf_high
  std::sort(tfs.begin(), tfs.end(),
            [](const sk_sp<SkTypeface>& a, const sk_sp<SkTypeface>& b) {
              return a->uniqueID() < b->uniqueID();
            });

  sk_sp<SkTypeface> tf_low = tfs[0];
  sk_sp<SkTypeface> tf_mid = tfs[1];
  sk_sp<SkTypeface> tf_high = tfs[2];

  ASSERT_NE(tf_low->uniqueID(), tf_mid->uniqueID());
  ASSERT_NE(tf_mid->uniqueID(), tf_high->uniqueID());

  auto other_font = base::MakeRefCounted<IdMutatingMockPlatformFont>(tf_mid);
  Canvas::StringWidthCacheKey key_other(u"IdMutationTest", other_font);

  auto mock_font = base::MakeRefCounted<IdMutatingMockPlatformFont>(tf_low);
  Canvas::StringWidthCacheKey key(u"IdMutationTest", mock_font);

  // Insert key_other first to make it the root
  cache.Put(key_other, 60.0f);
  cache.Put(key, 50.0f);
  EXPECT_EQ(cache.size(), 2U);

  // Simulate Skia cache purging/re-deriving the physical font ID
  // tf_low -> tf_high.
  // Now key (tf_high) is left child of key_other (tf_mid), which violates BST
  // (tf_high > tf_mid).
  mock_font->SimulateBackendIdMutation(tf_high);

  // Without the fix, the mutated ID will corrupt the RB-tree, causing Get()
  // to return end(). With the fix, the locked ID ensures it is found perfectly.
  EXPECT_NE(cache.Get(key), cache.end());

  // Clean up cache for other tests.
  cache.Clear();
  EXPECT_EQ(cache.size(), 0U);
}

}  // namespace gfx
