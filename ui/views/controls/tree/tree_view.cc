// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view.h"

#include <algorithm>
#include <utility>

#include "base/containers/adapters.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

using ui::TreeModel;
using ui::TreeModelNode;

namespace views {

// Insets around the view.
static constexpr int kHorizontalInset = 2;
// Padding before/after the image.
static constexpr int kImagePadding = 4;
// Size of the arrow region.
static constexpr int kArrowRegionSize = 12;
// Padding around the text (on each side).
static constexpr int kTextVerticalPadding = 3;
static constexpr int kTextHorizontalPadding = 2;
// Padding between the auxiliary text and the end of the line, handles RTL.
static constexpr int kAuxiliaryTextLineEndPadding = 5;
// How much children are indented from their parent.
static constexpr int kIndent = 20;

namespace {

void PaintRowIcon(gfx::Canvas* canvas,
                  const gfx::ImageSkia& icon,
                  int x,
                  const gfx::Rect& rect) {
  canvas->DrawImageInt(icon, rect.x() + x,
                       rect.y() + (rect.height() - icon.height()) / 2);
}

bool EventIsDoubleTapOrClick(const ui::LocatedEvent& event) {
  if (event.type() == ui::EventType::kGestureTap) {
    return event.AsGestureEvent()->details().tap_count() == 2;
  }
  return !!(event.flags() & ui::EF_IS_DOUBLE_CLICK);
}

int GetSpaceThicknessForFocusRing() {
  static const int kSpaceThicknessForFocusRing =
      FocusRing::kDefaultHaloThickness - FocusRing::kDefaultHaloInset;
  return kSpaceThicknessForFocusRing;
}

}  // namespace

TreeView::TreeView()
    : row_height_(font_list_.GetHeight() + kTextVerticalPadding * 2),
      drawing_provider_(std::make_unique<TreeViewDrawingProvider>()) {
  // Always focusable, even on Mac (consistent with NSOutlineView).
  SetFocusBehavior(FocusBehavior::ALWAYS);

  folder_icon_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kFolderChromeRefreshIcon, ui::kColorIcon);

  text_offset_ = folder_icon_.Size().width() + kImagePadding + kImagePadding +
                 kArrowRegionSize;

  SetInitialAccessibilityAttributes();
}

TreeView::~TreeView() {
  if (model_)
    model_->RemoveObserver(this);

  if (GetInputMethod() && selector_.get()) {
    // TreeView should have been blurred before destroy.
    DCHECK(selector_.get() != GetInputMethod()->GetTextInputClient());
  }

  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

// static
std::unique_ptr<ScrollView> TreeView::CreateScrollViewWithTree(
    std::unique_ptr<TreeView> tree) {
  auto scroll_view = ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(std::move(tree));
  return scroll_view;
}

void TreeView::SetModel(TreeModel* model) {
  if (model == model_)
    return;
  if (model_)
    model_->RemoveObserver(this);

  CancelEdit();

  model_ = model;
  selected_node_ = nullptr;
  active_node_ = nullptr;
  icons_.clear();
  root_.Reset(nullptr);
  root_.DeleteAll();
  GetViewAccessibility().RemoveAllVirtualChildViews();

  if (model_) {
    model_->AddObserver(this);
    model_->GetIcons(&icons_);

    ConfigureInternalNode(model_->GetRoot(), &root_);
    std::unique_ptr<AXVirtualView> ax_root_view =
        CreateAndSetAccessibilityView(&root_);
    GetViewAccessibility().AddVirtualChildView(std::move(ax_root_view));
    LoadChildren(&root_);
    root_.set_is_expanded(true);
    UpdateAccessiblePositionalPropertiesForNodeAndChildren(&root_);

    if (root_shown_)
      SetSelectedNode(root_.model_node());
    else if (!root_.children().empty())
      SetSelectedNode(root_.children().front().get()->model_node());
  }

  DrawnNodesChanged();
}

void TreeView::SetEditable(bool editable) {
  if (editable == editable_)
    return;
  editable_ = editable;
  CancelEdit();
}

void TreeView::StartEditing(TreeModelNode* node) {
  DCHECK(node);
  // Cancel the current edit.
  CancelEdit();
  // Make sure all ancestors are expanded.
  if (model_->GetParent(node))
    Expand(model_->GetParent(node));
  // Select the node, else if the user commits the edit the selection reverts.
  SetSelectedNode(node);
  if (GetSelectedNode() != node)
    return;  // Selection failed for some reason, don't start editing.
  DCHECK(!editing_);
  editing_ = true;
  if (!editor_) {
    editor_ = new Textfield;
    // Add the editor immediately as GetPreferredSize returns the wrong thing if
    // not parented.
    AddChildView(editor_.get());
    editor_->SetFontList(font_list_);
    empty_editor_size_ = editor_->GetPreferredSize({});
    editor_->set_controller(this);
  }
  editor_->SetText(selected_node_->model_node()->GetTitle());
  // TODO(crbug.com/40853810): Investigate whether accessible name should stay
  // in sync during editing.
  editor_->GetViewAccessibility().SetName(
      selected_node_->model_node()->GetAccessibleTitle());
  LayoutEditor();
  editor_->SetVisible(true);
  SchedulePaintForNode(selected_node_);
  editor_->RequestFocus();
  editor_->SelectAll(false);

  // Listen for focus changes so that we can cancel editing.
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);

  // Accelerators to commit/cancel edit.
  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

void TreeView::CancelEdit() {
  if (!editing_)
    return;

  // WARNING: don't touch |selected_node_|, it may be bogus.

  editing_ = false;
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
  editor_->SetVisible(false);
  SchedulePaint();

  RemoveAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  RemoveAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

void TreeView::CommitEdit() {
  if (!editing_)
    return;

  DCHECK(selected_node_);
  const bool editor_has_focus = editor_->HasFocus();
  model_->SetTitle(GetSelectedNode(), editor_->GetText());
  editor_->GetViewAccessibility().SetName(
      GetSelectedNode()->GetAccessibleTitle());
  selected_node_->UpdateAccessibleName();
  CancelEdit();
  if (editor_has_focus)
    RequestFocus();
}

TreeModelNode* TreeView::GetEditingNode() {
  return editing_ ? selected_node_->model_node() : nullptr;
}

void TreeView::SetSelectedNode(TreeModelNode* model_node) {
  UpdateSelection(model_node, SelectionType::kActiveAndSelected);
}

const TreeModelNode* TreeView::GetSelectedNode() const {
  return selected_node_ ? selected_node_->model_node() : nullptr;
}

void TreeView::SetActiveNode(TreeModelNode* model_node) {
  UpdateSelection(model_node, SelectionType::kActive);
}

const TreeModelNode* TreeView::GetActiveNode() const {
  return active_node_ ? active_node_->model_node() : nullptr;
}

void TreeView::Collapse(ui::TreeModelNode* model_node) {
  // Don't collapse the root if the root isn't shown, otherwise nothing is
  // displayed.
  if (model_node == root_.model_node() && !root_shown_)
    return;
  InternalNode* node = GetInternalNodeForModelNode(
      model_node, CreateType::kDontCreateIfNotLoaded);
  if (!node)
    return;
  bool was_expanded = IsExpanded(model_node);
  if (node->is_expanded()) {
    if (selected_node_ && selected_node_->HasAncestor(node))
      UpdateSelection(model_node, SelectionType::kActiveAndSelected);
    else if (active_node_ && active_node_->HasAncestor(node))
      UpdateSelection(model_node, SelectionType::kActive);
    node->set_is_expanded(false);
    UpdateAccessiblePositionalPropertiesForNodeAndChildren(node);
  }
  if (was_expanded) {
    DrawnNodesChanged();
    AXVirtualView* ax_view = node->accessibility_view();
    if (ax_view) {
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged);
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kRowCollapsed);
    }
    NotifyAccessibilityEvent(ax::mojom::Event::kRowCountChanged, true);
  }
}

