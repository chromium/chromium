// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_node_wrapper.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

// A global map from AXNodes to TestAXNodeWrappers.
std::map<AXNodeID, TestAXNodeWrapper*> g_node_id_to_wrapper_map;

// A global coordinate offset.
gfx::Vector2d g_offset;

// A global scale factor.
float g_scale_factor = 1.0;

// A global map that stores which node is focused on a determined tree.
//   - If a tree has no node being focused, there shouldn't be any entry on the
//     map associated with such tree, i.e. a pair {tree, nullptr} is invalid.
//   - For testing purposes, assume there is a single node being focused in the
//     entire tree and if such node is deleted, focus is completely lost.
std::map<AXTree*, AXNode*> g_focused_node_in_tree;

// A global indicating the last node which ShowContextMenu was called from.
AXNode* g_node_from_last_show_context_menu;

// A global indicating the last node which accessibility perform action
// default action was called from.
AXNode* g_node_from_last_default_action;

// A global indicating that AXPlatformNodeDelegate objects are web content.
bool g_is_web_content = false;

// A map of hit test results - a map from source node ID to destination node
// ID.
std::map<AXNodeID, AXNodeID> g_hit_test_result;

// A simple implementation of AXTreeObserver to catch when AXNodes are
// deleted so we can delete their wrappers.
class TestAXTreeObserver : public AXTreeObserver {
 private:
  void OnNodeDeleted(AXTree* tree, int32_t node_id) override {
    const auto iter = g_node_id_to_wrapper_map.find(node_id);
    if (iter != g_node_id_to_wrapper_map.end()) {
      TestAXNodeWrapper* wrapper = iter->second;
      delete wrapper;
      g_node_id_to_wrapper_map.erase(node_id);
    }
  }
};

TestAXTreeObserver g_ax_tree_observer;

}  // namespace

// static
TestAXNodeWrapper* TestAXNodeWrapper::GetOrCreate(AXTree* tree, AXNode* node) {
  if (!tree || !node)
    return nullptr;

  if (!tree->HasObserver(&g_ax_tree_observer))
    tree->AddObserver(&g_ax_tree_observer);
  auto iter = g_node_id_to_wrapper_map.find(node->id());
  if (iter != g_node_id_to_wrapper_map.end())
    return iter->second;
  TestAXNodeWrapper* wrapper = new TestAXNodeWrapper(tree, node);
  g_node_id_to_wrapper_map[node->id()] = wrapper;
  return wrapper;
}

// static
void TestAXNodeWrapper::SetGlobalCoordinateOffset(const gfx::Vector2d& offset) {
  g_offset = offset;
}

// static
const AXNode* TestAXNodeWrapper::GetNodeFromLastShowContextMenu() {
  return g_node_from_last_show_context_menu;
}

// static
const AXNode* TestAXNodeWrapper::GetNodeFromLastDefaultAction() {
  return g_node_from_last_default_action;
}

// static
void TestAXNodeWrapper::SetNodeFromLastDefaultAction(AXNode* node) {
  g_node_from_last_default_action = node;
}

// static
std::unique_ptr<base::AutoReset<float>> TestAXNodeWrapper::SetScaleFactor(
    float value) {
  return std::make_unique<base::AutoReset<float>>(&g_scale_factor, value);
}

// static
void TestAXNodeWrapper::SetGlobalIsWebContent(bool is_web_content) {
  g_is_web_content = is_web_content;
}

// static
void TestAXNodeWrapper::SetHitTestResult(AXNodeID src_node_id,
                                         AXNodeID dst_node_id) {
  g_hit_test_result[src_node_id] = dst_node_id;
}

// static
void TestAXNodeWrapper::ResetGlobalState() {
  g_node_id_to_wrapper_map.clear();
  g_focused_node_in_tree.clear();
  g_hit_test_result.clear();
  g_node_from_last_show_context_menu = nullptr;
  g_node_from_last_default_action = nullptr;
  g_scale_factor = 1.0;
  g_offset.set_x(0);
  g_offset.set_y(0);
}

TestAXNodeWrapper::~TestAXNodeWrapper() {
  platform_node_.ExtractAsDangling()->Destroy();
}

const AXNodeData& TestAXNodeWrapper::GetData() const {
  return node_->data();
}

