// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_H_
#define UI_ACCESSIBILITY_AX_NODE_H_

#include <stdint.h>

#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/stack.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_hypertext.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_text_attributes.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

class AXComputedNodeData;
class AXSelection;
class AXTableInfo;
class AXTreeManager;

struct AXLanguageInfo;
class AXTree;

// This class is used to represent a node in an accessibility tree (`AXTree`).
class AX_EXPORT AXNode final {
 public:
  // Replacement character used to represent an embedded (or, additionally for
  // text navigation, an empty) object. Part of the Unicode Standard.
  //
  // On some platforms, most objects are represented in the text of their
  // parents with a special "embedded object character" and not with their
  // actual text contents. Also on the same platforms, if a node has only
  // ignored descendants, i.e., it appears to be empty to assistive software, we
  // need to treat it as a character and a word boundary.
  static constexpr char kEmbeddedObjectCharacterUTF8[] = "\xEF\xBF\xBC";
  static constexpr char16_t kEmbeddedObjectCharacterUTF16[] = u"\xFFFC";
  // We compute the embedded characters' length instead of manually typing it in
  // order to avoid the variable pairs getting out of sync in a future update.
  static constexpr int kEmbeddedObjectCharacterLengthUTF8 =
      std::char_traits<char>::length(kEmbeddedObjectCharacterUTF8);
  static constexpr int kEmbeddedObjectCharacterLengthUTF16 =
      std::char_traits<char16_t>::length(kEmbeddedObjectCharacterUTF16);

  // Default values must be consistent with AXNodeData.
  static constexpr bool kDefaultBoolValue = AXNodeData::kDefaultBoolValue;
  static constexpr int kDefaultIntValue = AXNodeData::kDefaultIntValue;
  static constexpr float kDefaultFloatValue = AXNodeData::kDefaultFloatValue;

  template <typename NodeType,
            NodeType* (NodeType::*NextSibling)() const,
            NodeType* (NodeType::*PreviousSibling)() const,
            NodeType* (NodeType::*FirstChild)() const,
            NodeType* (NodeType::*LastChild)() const>
  class ChildIteratorBase {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = int;
    using value_type = NodeType;
    using pointer = NodeType*;
    using reference = NodeType&;

    ChildIteratorBase(const NodeType* parent, NodeType* child);
    ChildIteratorBase(const ChildIteratorBase& it);
    ~ChildIteratorBase() = default;
    bool operator==(const ChildIteratorBase& rhs) const;
    bool operator!=(const ChildIteratorBase& rhs) const;
    ChildIteratorBase& operator++();
    ChildIteratorBase& operator--();
    NodeType* get() const;
    NodeType& operator*() const;
    NodeType* operator->() const;

   protected:
    raw_ptr<const NodeType> parent_;
    raw_ptr<NodeType, DanglingUntriaged> child_;
  };

  // The constructor requires a parent, id, and index in parent, but
  // the data is not required. After initialization, only index_in_parent
  // and unignored_index_in_parent is allowed to change, the others are
  // guaranteed to never change.
  AXNode(AXTree* tree,
         AXNode* parent,
         AXNodeID id,
         size_t index_in_parent,
         size_t unignored_index_in_parent = 0u);
  virtual ~AXNode();

  // Accessors.
  AXTree* tree() const { return tree_; }
  AXNodeID id() const { return data_.id; }
  const AXNodeData& data() const { return data_; }

  // Returns ownership of |data_| to the caller; effectively clearing |data_|.
  AXNodeData&& TakeData();

  //
  // Methods for walking the tree.
  //
  // These come in four flavors: Methods that walk all the nodes, methods that
  // walk only the unignored nodes (effectively re-structuring the tree to
  // remove all ignored nodes), and another two variants that do the above plus
  // cross tree boundaries, effectively stiching together all accessibility
  // trees that are part of the same webpage, PDF or window into a large global
  // tree.

