// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
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

struct AX_EXPORT AXHypertext {
  AXHypertext();
  AXHypertext(const AXHypertext& other);
  ~AXHypertext();

  // Maps an embedded character offset in |hypertext| to an index in
  // |hyperlinks|.
  std::map<int32_t, int32_t> hyperlink_offset_to_index;

  // The unique id of a AXPlatformNodes for each hyperlink.
  // TODO(nektar): Replace object IDs with child indices if we decide that
  // we are not implementing IA2 hyperlinks for anything other than IA2
  // Hypertext.
  std::vector<int32_t> hyperlinks;

  base::string16 hypertext;
};

class AX_EXPORT AXPlatformNodeBase : public AXPlatformNode {
 public:
  AXPlatformNodeBase();
  ~AXPlatformNodeBase() override;

  virtual void Init(AXPlatformNodeDelegate* delegate);

  // These are simple wrappers to our delegate.
  const AXNodeData& GetData() const;
  gfx::NativeViewAccessible GetFocus();
  gfx::NativeViewAccessible GetParent() const;
  int GetChildCount() const;
  gfx::NativeViewAccessible ChildAtIndex(int index);

  // This needs to be implemented for each platform.
  virtual int GetIndexInParent();

  // AXPlatformNode.
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;

#if defined(OS_MACOSX)
  void AnnounceText(const base::string16& text) override;
#endif

  AXPlatformNodeDelegate* GetDelegate() const override;
  bool IsDescendantOf(AXPlatformNode* ancestor) const override;

  // Helpers.
  AXPlatformNodeBase* GetPreviousSibling();
  AXPlatformNodeBase* GetNextSibling();
  AXPlatformNodeBase* GetFirstChild();
  AXPlatformNodeBase* GetLastChild();
  bool IsDescendant(AXPlatformNodeBase* descendant);

  bool HasBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(ax::mojom::FloatAttribute attr) const;
  float GetFloatAttribute(ax::mojom::FloatAttribute attr) const;
  bool GetFloatAttribute(ax::mojom::FloatAttribute attr, float* value) const;

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const;

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;
  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  bool HasInheritedStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  base::string16 GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetInheritedStringAttribute(ax::mojom::StringAttribute attribute,
                                   std::string* value) const;
  bool GetInheritedString16Attribute(ax::mojom::StringAttribute attribute,
                                     base::string16* value) const;

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;

  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const;

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

  // If inside a table or ARIA grid, returns the zero-based index of the cell.
  // Indices are in row major order and each cell is counted once regardless of
  // its span. Returns base::nullopt if not a cell or if not inside a table.
  base::Optional<int> GetTableCellIndex() const;

  // If inside a table or ARIA grid, returns the physical column number for the
  // current cell. In contrast to logical columns, physical columns always start
  // from 0 and have no gaps in their numbering. Logical columns can be set
  // using aria-colindex. Returns base::nullopt if not a cell or if not inside a
  // table.
  base::Optional<int> GetTableColumn() const;

  // If inside a table or ARIA grid, returns the number of physical columns.
  // Returns base::nullopt if not inside a table.
  base::Optional<int> GetTableColumnCount() const;

  // If inside a table or ARIA grid, returns the number of ARIA columns.
  // Returns base::nullopt if not inside a table.
  base::Optional<int> GetTableAriaColumnCount() const;

  // If inside a table or ARIA grid, returns the number of physical columns that
  // this cell spans. Returns base::nullopt if not a cell or if not inside a
  // table.
  base::Optional<int> GetTableColumnSpan() const;

  // If inside a table or ARIA grid, returns the physical row number for the
  // current cell. In contrast to logical rows, physical rows always start from
  // 0 and have no gaps in their numbering. Logical rows can be set using
  // aria-rowindex. Returns base::nullopt if not a cell or if not inside a
  // table.
  base::Optional<int> GetTableRow() const;

  // If inside a table or ARIA grid, returns the number of physical rows.
  // Returns base::nullopt if not inside a table.
  base::Optional<int> GetTableRowCount() const;

  // If inside a table or ARIA grid, returns the number of ARIA rows.
  // Returns base::nullopt if not inside a table.
  base::Optional<int> GetTableAriaRowCount() const;

  // If inside a table or ARIA grid, returns the number of physical rows that
  // this cell spans. Returns base::nullopt if not a cell or if not inside a
  // table.
  base::Optional<int> GetTableRowSpan() const;

  // Returns true if either a descendant has selection (sel_focus_object_id) or
  // if this node is a simple text element and has text selection attributes.
  bool HasCaret();

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to the
  // platform.
  bool IsChildOfLeaf() const;

  // Returns true if this is a leaf node on this platform, meaning any
  // children should not be exposed to this platform's native accessibility
  // layer. Each platform subclass should implement this itself.
  // The definition of a leaf may vary depending on the platform,
  // but a leaf node should never have children that are focusable or
  // that might send notifications.
  bool IsLeaf();

