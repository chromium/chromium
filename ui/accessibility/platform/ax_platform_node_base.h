// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_text_attributes.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_text_boundary.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(USE_ATK)
#include <atk/atk.h>
#endif

namespace ui {

struct AXNodeData;

// TODO(nektar): Move this struct over to AXNode so that it can be accessed by
// AXPosition.
struct COMPONENT_EXPORT(AX_PLATFORM) AXLegacyHypertext {
  using OffsetToIndex = std::map<int32_t, int32_t>;

  AXLegacyHypertext();
  ~AXLegacyHypertext();
  AXLegacyHypertext(const AXLegacyHypertext& other);
  AXLegacyHypertext& operator=(const AXLegacyHypertext& other);
  AXLegacyHypertext(AXLegacyHypertext&& other) noexcept;
  AXLegacyHypertext& operator=(AXLegacyHypertext&& other);

  // A flag that should be set if the hypertext information in this struct is
  // out-of-date and needs to be updated. This flag should always be set upon
  // construction because constructing this struct doesn't compute the
  // hypertext.
  bool needs_update = true;

  // Maps an embedded character offset in |hypertext| to an index in
  // |hyperlinks|.
  OffsetToIndex hyperlink_offset_to_index;

  // The unique id of a AXPlatformNodes for each hyperlink.
  // TODO(nektar): Replace object IDs with child indices if we decide that
  // we are not implementing IA2 hyperlinks for anything other than IA2
  // Hypertext.
  std::vector<int32_t> hyperlinks;

  std::u16string hypertext;
};

class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeBase : public AXPlatformNode {
 public:
  using AXPosition = AXNodePosition::AXPositionInstance;

  ~AXPlatformNodeBase() override;
  AXPlatformNodeBase(const AXPlatformNodeBase&) = delete;
  AXPlatformNodeBase& operator=(const AXPlatformNodeBase&) = delete;

  // These are simple wrappers to our delegate.
  const AXNodeData& GetData() const;
  gfx::NativeViewAccessible GetFocus() const;
  gfx::NativeViewAccessible GetParent() const;
  size_t GetChildCount() const;
  gfx::NativeViewAccessible ChildAtIndex(size_t index) const;

  std::string GetName() const;

  // This returns nullopt if there's no parent, it's unable to find the child in
  // the list of its parent's children, or its parent doesn't have children.
  virtual std::optional<size_t> GetIndexInParent();

  // Returns a stack of ancestors of this node. The node at the top of the stack
  // is the top most ancestor.
  base::stack<gfx::NativeViewAccessible> GetAncestors();

  // Returns an optional integer indicating the logical order of this node
  // compared to another node or returns an empty optional if the nodes
  // are not comparable.
  //    0: if this position is logically equivalent to the other node
  //   <0: if this position is logically less than (before) the other node
  //   >0: if this position is logically greater than (after) the other node
  std::optional<int> CompareTo(AXPlatformNodeBase& other);

  // AXPlatformNode.
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;

#if BUILDFLAG(IS_APPLE)
  void AnnounceTextAs(const std::u16string& text,
                      AnnouncementType announcement_type) override;
#endif

  AXPlatformNodeDelegate* GetDelegate() const override;
  bool IsDescendantOf(AXPlatformNode* ancestor) const override;

  // Helpers.
  AXPlatformNodeBase* GetPlatformParent() const;
  AXPlatformNodeBase* GetPreviousSibling() const;
  AXPlatformNodeBase* GetNextSibling() const;
  AXPlatformNodeBase* GetFirstChild() const;
  AXPlatformNodeBase* GetLastChild() const;
  bool IsDescendant(AXPlatformNodeBase* descendant);

  AXNodeID GetNodeId() const;
  AXPlatformNodeBase* GetActiveDescendant() const;
  AXPlatformNodeBase* GetPlatformTextFieldAncestor() const;

  using AXPlatformNodeChildIterator =
      AXNode::ChildIteratorBase<AXPlatformNodeBase,
                                &AXPlatformNodeBase::GetNextSibling,
                                &AXPlatformNodeBase::GetPreviousSibling,
                                &AXPlatformNodeBase::GetFirstChild,
                                &AXPlatformNodeBase::GetLastChild>;
  AXPlatformNodeChildIterator AXPlatformNodeChildrenBegin() const;
  AXPlatformNodeChildIterator AXPlatformNodeChildrenEnd() const;

