// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_

#include <wrl/client.h>

#include <string>
#include <tuple>
#include <vector>

#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace ui {
class AX_EXPORT __declspec(uuid("3071e40d-a10d-45ff-a59f-6e8e1138e2c1"))
    AXPlatformNodeTextRangeProviderWin
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITextRangeProvider {
 public:
  BEGIN_COM_MAP(AXPlatformNodeTextRangeProviderWin)
  COM_INTERFACE_ENTRY(ITextRangeProvider)
  COM_INTERFACE_ENTRY(AXPlatformNodeTextRangeProviderWin)
  END_COM_MAP()

  AXPlatformNodeTextRangeProviderWin();
  ~AXPlatformNodeTextRangeProviderWin();

  // Creates an instance of the class.
  static ITextRangeProvider* CreateTextRangeProvider(
      AXPlatformNodeWin* owner,
      AXNodePosition::AXPositionInstance start,
      AXNodePosition::AXPositionInstance end);

  //
  // ITextRangeProvider methods.
  //

  IFACEMETHODIMP Clone(ITextRangeProvider** clone) override;
  IFACEMETHODIMP Compare(ITextRangeProvider* other, BOOL* result) override;
  IFACEMETHODIMP
  CompareEndpoints(TextPatternRangeEndpoint this_endpoint,
                   ITextRangeProvider* other,
                   TextPatternRangeEndpoint other_endpoint,
                   int* result) override;
  IFACEMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override;
  IFACEMETHODIMP
  FindAttribute(TEXTATTRIBUTEID attribute_id,
                VARIANT attribute_val,
                BOOL is_backward,
                ITextRangeProvider** result) override;
  IFACEMETHODIMP
  FindText(BSTR string,
           BOOL backwards,
           BOOL ignore_case,
           ITextRangeProvider** result) override;
  IFACEMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attribute_id,
                                   VARIANT* value) override;
  IFACEMETHODIMP
  GetBoundingRectangles(SAFEARRAY** screen_physical_pixel_rectangles) override;
  IFACEMETHODIMP
  GetEnclosingElement(IRawElementProviderSimple** element) override;
  IFACEMETHODIMP GetText(int max_count, BSTR* text) override;
  IFACEMETHODIMP Move(TextUnit unit, int count, int* units_moved) override;
  IFACEMETHODIMP
  MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                     TextUnit unit,
                     int count,
                     int* units_moved) override;
  IFACEMETHODIMP
  MoveEndpointByRange(TextPatternRangeEndpoint this_endpoint,
                      ITextRangeProvider* other,
                      TextPatternRangeEndpoint other_endpoint) override;
  IFACEMETHODIMP Select() override;
  IFACEMETHODIMP AddToSelection() override;
  IFACEMETHODIMP RemoveFromSelection() override;
  IFACEMETHODIMP ScrollIntoView(BOOL align_to_top) override;
  IFACEMETHODIMP GetChildren(SAFEARRAY** children) override;

 private:
  using AXPositionInstance = AXNodePosition::AXPositionInstance;
  using AXPositionInstanceType = typename AXPositionInstance::element_type;
  using AXNodeRange = AXRange<AXPositionInstanceType>;

  friend class AXPlatformNodeTextRangeProviderTest;
  friend class AXPlatformNodeTextProviderTest;
  friend class AXRangePhysicalPixelRectDelegate;

  static bool AtStartOfLinePredicate(const AXPositionInstance& position);
  static bool AtEndOfLinePredicate(const AXPositionInstance& position);

  static AXPositionInstance GetNextTextBoundaryPosition(
      const AXPositionInstance& position,
      ax::mojom::TextBoundary boundary_type,
      AXBoundaryBehavior boundary_behavior,
      ax::mojom::MoveDirection boundary_direction);

  // Prefer these *Impl methods when functionality is needed internally. We
  // should avoid calling external APIs internally as it will cause the
  // histograms to become innaccurate.
  HRESULT MoveEndpointByUnitImpl(TextPatternRangeEndpoint endpoint,
                                 TextUnit unit,
                                 int count,
                                 int* units_moved);

  IFACEMETHODIMP ExpandToEnclosingUnitImpl(TextUnit unit);

  base::string16 GetString(int max_count,
                           size_t* appended_newlines_count = nullptr);
  AXPlatformNodeWin* owner() const;
  const AXPositionInstance& start() const { return endpoints_.GetStart(); }
  const AXPositionInstance& end() const { return endpoints_.GetEnd(); }
  AXPlatformNodeDelegate* GetDelegate(
      const AXPositionInstanceType* position) const;
  AXPlatformNodeDelegate* GetDelegate(const AXTreeID tree_id,
                                      const AXNodeID node_id) const;

  template <typename AnchorIterator, typename ExpandMatchLambda>
  HRESULT FindAttributeRange(const TEXTATTRIBUTEID text_attribute_id,
                             VARIANT attribute_val,
                             const AnchorIterator first,
                             const AnchorIterator last,
                             ExpandMatchLambda expand_match);

  AXPositionInstance MoveEndpointByCharacter(const AXPositionInstance& endpoint,
                                             const int count,
                                             int* units_moved);
  AXPositionInstance MoveEndpointByWord(const AXPositionInstance& endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByLine(const AXPositionInstance& endpoint,
                                        bool is_start_endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByParagraph(const AXPositionInstance& endpoint,
                                             const bool is_start_endpoint,
                                             const int count,
                                             int* units_moved);
  AXPositionInstance MoveEndpointByPage(const AXPositionInstance& endpoint,
                                        const bool is_start_endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByFormat(const AXPositionInstance& endpoint,
                                          const int count,
                                          int* units_moved);
  AXPositionInstance MoveEndpointByDocument(const AXPositionInstance& endpoint,
                                            const int count,
                                            int* units_moved);

  AXPositionInstance MoveEndpointByUnitHelper(
      const AXPositionInstance& endpoint,
      const ax::mojom::TextBoundary boundary_type,
      const int count,
      int* units_moved);

  // A text range normalization is necessary to prevent a |start_| endpoint to
  // be positioned at the end of an anchor when it can be at the start of the
  // next anchor. After normalization, it is guaranteed that:
  // * both endpoints passed by parameter are always positioned on unignored
  //   anchors;
  // * both endpoints passed by parameter are never between a grapheme cluster;
  // * if the endpoints passed by parameter create a degenerate range, both
  //   endpoints are on the same anchor.
  // Normalization never updates the internal endpoints directly. Instead, it
  // normalizes the endpoints passed by parameter.
  // TODO(vicfei): Make static.
  void NormalizeTextRange(AXPositionInstance& start, AXPositionInstance& end);
  static void NormalizeAsUnignoredPosition(AXPositionInstance& position);
  static void NormalizeAsUnignoredTextRange(AXPositionInstance& start,
                                            AXPositionInstance& end);

  AXPlatformNodeDelegate* GetRootDelegate(const ui::AXTreeID tree_id);
  AXNode* GetSelectionCommonAnchor();
  void RemoveFocusFromPreviousSelectionIfNeeded(
      const AXNodeRange& new_selection);
  AXPlatformNodeWin* GetLowestAccessibleCommonPlatformNode() const;
  bool HasCaretOrSelectionInPlainTextField(
      const AXPositionInstance& position) const;

  void SetStart(AXPositionInstance start);
  void SetEnd(AXPositionInstance end);

  static bool TextAttributeIsArrayType(TEXTATTRIBUTEID attribute_id);
  static bool TextAttributeIsUiaReservedValue(
      const base::win::VariantVector& vector);
  static bool ShouldReleaseTextAttributeAsSafearray(
      TEXTATTRIBUTEID attribute_id,
      const base::win::VariantVector& vector);

  Microsoft::WRL::ComPtr<AXPlatformNodeWin> owner_;

  // Why we can't use a ScopedObserver here:
  // We tried using a ScopedObserver instead of a simple observer in this case,
  // but there appears to be a problem with the lifetime of the referenced
  // AXTreeManager in the ScopedObserver. The AXTreeManager can get deleted
  // before the TextRangeEndpoints does, so when the destructor of the
  // ScopedObserver calls ScopedObserver::RemoveAll on an already deleted
  // AXTreeManager, it crashes.
  class TextRangeEndpoints : public AXTreeObserver {
   public:
    TextRangeEndpoints();
    ~TextRangeEndpoints() override;
    const AXPositionInstance& GetStart() const { return start_; }
    const AXPositionInstance& GetEnd() const { return end_; }
    void SetStart(AXPositionInstance new_start);
    void SetEnd(AXPositionInstance new_end);

    void AddObserver(const AXTreeID tree_id);
    void RemoveObserver(const AXTreeID tree_id);
    void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override;
    void OnTreeManagerWillBeRemoved(AXTreeID previous_tree_id) override;

   private:
    AXPositionInstance start_;
    AXPositionInstance end_;
  };
  TextRangeEndpoints endpoints_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