void TreeView::Expand(TreeModelNode* node) {
  if (ExpandImpl(node)) {
    DrawnNodesChanged();
    InternalNode* internal_node =
        GetInternalNodeForModelNode(node, CreateType::kDontCreateIfNotLoaded);
    AXVirtualView* ax_view =
        internal_node ? internal_node->accessibility_view() : nullptr;
    if (ax_view) {
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged);
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kRowExpanded);
    }
    NotifyAccessibilityEvent(ax::mojom::Event::kRowCountChanged, true);
  }
  // TODO(sky): need to support auto_expand_children_.
}

void TreeView::ExpandAll(TreeModelNode* node) {
  DCHECK(node);
  // Expand the node.
  bool expanded_at_least_one = ExpandImpl(node);
  // And recursively expand all the children.
  const auto& children = model_->GetChildren(node);
  for (TreeModelNode* child : base::Reversed(children)) {
    if (ExpandImpl(child))
      expanded_at_least_one = true;
  }
  if (expanded_at_least_one) {
    DrawnNodesChanged();
    InternalNode* internal_node =
        GetInternalNodeForModelNode(node, CreateType::kDontCreateIfNotLoaded);
    AXVirtualView* ax_view =
        internal_node ? internal_node->accessibility_view() : nullptr;
    if (ax_view) {
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged);
      ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kRowExpanded);
    }
    NotifyAccessibilityEvent(ax::mojom::Event::kRowCountChanged, true);
  }
}

bool TreeView::IsExpanded(TreeModelNode* model_node) {
  if (!model_node) {
    // NULL check primarily for convenience for uses in this class so don't have
    // to add NULL checks every where we look up the parent.
    return true;
  }
  InternalNode* node = GetInternalNodeForModelNode(
      model_node, CreateType::kDontCreateIfNotLoaded);
  if (!node)
    return false;

  while (node) {
    if (!node->is_expanded())
      return false;
    node = node->parent();
  }
  return true;
}

void TreeView::SetRootShown(bool root_shown) {
  if (root_shown_ == root_shown)
    return;
  root_shown_ = root_shown;
  if (!root_shown_ && (selected_node_ == &root_ || active_node_ == &root_)) {
    const auto& children = model_->GetChildren(root_.model_node());
    TreeModelNode* first_child = children.empty() ? nullptr : children.front();
    if (selected_node_ == &root_)
      UpdateSelection(first_child, SelectionType::kActiveAndSelected);
    else if (active_node_ == &root_)
      UpdateSelection(first_child, SelectionType::kActive);
  }

  AXVirtualView* ax_view = root_.accessibility_view();
  // There should always be a virtual accessibility view for the root, unless
  // someone calls this method before setting a model.
  if (ax_view) {
    ax_view->NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged);
  }
  DrawnNodesChanged();
}

ui::TreeModelNode* TreeView::GetNodeForRow(int row) {
  int depth = 0;
  InternalNode* node = GetNodeByRow(row, &depth);
  return node ? node->model_node() : nullptr;
}

int TreeView::GetRowForNode(ui::TreeModelNode* node) {
  InternalNode* internal_node =
      GetInternalNodeForModelNode(node, CreateType::kDontCreateIfNotLoaded);
  if (!internal_node)
    return -1;
  int depth = 0;
  return GetRowForInternalNode(internal_node, &depth);
}

void TreeView::SetDrawingProvider(
    std::unique_ptr<TreeViewDrawingProvider> provider) {
  drawing_provider_ = std::move(provider);
}

void TreeView::SetInitialAccessibilityAttributes() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kTree);
  GetViewAccessibility().SetIsVertical(true);
  GetViewAccessibility().SetReadOnly(true);
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kActivate);
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

void TreeView::Layout(PassKey) {
  int width = preferred_size_.width();
  int height = preferred_size_.height();
  if (parent()) {
    width = std::max(parent()->width(), width);
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);
  LayoutEditor();
}

gfx::Size TreeView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return preferred_size_;
}

bool TreeView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN) {
    CommitEdit();
  } else {
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    CancelEdit();
    RequestFocus();
  }
  return true;
}

bool TreeView::OnMousePressed(const ui::MouseEvent& event) {
  return OnClickOrTap(event);
}

void TreeView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    if (OnClickOrTap(*event))
      event->SetHandled();
  }
}

void TreeView::ShowContextMenu(const gfx::Point& p,
                               ui::MenuSourceType source_type) {
  if (!model_)
    return;
  if (source_type == ui::MENU_SOURCE_MOUSE) {
    // Only invoke View's implementation (which notifies the
    // ContextMenuController) if over a node.
    gfx::Point local_point(p);
    ConvertPointFromScreen(this, &local_point);
    if (!GetNodeAtPoint(local_point))
      return;
  }
  View::ShowContextMenu(p, source_type);
}