const AXTreeData& TestAXNodeWrapper::GetTreeData() const {
  return tree_->data();
}

const AXSelection TestAXNodeWrapper::GetUnignoredSelection() const {
  return tree_->GetUnignoredSelection();
}

AXNodePosition::AXPositionInstance TestAXNodeWrapper::CreatePositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  if (node_->IsLeaf()) {
    return AXNodePosition::CreateTextPosition(*node_, offset, affinity);
  }
  return AXNodePosition::CreateTreePosition(*node_, offset);
}

AXNodePosition::AXPositionInstance TestAXNodeWrapper::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  return AXNodePosition::CreateTextPosition(*node_, offset, affinity);
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetNativeViewAccessible() {
  return ax_platform_node()->GetNativeViewAccessible();
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetParent() const {
  TestAXNodeWrapper* parent_wrapper =
      GetOrCreate(tree_, node_->GetUnignoredParent());
  return parent_wrapper ?
      parent_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

size_t TestAXNodeWrapper::GetChildCount() const {
  return InternalChildCount();
}

gfx::NativeViewAccessible TestAXNodeWrapper::ChildAtIndex(size_t index) const {
  TestAXNodeWrapper* child_wrapper = InternalGetChild(index);
  return child_wrapper ?
      child_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

gfx::Rect TestAXNodeWrapper::GetBoundsRect(
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreenPhysicalPixels:
      // For unit testing purposes, assume a device scale factor of 1 and fall
      // through.
    case AXCoordinateSystem::kScreenDIPs: {
      // We could optionally add clipping here if ever needed.
      gfx::RectF bounds = GetLocation();
      bounds.Offset(g_offset);

      // For test behavior only, for bounds that are offscreen we currently do
      // not apply clipping to the bounds but we still return the offscreen
      // status.
      if (offscreen_result) {
        *offscreen_result = DetermineOffscreenResult(bounds);
      }

      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect TestAXNodeWrapper::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreenPhysicalPixels:
    // For unit testing purposes, assume a device scale factor of 1 and fall
    // through.
    case AXCoordinateSystem::kScreenDIPs: {
      gfx::RectF bounds = GetLocation();
      // This implementation currently only deals with text node that has role
      // kInlineTextBox and kStaticText.
      // For test purposes, assume node with kStaticText always has a single
      // child with role kInlineTextBox.
      if (GetRole() == ax::mojom::Role::kInlineTextBox) {
        bounds = GetInlineTextRect(start_offset, end_offset);
      } else if (GetRole() == ax::mojom::Role::kStaticText &&
                 InternalChildCount() > 0) {
        TestAXNodeWrapper* child = InternalGetChild(0);
        if (child != nullptr &&
            child->GetRole() == ax::mojom::Role::kInlineTextBox) {
          bounds = child->GetInlineTextRect(start_offset, end_offset);
        }
      }

      bounds.Offset(g_offset);

      // For test behavior only, for bounds that are offscreen we currently do
      // not apply clipping to the bounds but we still return the offscreen
      // status.
      if (offscreen_result) {
        *offscreen_result = DetermineOffscreenResult(bounds);
      }

      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect TestAXNodeWrapper::GetHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreenPhysicalPixels:
    // For unit testing purposes, assume a device scale factor of 1 and fall
    // through.
    case AXCoordinateSystem::kScreenDIPs: {
      // Ignoring start, len, and clipped, as there's no clean way to map these
      // via unit tests.
      gfx::RectF bounds = GetLocation();
      bounds.Offset(g_offset);
      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

TestAXNodeWrapper* TestAXNodeWrapper::HitTestSyncInternal(int x, int y) {
  if (g_hit_test_result.find(node_->id()) != g_hit_test_result.end()) {
    int result_id = g_hit_test_result[node_->id()];
    AXNode* result_node = tree_->GetFromId(result_id);
    return GetOrCreate(tree_, result_node);
  }

  // Here we find the deepest child whose bounding box contains the given point.
  // The assumptions are that there are no overlapping bounding rects and that
  // all children have smaller bounding rects than their parents.
  if (!GetClippedScreenBoundsRect().Contains(gfx::Rect(x, y, 0, 0)))
    return nullptr;

  for (size_t i = 0; i < GetChildCount(); i++) {
    TestAXNodeWrapper* child = GetOrCreate(tree_, node_->children()[i]);
    if (!child)
      return nullptr;

    TestAXNodeWrapper* result = child->HitTestSyncInternal(x, y);
    if (result) {
      return result;
    }
  }
  return this;
}

gfx::NativeViewAccessible TestAXNodeWrapper::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  const TestAXNodeWrapper* wrapper =
      const_cast<TestAXNodeWrapper*>(this)->HitTestSyncInternal(
          screen_physical_pixel_x / g_scale_factor,
          screen_physical_pixel_y / g_scale_factor);
  return wrapper ? wrapper->ax_platform_node()->GetNativeViewAccessible()
                 : nullptr;
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetFocus() const {
  auto focused = g_focused_node_in_tree.find(tree_);
  if (focused != g_focused_node_in_tree.end() &&
      focused->second->IsDescendantOf(node_)) {
    return GetOrCreate(tree_, focused->second)
        ->ax_platform_node()
        ->GetNativeViewAccessible();
  }
  return nullptr;
}

bool TestAXNodeWrapper::IsMinimized() const {
  return minimized_;
}

bool TestAXNodeWrapper::IsWebContent() const {
  return g_is_web_content;
}

bool TestAXNodeWrapper::IsReadOnlySupported() const {
  return node_->IsReadOnlySupported();
}

bool TestAXNodeWrapper::IsReadOnlyOrDisabled() const {
  return node_->IsReadOnlyOrDisabled();
}

// Walk the AXTree and ensure that all wrappers are created
void TestAXNodeWrapper::BuildAllWrappers(AXTree* tree, AXNode* node) {
  TestAXNodeWrapper::GetOrCreate(tree, node);
  for (AXNode* child : node->children()) {
    TestAXNodeWrapper::GetOrCreate(tree, child);
    BuildAllWrappers(tree, child);
  }
}

void TestAXNodeWrapper::ResetNativeEventTarget() {
  native_event_target_ = gfx::kNullAcceleratedWidget;
}

AXPlatformNode* TestAXNodeWrapper::GetFromNodeID(int32_t id) {
  // Force creating all of the wrappers for this tree.
  BuildAllWrappers(tree_, tree_->root());

  const auto iter = g_node_id_to_wrapper_map.find(id);
  if (iter != g_node_id_to_wrapper_map.end())
    return iter->second->ax_platform_node();

  return nullptr;
}

AXPlatformNode* TestAXNodeWrapper::GetFromTreeIDAndNodeID(
    const AXTreeID& ax_tree_id,
    int32_t id) {
  // TestAXNodeWrapper only supports one accessibility tree.
  // Additional work would need to be done to support multiple trees.
  CHECK_EQ(GetTreeData().tree_id, ax_tree_id);
  return GetFromNodeID(id);
}

std::optional<size_t> TestAXNodeWrapper::GetIndexInParent() const {
  return node_ ? std::make_optional(node_->GetUnignoredIndexInParent())
               : std::nullopt;
}

void TestAXNodeWrapper::ReplaceIntAttribute(int32_t node_id,
                                            ax::mojom::IntAttribute attribute,
                                            int32_t value) {
  if (!tree_)
    return;

  AXNode* node = tree_->GetFromId(node_id);
  if (!node)
    return;

  AXNodeData new_data = node->data();
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>& attributes =
      new_data.int_attributes;

  std::erase_if(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddIntAttribute(attribute, value);
  node->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::FloatAttribute, float>>& attributes =
      new_data.float_attributes;

  std::erase_if(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddFloatAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceBoolAttribute(ax::mojom::BoolAttribute attribute,
                                             bool value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::BoolAttribute, bool>>& attributes =
      new_data.bool_attributes;

  std::erase_if(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddBoolAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::StringAttribute, std::string>>& attributes =
      new_data.string_attributes;

  std::erase_if(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddStringAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceTreeDataTextSelection(int32_t anchor_node_id,
                                                     int32_t anchor_offset,
                                                     int32_t focus_node_id,
                                                     int32_t focus_offset) {
  if (!tree_)
    return;

  AXTreeData new_tree_data = GetTreeData();
  new_tree_data.sel_anchor_object_id = anchor_node_id;
  new_tree_data.sel_anchor_offset = anchor_offset;
  new_tree_data.sel_focus_object_id = focus_node_id;
  new_tree_data.sel_focus_offset = focus_offset;

  tree_->UpdateDataForTesting(new_tree_data);
}

std::optional<int> TestAXNodeWrapper::GetTableRowCount() const {
  return node_->GetTableRowCount();
}

std::optional<int> TestAXNodeWrapper::GetTableColCount() const {
  return node_->GetTableColCount();
}

std::optional<int> TestAXNodeWrapper::GetTableAriaRowCount() const {
  return node_->GetTableAriaRowCount();
}

std::optional<int> TestAXNodeWrapper::GetTableAriaColCount() const {
  return node_->GetTableAriaColCount();
}

std::optional<int> TestAXNodeWrapper::GetTableCellCount() const {
  return node_->GetTableCellCount();
}

std::vector<AXNodeID> TestAXNodeWrapper::GetColHeaderNodeIds() const {
  return node_->GetTableColHeaderNodeIds();
}

std::vector<AXNodeID> TestAXNodeWrapper::GetColHeaderNodeIds(
    int col_index) const {
  return node_->GetTableColHeaderNodeIds(col_index);
}

std::vector<AXNodeID> TestAXNodeWrapper::GetRowHeaderNodeIds() const {
  return node_->GetTableCellRowHeaderNodeIds();
}

std::vector<AXNodeID> TestAXNodeWrapper::GetRowHeaderNodeIds(
    int row_index) const {
  return node_->GetTableRowHeaderNodeIds(row_index);
}

bool TestAXNodeWrapper::IsTableRow() const {
  return node_->IsTableRow();
}

std::optional<int> TestAXNodeWrapper::GetTableRowRowIndex() const {
  return node_->GetTableRowRowIndex();
}

bool TestAXNodeWrapper::IsTableCellOrHeader() const {
  return node_->IsTableCellOrHeader();
}

std::optional<int> TestAXNodeWrapper::GetTableCellIndex() const {
  return node_->GetTableCellIndex();
}

std::optional<int> TestAXNodeWrapper::GetTableCellColIndex() const {
  return node_->GetTableCellColIndex();
}

std::optional<int> TestAXNodeWrapper::GetTableCellRowIndex() const {
  return node_->GetTableCellRowIndex();
}

std::optional<int> TestAXNodeWrapper::GetTableCellColSpan() const {
  return node_->GetTableCellColSpan();
}

std::optional<int> TestAXNodeWrapper::GetTableCellRowSpan() const {
  return node_->GetTableCellRowSpan();
}

std::optional<int> TestAXNodeWrapper::GetTableCellAriaColIndex() const {
  return node_->GetTableCellAriaColIndex();
}

std::optional<int> TestAXNodeWrapper::GetTableCellAriaRowIndex() const {
  return node_->GetTableCellAriaRowIndex();
}

std::optional<int32_t> TestAXNodeWrapper::GetCellId(int row_index,
                                                    int col_index) const {
  AXNode* cell = node_->GetTableCellFromCoords(row_index, col_index);
  if (!cell)
    return std::nullopt;
  return cell->id();
}
std::optional<int32_t> TestAXNodeWrapper::GetCellIdAriaCoords(
    int aria_row_index,
    int aria_col_index) const {
  AXNode* cell =
      node_->GetTableCellFromAriaCoords(aria_row_index, aria_col_index);
  if (!cell) {
    return std::nullopt;
  }
  return cell->id();
}

gfx::AcceleratedWidget
TestAXNodeWrapper::GetTargetForNativeAccessibilityEvent() {
  return native_event_target_;
}

std::optional<int32_t> TestAXNodeWrapper::CellIndexToId(int cell_index) const {
  AXNode* cell = node_->GetTableCellFromIndex(cell_index);
  if (!cell)
    return std::nullopt;
  return cell->id();
}

bool TestAXNodeWrapper::IsCellOrHeaderOfAriaGrid() const {
  return node_->IsCellOrHeaderOfAriaGrid();
}

bool TestAXNodeWrapper::AccessibilityPerformAction(const AXActionData& data) {
  switch (data.action) {
    case ax::mojom::Action::kScrollToPoint:
      g_offset = gfx::Vector2d(data.target_point.x(), data.target_point.y());
      return true;
    case ax::mojom::Action::kSetScrollOffset: {
      int scroll_x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
      int scroll_x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
      int scroll_y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
      int scroll_y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
      int scroll_x =
          std::clamp(data.target_point.x(), scroll_x_min, scroll_x_max);
      int scroll_y =
          std::clamp(data.target_point.y(), scroll_y_min, scroll_y_max);

      ReplaceIntAttribute(node_->id(), ax::mojom::IntAttribute::kScrollX,
                          scroll_x);
      ReplaceIntAttribute(node_->id(), ax::mojom::IntAttribute::kScrollY,
                          scroll_y);
      return true;
    }
    case ax::mojom::Action::kScrollToMakeVisible: {
      auto offset = node_->data().relative_bounds.bounds.OffsetFromOrigin();
      g_offset = gfx::Vector2d(-offset.x(), -offset.y());
      return true;
    }

    case ax::mojom::Action::kDoDefault: {
      // If a default action such as a click is performed on an element, it
      // could result in a selected state change. In which case, the element's
      // selected state no longer comes from focus action, so we should set
      // |kSelectedFromFocus| to false.
      if (HasBoolAttribute(ax::mojom::BoolAttribute::kSelectedFromFocus))
        ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelectedFromFocus,
                             false);

      switch (GetRole()) {
        case ax::mojom::Role::kListBoxOption:
        case ax::mojom::Role::kGridCell: {
          bool current_value =
              GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
          ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                               !current_value);
          break;
        }
        case ax::mojom::Role::kRadioButton:
        case ax::mojom::Role::kMenuItemRadio: {
          if (GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue)
            ReplaceIntAttribute(
                node_->id(), ax::mojom::IntAttribute::kCheckedState,
                static_cast<int32_t>(ax::mojom::CheckedState::kFalse));
          else if (GetData().GetCheckedState() ==
                   ax::mojom::CheckedState::kFalse)
            ReplaceIntAttribute(
                node_->id(), ax::mojom::IntAttribute::kCheckedState,
                static_cast<int32_t>(ax::mojom::CheckedState::kTrue));
          break;
        }
        default:
          break;
      }
      SetNodeFromLastDefaultAction(node_);
      return true;
    }

    case ax::mojom::Action::kSetValue:
      if (GetData().IsRangeValueSupported()) {
        ReplaceFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                              std::stof(data.value));
      } else if (GetRole() == ax::mojom::Role::kTextField) {
        ReplaceStringAttribute(ax::mojom::StringAttribute::kValue, data.value);
      }
      return true;

    case ax::mojom::Action::kSetSelection: {
      ReplaceIntAttribute(data.anchor_node_id,
                          ax::mojom::IntAttribute::kTextSelStart,
                          data.anchor_offset);
      ReplaceIntAttribute(data.focus_node_id,
                          ax::mojom::IntAttribute::kTextSelEnd,
                          data.focus_offset);
      ReplaceTreeDataTextSelection(data.anchor_node_id, data.anchor_offset,
                                   data.focus_node_id, data.focus_offset);
      return true;
    }

    case ax::mojom::Action::kFocus: {
      g_focused_node_in_tree[tree_] = node_;

      // The platform has select follows focus behavior:
      // https://www.w3.org/TR/wai-aria-practices-1.1/#kbd_selection_follows_focus
      // For test purposes, we support select follows focus for all elements,
      // and not just single-selection container elements.
      if (IsSelectSupported(GetRole())) {
        ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
        ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelectedFromFocus,
                             true);
      }

      return true;
    }

    case ax::mojom::Action::kShowContextMenu:
      g_node_from_last_show_context_menu = node_;
      return true;

    default:
      return true;
  }
}

std::u16string TestAXNodeWrapper::GetLocalizedRoleDescriptionForUnlabeledImage()
    const {
  return u"Unlabeled image";
}

std::u16string TestAXNodeWrapper::GetLocalizedStringForLandmarkType() const {
  switch (GetRole()) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return u"banner";

    case ax::mojom::Role::kComplementary:
      return u"complementary";

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return u"content information";

    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSection:
      if (HasStringAttribute(ax::mojom::StringAttribute::kName))
        return u"region";
      [[fallthrough]];

    default:
      return {};
  }
}

std::u16string TestAXNodeWrapper::GetLocalizedStringForRoleDescription() const {
  switch (GetRole()) {
    case ax::mojom::Role::kArticle:
      return u"article";

    case ax::mojom::Role::kAudio:
      return u"audio";

    case ax::mojom::Role::kCode:
      return u"code";

    case ax::mojom::Role::kColorWell:
      return u"color picker";

    case ax::mojom::Role::kComment:
      return u"comment";

    case ax::mojom::Role::kContentInfo:
      return u"content information";

    case ax::mojom::Role::kDate:
      return u"date picker";

    case ax::mojom::Role::kDateTime: {
      std::string input_type;
      if (GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                             &input_type)) {
        if (input_type == "datetime-local") {
          return u"local date and time picker";
        } else if (input_type == "week") {
          return u"week picker";
        }
      }
      return {};
    }

    case ax::mojom::Role::kDefinition:
      return u"definition";

    case ax::mojom::Role::kDetails:
      return u"details";

    case ax::mojom::Role::kDocEndnote:
      return u"endnote";

    case ax::mojom::Role::kDocFootnote:
      return u"footnote";

    case ax::mojom::Role::kEmphasis:
      return u"emphasis";

    case ax::mojom::Role::kFigure:
      return u"figure";

    case ax::mojom::Role::kFooter:
      return u"footer";

    case ax::mojom::Role::kHeader:
      return u"header";

    case ax::mojom::Role::kMark:
      return u"highlight";

    case ax::mojom::Role::kMeter:
      return u"meter";

    case ax::mojom::Role::kSearchBox:
      return u"search box";

    case ax::mojom::Role::kSection: {
      if (HasStringAttribute(ax::mojom::StringAttribute::kName))
        return u"section";

      return {};
    }

    case ax::mojom::Role::kSectionFooter:
      return u"sectionfooter";

    case ax::mojom::Role::kSectionHeader:
      return u"searchheader";

    case ax::mojom::Role::kStatus:
      return u"output";

    case ax::mojom::Role::kStrong:
      return u"strong";

    case ax::mojom::Role::kTextField: {
      std::string input_type;
      if (GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                             &input_type)) {
        if (input_type == "email") {
          return u"email";
        } else if (input_type == "tel") {
          return u"telephone";
        } else if (input_type == "url") {
          return u"url";
        }
      }
      return {};
    }

    case ax::mojom::Role::kTime:
      return u"time";

    default:
      return {};
  }
}

std::u16string TestAXNodeWrapper::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      return u"To get missing image descriptions, open the context menu.";
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return u"Getting description...";
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      return u"Appears to contain adult content. No description available.";
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      return u"No description available.";
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return std::u16string();
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string TestAXNodeWrapper::GetStyleNameAttributeAsLocalizedString()
    const {
  AXNode* current_node = node_;
  while (current_node) {
    if (current_node->GetRole() == ax::mojom::Role::kMark)
      return u"mark";
    current_node = current_node->parent();
  }
  return std::u16string();
}

bool TestAXNodeWrapper::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

bool TestAXNodeWrapper::HasVisibleCaretOrSelection() const {
  return node_->HasVisibleCaretOrSelection();
}

std::vector<AXPlatformNode*>
TestAXNodeWrapper::GetSourceNodesForReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));
  return GetNodesFromRelationIdSet(
      tree_->GetReverseRelations(attr, GetData().id));
}

std::vector<AXPlatformNode*>
TestAXNodeWrapper::GetSourceNodesForReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  return GetNodesFromRelationIdSet(
      tree_->GetReverseRelations(attr, GetData().id));
}

AXPlatformNodeId TestAXNodeWrapper::GetUniqueId() const {
  return unique_id_;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      unique_id_(AXUniqueId::Create()),
      platform_node_(AXPlatformNode::Create(this)) {
#if BUILDFLAG(IS_WIN)
  native_event_target_ = gfx::kMockAcceleratedWidget;
#else
  native_event_target_ = gfx::kNullAcceleratedWidget;
#endif
}

bool TestAXNodeWrapper::IsOrderedSetItem() const {
  return node_->IsOrderedSetItem();
}

bool TestAXNodeWrapper::IsOrderedSet() const {
  return node_->IsOrderedSet();
}

std::optional<int> TestAXNodeWrapper::GetPosInSet() const {
  return node_->GetPosInSet();
}

std::optional<int> TestAXNodeWrapper::GetSetSize() const {
  return node_->GetSetSize();
}

SkColor TestAXNodeWrapper::GetColor() const {
  return node_->ComputeColor();
}

SkColor TestAXNodeWrapper::GetBackgroundColor() const {
  return node_->ComputeBackgroundColor();
}

gfx::RectF TestAXNodeWrapper::GetLocation() const {
  return GetData().relative_bounds.bounds;
}

size_t TestAXNodeWrapper::InternalChildCount() const {
  return node_->GetUnignoredChildCount();
}

TestAXNodeWrapper* TestAXNodeWrapper::InternalGetChild(size_t index) const {
  CHECK_LT(index, InternalChildCount());
  return GetOrCreate(tree_, node_->GetUnignoredChildAtIndex(index));
}

const std::vector<gfx::NativeViewAccessible>
TestAXNodeWrapper::GetUIADirectChildrenInRange(AXPlatformNodeDelegate* start,
                                               AXPlatformNodeDelegate* end) {
  return {};
}

// static
// Needs to stay in sync with AXPlatformNodeWin::ShouldHideChildrenForUIA.
bool TestAXNodeWrapper::ShouldHideChildrenForUIA(const AXNode* node) {
  if (!node)
    return false;

  auto role = node->GetRole();

  if (HasPresentationalChildren(role)) {
    return true;
  }

  switch (role) {
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kTextField:
      return true;
    default:
      return false;
  }
}

gfx::RectF TestAXNodeWrapper::GetInlineTextRect(const int start_offset,
                                                const int end_offset) const {
  DCHECK(start_offset >= 0 && end_offset >= 0 && start_offset <= end_offset);
  const std::vector<int32_t>& character_offsets =
      GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  gfx::RectF location = GetLocation();
  gfx::RectF bounds;

  switch (static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    // Currently only kNone and kLtr are supported text direction.
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr: {
      int start_pixel_offset =
          start_offset > 0 ? character_offsets[start_offset - 1] : location.x();
      int end_pixel_offset =
          end_offset > 0 ? character_offsets[end_offset - 1] : location.x();
      bounds =
          gfx::RectF(start_pixel_offset, location.y(),
                     end_pixel_offset - start_pixel_offset, location.height());
      break;
    }
    default:
      NOTIMPLEMENTED();
  }
  return bounds;
}

bool TestAXNodeWrapper::Intersects(gfx::RectF rect1, gfx::RectF rect2) const {
  // The logic below is based on gfx::RectF::Intersects.
  // gfx::RectF::Intersects returns false if either of the two rects is empty.
  // This function is used in tests to determine offscreen status. We want to
  // include empty rect in our logic since the bounding box of a degenerate text
  // range is initially empty (width=0), and we do not want to mark it as
  // offscreen.
  return rect1.x() < rect2.right() && rect1.right() > rect2.x() &&
         rect1.y() < rect2.bottom() && rect1.bottom() > rect2.y();
}

AXOffscreenResult TestAXNodeWrapper::DetermineOffscreenResult(
    gfx::RectF bounds) const {
  if (!tree_ || !tree_->root())
    return AXOffscreenResult::kOnscreen;

  const AXNodeData& root_web_area_node_data = tree_->root()->data();
  gfx::RectF root_web_area_bounds =
      root_web_area_node_data.relative_bounds.bounds;

  // For testing, we only look at the current node's bound relative to the root
  // web area bounds to determine offscreen status. We currently do not look at
  // the bounds of the immediate parent of the node for determining offscreen
  // status.
  // We only determine offscreen result if the root web area bounds is actually
  // set in the test, and we mark a node as offscreen only when |bounds| is
  // completely outside of |root_web_area_bounds|. We default the offscreen
  // result of every other situation to AXOffscreenResult::kOnscreen.
  if (!root_web_area_bounds.IsEmpty() &&
      !Intersects(root_web_area_bounds, bounds)) {
    return AXOffscreenResult::kOffscreen;
  }

  return AXOffscreenResult::kOnscreen;
}

}  // namespace ui