  const std::vector<raw_ptr<AXNode, VectorExperimental>>& GetAllChildren()
      const;
  size_t GetChildCount() const;
#if DCHECK_IS_ON()
  size_t GetSubtreeCount() const;
#endif
  size_t GetChildCountCrossingTreeBoundary() const;
  size_t GetUnignoredChildCount() const;
  size_t GetUnignoredChildCountCrossingTreeBoundary() const;
  AXNode* GetChildAtIndex(size_t index) const;
  AXNode* GetChildAtIndexCrossingTreeBoundary(size_t index) const;
  AXNode* GetUnignoredChildAtIndex(size_t index) const;
  AXNode* GetUnignoredChildAtIndexCrossingTreeBoundary(size_t index) const;
  AXNode* GetParent() const;
  AXNode* GetParentCrossingTreeBoundary() const;
  AXNode* GetUnignoredParent() const;
  AXNode* GetUnignoredParentCrossingTreeBoundary() const;
  base::queue<AXNode*> GetAncestorsCrossingTreeBoundaryAsQueue() const;
  base::stack<AXNode*> GetAncestorsCrossingTreeBoundaryAsStack() const;
  size_t GetIndexInParent() const;
  size_t GetUnignoredIndexInParent() const;
  AXNode* GetFirstChild() const;
  AXNode* GetFirstChildCrossingTreeBoundary() const;
  AXNode* GetFirstUnignoredChild() const;
  AXNode* GetFirstUnignoredChildCrossingTreeBoundary() const;
  AXNode* GetLastChild() const;
  AXNode* GetLastChildCrossingTreeBoundary() const;
  AXNode* GetLastUnignoredChild() const;
  AXNode* GetLastUnignoredChildCrossingTreeBoundary() const;

  AXNode* GetDeepestFirstDescendant() const;
  AXNode* GetDeepestFirstDescendantCrossingTreeBoundary() const;
  AXNode* GetDeepestFirstUnignoredDescendant() const;
  AXNode* GetDeepestFirstUnignoredDescendantCrossingTreeBoundary() const;
  AXNode* GetDeepestLastDescendant() const;
  AXNode* GetDeepestLastDescendantCrossingTreeBoundary() const;
  AXNode* GetDeepestLastUnignoredDescendant() const;
  AXNode* GetDeepestLastUnignoredDescendantCrossingTreeBoundary() const;

  AXNode* GetNextSibling() const;
  AXNode* GetNextUnignoredSibling() const;
  AXNode* GetPreviousSibling() const;
  AXNode* GetPreviousUnignoredSibling() const;

  // Traverse the tree in depth-first pre-order.
  AXNode* GetNextUnignoredInTreeOrder() const;
  AXNode* GetPreviousUnignoredInTreeOrder() const;

  //
  // Deprecated methods for walking the tree.
  //

  const std::vector<raw_ptr<AXNode, VectorExperimental>>& children() const {
    return children_;
  }
  AXNode* parent() const { return parent_; }
  size_t index_in_parent() const { return index_in_parent_; }

  //
  // Iterators for walking the tree in depth-first pre-order.
  //

  using AllChildIterator = ChildIteratorBase<AXNode,
                                             &AXNode::GetNextSibling,
                                             &AXNode::GetPreviousSibling,
                                             &AXNode::GetFirstChild,
                                             &AXNode::GetLastChild>;
  AllChildIterator AllChildrenBegin() const;
  AllChildIterator AllChildrenEnd() const;

  using AllChildCrossingTreeBoundaryIterator =
      ChildIteratorBase<AXNode,
                        &AXNode::GetNextSibling,
                        &AXNode::GetPreviousSibling,
                        &AXNode::GetFirstChildCrossingTreeBoundary,
                        &AXNode::GetLastChildCrossingTreeBoundary>;
  AllChildCrossingTreeBoundaryIterator AllChildrenCrossingTreeBoundaryBegin()
      const;
  AllChildCrossingTreeBoundaryIterator AllChildrenCrossingTreeBoundaryEnd()
      const;

  using UnignoredChildIterator =
      ChildIteratorBase<AXNode,
                        &AXNode::GetNextUnignoredSibling,
                        &AXNode::GetPreviousUnignoredSibling,
                        &AXNode::GetFirstUnignoredChild,
                        &AXNode::GetLastUnignoredChild>;
  UnignoredChildIterator UnignoredChildrenBegin() const;
  UnignoredChildIterator UnignoredChildrenEnd() const;