bool TreeView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (!model_)
    return false;

  AXVirtualView* ax_view = AXVirtualView::GetFromId(action_data.target_node_id);
  InternalNode* node =
      ax_view ? GetInternalNodeForVirtualView(ax_view) : nullptr;
  if (!node) {
    switch (action_data.action) {
      case ax::mojom::Action::kFocus:
        if (active_node_)
          return false;
        if (!HasFocus())
          RequestFocus();
        return true;
      case ax::mojom::Action::kBlur:
      case ax::mojom::Action::kScrollToMakeVisible:
        return View::HandleAccessibleAction(action_data);
      default:
        return false;
    }
  }

  switch (action_data.action) {
    case ax::mojom::Action::kDoDefault:
      SetSelectedNode(node->model_node());
      if (!HasFocus())
        RequestFocus();
      if (IsExpanded(node->model_node()))
        Collapse(node->model_node());
      else
        Expand(node->model_node());
      break;

    case ax::mojom::Action::kFocus:
      SetSelectedNode(node->model_node());
      if (!HasFocus())
        RequestFocus();
      break;

    case ax::mojom::Action::kScrollToMakeVisible:
      // GetForegroundBoundsForNode() returns RTL-flipped coordinates for paint.
      // Un-flip before passing to ScrollRectToVisible(), which uses layout
      // coordinates.
      ScrollRectToVisible(GetMirroredRect(GetForegroundBoundsForNode(node)));
      break;

    case ax::mojom::Action::kShowContextMenu:
      SetSelectedNode(node->model_node());
      if (!HasFocus())
        RequestFocus();
      ShowContextMenu(GetBoundsInScreen().CenterPoint(),
                      ui::MENU_SOURCE_KEYBOARD);
      break;

    default:
      return false;
  }

  return true;
}

void TreeView::TreeNodeAdded(TreeModel* model,
                             TreeModelNode* parent,
                             size_t index) {
  InternalNode* parent_node =
      GetInternalNodeForModelNode(parent, CreateType::kDontCreateIfNotLoaded);
  if (!parent_node || !parent_node->loaded_children())
    return;

  const auto& children = model_->GetChildren(parent);
  auto child = std::make_unique<InternalNode>();
  ConfigureInternalNode(children[index], child.get());
  std::unique_ptr<AXVirtualView> ax_view =
      CreateAndSetAccessibilityView(child.get());
  UpdateAccessiblePositionalProperties(child.get());
  parent_node->Add(std::move(child), index);
  DCHECK_LE(index, parent_node->accessibility_view()->GetChildCount());
  parent_node->accessibility_view()->AddChildViewAt(std::move(ax_view), index);

  // Adding a node may change positional properties of its existing siblings,
  // like the set size and position in set.
  for (auto& sibling : parent_node->children()) {
    UpdateAccessiblePositionalProperties(sibling.get());
  }

  if (IsExpanded(parent)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kRowCountChanged, true);
    DrawnNodesChanged();
  }
}

void TreeView::TreeNodeRemoved(TreeModel* model,
                               TreeModelNode* parent,
                               size_t index) {
  InternalNode* parent_node =
      GetInternalNodeForModelNode(parent, CreateType::kDontCreateIfNotLoaded);

  if (!parent_node || !parent_node->loaded_children())
    return;

  bool reset_selected_node = false;
  bool reset_active_node = false;
  InternalNode* child_removing = parent_node->children()[index].get();
  if (selected_node_ && selected_node_->HasAncestor(child_removing)) {
    selected_node_ = nullptr;
    reset_selected_node = true;
  }
  if (active_node_ && active_node_->HasAncestor(child_removing)) {
    active_node_ = nullptr;
    reset_active_node = true;
  }

  DCHECK(parent_node->accessibility_view()->Contains(
      child_removing->accessibility_view()));
  {
    AXVirtualView* view_to_remove = child_removing->accessibility_view();
    child_removing = nullptr;
    parent_node->Remove(index);
    parent_node->accessibility_view()->RemoveChildView(view_to_remove);
  }

  // Removing a node may change positional properties of its existing siblings,
  // like the set size and position in set.
  for (auto& sibling : parent_node->children()) {
    UpdateAccessiblePositionalProperties(sibling.get());
  }

  if (reset_selected_node || reset_active_node) {
    // Replace invalidated states with the nearest valid node.
    const auto& children = model_->GetChildren(parent);
    TreeModelNode* nearest_node = nullptr;
    if (!children.empty()) {
      nearest_node = children[std::min(index, children.size() - 1)];
    } else if (parent != root_.model_node() || root_shown_) {
      nearest_node = parent;
    }
    if (reset_selected_node)
      UpdateSelection(nearest_node, SelectionType::kActiveAndSelected);
    else if (reset_active_node)
      UpdateSelection(nearest_node, SelectionType::kActive);
  }

  if (IsExpanded(parent)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kRowCountChanged, true);
    DrawnNodesChanged();
  }
}

void TreeView::TreeNodeChanged(TreeModel* model, TreeModelNode* model_node) {
  InternalNode* node = GetInternalNodeForModelNode(
      model_node, CreateType::kDontCreateIfNotLoaded);
  if (!node)
    return;
  int old_width = node->text_width();
  UpdateNodeTextWidth(node);
  if (old_width != node->text_width() &&
      ((node == &root_ && root_shown_) ||
       (node != &root_ && IsExpanded(node->parent()->model_node())))) {
    node->accessibility_view()->NotifyAccessibilityEvent(
        ax::mojom::Event::kLocationChanged);
    DrawnNodesChanged();
  }

  node->UpdateAccessibleName();
}

void TreeView::ContentsChanged(Textfield* sender,
                               const std::u16string& new_contents) {}

bool TreeView::HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  switch (key_event.key_code()) {
    case ui::VKEY_RETURN:
      CommitEdit();
      return true;

    case ui::VKEY_ESCAPE:
      CancelEdit();
      RequestFocus();
      return true;

    default:
      return false;
  }
}

void TreeView::OnWillChangeFocus(View* focused_before, View* focused_now) {}

void TreeView::OnDidChangeFocus(View* focused_before, View* focused_now) {
  CommitEdit();
}

size_t TreeView::GetRowCount() {
  size_t row_count = root_.NumExpandedNodes();
  if (!root_shown_)
    row_count--;
  return row_count;
}

std::optional<size_t> TreeView::GetSelectedRow() {
  // Type-ahead searches should be relative to the active node, so return the
  // row of the active node for |PrefixSelector|.
  ui::TreeModelNode* model_node = GetActiveNode();
  if (!model_node)
    return std::nullopt;
  const int row = GetRowForNode(model_node);
  return (row == -1) ? std::nullopt
                     : std::make_optional(static_cast<size_t>(row));
}

