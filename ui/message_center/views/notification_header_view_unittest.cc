// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/vector_icons.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace message_center {

class NotificationHeaderViewTest : public views::ViewsTestBase {
 public:
  NotificationHeaderViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  NotificationHeaderViewTest(const NotificationHeaderViewTest&) = delete;
  NotificationHeaderViewTest& operator=(const NotificationHeaderViewTest&) =
      delete;
  ~NotificationHeaderViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));
    views::View* container =
        widget_.SetContentsView(std::make_unique<views::View>());

    notification_header_view_ =
        new NotificationHeaderView(views::Button::PressedCallback());
    container->AddChildView(notification_header_view_.get());

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  bool MatchesAppIconColor(SkColor color) {
    SkBitmap expected =
        *gfx::CreateVectorIcon(kProductIcon, kSmallImageSizeMD, color).bitmap();
    SkBitmap actual =
        *notification_header_view_->app_icon_for_testing().bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  }

  bool MatchesExpandIconColor(SkColor color) {
    constexpr int kExpandIconSize = 8;
    SkBitmap expected = *gfx::CreateVectorIcon(kNotificationExpandMoreIcon,
                                               kExpandIconSize, color)
                             .bitmap();
    SkBitmap actual =
        *notification_header_view_->expand_button()->GetImage().bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  }

 protected:
  raw_ptr<NotificationHeaderView, DanglingUntriaged> notification_header_view_ =
      nullptr;

 private:
  views::Widget widget_;
};

TEST_F(NotificationHeaderViewTest, UpdatesTimestampOverTime) {
  auto* timestamp_view =
      notification_header_view_->timestamp_view_for_testing();

  notification_header_view_->SetTimestamp(base::Time::Now() + base::Hours(3) +
                                          base::Minutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            timestamp_view->GetText());

  task_environment()->FastForwardBy(base::Hours(3));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            timestamp_view->GetText());

  task_environment()->FastForwardBy(base::Minutes(30));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      timestamp_view->GetText());

  task_environment()->FastForwardBy(base::Days(2));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            timestamp_view->GetText());
}

TEST_F(NotificationHeaderViewTest, AllowsHidingOfAppIcon) {
  // The icon should be shown by default...
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());

  // ... though it can be explicitly hidden...
  notification_header_view_->SetAppIconVisible(false);
  EXPECT_FALSE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());

  // ... and shown again.
  notification_header_view_->SetAppIconVisible(true);
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->IsDrawn());
}

TEST_F(NotificationHeaderViewTest, SetProgress) {
  int progress = 50;
  std::u16string expected_summary_text = l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress);

  notification_header_view_->SetProgress(progress);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, SetOverflowIndicator) {
  int count = 10;
  std::u16string expected_summary_text = l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count);

  notification_header_view_->SetOverflowIndicator(count);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, SetSummaryText) {
  std::u16string expected_summary_text = u"summary";

  notification_header_view_->SetSummaryText(expected_summary_text);

  auto* summary_text = notification_header_view_->summary_text_for_testing();
  EXPECT_TRUE(summary_text->GetVisible());
  EXPECT_EQ(expected_summary_text, summary_text->GetText());
}

TEST_F(NotificationHeaderViewTest, TimestampHiddenWithProgress) {
  auto* timestamp_view =
      notification_header_view_->timestamp_view_for_testing();
  notification_header_view_->SetTimestamp(base::Time::Now());

  // We do not show the timestamp viev if there is a progress view.
  notification_header_view_->SetProgress(/*progress=*/50);
  EXPECT_FALSE(timestamp_view->GetVisible());

  // Make sure we show the timestamp view with overflow indicators.
  notification_header_view_->SetOverflowIndicator(/*count=*/10);
  EXPECT_TRUE(timestamp_view->GetVisible());

  // Make sure we show the timestamp view with summary text.
  notification_header_view_->SetSummaryText(u"summary");
  EXPECT_TRUE(timestamp_view->GetVisible());
}

