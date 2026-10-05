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
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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

TEST_F(JsonViewBuilderTest, TestBuildDialogModel_Basic) {
  const char kJson[] = R"({
    "type": "DialogModel",
    "title": "Clear History",
    "subtitle": "Select items to delete",
    "is_alert_dialog": true,
    "close_on_deactivate": true,
    "buttons": {
      "ok": { "label": "Delete Data", "style": "kProminent" },
      "cancel": { "label": "Dismiss" }
    },
    "fields": [
      {
        "type": "paragraph",
        "header": "Warning",
        "text": "This action cannot be undone."
      },
      {
        "type": "checkbox",
        "id": 101,
        "label": "Browsing History",
        "checked": true
      },
      {
        "type": "combobox",
        "id": 102,
        "label": "Time Range",
        "options": ["Past Hour", "Past 24 Hours", "All Time"],
        "selected_index": 1
      },
      {
        "type": "textfield",
        "id": 103,
        "label": "Confirmation Phrase",
        "placeholder": "Type DELETE"
      },
      {
        "type": "separator"
      }
    ],
    "footnote": "Synced devices will also be updated."
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<ui::DialogModel> model =
      JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
  ASSERT_NE(model, nullptr) << error_msg;
}

TEST_F(JsonViewBuilderTest, TestBuildDialogModel_CustomView) {
  const char kJson[] = R"({
    "type": "DialogModel",
    "title": "Custom Embedded Dialog",
    "fields": [
      {
        "type": "custom_view",
        "id": 201,
        "field_type": "kControl",
        "view": {
          "type": "BoxLayoutView",
          "properties": {
            "Orientation": "kHorizontal"
          },
          "children": [
            {
              "type": "Label",
              "properties": {
                "ID": 999,
                "Text": "Embedded Custom Label"
              }
            }
          ]
        }
      }
    ]
  })";

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value()) << result.error().message;

  std::unique_ptr<ui::DialogModel> model =
      JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
  ASSERT_NE(model, nullptr) << error_msg;
}

TEST_F(JsonViewBuilderTest, TestBuildDialogModel_TextfieldValidation) {
  // 1. Textfield with label and accessible_name
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "title": "Textfield Dialog",
      "fields": [
        {
          "type": "textfield",
          "id": "user_input",
          "label": "Username",
          "accessible_name": "Enter your username",
          "text": "test_user"
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    ASSERT_NE(model, nullptr) << error_msg;

    ui::ElementIdentifier id = ui::ElementIdentifier::FromName("user_input");
    ASSERT_TRUE(static_cast<bool>(id));
    ui::DialogModelTextfield* tf = model->GetTextfieldByUniqueId(id);
    ASSERT_NE(tf, nullptr);
    EXPECT_EQ(tf->label(), u"Username");
    EXPECT_EQ(tf->accessible_name(), u"Enter your username");
    EXPECT_EQ(tf->text(), u"test_user");
  }

  // 2. Textfield with only accessible_name (no label) succeeds
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "textfield",
          "id": "search_box",
          "accessible_name": "Search Query"
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    ASSERT_NE(model, nullptr) << error_msg;
    ui::ElementIdentifier id = ui::ElementIdentifier::FromName("search_box");
    ui::DialogModelTextfield* tf = model->GetTextfieldByUniqueId(id);
    ASSERT_NE(tf, nullptr);
    EXPECT_TRUE(tf->label().empty());
    EXPECT_EQ(tf->accessible_name(), u"Search Query");
  }

  // 3. Textfield without label and without accessible_name returns error (no
  // CHECK crash)
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "textfield",
          "id": "invalid_tf",
          "text": "no_label_or_name"
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(error_msg.find(
                  "requires either a non-empty 'label' or 'accessible_name'"),
              std::string::npos);
  }
}

TEST_F(JsonViewBuilderTest, TestBuildDialogModel_PasswordField) {
  // 1. Password field with label, accessible_text, and incorrect_password_text
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "title": "Password Dialog",
      "fields": [
        {
          "type": "password_field",
          "id": "pwd_field",
          "label": "Main Password",
          "accessible_text": "Account Main Password",
          "incorrect_password_text": "Incorrect password. Try again."
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    ASSERT_NE(model, nullptr) << error_msg;

    ui::ElementIdentifier id = ui::ElementIdentifier::FromName("pwd_field");
    ASSERT_TRUE(static_cast<bool>(id));
    ui::DialogModelPasswordField* pf = model->GetPasswordFieldByUniqueId(id);
    ASSERT_NE(pf, nullptr);
    EXPECT_EQ(pf->label(), u"Main Password");
    EXPECT_EQ(pf->accessible_name(), u"Account Main Password");
    EXPECT_EQ(pf->incorrect_password_text(), u"Incorrect password. Try again.");
  }

  // 2. Password field without label or accessible_name fails gracefully
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "password_field",
          "id": "invalid_pwd"
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(error_msg.find(
                  "requires either a non-empty 'label' or 'accessible_name'"),
              std::string::npos);
  }
}