  using UnignoredChildCrossingTreeBoundaryIterator =
      ChildIteratorBase<AXNode,
                        &AXNode::GetNextUnignoredSibling,
                        &AXNode::GetPreviousUnignoredSibling,
                        &AXNode::GetFirstUnignoredChildCrossingTreeBoundary,
                        &AXNode::GetLastUnignoredChildCrossingTreeBoundary>;
  UnignoredChildCrossingTreeBoundaryIterator
  UnignoredChildrenCrossingTreeBoundaryBegin() const;
  UnignoredChildCrossingTreeBoundaryIterator
  UnignoredChildrenCrossingTreeBoundaryEnd() const;

  // Returns true if this is a node on which accessibility events make sense to
  // be fired. Events are not needed on nodes that will, for example, never
  // appear in a tree that is visible to assistive software, as there will be no
  // software to handle the event on the other end.
  bool CanFireEvents() const;

  AXNode* GetLowestCommonAncestor(const AXNode& other);

  // Returns an optional integer indicating the logical order of this node
  // compared to another node, or returns an empty optional if the nodes are not
  // comparable. Nodes are not comparable if they do not share a common
  // ancestor.
  //
  //    0: if this node is logically equivalent to the other node.
  //   <0: if this node is logically less than the other node.
  //   >0: if this node is logically greater than the other node.
  //
  // Another way to look at the nodes' relative positions/logical orders is that
  // they are equivalent to pre-order traversal of the tree. If we pre-order
  // traverse from the root, the node that we visited earlier is always going to
  // be before (logically less) the node we visit later.
  std::optional<int> CompareTo(const AXNode& other) const;

  bool IsDataValid() const { return data_.id != kInvalidAXNodeID; }

  // Returns true if the node has any of the text related roles, including
  // kStaticText, kInlineTextBox and kListMarker (for Legacy Layout). Does not
  // include any text field roles.
  bool IsText() const;

  // Returns true if the node has any line break related roles or is the child
  // of a node with line break related roles.
  bool IsLineBreak() const;

  // Set the node's accessibility data. This may be done during initialization
  // or later when the node data changes.
  void SetData(const AXNodeData& src);

  // Update this node's location. This is separate from |SetData| just because
  // changing only the location is common and should be more efficient than
  // re-copying all of the data.
  //
  // The node's location is stored as a relative bounding box, the ID of
  // the element it's relative to, and an optional transformation matrix.
  // See ax_node_data.h for details.
  void SetLocation(AXNodeID offset_container_id,
                   const gfx::RectF& location,
                   gfx::Transform* transform);

  // Update this node's scroll x and y. This is separate from |SetData| just
  // because changing only the scroll info is common and should be more
  // efficient than re-copying all of the data.
  void SetScrollInfo(const int& scroll_x, const int& scroll_y);
  void GetScrollInfo(int* scroll_x, int* scroll_y) const;

  // Set the index in parent, for example if siblings were inserted or deleted.
  void SetIndexInParent(size_t index_in_parent);

  // When the node's `IsIgnored()` value changes, updates the cached values for
  // the unignored index in parent and the unignored child count.
  void UpdateUnignoredCachedValues();

  // Swap the internal children vector with |children|. This instance
  // now owns all of the passed children.
  void SwapChildren(std::vector<raw_ptr<AXNode, VectorExperimental>>* children);

  // Returns true if this node is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(const AXNode* ancestor) const;
  bool IsDescendantOfCrossingTreeBoundary(const AXNode* ancestor) const;

  // If the color is transparent, blends with the ancestor's color.
  // Note that this is imperfect; it won't work if a node is absolute-
  // positioned outside of its ancestor. However, it handles the most
  // common cases.
  SkColor ComputeColor() const;
  SkColor ComputeBackgroundColor() const;

  AXTreeManager* GetManager() const;

  //
  // Methods for accessing caret and selection information.
  //

  // Returns true if the caret is visible or there is an active selection inside
  // this node.
  bool HasVisibleCaretOrSelection() const;

  // Gets the current selection from the accessibility tree.
  AXSelection GetSelection() const;

