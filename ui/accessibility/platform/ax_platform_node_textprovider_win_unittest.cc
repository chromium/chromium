// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include <vector>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

using Microsoft::WRL::ComPtr;

namespace ui {

// Helper macros for UIAutomation HRESULT expectations
#define EXPECT_UIA_INVALIDOPERATION(expr) \
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_INVALIDOPERATION), (expr))
#define EXPECT_INVALIDARG(expr) \
  EXPECT_EQ(static_cast<HRESULT>(E_INVALIDARG), (expr))

class AXPlatformNodeTextProviderTest : public AXPlatformNodeWinTest {
 public:
  AXPlatformNodeTextProviderTest() = default;
  ~AXPlatformNodeTextProviderTest() override = default;
  AXPlatformNodeTextProviderTest(const AXPlatformNodeTextProviderTest&) =
      delete;
  AXPlatformNodeTextProviderTest& operator=(
      const AXPlatformNodeTextProviderTest&) = delete;

 protected:
  void SetOwner(AXPlatformNodeWin* owner,
                ITextRangeProvider* destination_range) {
    ComPtr<ITextRangeProvider> destination_provider = destination_range;
    ComPtr<AXPlatformNodeTextRangeProviderWin> destination_provider_interal;

    destination_provider->QueryInterface(
        IID_PPV_ARGS(&destination_provider_interal));
    destination_provider_interal->SetOwnerForTesting(owner);
  }
  AXPlatformNodeWin* GetOwner(
      const AXPlatformNodeTextProviderWin* text_provider) {
    return text_provider->owner_.Get();
  }
  const AXNodePosition::AXPositionInstance& GetStart(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->start();
  }
  const AXNodePosition::AXPositionInstance& GetEnd(
      const AXPlatformNodeTextRangeProviderWin* text_range) {
    return text_range->end();
  }
};

TEST_F(AXPlatformNodeTextProviderTest, CreateDegenerateRangeFromStart) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kLink
    ++++++3 kStaticText name="some-text"
    ++++++4 kStaticText name="more-text"
  )HTML"));

  Init(update);
  AXNode* root_node = GetRoot();
  AXNode* link_node = root_node->children()[0];
  AXNode* text2_node = link_node->children()[1];
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(root_node));
  DCHECK(owner);

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<IRawElementProviderSimple> link_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(link_node);
  ComPtr<IRawElementProviderSimple> text2_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text2_node);

  ComPtr<AXPlatformNodeWin> root_platform_node;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->QueryInterface(IID_PPV_ARGS(&root_platform_node)));
  ComPtr<AXPlatformNodeWin> link_platform_node;
  EXPECT_HRESULT_SUCCEEDED(
      link_node_raw->QueryInterface(IID_PPV_ARGS(&link_platform_node)));
  ComPtr<AXPlatformNodeWin> text2_platform_node;
  EXPECT_HRESULT_SUCCEEDED(
      text2_node_raw->QueryInterface(IID_PPV_ARGS(&text2_platform_node)));

  // Degenerate range created on root node should be:
  // <>some-textmore-text
  ComPtr<ITextRangeProvider> text_range_provider;
  AXPlatformNodeTextProviderWin::CreateDegenerateRangeAtStart(
      root_platform_node.Get(), &text_range_provider);
  SetOwner(owner, text_range_provider.Get());
  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L""));

  ComPtr<AXPlatformNodeTextRangeProviderWin> actual_range;
  text_range_provider->QueryInterface(IID_PPV_ARGS(&actual_range));
  AXNodePosition::AXPositionInstance expected_start, expected_end;
  expected_start = root_platform_node->GetDelegate()->CreateTextPositionAt(0);
  expected_end = expected_start->Clone();
  EXPECT_EQ(*GetStart(actual_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(actual_range.Get()), *expected_end);
  text_content.Release();

  // Degenerate range created on link node should be:
  // <>some textmore text
  AXPlatformNodeTextProviderWin::CreateDegenerateRangeAtStart(
      link_platform_node.Get(), &text_range_provider);
  SetOwner(owner, text_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L""));
  text_range_provider->QueryInterface(IID_PPV_ARGS(&actual_range));
  EXPECT_EQ(*GetStart(actual_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(actual_range.Get()), *expected_end);
  text_content.Release();

  // Degenerate range created on more text node should be:
  // some text<>more text
  AXPlatformNodeTextProviderWin::CreateDegenerateRangeAtStart(
      text2_platform_node.Get(), &text_range_provider);
  SetOwner(owner, text_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L""));
  text_range_provider->QueryInterface(IID_PPV_ARGS(&actual_range));
  expected_start = text2_platform_node->GetDelegate()->CreateTextPositionAt(0);
  expected_end = expected_start->Clone();
  EXPECT_EQ(*GetStart(actual_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(actual_range.Get()), *expected_end);
  text_content.Release();
}

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderRangeFromChild) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="some-text"
    ++++3 kStaticText
  )HTML"));

  Init(update);

  AXNode* root_node = GetRoot();
  AXNode* text_node = root_node->children()[0];
  AXNode* empty_text_node = root_node->children()[1];
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(root_node));
  DCHECK(owner);

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
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"some-text"));

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
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr empty_text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, empty_text_content.Receive()));
  EXPECT_EQ(0, wcscmp(empty_text_content.Get(), L""));

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