  bool IsInvisibleOrIgnored() const;

  // Returns true if this node can be scrolled either in the horizontal or the
  // vertical direction.
  bool IsScrollable() const;

  // Returns true if this node can be scrolled in the horizontal direction.
  bool IsHorizontallyScrollable() const;

  // Returns true if this node can be scrolled in the vertical direction.
  bool IsVerticallyScrollable() const;

  // Returns true if this node has role of StaticText, LineBreak, or
  // InlineTextBox
  bool IsTextOnlyObject() const;

  // Returns true if the node is an editable text field.
  bool IsPlainTextField() const;

  bool HasFocus();

  // Returns the text of this node and represent the text of descendant nodes
  // with a special character in place of every embedded object. This represents
  // the concept of text in ATK and IA2 APIs.
  virtual base::string16 GetHypertext() const;

  // Returns the text of this node and all descendant nodes; including text
  // found in embedded objects.
  virtual base::string16 GetInnerText() const;

  virtual base::string16 GetValue() const;

  // Represents a non-static text node in IAccessibleHypertext (and ATK in the
  // future). This character is embedded in the response to
  // IAccessibleText::get_text, indicating the position where a non-static text
  // child object appears.
  static const base::char16 kEmbeddedCharacter;

  // Get a node given its unique id or null in the case that the id is unknown.
  static AXPlatformNode* GetFromUniqueId(int32_t unique_id);

  // Return the number of instances of AXPlatformNodeBase, for leak testing.
  static size_t GetInstanceCountForTesting();

  // This method finds text boundaries in the text used for platform text APIs.
  // Implementations may use side-channel data such as line or word indices to
  // produce appropriate results.
  virtual int FindTextBoundary(AXTextBoundary boundary,
                               int offset,
                               AXTextBoundaryDirection direction,
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

  // Return the nearest text index to a point in screen coordinates for an
  // accessibility node. If the node is not a text only node, the implicit
  // nearest index is zero. Note this will only find the index of text on the
  // input node. The node's subtree will not be searched.
  int NearestTextIndexToPoint(gfx::Point point);

  ui::TextAttributeList ComputeTextAttributes() const;

  //
  // Delegate.  This is a weak reference which owns |this|.
  //
  AXPlatformNodeDelegate* delegate_;

 protected:
  bool IsDocument() const;
  bool IsRichTextField() const;
  bool IsSelectionItemSupported() const;

  // Get the range value text, which might come from aria-valuetext or
  // a floating-point value. This is different from the value string
  // attribute used in input controls such as text boxes and combo boxes.
  base::string16 GetRangeValueText() const;

  // Get the role description from the node data or from the image annotation
  // status.
  base::string16 GetRoleDescription() const;
  base::string16 GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute()
      const;

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
  using PlatformAttributeList = std::vector<base::string16>;
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
  void UpdateComputedHypertext();

  // Selection helper functions.
  // The following functions retrieve the endpoints of the current selection.
  // First they check for a local selection found on the current control, e.g.
  // when querying the selection on a textarea.
  // If not found they retrieve the global selection found on the current frame.
  int GetUnignoredSelectionAnchor();
  int GetUnignoredSelectionFocus();

  // Retrieves the selection offsets in the way required by the IA2 APIs.
  // selection_start and selection_end are -1 when there is no selection active
  // on this object.
  // The greatest of the two offsets is one past the last character of the
  // selection.)
  void GetSelectionOffsets(int* selection_start, int* selection_end);
  void GetSelectionOffsetsFromTree(int* selection_start, int* selection_end);

  // Returns the hyperlink at the given text position, or nullptr if no
  // hyperlink can be found.
  AXPlatformNodeBase* GetHyperlinkFromHypertextOffset(int offset);

  // Functions for retrieving offsets for hyperlinks and hypertext.
  // Return -1 in case of failure.
  int32_t GetHyperlinkIndexFromChild(AXPlatformNodeBase* child);
  int32_t GetHypertextOffsetFromHyperlinkIndex(int32_t hyperlink_index);
  int32_t GetHypertextOffsetFromChild(AXPlatformNodeBase* child);
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

  bool IsSameHypertextCharacter(const AXHypertext& old_hypertext,
                                size_t old_char_index,
                                size_t new_char_index);
  void ComputeHypertextRemovedAndInserted(const AXHypertext& old_hypertext,
                                          size_t* start,
                                          size_t* old_len,
                                          size_t* new_len);

  base::Optional<int> GetPosInSet() const;
  base::Optional<int> GetSetSize() const;

  std::string GetInvalidValue() const;

  AXHypertext hypertext_;

 private:
  // Return true if the index represents a text character.
  bool IsText(const base::string16& text,
              size_t index,
              bool is_indexed_from_end = false);

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