TEST_F(NotificationHeaderViewTest, ColorContrastEnforcement) {
  notification_header_view_->SetSummaryText(u"summary");
  auto* summary_text = notification_header_view_->summary_text_for_testing();
  notification_header_view_->ClearAppIcon();
  notification_header_view_->SetExpandButtonEnabled(true);
  notification_header_view_->SetExpanded(false);

  // A bright background should enforce dark enough icons.
  notification_header_view_->SetBackgroundColor(SK_ColorWHITE);
  notification_header_view_->SetColor(SK_ColorWHITE);
  SkColor expected_color =
      color_utils::BlendForMinContrast(SK_ColorWHITE, SK_ColorWHITE).color;
  EXPECT_EQ(expected_color, summary_text->GetEnabledColor());
  EXPECT_TRUE(MatchesAppIconColor(expected_color));
  EXPECT_TRUE(MatchesExpandIconColor(expected_color));

  // A dark background should enforce bright enough icons.
  notification_header_view_->SetBackgroundColor(SK_ColorBLACK);
  notification_header_view_->SetColor(SK_ColorBLACK);
  expected_color =
      color_utils::BlendForMinContrast(SK_ColorBLACK, SK_ColorBLACK).color;
  EXPECT_EQ(expected_color, summary_text->GetEnabledColor());
  EXPECT_TRUE(MatchesAppIconColor(expected_color));
  EXPECT_TRUE(MatchesExpandIconColor(expected_color));
}

TEST_F(NotificationHeaderViewTest, DefaultFocusBehavior) {
  EXPECT_EQ(views::View::FocusBehavior::ACCESSIBLE_ONLY,
            notification_header_view_->GetFocusBehavior());
}

TEST_F(NotificationHeaderViewTest, AppIconAndExpandButtonNotVisible) {
  // Make sure that app icon and expand button are not visible if used for an
  // ash notification.
  auto notification_header_view = std::make_unique<NotificationHeaderView>(
      views::Button::PressedCallback());
  notification_header_view->SetIsInAshNotificationView(true);

  EXPECT_FALSE(
      notification_header_view->app_icon_view_for_testing()->GetVisible());
  EXPECT_FALSE(notification_header_view->expand_button()->GetVisible());
}

TEST_F(NotificationHeaderViewTest, GroupChildNotificationVisibility) {
  notification_header_view_->SetSummaryText(u"summary");
  notification_header_view_->SetTimestamp(base::Time::Now());

  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->expand_button()->GetVisible());
  EXPECT_TRUE(
      notification_header_view_->summary_text_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->summary_text_divider_->GetVisible());
  EXPECT_TRUE(
      notification_header_view_->timestamp_view_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->timestamp_divider_->GetVisible());

  // For group child notification, all the views except `timestamp_view_` should
  // not be visible.
  notification_header_view_->SetIsInGroupChildNotification(
      /*is_in_group_child_notification=*/true);
  EXPECT_FALSE(
      notification_header_view_->app_icon_view_for_testing()->GetVisible());
  EXPECT_FALSE(notification_header_view_->expand_button()->GetVisible());
  EXPECT_FALSE(
      notification_header_view_->summary_text_for_testing()->GetVisible());
  EXPECT_FALSE(notification_header_view_->summary_text_divider_->GetVisible());
  EXPECT_FALSE(notification_header_view_->timestamp_divider_->GetVisible());
  EXPECT_TRUE(
      notification_header_view_->timestamp_view_for_testing()->GetVisible());

  // Switching back.
  notification_header_view_->SetIsInGroupChildNotification(
      /*is_in_group_child_notification=*/false);
  EXPECT_TRUE(
      notification_header_view_->app_icon_view_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->expand_button()->GetVisible());
  EXPECT_TRUE(
      notification_header_view_->summary_text_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->summary_text_divider_->GetVisible());
  EXPECT_TRUE(
      notification_header_view_->timestamp_view_for_testing()->GetVisible());
  EXPECT_TRUE(notification_header_view_->timestamp_divider_->GetVisible());
}