TEST_F(AXPlatformNodeTextProviderTest,
       ITextProviderRangeFromChildMultipleChildren) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kDialog
    ++++++3 kStaticText name="Dialog-label."
    ++++++4 kStaticText name="Dialog-description."
    ++++++5 kButton
    ++++++++6 kImage
    ++++++++7 kStaticText name="ok."
    ++++++8 kStaticText name="Some-more-detail-about-dialog."
  )HTML"));

  Init(update);

  AXNode* root_node = GetRoot();
  AXNode* dialog_node = root_node->children()[0];
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(root_node));
  DCHECK(owner);

  ComPtr<IRawElementProviderSimple> root_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(root_node);
  ComPtr<IRawElementProviderSimple> dialog_node_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(dialog_node);

  // Call RangeFromChild on the root with the dialog child passed in.
  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(text_provider->RangeFromChild(dialog_node_raw.Get(),
                                                         &text_range_provider));
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  // TODO(javiercon): change test back to spaces when we update the parser.
  EXPECT_EQ(base::WideToUTF16(text_content.Get()),
            u"Dialog-label.Dialog-description.\n" + kEmbeddedCharacterAsString +
                u"\nok.Some-more-detail-" + u"about-dialog.");

  // Check the reverse relationship that GetEnclosingElement on the text range
  // gives back the dialog.
  ComPtr<IRawElementProviderSimple> enclosing_element;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetEnclosingElement(&enclosing_element));
  EXPECT_EQ(enclosing_element.Get(), dialog_node_raw.Get());
}

