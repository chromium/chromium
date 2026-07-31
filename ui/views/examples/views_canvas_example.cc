// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/views_canvas_example.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/json_view_builder.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views::examples {

namespace {

const char kBootstrapJsonScript[] = R"({
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

const char kDefaultJsonScript[] =
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
        "BetweenChildSpacing":)"
    R"( "DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL",
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
        "BetweenChildSpacing":)"
    R"( "DistanceMetric:DISTANCE_RELATED_BUTTON_HORIZONTAL",
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

class CanvasRootView : public BoxLayoutView {
 public:
  METADATA_HEADER(CanvasRootView, BoxLayoutView)

 public:
  explicit CanvasRootView(base::OnceClosure on_added_to_widget)
      : on_added_to_widget_(std::move(on_added_to_widget)) {}

  CanvasRootView(const CanvasRootView&) = delete;
  CanvasRootView& operator=(const CanvasRootView&) = delete;

  ~CanvasRootView() override = default;

  void AddedToWidget() override {
    BoxLayoutView::AddedToWidget();
    if (on_added_to_widget_) {
      std::move(on_added_to_widget_).Run();
    }
  }

 private:
  base::OnceClosure on_added_to_widget_;
};

BEGIN_METADATA(CanvasRootView)
END_METADATA

}  // namespace

ViewsCanvasExample::ViewsCanvasExample() : ExampleBase("Views Canvas") {}

ViewsCanvasExample::~ViewsCanvasExample() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void ViewsCanvasExample::CreateExampleView(View* container) {
  // Use FillLayout on the parent container.
  container->SetUseDefaultFillLayout(true);

  // 1. Parse the Bootstrap JSON script to build the page itself.
  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      kBootstrapJsonScript, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  CHECK(result.has_value())
      << "Bootstrap JSON Error: " << result.error().message;
  CHECK(result->is_dict()) << "Bootstrap JSON Error: Root must be an object";

  const base::DictValue& bootstrap_dict = result->GetDict();

  // Pass 1: Build the view hierarchy.
  // The root view is special-cased as a CanvasRootView to detect when it is
  // added to the Widget.
  auto built_root = std::make_unique<CanvasRootView>(base::BindOnce(
      &ViewsCanvasExample::OnAddedToWidget, base::Unretained(this)));

  const base::ListValue* children = bootstrap_dict.FindList("children");
  if (children) {
    for (const auto& child_val : *children) {
      CHECK(child_val.is_dict())
          << "Bootstrap JSON Error: Child must be an object";
      auto child_view =
          JsonViewBuilder::BuildView(child_val.GetDict(), &error_msg);
      CHECK(child_view) << "Bootstrap Build Error: " << error_msg;
      built_root->AddChildView(std::move(child_view));
    }
  }

  // Add the root to the parent container.
  views::View* root_view = container->AddChildView(std::move(built_root));

  // Pass 2: Apply properties.
  bool apply_ok = JsonViewBuilder::ApplyPropertiesRecursive(
      root_view, bootstrap_dict, &error_msg);
  CHECK(apply_ok) << "Bootstrap Property Error: " << error_msg;

  // 2. Hook up members using IDs from the bootstrap JSON.
  json_editor_ = views::AsViewClass<Textarea>(root_view->GetViewByID(1));
  CHECK(json_editor_);

  status_label_ = views::AsViewClass<Label>(root_view->GetViewByID(2));
  CHECK(status_label_);

  preview_container_ = root_view->GetViewByID(3);
  CHECK(preview_container_);

  views::Button* render_button =
      views::AsViewClass<views::Button>(root_view->GetViewByID(4));

  views::Button* open_button =
      views::AsViewClass<views::Button>(root_view->GetViewByID(5));

  // Set the required accessible name on the editor manually (as it's not a
  // metadata property).
  json_editor_->GetViewAccessibility().SetName(u"JSON Script Editor");

  // 3. Wire up callbacks.
  if (render_button) {
    render_button->SetCallback(base::BindRepeating(
        &ViewsCanvasExample::OnRenderButtonPressed, base::Unretained(this)));
  }
  if (open_button) {
    open_button->SetCallback(base::BindRepeating(
        &ViewsCanvasExample::OnOpenButtonPressed, base::Unretained(this)));
  }

  // 4. Load the default script.
  json_editor_->SetText(
      base::UTF8ToUTF16(std::string_view(kDefaultJsonScript)));
}