  // Gets the unignored selection from the accessibility tree, meaning the
  // selection whose endpoints are on unignored nodes. (An "ignored" node is a
  // node that is not exposed to platform APIs: See `IsIgnored`.)
  AXSelection GetUnignoredSelection() const;

  //
  // Methods for accessing accessibility attributes including attributes that
  // are computed on the browser side. (See `AXNodeData` and
  // `AXComputedNodeData` for more information.)
  //
  // Please prefer using the methods in this class for retrieving attributes, as
  // computed attributes would be automatically returned if available.
  // Requesting the computed value for an attribute that cannot be computed
  // triggers a DCHECK failure. All Get...Attribute methods in this class do
  // the appropriate verification before requesting a computed attribute value.
  //
  // Each of the Get...Attribute methods returns a default value if not set.
  // The Has...Attribute methods can be used to disambiguate a missing value
  // from a default value. The default values are 0 for numerical attributes,
  // an empty string for string attributes, an empty list for list valued
  // attributes, and false for boolean attributes.
  //
  // Example:
  //
  // const std::string& value =
  //     GetStringAttribute(ax::mojom::StringAttribute::kValue);
  // if (!value.empty() ||
  //     HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
  //    // Handle explicitly set attribute even if an empty string.
  // }
  //
  // Unless specifically needing a UTF16 string, it is generally advisable to
  // use UTF8 strings, since these are fetched as a constant reference, whereas
  // the UTF16 versions are converted from their UTF8 counterparts on demand.
  //
  // An explicitly set attribute may disagree with the computed value. The
  // Get..Attribute methods return the explicitly set value rather than the
  // computed value in this case.
  //
  ax::mojom::Role GetRole() const { return data().role; }