TEST_F(AXPlatformNodeTextProviderTest, NearestTextIndexToPoint) {
  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kInlineTextBox;
  text_data.SetName("text");
  // spacing: "t-e-x---t-"
  text_data.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                                {2, 4, 8, 10});

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.relative_bounds.bounds = gfx::RectF(1, 1, 2, 2);
  root_data.child_ids.push_back(2);

  Init(root_data, text_data);

  AXNode* root_node = GetRoot();
  AXNode* text_node = root_node->children()[0];

  struct NearestTextIndexTestData {
    raw_ptr<AXNode> node;
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

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderDocumentRange) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="some-text"
  )HTML"));

  Init(update);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest,
       ITextProviderDocumentRangeTrailingIgnored) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kGenericContainer
    ++++++3 kStaticText name="Hello"
    ++++4 kGenericContainer
    ++++++5 kGenericContainer
    ++++++++6 kStaticText name="3.14"
    ++++7 kGenericContainer state=kIgnored
    ++++++8 kGenericContainer state=kIgnored
    ++++++++9 kStaticText state=kIgnored name="ignored"
  )HTML"));

  update.nodes[1].AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds, {3});
  update.nodes[3].AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds, {5});

  Init(update);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));

  ComPtr<AXPlatformNodeTextRangeProviderWin> text_range;
  text_range_provider->QueryInterface(IID_PPV_ARGS(&text_range));

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));
  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());

  AXNodePosition::AXPositionInstance expected_start =
      owner->GetDelegate()->CreateTextPositionAt(0)->AsLeafTextPosition();
  AXNodePosition::AXPositionInstance expected_end =
      owner->GetDelegate()
          ->CreateTextPositionAt(0)
          ->CreatePositionAtEndOfAnchor()
          ->AsLeafTextPosition();
  EXPECT_EQ(*GetStart(text_range.Get()), *expected_start);
  EXPECT_EQ(*GetEnd(text_range.Get()), *expected_end);
}

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderDocumentRangeNested) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kParagraph
    ++++++3 kStaticText name="some-text"
  )HTML"));

  Init(update);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &text_provider));

  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->get_DocumentRange(&text_range_provider));
}

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderSupportedSelection) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="some-text"
  )HTML"));

  Init(update);

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

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderGetSelection) {
  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  AXNodeData textbox_data;
  textbox_data.id = 3;
  textbox_data.role = ax::mojom::Role::kInlineTextBox;
  textbox_data.SetName("textbox text");
  textbox_data.AddState(ax::mojom::State::kEditable);

  AXNodeData nonatomic_textfield_data;
  nonatomic_textfield_data.id = 4;
  nonatomic_textfield_data.role = ax::mojom::Role::kTextField;
  nonatomic_textfield_data.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);
  nonatomic_textfield_data.child_ids = {5};

  AXNodeData text_child_data;
  text_child_data.id = 5;
  text_child_data.role = ax::mojom::Role::kStaticText;
  text_child_data.SetName("text");

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.SetName("Document");
  root_data.child_ids = {2, 3, 4};

  AXTreeUpdate update;
  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes = {root_data, text_data, textbox_data, nonatomic_textfield_data,
                  text_child_data};
  Init(update);

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
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"some"));
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
  SetOwner(owner, text_range_provider.Get());

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"some"));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Verify that text ranges at an insertion point returns a degenerate (empty)
  // text range via textbox with sel_anchor_offset equal to sel_focus_offset
  selected_tree_data.sel_focus_object_id = 3;
  selected_tree_data.sel_anchor_object_id = 3;
  selected_tree_data.sel_anchor_offset = 1;
  selected_tree_data.sel_focus_offset = 1;

  AXNode* text_edit_node = GetRoot()->children()[1];

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
  SetOwner(owner, text_edit_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_edit_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0U, text_content.Length());
  text_content.Reset();
  selections.Reset();
  text_edit_range_provider.Reset();

  // Verify selections that span multiple nodes
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_focus_offset = 0;
  selected_tree_data.sel_anchor_object_id = 3;
  selected_tree_data.sel_anchor_offset = 12;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selections.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selections.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  SetOwner(owner, text_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"some texttextbox text"));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Verify SAFEARRAY value for degenerate selection.
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 1;
  selected_tree_data.sel_focus_offset = 1;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetUBound(selections.Get(), 1, &ubound));
  EXPECT_EQ(0, ubound);
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetLBound(selections.Get(), 1, &lbound));
  EXPECT_EQ(0, lbound);

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  SetOwner(owner, text_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L""));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Verify that the selection set on a non-atomic text field returns the
  // correct selection. Because the anchor/focus is a non-leaf element, the
  // offset passed here is a child offset and not a text offset. This means that
  // the accessible selection received should include the entire leaf text child
  // and not only the first character of that non-atomic text field.
  selected_tree_data.sel_anchor_object_id = 4;
  selected_tree_data.sel_anchor_offset = 0;
  selected_tree_data.sel_focus_object_id = 4;
  selected_tree_data.sel_focus_offset = 1;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  SetOwner(owner, text_range_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"text"));
  text_content.Reset();
  selections.Reset();
  text_range_provider.Reset();

  // Now delete the tree (which will delete the associated elements) and verify
  // that UIA_E_ELEMENTNOTAVAILABLE is returned when calling GetSelection on
  // a dead element
  DestroyTree();

  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            text_edit_provider->GetSelection(selections.Receive()));
}

