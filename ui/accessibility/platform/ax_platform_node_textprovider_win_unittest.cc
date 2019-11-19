// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/base/win/accessibility_misc_utils.h"

using Microsoft::WRL::ComPtr;

namespace ui {

// Helper macros for UIAutomation HRESULT expectations
#define EXPECT_UIA_INVALIDOPERATION(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_INVALIDOPERATION), (expr))
#define EXPECT_INVALIDARG(expr) \
  EXPECT_EQ(static_cast<HRESULT>(E_INVALIDARG), (expr))

class AXPlatformNodeTextProviderTest : public ui::AXPlatformNodeWinTest {
 public:
  ui::AXPlatformNodeWin* GetOwner(
      const AXPlatformNodeTextProviderWin* text_provider) {
    return text_provider->owner_.Get();
  }
  const AXNodePosition::AXPositionInstance& GetStart(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->start_;
  }
  const AXNodePosition::AXPositionInstance& GetEnd(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->end_;
  }
};

class MockIRawElementProviderSimple
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IRawElementProviderSimple {
 public:
  BEGIN_COM_MAP(MockIRawElementProviderSimple)
  COM_INTERFACE_ENTRY(IRawElementProviderSimple)
  END_COM_MAP()

  MockIRawElementProviderSimple() {}
  ~MockIRawElementProviderSimple() {}

  static HRESULT CreateMockIRawElementProviderSimple(
      IRawElementProviderSimple** provider) {
    CComObject<MockIRawElementProviderSimple>* raw_element_provider = nullptr;
    HRESULT hr = CComObject<MockIRawElementProviderSimple>::CreateInstance(
        &raw_element_provider);
    if (SUCCEEDED(hr)) {
      *provider = raw_element_provider;
    }

    return hr;
  }

  //
  // IRawElementProviderSimple methods.
  //
  IFACEMETHODIMP GetPatternProvider(PATTERNID pattern_id,
                                    IUnknown** result) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetPropertyValue(PROPERTYID property_id,
                                  VARIANT* result) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP
  get_ProviderOptions(enum ProviderOptions* ret) override { return E_NOTIMPL; }

  IFACEMETHODIMP
  get_HostRawElementProvider(IRawElementProviderSimple** provider) override {
    return E_NOTIMPL;
  }
};

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderRangeFromChild) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData empty_text_data;
  empty_text_data.id = 3;
  empty_text_data.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);
  root_data.child_ids.push_back(3);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  update.nodes.push_back(empty_text_data);

  Init(update);

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTree(tree_.get());
  AXNode* text_node = root_node->children()[0];
  AXNode* empty_text_node = root_node->children()[1];

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<IRawElementProviderSimple> text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_node);
  ComPtr<IRawElementProviderSimple> empty_text_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(empty_text_node);

  // Call RangeFromChild on the root with the text child passed in.
  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->RangeFromChild(text_node_raw.Get(), &text_range_provider));

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(static_cast<BSTR>(text_content), L"some text"));

  // Now test that the reverse relation doesn't return a valid
  // ITextRangeProvider, and instead returns E_INVALIDARG.
  EXPECT_HRESULT_SUCCEEDED(
      text_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_INVALIDARG(
      text_provider->RangeFromChild(root_node_raw.Get(), &text_range_provider));

  // Now test that a child with no text returns a degenerate range.
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_HRESULT_SUCCEEDED(text_provider->RangeFromChild(
      empty_text_node_raw.Get(), &text_range_provider));

  base::win::ScopedBstr empty_text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, empty_text_content.Receive()));
  EXPECT_EQ(0, wcscmp(static_cast<BSTR>(empty_text_content), L""));

  // Test that passing in an object from a different instance of
  // IRawElementProviderSimple than that of the valid text provider
  // returns UIA_E_INVALIDOPERATION.
  ComPtr<IRawElementProviderSimple> other_root_node_raw;
  MockIRawElementProviderSimple::CreateMockIRawElementProviderSimple(
      &other_root_node_raw);

  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  EXPECT_UIA_INVALIDOPERATION(text_provider->RangeFromChild(
      other_root_node_raw.Get(), &text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest, TestNearestTextIndexToPoint) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kInlineTextBox;
  text_data.SetName("text");
  // spacing: "t-e-x---t-"
  text_data.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                                {2, 4, 8, 10});

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kStaticText;
  root_data.relative_bounds.bounds = gfx::RectF(1, 1, 2, 2);
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  AXNode* root_node = GetRootNode();
  AXNodePosition::SetTree(tree_.get());
  AXNode* text_node = root_node->children()[0];

  struct NearestTextIndexTestData {
    AXNode* node;
    struct point_offset_expected_index_pair {
      int point_offset_x;
      int expected_index;
    };
    std::vector<point_offset_expected_index_pair> test_data;
  };
  NearestTextIndexTestData nodes[] = {
      {text_node,
       {{0, 0}, {2, 0}, {3, 1}, {4, 1}, {5, 2}, {8, 2}, {9, 3}, {10, 3}}},
      {root_node,
       {{0, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {8, 0}, {9, 0}, {10, 0}}}};
  for (auto data : nodes) {
    ComPtr<IRawElementProviderSimple> element_provider =
        QueryInterfaceFromNode<IRawElementProviderSimple>(data.node);
    ComPtr<ITextProvider> text_provider;
    EXPECT_HRESULT_SUCCEEDED(element_provider->GetPatternProvider(
        UIA_TextPatternId, &text_provider));
    // get internal implementation to access helper for testing
    ComPtr<AXPlatformNodeTextProviderWin> platform_text_provider;
    EXPECT_HRESULT_SUCCEEDED(
        text_provider->QueryInterface(IID_PPV_ARGS(&platform_text_provider)));

    ComPtr<AXPlatformNodeWin> platform_node;
    EXPECT_HRESULT_SUCCEEDED(
        element_provider->QueryInterface(IID_PPV_ARGS(&platform_node)));

    for (auto pair : data.test_data) {
      EXPECT_EQ(pair.expected_index, platform_node->NearestTextIndexToPoint(
                                         gfx::Point(pair.point_offset_x, 0)));
    }
  }
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderDocumentRange) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderDocumentRangeNested) {
  ui::AXNodeData text_data;
  text_data.id = 3;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData paragraph_data;
  paragraph_data.id = 2;
  paragraph_data.role = ax::mojom::Role::kParagraph;
  paragraph_data.child_ids.push_back(3);

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, paragraph_data, text_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderSupportedSelection) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  SupportedTextSelection text_selection_mode;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_SupportedTextSelection(&text_selection_mode));
  EXPECT_EQ(text_selection_mode, SupportedTextSelection_Single);
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderGetSelection) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData textbox_data;
  textbox_data.id = 3;
  textbox_data.role = ax::mojom::Role::kInlineTextBox;
  textbox_data.SetName("textbox text");
  textbox_data.AddState(ax::mojom::State::kEditable);

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);
  root_data.child_ids.push_back(3);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  update.nodes.push_back(textbox_data);
  Init(update);

  AXNodePosition::SetTree(tree_.get());

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));

  base::win::ScopedSafearray selections;
  root_text_provider->GetSelection(selections.Receive());
  ASSERT_EQ(nullptr, selections.Get());

  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));

  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  AXTreeData& selected_tree_data =
      const_cast<AXTreeData&>(owner->GetDelegate()->GetTreeData());
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 0;
  selected_tree_data.sel_focus_offset = 4;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  LONG ubound;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selections.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  LONG lbound;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selections.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);

  LONG index = 0;
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content, L"some"));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Verify that start and end are appropriately swapped when sel_anchor_offset
  // is greater than sel_focus_offset
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 4;
  selected_tree_data.sel_focus_offset = 0;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selections.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selections.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content, L"some"));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Verify that text ranges at an insertion point returns a degenerate (empty)
  // text range via textbox with sel_anchor_offset equal to sel_focus_offset
  selected_tree_data.sel_focus_object_id = 3;
  selected_tree_data.sel_anchor_object_id = 3;
  selected_tree_data.sel_anchor_offset = 1;
  selected_tree_data.sel_focus_offset = 1;

  AXNode* text_edit_node = GetRootNode()->children()[1];

  ComPtr<IRawElementProviderSimple> text_edit_com =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text_edit_node);

  ComPtr<ITextProvider> text_edit_provider;
  EXPECT_HRESULT_SUCCEEDED(text_edit_com->GetPatternProvider(
      UIA_TextPatternId, &text_edit_provider));

  selections.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      text_edit_provider->GetSelection(selections.Receive()));
  EXPECT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selections.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selections.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);

  ComPtr<ITextRangeProvider> text_edit_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      SafeArrayGetElement(selections.Get(), &index,
                          static_cast<void**>(&text_edit_range_provider)));

  EXPECT_HRESULT_SUCCEEDED(
      text_edit_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0U, SysStringLen(text_content));
  text_content.Reset();
  selections.Reset();
  text_edit_range_provider.Reset();

  // Verify that we don't fill the SAFEARRAY when there is no selection and the
  // node is not editable.
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 1;
  selected_tree_data.sel_focus_offset = 1;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_EQ(nullptr, selections.Get());

  // Now delete the tree (which will delete the associated elements) and verify
  // that UIA_E_ELEMENTNOTAVAILABLE is returned when calling GetSelection on
  // a dead element
  tree_.reset();

  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            text_edit_provider->GetSelection(selections.Receive()));

  AXNodePosition::SetTree(nullptr);
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderGetActiveComposition) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  Init(update);

  AXNodePosition::SetTree(tree_.get());

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));

  ComPtr<ITextEditProvider> root_text_edit_provider;
  EXPECT_HRESULT_SUCCEEDED(root_node->GetPatternProvider(
      UIA_TextEditPatternId, &root_text_edit_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  root_text_edit_provider->GetActiveComposition(&text_range_provider);
  ASSERT_EQ(nullptr, text_range_provider);

  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = 1;
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  owner->GetDelegate()->AccessibilityPerformAction(action_data);
  const base::string16 active_composition_text = L"a";
  owner->OnActiveComposition(gfx::Range(0, 1), active_composition_text, false);

  root_text_edit_provider->GetActiveComposition(&text_range_provider);
  ASSERT_NE(nullptr, text_range_provider);
  ComPtr<AXPlatformNodeTextRangeProviderWin> actual_range;
  AXNodePosition::AXPositionInstance expected_start =
      owner->GetDelegate()->CreateTextPositionAt(0);
  AXNodePosition::AXPositionInstance expected_end =
      owner->GetDelegate()->CreateTextPositionAt(1);
  text_range_provider->QueryInterface(IID_PPV_ARGS(&actual_range));
  EXPECT_EQ(*GetStart(actual_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(actual_range.Get()), *expected_end);
}

TEST_F(AXPlatformNodeTextProviderTest, TestITextProviderGetConversionTarget) {
  ui::AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.SetName("Document");
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.child_ids.push_back(2);

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);
  update.nodes.push_back(text_data);
  Init(update);

  AXNodePosition::SetTree(tree_.get());

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));

  ComPtr<ITextEditProvider> root_text_edit_provider;
  EXPECT_HRESULT_SUCCEEDED(root_node->GetPatternProvider(
      UIA_TextEditPatternId, &root_text_edit_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  root_text_edit_provider->GetConversionTarget(&text_range_provider);
  ASSERT_EQ(nullptr, text_range_provider);

  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = 1;
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  owner->GetDelegate()->AccessibilityPerformAction(action_data);
  const base::string16 active_composition_text = L"a";
  owner->OnActiveComposition(gfx::Range(0, 1), active_composition_text, false);

  root_text_edit_provider->GetConversionTarget(&text_range_provider);
  ASSERT_NE(nullptr, text_range_provider);
  ComPtr<AXPlatformNodeTextRangeProviderWin> actual_range;
  AXNodePosition::AXPositionInstance expected_start =
      owner->GetDelegate()->CreateTextPositionAt(0);
  AXNodePosition::AXPositionInstance expected_end =
      owner->GetDelegate()->CreateTextPositionAt(1);
  text_range_provider->QueryInterface(IID_PPV_ARGS(&actual_range));
  EXPECT_EQ(*GetStart(actual_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(actual_range.Get()), *expected_end);
}

}  // namespace ui