void TreeView::SetSelectedRow(std::optional<size_t> row) {
  // Type-ahead manipulates selection because active node is synced to selected
  // node, so call SetSelectedNode() instead of SetActiveNode().
  // TODO(crbug.com/40691087): Decouple active node from selected node by adding
  // new keyboard affordances.
  SetSelectedNode(
      GetNodeForRow(row.has_value() ? static_cast<int>(row.value()) : -1));
}

std::u16string TreeView::GetTextForRow(size_t row) {
  return GetNodeForRow(static_cast<int>(row))->GetTitle();
}

gfx::Point TreeView::GetKeyboardContextMenuLocation() {
  gfx::Rect vis_bounds(GetVisibleBounds());
  int x = 0;
  int y = 0;
  if (active_node_) {
    gfx::Rect node_bounds(GetForegroundBoundsForNode(active_node_));
    if (node_bounds.Intersects(vis_bounds))
      node_bounds.Intersect(vis_bounds);
    gfx::Point menu_point(node_bounds.CenterPoint());
    x = std::clamp(menu_point.x(), vis_bounds.x(), vis_bounds.right());
    y = std::clamp(menu_point.y(), vis_bounds.y(), vis_bounds.bottom());
  }
  gfx::Point screen_loc(x, y);
  if (base::i18n::IsRTL())
    screen_loc.set_x(vis_bounds.width() - screen_loc.x());
  ConvertPointToScreen(this, &screen_loc);
  return screen_loc;
}

bool TreeView::OnKeyPressed(const ui::KeyEvent& event) {
  if (!HasFocus())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_F2:
      if (!editing_) {
        TreeModelNode* active_node = GetActiveNode();
        if (active_node &&
            (!controller_ || controller_->CanEdit(this, active_node))) {
          StartEditing(active_node);
        }
      }
      return true;

    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      IncrementSelection(event.key_code() == ui::VKEY_UP
                             ? IncrementType::kPrevious
                             : IncrementType::kNext);
      return true;

    case ui::VKEY_LEFT:
      if (base::i18n::IsRTL())
        ExpandOrSelectChild();
      else
        CollapseOrSelectParent();
      return true;

    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL())
        CollapseOrSelectParent();
      else
        ExpandOrSelectChild();
      return true;

    default:
      break;
  }
  return false;
}

void TreeView::OnPaint(gfx::Canvas* canvas) {
  // Don't invoke View::OnPaint so that we can render our own focus border.
  canvas->DrawColor(GetColorProvider()->GetColor(ui::kColorTreeBackground));

  int min_y, max_y;
  {
    SkRect sk_clip_rect;
    if (canvas->sk_canvas()->getLocalClipBounds(&sk_clip_rect)) {
      // Pixels partially inside the clip rect should be included.
      gfx::Rect clip_rect =
          gfx::ToEnclosingRect(gfx::SkRectToRectF(sk_clip_rect));
      min_y = clip_rect.y();
      max_y = clip_rect.bottom();
    } else {
      gfx::Rect vis_bounds = GetVisibleBounds();
      min_y = vis_bounds.y();
      max_y = vis_bounds.bottom();
    }
  }

  int min_row = std::max(0, min_y / row_height_);
  int max_row = max_y / row_height_;
  if (max_y % row_height_ != 0)
    max_row++;
  int current_row = root_row();
  PaintRows(canvas, min_row, max_row, &root_, root_depth(), &current_row);
}

void TreeView::OnFocus() {
  if (GetInputMethod())
    GetInputMethod()->SetFocusedTextInputClient(GetPrefixSelector());
  View::OnFocus();
  SchedulePaintForNode(selected_node_);

  // Notify the InputMethod so that it knows to query the TextInputClient.
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(GetPrefixSelector());

  SetHasFocusIndicator(true);
}

void TreeView::OnBlur() {
  if (GetInputMethod())
    GetInputMethod()->DetachTextInputClient(GetPrefixSelector());
  SchedulePaintForNode(selected_node_);
  if (selector_)
    selector_->OnViewBlur();
  SetHasFocusIndicator(false);
}

void TreeView::UpdateSelection(TreeModelNode* model_node,
                               SelectionType selection_type) {
  CancelEdit();
  if (model_node && model_->GetParent(model_node))
    Expand(model_->GetParent(model_node));
  if (model_node && model_node == root_.model_node() && !root_shown_)
    return;  // Ignore requests for the root when not shown.
  InternalNode* node = model_node
                           ? GetInternalNodeForModelNode(
                                 model_node, CreateType::kCreateIfNotLoaded)
                           : nullptr;

  // Force update if old value was nullptr to handle case of TreeNodesRemoved
  // explicitly resetting selected_node_ or active_node_ before invoking this.
  bool active_changed = (!active_node_ || active_node_ != node);
  bool selection_changed =
      (selection_type == SelectionType::kActiveAndSelected &&
       (!selected_node_ || selected_node_ != node));

  // Update tree view states to new values.
  if (active_changed)
    active_node_ = node;

  if (selection_changed) {
    SchedulePaintForNode(selected_node_);
    SetAccessibleSelectionForNode(selected_node_, false);
    selected_node_ = node;
    SetAccessibleSelectionForNode(selected_node_, true);
    SchedulePaintForNode(selected_node_);
  }

  if (active_changed && node) {
    // GetForegroundBoundsForNode() returns RTL-flipped coordinates for paint.
    // Un-flip before passing to ScrollRectToVisible(), which uses layout
    // coordinates.
    // TODO(crbug.com/40204541): We should not be doing synchronous layout here
    // but instead we should call into this asynchronously after the Views
    // tree has processed a layout pass which happens asynchronously.
    if (auto* widget = GetWidget())
      widget->LayoutRootViewIfNecessary();
    ScrollRectToVisible(GetMirroredRect(GetForegroundBoundsForNode(node)));
  }

  // Notify assistive technologies of state changes.
  if (active_changed) {
    // Update |ViewAccessibility| so that focus lands directly on this node when
    // |FocusManager| gives focus to the tree view. This update also fires an
    // accessible focus event.
    GetViewAccessibility().OverrideFocus(node ? node->accessibility_view()
                                              : nullptr);
  }

  if (selection_changed) {
    AXVirtualView* ax_selected_view =
        node ? node->accessibility_view() : nullptr;
    if (ax_selected_view)
      ax_selected_view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection);
    else
      NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  }

  // Notify controller of state changes.
  if (selection_changed && controller_)
    controller_->OnTreeViewSelectionChanged(this);
}