TEST_F(AXPlatformNodeTextProviderTest, ITextRangeProviderGetSelectionRefCount) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="hello"
  )HTML"));

  Init(update);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));

  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));

  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  AXTreeData& selected_tree_data =
      const_cast<AXTreeData&>(owner->GetDelegate()->GetTreeData());
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 0;
  selected_tree_data.sel_focus_offset = 5;

  base::win::ScopedSafearray selections;
  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  LONG index = 0;
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));

  // Validate that there was only one reference to the `text_range_provider`.
  ASSERT_EQ(1U, text_range_provider->Release());

  // This is needed to avoid calling SafeArrayDestroy from SafeArray's dtor when
  // exiting the scope, which would crash trying to release the already
  // destroyed `text_range_provider`.
  selections.Release();
}

TEST_F(AXPlatformNodeTextProviderTest,
       TestRemoveTextInvalidatingPositionForComparison) {
  TestAXTreeUpdate initial_state(std::string(R"HTML(
    ++1 kRootWebArea
    ++++2 kStaticText name="aaa"
    ++++++3 kInlineTextBox name="aaa"
  )HTML"));

  Init(initial_state);

  ComPtr<IRawElementProviderSimple> root_node =
      GetRootIRawElementProviderSimple();

  ComPtr<ITextProvider> root_text_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_node->GetPatternProvider(UIA_TextPatternId, &root_text_provider));

  ComPtr<AXPlatformNodeTextProviderWin> root_platform_node;
  root_text_provider->QueryInterface(IID_PPV_ARGS(&root_platform_node));

  base::win::ScopedSafearray selections;
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  AXTreeData& selected_tree_data =
      const_cast<AXTreeData&>(owner->GetDelegate()->GetTreeData());
  selected_tree_data.sel_focus_object_id = 2;
  selected_tree_data.sel_anchor_object_id = 2;
  selected_tree_data.sel_anchor_offset = 0;
  selected_tree_data.sel_focus_offset = 3;

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  LONG index = 0;
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"aaa"));

  selections.Reset();
  text_range_provider.Reset();
  text_content.Reset();

  AXTreeUpdate update;
  update.nodes.resize(2);
  update.nodes[0] = initial_state.nodes[1];
  update.nodes[0].SetName("aa");
  update.nodes[1] = initial_state.nodes[2];
  update.nodes[1].SetName("aa");
  ASSERT_TRUE(GetTree()->Unserialize(update));

  root_text_provider->GetSelection(selections.Receive());
  ASSERT_NE(nullptr, selections.Get());

  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      selections.Get(), &index, static_cast<void**>(&text_range_provider)));
  SetOwner(owner, text_range_provider.Get());

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"aa"));
}

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderGetActiveComposition) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="some-text"
  )HTML"));
  Init(update);

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

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = 1;
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  owner->GetDelegate()->AccessibilityPerformAction(action_data);
  const std::u16string active_composition_text = u"a";
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

TEST_F(AXPlatformNodeTextProviderTest, ITextProviderGetConversionTarget) {
  TestAXTreeUpdate update(std::string(R"HTML(
    ++1 kRootWebArea name="Document"
    ++++2 kStaticText name="some-text"
  )HTML"));

  Init(update);

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

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = 1;
  AXPlatformNodeWin* owner = GetOwner(root_platform_node.Get());
  owner->GetDelegate()->AccessibilityPerformAction(action_data);
  const std::u16string active_composition_text = u"a";
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