  bool HasBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().HasBoolAttribute(attribute);
  }
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().GetBoolAttribute(attribute);
  }

  bool HasFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().HasFloatAttribute(attribute);
  }
  float GetFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().GetFloatAttribute(attribute);
  }

  const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
  GetIntAttributes() const {
    return data().int_attributes;
  }
  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool CanComputeIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;

  const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
  GetStringAttributes() const {
    return data().string_attributes;
  }
  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  bool CanComputeStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;

  std::u16string GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  bool HasInheritedStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  std::u16string GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  const std::vector<
      std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
  GetIntListAttributes() const {
    return data().intlist_attributes;
  }
  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  bool CanComputeIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;

  bool HasStringListAttribute(ax::mojom::StringListAttribute attribute) const {
    return data().HasStringListAttribute(attribute);
  }
  const std::vector<std::string>& GetStringListAttribute(
      ax::mojom::StringListAttribute attribute) const {
    return data().GetStringListAttribute(attribute);
  }

  const base::StringPairs& GetHtmlAttributes() const {
    return data().html_attributes;
  }
  bool HasHtmlAttribute(const char* attribute) const {
    return data().HasHtmlAttribute(attribute);
  }
  const std::string& GetHtmlAttribute(const char* attribute) const {
    return data().GetHtmlAttribute(attribute);
  }
  std::u16string GetHtmlAttributeUTF16(const char* attribute) const {
    return data().GetHtmlAttributeUTF16(attribute);
  }

  AXTextAttributes GetTextAttributes() const {
    return data().GetTextAttributes();
  }

  bool HasState(ax::mojom::State state) const { return data().HasState(state); }
  ax::mojom::State GetState() const {
    return static_cast<ax::mojom::State>(data().state);
  }

  bool HasAction(ax::mojom::Action action) const {
    return data().HasAction(action);
  }

  bool HasTextStyle(ax::mojom::TextStyle text_style) const {
    return data().HasTextStyle(text_style);
  }

  ax::mojom::NameFrom GetNameFrom() const { return data().GetNameFrom(); }

  ax::mojom::DescriptionFrom GetDescriptionFrom() const {
    return data().GetDescriptionFrom();
  }

  ax::mojom::InvalidState GetInvalidState() const {
    return data().GetInvalidState();
  }

  // Return the hierarchical level if supported.
  std::optional<int> GetHierarchicalLevel() const;

  // PosInSet and SetSize public methods.
  bool IsOrderedSetItem() const;
  bool IsOrderedSet() const;
  std::optional<int> GetPosInSet() const;
  std::optional<int> GetSetSize() const;

  // Helpers for GetPosInSet and GetSetSize.
  // Returns true if the role of ordered set matches the role of item.
  // Returns false otherwise.
  bool SetRoleMatchesItemRole(const AXNode* ordered_set) const;

  // Container objects that should be ignored for computing PosInSet and SetSize
  // for ordered sets.
  bool IsIgnoredContainerForOrderedSet() const;

  // Helper functions that returns true when we are on a row/row group inside of
  // a tree grid. Also works for rows that are part of a row group inside a tree
  // grid. Returns false otherwise.
  bool IsRowInTreeGrid(const AXNode* ordered_set) const;
  bool IsRowGroupInTreeGrid() const;

  // Returns the accessible name for this node. This could have originated from
  // e.g. an onscreen label, or an ARIA label.
  const std::string& GetNameUTF8() const;
  std::u16string GetNameUTF16() const;

  // If this node is a leaf, returns the text content of this node. This is
  // equivalent to its visible accessible name. Otherwise, if this node is not a
  // leaf, represents every non-textual child node with a special "embedded
  // object character", and every textual child node with its text content.
  // Textual nodes include e.g. static text and white space.
  //
  // This is how displayed text and embedded objects are represented in
  // ATK and IAccessible2 APIs.
  //
  // TODO(nektar): Consider changing the return value to std::string.
  const std::u16string& GetHypertext() const;

  // Temporary accessor methods until hypertext is fully migrated to this class.
  // Hypertext won't eventually need to be accessed outside this class.
  const std::map<int, int>& GetHypertextOffsetToHyperlinkChildIndex() const;

  // Returns the text that is found inside this node and all its descendants;
  // including text found in embedded objects.
  //
  // Only text displayed on screen is included. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node, is
  // not returned.
  //
  // Does not take into account line breaks that have been introduced by layout.
  // For example, in the Web context, "A<div>B</div>C" would produce "ABC".
  const std::string& GetTextContentUTF8() const;
  const std::u16string& GetTextContentUTF16() const;

  // Returns the length of the text (in code units) that is found inside
  // this node and all its descendants; including text found in embedded
  // objects.
  //
  // Only text displayed on screen is counted. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node, is
  // not included.
  //
  // The length of the text is either in UTF8 or UTF16 code units, not in
  // grapheme clusters.
  int GetTextContentLengthUTF8() const;
  int GetTextContentLengthUTF16() const;

  // Returns the smallest bounding box that can enclose the given range of
  // characters in the node's text contents. The bounding box is relative to
  // this node's coordinate system as specified in
  // `AXNodeData::relative_bounds`.
  //
  // Note that `start_offset` and `end_offset` are in UTF16 code units, not in
  // grapheme clusters. For example, the following Hindi text
  // u"\x0939\x093F\x0928\x094D\x0926\x0940" consists of two glyphs and has
  // character offsets {40, 40, 59, 59, 59, 59} since the first glyph is
  // represented by 2 code units in UTF16 and the second by 4 code units.
  gfx::RectF GetTextContentRangeBoundsUTF16(int start_offset,
                                            int end_offset) const;

  // Returns a string representing the language code.
  //
  // This will consider the language declared in the DOM, and may eventually
  // attempt to automatically detect the language from the text.
  //
  // This language code will be BCP 47.
  //
  // Returns empty string if no appropriate language was found.
  std::string GetLanguage() const;

  // Returns the value of a control such as an atomic text field (<input> or
  // <textarea>), a content editable, a submit button, a slider, a progress bar,
  // a scroll bar, a meter, a spinner, a <select> element, a date picker or an
  // ARIA combo box. In order to minimize cross-process communication between
  // the renderer and the browser, this method may compute the value from the
  // control's inner text in the case of a content editable. For range controls,
  // such as sliders and scroll bars, the value of aria-valuetext takes priority
  // over the value of aria-valuenow.
  std::string GetValueForControl() const;

  //
  // Helper functions for tables, table rows, and table cells.
  // Most of these functions construct and cache an AXTableInfo behind
  // the scenes to infer many properties of tables.
  //
  // These interfaces use attributes provided by the source of the
  // AX tree where possible, but fills in missing details and ignores
  // specified attributes when they're bad or inconsistent. That way
  // you're guaranteed to get a valid, consistent table when using these
  // interfaces.
  //

  // Table-like nodes (including grids). All indices are 0-based except
  // ARIA indices are all 1-based. In other words, the top-left corner
  // of the table is row 0, column 0, cell index 0 - but that same cell
  // has a minimum ARIA row index of 1 and column index of 1.
  //
  // The below methods return std::nullopt if the AXNode they are called on is
  // not inside a table.
  bool IsTable() const;
  std::optional<int> GetTableColCount() const;
  std::optional<int> GetTableRowCount() const;
  std::optional<int> GetTableAriaColCount() const;
  std::optional<int> GetTableAriaRowCount() const;
  std::optional<int> GetTableCellCount() const;
  AXNode* GetTableCaption() const;
  AXNode* GetTableCellFromIndex(int index) const;
  AXNode* GetTableCellFromCoords(int row_index, int col_index) const;
  AXNode* GetTableCellFromAriaCoords(int aria_row_index, int aria_col_index) const;
  // Get all the column header node ids of the table.
  std::vector<AXNodeID> GetTableColHeaderNodeIds() const;
  // Get the column header node ids associated with |col_index|.
  std::vector<AXNodeID> GetTableColHeaderNodeIds(int col_index) const;
  // Get the row header node ids associated with |row_index|.
  std::vector<AXNodeID> GetTableRowHeaderNodeIds(int row_index) const;
  std::vector<AXNodeID> GetTableUniqueCellIds() const;
  // Extra computed nodes for the accessibility tree for macOS:
  // one column node for each table column, followed by one
  // table header container node, or nullptr if not applicable.
  const std::vector<raw_ptr<AXNode, VectorExperimental>>* GetExtraMacNodes()
      const;

  // Return true for mock nodes added to the map, such as extra mac nodes.
  bool IsGenerated() const;

  // Table row-like nodes.
  bool IsTableRow() const;
  std::optional<int> GetTableRowRowIndex() const;
  // Get the node ids that represent rows in a table.
  std::vector<AXNodeID> GetTableRowNodeIds() const;