bool TreeView::OnClickOrTap(const ui::LocatedEvent& event) {
  CommitEdit();

  InternalNode* node = GetNodeAtPoint(event.location());
  if (node) {
    bool hits_arrow = IsPointInExpandControl(node, event.location());
    if (!hits_arrow)
      SetSelectedNode(node->model_node());

    if (hits_arrow || EventIsDoubleTapOrClick(event)) {
      if (node->is_expanded())
        Collapse(node->model_node());
      else
        Expand(node->model_node());
    }
  }

  if (!HasFocus())
    RequestFocus();
  return true;
}

void TreeView::LoadChildren(InternalNode* node) {
  DCHECK(node->children().empty());
  DCHECK(!node->loaded_children());
  node->set_loaded_children(true);
  for (auto* model_child : model_->GetChildren(node->model_node())) {
    std::unique_ptr<InternalNode> child = std::make_unique<InternalNode>();
    ConfigureInternalNode(model_child, child.get());
    std::unique_ptr<AXVirtualView> ax_view =
        CreateAndSetAccessibilityView(child.get());
    auto* added_node = node->Add(std::move(child));
    node->accessibility_view()->AddChildView(std::move(ax_view));
    UpdateAccessiblePositionalProperties(added_node);
  }
}

void TreeView::UpdateAccessiblePositionalProperties(InternalNode* node) {
  if (!node || !node->accessibility_view()) {
    return;
  }

  ui::AXNodeData& node_data = node->accessibility_view()->GetCustomData();

  int row = -1;

  if (IsRoot(node)) {
    const int depth = root_depth();
    if (depth >= 0) {
      row = 1;
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                int32_t{depth + 1});
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, 1);
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, 1);
    }
  } else {
    if (!node->parent()) {
      return;
    }

    if (IsExpanded(node->parent()->model_node())) {
      int depth = 0;
      row = GetRowForInternalNode(node, &depth);
      if (depth >= 0) {
        node_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                  int32_t{depth + 1});
      }
    }

    // Per the ARIA Spec, aria-posinset and aria-setsize are 1-based
    // not 0-based.
    size_t pos_in_parent = node->parent()->GetIndexOf(node).value() + 1;
    size_t sibling_size =
        model_->GetChildren(node->parent()->model_node()).size();
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                              static_cast<int32_t>(pos_in_parent));
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                              static_cast<int32_t>(sibling_size));
  }

  int ignored_depth;
  const bool is_visible_or_offscreen =
      row >= 0 && GetNodeByRow(row, &ignored_depth) == node;
  if (is_visible_or_offscreen) {
    node_data.AddState(ax::mojom::State::kFocusable);
    node_data.AddAction(ax::mojom::Action::kFocus);
    node_data.AddAction(ax::mojom::Action::kScrollToMakeVisible);
    gfx::Rect node_bounds = GetBackgroundBoundsForNode(node);
    node_data.relative_bounds.bounds = gfx::RectF(node_bounds);
  } else {
    node_data.AddState(node != &root_ || root_shown_
                           ? ax::mojom::State::kInvisible
                           : ax::mojom::State::kIgnored);
  }
}

void TreeView::UpdateAccessiblePositionalPropertiesForNodeAndChildren(
    InternalNode* node) {
  UpdateAccessiblePositionalProperties(node);
  for (auto& child : node->children()) {
    UpdateAccessiblePositionalProperties(child.get());
  }
}

void TreeView::ConfigureInternalNode(TreeModelNode* model_node,
                                     InternalNode* node) {
  node->Reset(model_node);
  UpdateNodeTextWidth(node);
}

bool TreeView::IsRoot(const InternalNode* node) const {
  return node == &root_;
}

void TreeView::UpdateNodeTextWidth(InternalNode* node) {
  int width = 0, height = 0;
  gfx::Canvas::SizeStringInt(node->model_node()->GetTitle(), font_list_, &width,
                             &height, 0, gfx::Canvas::NO_ELLIPSIS);
  node->set_text_width(width);
}

std::unique_ptr<AXVirtualView> TreeView::CreateAndSetAccessibilityView(
    InternalNode* node) {
  DCHECK(node);
  auto ax_view = std::make_unique<AXVirtualView>();
  ui::AXNodeData& node_data = ax_view->GetCustomData();
  node_data.role = ax::mojom::Role::kTreeItem;
  if (base::i18n::IsRTL()) {
    node_data.SetTextDirection(ax::mojom::WritingDirection::kRtl);
  }

  node->set_accessibility_view(ax_view.get());
  node->UpdateAccessibleName();
  return ax_view;
}

void TreeView::SetAccessibleSelectionForNode(InternalNode* node,
                                             bool selected) {
  if (!node) {
    return;
  }
  AXVirtualView* ax_view = node->accessibility_view();
  DCHECK(ax_view);

  ui::AXNodeData& node_data = ax_view->GetCustomData();
  node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected);
  node_data.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kSelect);
}

void TreeView::DrawnNodesChanged() {
  UpdatePreferredSize();
  PreferredSizeChanged();
  SchedulePaint();
}

void TreeView::UpdatePreferredSize() {
  preferred_size_ = gfx::Size();
  if (!model_)
    return;

  preferred_size_.SetSize(
      root_.GetMaxWidth(this, text_offset_, root_shown_ ? 1 : 0) +
          kTextHorizontalPadding * 2,
      row_height_ * base::checked_cast<int>(GetRowCount()));

  // When the editor is visible, more space is needed beyond the regular row,
  // such as for drawing the focus ring.
  // If this tree view is scrolled through layers, there is contension for
  // updating layer bounds and scroll within the same layout call. So an
  // extra row's height is added as the buffer space.
  int horizontal_space = GetSpaceThicknessForFocusRing();
  int vertical_space =
      std::max(0, (empty_editor_size_.height() - font_list_.GetHeight()) / 2 -
                      kTextVerticalPadding) +
      GetSpaceThicknessForFocusRing() + row_height_;
  preferred_size_.Enlarge(horizontal_space, vertical_space);
}

