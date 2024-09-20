// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/strings/string_split.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_clipping_behavior.h"
#include "ui/accessibility/ax_coordinate_system.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_offscreen_result.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_text_attributes.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class Rect;

}  // namespace gfx

namespace ui {

struct AXActionData;
struct AXNodeData;
struct AXTreeData;
class ChildIterator;

using TextAttribute = std::pair<std::string, std::string>;
using TextAttributeList = std::vector<TextAttribute>;

// A TextAttributeMap is a map between the text offset in UTF-16 characters in
// the node hypertext and the TextAttributeList that starts at that location.
// An empty TextAttributeList signifies a return to the default node
// TextAttributeList.
using TextAttributeMap = std::map<int, TextAttributeList>;

// An object that wants to be accessible should derive from this class.
// AXPlatformNode subclasses use this interface to query all of the information
// about the object in order to implement native accessibility APIs.
//
// Note that AXPlatformNode has support for accessibility trees where some
// of the objects in the tree are not implemented using AXPlatformNode.
// For example, you may have a native window with platform-native widgets
// in it, but in that window you have custom controls that use AXPlatformNode
// to provide accessibility. That's why GetParent, ChildAtIndex, HitTestSync,
// and GetFocus all return a gfx::NativeViewAccessible - so you can return a
// native accessible if necessary, and AXPlatformNode::GetNativeViewAccessible
// otherwise.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeDelegate {
 public:
  using AXPosition = AXNodePosition::AXPositionInstance;
  using SerializedPosition = AXNodePosition::SerializedPosition;
  using AXRange = ui::AXRange<AXPosition::element_type>;

  AXPlatformNodeDelegate();

  AXPlatformNodeDelegate(const AXPlatformNodeDelegate&) = delete;
  AXPlatformNodeDelegate& operator=(const AXPlatformNodeDelegate&) = delete;

  virtual ~AXPlatformNodeDelegate() = default;

  const AXNode* node() const { return node_; }
  AXNode* node() { return node_; }
  void SetNode(AXNode& node);
  void reset_node() { node_ = nullptr; }
  AXTreeManager* GetTreeManager() const;

  // Returns the AXNodeID of the AXNode that this delegate encapsulates (if
  // any), otherwise returns kInvalidAXNodeID
  AXNodeID GetId() const;

  // Get the accessibility data that should be exposed for this node. This data
  // is readonly and comes directly from the accessibility tree's source, e.g.
  // Blink.
  //
  // Virtually all of the information could be obtained from this structure
  // (role, state, name, cursor position, etc.) However, please prefer using
  // specific accessor methods, such as `GetStringAttribute` or
  // `GetTableCellRowIndex`, instead of directly accessing this structure,
  // because any attributes that could automatically be computed in the browser
  // process would also be returned. The browser process would try to correct
  // missing or erroneous information too.
  virtual const AXNodeData& GetData() const;

  // Get some extra data about the accessibility tree that contains this node.
  virtual const AXTreeData& GetTreeData() const;

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or std::u16string, for convenience.

