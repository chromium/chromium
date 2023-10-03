// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link_fragment.h"

#include <array>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/base_control_test_widget.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr char16_t kLinkLabel[] = u"Test label";

class LinkFragmentTest : public test::BaseControlTestWidget {
 public:
  LinkFragmentTest() {
    for (auto& fragment : fragments_) {
      fragment = nullptr;
    }
  }
  ~LinkFragmentTest() override = default;

  void SetUp() override {
    test::BaseControlTestWidget::SetUp();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget()->GetNativeWindow());
  }

  void TearDown() override {
    fragments_.fill(nullptr);
    test::BaseControlTestWidget::TearDown();
  }

 protected:
  void CreateWidgetContent(View* container) override {
    // Fragment 0 is stand-alone.
    fragments_[0] =
        container->AddChildView(std::make_unique<LinkFragment>(kLinkLabel));
    gfx::Rect current_rect =
        gfx::ScaleToEnclosedRect(container->GetLocalBounds(), 0.3f);
    fragments_[0]->SetBoundsRect(current_rect);
    int width = current_rect.width();

    // Fragments 1 and 2 are connected.
    current_rect.Offset(width, 0);
    fragments_[1] =
        container->AddChildView(std::make_unique<LinkFragment>(kLinkLabel));
    fragments_[1]->SetBoundsRect(current_rect);

    current_rect.Offset(width, 0);
    fragments_[2] = container->AddChildView(std::make_unique<LinkFragment>(
        kLinkLabel, style::CONTEXT_LABEL, style::STYLE_LINK, fragment(1)));
    fragments_[2]->SetBoundsRect(current_rect);
  }

  LinkFragment* fragment(size_t index) {
    DCHECK_LT(index, 3u);
    return fragments_[index];
  }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  // Returns bounds of the fragment.
  gfx::Rect GetBoundsForFragment(size_t index) {
    return fragment(index)->GetBoundsInScreen();
  }

 private:
  std::array<raw_ptr<LinkFragment>, 3> fragments_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace

TEST_F(LinkFragmentTest, Metadata) {
  for (size_t index = 0; index < 3; ++index) {
    // Needed to avoid failing DCHECK when setting maximum width.
    fragment(index)->SetMultiLine(true);
    test::TestViewMetadata(fragment(index));
  }
}

// Tests that hovering and unhovering a link adds and removes an underline
// under all connected fragments.
TEST_F(LinkFragmentTest, TestUnderlineOnHover) {
  // A link fragment should be underlined.
  const gfx::Point point_outside =
      GetBoundsForFragment(2).bottom_right() + gfx::Vector2d(1, 1);
  event_generator()->MoveMouseTo(point_outside);
  EXPECT_FALSE(fragment(0)->IsMouseHovered());

  const auto is_underlined = [this](size_t index) {
    return !!(fragment(index)->font_list().GetFontStyle() &
              gfx::Font::UNDERLINE);
  };
  EXPECT_TRUE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  // A non-hovered link fragment should not be underlined.
  fragment(0)->SetForceUnderline(false);
  fragment(1)->SetForceUnderline(false);
  fragment(2)->SetForceUnderline(false);
  EXPECT_FALSE(is_underlined(0));
  EXPECT_FALSE(is_underlined(1));
  EXPECT_FALSE(is_underlined(2));

  // Hovering the first link fragment underlines it.
  event_generator()->MoveMouseTo(GetBoundsForFragment(0).CenterPoint());
  EXPECT_TRUE(fragment(0)->IsMouseHovered());
  EXPECT_TRUE(is_underlined(0));
  // The other link fragments stay non-hovered.
  EXPECT_FALSE(is_underlined(1));
  EXPECT_FALSE(is_underlined(2));

  // Un-hovering the link removes the underline again.
  event_generator()->MoveMouseTo(point_outside);
  EXPECT_FALSE(fragment(0)->IsMouseHovered());
  EXPECT_FALSE(is_underlined(0));
  EXPECT_FALSE(is_underlined(1));
  EXPECT_FALSE(is_underlined(2));

  // Hovering the second link fragment underlines both the second and the
  // third fragment.
  event_generator()->MoveMouseTo(GetBoundsForFragment(1).CenterPoint());
  EXPECT_TRUE(fragment(1)->IsMouseHovered());
  EXPECT_FALSE(fragment(2)->IsMouseHovered());
  EXPECT_FALSE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  // The same is true for hovering the third fragment.
  event_generator()->MoveMouseTo(GetBoundsForFragment(2).CenterPoint());
  EXPECT_TRUE(fragment(2)->IsMouseHovered());
  EXPECT_FALSE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  // Moving outside removes the underline again.
  event_generator()->MoveMouseTo(point_outside);
  EXPECT_FALSE(is_underlined(0));
  EXPECT_FALSE(is_underlined(1));
  EXPECT_FALSE(is_underlined(2));
}

// Tests that focusing and unfocusing a link keeps the underline and adds a
// focus ring for all connected fragments.
TEST_F(LinkFragmentTest, TestUnderlineAndFocusRingOnFocus) {
  const auto is_underlined = [this](size_t index) {
    return !!(fragment(index)->font_list().GetFontStyle() &
              gfx::Font::UNDERLINE);
  };

  // A non-focused link fragment should be underlined.
  EXPECT_TRUE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  EXPECT_FALSE(views::FocusRing::Get(fragment(0))->ShouldPaintForTesting());
  EXPECT_FALSE(views::FocusRing::Get(fragment(1))->ShouldPaintForTesting());
  EXPECT_FALSE(views::FocusRing::Get(fragment(2))->ShouldPaintForTesting());

  // Focusing on fragment 0, which is standalone, will only show focus ring for
  // that fragment.
  fragment(0)->RequestFocus();

  EXPECT_TRUE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  EXPECT_TRUE(views::FocusRing::Get(fragment(0))->ShouldPaintForTesting());
  EXPECT_FALSE(views::FocusRing::Get(fragment(1))->ShouldPaintForTesting());
  EXPECT_FALSE(views::FocusRing::Get(fragment(2))->ShouldPaintForTesting());

  // Focusing on fragment 1, which is connected to fragment 2, will focus both
  // fragments 1 and 2.
  fragment(1)->RequestFocus();

  EXPECT_TRUE(is_underlined(0));
  EXPECT_TRUE(is_underlined(1));
  EXPECT_TRUE(is_underlined(2));

  EXPECT_FALSE(views::FocusRing::Get(fragment(0))->ShouldPaintForTesting());
  EXPECT_TRUE(views::FocusRing::Get(fragment(1))->ShouldPaintForTesting());
  EXPECT_TRUE(views::FocusRing::Get(fragment(2))->ShouldPaintForTesting());
}

}  // namespace views
