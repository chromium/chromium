// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_assistant_structure.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

// TODO(muyuanli): share with BrowserAccessibility.
bool IsTextField(const AXNode* node, uint32_t state) {
  return node->data().IsTextField();
}

bool IsRichTextEditable(const AXNode* node) {
  const AXNode* parent = node->GetUnignoredParent();
  return node->data().HasState(ax::mojom::State::kRichlyEditable) &&
         (!parent ||
          !parent->data().HasState(ax::mojom::State::kRichlyEditable));
}

bool IsAtomicTextField(const AXNode* node) {
  const std::string& html_tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  if (html_tag == "input") {
    std::string input_type;
    if (!node->GetHtmlAttribute("type", &input_type))
      return true;
    return input_type.empty() || input_type == "email" ||
           input_type == "password" || input_type == "search" ||
           input_type == "tel" || input_type == "text" || input_type == "url" ||
           input_type == "number";
  }
  return html_tag == "textarea";
}

bool IsLeaf(const AXNode* node) {
  if (node->children().empty())
    return true;

  if (IsAtomicTextField(node) || node->IsText()) {
    return true;
  }

  switch (node->data().role) {
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kInputTime:
      return true;
    default:
      return false;
  }
}

std::u16string GetInnerText(const AXNode* node) {
  if (node->IsText()) {
    return node->GetString16Attribute(ax::mojom::StringAttribute::kName);
  }
  std::u16string text;
  for (size_t i = 0; i < node->GetUnignoredChildCount(); ++i) {
    AXNode* child = node->GetUnignoredChildAtIndex(i);
    text += GetInnerText(child);
  }
  return text;
}

std::u16string GetValue(const AXNode* node) {
  std::u16string value =
      node->GetString16Attribute(ax::mojom::StringAttribute::kValue);

  if (value.empty() &&
      (IsTextField(node, node->data().state) || IsRichTextEditable(node)) &&
      !IsAtomicTextField(node)) {
    value = GetInnerText(node);
  }

  // Always obscure passwords.
  if (node->data().HasState(ax::mojom::State::kProtected))
    value = std::u16string(value.size(), kSecurePasswordBullet);

  return value;
}

std::u16string GetText(const AXNode* node) {
  if (node->data().role == ax::mojom::Role::kPdfRoot ||
      node->data().role == ax::mojom::Role::kIframe ||
      node->data().role == ax::mojom::Role::kIframePresentational) {
    return std::u16string();
  }

  ax::mojom::NameFrom name_from = node->data().GetNameFrom();

  if (!ui::IsLeaf(node) && name_from == ax::mojom::NameFrom::kContents) {
    return std::u16string();
  }

  std::u16string value = GetValue(node);

  if (!value.empty()) {
    if (node->data().HasState(ax::mojom::State::kEditable))
      return value;

    switch (node->data().role) {
      case ax::mojom::Role::kComboBoxMenuButton:
      case ax::mojom::Role::kTextFieldWithComboBox:
      case ax::mojom::Role::kPopUpButton:
      case ax::mojom::Role::kTextField:
        return value;
      default:
        break;
    }
  }

  if (node->data().role == ax::mojom::Role::kColorWell) {
    unsigned int color = static_cast<unsigned int>(
        node->GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    unsigned int red = color >> 16 & 0xFF;
    unsigned int green = color >> 8 & 0xFF;
    unsigned int blue = color >> 0 & 0xFF;
    return base::UTF8ToUTF16(
        base::StringPrintf("#%02X%02X%02X", red, green, blue));
  }

  std::u16string text =
      node->GetString16Attribute(ax::mojom::StringAttribute::kName);
  std::u16string description =
      node->GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  if (!description.empty()) {
    if (!text.empty())
      text += u" ";
    text += description;
  }

  if (text.empty())
    text = value;

  if (node->data().role == ax::mojom::Role::kRootWebArea ||
      node->data().role == ax::mojom::Role::kPdfRoot) {
    return text;
  }

  if (text.empty() && IsLeaf(node)) {
    for (size_t i = 0; i < node->GetUnignoredChildCount(); ++i) {
      AXNode* child = node->GetUnignoredChildAtIndex(i);
      text += GetText(child);
    }
  }

  if (text.empty() && (ui::IsLink(node->data().role) ||
                       node->data().role == ax::mojom::Role::kImage)) {
    std::u16string url =
        node->GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    text = AXUrlBaseText(url);
  }

  return text;
}

// Get string representation of ax::mojom::Role. We are not using ToString() in
// ax_enums.h since the names are subject to change in the future and
// we are only interested in a subset of the roles.
absl::optional<std::string> AXRoleToString(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kArticle:
      return absl::optional<std::string>("article");
    case ax::mojom::Role::kBanner:
      return absl::optional<std::string>("banner");
    case ax::mojom::Role::kCaption:
      return absl::optional<std::string>("caption");
    case ax::mojom::Role::kComplementary:
      return absl::optional<std::string>("complementary");
    case ax::mojom::Role::kDate:
      return absl::optional<std::string>("date");
    case ax::mojom::Role::kDateTime:
      return absl::optional<std::string>("date_time");
    case ax::mojom::Role::kDefinition:
      return absl::optional<std::string>("definition");
    case ax::mojom::Role::kDetails:
      return absl::optional<std::string>("details");
    case ax::mojom::Role::kDocument:
      return absl::optional<std::string>("document");
    case ax::mojom::Role::kFeed:
      return absl::optional<std::string>("feed");
    case ax::mojom::Role::kHeading:
      return absl::optional<std::string>("heading");
    case ax::mojom::Role::kIframe:
      return absl::optional<std::string>("iframe");
    case ax::mojom::Role::kIframePresentational:
      return absl::optional<std::string>("iframe_presentational");
    case ax::mojom::Role::kList:
      return absl::optional<std::string>("list");
    case ax::mojom::Role::kListItem:
      return absl::optional<std::string>("list_item");
    case ax::mojom::Role::kMain:
      return absl::optional<std::string>("main");
    case ax::mojom::Role::kParagraph:
      return absl::optional<std::string>("paragraph");
    default:
      return absl::optional<std::string>();
  }
}