void TreeView::LayoutEditor() {
  if (!editing_)
    return;

  DCHECK(selected_node_);
  // Position the editor so that its text aligns with the text we drew.
  gfx::Rect row_bounds = GetForegroundBoundsForNode(selected_node_);

  // GetForegroundBoundsForNode() returns a "flipped" x for painting. First, un-
  // flip it for the following calculations and ScrollRectToVisible().
  row_bounds.set_x(
      GetMirroredXWithWidthInView(row_bounds.x(), row_bounds.width()));
  row_bounds.set_x(row_bounds.x() + text_offset_);
  row_bounds.set_width(row_bounds.width() - text_offset_);
  row_bounds.Inset(
      gfx::Insets::VH(kTextVerticalPadding, kTextHorizontalPadding));
  row_bounds.Inset(gfx::Insets::VH(
      -(empty_editor_size_.height() - font_list_.GetHeight()) / 2,
      -empty_editor_size_.width() / 2));
  // Give a little extra space for editing.
  row_bounds.set_width(row_bounds.width() + 50);
  // If contained within a ScrollView, make sure the editor doesn't extend past
  // the viewport bounds.
  ScrollView* scroll_view = ScrollView::GetScrollViewForContents(this);
  if (scroll_view) {
    gfx::Rect content_bounds = scroll_view->GetContentsBounds();
    row_bounds.set_size(
        gfx::Size(std::min(row_bounds.width(), content_bounds.width()),
                  std::min(row_bounds.height(), content_bounds.height())));
  }
  // The visible bounds should include the focus ring which is outside the
  // |row_bounds|.
  gfx::Rect outter_bounds = row_bounds;
  outter_bounds.Inset(-GetSpaceThicknessForFocusRing());
  // Scroll as necessary to ensure that the editor is visible.
  ScrollRectToVisible(outter_bounds);
  editor_->SetBoundsRect(row_bounds);
  editor_->DeprecatedLayoutImmediately();
}

void TreeView::SchedulePaintForNode(InternalNode* node) {
  if (!node)
    return;  // Explicitly allow NULL to be passed in.
  SchedulePaintInRect(GetBoundsForNode(node));
}

void TreeView::PaintRows(gfx::Canvas* canvas,
                         int min_row,
                         int max_row,
                         InternalNode* node,
                         int depth,
                         int* row) {
  if (*row >= max_row)
    return;

  if (*row >= min_row && *row < max_row)
    PaintRow(canvas, node, *row, depth);
  (*row)++;
  if (!node->is_expanded())
    return;
  depth++;
  for (size_t i = 0; i < node->children().size() && *row < max_row; ++i)
    PaintRows(canvas, min_row, max_row, node->children()[i].get(), depth, row);
}

void TreeView::PaintRow(gfx::Canvas* canvas,
                        InternalNode* node,
                        int row,
                        int depth) {
  gfx::Rect bounds(GetForegroundBoundsForNodeImpl(node, row, depth));
  const SkColor selected_row_bg_color =
      drawing_provider()->GetBackgroundColorForNode(this, node->model_node());

  // Paint the row background.
  if (PlatformStyle::kTreeViewSelectionPaintsEntireRow &&
      selected_node_ == node) {
    canvas->FillRect(GetBackgroundBoundsForNode(node), selected_row_bg_color);
  }

  if (!model_->GetChildren(node->model_node()).empty())
    PaintExpandControl(canvas, bounds, node->is_expanded());

  if (drawing_provider()->ShouldDrawIconForNode(this, node->model_node()))
    PaintNodeIcon(canvas, node, bounds);

  // Paint the text background and text. In edit mode, the selected node is a
  // separate editing control, so it does not need to be painted here.
  if (editing_ && selected_node_ == node)
    return;

  gfx::Rect text_bounds(GetTextBoundsForNode(node));
  if (base::i18n::IsRTL())
    text_bounds.set_x(bounds.x());

  // Paint the background on the selected row.
  if (!PlatformStyle::kTreeViewSelectionPaintsEntireRow &&
      node == selected_node_) {
    canvas->FillRect(text_bounds, selected_row_bg_color);
  }

  // Paint the auxiliary text.
  std::u16string aux_text =
      drawing_provider()->GetAuxiliaryTextForNode(this, node->model_node());
  if (!aux_text.empty()) {
    gfx::Rect aux_text_bounds = GetAuxiliaryTextBoundsForNode(node);
    // Only draw if there's actually some space left for the auxiliary text.
    if (!aux_text_bounds.IsEmpty()) {
      int align = base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_LEFT
                                      : gfx::Canvas::TEXT_ALIGN_RIGHT;
      canvas->DrawStringRectWithFlags(
          aux_text, font_list_,
          drawing_provider()->GetAuxiliaryTextColorForNode(this,
                                                           node->model_node()),
          aux_text_bounds, align);
    }
  }

  // Paint the text.
  const gfx::Rect internal_bounds(
      text_bounds.x() + kTextHorizontalPadding,
      text_bounds.y() + kTextVerticalPadding,
      text_bounds.width() - kTextHorizontalPadding * 2,
      text_bounds.height() - kTextVerticalPadding * 2);
  canvas->DrawStringRect(
      node->model_node()->GetTitle(), font_list_,
      drawing_provider()->GetTextColorForNode(this, node->model_node()),
      internal_bounds);
}

void TreeView::PaintExpandControl(gfx::Canvas* canvas,
                                  const gfx::Rect& node_bounds,
                                  bool expanded) {
  gfx::ImageSkia arrow = gfx::CreateVectorIcon(
      vector_icons::kSubmenuArrowIcon,
      color_utils::DeriveDefaultIconColor(
          drawing_provider()->GetTextColorForNode(this, nullptr)));
  if (expanded) {
    arrow = gfx::ImageSkiaOperations::CreateRotatedImage(
        arrow, base::i18n::IsRTL() ? SkBitmapOperations::ROTATION_270_CW
                                   : SkBitmapOperations::ROTATION_90_CW);
  }
  gfx::Rect arrow_bounds = node_bounds;
  arrow_bounds.Inset(
      gfx::Insets::VH((node_bounds.height() - arrow.height()) / 2,
                      (kArrowRegionSize - arrow.width()) / 2));
  canvas->DrawImageInt(arrow,
                       base::i18n::IsRTL()
                           ? arrow_bounds.right() - arrow.width()
                           : arrow_bounds.x(),
                       arrow_bounds.y());
}

void TreeView::PaintNodeIcon(gfx::Canvas* canvas,
                             InternalNode* node,
                             const gfx::Rect& bounds) {
  std::optional<size_t> icon_index = model_->GetIconIndex(node->model_node());
  int icon_x = kArrowRegionSize + kImagePadding;
  if (!icon_index.has_value()) {
    // Flip just the |bounds| region of |canvas|.
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->Translate(gfx::Vector2d(bounds.x(), 0));
    scoped_canvas.FlipIfRTL(bounds.width());
    // Now paint the icon local to that flipped region.
    PaintRowIcon(canvas, folder_icon_.Rasterize(GetColorProvider()), icon_x,
                 gfx::Rect(0, bounds.y(), bounds.width(), bounds.height()));
  } else {
    const gfx::ImageSkia& icon =
        icons_[icon_index.value()].Rasterize(GetColorProvider());
    icon_x += (folder_icon_.Size().width() - icon.width()) / 2;
    if (base::i18n::IsRTL())
      icon_x = bounds.width() - icon_x - icon.width();
    PaintRowIcon(canvas, icon, icon_x, bounds);
  }
}

