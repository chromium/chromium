// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_assistant_structure.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/range/range.h"

namespace ui {

namespace {

// TODO(muyuanli): share with BrowserAccessibility.
bool IsTextField(const AXNode* node) {
  return node->data().IsTextField();
}

bool IsRichTextEditable(const AXNode* node) {
  const AXNode* parent = node->GetUnignoredParent();
  return node->HasState(ax::mojom::State::kRichlyEditable) &&
         (!parent || !parent->HasState(ax::mojom::State::kRichlyEditable));
}

bool IsAtomicTextField(const AXNode* node) {
  const std::string& html_tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  if (html_tag == "input") {
    const std::string& input_type = node->GetHtmlAttribute("type");
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

  switch (node->GetRole()) {
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
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    text += GetInnerText(iter.get());
  }
  return text;
}

std::u16string GetValue(const AXNode* node) {
  std::u16string value =
      node->GetString16Attribute(ax::mojom::StringAttribute::kValue);

  if (value.empty() && (IsTextField(node) || IsRichTextEditable(node)) &&
      !IsAtomicTextField(node)) {
    value = GetInnerText(node);
  }

  // Always obscure passwords.
  if (node->HasState(ax::mojom::State::kProtected))
    value = std::u16string(value.size(), kSecurePasswordBullet);

  return value;
}

std::u16string GetText(const AXNode* node) {
  if (node->GetRole() == ax::mojom::Role::kPdfRoot ||
      node->GetRole() == ax::mojom::Role::kIframe ||
      node->GetRole() == ax::mojom::Role::kIframePresentational) {
    return std::u16string();
  }

  ax::mojom::NameFrom name_from = node->GetNameFrom();

  if (!IsLeaf(node) && name_from == ax::mojom::NameFrom::kContents) {
    return std::u16string();
  }

  std::u16string value = GetValue(node);

  if (!value.empty()) {
    if (node->HasState(ax::mojom::State::kEditable))
      return value;

    switch (node->GetRole()) {
      case ax::mojom::Role::kComboBoxMenuButton:
      case ax::mojom::Role::kComboBoxSelect:
      case ax::mojom::Role::kPopUpButton:
      case ax::mojom::Role::kTextFieldWithComboBox:
      case ax::mojom::Role::kTextField:
        return value;
      default:
        break;
    }
  }

  if (node->GetRole() == ax::mojom::Role::kColorWell) {
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

  if (node->GetRole() == ax::mojom::Role::kRootWebArea ||
      node->GetRole() == ax::mojom::Role::kPdfRoot) {
    return text;
  }

  if (text.empty() && IsLeaf(node)) {
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      text += GetInnerText(iter.get());
    }
  }

  if (text.empty() &&
      (IsLink(node->GetRole()) || node->GetRole() == ax::mojom::Role::kImage)) {
    std::u16string url =
        node->GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    text = AXUrlBaseText(url);
  }

  return text;
}

// Get string representation of ax::mojom::Role. We are not using ToString() in
// ax_enums.h since the names are subject to change in the future and
// we are only interested in a subset of the roles.
std::optional<std::string> AXRoleToString(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kArticle:
      return std::optional<std::string>("article");
    case ax::mojom::Role::kBanner:
      return std::optional<std::string>("banner");
    case ax::mojom::Role::kCaption:
      return std::optional<std::string>("caption");
    case ax::mojom::Role::kComplementary:
      return std::optional<std::string>("complementary");
    case ax::mojom::Role::kDate:
      return std::optional<std::string>("date");
    case ax::mojom::Role::kDateTime:
      return std::optional<std::string>("date_time");
    case ax::mojom::Role::kDefinition:
      return std::optional<std::string>("definition");
    case ax::mojom::Role::kDetails:
      return std::optional<std::string>("details");
    case ax::mojom::Role::kDocument:
      return std::optional<std::string>("document");
    case ax::mojom::Role::kFeed:
      return std::optional<std::string>("feed");
    case ax::mojom::Role::kHeading:
      return std::optional<std::string>("heading");
    case ax::mojom::Role::kIframe:
      return std::optional<std::string>("iframe");
    case ax::mojom::Role::kIframePresentational:
      return std::optional<std::string>("iframe_presentational");
    case ax::mojom::Role::kList:
      return std::optional<std::string>("list");
    case ax::mojom::Role::kListItem:
      return std::optional<std::string>("list_item");
    case ax::mojom::Role::kMain:
      return std::optional<std::string>("main");
    case ax::mojom::Role::kParagraph:
      return std::optional<std::string>("paragraph");
    default:
      return std::optional<std::string>();
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

// |parent_absolute_clipped_rect| is the parent of the current subtree, and its
// coordinates are relative to the top of the page.
void WalkAXTreeDepthFirst(const AXNode* node,
                          const gfx::Rect& parent_absolute_clipped_rect,
                          const gfx::Rect& parent_absolute_unclipped_rect,
                          const int root_scroll_y,
                          const AXTreeUpdate& update,
                          const AXTree* tree,
                          WalkAXTreeConfig* config,
                          AssistantTree* assistant_tree,
                          AssistantNode* result) {
  result->text = GetText(node);
  result->class_name =
      AXRoleToAndroidClassName(node->GetRole(), node->GetUnignoredParent());
  result->role = AXRoleToString(node->GetRole());

  result->text_size = -1.0;
  result->bgcolor = 0;
  result->color = 0;
  result->bold = false;
  result->italic = false;
  result->line_through = false;
  result->underline = false;

  if (node->HasFloatAttribute(ax::mojom::FloatAttribute::kFontSize)) {
    result->text_size =
        node->GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize);
    result->color = node->GetIntAttribute(ax::mojom::IntAttribute::kColor);
    result->bgcolor =
        node->GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor);
    result->bold = node->HasTextStyle(ax::mojom::TextStyle::kBold);
    result->italic = node->HasTextStyle(ax::mojom::TextStyle::kItalic);
    result->line_through =
        node->HasTextStyle(ax::mojom::TextStyle::kLineThrough);
    result->underline = node->HasTextStyle(ax::mojom::TextStyle::kUnderline);
  }

  const gfx::Rect& absolute_clipped_rect =
      gfx::ToEnclosingRect(tree->GetTreeBounds(node));
  const gfx::Rect& absolute_unclipped_rect = gfx::ToEnclosingRect(
      tree->GetTreeBounds(node, nullptr, /* clip_bounds = */ false));

  // Calculate the parent relative bounds. For the root node, these bounds are
  // the same as the absolute bounds above.
  gfx::Rect parent_relative_clipped_rect = absolute_clipped_rect;
  gfx::Rect parent_relative_unclipped_rect = absolute_unclipped_rect;
  bool is_root = !node->GetUnignoredParent();
  if (!is_root) {
    parent_relative_clipped_rect.Offset(
        -parent_absolute_clipped_rect.OffsetFromOrigin());
    parent_relative_unclipped_rect.Offset(
        -parent_absolute_unclipped_rect.OffsetFromOrigin());
  }

  result->rect = parent_relative_clipped_rect;
  result->unclipped_rect = parent_relative_unclipped_rect;

  // Create a Rect for the absolute unclipped bounds with the scrolling of the
  // root container removed.
  gfx::Rect absolute_unclipped_rect_unscrolled = absolute_unclipped_rect;
  absolute_unclipped_rect_unscrolled.Offset(0, root_scroll_y);
  result->page_absolute_rect = absolute_unclipped_rect_unscrolled;

  // Selection state comes from the tree data rather than
  // GetUnignoredSelection() which uses AXPosition, as AXPosition requires a
  // valid and registered AXTreeID, which exists only when accessibility is
  // enabled. As an AXTreeSnapshotter does not enable accessibility, it is not
  // able to use AXPosition.
  if (IsLeaf(node) && update.has_tree_data) {
    int start_selection = 0;
    int end_selection = 0;
    if (update.tree_data.sel_anchor_object_id == node->id()) {
      start_selection = update.tree_data.sel_anchor_offset;
      config->should_select_leaf = true;
    }

    if (config->should_select_leaf) {
      end_selection = static_cast<int32_t>(GetText(node).length());
    }

    if (update.tree_data.sel_focus_object_id == node->id()) {
      end_selection = update.tree_data.sel_focus_offset;
      config->should_select_leaf = false;
    }
    if (end_selection > 0)
      result->selection =
          std::make_optional<gfx::Range>(start_selection, end_selection);
  }

  result->html_tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  result->css_display =
      node->GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
  result->html_attributes = node->GetHtmlAttributes();

  // Always add root scroll values for debugging scrolling.
  result->html_attributes.emplace_back("root_scroll_y",
                                       base::NumberToString(root_scroll_y));

  std::string class_name =
      node->GetStringAttribute(ax::mojom::StringAttribute::kClassName);
  if (!class_name.empty())
    result->html_attributes.emplace_back("class", class_name);

  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    auto* n = AddChild(assistant_tree);
    result->children_indices.push_back(assistant_tree->nodes.size() - 1);
    WalkAXTreeDepthFirst(iter.get(), absolute_clipped_rect,
                         absolute_unclipped_rect, root_scroll_y, update, tree,
                         config, assistant_tree, n);
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

  int root_scroll_y =
      tree->root()->GetIntAttribute(ax::mojom::IntAttribute::kScrollY);

  WalkAXTreeDepthFirst(tree->root(), gfx::Rect(), gfx::Rect(), root_scroll_y,
                       update, tree.get(), &config, assistant_tree.get(), root);
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
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kInputTime:
      return kAXSpinnerClassname;
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kPdfActionableHighlight:
      return kAXButtonClassname;
    case ax::mojom::Role::kCheckBox:
      return kAXCheckBoxClassname;
    case ax::mojom::Role::kRadioButton:
      return kAXRadioButtonClassname;
    case ax::mojom::Role::kRadioGroup:
      return kAXRadioGroupClassname;
    case ax::mojom::Role::kSwitch:
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
    case ax::mojom::Role::kDirectoryDeprecated:
    case ax::mojom::Role::kPreDeprecated:
      NOTREACHED();
    default:
      return kAXViewClassname;
  }
}

}  // namespace ui