  virtual ax::mojom::Role GetRole() const;
  bool HasBoolAttribute(ax::mojom::BoolAttribute attribute) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute, bool* value) const;

  bool HasFloatAttribute(ax::mojom::FloatAttribute attribute) const;
  float GetFloatAttribute(ax::mojom::FloatAttribute attribute) const;
  bool GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                         float* value) const;

  const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
  GetIntAttributes() const;
  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const;

  const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
  GetStringAttributes() const;
  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;
  std::u16string GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            std::u16string* value) const;

  bool HasInheritedStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetInheritedStringAttribute(ax::mojom::StringAttribute attribute,
                                   std::string* value) const;
  std::u16string GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetInheritedString16Attribute(ax::mojom::StringAttribute attribute,
                                     std::u16string* value) const;

  const std::vector<
      std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
  GetIntListAttributes() const;
  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;
  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  bool HasStringListAttribute(ax::mojom::StringListAttribute attribute) const;
  const std::vector<std::string>& GetStringListAttribute(
      ax::mojom::StringListAttribute attribute) const;
  bool GetStringListAttribute(ax::mojom::StringListAttribute attribute,
                              std::vector<std::string>* value) const;

  bool HasHtmlAttribute(const char* attribute) const;
  const base::StringPairs& GetHtmlAttributes() const;
  bool GetHtmlAttribute(const char* attribute, std::string* value) const;
  bool GetHtmlAttribute(const char* attribute, std::u16string* value) const;

  AXTextAttributes GetTextAttributes() const;

  bool HasState(ax::mojom::State state) const;
  ax::mojom::State GetState() const;

  bool HasAction(ax::mojom::Action action) const;

  bool HasTextStyle(ax::mojom::TextStyle text_style) const;

  ax::mojom::NameFrom GetNameFrom() const;

  bool HasNameFromOtherElement() const;

  // Returns the selection container if inside one.
  AXPlatformNodeBase* GetSelectionContainer() const;

  // Returns the table or ARIA grid if inside one.
  AXPlatformNodeBase* GetTable() const;

  // If inside an HTML or ARIA table, returns the object containing the caption.
  // Returns nullptr if not inside a table, or if there is no
  // caption.
  AXPlatformNodeBase* GetTableCaption() const;

  // If inside a table or ARIA grid, returns the cell found at the given index.
  // Indices are in row major order and each cell is counted once regardless of
  // its span. Returns nullptr if the cell is not found or if not inside a
  // table.
  AXPlatformNodeBase* GetTableCell(int index) const;

  // If inside a table or ARIA grid, returns the cell at the given row and
  // column (0-based). Works correctly with cells that span multiple rows or
  // columns. Returns nullptr if the cell is not found or if not inside a
  // table.
  AXPlatformNodeBase* GetTableCell(int row, int column) const;

  // If inside a table or ARIA grid and given 1-based row and column,
  // returns the cell in ARIA grid or table that matches the corresponding
  // 1-based aria-rowindex and aria-colindex values, if one exists.
  AXPlatformNodeBase* GetAriaTableCell(int aria_row, int aria_column) const;

  // If inside a table or ARIA grid, returns the zero-based index of the cell.
  // Indices are in row major order and each cell is counted once regardless of
  // its span. Returns std::nullopt if not a cell or if not inside a table.
  std::optional<int> GetTableCellIndex() const;

  // If inside a table or ARIA grid, returns the physical column number for the
  // current cell. In contrast to logical columns, physical columns always start
  // from 0 and have no gaps in their numbering. Logical columns can be set
  // using aria-colindex. Returns std::nullopt if not a cell or if not inside a
  // table.
  std::optional<int> GetTableColumn() const;

  // If inside a table or ARIA grid, returns the number of physical columns.
  // Returns std::nullopt if not inside a table.
  std::optional<int> GetTableColumnCount() const;

  // If inside a table or ARIA grid, returns the number of ARIA columns.
  // Returns std::nullopt if not inside a table.
  std::optional<int> GetTableAriaColumnCount() const;

  // If inside a table or ARIA grid, returns the number of physical columns that
  // this cell spans. Returns std::nullopt if not a cell or if not inside a
  // table.
  std::optional<int> GetTableColumnSpan() const;

  // If inside a table or ARIA grid, returns the physical row number for the
  // current cell. In contrast to logical rows, physical rows always start from
  // 0 and have no gaps in their numbering. Logical rows can be set using
  // aria-rowindex. Returns std::nullopt if not a cell or if not inside a
  // table.
  std::optional<int> GetTableRow() const;

  // If inside a table or ARIA grid, returns the number of physical rows.
  // Returns std::nullopt if not inside a table.
  std::optional<int> GetTableRowCount() const;

  // If inside a table or ARIA grid, returns the number of ARIA rows.
  // Returns std::nullopt if not inside a table.
  std::optional<int> GetTableAriaRowCount() const;

  // If inside a table or ARIA grid, returns the number of physical rows that
  // this cell spans. Returns std::nullopt if not a cell or if not inside a
  // table.
  std::optional<int> GetTableRowSpan() const;

  // Returns the font size converted to points, if available.
  std::optional<float> GetFontSizeInPoints() const;

  // See `AXNode::HasVisibleCaretOrSelection`.
  bool HasVisibleCaretOrSelection() const;

  // See AXPlatformNodeDelegate::IsChildOfLeaf().
  bool IsChildOfLeaf() const;

  // See AXPlatformNodeDelegate::IsLeaf().
  bool IsLeaf() const;

  // See AXPlatformNodeDelegate::IsInvisibleOrIgnored().
  bool IsInvisibleOrIgnored() const;

  // Returns true if this node is currently focused.
  bool IsFocused() const;

  // Returns true if this node is focusable.
  // This does more than just use HasState(ax::mojom::State::kFocusable) -- it
  // also checks whether the object is a likely activedescendant.
  bool IsFocusable() const;

  // Returns true if this node can be scrolled either in the horizontal or the
  // vertical direction.
  bool IsScrollable() const;

  // Returns true if this node can be scrolled in the horizontal direction.
  bool IsHorizontallyScrollable() const;

  // Returns true if this node can be scrolled in the vertical direction.
  bool IsVerticallyScrollable() const;

  // See AXNodeData::IsTextField().
  bool IsTextField() const;

  // See AXNodeData::IsAtomicTextField().
  bool IsAtomicTextField() const;

  // See AXNodeData::IsNonAtomicTextField().
  bool IsNonAtomicTextField() const;

  // See AXNode::IsText().
  bool IsText() const;

  // Determines whether an element should be exposed with checkable state, and
  // possibly the checked state. Examples are check box and radio button.
  // Objects that are exposed as toggle buttons use the platform pressed state
  // in some platform APIs, and should not be exposed as checkable. They don't
  // expose the platform equivalent of the internal checked state.
  virtual bool IsPlatformCheckable() const;

  bool HasFocus();

  // If this node is a leaf, returns the visible accessible name of this node.
  // Otherwise represents every non-textual child node with a special "embedded
  // object character", and every textual child node with its visible accessible
  // name. This is how displayed text and embedded objects are represented in
  // ATK and IA2 APIs.
  std::u16string GetHypertext() const;

  // Returns the text that is found inside this node and all its descendants;
  // including text found in embedded objects.
  //
  // Only text displayed on screen is included. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node,
  // e.g. aria-label and HTML title, is not returned.
  std::u16string GetTextContentUTF16() const;

  // Returns the value of a control such as a text field, a slider, a <select>
  // element, a date picker or an ARIA combo box. In order to minimize
  // cross-process communication between the renderer and the browser, may
  // compute the value from the control's inner text in the case of a text
  // field.
  std::u16string GetValueForControl() const;

  // Represents a non-static text node in IAccessibleHypertext (and ATK in the
  // future). This character is embedded in the response to
  // IAccessibleText::get_text, indicating the position where a non-static text
  // child object appears.
  static const char16_t kEmbeddedCharacter;

  // Get a node given its unique id or null in the case that the id is unknown.
  static AXPlatformNode* GetFromUniqueId(int32_t unique_id);

  // Return the number of instances of AXPlatformNodeBase, for leak testing.
  static size_t GetInstanceCountForTesting();

  static void SetOnNotifyEventCallbackForTesting(
      ax::mojom::Event event_type,
      base::RepeatingClosure callback);

  // This method finds text boundaries in the text used for platform text APIs.
  // Implementations may use side-channel data such as line or word indices to
  // produce appropriate results.
  // Returns -1 if the requested boundary has not been found.
  virtual int FindTextBoundary(ax::mojom::TextBoundary boundary,
                               int offset,
                               ax::mojom::MoveDirection direction,
                               ax::mojom::TextAffinity affinity) const;

  enum ScrollType {
    TopLeft,
    BottomRight,
    TopEdge,
    BottomEdge,
    LeftEdge,
    RightEdge,
    Anywhere,
  };
  bool ScrollToNode(ScrollType scroll_type);

  // This will return the nearest leaf node to the point, the leaf node will not
  // necessarily be directly under the point. This utilizes
  // AXPlatformNodeDelegate::HitTestSync, which in the case of
  // BrowserAccessibility, may not be accurate after a single call. See
  // BrowserAccessibilityManager::CachingAsyncHitTest
  AXPlatformNodeBase* NearestLeafToPoint(gfx::Point point) const;

  // Return the nearest text index to a point in screen coordinates for an
  // accessibility node. If the node is not a text only node, the implicit
  // nearest index is zero. Note this will only find the index of text on the
  // input node. Due to perf concerns, this should only be called on leaf nodes.
  int NearestTextIndexToPoint(gfx::Point point);

  TextAttributeList ComputeTextAttributes() const;

  // Get the number of items selected. It checks kMultiselectable and uses
  // GetSelectedItems to get the selected number.
  int GetSelectionCount() const;

  // If this object is a container that supports selectable children, returns
  // the selected item at the provided index.
  AXPlatformNodeBase* GetSelectedItem(int selected_index) const;

  // If this object is a container that supports selectable children,
  // returns the number of selected items in this container.
  // |out_selected_items| could be set to nullptr if the caller just
  // needs to know the number of items selected.
  // |max_items| represents the number that the caller expects as a
  // maximum. For a single selection list box, it will be 1.
  int GetSelectedItems(
      int max_items,
      std::vector<AXPlatformNodeBase*>* out_selected_items = nullptr) const;

  //
  // Delegate.  This is a weak reference which owns |this|.
  //
  raw_ptr<AXPlatformNodeDelegate> delegate_ = nullptr;

  // Uses the delegate to calculate this node's PosInSet.
  std::optional<int> GetPosInSet() const;

  // Uses the delegate to calculate this node's SetSize.
  std::optional<int> GetSetSize() const;

  // Returns true if this object is at the root of what most accessibility APIs
  // consider to be a document, such as the root of a webpage, an iframe, or a
  // PDF.
  bool IsPlatformDocument() const;

 protected:
  AXPlatformNodeBase();

  // AXPlatformNode overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;

  bool IsStructuredAnnotation() const;

  // Get the role description from the node data or from the image annotation
  // status.
  std::u16string GetRoleDescription() const;
  std::u16string GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute()
      const;

  // Return true if a kImage corresponds to an image map (has children).
  // Cannot be called on nodes with a role other than kImage.
  bool IsImageWithMap() const;

  // Return true if a descendant of this has a kComment.
  static bool DescendantHasComment(const AXPlatformNodeBase* node);

  // Cast a gfx::NativeViewAccessible to an AXPlatformNodeBase if it is one,
  // or return NULL if it's not an instance of this class.
  static AXPlatformNodeBase* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  virtual void Dispose();

  // Sets the hypertext selection in this object if possible.
  bool SetHypertextSelection(int start_offset, int end_offset);