TEST_F(NotificationHeaderViewTest, AccessibleExpandAndCollapse) {
  notification_header_view_->SetExpandButtonEnabled(true);
  notification_header_view_->SetExpanded(false);

  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  ui::AXNodeData data;

  // Initially the view is collapsed and there are no expanded-changed events.
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            0);

  ax_counter.ResetAllCounts();
  data = ui::AXNodeData();

  // Expanding the view should result the expanded state being present and an
  // expanded-changed event being fired.
  notification_header_view_->SetExpanded(true);
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            1);

  ax_counter.ResetAllCounts();
  data = ui::AXNodeData();

  // Calling `SetExpanded` without the changing the expanded state should not
  // result in an expanded-changed event being fired.
  notification_header_view_->SetExpanded(true);
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            0);

  ax_counter.ResetAllCounts();
  data = ui::AXNodeData();

  // Collapsing the view should result the collapsed state being present and an
  // expanded-changed event being fired.
  notification_header_view_->SetExpanded(false);
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            1);

  ax_counter.ResetAllCounts();
  data = ui::AXNodeData();

  // If the expand button is not enabled, the view cannot be expanded/collapsed.
  // As a result, setting expanded to true should not fire an expanded-changed
  // event, and neither the expanded nor the collapsed state should be present.
  notification_header_view_->SetExpandButtonEnabled(false);
  notification_header_view_->SetExpanded(true);
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            0);

  // Because `SetExpandButtonEnabled` calls `View::SetVisible`, the parent view
  // may or may not fire a children-changed event on the parent. Whether or not
  // it does is based on whether or not the button is "ignored" (not included in
  // the accessibility tree). Because this can depend on a number of factors,
  // including platform-specific differences, base test expectations on the
  // "ignored" state to minimize test flakiness.
  bool expand_button_is_ignored = notification_header_view_->expand_button()
                                      ->GetViewAccessibility()
                                      .GetIsIgnored();
  EXPECT_EQ(
      ax_counter.GetCount(ax::mojom::Event::kChildrenChanged,
                          notification_header_view_->expand_button()->parent()),
      expand_button_is_ignored ? 0 : 1);

  ax_counter.ResetAllCounts();
  data = ui::AXNodeData();

  // If the expand button is re-enabled, the view is once again expandable.
  // However, just re-enabling the button does not toggle the expanded state of
  // the view. As a result, we should expect the previously-set expanded state
  // to be present, but no expanded-changed event fired.
  notification_header_view_->SetExpandButtonEnabled(true);
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kExpandedChanged,
                                notification_header_view_),
            0);

  // A focusable, visible expand button should never be "ignored" on any
  // platform. As a result, `View::SetVisible` should fire a children-changed
  // event on the parent view.
  expand_button_is_ignored = notification_header_view_->expand_button()
                                 ->GetViewAccessibility()
                                 .GetIsIgnored();
  EXPECT_FALSE(expand_button_is_ignored);
  EXPECT_EQ(
      ax_counter.GetCount(ax::mojom::Event::kChildrenChanged,
                          notification_header_view_->expand_button()->parent()),
      1);
}

TEST_F(NotificationHeaderViewTest, AccessibleNameTest) {
  ui::AXNodeData data;
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(data.GetNameFrom(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  data = ui::AXNodeData();
  notification_header_view_->SetAppName(u"Some app name");
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Some app name");
}

TEST_F(NotificationHeaderViewTest, AccessibleRoleTest) {
  ui::AXNodeData data;
  notification_header_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGenericContainer);

  data = ui::AXNodeData();
  notification_header_view_->expand_button()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName), u"");
  EXPECT_EQ(data.GetNameFrom(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

TEST_F(NotificationHeaderViewTest, AccessibleDescription) {
  EXPECT_EQ(
      notification_header_view_->GetViewAccessibility().GetCachedDescription(),
      u" ");

  notification_header_view_->SetSummaryText(u"Example Summary");

  EXPECT_EQ(
      notification_header_view_->GetViewAccessibility().GetCachedDescription(),
      u"Example Summary ");

  auto* timestamp_view =
      notification_header_view_->timestamp_view_for_testing();

  notification_header_view_->SetTimestamp(base::Time::Now() + base::Hours(3) +
                                          base::Minutes(30));

  EXPECT_EQ(
      notification_header_view_->GetViewAccessibility().GetCachedDescription(),
      u"Example Summary " + timestamp_view->GetText());

  int progress = 1;
  notification_header_view_->SetProgress(progress);

  EXPECT_EQ(
      notification_header_view_->GetViewAccessibility().GetCachedDescription(),
      l10n_util::GetStringFUTF16Int(
          IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress) +
          u" " + timestamp_view->GetText());

  int count = 4;
  notification_header_view_->SetOverflowIndicator(count);

  EXPECT_EQ(
      notification_header_view_->GetViewAccessibility().GetCachedDescription(),
      l10n_util::GetStringFUTF16Int(
          IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR,
          count) +
          u" " + timestamp_view->GetText());
}

TEST_F(NotificationHeaderViewTest, MetadataTest) {
  views::test::TestViewMetadata(notification_header_view_);
}

}  // namespace message_center