TreeView::InternalNode* TreeView::GetInternalNodeForModelNode(
    ui::TreeModelNode* model_node,
    CreateType create_type) {
  if (model_node == root_.model_node())
    return &root_;
  InternalNode* parent_internal_node =
      GetInternalNodeForModelNode(model_->GetParent(model_node), create_type);
  if (!parent_internal_node)
    return nullptr;
  if (!parent_internal_node->loaded_children()) {
    if (create_type == CreateType::kDontCreateIfNotLoaded) {
      return nullptr;
    }
    LoadChildren(parent_internal_node);
  }
  size_t index =
      model_->GetIndexOf(parent_internal_node->model_node(), model_node)
          .value();
  return parent_internal_node->children()[index].get();
}

TreeView::InternalNode* TreeView::GetInternalNodeForVirtualView(
    AXVirtualView* ax_view) {
  if (ax_view == root_.accessibility_view())
    return &root_;
  DCHECK(ax_view);
  InternalNode* parent_internal_node =
      GetInternalNodeForVirtualView(ax_view->virtual_parent_view());
  if (!parent_internal_node)
    return nullptr;
  DCHECK(parent_internal_node->loaded_children());
  AXVirtualView* parent_ax_view = parent_internal_node->accessibility_view();
  DCHECK(parent_ax_view);
  auto index = parent_ax_view->GetIndexOf(ax_view);
  return parent_internal_node->children()[index.value()].get();
}

gfx::Rect TreeView::GetBoundsForNode(InternalNode* node) {
  int row, ignored_depth;
  row = GetRowForInternalNode(node, &ignored_depth);
  return gfx::Rect(0, row * row_height_, width(), row_height_);
}

gfx::Rect TreeView::GetBackgroundBoundsForNode(InternalNode* node) {
  return PlatformStyle::kTreeViewSelectionPaintsEntireRow
             ? GetBoundsForNode(node)
             : GetForegroundBoundsForNode(node);
}

gfx::Rect TreeView::GetForegroundBoundsForNode(InternalNode* node) {
  int row, depth;
  row = GetRowForInternalNode(node, &depth);
  return GetForegroundBoundsForNodeImpl(node, row, depth);
}

gfx::Rect TreeView::GetTextBoundsForNode(InternalNode* node) {
  gfx::Rect bounds(GetForegroundBoundsForNode(node));
  if (drawing_provider()->ShouldDrawIconForNode(this, node->model_node()))
    bounds.Inset(gfx::Insets::TLBR(0, text_offset_, 0, 0));
  else
    bounds.Inset(gfx::Insets::TLBR(0, kArrowRegionSize, 0, 0));
  return bounds;
}

// The auxiliary text for a node can use all the parts of the row's bounds that
// are logical-after the row's text, and is aligned opposite to the row's text -
// that is, in LTR locales it is trailing aligned, and in RTL locales it is
// leading aligned.
gfx::Rect TreeView::GetAuxiliaryTextBoundsForNode(InternalNode* node) {
  gfx::Rect text_bounds = GetTextBoundsForNode(node);
  int width = base::i18n::IsRTL()
                  ? text_bounds.x() - kTextHorizontalPadding -
                        kAuxiliaryTextLineEndPadding
                  : bounds().width() - text_bounds.right() -
                        kTextHorizontalPadding - kAuxiliaryTextLineEndPadding;
  if (width < 0)
    return gfx::Rect();
  int x = base::i18n::IsRTL()
              ? kAuxiliaryTextLineEndPadding
              : bounds().right() - width - kAuxiliaryTextLineEndPadding;
  return gfx::Rect(x, text_bounds.y(), width, text_bounds.height());
}

gfx::Rect TreeView::GetForegroundBoundsForNodeImpl(InternalNode* node,
                                                   int row,
                                                   int depth) {
  int width =
      drawing_provider()->ShouldDrawIconForNode(this, node->model_node())
          ? text_offset_ + node->text_width() + kTextHorizontalPadding * 2
          : kArrowRegionSize + node->text_width() + kTextHorizontalPadding * 2;

  gfx::Rect rect(depth * kIndent + kHorizontalInset, row * row_height_, width,
                 row_height_);
  rect.set_x(GetMirroredXWithWidthInView(rect.x(), rect.width()));
  return rect;
}

int TreeView::GetRowForInternalNode(InternalNode* node, int* depth) {
  DCHECK(!node->parent() || IsExpanded(node->parent()->model_node()));
  *depth = -1;
  int row = -1;
  const InternalNode* tmp_node = node;
  while (tmp_node->parent()) {
    size_t index_in_parent = tmp_node->parent()->GetIndexOf(tmp_node).value();
    (*depth)++;
    row++;  // For node.
    for (size_t i = 0; i < index_in_parent; ++i) {
      row += static_cast<int>(
          tmp_node->parent()->children()[i]->NumExpandedNodes());
    }
    tmp_node = tmp_node->parent();
  }
  if (root_shown_) {
    (*depth)++;
    row++;
  }
  return row;
}

TreeView::InternalNode* TreeView::GetNodeAtPoint(const gfx::Point& point) {
  int row = point.y() / row_height_;
  int depth = -1;
  InternalNode* node = GetNodeByRow(row, &depth);
  if (!node)
    return nullptr;

  // If the entire row gets a selected background, clicking anywhere in the row
  // serves to hit this node.
  if (PlatformStyle::kTreeViewSelectionPaintsEntireRow)
    return node;
  gfx::Rect bounds(GetForegroundBoundsForNodeImpl(node, row, depth));
  return bounds.Contains(point) ? node : nullptr;
}

TreeView::InternalNode* TreeView::GetNodeByRow(int row, int* depth) {
  int current_row = root_row();
  *depth = 0;
  return GetNodeByRowImpl(&root_, row, root_depth(), &current_row, depth);
}

