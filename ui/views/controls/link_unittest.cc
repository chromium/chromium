// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/base_control_test_widget.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class LinkTest : public test::BaseControlTestWidget {
 public:
  LinkTest() = default;
  LinkTest(const LinkTest&) = delete;
  LinkTest& operator=(const LinkTest&) = delete;
  ~LinkTest() override = default;

  void SetUp() override {
    test::BaseControlTestWidget::SetUp();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget()->GetNativeWindow());
  }

  void TearDown() override {
    link_ = nullptr;
    test::BaseControlTestWidget::TearDown();
  }

 protected:
  // Simulates a left mouse button press on `link`.
  void PressLink(Link* link) {
    event_generator()->MoveMouseTo(link->GetBoundsInScreen().CenterPoint());
    event_generator()->PressLeftButton();
  }

  SkColor ExpectedColor(ui::ColorId id) {
    return link()->GetColorProvider()->GetColor(id);
  }

  void CreateWidgetContent(View* container) override {
    // Create a widget containing a link which does not take the full size.
    link_ = container->AddChildView(std::make_unique<Link>(u"TestLink"));
    link_->SetBoundsRect(
        gfx::ScaleToEnclosedRect(container->GetLocalBounds(), 0.5f));
  }

  Link* link() { return link_; }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

 public:
  raw_ptr<Link> link_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace

TEST_F(LinkTest, Metadata) {
  link()->SetMultiLine(true);
  test::TestViewMetadata(link());
}

TEST_F(LinkTest, TestLinkClick) {
  bool link_clicked = false;
  link()->SetCallback(base::BindRepeating(
      [](bool* link_clicked) { *link_clicked = true; }, &link_clicked));
  link()->SizeToPreferredSize();
  gfx::Point point = link()->bounds().CenterPoint();
  ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  link()->OnMouseReleased(release);
  EXPECT_TRUE(link_clicked);
}

TEST_F(LinkTest, TestLinkTap) {
  bool link_clicked = false;
  link()->SetCallback(base::BindRepeating(
      [](bool* link_clicked) { *link_clicked = true; }, &link_clicked));
  link()->SizeToPreferredSize();
  gfx::Point point = link()->bounds().CenterPoint();
  ui::GestureEvent tap_event(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  link()->OnGestureEvent(&tap_event);
  EXPECT_TRUE(link_clicked);
}

// Tests that hovering and unhovering a link adds and removes an underline.
TEST_F(LinkTest, TestUnderlineOnHover) {
  // A link should be underlined.
  const gfx::Rect link_bounds = link()->GetBoundsInScreen();
  const gfx::Point off_link = link_bounds.bottom_right() + gfx::Vector2d(1, 1);
  event_generator()->MoveMouseTo(off_link);
  EXPECT_FALSE(link()->IsMouseHovered());
  const auto link_underlined = [link = link()]() {
    return !!(link->font_list().GetFontStyle() & gfx::Font::UNDERLINE);
  };
  EXPECT_TRUE(link_underlined());

  // A non-hovered link should should be underlined.
  // For a11y, A link should be underlined by default. If forcefuly remove an
  // underline, the underline appears according to hovering.
  link()->SetForceUnderline(false);
  EXPECT_FALSE(link_underlined());

  // Hovering the link should underline it.
  event_generator()->MoveMouseTo(link_bounds.CenterPoint());
  EXPECT_TRUE(link()->IsMouseHovered());
  EXPECT_TRUE(link_underlined());

  // Un-hovering the link should remove the underline again.
  event_generator()->MoveMouseTo(off_link);
  EXPECT_FALSE(link()->IsMouseHovered());
  EXPECT_FALSE(link_underlined());
}

// Tests that focusing and unfocusing a link keeps the underline and adds
// focus ring.
TEST_F(LinkTest, TestUnderlineAndFocusRingOnFocus) {
  const auto link_underlined = [link = link()]() {
    return !!(link->font_list().GetFontStyle() & gfx::Font::UNDERLINE);
  };

  // A non-focused link should be underlined and not have a focus ring.
  EXPECT_TRUE(link_underlined());
  EXPECT_FALSE(views::FocusRing::Get(link())->ShouldPaintForTesting());

  // A focused link should be underlined and it should have a focus ring.
  link()->RequestFocus();
  EXPECT_TRUE(link_underlined());
  EXPECT_TRUE(views::FocusRing::Get(link())->ShouldPaintForTesting());
}

TEST_F(LinkTest, AccessibleProperties) {
  ui::AXNodeData data;
  link()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"TestLink");
  EXPECT_EQ(link()->GetViewAccessibility().GetCachedName(), u"TestLink");
  EXPECT_EQ(data.role, ax::mojom::Role::kLink);
  EXPECT_FALSE(link()->GetViewAccessibility().GetIsIgnored());

  // Setting the accessible name to a non-empty string should replace the name
  // from the link text.
  data = ui::AXNodeData();
  std::u16string accessible_name = u"Accessible Name";
  link()->GetViewAccessibility().SetName(accessible_name);
  link()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            accessible_name);
  EXPECT_EQ(link()->GetViewAccessibility().GetCachedName(), accessible_name);
  EXPECT_EQ(data.role, ax::mojom::Role::kLink);
  EXPECT_FALSE(link()->GetViewAccessibility().GetIsIgnored());

  // Setting the accessible name to an empty string should cause the link text
  // to be used as the name.
  data = ui::AXNodeData();
  link()->GetViewAccessibility().SetName(std::u16string());
  link()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"TestLink");
  EXPECT_EQ(link()->GetViewAccessibility().GetCachedName(), u"TestLink");
  EXPECT_EQ(data.role, ax::mojom::Role::kLink);
  EXPECT_FALSE(link()->GetViewAccessibility().GetIsIgnored());

  // Setting the link to an empty string without setting a new accessible
  // name should cause the view to become "ignored" again.
  data = ui::AXNodeData();
  link()->SetText(std::u16string());
  link()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            std::u16string());
  EXPECT_EQ(link()->GetViewAccessibility().GetCachedName(), std::u16string());
  EXPECT_EQ(data.role, ax::mojom::Role::kLink);
  EXPECT_TRUE(link()->GetViewAccessibility().GetIsIgnored());
}