AssistantNode* AddChild(AssistantTree* tree) {
  auto node = std::make_unique<AssistantNode>();
  tree->nodes.push_back(std::move(node));
  return tree->nodes.back().get();
}

struct WalkAXTreeConfig {
  bool should_select_leaf;
};

void WalkAXTreeDepthFirst(const AXNode* node,
                          const gfx::Rect& rect,
                          const AXTreeUpdate& update,
                          const AXTree* tree,
                          WalkAXTreeConfig* config,
                          AssistantTree* assistant_tree,
                          AssistantNode* result) {
  result->text = GetText(node);
  result->class_name =
      AXRoleToAndroidClassName(node->data().role, node->GetUnignoredParent());
  result->role = AXRoleToString(node->data().role);

  result->text_size = -1.0;
  result->bgcolor = 0;
  result->color = 0;
  result->bold = 0;
  result->italic = 0;
  result->line_through = 0;
  result->underline = 0;

  if (node->HasFloatAttribute(ax::mojom::FloatAttribute::kFontSize)) {
    gfx::RectF text_size_rect(
        0, 0, 1, node->GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize));
    gfx::Rect scaled_text_size_rect =
        gfx::ToEnclosingRect(tree->RelativeToTreeBounds(node, text_size_rect));
    result->text_size = scaled_text_size_rect.height();

    result->color = node->GetIntAttribute(ax::mojom::IntAttribute::kColor);
    result->bgcolor =
        node->GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor);
    result->bold = node->data().HasTextStyle(ax::mojom::TextStyle::kBold);
    result->italic = node->data().HasTextStyle(ax::mojom::TextStyle::kItalic);
    result->line_through =
        node->data().HasTextStyle(ax::mojom::TextStyle::kLineThrough);
    result->underline =
        node->data().HasTextStyle(ax::mojom::TextStyle::kUnderline);
  }

  const gfx::Rect& absolute_rect =
      gfx::ToEnclosingRect(tree->GetTreeBounds(node));
  gfx::Rect parent_relative_rect = absolute_rect;
  bool is_root = !node->GetUnignoredParent();
  if (!is_root) {
    parent_relative_rect.Offset(-rect.OffsetFromOrigin());
  }
  result->rect = gfx::Rect(parent_relative_rect.x(), parent_relative_rect.y(),
                           absolute_rect.width(), absolute_rect.height());

  if (IsLeaf(node) && update.has_tree_data) {
    int start_selection = 0;
    int end_selection = 0;
    AXTree::Selection unignored_selection = tree->GetUnignoredSelection();
    if (unignored_selection.anchor_object_id == node->id()) {
      start_selection = unignored_selection.anchor_offset;
      config->should_select_leaf = true;
    }

    if (config->should_select_leaf) {
      end_selection = static_cast<int32_t>(GetText(node).length());
    }

    if (unignored_selection.focus_object_id == node->id()) {
      end_selection = unignored_selection.focus_offset;
      config->should_select_leaf = false;
    }
    if (end_selection > 0)
      result->selection =
          absl::make_optional<gfx::Range>(start_selection, end_selection);
  }

  result->html_tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  result->css_display =
      node->GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
  result->html_attributes = node->data().html_attributes;

  std::string class_name =
      node->GetStringAttribute(ax::mojom::StringAttribute::kClassName);
  if (!class_name.empty())
    result->html_attributes.push_back({"class", class_name});

  for (size_t i = 0; i < node->GetUnignoredChildCount(); ++i) {
    AXNode* child = node->GetUnignoredChildAtIndex(i);
    auto* n = AddChild(assistant_tree);
    result->children_indices.push_back(assistant_tree->nodes.size() - 1);
    WalkAXTreeDepthFirst(child, absolute_rect, update, tree, config,
                         assistant_tree, n);
  }
}

}  // namespace