#if BUILDFLAG(IS_APPLE)
  // Table column-like nodes. These nodes are only present on macOS.
  bool IsTableColumn() const;
  std::optional<int> GetTableColColIndex() const;
#endif  // BUILDFLAG(IS_APPLE)

  // Table cell-like nodes.
  bool IsTableCellOrHeader() const;
  std::optional<int> GetTableCellIndex() const;
  std::optional<int> GetTableCellColIndex() const;
  // The row index of a cell. If a row is passed in, use the first cell.
  std::optional<int> GetTableCellRowIndex() const;
  std::optional<int> GetTableCellColSpan() const;
  std::optional<int> GetTableCellRowSpan() const;
  std::optional<int> GetTableCellAriaColIndex() const;
  // The ARIA row index of a cell. If a row is passed in, use the first cell.
  std::optional<int> GetTableCellAriaRowIndex() const;
  std::vector<AXNodeID> GetTableCellColHeaderNodeIds() const;
  std::vector<AXNodeID> GetTableCellRowHeaderNodeIds() const;
  void GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const;
  void GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const;

  // Returns true if this node is a cell or a row/column header in an ARIA grid
  // or treegrid.
  bool IsCellOrHeaderOfAriaGrid() const;

  // Return an object containing information about the languages detected on
  // this node.
  // Callers should not retain this pointer, instead they should request it
  // every time it is needed.
  //
  // Returns nullptr if the node has no language info.
  AXLanguageInfo* GetLanguageInfo() const;

  // This should only be called by LabelLanguageForSubtree and is used as part
  // of the language detection feature.
  void SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info);

  // Destroy the language info for this node.
  void ClearLanguageInfo();

  // Get a reference to the cached information stored for this node.
  const AXComputedNodeData& GetComputedNodeData() const;

  // Clear the cached information stored for this node because it is
  // out-of-date.
  void ClearComputedNodeData();

  // Returns true if node is a group and is a direct descendant of a set-like
  // element.
  bool IsEmbeddedGroup() const;

  // Returns true if this node has the ignored state or a presentational ARIA
  // role. Focused nodes are, by design, not ignored.
  bool IsIgnored() const;

  // Some nodes are not ignored but should be skipped during text navigation.
  // For example, on some platforms screen readers should not stop when
  // encountering a splitter during character and word navigation.
  bool IsIgnoredForTextNavigation() const;

  // Returns true if node is invisible, or if it is ignored as determined by
  // `AXNode::IsIgnored()`.
  bool IsInvisibleOrIgnored() const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to any
  // platform's accessibility layer.
  bool IsChildOfLeaf() const;

  // Returns true if this is a leaf node that has no text content. Note that all
  // descendants of a leaf node are not exposed to any platform's accessibility
  // layer, but they may be used to compute the node's text content. Note also
  // that, ignored nodes (leaf or otherwise) do not expose their text content or
  // hypertext to the platforms' accessibility layer, but they expose the text
  // content or hypertext of their unignored descendants.
  //
  // For example, empty text fields might have a set of unignored nested divs
  // inside them:
  // ++kTextField
  // ++++kGenericContainer
  // ++++++kGenericContainer
  bool IsEmptyLeaf() const;

  // Returns true if this is a leaf node, meaning all its
  // descendants should not be exposed to any platform's accessibility
  // layer.
  //
  // The definition of a leaf includes nodes with children that are exclusively
  // an internal renderer implementation, such as the children of an HTML-based
  // text field (<input> and <textarea>), as well as nodes with presentational
  // children according to the ARIA and HTML5 Specs. Also returns true if all of
  // the node's descendants are ignored.
  //
  // A leaf node should never have children that are focusable or
  // that might send notifications.
  bool IsLeaf() const;

  // Helper to determine if the node is focusable. This does more than just
  // use HasState(ax::mojom::State::kFocusable) -- it also checks whether the
  // object is a likely activedescendant.
  bool IsFocusable() const;

  // Helper to determine whether the node can be an active descendant, and is a
  // likely candidate to be one. An id and an ARIA role are required, and the
  // role must be item-like.
  bool IsLikelyARIAActiveDescendant() const;

  // Returns true if this node is a list marker or if it's a descendant
  // of a list marker node. Returns false otherwise.
  bool IsInListMarker() const;

  // Returns true if this node is a collapsed combobox select that is parent to
  // a menu list popup.
  bool IsCollapsedMenuListSelect() const;

  // Returns true if this node is at the root of an accessibility tree that is
  // hosted by a presentational iframe.
  bool IsRootWebAreaForPresentationalIframe() const;

  // Returns the popup button ancestor of this current node if any. The popup
  // button needs to be the parent of a menu list popup and needs to be
  // collapsed.
  AXNode* GetCollapsedMenuListSelectAncestor() const;

  // If this node is exposed to the platform's accessibility layer, returns this
  // node. Otherwise, returns the lowest ancestor that is exposed to the
  // platform. (See `IsLeaf()` and `IsIgnored()` for information on what is
  // exposed to platform APIs.)
  AXNode* GetLowestPlatformAncestor() const;

  // If this node is within an editable region, returns the node that is at the
  // root of that editable region, otherwise returns nullptr. In accessibility,
  // an editable region is synonymous to a node with the kTextField role, or a
  // contenteditable without the role, (see `AXNodeData::IsTextField()`).
  AXNode* GetTextFieldAncestor() const;

  // Get the native text field's deepest container; the lowest descendant that
  // contains all its text. Returns nullptr if the text field is empty, or if it
  // is not an atomic text field, (e.g., <input> or <textarea>).
  AXNode* GetTextFieldInnerEditorElement() const;

  // If this node is within a container (or widget) that supports either single
  // or multiple selection, returns the node that represents the container.
  AXNode* GetSelectionContainer() const;

  // If this node is within a table, returns the node that represents the table.
  AXNode* GetTableAncestor() const;

  // Returns true if this node is either an atomic text field , or one of its
  // ancestors is. An atomic text field does not expose its internal
  // implementation to assistive software, appearing as a single leaf node in
  // the accessibility tree. Examples include: An <input> or a <textarea> on the
  // Web, a text field in a PDF form, a Views-based text field, or a native
  // Android one.
  bool IsDescendantOfAtomicTextField() const;

  // Finds and returns a pointer to ordered set containing node.
  AXNode* GetOrderedSet() const;

  // Returns true if the node supports the read-only attribute.
  bool IsReadOnlySupported() const;

  // Returns true if the node is marked read-only or is disabled. By default,
  // all nodes that can't be edited are read-only.
  bool IsReadOnlyOrDisabled() const;

  // Returns true if node is from Views (and not web content).
  bool IsView() const;

 private:
  AXTableInfo* GetAncestorTableInfo() const;
  void IdVectorToNodeVector(const std::vector<AXNodeID>& ids,
                            std::vector<AXNode*>* nodes) const;

  int UpdateUnignoredCachedValuesRecursive(int start_index);
  AXNode* ComputeLastUnignoredChildRecursive() const;
  AXNode* ComputeFirstUnignoredChildRecursive() const;

  // Returns the value of a range control such as a slider or a scroll bar in
  // text format.
  std::string GetTextForRangeValue() const;

  // Returns the value of a color well (a color chooser control) in a human
  // readable format. For example: "50% red 40% green 90% blue".
  std::string GetValueForColorWell() const;

  // Compute the actual value of a color attribute that needs to be
  // blended with ancestor colors.
  SkColor ComputeColorAttribute(ax::mojom::IntAttribute color_attr) const;

  const raw_ptr<AXTree> tree_;  // Owns this.
  size_t index_in_parent_;
  size_t unignored_index_in_parent_;
  size_t unignored_child_count_ = 0;
  const raw_ptr<AXNode> parent_;
  std::vector<raw_ptr<AXNode, VectorExperimental>> children_;

  // Stores information about this node that is immutable and which has been
  // computed by the tree's source, such as `content::BlinkAXTreeSource`.
  AXNodeData data_;

  // See the class comment in "ax_hypertext.h" for an explanation of this
  // member.
  mutable AXHypertext hypertext_;

  // Stores information about this node that can be computed on demand and
  // cached.
  mutable std::unique_ptr<AXComputedNodeData> computed_node_data_;

  // Stores the detected language computed from the node's text.
  std::unique_ptr<AXLanguageInfo> language_info_;
};

AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXNode& node);
AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXNode* node);

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::ChildIteratorBase(const NodeType* parent,
                                                        NodeType* child)
    : parent_(parent), child_(child) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::ChildIteratorBase(const ChildIteratorBase&
                                                            it)
    : parent_(it.parent_), child_(it.child_) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::ChildIteratorBase<NodeType,
                               NextSibling,
                               PreviousSibling,
                               FirstChild,
                               LastChild>::operator==(const ChildIteratorBase&
                                                          rhs) const {
  return parent_ == rhs.parent_ && child_ == rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::ChildIteratorBase<NodeType,
                               NextSibling,
                               PreviousSibling,
                               FirstChild,
                               LastChild>::operator!=(const ChildIteratorBase&
                                                          rhs) const {
  return parent_ != rhs.parent_ || child_ != rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>&
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::operator++() {
  // |child_ = nullptr| denotes the iterator's past-the-end condition. When we
  // increment the iterator past the end, we remain at the past-the-end iterator
  // condition.
  if (child_ && parent_) {
    if (child_ == (parent_->*LastChild)())
      child_ = nullptr;
    else
      child_ = (child_->*NextSibling)();
  }

  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>&
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::operator--() {
  if (parent_) {
    // If the iterator is past the end, |child_=nullptr|, decrement the iterator
    // gives us the last iterator element.
    if (!child_)
      child_ = (parent_->*LastChild)();
    // Decrement the iterator gives us the previous element, except when the
    // iterator is at the beginning; in which case, decrementing the iterator
    // remains at the beginning.
    else if (child_ != (parent_->*FirstChild)())
      child_ = (child_->*PreviousSibling)();
  }

  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::get() const {
  DCHECK(child_);
  return child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType& AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::operator*() const {
  DCHECK(child_);
  return *child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::operator->() const {
  DCHECK(child_);
  return child_;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_H_