// Tests that TypographyProvider maps the link styles to the link color ids.
TEST_F(LinkTest, TypographyProviderLinkColorIds) {
  const TypographyProvider& provider = TypographyProvider::Get();

  EXPECT_EQ(ui::kColorLinkForeground,
            provider.GetColorId(style::CONTEXT_LABEL, style::STYLE_LINK));
  EXPECT_EQ(
      ui::kColorLinkForegroundPressed,
      provider.GetColorId(style::CONTEXT_LABEL, style::STYLE_LINK_PRESSED));
  EXPECT_EQ(
      ui::kColorLinkForegroundDisabled,
      provider.GetColorId(style::CONTEXT_LABEL, style::STYLE_LINK_DISABLED));

  EXPECT_EQ(
      ui::kColorLinkForegroundOnBubbleFooter,
      provider.GetColorId(style::CONTEXT_BUBBLE_FOOTER, style::STYLE_LINK));
  EXPECT_EQ(ui::kColorLinkForegroundPressedOnBubbleFooter,
            provider.GetColorId(style::CONTEXT_BUBBLE_FOOTER,
                                style::STYLE_LINK_PRESSED));
  EXPECT_EQ(ui::kColorLinkForegroundDisabled,
            provider.GetColorId(style::CONTEXT_BUBBLE_FOOTER,
                                style::STYLE_LINK_DISABLED));
}

// Tests the color of a link in the default context in each state.
TEST_F(LinkTest, GetColor) {
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForeground), link()->GetColor());

  PressLink(link());
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundPressed), link()->GetColor());

  link()->OnMouseCaptureLost();
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForeground), link()->GetColor());

  link()->SetEnabled(false);
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundDisabled),
            link()->GetColor());
}

// Tests the color of a link in the bubble footer context in each state.
TEST_F(LinkTest, GetColorOnBubbleFooter) {
  Link* footer_link = widget()->GetContentsView()->AddChildView(
      std::make_unique<Link>(u"FooterLink", style::CONTEXT_BUBBLE_FOOTER));
  footer_link->SizeToPreferredSize();

  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundOnBubbleFooter),
            footer_link->GetColor());

  PressLink(footer_link);
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundPressedOnBubbleFooter),
            footer_link->GetColor());

  footer_link->OnMouseCaptureLost();
  footer_link->SetEnabled(false);
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundDisabled),
            footer_link->GetColor());
}

// Tests that an explicitly set enabled color overrides the normal and pressed
// colors, but not the disabled color.
TEST_F(LinkTest, GetColorWithRequestedEnabledColor) {
  link()->SetEnabledColor(SK_ColorMAGENTA);
  EXPECT_EQ(SK_ColorMAGENTA, link()->GetColor());

  PressLink(link());
  EXPECT_EQ(SK_ColorMAGENTA, link()->GetColor());

  link()->OnMouseCaptureLost();
  link()->SetEnabled(false);
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundDisabled),
            link()->GetColor());
}

// Tests that the enabled, non-pressed color is derived from the link's text
// style, while the pressed and disabled colors are not.
TEST_F(LinkTest, GetColorUsesTextStyle) {
  for (int text_style : {style::STYLE_LINK_2, style::STYLE_LINK_3,
                         style::STYLE_LINK_4, style::STYLE_LINK_5}) {
    SCOPED_TRACE(testing::Message() << "text_style: " << text_style);
    link()->SetTextStyle(text_style);
    EXPECT_EQ(ExpectedColor(ui::kColorLinkForeground), link()->GetColor());
  }

  link()->SetTextStyle(style::STYLE_SECONDARY);
  EXPECT_EQ(ExpectedColor(TypographyProvider::Get().GetColorId(
                style::CONTEXT_LABEL, style::STYLE_SECONDARY)),
            link()->GetColor());

  PressLink(link());
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundPressed), link()->GetColor());

  link()->OnMouseCaptureLost();
  link()->SetEnabled(false);
  EXPECT_EQ(ExpectedColor(ui::kColorLinkForegroundDisabled),
            link()->GetColor());
}

}  // namespace views