AssistantNode::AssistantNode() = default;
AssistantNode::AssistantNode(const AssistantNode& other) = default;
AssistantNode::~AssistantNode() = default;

AssistantTree::AssistantTree() = default;
AssistantTree::~AssistantTree() = default;

AssistantTree::AssistantTree(const AssistantTree& other) {
  for (const auto& node : other.nodes)
    nodes.emplace_back(std::make_unique<AssistantNode>(*node));
}

std::unique_ptr<AssistantTree> CreateAssistantTree(const AXTreeUpdate& update) {
  auto tree = std::make_unique<AXSerializableTree>();
  auto assistant_tree = std::make_unique<AssistantTree>();
  auto* root = AddChild(assistant_tree.get());
  if (!tree->Unserialize(update))
    LOG(FATAL) << tree->error();
  WalkAXTreeConfig config{
      false,         // should_select_leaf
  };
  WalkAXTreeDepthFirst(tree->root(), gfx::Rect(), update, tree.get(), &config,
                       assistant_tree.get(), root);
  return assistant_tree;
}

std::u16string AXUrlBaseText(std::u16string url) {
  // Given a url like http://foo.com/bar/baz.png, just return the
  // base text, e.g., "baz".
  int trailing_slashes = 0;
  while (url.size() - trailing_slashes > 0 &&
         url[url.size() - trailing_slashes - 1] == '/') {
    trailing_slashes++;
  }
  if (trailing_slashes)
    url = url.substr(0, url.size() - trailing_slashes);
  size_t slash_index = url.rfind('/');
  if (slash_index != std::string::npos)
    url = url.substr(slash_index + 1);
  size_t dot_index = url.rfind('.');
  if (dot_index != std::string::npos)
    url = url.substr(0, dot_index);
  return url;
}

const char* AXRoleToAndroidClassName(ax::mojom::Role role, bool has_parent) {
  switch (role) {
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
      return kAXEditTextClassname;
    case ax::mojom::Role::kSlider:
      return kAXSeekBarClassname;
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kInputTime:
      return kAXSpinnerClassname;
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kPdfActionableHighlight:
      return kAXButtonClassname;
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kSwitch:
      return kAXCheckBoxClassname;
    case ax::mojom::Role::kRadioButton:
      return kAXRadioButtonClassname;
    case ax::mojom::Role::kToggleButton:
      return kAXToggleButtonClassname;
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kSvgRoot:
      return kAXImageClassname;
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kProgressIndicator:
      return kAXProgressBarClassname;
    case ax::mojom::Role::kTabList:
      return kAXTabWidgetClassname;
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTable:
      return kAXGridViewClassname;
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDirectory:
      return kAXListViewClassname;
    case ax::mojom::Role::kDialog:
      return kAXDialogClassname;
    case ax::mojom::Role::kRootWebArea:
      return has_parent ? kAXViewClassname : kAXWebViewClassname;
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
      return kAXMenuItemClassname;
    case ax::mojom::Role::kStaticText:
      return kAXTextViewClassname;
    default:
      return kAXViewClassname;
  }
}

}  // namespace ui
