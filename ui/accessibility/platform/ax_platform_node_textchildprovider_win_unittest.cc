// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_textchildprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
#include "ui/accessibility/test_ax_tree_update.h"

using Microsoft::WRL::ComPtr;

namespace ui {

class AXPlatformNodeTextChildProviderTest : public AXPlatformNodeWinTest {
 protected:
  // Construct an accessibility tree for testing ITextChildProvider resolution
  // from various positions in the tree. The following tree configuration
  // is constructed:
  //
  // root_________________________________________________
  // |                                                    |
  // nontext_child_of_root______                          text_child_of_root
  // |                          |                         |
  // nontext_child_of_nontext   text_child_of_nontext     text_child_of_text
  //
  // nontext leaf elements are considered as embedded objects and expose a
  // character to allow the text pattern navigation to work with them too.
  // Because of that, a nontext leaf element is treated as a text element.
  void SetUp() override {
    TestAXTreeUpdate update(std::string(R"HTML(
      ++1 kRootWebArea
      ++++2 kGroup
      ++++++3 kGroup
      ++++++4 kStaticText
      ++++++5 kButton
      ++++6 kStaticText
      ++++++7 kInlineTextBox
    )HTML"));

    update.nodes[1].SetName("non text child of root.");
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kAttribute);
    update.nodes[2].SetName("non text child of nontext.");
    update.nodes[2].SetNameFrom(ax::mojom::NameFrom::kAttribute);
    update.nodes[3].SetName("text child of nontext.");
    update.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
    update.nodes[5].SetName("text child of root.");
    update.nodes[5].SetNameFrom(ax::mojom::NameFrom::kContents);
    update.nodes[6].SetName("text child of text.");
    update.nodes[6].SetNameFrom(ax::mojom::NameFrom::kContents);

    Init(update);

    AXNode* root_node = GetRoot();
    AXNode* nontext_child_of_root_node = root_node->children()[0];
    AXNode* text_child_of_root_node = root_node->children()[1];
    AXNode* nontext_child_of_nontext_node =
        nontext_child_of_root_node->children()[0];
    AXNode* text_child_of_nontext_node =
        nontext_child_of_root_node->children()[1];
    AXNode* text_child_of_text_node = text_child_of_root_node->children()[0];

    InitITextChildProvider(root_node, root_provider_raw_,
                           root_text_child_provider_);
    InitITextChildProvider(nontext_child_of_root_node,
                           nontext_child_of_root_provider_raw_,
                           nontext_child_of_root_text_child_provider_);
    InitITextChildProvider(text_child_of_root_node,
                           text_child_of_root_text_provider_raw_,
                           text_child_of_root_text_child_provider_);
    InitITextChildProvider(nontext_child_of_nontext_node,
                           nontext_child_of_nontext_text_provider_raw_,
                           nontext_child_of_nontext_text_child_provider_);
    InitITextChildProvider(text_child_of_nontext_node,
                           text_child_of_nontext_text_provider_raw_,
                           text_child_of_nontext_text_child_provider_);
    InitITextChildProvider(text_child_of_text_node,
                           text_child_of_text_text_provider_raw_,
                           text_child_of_text_text_child_provider_);
    AXPlatformNodeWinTest::SetUp();
  }

  void InitITextChildProvider(
      AXNode* node,
      ComPtr<IRawElementProviderSimple>& raw_element_provider,
      ComPtr<ITextChildProvider>& text_child_provider) {
    raw_element_provider =
        QueryInterfaceFromNode<IRawElementProviderSimple>(node);

    EXPECT_HRESULT_SUCCEEDED(raw_element_provider->GetPatternProvider(
        UIA_TextChildPatternId, &text_child_provider));

    // If the element does not support ITextChildProvider, create one anyways
    // for testing purposes.
    if (!text_child_provider) {
      AXPlatformNodeWin* platform_node =
          (AXPlatformNodeWin*)raw_element_provider.Get();

      ComPtr<ITextChildProvider> new_child_provider =
          AXPlatformNodeTextChildProviderWin::Create(platform_node);
      new_child_provider->QueryInterface(IID_PPV_ARGS(&text_child_provider));
    }
  }

  void SetOwner(AXPlatformNodeWin* owner,
                ITextRangeProvider* destination_range) {
    ComPtr<ITextRangeProvider> destination_provider = destination_range;
    ComPtr<AXPlatformNodeTextRangeProviderWin> destination_provider_interal;

    destination_provider->QueryInterface(
        IID_PPV_ARGS(&destination_provider_interal));
    destination_provider_interal->SetOwnerForTesting(owner);
  }

  ComPtr<IRawElementProviderSimple> root_provider_raw_;
  ComPtr<IRawElementProviderSimple> nontext_child_of_root_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_root_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> nontext_child_of_nontext_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_nontext_text_provider_raw_;
  ComPtr<IRawElementProviderSimple> text_child_of_text_text_provider_raw_;