TreeView::InternalNode* TreeView::GetNodeByRowImpl(InternalNode* node,
                                                   int target_row,
                                                   int current_depth,
                                                   int* current_row,
                                                   int* node_depth) {
  if (*current_row == target_row) {
    *node_depth = current_depth;
    return node;
  }
  (*current_row)++;
  if (node->is_expanded()) {
    current_depth++;
    for (const auto& child : node->children()) {
      InternalNode* result = GetNodeByRowImpl(
          child.get(), target_row, current_depth, current_row, node_depth);
      if (result)
        return result;
    }
  }
  return nullptr;
}

void TreeView::IncrementSelection(IncrementType type) {
  if (!model_)
    return;

  if (!active_node_) {
    // If nothing is selected select the first or last node.
    if (root_.children().empty())
      return;
    if (type == IncrementType::kPrevious) {
      size_t row_count = GetRowCount();
      int depth = 0;
      DCHECK(row_count);
      InternalNode* node =
          GetNodeByRow(static_cast<int>(row_count - 1), &depth);
      SetSelectedNode(node->model_node());
    } else if (root_shown_) {
      SetSelectedNode(root_.model_node());
    } else {
      SetSelectedNode(root_.children().front()->model_node());
    }
    return;
  }

  int depth = 0;
  int delta = type == IncrementType::kPrevious ? -1 : 1;
  int row = GetRowForInternalNode(active_node_, &depth);
  int new_row =
      std::clamp(row + delta, 0, base::checked_cast<int>(GetRowCount()) - 1);
  if (new_row == row)
    return;  // At the end/beginning.
  SetSelectedNode(GetNodeByRow(new_row, &depth)->model_node());
}

void TreeView::CollapseOrSelectParent() {
  if (active_node_) {
    if (active_node_->is_expanded())
      Collapse(active_node_->model_node());
    else if (active_node_->parent())
      SetSelectedNode(active_node_->parent()->model_node());
  }
}

void TreeView::ExpandOrSelectChild() {
  if (active_node_) {
    if (!active_node_->is_expanded())
      Expand(active_node_->model_node());
    else if (!active_node_->children().empty())
      SetSelectedNode(active_node_->children().front()->model_node());
  }
}

bool TreeView::ExpandImpl(TreeModelNode* model_node) {
  TreeModelNode* parent = model_->GetParent(model_node);
  if (!parent) {
    // Node should be the root.
    DCHECK_EQ(root_.model_node(), model_node);
    bool was_expanded = root_.is_expanded();
    root_.set_is_expanded(true);
    UpdateAccessiblePositionalPropertiesForNodeAndChildren(&root_);
    return !was_expanded;
  }

  // Expand all the parents.
  bool return_value = ExpandImpl(parent);
  InternalNode* internal_node =
      GetInternalNodeForModelNode(model_node, CreateType::kCreateIfNotLoaded);
  DCHECK(internal_node);
  if (!internal_node->is_expanded()) {
    if (!internal_node->loaded_children())
      LoadChildren(internal_node);
    internal_node->set_is_expanded(true);
    UpdateAccessiblePositionalPropertiesForNodeAndChildren(internal_node);
    return_value = true;
  }
  return return_value;
}

PrefixSelector* TreeView::GetPrefixSelector() {
  if (!selector_)
    selector_ = std::make_unique<PrefixSelector>(this, this);
  return selector_.get();
}

bool TreeView::IsPointInExpandControl(InternalNode* node,
                                      const gfx::Point& point) {
  if (model_->GetChildren(node->model_node()).empty())
    return false;

  int depth = -1;
  int row = GetRowForInternalNode(node, &depth);

  int arrow_dx = depth * kIndent + kHorizontalInset;
  gfx::Rect arrow_bounds(arrow_dx, row * row_height_, kArrowRegionSize,
                         row_height_);
  if (base::i18n::IsRTL())
    arrow_bounds.set_x(width() - arrow_dx - kArrowRegionSize);
  return arrow_bounds.Contains(point);
}

void TreeView::SetHasFocusIndicator(bool shows) {
  // If this View is the grandchild of a ScrollView, use the grandparent
  // ScrollView for the focus ring instead of this View so that the focus ring
  // won't be scrolled.
  ScrollView* scroll_view = ScrollView::GetScrollViewForContents(this);
  if (scroll_view)
    scroll_view->SetHasFocusIndicator(shows);
}

// InternalNode ----------------------------------------------------------------

TreeView::InternalNode::InternalNode() {
  SetAccessibleIsExpanded(is_expanded_);
}

TreeView::InternalNode::~InternalNode() = default;

void TreeView::InternalNode::Reset(ui::TreeModelNode* node) {
  model_node_ = node;
  loaded_children_ = false;
  is_expanded_ = false;
  text_width_ = 0;
  accessibility_view_ = nullptr;
}

void TreeView::InternalNode::set_is_expanded(bool expanded) {
  is_expanded_ = expanded;
  SetAccessibleIsExpanded(is_expanded_);
}

void TreeView::InternalNode::SetAccessibleIsExpanded(bool expanded) {
  if (!accessibility_view_) {
    return;
  }

  ui::AXNodeData& node_data = accessibility_view_->GetCustomData();

  if (expanded) {
    node_data.RemoveState(ax::mojom::State::kCollapsed);
    node_data.AddState(ax::mojom::State::kExpanded);
  } else {
    node_data.RemoveState(ax::mojom::State::kExpanded);
    node_data.AddState(ax::mojom::State::kCollapsed);
  }
}

size_t TreeView::InternalNode::NumExpandedNodes() const {
  size_t result = 1;  // For this.
  if (!is_expanded_)
    return result;
  for (const auto& child : children())
    result += child->NumExpandedNodes();
  return result;
}

void TreeView::InternalNode::UpdateAccessibleName() {
  if (!accessibility_view_) {
    return;
  }

  std::u16string name = model_node()->GetTitle();
  ui::AXNodeData& node_data = accessibility_view_->GetCustomData();
  if (name.empty()) {
    node_data.SetNameExplicitlyEmpty();
  } else {
    node_data.SetNameChecked(name);
  }
}

int TreeView::InternalNode::GetMaxWidth(TreeView* tree, int indent, int depth) {
  bool has_icon =
      tree->drawing_provider()->ShouldDrawIconForNode(tree, model_node());
  int max_width = (has_icon ? text_width_ : kArrowRegionSize) + indent * depth;
  if (!is_expanded_)
    return max_width;
  for (const auto& child : children()) {
    max_width =
        std::max(max_width, child->GetMaxWidth(tree, indent, depth + 1));
  }
  return max_width;
}

BEGIN_METADATA(TreeView)
END_METADATA

}  // namespace views