TEST_F(JsonViewBuilderTest, TestBuildDialogModel_ComboboxValidation) {
  // 1. Combobox with omitted selected_index / default_index defaults to 0 and
  // does not crash
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "combobox",
          "id": "combo_default",
          "label": "Language",
          "options": ["English", "Spanish", "French"]
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    ASSERT_NE(model, nullptr) << error_msg;

    ui::ElementIdentifier id = ui::ElementIdentifier::FromName("combo_default");
    ASSERT_TRUE(static_cast<bool>(id));
    ui::DialogModelCombobox* cb = model->GetComboboxByUniqueId(id);
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(cb->selected_index(), 0u);
    EXPECT_EQ(cb->label(), u"Language");
    ASSERT_NE(cb->combobox_model(), nullptr);
    EXPECT_EQ(cb->combobox_model()->GetItemCount(), 3u);
  }

  // 2. Combobox with valid explicit selected_index
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "combobox",
          "id": "combo_explicit",
          "options": ["Option A", "Option B", "Option C"],
          "selected_index": 2
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    ASSERT_NE(model, nullptr) << error_msg;

    ui::ElementIdentifier id =
        ui::ElementIdentifier::FromName("combo_explicit");
    ASSERT_TRUE(static_cast<bool>(id));
    ui::DialogModelCombobox* cb = model->GetComboboxByUniqueId(id);
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(cb->selected_index(), 2u);
  }

  // 3. Combobox with empty options returns error
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "combobox",
          "id": "empty_combo",
          "options": []
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(error_msg.find("requires a non-empty 'options' list"),
              std::string::npos);
  }

  // 4. Combobox with out-of-bounds index returns error
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "combobox",
          "options": ["Single Option"],
          "selected_index": 5
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(error_msg.find("out of bounds"), std::string::npos);
  }

  // 5. Combobox with non-integer selected_index returns error
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "combobox",
          "options": ["Option 1"],
          "selected_index": "not_an_int"
        }
      ]
    })";
    std::string error_msg;
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    auto model =
        JsonViewBuilder::BuildDialogModel(result->GetDict(), &error_msg);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(error_msg.find("Property 'selected_index' must be an integer"),
              std::string::npos);
  }
}

TEST_F(JsonViewBuilderTest, TestDialogModel_UnknownProperties) {
  const char kJson[] = R"({
    "type": "DialogModel",
    "title": "My Title",
    "subttile": "Typo subtitle"
  })";
  auto result =
      base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(result.has_value());
  std::string err;
  auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
  EXPECT_EQ(model, nullptr);
  EXPECT_NE(err.find("Unknown property in DialogModel: subttile"),
            std::string::npos);
}

TEST_F(JsonViewBuilderTest, TestDialogModel_TypeValidation) {
  // 1. Invalid title type
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "title": 123
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Property 'title' must be a string"), std::string::npos);
  }

  // 2. Invalid close_on_deactivate type
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "close_on_deactivate": "false"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Property 'close_on_deactivate' must be a boolean"),
              std::string::npos);
  }

  // 3. Invalid fields type
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": "not_a_list"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Property 'fields' must be a list"), std::string::npos);
  }

  // 4. Invalid field element type
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": ["not_a_dict"]
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Field entry in 'fields' list must be a dictionary"),
              std::string::npos);
  }

  // 5. Unknown button type in buttons dict
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "buttons": {
        "cancle": { "label": "Cancel" }
      }
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Unknown button type in buttons dict: cancle"),
              std::string::npos);
  }

  // 6. Invalid button style
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "buttons": {
        "ok": { "style": "invalid_style" }
      }
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Invalid button style: invalid_style"),
              std::string::npos);
  }
}

TEST_F(JsonViewBuilderTest, TestDialogModel_ImageParsing) {
  // 1. Valid vector icon
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "icon": "vector_icon:info,20"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    ASSERT_NE(model, nullptr) << err;
  }

  // 2. Unknown vector icon
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "icon": "vector_icon:nonexistent_icon_xyz"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Unsupported image format or unknown icon"),
              std::string::npos);
  }

  // 3. Invalid vector icon size (non-numeric)
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "icon": "vector_icon:info,abc"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Invalid vector icon size"), std::string::npos);
  }

  // 4. Invalid vector icon size (negative / zero)
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "icon": "vector_icon:info,-5"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Vector icon size must be positive"), std::string::npos);
  }

  // 5. Unknown ColorId in vector icon
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "icon": "vector_icon:info,16,ColorId:kUnknownColorIdentifier"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Unknown ColorId: kUnknownColorIdentifier"),
              std::string::npos);
  }

  // 6. Invalid solid image dimensions
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "banner": "solid,red,not_an_int,16"
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Invalid solid image width: not_an_int"),
              std::string::npos);
  }
}

TEST_F(JsonViewBuilderTest, TestDialogModel_MenuItemValidation) {
  // 1. Valid menu item with icon and id
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "menu_item",
          "id": "menu_action_item",
          "label": "Open File",
          "icon": "vector_icon:info,16",
          "is_enabled": true
        }
      ]
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    ASSERT_NE(model, nullptr) << err;
    ui::ElementIdentifier id =
        ui::ElementIdentifier::FromName("menu_action_item");
    ASSERT_TRUE(static_cast<bool>(id));
    ui::DialogModelField* field = model->GetFieldByUniqueId(id);
    ASSERT_NE(field, nullptr);
    ui::DialogModelMenuItem* item = field->AsMenuItem();
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->label(), u"Open File");
    EXPECT_FALSE(item->icon().IsEmpty());
    EXPECT_TRUE(item->is_enabled());
  }

  // 2. Menu item with invalid icon fails
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "menu_item",
          "label": "Bad Item",
          "icon": "vector_icon:invalid_icon_name"
        }
      ]
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Unsupported image format or unknown icon"),
              std::string::npos);
  }

  // 3. Menu item with non-bool is_enabled fails
  {
    const char kJson[] = R"({
      "type": "DialogModel",
      "fields": [
        {
          "type": "menu_item",
          "label": "Bad Enabled",
          "is_enabled": "true"
        }
      ]
    })";
    auto result =
        base::JSONReader::Read(kJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(result.has_value());
    std::string err;
    auto model = JsonViewBuilder::BuildDialogModel(result->GetDict(), &err);
    EXPECT_EQ(model, nullptr);
    EXPECT_NE(err.find("Property 'is_enabled' must be a boolean"),
              std::string::npos);
  }
}

}  // namespace views::examples