  ComPtr<ITextChildProvider> root_text_child_provider_;
  ComPtr<ITextChildProvider> nontext_child_of_root_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_root_text_child_provider_;
  ComPtr<ITextChildProvider> nontext_child_of_nontext_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_nontext_text_child_provider_;
  ComPtr<ITextChildProvider> text_child_of_text_text_child_provider_;
};

// ITextChildProvider::TextContainer Tests
//
// For each possible position in the tree verify:
// 1) A text container can/cannot be retrieved if an ancestor does/doesn't
//    support the UIA Text control pattern.
// 2) Any retrieved text container is the nearest ancestor text container.
// 3) A Text control can in fact be retrieved from any retrieved text
//    container.

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromRoot) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      root_text_child_provider_->get_TextContainer(&text_container));
  ASSERT_EQ(nullptr, text_container.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromNontextChildOfRoot) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      nontext_child_of_root_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container.Get());

  EXPECT_EQ(root_provider_raw_.Get(), text_container.Get());

  ComPtr<IUnknown> pattern_provider;
  ComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(UIA_TextPatternId, &pattern_provider);
  ASSERT_NE(nullptr, pattern_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(pattern_provider.As(&text_container_text_provider));
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromTextChildOfRoot) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_root_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container.Get());

  EXPECT_EQ(root_provider_raw_.Get(), text_container.Get());

  ComPtr<IUnknown> pattern_provider;
  ComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(UIA_TextPatternId, &pattern_provider);
  ASSERT_NE(nullptr, pattern_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(pattern_provider.As(&text_container_text_provider));
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromNontextChildOfNontext) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      nontext_child_of_nontext_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container.Get());

  EXPECT_EQ(root_provider_raw_.Get(), text_container.Get());

  ComPtr<IUnknown> pattern_provider;
  ComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(UIA_TextPatternId, &pattern_provider);
  ASSERT_NE(nullptr, pattern_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(pattern_provider.As(&text_container_text_provider));
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromTextChildOfNontext) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_nontext_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container.Get());

  EXPECT_EQ(root_provider_raw_.Get(), text_container.Get());

  ComPtr<IUnknown> pattern_provider;
  ComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(UIA_TextPatternId, &pattern_provider);
  ASSERT_NE(nullptr, pattern_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(pattern_provider.As(&text_container_text_provider));
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextContainerFromTextChildOfText) {
  ComPtr<IRawElementProviderSimple> text_container;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_text_text_child_provider_->get_TextContainer(
          &text_container));
  ASSERT_NE(nullptr, text_container.Get());

  EXPECT_EQ(text_child_of_root_text_provider_raw_.Get(), text_container.Get());

  ComPtr<IUnknown> pattern_provider;
  ComPtr<ITextProvider> text_container_text_provider;
  text_container->GetPatternProvider(UIA_TextPatternId, &pattern_provider);
  ASSERT_NE(nullptr, pattern_provider.Get());
  EXPECT_HRESULT_SUCCEEDED(pattern_provider.As(&text_container_text_provider));
}

// ITextChildProvider::TextRange Tests
//
// For each possible position in the tree verify:
// 1) A text range can/cannot be retrieved if an ancestor does/doesn't
//    support the UIA Text control pattern.
// 2) Any retrieved text range encloses the child element.
TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromRoot) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      root_text_child_provider_->get_TextRange(&text_range_provider));
  EXPECT_EQ(nullptr, text_range_provider.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromNontextChildOfRoot) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      nontext_child_of_root_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider.Get());
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(GetRoot()));
  ASSERT_NE(nullptr, owner);
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  // By design, empty objects, such as the unlabelled image in this case, are
  // placed in their own paragraph for easier screen reader navigation.
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(base::WideToUTF16(text_content.Get()),
            u"non text child of nontext.\ntext child of nontext.\n" +
                kEmbeddedCharacterAsString);

  ComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  EXPECT_EQ(nontext_child_of_root_provider_raw_.Get(), enclosing_element.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromTextChildOfRoot) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_root_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider.Get());
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(GetRoot()));
  ASSERT_NE(nullptr, owner);
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"text child of text."));

  ComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  EXPECT_EQ(text_child_of_root_text_provider_raw_.Get(),
            enclosing_element.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromNontextChildOfNontext) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      nontext_child_of_nontext_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider.Get());
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(GetRoot()));
  ASSERT_NE(nullptr, owner);
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(base::WideToUTF16(text_content.Get()),
            u"non text child of nontext.");

  ComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  EXPECT_EQ(nontext_child_of_nontext_text_provider_raw_.Get(),
            enclosing_element.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromTextChildOfNontext) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_nontext_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider.Get());
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(GetRoot()));
  ASSERT_NE(nullptr, owner);
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"text child of nontext."));

  ComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  EXPECT_EQ(text_child_of_nontext_text_provider_raw_.Get(),
            enclosing_element.Get());
}

TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderTextRangeFromTextChildOfText) {
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(
      text_child_of_text_text_child_provider_->get_TextRange(
          &text_range_provider));
  ASSERT_NE(nullptr, text_range_provider.Get());
  AXPlatformNodeWin* owner =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeFromNode(GetRoot()));
  ASSERT_NE(nullptr, owner);
  SetOwner(owner, text_range_provider.Get());

  base::win::ScopedBstr text_content;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_EQ(0, wcscmp(text_content.Get(), L"text child of text."));

  ComPtr<IRawElementProviderSimple> enclosing_element;
  text_range_provider->GetEnclosingElement(&enclosing_element);
  EXPECT_EQ(text_child_of_root_text_provider_raw_.Get(),
            enclosing_element.Get());
}

// ITextChildProvider Tests - Inactive AX Tree
//
// Test that both ITextChildProvider::GetTextContainer and
// ITextChildProvider::GetTextContainer fail under an inactive AX tree.
TEST_F(AXPlatformNodeTextChildProviderTest,
       ITextChildProviderInactiveAccessibilityTree) {
  DestroyTree();

  // Test that GetTextContainer fails under an inactive tree.
  ComPtr<IRawElementProviderSimple> text_container;
  HRESULT hr = nontext_child_of_root_text_child_provider_->get_TextContainer(
      &text_container);
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), hr);

  // Test that GetTextRange fails under an inactive tree.
  ComPtr<ITextRangeProvider> text_range_provider;
  hr = nontext_child_of_root_text_child_provider_->get_TextRange(
      &text_range_provider);
  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), hr);
}

}  // namespace ui