void ViewsCanvasExample::OnAddedToWidget() {
  // 5. Check command line arguments for JSON script override.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string json_text = std::string(kDefaultJsonScript);

  if (command_line->HasSwitch("views-canvas-json")) {
    json_text = command_line->GetSwitchValueASCII("views-canvas-json");
  } else if (command_line->HasSwitch("views-canvas-json-file")) {
    base::FilePath path =
        command_line->GetSwitchValuePath("views-canvas-json-file");
    std::string file_content;
    if (base::ReadFileToString(path, &file_content)) {
      json_text = file_content;
    }
  }

  if (json_editor_) {
    json_editor_->SetText(base::UTF8ToUTF16(json_text));
  }
  RebuildPreview(json_text);
}

void ViewsCanvasExample::OnRenderButtonPressed() {
  if (json_editor_) {
    std::string json_text = base::UTF16ToUTF8(json_editor_->GetText());
    RebuildPreview(json_text);
  }
}

void ViewsCanvasExample::OnOpenButtonPressed() {
  if (!select_file_dialog_) {
    select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
  }
  gfx::NativeWindow owning_window =
      preview_container_->GetWidget()->GetNativeWindow();
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  std::u16string(), base::FilePath(), nullptr,
                                  0, base::FilePath::StringType(),
                                  owning_window, nullptr);
}

void ViewsCanvasExample::FileSelected(const ui::SelectedFileInfo& file,
                                      int index) {
  std::string file_content;
  if (base::ReadFileToString(file.path(), &file_content) && json_editor_) {
    json_editor_->SetText(base::UTF8ToUTF16(file_content));
    RebuildPreview(file_content);
  }
}

void ViewsCanvasExample::FileSelectionCanceled() {}

void ViewsCanvasExample::RebuildPreview(const std::string& json_text) {
  if (!preview_container_ || !status_label_) {
    return;
  }

  // Clear existing preview.
  preview_container_->RemoveAllChildViews();

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      json_text, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  if (!result.has_value()) {
    status_label_->SetText(
        base::UTF8ToUTF16("JSON Error: " + result.error().message));
    status_label_->SetEnabledColor(SK_ColorRED);
    preview_container_->InvalidateLayout();
    return;
  }

  if (!result->is_dict()) {
    status_label_->SetText(u"JSON Error: Root must be an object");
    status_label_->SetEnabledColor(SK_ColorRED);
    preview_container_->InvalidateLayout();
    return;
  }

  const base::DictValue& root_dict = result->GetDict();

  // Pass 1: Build View Tree
  std::unique_ptr<views::View> built_root =
      JsonViewBuilder::BuildView(root_dict, &error_msg);
  if (!built_root) {
    status_label_->SetText(base::UTF8ToUTF16("Build Error: " + error_msg));
    status_label_->SetEnabledColor(SK_ColorRED);
    preview_container_->InvalidateLayout();
    return;
  }

  // Pass 2: Apply Properties
  // Add to container first so parent relationships are established for layout
  // properties.
  views::View* root_view =
      preview_container_->AddChildView(std::move(built_root));
  if (!JsonViewBuilder::ApplyPropertiesRecursive(root_view, root_dict,
                                                 &error_msg)) {
    preview_container_->RemoveAllChildViews();
    status_label_->SetText(base::UTF8ToUTF16("Property Error: " + error_msg));
    status_label_->SetEnabledColor(SK_ColorRED);
    preview_container_->InvalidateLayout();
    return;
  }

  // Success
  status_label_->SetText(u"Rendered successfully!");
  status_label_->SetEnabledColor(gfx::kGoogleGreen800);
  preview_container_->InvalidateLayout();
}

}  // namespace views::examples
