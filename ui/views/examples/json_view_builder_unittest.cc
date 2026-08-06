// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/json_view_builder.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/font_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace views::examples {

namespace {

const char kDefaultSampleJson[] =
    R"({
  "type": "BoxLayoutView",
  "properties": {
    "Orientation": "kVertical",
    "BetweenChildSpacing": "DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL",
    "InsideBorderInsets": "InsetsMetric:INSETS_DIALOG",
    "background": "solid,ColorId:kColorPrimaryBackground"
  },
  "children": [
    {
      "type": "Label",
      "properties": {
        "Text": "Dynamic Views from JSON",
        "HorizontalAlignment": "ALIGN_CENTER",
        "TextStyle": "STYLE_PRIMARY",
        "EnabledColor": "ColorId:kColorAccent"
      }
    },
    {
      "type": "BoxLayoutView",
      "properties": {
        "Orientation": "kHorizontal",
        "BetweenChildSpacing": )"
    R"("DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL",
        "InsideBorderInsets": "InsetsMetric:INSETS_DIALOG_SUBSECTION",
        "border": "solid,1,ColorId:kColorSeparator",
        "background": "solid,ColorId:kColorPrimaryBackground"
      },
      "children": [
        {
          "type": "Label",
          "properties": {
            "Text": "Enter Name:"
          }
        },
        {
          "type": "Textfield",
          "properties": {
            "PlaceholderText": "Type your name...",
            "layout_flex": "1"
          }
        }
      ]
    },
    {
      "type": "BoxLayoutView",
      "properties": {
        "Orientation": "kHorizontal",
        "BetweenChildSpacing": )"
    R"("DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL",
        "InsideBorderInsets": "InsetsMetric:INSETS_DIALOG_SUBSECTION"
      },
      "children": [
        {
          "type": "Checkbox",
          "properties": {
            "Text": "Remember me"
          }
        },
        {
          "type": "ToggleButton",
          "properties": {
            "Visible": "true",
            "AccessibleName": "Notifications Toggle"
          }
        }
      ]
    },
    {
      "type": "MdTextButton",
      "properties": {
        "Text": "Submit Form",
        "Style": "kProminent"
      }
    }
  ]
})";

const char kBootstrapSampleJson[] =
    R"({
  "type": "BoxLayoutView",
  "properties": {
    "Orientation": "kHorizontal",
    "BetweenChildSpacing": 10,
    "InsideBorderInsets": "10,10,10,10"
  },
  "children": [
    {
      "type": "BoxLayoutView",
      "properties": {
        "Orientation": "kVertical",
        "BetweenChildSpacing": 5,
        "layout_flex": "1"
      },
      "children": [
        {
          "type": "Label",
          "properties": {
            "Text": "JSON Script Editor",
            "TextStyle": "STYLE_EMPHASIZED"
          }
        },
        {
          "type": "Textarea",
          "properties": {
            "ID": "1",
            "layout_flex": "1"
          }
        },
        {
          "type": "BoxLayoutView",
          "properties": {
            "Orientation": "kHorizontal",
            "BetweenChildSpacing": 10
          },
          "children": [
            {
              "type": "MdTextButton",
              "properties": {
                "ID": "4",
                "Text": "Render",
                "Style": "kProminent"
              }
            },
            {
              "type": "MdTextButton",
              "properties": {
                "ID": "5",
                "Text": "Open File..."
              }
            },
            {
              "type": "Label",
              "properties": {
                "ID": "2",
                "Text": "Click Render to build tree",
                "HorizontalAlignment": "ALIGN_LEFT",
                "layout_flex": "1"
              }
            }
          ]
        }
      ]
    },
    {
      "type": "BoxLayoutView",
      "properties": {
        "Orientation": "kVertical",
        "BetweenChildSpacing": 5,
        "layout_flex": "1"
      },
      "children": [
        {
          "type": "Label",
          "properties": {
            "Text": "Live Preview",
            "TextStyle": "STYLE_EMPHASIZED"
          }
        },
        {
          "type": "View",
          "properties": {
            "border": "solid,1,0xCCCCCC",
            "UseDefaultFillLayout": "true",
            "layout_flex": "1"
          },
          "children": [
            {
              "type": "View",
              "properties": {
                "ID": "3",
                "UseDefaultFillLayout": "true"
              }
            }
          ]
        }
      ]
    }
  ]
})";

}  // namespace

class JsonViewBuilderTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    static bool initialized = false;
    if (!initialized) {
      initialized = true;
      gfx::InitializeFonts();
      if (!ui::ResourceBundle::HasSharedInstance()) {
        base::FilePath ui_test_pak_path;
        if (base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path)) {
          ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
        }
      }
    }
  }

  static void TearDownTestSuite() {
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  views::test::TestLayoutProvider layout_provider_;
};

TEST_F(JsonViewBuilderTest, TestScrollView) {
  const char kJson[] = R"({
    "type": "ScrollView",
    "properties": {
      "HorizontalScrollBarMode": "kHiddenButEnabled",
      "VerticalScrollBarMode": "kDisabled",
      "TreatAllScrollEventsAsHorizontal": true
    },
    "children": [
      {
        "type": "BoxLayoutView",
        "properties": {
          "Orientation": "kHorizontal",
          "BetweenChildSpacing": )"
                       R"("DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL"
        },
        "children": [
          {
            "type": "View",
            "properties": {
              "ID": 100
            }
          }
        ]
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;
  ASSERT_TRUE(result->is_dict());

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::ScrollView* scroll_view =
      views::AsViewClass<views::ScrollView>(view.get());
  ASSERT_NE(scroll_view, nullptr);
  EXPECT_NE(scroll_view->contents(), nullptr);
  EXPECT_NE(scroll_view->contents()->GetViewByID(100), nullptr);
  EXPECT_EQ(scroll_view->GetHorizontalScrollBarMode(),
            views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  EXPECT_EQ(scroll_view->GetVerticalScrollBarMode(),
            views::ScrollView::ScrollBarMode::kDisabled);
  EXPECT_TRUE(scroll_view->GetTreatAllScrollEventsAsHorizontal());
}

TEST_F(JsonViewBuilderTest, TestScrollViewMultipleChildrenError) {
  const char kJson[] = R"({
    "type": "ScrollView",
    "children": [
      { "type": "View" },
      { "type": "View" }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;
  ASSERT_TRUE(result->is_dict());

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  EXPECT_EQ(view, nullptr);
  EXPECT_EQ(error_msg, "ScrollView can only have a single child view in JSON");
}

TEST_F(JsonViewBuilderTest, TestBoxLayoutView) {
  const char kJson[] = R"({
    "type": "BoxLayoutView",
    "properties": {
      "Orientation": "kVertical",
      "BetweenChildSpacing": 15,
      "InsideBorderInsets": "5,10,15,20"
    },
    "children": [
      {
        "type": "View",
        "properties": {
          "ID": 101,
          "layout_flex": "2"
        }
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::BoxLayoutView* box_layout_view =
      views::AsViewClass<views::BoxLayoutView>(view.get());
  ASSERT_NE(box_layout_view, nullptr);
  EXPECT_EQ(box_layout_view->GetOrientation(),
            views::BoxLayout::Orientation::kVertical);
  EXPECT_EQ(box_layout_view->GetBetweenChildSpacing(), 15);
  EXPECT_EQ(box_layout_view->GetInsideBorderInsets(),
            gfx::Insets::TLBR(5, 10, 15, 20));
}

TEST_F(JsonViewBuilderTest, TestFlexLayoutView) {
  const char kJson[] = R"({
    "type": "FlexLayoutView",
    "properties": {
      "Orientation": "kHorizontal"
    },
    "children": [
      {
        "type": "View",
        "properties": {
          "ID": 200,
          "layout_flex": "1"
        }
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::FlexLayoutView* flex_layout_view =
      views::AsViewClass<views::FlexLayoutView>(view.get());
  ASSERT_NE(flex_layout_view, nullptr);
  EXPECT_EQ(flex_layout_view->GetOrientation(),
            views::LayoutOrientation::kHorizontal);
  EXPECT_NE(flex_layout_view->GetViewByID(200), nullptr);
}

TEST_F(JsonViewBuilderTest, TestDefaultSampleJson) {
  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kDefaultSampleJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  EXPECT_TRUE(views::IsViewClass<views::BoxLayoutView>(view.get()));
  EXPECT_EQ(view->children().size(), 4u);
}

TEST_F(JsonViewBuilderTest, TestBootstrapSampleJson) {
  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kBootstrapSampleJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  EXPECT_TRUE(views::IsViewClass<views::BoxLayoutView>(view.get()));
  EXPECT_EQ(view->children().size(), 2u);
  EXPECT_NE(view->GetViewByID(1), nullptr);
  EXPECT_NE(view->GetViewByID(2), nullptr);
  EXPECT_NE(view->GetViewByID(3), nullptr);
  EXPECT_NE(view->GetViewByID(4), nullptr);
  EXPECT_NE(view->GetViewByID(5), nullptr);
}

TEST_F(JsonViewBuilderTest, TestImageView) {
  const char kJson[] = R"({
    "type": "ImageView",
    "properties": {
      "ImageSize": "24,24",
      "HorizontalAlignment": "kCenter",
      "VerticalAlignment": "kCenter",
      "CornerRadius": 4,
      "TooltipText": "Profile Image",
      "image": "vector_icon:info,24"
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::ImageView* image_view =
      views::AsViewClass<views::ImageView>(view.get());
  ASSERT_NE(image_view, nullptr);
  EXPECT_EQ(image_view->GetImageModel().Size(), gfx::Size(24, 24));
  EXPECT_EQ(image_view->GetHorizontalAlignment(),
            views::ImageView::Alignment::kCenter);
  EXPECT_EQ(image_view->GetVerticalAlignment(),
            views::ImageView::Alignment::kCenter);
  EXPECT_EQ(image_view->GetCornerRadius(), 4);
  EXPECT_EQ(image_view->GetTooltipText(), u"Profile Image");
  EXPECT_FALSE(image_view->GetImageModel().IsEmpty());
}

TEST_F(JsonViewBuilderTest, TestLabel) {
  const char kJson[] = R"({
    "type": "Label",
    "properties": {
      "Text": "Sample Headline",
      "TextStyle": "STYLE_HEADLINE_1",
      "TextContext": "CONTEXT_DIALOG_TITLE",
      "HorizontalAlignment": "ALIGN_RIGHT",
      "EnabledColor": "red"
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::Label* label = views::AsViewClass<views::Label>(view.get());
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->GetText(), u"Sample Headline");
  EXPECT_EQ(label->GetTextStyle(), views::style::STYLE_HEADLINE_1);
  EXPECT_EQ(label->GetTextContext(), views::style::CONTEXT_DIALOG_TITLE);
  EXPECT_EQ(label->GetHorizontalAlignment(), gfx::ALIGN_RIGHT);
  EXPECT_EQ(label->GetEnabledColor(), SK_ColorRED);
}

TEST_F(JsonViewBuilderTest, TestLink) {
  const char kJson[] = R"({
    "type": "Link",
    "properties": {
      "Text": "Click here to learn more",
      "ForceUnderline": true,
      "Enabled": true
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::Link* link = views::AsViewClass<views::Link>(view.get());
  ASSERT_NE(link, nullptr);
  EXPECT_EQ(link->GetText(), u"Click here to learn more");
  EXPECT_TRUE(link->GetForceUnderline());
}

TEST_F(JsonViewBuilderTest, TestSlider) {
  const char kJson[] = R"({
    "type": "Slider",
    "properties": {
      "Value": 0.75,
      "ValueIndicatorRadius": 6,
      "EnableAccessibilityEvents": false,
      "style": "minimal"
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::Slider* slider = views::AsViewClass<views::Slider>(view.get());
  ASSERT_NE(slider, nullptr);
  EXPECT_FLOAT_EQ(slider->GetValue(), 0.75f);
  EXPECT_EQ(slider->GetValueIndicatorRadius(), 6);
  EXPECT_EQ(slider->style(), views::Slider::RenderingStyle::kMinimalStyle);
}

TEST_F(JsonViewBuilderTest, TestStyledLabel) {
  const char kJson[] = R"({
    "type": "StyledLabel",
    "properties": {
      "Text": "This is a styled label with a link inside.",
      "DefaultTextStyle": "STYLE_BODY_2",
      "HorizontalAlignment": "ALIGN_CENTER",
      "ranges": [
        {
          "start": 0,
          "length": 4,
          "style": "STYLE_HEADLINE_4_BOLD",
          "color": "blue"
        },
        {
          "start": 31,
          "length": 4,
          "style": "STYLE_LINK",
          "tooltip": "Link tooltip"
        }
      ]
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::StyledLabel* styled_label =
      views::AsViewClass<views::StyledLabel>(view.get());
  ASSERT_NE(styled_label, nullptr);
  EXPECT_EQ(styled_label->GetText(),
            u"This is a styled label with a link inside.");
  EXPECT_EQ(styled_label->GetDefaultTextStyle(), views::style::STYLE_BODY_2);
}

TEST_F(JsonViewBuilderTest, TestThrobber) {
  const char kJson[] = R"({
    "type": "BoxLayoutView",
    "properties": {
      "Orientation": "kHorizontal"
    },
    "children": [
      {
        "type": "Throbber",
        "properties": {
          "Checked": true,
          "running": true
        }
      },
      {
        "type": "SmoothedThrobber",
        "properties": {
          "StartDelayMs": 100,
          "StopDelayMs": 200,
          "running": true
        }
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  ASSERT_EQ(view->children().size(), 2u);
  views::Throbber* throbber1 =
      views::AsViewClass<views::Throbber>(view->children()[0]);
  ASSERT_NE(throbber1, nullptr);
  EXPECT_TRUE(throbber1->GetChecked());

  views::SmoothedThrobber* throbber2 =
      views::AsViewClass<views::SmoothedThrobber>(view->children()[1]);
  ASSERT_NE(throbber2, nullptr);
  EXPECT_EQ(throbber2->GetStartDelay(), base::Milliseconds(100));
  EXPECT_EQ(throbber2->GetStopDelay(), base::Milliseconds(200));
}

TEST_F(JsonViewBuilderTest, TestTextarea) {
  const char kJson[] = R"({
    "type": "Textarea",
    "properties": {
      "Text": "Multi-line\nText Content",
      "PlaceholderText": "Enter notes here..."
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::Textarea* textarea = views::AsViewClass<views::Textarea>(view.get());
  ASSERT_NE(textarea, nullptr);
  EXPECT_EQ(textarea->GetText(), u"Multi-line\nText Content");
  EXPECT_EQ(textarea->GetPlaceholderText(), u"Enter notes here...");
}

TEST_F(JsonViewBuilderTest, TestTabbedPane) {
  const char kJson[] = R"({
    "type": "TabbedPane",
    "properties": {
      "SelectedTabIndex": 1,
      "DrawTabDivider": true
    },
    "children": [
      {
        "title": "General",
        "type": "Label",
        "properties": {
          "Text": "General Settings Content"
        }
      },
      {
        "title": "Advanced",
        "type": "Label",
        "properties": {
          "Text": "Advanced Settings Content"
        }
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::TabbedPane* tabbed_pane =
      views::AsViewClass<views::TabbedPane>(view.get());
  ASSERT_NE(tabbed_pane, nullptr);
  EXPECT_EQ(tabbed_pane->GetTabCount(), 2u);
  EXPECT_EQ(tabbed_pane->GetSelectedTabIndex(), 1u);

  const views::Label* label1 =
      views::AsViewClass<views::Label>(tabbed_pane->GetTabContents(0));
  ASSERT_NE(label1, nullptr);
  EXPECT_EQ(label1->GetText(), u"General Settings Content");

  const views::Label* label2 =
      views::AsViewClass<views::Label>(tabbed_pane->GetTabContents(1));
  ASSERT_NE(label2, nullptr);
  EXPECT_EQ(label2->GetText(), u"Advanced Settings Content");
}

TEST_F(JsonViewBuilderTest, TestTableView) {
  const char kJson[] = R"({
    "type": "TableView",
    "properties": {
      "TableType": "TEXT_ONLY",
      "SingleSelection": true,
      "columns": [
        { "id": 0, "title": "Fruit", "percent": 0.5, "sortable": true },
        { "id": 1, "title": "Color", "percent": 0.3, "alignment": "CENTER" },
        { "id": 2, "title": "Price", "percent": 0.2, "alignment": "RIGHT" }
      ],
      "rows": [
        ["Apple", "Red", "$1.20"],
        ["Banana", "Yellow", "$0.50"],
        ["Kiwi", "Green", "$2.00"]
      ]
    }
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<views::View> view =
      JsonViewBuilder::BuildView(result->GetDict(), &error_msg);
  ASSERT_NE(view, nullptr) << error_msg;

  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      view.get(), result->GetDict(), &error_msg);
  EXPECT_TRUE(apply_ok) << error_msg;

  views::TableView* table_view =
      views::AsViewClass<views::TableView>(view.get());
  ASSERT_NE(table_view, nullptr);
  EXPECT_EQ(table_view->GetRowCount(), 3u);
  EXPECT_TRUE(table_view->GetSingleSelection());
  EXPECT_EQ(table_view->model()->GetText(0, 0), u"Apple");
  EXPECT_EQ(table_view->model()->GetText(0, 1), u"Red");
  EXPECT_EQ(table_view->model()->GetText(0, 2), u"$1.20");
  EXPECT_EQ(table_view->model()->GetText(1, 0), u"Banana");
  EXPECT_EQ(table_view->model()->GetText(2, 0), u"Kiwi");
}

}  // namespace views::examples