  ax::mojom::Role GetRole() const;
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
  // TODO(accessibility): Deprecate. This version is likely less efficient than
  // calling has followed by get since it creates a copy of the string rather
  // than returning a const ref.
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;
  std::u16string GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  // TODO(accessibility): Deprecate in favor of using a has check if distinction
  // between empty string and missing value is important.
  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            std::u16string* value) const;
  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  std::u16string GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;
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
  // TODO(accessibility): Deprecate this method in favor of separate calls to
  // Has and Get. This version forces a copy of the list, which is inefficient
  // in cases where a const reference would suffice.
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
  bool HasDefaultActionVerb() const;
  std::vector<ax::mojom::Action> GetSupportedActions() const;
  bool HasTextStyle(ax::mojom::TextStyle text_style) const;
  ax::mojom::NameFrom GetNameFrom() const;
  ax::mojom::DescriptionFrom GetDescriptionFrom() const;

  // Returns the text of this node and all descendant nodes; including text
  // found in embedded objects.
  //
  // Only text displayed on screen is included. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node,
  // e.g. aria-label and HTML title, is not returned.
  virtual std::u16string GetTextContentUTF16() const;
  virtual int GetTextContentLengthUTF16() const;

  // Returns the value of a control such as a text field, a slider, a <select>
  // element, a date picker or an ARIA combo box. In order to minimize
  // cross-process communication between the renderer and the browser, may
  // compute the value from the control's inner text in the case of a text
  // field.
  virtual std::u16string GetValueForControl() const;

  // See `AXNode::GetUnignoredSelection`.
  virtual const AXSelection GetUnignoredSelection() const;

  // Creates a text position rooted at this object if it's a leaf node, or a
  // tree position otherwise.
  virtual AXNodePosition::AXPositionInstance CreatePositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const;

  // Creates a text position rooted at this object.
  virtual AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const;

  // Get the accessibility node for the NSWindow which contains this node. This
  // method is only meaningful on macOS.
  virtual gfx::NativeViewAccessible GetNSWindow();

  // Get the node for this delegate, which may be an `AXPlatformNode` or it may
  // be an object directly exposed by the platform's OS.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible();

  // Get the parent of the node, which may be an `AXPlatformNode` or it may
  // be an object directly exposed by the platform's OS.
  //
  // Note that for accessibility trees that have ignored nodes, this method
  // returns only unignored parents. All ignored nodes are recursively removed.
  // (An ignored node means that the node should not be exposed to platform
  // APIs: See `IsIgnored`.)
  virtual gfx::NativeViewAccessible GetParent() const;

  // Get the index in the parent's list of unignored children. Returns `nullopt`
  // if an unignored parent is unavailable, e.g. if this node is at the root of
  // all accessibility trees.
  virtual std::optional<size_t> GetIndexInParent() const;

  // Get the number of children of this node.
  //
  // Note that for accessibility trees that have ignored nodes, this method
  // should return the number of unignored children. All ignored nodes are
  // recursively removed from the children count. (An ignored node means that
  // the node should not be exposed to platform APIs: See
  // `IsIgnored`.)
  virtual size_t GetChildCount() const;

  // Get a child of a node given a 0-based index.
  //
  // Note that for accessibility trees that have ignored nodes, this method
  // returns only unignored children. All ignored nodes are recursively removed.
  // (An ignored node means that the node should not be exposed to platform
  // APIs: See `IsIgnored`.)
  virtual gfx::NativeViewAccessible ChildAtIndex(size_t index) const;

  // Returns true if this node is within a modal dialog.
  virtual bool HasModalDialog() const;

  // Gets the first unignored child of this node, or nullptr if no children
  // exist.
  virtual gfx::NativeViewAccessible GetFirstChild() const;

  // Gets the last unignored child of this node, or nullptr if no children
  // exist.
  virtual gfx::NativeViewAccessible GetLastChild() const;

  // Gets the next unignored sibling of this node, or nullptr if no such node
  // exists.
  virtual gfx::NativeViewAccessible GetNextSibling() const;

  // Gets the previous sibling of this node, or nullptr if no such node
  // exists.
  virtual gfx::NativeViewAccessible GetPreviousSibling() const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to any
  // platform's accessibility APIs.
  virtual bool IsChildOfLeaf() const;

  // Returns true if this node is either an atomic text field , or one of its
  // ancestors is. An atomic text field does not expose its internal
  // implementation to assistive software, appearing as a single leaf node in
  // the accessibility tree. It includes <input>, <textarea> and Views-based
  // text fields.
  bool IsDescendantOfAtomicTextField() const;

  // Returns true if this object is at the root of what most accessibility APIs
  // consider to be a document, such as the root of a webpage, an iframe, or a
  // PDF.
  virtual bool IsPlatformDocument() const;

  // Returns true if this node is focused.
  virtual bool IsFocused() const;

  // Returns true if this node is focusable or is a likely active descendant.
  virtual bool IsFocusable() const;

  // Returns true if this node is ignored and should be hidden from the
  // accessibility tree. Methods that are used to navigate the accessibility
  // tree, such as "ChildAtIndex", "GetParent", and "GetChildCount", among
  // others, also skip ignored nodes. This does not impact the node's
  // descendants.
  //
  // Only relevant for accessibility trees that support ignored nodes.)
  virtual bool IsIgnored() const;

  // Returns true if this is a leaf node, meaning all its
  // children should not be exposed to any platform's native accessibility
  // layer.
  virtual bool IsLeaf() const;

  // Returns true if this node is invisible or ignored. (Only relevant for
  // accessibility trees that support ignored nodes.)
  virtual bool IsInvisibleOrIgnored() const;

  // Returns true if this is a top-level browser window that doesn't have a
  // parent accessible node, or its parent is the application accessible node on
  // platforms that have one.
  virtual bool IsToplevelBrowserWindow();

  // If this object is exposed to the platform's accessibility layer, returns
  // this object. Otherwise, returns the platform leaf or lowest unignored
  // ancestor under which this object is found.
  //
  // (An ignored node means that the node should not be exposed to platform
  // APIs: See `IsIgnored`.)
  virtual gfx::NativeViewAccessible GetLowestPlatformAncestor() const;

  // If this node is within an editable region, returns the node that is at the
  // root of that editable region, otherwise returns nullptr. In accessibility,
  // an editable region is synonymous to a node with the kTextField role, or a
  // contenteditable without the role, (see `AXNodeData::IsTextField()`).
  virtual gfx::NativeViewAccessible GetTextFieldAncestor() const;

  // If this node is within a container (or widget) that supports either single
  // or multiple selection, returns the node that represents the container.
  virtual gfx::NativeViewAccessible GetSelectionContainer() const;

  // If within a table, returns the node representing the table.
  virtual gfx::NativeViewAccessible GetTableAncestor() const;

  virtual std::unique_ptr<ChildIterator> ChildrenBegin() const;
  virtual std::unique_ptr<ChildIterator> ChildrenEnd() const;

  // Returns the accessible name for this node. This could either be derived
  // from visible text, such as the node's contents or an associated label, or
  // be manually set by the node's owner, e.g. via an aria-label in HTML.
  const std::string& GetName() const;

  // Returns the accessible description for the node.
  // An accessible description gives more information about the node in
  // contrast to the accessible name which is a shorter label for the node.
  const std::string& GetDescription() const;

  // Returns the text of this node and represents the text of descendant nodes
  // with a special character in place of every embedded object. This represents
  // the concept of text in ATK and IA2 APIs.
  virtual std::u16string GetHypertext() const;

  // Temporary accessor method until hypertext is fully migrated to `AXNode`
  // from `AXPlatformNodeBase`.
  // TODO(nektar): Remove this once selection handling is fully migrated to
  // `AXNode`.
  const std::map<int, int>& GetHypertextOffsetToHyperlinkChildIndex() const;

  // Set the selection in the hypertext of this node. Depending on the
  // implementation, this may mean the new selection will span multiple nodes.
  virtual bool SetHypertextSelection(int start_offset, int end_offset);

  // Compute the text attributes map for the node associated with this
  // delegate, given a set of default text attributes that apply to the entire
  // node. A text attribute map associates a list of text attributes with a
  // given hypertext offset in this node.
  virtual TextAttributeMap ComputeTextAttributeMap(
      const TextAttributeList& default_attributes) const;

  virtual std::wstring ComputeListItemNameFromContent() const;

  // Get the inherited font family name for text attributes. We need this
  // because inheritance works differently between the different delegate
  // implementations.
  std::string GetInheritedFontFamilyName() const;

  // Returns the bounds of this node in the coordinate system indicated. If the
  // clipping behavior is set to clipped, clipping is applied. If an offscreen
  // result address is provided, it will be populated depending on whether the
  // returned bounding box is onscreen or offscreen.
  virtual gfx::Rect GetBoundsRect(
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Derivative utils for AXPlatformNodeDelegate::GetBoundsRect
  gfx::Rect GetClippedScreenBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetUnclippedScreenBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetClippedRootFrameBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetUnclippedRootFrameBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetClippedFrameBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetUnclippedFrameBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Return the bounds of the text range given by text offsets relative to
  // GetHypertext in the coordinate system indicated. If the clipping behavior
  // is set to clipped, clipping is applied. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  virtual gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Return the bounds of the text range given by text offsets relative to
  // GetInnerText in the coordinate system indicated. If the clipping behavior
  // is set to clipped, clipping is applied. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  virtual gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Do a *synchronous* hit test of the given location in global screen physical
  // pixel coordinates, and the node within this node's subtree (inclusive)
  // that's hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  virtual gfx::NativeViewAccessible HitTestSync(
      int screen_physical_pixel_x,
      int screen_physical_pixel_y) const;

  // Returns the node within this node's subtree (inclusive) that currently has
  // focus, or return nullptr if this subtree is not connected to the top
  // document through its ancestry chain.
  virtual gfx::NativeViewAccessible GetFocus() const;

  // Get whether this node is offscreen.
  virtual bool IsOffscreen() const;

  // Get whether this node is a minimized window.
  virtual bool IsMinimized() const;

  // See AXNode::IsText().
  bool IsText() const;

  // Get whether this node is in the web content vs. the Views layer.
  virtual bool IsWebContent() const;

  // Get whether this node can be marked as read-only.
  virtual bool IsReadOnlySupported() const;

  // Get whether this node is marked as read-only or is disabled.
  virtual bool IsReadOnlyOrDisabled() const;

  // Returns true if the IA2 node is selected.
  bool IsIA2NodeSelected() const;

  // Returns true if the UIA node is selected.
  // For radio buttons, returns true if the node's 'checked' state is true.
  bool IsUIANodeSelected() const;

  // See `AXNode::HasVisibleCaretOrSelection`.
  virtual bool HasVisibleCaretOrSelection() const;

  // Get a node in the platform AX tree given the ID of its
  // corresponding node in the Blink AX tree.
  virtual AXPlatformNode* GetFromNodeID(int32_t id);

  // Get a node from a different tree using a tree ID and node ID.
  // Note that this is only guaranteed to work if the other tree is of the
  // same type, i.e. it won't work between web and views or vice-versa.
  virtual AXPlatformNode* GetFromTreeIDAndNodeID(const AXTreeID& ax_tree_id,
                                                 int32_t id);

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a target nodes for which this delegate's node has that relationship
  // attribute or NULL if there is no such relationship.
  virtual AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr);

  // Given a node ID attribute (one where IsNodeIdIntListAttribute is true),
  // return a vector of all target nodes for which this delegate's node has that
  // relationship attribute.
  virtual std::vector<AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr);

  // Given an attribute which could be used to establish a reverse relationship
  // between this node and a set of other nodes (AKA the source nodes), return
  // the list of source nodes if any.
  virtual std::vector<AXPlatformNode*> GetSourceNodesForReverseRelations(
      ax::mojom::IntAttribute attr);

  // Given an attribute which could be used to establish a reverse relationship
  // between this node and a set of other nodes (AKA the source nodes), return
  // the list of source nodes if any.
  virtual std::vector<AXPlatformNode*> GetSourceNodesForReverseRelations(
      ax::mojom::IntListAttribute attr);

  // Given a potential target, check if this node can point to `target` with a
  // relation.
  bool IsValidRelationTarget(AXPlatformNode* target) const;

  // Returns the string representation of the unique ID assigned by the author,
  // otherwise returns an empty string if none has been assigned. The author ID
  // must be persistent across any instance of the application, regardless of
  // locale. The author ID should be unique among sibling accessibility nodes
  // and is best if unique across the application, however, not meeting this
  // requirement is non-fatal.
  virtual std::u16string GetAuthorUniqueId() const;

  // Returns an ID that is unique for this node across all accessibility trees
  // in the current application or desktop.
  virtual AXPlatformNodeId GetUniqueId() const;

  // Return a vector of all the descendants of this delegate's node. This method
  // is only meaningful for Windows UIA.
  virtual const std::vector<gfx::NativeViewAccessible>
  GetUIADirectChildrenInRange(AXPlatformNodeDelegate* start,
                              AXPlatformNodeDelegate* end);

  // Return a string representing the language code.
  //
  // For web content, this will consider the language declared in the DOM, and
  // may eventually attempt to automatically detect the language from the text.
  //
  // This language code will be BCP 47.
  //
  // Returns empty string if no appropriate language was found or if this node
  // uses the default interface language.
  std::string GetLanguage() const;

  //
  // Tables. All of these should be called on a node that has a table-like
  // role, otherwise they return nullopt.
  // Methods with "Aria" in their name work with author-privided aria
  // values, or computed values derived from the author-specified ones.
  // Please note that aria has 1-based rows and columns.
  //
  bool IsTable() const;
  virtual std::optional<int> GetTableColCount() const;
  virtual std::optional<int> GetTableRowCount() const;
  virtual std::optional<int> GetTableAriaColCount() const;
  virtual std::optional<int> GetTableAriaRowCount() const;
  virtual std::optional<int> GetTableCellCount() const;
  virtual std::vector<int32_t> GetColHeaderNodeIds() const;
  virtual std::vector<int32_t> GetColHeaderNodeIds(int col_index) const;
  virtual std::vector<int32_t> GetRowHeaderNodeIds() const;
  virtual std::vector<int32_t> GetRowHeaderNodeIds(int row_index) const;
  virtual AXPlatformNode* GetTableCaption() const;

  //
  // Nodes with a table row-like role.
  //
  virtual bool IsTableRow() const;
  virtual std::optional<int> GetTableRowRowIndex() const;

  //
  // Nodes with a table cell-like role.
  //
  virtual bool IsTableCellOrHeader() const;
  virtual std::optional<int> GetTableCellIndex() const;
  virtual std::optional<int> GetTableCellColIndex() const;
  virtual std::optional<int> GetTableCellRowIndex() const;
  virtual std::optional<int> GetTableCellColSpan() const;
  virtual std::optional<int> GetTableCellRowSpan() const;
  virtual std::optional<int> GetTableCellAriaColIndex() const;
  virtual std::optional<int> GetTableCellAriaRowIndex() const;
  virtual std::optional<int32_t> GetCellId(int row_index, int col_index) const;
  virtual std::optional<int32_t> GetCellIdAriaCoords(int aria_row_index,
                                                     int aria_col_index) const;
  virtual std::optional<int32_t> CellIndexToId(int cell_index) const;

  // Returns true if this node is a cell or a row/column header in an ARIA grid
  // or treegrid.
  virtual bool IsCellOrHeaderOfAriaGrid() const;

  // See `AXNode::IsRootWebAreaForPresentationalIframe()`.
  virtual bool IsRootWebAreaForPresentationalIframe() const;

  // Ordered-set-like and item-like nodes.
  virtual bool IsOrderedSetItem() const;
  virtual bool IsOrderedSet() const;
  virtual std::optional<int> GetPosInSet() const;
  virtual std::optional<int> GetSetSize() const;

  // Computed colors, taking blending into account.
  virtual SkColor GetColor() const;
  virtual SkColor GetBackgroundColor() const;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  virtual gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent();

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  virtual bool AccessibilityPerformAction(const AXActionData& data);

  //
  // Localized strings.
  //
  virtual std::u16string GetLocalizedRoleDescriptionForUnlabeledImage() const;
  virtual std::u16string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const;
  virtual std::u16string GetLocalizedStringForLandmarkType() const;
  virtual std::u16string GetLocalizedStringForRoleDescription() const;
  virtual std::u16string GetStyleNameAttributeAsLocalizedString() const;

  virtual void SetIsPrimaryWebContentsForWindow();
  virtual bool IsPrimaryWebContentsForWindow() const;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be false if not in testing mode.
  virtual bool ShouldIgnoreHoveredStateForTesting();

  // Creates a string representation of this delegate's data.
  std::string ToString() const { return GetData().ToString(); }

  // Returns a string representation of the subtree of delegates rooted at this
  // delegate.
  std::string SubtreeToString() { return SubtreeToStringHelper(0u); }

  friend std::ostream& operator<<(std::ostream& stream,
                                  const AXPlatformNodeDelegate& delegate) {
    return stream << delegate.ToString();
  }

 protected:
  explicit AXPlatformNodeDelegate(AXNode* node);

  virtual std::string SubtreeToStringHelper(size_t level);

  virtual void NotifyAccessibilityApiUsage() const {}

  AXPlatformNodeDelegate* GetParentDelegate() const;

  // Given a set of Blink node IDs, get their respective platform nodes and
  // return only those that are valid targets for a relation.
  std::vector<AXPlatformNode*> GetNodesFromRelationIdSet(
      const std::set<AXNodeID>& ids);

 private:
  // The underlying node. This could change during the lifetime of this object
  // if this object has been reparented, i.e. moved to another part of the tree.
  // In this case, a new `AXNode` would be created by `AXTree`, which would
  // however reuse the same `AXNodeID`.
  //
  // Weak, `AXTree` owns this.
  raw_ptr<AXNode> node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
