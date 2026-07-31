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
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
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

}  // namespace views::examples