#if BUILDFLAG(USE_ATK)
  using PlatformAttributeList = AtkAttributeSet*;
#else
  using PlatformAttributeList = std::vector<std::wstring>;
#endif

  // Compute the attributes exposed via platform accessibility objects and put
  // them into an attribute list, |attributes|. Currently only used by
  // IAccessible2 on Windows and ATK on Aura Linux.
  void ComputeAttributes(PlatformAttributeList* attributes);

  // If the string attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::StringAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // If the bool attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::BoolAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // If the int attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::IntAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // A helper to add the given string value to |attributes|.
  virtual void AddAttributeToList(const char* name,
                                  const std::string& value,
                                  PlatformAttributeList* attributes);

  // A virtual method that subclasses use to actually add the attribute to
  // |attributes|.
  virtual void AddAttributeToList(const char* name,
                                  const char* value,
                                  PlatformAttributeList* attributes);

  // Escapes characters in string attributes as required by the IA2 Spec
  // and AT-SPI2. It's okay for input to be the same as output.
  static void SanitizeStringAttribute(const std::string& input,
                                      std::string* output);

  // Escapes characters in text attribute values as required by the platform.
  // It's okay for input to be the same as output. The default implementation
  // does nothing to the input value.
  virtual void SanitizeTextAttributeValue(const std::string& input,
                                          std::string* output) const;

  // Compute the hypertext for this node to be exposed via IA2 and ATK This
  // method is responsible for properly embedding children using the special
  // embedded element character.
  void UpdateComputedHypertext() const;

  // Selection helper functions.
  // The following functions retrieve the endpoints of the current selection.
  // First they check for a local selection found on the current control, e.g.
  // when querying the selection on a textarea.
  // If not found they retrieve the global selection found on the current frame.
  int GetSelectionAnchor(const AXSelection* selection);
  int GetSelectionFocus(const AXSelection* selection);

  // Retrieves the selection offsets in the way required by the IA2 APIs.
  // selection_start and selection_end are -1 when there is no selection active
  // on this object.
  // The greatest of the two offsets is one past the last character of the
  // selection.)
  void GetSelectionOffsets(int* selection_start, int* selection_end);
  void GetSelectionOffsets(const AXSelection* selection,
                           int* selection_start,
                           int* selection_end);
  void GetSelectionOffsetsFromTree(const AXSelection* selection,
                                   int* selection_start,
                                   int* selection_end);

  // Returns the hyperlink at the given text position, or nullptr if no
  // hyperlink can be found.
  AXPlatformNodeBase* GetHyperlinkFromHypertextOffset(int offset);

  // Functions for retrieving offsets for hyperlinks and hypertext.
  // Return -1 in case of failure.
  int32_t GetHyperlinkIndexFromChild(AXPlatformNodeBase* child);
  int32_t GetHypertextOffsetFromHyperlinkIndex(int32_t hyperlink_index);
  int32_t GetHypertextOffsetFromChild(AXPlatformNodeBase* child);
  int HypertextOffsetFromChildIndex(int child_index) const;
  int32_t GetHypertextOffsetFromDescendant(AXPlatformNodeBase* descendant);

  // If the selection endpoint is either equal to or an ancestor of this object,
  // returns endpoint_offset.
  // If the selection endpoint is a descendant of this object, returns its
  // offset. Otherwise, returns either 0 or the length of the hypertext
  // depending on the direction of the selection.
  // Returns -1 in case of unexpected failure, e.g. the selection endpoint
  // cannot be found in the accessibility tree.
  int GetHypertextOffsetFromEndpoint(AXPlatformNodeBase* endpoint_object,
                                     int endpoint_offset);

  AXPlatformNodeBase::AXPosition HypertextOffsetToEndpoint(
      int hypertext_offset) const;

  bool IsSameHypertextCharacter(const AXLegacyHypertext& old_hypertext,
                                size_t old_char_index,
                                size_t new_char_index);
  void ComputeHypertextRemovedAndInserted(
      const AXLegacyHypertext& old_hypertext,
      size_t* start,
      size_t* old_len,
      size_t* new_len);

  // Based on the characteristics of this object, such as its role and the
  // presence of a multiselectable attribute, returns the maximum number of
  // selectable children that this object could potentially contain.
  int GetMaxSelectableItems() const;

  mutable AXLegacyHypertext hypertext_;

 private:
  // Returns true if the index represents a text character.
  bool IsText(const std::u16string& text,
              size_t index,
              bool is_indexed_from_end = false);

  // Compute value for object attribute details-roles on aria-details nodes.
  std::string ComputeDetailsRoles() const;

  // Is there an aria-describedby that points to a role="tooltip".
  bool IsDescribedByTooltip() const;

  friend AXPlatformNode* AXPlatformNode::Create(
      AXPlatformNodeDelegate* delegate);

  FRIEND_TEST_ALL_PREFIXES(AXPlatformNodeTest, HypertextOffsetFromEndpoint);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
