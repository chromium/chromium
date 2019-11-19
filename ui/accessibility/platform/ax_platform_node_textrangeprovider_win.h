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
#include "ui/accessibility/ax_text_boundary.h"
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

  STDMETHODIMP Clone(ITextRangeProvider** clone) override;
  STDMETHODIMP Compare(ITextRangeProvider* other, BOOL* result) override;
  STDMETHODIMP
  CompareEndpoints(TextPatternRangeEndpoint this_endpoint,
                   ITextRangeProvider* other,
                   TextPatternRangeEndpoint other_endpoint,
                   int* result) override;
  STDMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override;
  STDMETHODIMP
  FindAttribute(TEXTATTRIBUTEID attribute_id,
                VARIANT attribute_val,
                BOOL is_backward,
                ITextRangeProvider** result) override;
  STDMETHODIMP
  FindText(BSTR string,
           BOOL backwards,
           BOOL ignore_case,
           ITextRangeProvider** result) override;
  STDMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attribute_id,
                                 VARIANT* value) override;
  STDMETHODIMP
  GetBoundingRectangles(SAFEARRAY** rectangles) override;
  STDMETHODIMP
  GetEnclosingElement(IRawElementProviderSimple** element) override;
  STDMETHODIMP GetText(int max_count, BSTR* text) override;
  STDMETHODIMP Move(TextUnit unit, int count, int* units_moved) override;
  STDMETHODIMP
  MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                     TextUnit unit,
                     int count,
                     int* units_moved) override;
  STDMETHODIMP
  MoveEndpointByRange(TextPatternRangeEndpoint this_endpoint,
                      ITextRangeProvider* other,
                      TextPatternRangeEndpoint other_endpoint) override;
  STDMETHODIMP Select() override;
  STDMETHODIMP AddToSelection() override;
  STDMETHODIMP RemoveFromSelection() override;
  STDMETHODIMP ScrollIntoView(BOOL align_to_top) override;
  STDMETHODIMP GetChildren(SAFEARRAY** children) override;

 private:
  using AXPositionInstance = AXNodePosition::AXPositionInstance;
  using AXPositionInstanceType = typename AXPositionInstance::element_type;
  using AXNodeRange = AXRange<AXPositionInstanceType>;

  friend class AXPlatformNodeTextRangeProviderTest;
  friend class AXPlatformNodeTextProviderTest;
  friend class AXRangeScreenRectDelegateImpl;

  static bool AtStartOfLinePredicate(const AXPositionInstance& position);
  static bool AtEndOfLinePredicate(const AXPositionInstance& position);

  base::string16 GetString(int max_count,
                           size_t* appended_newlines_count = nullptr);
  AXPlatformNodeWin* owner() const;
  AXPlatformNodeDelegate* GetDelegate(
      const AXPositionInstanceType* position) const;
  AXPlatformNodeDelegate* GetDelegate(const AXTreeID tree_id,
                                      const AXNode::AXID node_id) const;

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
                                        bool endpoint_is_start,
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
      const AXTextBoundary boundary_type,
      const int count,
      int* units_moved);

  void NormalizeAsUnignoredTextRange();
  void NormalizeTextRange();

  Microsoft::WRL::ComPtr<AXPlatformNodeWin> owner_;
  AXPositionInstance start_;
  AXPositionInstance end_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
