// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_win.h"

#include <objbase.h>

#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"
#include "ui/base/win/atl_module.h"

namespace ui {

#define EXPECT_IA2_TEXT_AT_OFFSET(provider, index, text_boundary, expected_hr, \
                                  start, end, text)                            \
  {                                                                            \
    LONG actual_start;                                                         \
    LONG actual_end;                                                           \
    base::win::ScopedBstr actual_text;                                         \
    EXPECT_EQ(expected_hr,                                                     \
              provider->get_textAtOffset(index, text_boundary, &actual_start,  \
                                         &actual_end, actual_text.Receive())); \
    EXPECT_EQ(start, actual_start);                                            \
    EXPECT_EQ(end, actual_end);                                                \
    EXPECT_STREQ(text, actual_text.Get());                                     \
  }

#define EXPECT_IA2_TEXT_BEFORE_OFFSET(provider, index, text_boundary, \
                                      expected_hr, start, end, text)  \
  {                                                                   \
    LONG actual_start;                                                \
    LONG actual_end;                                                  \
    base::win::ScopedBstr actual_text;                                \
    EXPECT_EQ(expected_hr, provider->get_textBeforeOffset(            \
                               index, text_boundary, &actual_start,   \
                               &actual_end, actual_text.Receive()));  \
    EXPECT_EQ(start, actual_start);                                   \
    EXPECT_EQ(end, actual_end);                                       \
    EXPECT_STREQ(text, actual_text.Get());                            \
  }

#define EXPECT_IA2_TEXT_AFTER_OFFSET(provider, index, text_boundary, \
                                     expected_hr, start, end, text)  \
  {                                                                  \
    LONG actual_start;                                               \
    LONG actual_end;                                                 \
    base::win::ScopedBstr actual_text;                               \
    EXPECT_EQ(expected_hr, provider->get_textAfterOffset(            \
                               index, text_boundary, &actual_start,  \
                               &actual_end, actual_text.Receive())); \
    EXPECT_EQ(start, actual_start);                                  \
    EXPECT_EQ(end, actual_end);                                      \
    EXPECT_STREQ(text, actual_text.Get());                           \
  }

#define EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants,                \
                                                expected_descendants)       \
  {                                                                         \
    size_t count = descendants.size();                                      \
    EXPECT_EQ(count, expected_descendants.size());                          \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(                                                            \
          AXPlatformNode::FromNativeViewAccessible(descendants[i])          \
              ->GetDelegate()                                               \
              ->GetData()                                                   \
              .ToString(),                                                  \
          AXPlatformNode::FromNativeViewAccessible(expected_descendants[i]) \
              ->GetDelegate()                                               \
              ->GetData()                                                   \
              .ToString());                                                 \
    }                                                                       \
  }

// BrowserAccessibilityWinTest ------------------------------------------------

class BrowserAccessibilityWinTest : public ::testing::Test {
 public:
  BrowserAccessibilityWinTest();

  BrowserAccessibilityWinTest(const BrowserAccessibilityWinTest&) = delete;
  BrowserAccessibilityWinTest& operator=(const BrowserAccessibilityWinTest&) =
      delete;

  ~BrowserAccessibilityWinTest() override;

 protected:
  std::unique_ptr<TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  TestAXNodeIdDelegate node_id_delegate_;

 private:
  void SetUp() override;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

BrowserAccessibilityWinTest::BrowserAccessibilityWinTest() {}

BrowserAccessibilityWinTest::~BrowserAccessibilityWinTest() {}

void BrowserAccessibilityWinTest::SetUp() {
  win::CreateATLModuleIfNeeded();
  test_browser_accessibility_delegate_ =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
}

// Actual tests ---------------------------------------------------------------

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityWinTest, TestNoLeaks) {
  // Create AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;
  button.SetName("Button");

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.SetName("Checkbox");

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("Document");
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, button, checkbox), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  // Delete the manager and test that all 3 instances are deleted.
  manager.reset();

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdateForTesting(root, button, checkbox), node_id_delegate_,
      test_browser_accessibility_delegate_.get()));
  IAccessible* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
          ->GetCOM();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = root_accessible->get_accChild(childid_self, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  base::win::ScopedVariant one(1);
  hr = root_accessible->get_accChild(one, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  manager.reset();

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  child1_iaccessible->Release();
}

TEST_F(BrowserAccessibilityWinTest, TestChildrenChange) {
  // Create AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AXNodeData text;
  text.id = 2;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("old text");

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("Document");
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, text), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedVariant one(1);
  Microsoft::WRL::ComPtr<IDispatch> text_dispatch;
  HRESULT hr = ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
                   ->GetCOM()
                   ->get_accChild(one, &text_dispatch);
  ASSERT_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<IAccessible> text_accessible;
  hr = text_dispatch.As(&text_accessible);
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"old text", std::wstring(name.Get()));
  name.Reset();

  text_dispatch.Reset();
  text_accessible.Reset();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  AXNodeData text2;
  text2.id = 2;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("new text");
  AXUpdatesAndEvents event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(text2);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
           ->GetCOM()
           ->get_accChild(one, &text_dispatch);
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.As(&text_accessible);
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"new text", std::wstring(name.Get()));

  text_dispatch.Reset();
  text_accessible.Reset();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestChildrenChangeNoLeaks) {
  // Create AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AXNodeData div;
  div.id = 2;
  div.role = ax::mojom::Role::kGroup;

  AXNodeData text3;
  text3.id = 3;
  text3.role = ax::mojom::Role::kStaticText;

  AXNodeData text4;
  text4.id = 4;
  text4.role = ax::mojom::Role::kStaticText;

  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility and ensure that exactly 4 instances were
  // created. Note that the manager takes ownership of the factory.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, div, text3, text4),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.child_ids.clear();
  AXUpdatesAndEvents event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(root);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestTextBoundaries) {
  //
  // +-1 root
  //   +-2 text_field
  //     +-3 text_container
  //       +-4 static_text1 "One two three."
  //     |   +-5 inline_box1 "One two three."
  //       +-6 line_break1 "\n"
  //       +-7 static_text2 "Four five six."
  //     |   +-8 inline_box2 "Four five six."
  //       +-9 line_break2 "\n" kIsLineBreakingObject
  //       +-10 static_text3 "Seven eight nine."
  //         +-11 inline_box3 "Seven eight nine."
  //
  std::string line1 = "One two three.";
  std::string line2 = "Four five six.";
  std::string line3 = "Seven eight nine.";
  std::string text_value = line1 + '\n' + line2 + '\n' + line3;

  AXNodeData root;
  root.id = 1;
  AXNodeData text_field;
  text_field.id = 2;
  AXNodeData text_container;
  text_container.id = 3;
  AXNodeData static_text1;
  static_text1.id = 4;
  AXNodeData inline_box1;
  inline_box1.id = 5;
  AXNodeData line_break1;
  line_break1.id = 6;
  AXNodeData static_text2;
  static_text2.id = 7;
  AXNodeData inline_box2;
  inline_box2.id = 8;
  AXNodeData line_break2;
  line_break2.id = 9;
  AXNodeData static_text3;
  static_text3.id = 10;
  AXNodeData inline_box3;
  inline_box3.id = 11;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {text_field.id};

  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  text_field.SetValue(text_value);
  text_field.AddIntListAttribute(ax::mojom::IntListAttribute::kLineStarts,
                                 {15});
  text_field.child_ids = {text_container.id};

  text_container.role = ax::mojom::Role::kGenericContainer;
  text_container.child_ids = {static_text1.id, line_break1.id, static_text2.id,
                              line_break2.id, static_text3.id};

  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.AddState(ax::mojom::State::kEditable);
  static_text1.SetName(line1);
  static_text1.child_ids = {inline_box1.id};

  inline_box1.role = ax::mojom::Role::kInlineTextBox;
  inline_box1.AddState(ax::mojom::State::kEditable);
  inline_box1.SetName(line1);
  inline_box1.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  {0, 4, 8});

  line_break1.role = ax::mojom::Role::kLineBreak;
  line_break1.AddState(ax::mojom::State::kEditable);
  line_break1.SetName("\n");

  inline_box1.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                              line_break1.id);
  line_break1.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box1.id);

  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddState(ax::mojom::State::kEditable);
  static_text2.SetName(line2);
  static_text2.child_ids = {inline_box2.id};

  inline_box2.role = ax::mojom::Role::kInlineTextBox;
  inline_box2.AddState(ax::mojom::State::kEditable);
  inline_box2.SetName(line2);
  inline_box2.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  {0, 5, 10});

  line_break2.role = ax::mojom::Role::kLineBreak;
  line_break2.AddState(ax::mojom::State::kEditable);
  line_break2.SetName("\n");
  line_break2.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);

  inline_box2.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                              line_break2.id);
  line_break2.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box2.id);

  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.AddState(ax::mojom::State::kEditable);
  static_text3.SetName(line3);
  static_text3.child_ids = {inline_box3.id};

  inline_box3.role = ax::mojom::Role::kInlineTextBox;
  inline_box3.AddState(ax::mojom::State::kEditable);
  inline_box3.SetName(line3);
  inline_box3.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  {0, 6, 12});

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, text_field, text_container,
                                     static_text1, inline_box1, line_break1,
                                     static_text2, inline_box2, line_break2,
                                     static_text3, inline_box3),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_obj);
  ASSERT_EQ(1U, root_obj->PlatformChildCount());

  BrowserAccessibilityComWin* text_field_obj =
      ToBrowserAccessibilityComWin(root_obj->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_field_obj);

  LONG text_len;
  EXPECT_EQ(S_OK, text_field_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, text_field_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text_value, base::WideToUTF8(std::wstring(text.Get())));
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(L"One ", text.Get());
  text.Reset();

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/1, /*end=*/2,
                            /*text=*/L"n");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len, IA2_TEXT_BOUNDARY_WORD,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_WORD,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/4,
                            /*text=*/L"One ");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 6, IA2_TEXT_BOUNDARY_WORD,
                            /*expected_hr=*/S_OK, /*start=*/4, /*end=*/8,
                            /*text=*/L"two ");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len - 1,
                            IA2_TEXT_BOUNDARY_WORD,
                            /*expected_hr=*/S_OK, /*start=*/42, /*end=*/47,
                            /*text=*/L"nine.");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_LINE,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/15,
                            /*text=*/L"One two three.\n");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len, IA2_TEXT_BOUNDARY_LINE,
                            /*expected_hr=*/S_OK, /*start=*/30, /*end=*/47,
                            /*text=*/L"Seven eight nine.");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_PARAGRAPH,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/15,
                            /*text=*/L"One two three.\n");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 29, IA2_TEXT_BOUNDARY_PARAGRAPH,
                            /*expected_hr=*/S_OK, /*start=*/15, /*end=*/30,
                            /*text=*/L"Four five six.\n");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, 30, IA2_TEXT_BOUNDARY_PARAGRAPH,
                            /*expected_hr=*/S_OK, /*start=*/30, /*end=*/47,
                            /*text=*/L"Seven eight nine.");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len - 1,
                            IA2_TEXT_BOUNDARY_PARAGRAPH,
                            /*expected_hr=*/S_OK, /*start=*/30, /*end=*/47,
                            /*text=*/L"Seven eight nine.");

  EXPECT_IA2_TEXT_AT_OFFSET(text_field_obj, text_len,
                            IA2_TEXT_BOUNDARY_PARAGRAPH,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 0, IA2_TEXT_BOUNDARY_CHAR,
                                /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                                /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_CHAR,
                                /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
                                /*text=*/L"O");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(
      text_field_obj, text_len, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/E_INVALIDARG, /*start=*/0, /*end=*/0,
      /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(
      text_field_obj, text_len, IA2_TEXT_BOUNDARY_WORD,
      /*expected_hr=*/E_INVALIDARG, /*start=*/0, /*end=*/0,
      /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_WORD,
                                /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                                /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 4, IA2_TEXT_BOUNDARY_WORD,
                                /*expected_hr=*/S_OK, /*start=*/0, /*end=*/4,
                                /*text=*/L"One ");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 6, IA2_TEXT_BOUNDARY_WORD,
                                /*expected_hr=*/S_OK, /*start=*/0, /*end=*/4,
                                /*text=*/L"One ");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, text_len - 1,
                                IA2_TEXT_BOUNDARY_WORD,
                                /*expected_hr=*/S_OK, /*start=*/36, /*end=*/42,
                                /*text=*/L"eight ");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 0, IA2_TEXT_BOUNDARY_LINE,
                                /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                                /*text=*/nullptr);

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, text_len - 1,
                                IA2_TEXT_BOUNDARY_LINE,
                                /*expected_hr=*/S_OK, /*start=*/15, /*end=*/30,
                                /*text=*/L"Four five six.\n");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 18, IA2_TEXT_BOUNDARY_PARAGRAPH,
                                /*expected_hr=*/S_OK, /*start=*/0, /*end=*/15,
                                /*text=*/L"One two three.\n");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 29, IA2_TEXT_BOUNDARY_PARAGRAPH,
                                /*expected_hr=*/S_OK, /*start=*/0, /*end=*/15,
                                /*text=*/L"One two three.\n");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, 30, IA2_TEXT_BOUNDARY_PARAGRAPH,
                                /*expected_hr=*/S_OK, /*start=*/15, /*end=*/30,
                                /*text=*/L"Four five six.\n");

  EXPECT_IA2_TEXT_BEFORE_OFFSET(text_field_obj, text_len - 1,
                                IA2_TEXT_BOUNDARY_PARAGRAPH,
                                /*expected_hr=*/S_OK, /*start=*/15, /*end=*/30,
                                /*text=*/L"Four five six.\n");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 0, IA2_TEXT_BOUNDARY_CHAR,
                               /*expected_hr=*/S_OK, /*start=*/1, /*end=*/2,
                               /*text=*/L"n");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_CHAR,
                               /*expected_hr=*/S_OK, /*start=*/2, /*end=*/3,
                               /*text=*/L"e");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, text_len, IA2_TEXT_BOUNDARY_CHAR,
                               /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                               /*end=*/0,
                               /*text=*/nullptr);

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 1, IA2_TEXT_BOUNDARY_WORD,
                               /*expected_hr=*/S_OK, /*start=*/4, /*end=*/8,
                               /*text=*/L"two ");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 6, IA2_TEXT_BOUNDARY_WORD,
                               /*expected_hr=*/S_OK, /*start=*/8, /*end=*/15,
                               /*text=*/L"three.\n");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, text_len, IA2_TEXT_BOUNDARY_WORD,
                               /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                               /*end=*/0,
                               /*text=*/nullptr);

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 0, IA2_TEXT_BOUNDARY_LINE,
                               /*expected_hr=*/S_OK, /*start=*/15, /*end=*/30,
                               /*text=*/L"Four five six.\n");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, text_len - 1,
                               IA2_TEXT_BOUNDARY_LINE,
                               /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                               /*text=*/nullptr);

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 18, IA2_TEXT_BOUNDARY_PARAGRAPH,
                               /*expected_hr=*/S_OK, /*start=*/30, /*end=*/47,
                               /*text=*/L"Seven eight nine.");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, 29, IA2_TEXT_BOUNDARY_PARAGRAPH,
                               /*expected_hr=*/S_OK, /*start=*/30, /*end=*/47,
                               /*text=*/L"Seven eight nine.");

  EXPECT_IA2_TEXT_AFTER_OFFSET(text_field_obj, text_len - 1,
                               IA2_TEXT_BOUNDARY_PARAGRAPH,
                               /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                               /*text=*/nullptr);

  EXPECT_EQ(S_OK, text_field_obj->get_text(0, IA2_TEXT_OFFSET_LENGTH,
                                           text.Receive()));
  EXPECT_EQ(text_value, base::WideToUTF8(std::wstring(text.Get())));

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestSimpleHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const LONG text_name_len = text1_name.length() + text2_name.length();

  AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(text1_name);

  AXNodeData text2;
  text2.id = 12;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(text2_name);

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(text1.id);
  root.child_ids.push_back(text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, text1, text2), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityComWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
          ->GetCOM();

  LONG text_len;
  EXPECT_EQ(S_OK, root_obj->get_nCharacters(&text_len));
  EXPECT_EQ(text_name_len, text_len);

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, root_obj->get_text(0, text_name_len, text.Receive()));
  EXPECT_EQ(text1_name + text2_name,
            base::WideToUTF8(std::wstring(text.Get())));

  LONG hyperlink_count;
  EXPECT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(0, hyperlink_count);

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, &hyperlink));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(0, &hyperlink));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(text_name_len, &hyperlink));
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlink(text_name_len + 1, &hyperlink));

  LONG hyperlink_index;
  EXPECT_EQ(S_FALSE, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  // Invalid arguments should not be modified.
  hyperlink_index = -2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(text_name_len, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(-1, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(text_name_len + 1, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestComplexHypertext) {
  const std::u16string text1_name = u"One two three.";
  const std::u16string combo_box_name = u"City:";
  const std::u16string combo_box_value = u"Happyland";
  const std::u16string text2_name = u" Four five six.";
  const std::u16string check_box_name = u"I agree";
  const std::u16string check_box_value = u"Checked";
  const std::u16string button_text_name = u"Red";
  const std::u16string link_text_name = u"Blue";
  // Each control (combo / check box, button and link) will be represented by an
  // embedded object character.
  const std::u16string embed(1, AXPlatformNodeBase::kEmbeddedCharacter);
  const std::u16string root_hypertext =
      text1_name + embed + text2_name + embed + embed + embed;
  const LONG root_hypertext_len = root_hypertext.length();

  AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(base::UTF16ToUTF8(text1_name));

  AXNodeData combo_box;
  combo_box.id = 12;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  combo_box.SetName(base::UTF16ToUTF8(combo_box_name));
  combo_box.SetValue(base::UTF16ToUTF8(combo_box_value));

  AXNodeData text2;
  text2.id = 13;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(base::UTF16ToUTF8(text2_name));

  AXNodeData check_box;
  check_box.id = 14;
  check_box.role = ax::mojom::Role::kCheckBox;
  check_box.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box.SetName(base::UTF16ToUTF8(check_box_name));
  // ARIA checkbox where the name is derived from its inner text.
  check_box.SetNameFrom(ax::mojom::NameFrom::kContents);
  check_box.SetValue(base::UTF16ToUTF8(check_box_value));

  AXNodeData button, button_text;
  button.id = 15;
  button_text.id = 17;
  button.role = ax::mojom::Role::kButton;
  button.SetName(base::UTF16ToUTF8(button_text_name));
  button.SetNameFrom(ax::mojom::NameFrom::kContents);
  // A single text child with the same name should be hidden from accessibility
  // to prevent double speaking.
  button_text.role = ax::mojom::Role::kStaticText;
  button_text.SetName(base::UTF16ToUTF8(button_text_name));
  button.child_ids.push_back(button_text.id);

  AXNodeData link, link_text;
  link.id = 16;
  link_text.id = 18;
  link.role = ax::mojom::Role::kLink;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.SetName(base::UTF16ToUTF8(link_text_name));
  link.child_ids.push_back(link_text.id);

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(text1.id);
  root.child_ids.push_back(combo_box.id);
  root.child_ids.push_back(text2.id);
  root.child_ids.push_back(check_box.id);
  root.child_ids.push_back(button.id);
  root.child_ids.push_back(link.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, text1, combo_box, text2, check_box,
                                     button, button_text, link, link_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityComWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
          ->GetCOM();

  LONG text_len;
  EXPECT_EQ(S_OK, root_obj->get_nCharacters(&text_len));
  EXPECT_EQ(root_hypertext_len, text_len);

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, root_obj->get_text(0, root_hypertext_len, text.Receive()));
  EXPECT_EQ(root_hypertext, base::WideToUTF16(text.Get()));
  text.Reset();

  LONG hyperlink_count;
  EXPECT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(4, hyperlink_count);

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  Microsoft::WRL::ComPtr<IAccessibleText> hypertext;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, &hyperlink));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(4, &hyperlink));

  // Get the text of the combo box.
  // It should be its value.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(0, &hyperlink));
  EXPECT_EQ(S_OK, hyperlink.As(&hypertext));
  EXPECT_EQ(S_OK,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_EQ(combo_box_value, base::WideToUTF16(text.Get()));
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the check box.
  // It should be its name.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, &hyperlink));
  EXPECT_EQ(S_OK, hyperlink.As(&hypertext));
  EXPECT_EQ(S_OK,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_EQ(check_box_name, base::WideToUTF16(text.Get()));
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the button.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(2, &hyperlink));
  EXPECT_EQ(S_OK, hyperlink.As(&hypertext));
  EXPECT_EQ(S_OK,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_EQ(button_text_name, base::WideToUTF16(text.Get()));
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the link.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(3, &hyperlink));
  EXPECT_EQ(S_OK, hyperlink.As(&hypertext));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, text.Receive()));
  EXPECT_EQ(link_text_name, base::WideToUTF16(text.Get()));
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  LONG hyperlink_index;
  EXPECT_EQ(S_FALSE, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  // Invalid arguments should not be modified.
  hyperlink_index = -2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(root_hypertext_len, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(14, &hyperlink_index));
  EXPECT_EQ(0, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(30, &hyperlink_index));
  EXPECT_EQ(1, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(31, &hyperlink_index));
  EXPECT_EQ(2, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(32, &hyperlink_index));
  EXPECT_EQ(3, hyperlink_index);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestGetUIADirectChildrenInRange) {
  // Set up ax tree with the following structure:
  //
  // root___________________________________________________
  // |              |                |       |          |
  // para1____       link1 (ignored)  link2   button     para2___________
  // |       |      |                |                  |       |      |
  // text1   text2  text3            text4              text_5  image  text6

  AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("One two three.");

  AXNodeData text2;
  text2.id = 112;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("Two three four.");

  AXNodeData text3;
  text3.id = 113;
  text3.role = ax::mojom::Role::kStaticText;
  text3.SetName("Three four five.");
  text3.AddState(ax::mojom::State::kIgnored);

  AXNodeData text4;
  text4.id = 114;
  text4.role = ax::mojom::Role::kStaticText;
  text4.SetName("four five six.");

  AXNodeData text5;
  text5.id = 115;
  text5.role = ax::mojom::Role::kStaticText;
  text5.SetName("five six seven.");

  AXNodeData image;
  image.id = 116;
  image.role = ax::mojom::Role::kImage;

  AXNodeData text6;
  text6.id = 117;
  text6.role = ax::mojom::Role::kStaticText;
  text6.SetName("six seven eight.");

  AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids = {text1.id, text2.id};

  AXNodeData link1;
  link1.id = 12;
  link1.role = ax::mojom::Role::kLink;
  link1.child_ids = {text3.id};
  link1.AddState(ax::mojom::State::kIgnored);

  AXNodeData link2;
  link2.id = 13;
  link2.role = ax::mojom::Role::kLink;
  link2.child_ids = {text4.id};

  AXNodeData button;
  button.id = 14;
  button.role = ax::mojom::Role::kButton;

  AXNodeData para2;
  para2.id = 15;
  para2.role = ax::mojom::Role::kParagraph;
  para2.child_ids = {text5.id, image.id, text6.id};

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {para1.id, link1.id, link2.id, button.id, para2.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, para1, text1, text2, link1, text3,
                                     link2, text4, button, para2, text5, image,
                                     text6),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  BrowserAccessibility* para1_obj = manager->GetFromID(11);
  BrowserAccessibility* link2_obj = manager->GetFromID(13);
  BrowserAccessibility* button_obj = manager->GetFromID(14);
  BrowserAccessibility* para2_obj = manager->GetFromID(15);
  BrowserAccessibility* text1_obj = manager->GetFromID(111);
  BrowserAccessibility* text2_obj = manager->GetFromID(112);
  BrowserAccessibility* text3_obj = manager->GetFromID(113);
  BrowserAccessibility* text4_obj = manager->GetFromID(114);
  BrowserAccessibility* text5_obj = manager->GetFromID(115);
  BrowserAccessibility* image_obj = manager->GetFromID(116);
  BrowserAccessibility* text6_obj = manager->GetFromID(117);

  // When a range starts and end on a leaf node, no nodes should be returned.
  std::vector<gfx::NativeViewAccessible> descendants;
  std::vector<gfx::NativeViewAccessible> expected_descendants = {};

  descendants = text1_obj->GetUIADirectChildrenInRange(text1_obj, text1_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text2_obj->GetUIADirectChildrenInRange(text2_obj, text2_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text3_obj->GetUIADirectChildrenInRange(text3_obj, text3_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text4_obj->GetUIADirectChildrenInRange(text4_obj, text4_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text5_obj->GetUIADirectChildrenInRange(text5_obj, text5_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = image_obj->GetUIADirectChildrenInRange(image_obj, image_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text5_obj->GetUIADirectChildrenInRange(text6_obj, text6_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = button_obj->GetUIADirectChildrenInRange(button_obj, button_obj);
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // When called on a range that spans the entire document, the unignored link
  // object and the button should be returned by the function as they are the
  // two UIA embedded objects that are direct children of the root node.
  descendants = root_obj->GetUIADirectChildrenInRange(root_obj, root_obj);
  expected_descendants = {link2_obj->GetNativeViewAccessible(),
                          button_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // When called on a range that doesn't contain any UIA embedded object,
  // nothing should be returned from the function.
  descendants = para1_obj->GetUIADirectChildrenInRange(text2_obj, button_obj);
  expected_descendants = {};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Validate that the function doesn't include objects that are outside of the
  // the range. In this example, the button object shouldn't be included.
  descendants = root_obj->GetUIADirectChildrenInRange(text2_obj, text4_obj);
  expected_descendants = {link2_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Validates that it works on other nodes than the root one.
  descendants = para2_obj->GetUIADirectChildrenInRange(text5_obj, text6_obj);
  expected_descendants = {image_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestCreateEmptyDocument) {
  std::unique_ptr<BrowserAccessibilityManager> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetBrowserAccessibilityRoot();
  EXPECT_EQ(kInitialEmptyDocumentRootNodeID, root->GetId());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  EXPECT_EQ(ax::mojom::State::kNone, root->GetState());

  // Tree with a child textfield.
  AXNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = ax::mojom::Role::kRootWebArea;
  tree1_1.child_ids.push_back(2);

  AXNodeData tree1_2;
  tree1_2.id = 2;
  tree1_2.role = ax::mojom::Role::kTextField;
  tree1_2.AddState(ax::mojom::State::kEditable);
  tree1_2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  tree1_2.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");

  // Process a load complete.
  AXUpdatesAndEvents event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].node_id_to_clear = root->GetId();
  event_bundle.updates[0].root_id = tree1_1.id;
  event_bundle.updates[0].nodes.push_back(tree1_1);
  event_bundle.updates[0].nodes.push_back(tree1_2);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // The root for the initial empty document is replaced.
  root = manager->GetBrowserAccessibilityRoot();

  BrowserAccessibility* acc1_2 = manager->GetFromID(2);
  EXPECT_EQ(ax::mojom::Role::kTextField, acc1_2->GetRole());
  EXPECT_EQ(2, acc1_2->GetId());

  // Tree with a child button.
  AXNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.role = ax::mojom::Role::kRootWebArea;
  tree2_1.child_ids.push_back(3);

  AXNodeData tree2_2;
  tree2_2.id = 3;
  tree2_2.role = ax::mojom::Role::kButton;

  event_bundle.updates[0].nodes.clear();
  event_bundle.updates[0].node_id_to_clear = tree1_1.id;
  event_bundle.updates[0].root_id = tree2_1.id;
  event_bundle.updates[0].nodes.push_back(tree2_1);
  event_bundle.updates[0].nodes.push_back(tree2_2);

  // Unserialize the new tree update.
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Verify that the root has been cleared, not replaced.
  EXPECT_EQ(root, manager->GetBrowserAccessibilityRoot());

  BrowserAccessibility* acc2_2 = manager->GetFromID(3);
  EXPECT_EQ(ax::mojom::Role::kButton, acc2_2->GetRole());
  EXPECT_EQ(3, acc2_2->GetId());

  // Ensure we properly cleaned up.
  manager.reset();
}

int32_t GetUniqueId(BrowserAccessibility* accessibility) {
  BrowserAccessibilityWin* win_root = ToBrowserAccessibilityWin(accessibility);
  return win_root->GetCOM()->GetUniqueId();
}

// This is a regression test for a bug where the initial empty document
// loaded by a BrowserAccessibilityManagerWin couldn't be looked up by
// its UniqueIDWin, because the AX Tree was loaded in
// BrowserAccessibilityManager code before BrowserAccessibilityManagerWin
// was initialized.
TEST_F(BrowserAccessibilityWinTest, EmptyDocHasUniqueIdWin) {
  std::unique_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetBrowserAccessibilityRoot();
  EXPECT_EQ(kInitialEmptyDocumentRootNodeID, root->GetId());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  EXPECT_EQ(ax::mojom::State::kNone, root->GetState());

  BrowserAccessibilityWin* win_root = ToBrowserAccessibilityWin(root);

  AXPlatformNode* node = static_cast<AXPlatformNode*>(
      AXPlatformNodeWin::GetFromUniqueId(GetUniqueId(win_root)));

  AXPlatformNode* other_node = static_cast<AXPlatformNode*>(win_root->GetCOM());
  ASSERT_EQ(node, other_node);
}

TEST_F(BrowserAccessibilityWinTest, TestIA2Attributes) {
  AXNodeData pseudo_before;
  pseudo_before.id = 2;
  pseudo_before.role = ax::mojom::Role::kGenericContainer;
  pseudo_before.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                   "<pseudo:before>");
  pseudo_before.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                   "none");

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.SetCheckedState(ax::mojom::CheckedState::kTrue);
  checkbox.SetName("Checkbox");

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);
  root.SetName("Document");
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, pseudo_before, checkbox),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* pseudo_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, pseudo_accessible);

  base::win::ScopedBstr attributes;
  HRESULT hr =
      pseudo_accessible->GetCOM()->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, attributes.Get());
  std::wstring attributes_str(attributes.Get(), attributes.Length());
  EXPECT_EQ(L"display:none;tag:<pseudo\\:before>;", attributes_str);

  BrowserAccessibilityWin* checkbox_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, checkbox_accessible);

  attributes.Reset();
  hr = checkbox_accessible->GetCOM()->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, attributes.Get());
  attributes_str = std::wstring(attributes.Get(), attributes.Length());
  EXPECT_EQ(L"checkable:true;name-from:attribute;explicit-name:true;",
            attributes_str);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestValueAttributeInTextControls) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData combo_box, combo_box_text_container, combo_box_text;
  combo_box.id = 2;
  combo_box_text_container.id = 3;
  combo_box_text.id = 4;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box_text_container.role = ax::mojom::Role::kGenericContainer;
  combo_box_text.role = ax::mojom::Role::kStaticText;
  combo_box.SetName("Combo box:");
  combo_box.SetValue("Combo box text");
  combo_box_text.SetName("Combo box text");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box_text.AddState(ax::mojom::State::kEditable);
  combo_box.child_ids = {combo_box_text_container.id};
  combo_box_text_container.child_ids = {combo_box_text.id};

  AXNodeData search_box, search_box_text_container, search_box_text, new_line;
  search_box.id = 5;
  search_box_text_container.id = 6;
  search_box_text.id = 7;
  new_line.id = 8;
  search_box.role = ax::mojom::Role::kSearchBox;
  search_box_text_container.role = ax::mojom::Role::kGenericContainer;
  search_box_text.role = ax::mojom::Role::kStaticText;
  new_line.role = ax::mojom::Role::kLineBreak;
  search_box.SetName("Search for:");
  search_box.SetValue("Search box text\n");
  search_box_text.SetName("Search box text");
  new_line.SetName("\n");
  search_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  search_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  search_box.AddState(ax::mojom::State::kEditable);
  search_box.AddState(ax::mojom::State::kFocusable);
  search_box_text.AddState(ax::mojom::State::kEditable);
  new_line.AddState(ax::mojom::State::kEditable);
  search_box.child_ids = {search_box_text_container.id};
  search_box_text_container.child_ids = {search_box_text.id, new_line.id};

  AXNodeData text_field, text_field_text_container;
  text_field.id = 9;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddState(ax::mojom::State::kFocusable);
  // Exposes a placeholder. The text container is otherwise empty.
  text_field.SetValue("Text field text");
  text_field_text_container.id = 10;
  text_field_text_container.role = ax::mojom::Role::kGenericContainer;
  text_field.child_ids.push_back(text_field_text_container.id);

  AXNodeData link, link_text;
  link.id = 11;
  link_text.id = 12;
  link.role = ax::mojom::Role::kLink;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.SetName("Link text");
  link.child_ids.push_back(link_text.id);

  AXNodeData slider, slider_text;
  slider.id = 13;
  slider_text.id = 14;
  slider.role = ax::mojom::Role::kSlider;
  slider_text.role = ax::mojom::Role::kStaticText;
  slider.AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, 5.0F);
  slider_text.SetName("Slider text");
  slider.child_ids.push_back(slider_text.id);

  root.child_ids = {combo_box.id, search_box.id, text_field.id, link.id,
                    slider.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(
              root, combo_box, combo_box_text_container, combo_box_text,
              search_box, search_box_text_container, search_box_text, new_line,
              text_field, text_field_text_container, link, link_text, slider,
              slider_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(5U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* combo_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, combo_box_accessible);
  AXTreeData data = manager->GetTreeData();
  data.focus_id = combo_box_accessible->GetId();
  manager->ax_tree()->UpdateDataForTesting(data);
  ASSERT_EQ(combo_box_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));
  BrowserAccessibilityWin* search_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, search_box_accessible);
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(2));
  ASSERT_NE(nullptr, text_field_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(3));
  ASSERT_NE(nullptr, link_accessible);
  BrowserAccessibilityWin* slider_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(4));
  ASSERT_NE(nullptr, slider_accessible);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedVariant childid_slider(5);
  base::win::ScopedBstr value;

  HRESULT hr = combo_box_accessible->GetCOM()->get_accValue(childid_self,
                                                            value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Combo box text", value.Get());
  value.Reset();
  hr = search_box_accessible->GetCOM()->get_accValue(childid_self,
                                                     value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Search box text\n", value.Get());
  value.Reset();
  hr = text_field_accessible->GetCOM()->get_accValue(childid_self,
                                                     value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Text field text", value.Get());
  value.Reset();

  // Other controls, such as links, should not use their inner text as their
  // value. Only text entry controls.
  hr = link_accessible->GetCOM()->get_accValue(childid_self, value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0u, value.Length());
  value.Reset();

  // Sliders and other range controls should expose their current value and not
  // their inner text.
  // Also, try accessing the slider via its child number instead of directly.
  hr = root_accessible->GetCOM()->get_accValue(childid_slider, value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"5", value.Get());
  value.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestWordBoundariesInTextControls) {
  const std::string line1("This is a very LONG line of text that ");
  const std::string line2("should wrap on more than one lines ");
  const std::string text(line1 + line2);

  std::vector<int32_t> line1_word_starts;
  line1_word_starts.push_back(0);
  line1_word_starts.push_back(5);
  line1_word_starts.push_back(8);
  line1_word_starts.push_back(10);
  line1_word_starts.push_back(15);
  line1_word_starts.push_back(20);
  line1_word_starts.push_back(25);
  line1_word_starts.push_back(28);
  line1_word_starts.push_back(33);

  std::vector<int32_t> line2_word_starts;
  line2_word_starts.push_back(0);
  line2_word_starts.push_back(7);
  line2_word_starts.push_back(12);
  line2_word_starts.push_back(15);
  line2_word_starts.push_back(20);
  line2_word_starts.push_back(25);
  line2_word_starts.push_back(29);

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData textarea, textarea_div, textarea_text;
  textarea.id = 2;
  textarea_div.id = 3;
  textarea_text.id = 4;
  textarea.role = ax::mojom::Role::kTextField;
  textarea_div.role = ax::mojom::Role::kGenericContainer;
  textarea_text.role = ax::mojom::Role::kStaticText;
  textarea.AddState(ax::mojom::State::kEditable);
  textarea.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "textarea");
  textarea.AddState(ax::mojom::State::kFocusable);
  textarea.AddState(ax::mojom::State::kMultiline);
  textarea_div.AddState(ax::mojom::State::kEditable);
  textarea_text.AddState(ax::mojom::State::kEditable);
  textarea.SetValue(text);
  textarea_text.SetName(text);
  textarea.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "textarea");
  textarea.child_ids.push_back(textarea_div.id);
  textarea_div.child_ids.push_back(textarea_text.id);

  AXNodeData textarea_line1, textarea_line2;
  textarea_line1.id = 5;
  textarea_line2.id = 6;
  textarea_line1.role = ax::mojom::Role::kInlineTextBox;
  textarea_line2.role = ax::mojom::Role::kInlineTextBox;
  textarea_line1.AddState(ax::mojom::State::kEditable);
  textarea_line2.AddState(ax::mojom::State::kEditable);
  textarea_line1.SetName(line1);
  textarea_line2.SetName(line2);
  textarea_line1.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                     line1_word_starts);
  textarea_line2.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                     line2_word_starts);
  textarea_text.child_ids.push_back(textarea_line1.id);
  textarea_text.child_ids.push_back(textarea_line2.id);

  AXNodeData text_field, text_field_div, text_field_text;
  text_field.id = 7;
  text_field_div.id = 8;
  text_field_text.id = 9;
  text_field.role = ax::mojom::Role::kTextField;
  text_field_div.role = ax::mojom::Role::kGenericContainer;
  text_field_text.role = ax::mojom::Role::kStaticText;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  text_field.AddState(ax::mojom::State::kFocusable);
  text_field_div.AddState(ax::mojom::State::kEditable);
  text_field_text.AddState(ax::mojom::State::kEditable);
  text_field.SetValue(line1);
  text_field_text.SetName(line1);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  text_field.child_ids.push_back(text_field_div.id);
  text_field_div.child_ids.push_back(text_field_text.id);

  AXNodeData text_field_line;
  text_field_line.id = 10;
  text_field_line.role = ax::mojom::Role::kInlineTextBox;
  text_field_line.AddState(ax::mojom::State::kEditable);
  text_field_line.SetName(line1);
  text_field_line.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                      line1_word_starts);
  text_field_text.child_ids.push_back(text_field_line.id);

  root.child_ids.push_back(2);  // Textarea.
  root.child_ids.push_back(7);  // Text field.

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, textarea, textarea_div,
                                     textarea_text, textarea_line1,
                                     textarea_line2, text_field, text_field_div,
                                     text_field_text, text_field_line),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* textarea_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, textarea_accessible);
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, text_field_accessible);

  Microsoft::WRL::ComPtr<IAccessibleText> textarea_object;
  EXPECT_HRESULT_SUCCEEDED(textarea_accessible->GetCOM()->QueryInterface(
      IID_PPV_ARGS(&textarea_object)));
  Microsoft::WRL::ComPtr<IAccessibleText> text_field_object;
  EXPECT_HRESULT_SUCCEEDED(text_field_accessible->GetCOM()->QueryInterface(
      IID_PPV_ARGS(&text_field_object)));

  LONG offset = 0;
  while (offset < static_cast<LONG>(text.length())) {
    LONG start, end;
    base::win::ScopedBstr word;
    EXPECT_EQ(S_OK,
              textarea_object->get_textAtOffset(offset, IA2_TEXT_BOUNDARY_WORD,
                                                &start, &end, word.Receive()));
    EXPECT_EQ(offset, start);
    EXPECT_LT(offset, end);
    LONG space_offset = static_cast<LONG>(text.find(' ', offset));
    EXPECT_EQ(space_offset + 1, end);
    LONG length = end - start;
    EXPECT_EQ(base::ASCIIToWide(text.substr(start, length)), word.Get());
    word.Reset();
    offset = end;
  }

  offset = 0;
  while (offset < static_cast<LONG>(line1.length())) {
    LONG start, end;
    base::win::ScopedBstr word;
    EXPECT_EQ(S_OK, text_field_object->get_textAtOffset(
                        offset, IA2_TEXT_BOUNDARY_WORD, &start, &end,
                        word.Receive()));
    EXPECT_EQ(offset, start);
    EXPECT_LT(offset, end);
    LONG space_offset = static_cast<LONG>(line1.find(' ', offset));
    EXPECT_EQ(space_offset + 1, end);
    LONG length = end - start;
    EXPECT_EQ(base::ASCIIToWide(text.substr(start, length)), word.Get());
    word.Reset();
    offset = end;
  }

  textarea_object.Reset();
  text_field_object.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TextBoundariesOnlyEmbeddedObjectsNoCrash) {
  // Update the tree structure to test get_textAtOffset from an
  // embedded object that has no text, only an embedded object child.
  //
  // +-1 root_data
  //   +-2 menu_data
  //   | +-3 button_1_data
  //   | +-4 button_2_data
  //   +-5 static_text_data "after"
  //
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData menu_data;
  menu_data.id = 2;
  menu_data.role = ax::mojom::Role::kMenu;

  AXNodeData button_1_data;
  button_1_data.id = 3;
  button_1_data.role = ax::mojom::Role::kButton;

  AXNodeData button_2_data;
  button_2_data.id = 4;
  button_2_data.role = ax::mojom::Role::kButton;

  AXNodeData static_text_data;
  static_text_data.id = 5;
  static_text_data.role = ax::mojom::Role::kStaticText;
  static_text_data.SetName("after");

  root_data.child_ids = {menu_data.id, static_text_data.id};
  menu_data.child_ids = {button_1_data.id, button_2_data.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root_data, menu_data, button_1_data,
                                     button_2_data, static_text_data),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityComWin* menu_accessible_com =
      ToBrowserAccessibilityComWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, menu_accessible_com);
  ASSERT_EQ(ax::mojom::Role::kMenu, menu_accessible_com->GetRole());

  EXPECT_IA2_TEXT_AT_OFFSET(
      menu_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));
}

TEST_F(BrowserAccessibilityWinTest,
       DISABLED_TestTextBoundariesEmbeddedCharacterText) {
  // Update the tree structure to test empty leaf text positions.
  //
  // +-1 root_data
  //   +-2 body_data
  //     +-3 static_text_1_data "before"
  //     | +-4 inline_text_1_data "before"
  //     +-5 menu_data_1
  //     | +-6 button_data
  //     |   +-7 button_leaf_container_data
  //     |   +-8 button_leaf_svg_data
  //     +-9 menu_data_2
  //     +-10 static_text_2_data "after"
  //     | +-11 inline_text_2_data "after"
  //     +-12 static_text_3_data "tail"
  //     | +-13 inline_text_3_data "tail"
  //
  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData body_data;
  body_data.id = 2;
  body_data.role = ax::mojom::Role::kGenericContainer;

  AXNodeData static_text_1_data;
  static_text_1_data.id = 3;
  static_text_1_data.role = ax::mojom::Role::kStaticText;
  static_text_1_data.SetName("before");

  AXNodeData inline_text_1_data;
  inline_text_1_data.id = 4;
  inline_text_1_data.role = ax::mojom::Role::kInlineTextBox;
  inline_text_1_data.SetName("before");

  AXNodeData menu_data_1;
  menu_data_1.id = 5;
  menu_data_1.role = ax::mojom::Role::kMenu;

  AXNodeData button_data;
  button_data.id = 6;
  button_data.role = ax::mojom::Role::kButton;

  AXNodeData button_leaf_container_data;
  button_leaf_container_data.id = 7;
  button_leaf_container_data.role = ax::mojom::Role::kGenericContainer;

  AXNodeData button_leaf_svg_data;
  button_leaf_svg_data.id = 8;
  button_leaf_svg_data.role = ax::mojom::Role::kSvgRoot;

  AXNodeData menu_data_2;
  menu_data_2.id = 9;
  menu_data_2.role = ax::mojom::Role::kMenu;

  AXNodeData static_text_2_data;
  static_text_2_data.id = 10;
  static_text_2_data.role = ax::mojom::Role::kStaticText;
  static_text_2_data.SetName("after");

  AXNodeData inline_text_2_data;
  inline_text_2_data.id = 11;
  inline_text_2_data.role = ax::mojom::Role::kInlineTextBox;
  inline_text_2_data.SetName("after");

  AXNodeData static_text_3_data;
  static_text_3_data.id = 12;
  static_text_3_data.role = ax::mojom::Role::kStaticText;
  static_text_3_data.SetName("tail");

  AXNodeData inline_text_3_data;
  inline_text_3_data.id = 13;
  inline_text_3_data.role = ax::mojom::Role::kInlineTextBox;
  inline_text_3_data.SetName("tail");

  root_data.child_ids = {body_data.id};
  body_data.child_ids = {static_text_1_data.id, menu_data_1.id, menu_data_2.id,
                         static_text_2_data.id, static_text_3_data.id};
  menu_data_1.child_ids = {button_data.id};
  button_data.child_ids = {button_leaf_container_data.id,
                           button_leaf_svg_data.id};
  static_text_1_data.child_ids = {inline_text_1_data.id};
  static_text_2_data.child_ids = {inline_text_2_data.id};
  static_text_3_data.child_ids = {inline_text_3_data.id};

  AXTreeUpdate update;
  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes = {root_data,
                  body_data,
                  static_text_1_data,
                  inline_text_1_data,
                  menu_data_1,
                  button_data,
                  button_leaf_container_data,
                  button_leaf_svg_data,
                  menu_data_2,
                  static_text_2_data,
                  inline_text_2_data,
                  static_text_3_data,
                  inline_text_3_data};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* body_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, body_accessible);
  ASSERT_EQ(5U, body_accessible->PlatformChildCount());
  BrowserAccessibilityComWin* body_accessible_com = body_accessible->GetCOM();
  ASSERT_NE(nullptr, body_accessible_com);

  BrowserAccessibilityComWin* static_text_1_com =
      ToBrowserAccessibilityWin(body_accessible->PlatformGetChild(0))->GetCOM();
  ASSERT_NE(nullptr, static_text_1_com);
  ASSERT_EQ(ax::mojom::Role::kStaticText, static_text_1_com->GetRole());

  BrowserAccessibilityComWin* menu_1_accessible_com =
      ToBrowserAccessibilityWin(body_accessible->PlatformGetChild(1))->GetCOM();
  ASSERT_NE(nullptr, menu_1_accessible_com);
  ASSERT_EQ(ax::mojom::Role::kMenu, menu_1_accessible_com->GetRole());

  BrowserAccessibilityComWin* menu_2_accessible_com =
      ToBrowserAccessibilityWin(body_accessible->PlatformGetChild(2))->GetCOM();
  ASSERT_NE(nullptr, menu_2_accessible_com);
  ASSERT_EQ(ax::mojom::Role::kMenu, menu_2_accessible_com->GetRole());

  BrowserAccessibilityComWin* static_text_2_com =
      ToBrowserAccessibilityWin(body_accessible->PlatformGetChild(3))->GetCOM();
  ASSERT_NE(nullptr, static_text_2_com);
  ASSERT_EQ(ax::mojom::Role::kStaticText, static_text_2_com->GetRole());

  BrowserAccessibilityComWin* static_text_3_com =
      ToBrowserAccessibilityWin(body_accessible->PlatformGetChild(4))->GetCOM();
  ASSERT_NE(nullptr, static_text_3_com);
  ASSERT_EQ(ax::mojom::Role::kStaticText, static_text_3_com->GetRole());

  // [obj] stands for the embedded object replacement character \xFFFC.

  // L"<b>efore" [obj] [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
                            /*text=*/L"b");

  // L"bef<o>re" [obj] [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 3, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/3, /*end=*/4,
                            /*text=*/L"o");

  // L"befor<e>" [obj] [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 5, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/5, /*end=*/6,
                            /*text=*/L"e");

  // L"before" <[obj]> [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(
      body_accessible_com, 6, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/6, /*end=*/7,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // L"before" [obj] <[obj]> L"after" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(
      body_accessible_com, 7, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/7, /*end=*/8,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // L"before" [obj] [obj] L"<a>fter" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 8, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/8, /*end=*/9,
                            /*text=*/L"a");

  // L"before" [obj] [obj] L"a<f>ter" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 9, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/9, /*end=*/10,
                            /*text=*/L"f");

  // L"before" [obj] [obj] L"afte<r>" L"tail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 12, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/12, /*end=*/13,
                            /*text=*/L"r");

  // L"before" [obj] [obj] L"after" L"<t>ail"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 13, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/13, /*end=*/14,
                            /*text=*/L"t");

  // L"before" [obj] [obj] L"after" L"ta<i>l"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 15, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/15, /*end=*/16,
                            /*text=*/L"i");

  // L"before" [obj] [obj] L"after" L"tai<l>"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 16, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/16, /*end=*/17,
                            /*text=*/L"l");

  // L"before" [obj] [obj] L"after" L"tail<>"
  EXPECT_IA2_TEXT_AT_OFFSET(body_accessible_com, 17, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // L"<b>efore"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_1_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
                            /*text=*/L"b");

  // L"be<f>ore"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_1_com, 2, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/2, /*end=*/3,
                            /*text=*/L"f");

  // L"before<>"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_1_com, 6, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // <[obj]>
  EXPECT_IA2_TEXT_AT_OFFSET(
      menu_1_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // [obj]<>
  EXPECT_IA2_TEXT_AT_OFFSET(menu_1_accessible_com, 1, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // L"<>"
  EXPECT_IA2_TEXT_AT_OFFSET(menu_2_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // L"<a>fter"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_2_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
                            /*text=*/L"a");

  // L"af<t>er"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_2_com, 2, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/2, /*end=*/3,
                            /*text=*/L"t");

  // L"after<>"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_2_com, 5, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // L"<t>ail"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_3_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/0, /*end=*/1,
                            /*text=*/L"t");

  // L"ta<i>l"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_3_com, 2, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/S_OK, /*start=*/2, /*end=*/3,
                            /*text=*/L"i");

  // L"tail<>"
  EXPECT_IA2_TEXT_AT_OFFSET(static_text_3_com, 4, IA2_TEXT_BOUNDARY_CHAR,
                            /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                            /*end=*/0,
                            /*text=*/nullptr);

  // L"before" [obj] <[obj]> L"<a>fter" L"tail"
  EXPECT_IA2_TEXT_BEFORE_OFFSET(
      body_accessible_com, 7, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/6, /*end=*/7,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // L"before" <[obj]> [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_BEFORE_OFFSET(body_accessible_com, 6, IA2_TEXT_BOUNDARY_CHAR,
                                /*expected_hr=*/S_OK, /*start=*/5, /*end=*/6,
                                /*text=*/L"e");

  // <[obj]>
  EXPECT_IA2_TEXT_BEFORE_OFFSET(menu_1_accessible_com, 0,
                                IA2_TEXT_BOUNDARY_CHAR,
                                /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                                /*text=*/nullptr);

  // L"<>"
  EXPECT_IA2_TEXT_BEFORE_OFFSET(menu_2_accessible_com, 0,
                                IA2_TEXT_BOUNDARY_CHAR,
                                /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                                /*end=*/0,
                                /*text=*/nullptr);

  // L"befor<e>" [obj] <[obj]> L"after" L"tail"
  EXPECT_IA2_TEXT_AFTER_OFFSET(
      body_accessible_com, 5, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/6, /*end=*/7,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // L"before" <[obj]> [obj] L"after" L"tail"
  EXPECT_IA2_TEXT_AFTER_OFFSET(
      body_accessible_com, 6, IA2_TEXT_BOUNDARY_CHAR,
      /*expected_hr=*/S_OK, /*start=*/7, /*end=*/8,
      /*text=*/
      base::as_wcstr(std::u16string{AXPlatformNodeBase::kEmbeddedCharacter}));

  // <[obj]>
  EXPECT_IA2_TEXT_AFTER_OFFSET(menu_1_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                               /*expected_hr=*/S_FALSE, /*start=*/0, /*end=*/0,
                               /*text=*/nullptr);

  // L"<>"
  EXPECT_IA2_TEXT_AFTER_OFFSET(menu_2_accessible_com, 0, IA2_TEXT_BOUNDARY_CHAR,
                               /*expected_hr=*/E_INVALIDARG, /*start=*/0,
                               /*end=*/0,
                               /*text=*/nullptr);
}

TEST_F(BrowserAccessibilityWinTest, TestCaretAndSelectionInSimpleFields) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box.SetValue("Test1");
  // Place the caret between 't' and 'e'.
  combo_box.AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, 1);
  combo_box.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, 1);

  AXNodeData text_field;
  text_field.id = 3;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  text_field.AddState(ax::mojom::State::kFocusable);
  text_field.SetValue("Test2");
  // Select the letter 'e'.
  text_field.AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, 1);
  text_field.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, 2);

  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, combo_box, text_field),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* combo_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, combo_box_accessible);
  AXTreeData data = manager->GetTreeData();
  data.focus_id = combo_box_accessible->GetId();
  data.sel_anchor_object_id = combo_box_accessible->GetId();
  data.sel_focus_object_id = combo_box_accessible->GetId();
  manager->ax_tree()->UpdateDataForTesting(data);
  ASSERT_EQ(combo_box_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, text_field_accessible);

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  // Test get_caretOffset.
  HRESULT hr = combo_box_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);
  // The caret should not be visible because the text field is not focused.
  hr = text_field_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(0, caret_offset);

  // Move the focus to the text field.
  data = manager->GetTreeData();
  data.focus_id = text_field_accessible->GetId();
  data.sel_anchor_object_id = text_field_accessible->GetId();
  data.sel_focus_object_id = text_field_accessible->GetId();
  manager->ax_tree()->UpdateDataForTesting(data);
  ASSERT_EQ(text_field_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  // The caret should now appear at the end of the text field.
  hr = text_field_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2, caret_offset);

  // Test get_nSelections.
  hr = combo_box_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = text_field_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);

  // Test get_selection.
  hr = combo_box_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(E_INVALIDARG, hr);  // No selections available.
  hr = text_field_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  EXPECT_EQ(2, selection_end);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestCaretInContentEditables) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kEditable);
  div_editable.AddState(ax::mojom::State::kRichlyEditable);
  div_editable.AddState(ax::mojom::State::kFocusable);
  div_editable.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);

  AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.AddState(ax::mojom::State::kEditable);
  text.AddState(ax::mojom::State::kRichlyEditable);
  text.SetName("Click ");

  AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kRichlyEditable);
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");

  AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kRichlyEditable);
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  AXTreeUpdate update =
      MakeAXTreeUpdateForTesting(root, div_editable, link, link_text, text);

  // Place the caret between 'h' and 'e'.
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 5;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 1;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;

  // No selection should be present.
  HRESULT hr =
      div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);

  // The caret should be on the embedded object character.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6, caret_offset);

  // Move the focus to the content editable.
  AXTreeData data = manager->GetTreeData();
  data.focus_id = div_editable_accessible->GetId();
  manager->ax_tree()->UpdateDataForTesting(data);
  ASSERT_EQ(div_editable_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      ToBrowserAccessibilityWin(link_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, link_text_accessible);

  // The caret should not have moved.
  hr = div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6, caret_offset);

  hr = link_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = link_text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);

  hr = link_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);
  hr = link_text_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestSelectionInContentEditables) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kFocusable);
  div_editable.AddState(ax::mojom::State::kEditable);

  AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.AddState(ax::mojom::State::kFocusable);
  text.AddState(ax::mojom::State::kEditable);
  text.SetName("Click ");

  AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");

  AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  AXTreeUpdate update =
      MakeAXTreeUpdateForTesting(root, div_editable, link, link_text, text);

  // Select the following part of the text: "lick here".
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 4;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      ToBrowserAccessibilityWin(link_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, link_text_accessible);

  // get_nSelections should work on all objects.
  HRESULT hr =
      div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = link_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = link_text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);

  // get_selection should be unaffected by focus placement.
  hr = div_editable_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  // selection_end should be after embedded object character.
  EXPECT_EQ(7, selection_end);

  hr = text_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  // No embedded character on this object, only the first part of the text.
  EXPECT_EQ(6, selection_end);
  hr = link_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, selection_start);
  EXPECT_EQ(4, selection_end);
  hr = link_text_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, selection_start);
  EXPECT_EQ(4, selection_end);

  // The caret should be at the focus (the end) of the selection.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7, caret_offset);

  // Move the focus to the content editable.
  AXTreeData data = manager->GetTreeData();
  data.focus_id = div_editable_accessible->GetId();
  manager->ax_tree()->UpdateDataForTesting(data);
  ASSERT_EQ(div_editable_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  // The caret should not have moved.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7, caret_offset);

  // The caret offset should reflect the position of the selection's focus in
  // any given object.
  hr = link_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4, caret_offset);
  hr = link_text_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4, caret_offset);

  hr = div_editable_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  EXPECT_EQ(7, selection_end);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestIAccessibleHyperlink) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData div;
  div.id = 2;
  div.role = ax::mojom::Role::kGenericContainer;
  div.AddState(ax::mojom::State::kFocusable);

  AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("Click ");

  AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");
  link.SetNameFrom(ax::mojom::NameFrom::kContents);
  link.AddStringAttribute(ax::mojom::StringAttribute::kUrl, "example.com");

  root.child_ids.push_back(div.id);
  div.child_ids = {text.id, link.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, div, link, text), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_accessible);
  ASSERT_EQ(2U, div_accessible->PlatformChildCount());

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);

  // -1 is never a valid value.
  LONG n_actions = -1;
  LONG start_index = -1;
  LONG end_index = -1;

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedVariant anchor;
  base::win::ScopedVariant anchor_target;
  base::win::ScopedBstr bstr;

  std::u16string div_hypertext(u"Click ");
  div_hypertext.push_back(AXPlatformNodeBase::kEmbeddedCharacter);

  // div_accessible and link_accessible are the only IA2 hyperlinks.
  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      div_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      link_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();

  EXPECT_HRESULT_SUCCEEDED(root_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(2, n_actions);
  EXPECT_HRESULT_SUCCEEDED(div_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(2, n_actions);
  EXPECT_HRESULT_SUCCEEDED(text_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(2, n_actions);
  EXPECT_HRESULT_SUCCEEDED(link_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(2, n_actions);

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  HRESULT hr = div_accessible->GetCOM()->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_EQ(div_hypertext, base::WideToUTF16(bstr.Get()));
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  hr = link_accessible->GetCOM()->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_STREQ(L"here", bstr.Get());
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      div_accessible->GetCOM()->get_anchor(1, anchor.Receive()));
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      link_accessible->GetCOM()->get_anchor(1, anchor.Receive()));
  anchor.Reset();

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = div_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  // Target should be empty.
  EXPECT_STREQ(L"", bstr.Get());
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = link_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  EXPECT_STREQ(L"example.com", bstr.Get());
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      div_accessible->GetCOM()->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      link_accessible->GetCOM()->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(
      div_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_EQ(0, start_index);
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(
      link_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_EQ(6, start_index);

  EXPECT_HRESULT_FAILED(root_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(div_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_EQ(1, end_index);
  EXPECT_HRESULT_FAILED(text_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(link_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_EQ(7, end_index);
}

TEST_F(BrowserAccessibilityWinTest, TestTextAttributesInContentEditables) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kEditable);
  div_editable.AddState(ax::mojom::State::kRichlyEditable);
  div_editable.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);
  div_editable.AddState(ax::mojom::State::kFocusable);
  div_editable.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                  "Helvetica");

  AXNodeData text_before;
  text_before.id = 3;
  text_before.role = ax::mojom::Role::kStaticText;
  text_before.AddState(ax::mojom::State::kEditable);
  text_before.AddState(ax::mojom::State::kRichlyEditable);
  text_before.SetName("Before ");
  text_before.AddTextStyle(ax::mojom::TextStyle::kBold);
  text_before.AddTextStyle(ax::mojom::TextStyle::kItalic);

  AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kRichlyEditable);
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("lnk");
  link.AddTextStyle(ax::mojom::TextStyle::kUnderline);

  AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kRichlyEditable);
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("lnk");
  link_text.AddTextStyle(ax::mojom::TextStyle::kUnderline);

  // The name "lnk" is misspelled.
  std::vector<int32_t> marker_types{
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)};
  std::vector<int32_t> marker_starts{0};
  std::vector<int32_t> marker_ends{3};
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                marker_types);
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                marker_starts);
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                marker_ends);

  AXNodeData text_after;
  text_after.id = 6;
  text_after.role = ax::mojom::Role::kStaticText;
  text_after.AddState(ax::mojom::State::kEditable);
  text_after.AddState(ax::mojom::State::kRichlyEditable);
  text_after.SetName(" after.");
  // Leave text style as normal.

  root.child_ids.push_back(div_editable.id);
  div_editable.child_ids.push_back(text_before.id);
  div_editable.child_ids.push_back(link.id);
  div_editable.child_ids.push_back(text_after.id);
  link.child_ids.push_back(link_text.id);

  AXTreeUpdate update = MakeAXTreeUpdateForTesting(
      root, div_editable, text_before, link, link_text, text_after);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_div =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(3U, ax_div->PlatformChildCount());

  BrowserAccessibilityWin* ax_before =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_before);
  BrowserAccessibilityWin* ax_link =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(1));
  ASSERT_NE(nullptr, ax_link);
  ASSERT_EQ(1U, ax_link->PlatformChildCount());
  BrowserAccessibilityWin* ax_after =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(2));
  ASSERT_NE(nullptr, ax_after);

  BrowserAccessibilityWin* ax_link_text =
      ToBrowserAccessibilityWin(ax_link->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_link_text);

  HRESULT hr;
  LONG n_characters, start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  ASSERT_HRESULT_SUCCEEDED(ax_root->GetCOM()->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);
  ASSERT_HRESULT_SUCCEEDED(ax_div->GetCOM()->get_nCharacters(&n_characters));
  ASSERT_EQ(15, n_characters);

  // Test the style of the root.
  hr = ax_root->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                         text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(1, end_offset);
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-family:Helvetica"));
  text_attributes.Reset();

  // Test the style of text_before.
  for (LONG offset = 0; offset < 7; ++offset) {
    hr = ax_div->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                          text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(7, end_offset);
    std::wstring attributes(text_attributes.Get());
    EXPECT_NE(std::wstring::npos, attributes.find(L"font-family:Helvetica"));
    EXPECT_NE(std::wstring::npos, attributes.find(L"font-weight:bold"));
    EXPECT_NE(std::wstring::npos, attributes.find(L"font-style:italic"));
    text_attributes.Reset();
  }

  // Test the style of the link.
  hr = ax_link->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                         text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(3, end_offset);
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-family:Helvetica"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-weight:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-style:"));
  EXPECT_NE(
      std::wstring::npos,
      std::wstring(text_attributes.Get()).find(L"text-underline-style:solid"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"text-underline-type:"));
  // For compatibility with Firefox, spelling attributes should also be
  // propagated to the parent of static text leaves.
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
  text_attributes.Reset();

  hr = ax_link_text->GetCOM()->get_attributes(2, &start_offset, &end_offset,
                                              text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(3, end_offset);
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-family:Helvetica"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-weight:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-style:"));
  EXPECT_NE(
      std::wstring::npos,
      std::wstring(text_attributes.Get()).find(L"text-underline-style:solid"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"text-underline-type:"));
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
  text_attributes.Reset();

  // Test the style of text_after.
  for (LONG offset = 8; offset < 15; ++offset) {
    hr = ax_div->GetCOM()->get_attributes(offset, &start_offset, &end_offset,
                                          text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(8, start_offset);
    EXPECT_EQ(15, end_offset);
    std::wstring attributes(text_attributes.Get());
    EXPECT_NE(std::wstring::npos, attributes.find(L"font-family:Helvetica"));
    EXPECT_EQ(std::wstring::npos, attributes.find(L"font-weight:"));
    EXPECT_EQ(std::wstring::npos, attributes.find(L"font-style:"));
    EXPECT_EQ(std::wstring::npos, std::wstring(text_attributes.Get())
                                      .find(L"text-underline-style:solid"));
    EXPECT_EQ(
        std::wstring::npos,
        std::wstring(text_attributes.Get()).find(L"text-underline-type:"));
    EXPECT_EQ(std::wstring::npos, attributes.find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Test the style of the static text nodes.
  hr = ax_before->GetCOM()->get_attributes(6, &start_offset, &end_offset,
                                           text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-family:Helvetica"));
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-weight:bold"));
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-style:italic"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
  text_attributes.Reset();

  hr = ax_after->GetCOM()->get_attributes(6, &start_offset, &end_offset,
                                          text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  EXPECT_NE(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-family:Helvetica"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-weight:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"font-style:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"text-underline-style:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"text-underline-type:"));
  EXPECT_EQ(std::wstring::npos,
            std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
  text_attributes.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest,
       TestExistingMisspellingsInSimpleTextFields) {
  std::string value1("Testing .");
  // The word "helo" is misspelled.
  std::string value2("Helo there.");

  LONG value1_length = static_cast<LONG>(value1.length());
  LONG value2_length = static_cast<LONG>(value2.length());
  LONG combo_box_value_length = value1_length + value2_length;

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box.SetValue(value1 + value2);

  AXNodeData combo_box_div;
  combo_box_div.id = 3;
  combo_box_div.role = ax::mojom::Role::kGenericContainer;
  combo_box_div.AddState(ax::mojom::State::kEditable);

  AXNodeData static_text1;
  static_text1.id = 4;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.AddState(ax::mojom::State::kEditable);
  static_text1.SetName(value1);

  AXNodeData static_text2;
  static_text2.id = 5;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddState(ax::mojom::State::kEditable);
  static_text2.SetName(value2);

  std::vector<int32_t> marker_types;
  marker_types.push_back(
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling));
  std::vector<int32_t> marker_starts;
  marker_starts.push_back(0);
  std::vector<int32_t> marker_ends;
  marker_ends.push_back(4);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                   marker_types);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   marker_starts);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   marker_ends);

  root.child_ids.push_back(combo_box.id);
  combo_box.child_ids.push_back(combo_box_div.id);
  combo_box_div.child_ids.push_back(static_text1.id);
  combo_box_div.child_ids.push_back(static_text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, combo_box, combo_box_div,
                                     static_text1, static_text2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_combo_box =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_combo_box);

  HRESULT hr;
  LONG start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  // Ensure that the first part of the value is not marked misspelled.
  for (LONG offset = 0; offset < value1_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(std::wstring(text_attributes.Get()).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(value1_length, end_offset);
    text_attributes.Reset();
  }

  // Ensure that "helo" is marked misspelled.
  for (LONG offset = value1_length; offset < value1_length + 4; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(value1_length, start_offset);
    EXPECT_EQ(value1_length + 4, end_offset);
    EXPECT_NE(std::wstring::npos,
              std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Ensure that the last part of the value is not marked misspelled.
  for (LONG offset = value1_length + 4; offset < combo_box_value_length;
       ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(std::wstring(text_attributes.Get()).empty());
    EXPECT_EQ(value1_length + 4, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestNewMisspellingsInSimpleTextFields) {
  std::string value1("Testing .");
  // The word "helo" is misspelled.
  std::string value2("Helo there.");

  LONG value1_length = static_cast<LONG>(value1.length());
  LONG value2_length = static_cast<LONG>(value2.length());
  LONG combo_box_value_length = value1_length + value2_length;

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kInputType, "text");
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box.SetValue(value1 + value2);

  AXNodeData combo_box_div;
  combo_box_div.id = 3;
  combo_box_div.role = ax::mojom::Role::kGenericContainer;
  combo_box_div.AddState(ax::mojom::State::kEditable);

  AXNodeData static_text1;
  static_text1.id = 4;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.AddState(ax::mojom::State::kEditable);
  static_text1.SetName(value1);

  AXNodeData static_text2;
  static_text2.id = 5;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddState(ax::mojom::State::kEditable);
  static_text2.SetName(value2);

  root.child_ids.push_back(combo_box.id);
  combo_box.child_ids.push_back(combo_box_div.id);
  combo_box_div.child_ids.push_back(static_text1.id);
  combo_box_div.child_ids.push_back(static_text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, combo_box, combo_box_div,
                                     static_text1, static_text2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ASSERT_NE(nullptr, manager->GetBrowserAccessibilityRoot());
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_combo_box =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_combo_box);

  HRESULT hr;
  LONG start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  // Ensure that nothing is marked misspelled.
  for (LONG offset = 0; offset < combo_box_value_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(std::wstring(text_attributes.Get()).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  // Add the spelling markers on "helo".
  std::vector<int32_t> marker_types{
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)};
  std::vector<int32_t> marker_starts{0};
  std::vector<int32_t> marker_ends{4};
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                   marker_types);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   marker_starts);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   marker_ends);
  AXTree* tree = const_cast<AXTree*>(manager->ax_tree());
  ASSERT_NE(nullptr, tree);
  AXTreeUpdate update = MakeAXTreeUpdateForTesting(static_text2);
  update.tree_data.tree_id = manager->GetTreeID();
  ASSERT_TRUE(tree->Unserialize(update));

  // Ensure that value1 is still not marked misspelled.
  for (LONG offset = 0; offset < value1_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(std::wstring(text_attributes.Get()).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(value1_length, end_offset);
    text_attributes.Reset();
  }

  // Ensure that "helo" is now marked misspelled.
  for (LONG offset = value1_length; offset < value1_length + 4; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(value1_length, start_offset);
    EXPECT_EQ(value1_length + 4, end_offset);
    EXPECT_NE(std::wstring::npos,
              std::wstring(text_attributes.Get()).find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Ensure that the last part of the value is not marked misspelled.
  for (LONG offset = value1_length + 4; offset < combo_box_value_length;
       ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(std::wstring(text_attributes.Get()).empty());
    EXPECT_EQ(value1_length + 4, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestDeepestFirstLastChild) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(3);

  AXNodeData child2_child1;
  child2_child1.id = 4;
  child2_child1.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(4);

  AXNodeData child2_child2;
  child2_child2.id = 5;
  child2_child2.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, child1, child2, child2_child1,
                                     child2_child2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_accessible =
      manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());
  BrowserAccessibility* child1_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child1_accessible);
  BrowserAccessibility* child2_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_accessible);
  ASSERT_EQ(0U, child2_accessible->PlatformChildCount());
  ASSERT_EQ(2U, child2_accessible->InternalChildCount());
  BrowserAccessibility* child2_child1_accessible =
      child2_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, child2_child1_accessible);
  BrowserAccessibility* child2_child2_accessible =
      child2_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, child2_child2_accessible);

  EXPECT_EQ(child1_accessible, root_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child1_accessible, root_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(child2_accessible, root_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(child2_child2_accessible,
            root_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child1_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child1_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child2_child1_accessible,
            child2_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(nullptr, child2_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(child2_child2_accessible,
            child2_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->InternalDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->InternalDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->InternalDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->InternalDeepestLastChild());
}

TEST_F(BrowserAccessibilityWinTest, TestInheritedStringAttributes) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en-US");
  root.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "Helvetica");

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  child2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  child2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "Arial");
  root.child_ids.push_back(3);

  AXNodeData child2_child1;
  child2_child1.id = 4;
  child2_child1.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(4);

  AXNodeData child2_child2;
  child2_child2.id = 5;
  child2_child2.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, child1, child2, child2_child1,
                                     child2_child2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_accessible =
      manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* child1_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child1_accessible);
  BrowserAccessibility* child2_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_accessible);
  BrowserAccessibility* child2_child1_accessible =
      child2_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, child2_child1_accessible);
  BrowserAccessibility* child2_child2_accessible =
      child2_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, child2_child2_accessible);

  // Test GetInheritedString16Attribute(attribute).
  EXPECT_EQ(u"en-US", root_accessible->GetInheritedString16Attribute(
                          ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(u"en-US", child1_accessible->GetInheritedString16Attribute(
                          ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(u"fr", child2_accessible->GetInheritedString16Attribute(
                       ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(u"fr", child2_child1_accessible->GetInheritedString16Attribute(
                       ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(u"fr", child2_child2_accessible->GetInheritedString16Attribute(
                       ax::mojom::StringAttribute::kLanguage));

  // Test GetInheritedStringAttribute(attribute).
  EXPECT_EQ("Helvetica", root_accessible->GetInheritedStringAttribute(
                             ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Helvetica", child1_accessible->GetInheritedStringAttribute(
                             ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_child1_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_child2_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
}

TEST_F(BrowserAccessibilityWinTest, AccChildOnlyReturnsDescendants) {
  AXNodeData root_node;
  root_node.id = 1;
  root_node.role = ax::mojom::Role::kRootWebArea;

  AXNodeData child_node;
  child_node.id = 2;
  root_node.child_ids.push_back(2);

  std::unique_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          MakeAXTreeUpdateForTesting(root_node, child_node), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root = manager->GetBrowserAccessibilityRoot();
  BrowserAccessibility* child = root->PlatformGetChild(0);

  base::win::ScopedVariant root_unique_id_variant(-GetUniqueId(root));
  Microsoft::WRL::ComPtr<IDispatch> result;
  EXPECT_EQ(E_INVALIDARG,
            ToBrowserAccessibilityWin(child)->GetCOM()->get_accChild(
                root_unique_id_variant, &result));

  base::win::ScopedVariant child_unique_id_variant(-GetUniqueId(child));
  EXPECT_EQ(S_OK, ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
                      child_unique_id_variant, &result));
}

// TODO(crbug.com/41439880): Disabled due to flakiness.
TEST_F(BrowserAccessibilityWinTest, DISABLED_TestIAccessible2Relations) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  // Reflexive relations should be ignored.
  std::vector<int32_t> describedby_ids = {1, 2, 3};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds,
                           describedby_ids);

  AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, child1, child2), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, ax_root);
  BrowserAccessibilityWin* ax_child1 =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_child1);
  BrowserAccessibilityWin* ax_child2 =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(1));
  ASSERT_NE(nullptr, ax_child2);

  LONG n_relations = 0;
  LONG n_targets = 0;
  LONG unique_id = 0;
  base::win::ScopedBstr relation_type;
  Microsoft::WRL::ComPtr<IAccessibleRelation> describedby_relation;
  Microsoft::WRL::ComPtr<IAccessibleRelation> description_for_relation;
  Microsoft::WRL::ComPtr<IUnknown> target;
  Microsoft::WRL::ComPtr<IAccessible2> ax_target;

  EXPECT_HRESULT_SUCCEEDED(ax_root->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_root->GetCOM()->get_relation(0, &describedby_relation));
  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"describedBy", std::wstring(relation_type.Get()));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(describedby_relation->get_nTargets(&n_targets));
  EXPECT_EQ(2, n_targets);

  EXPECT_HRESULT_SUCCEEDED(describedby_relation->get_target(0, &target));
  target.As(&ax_target);
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_child1), unique_id);
  ax_target.Reset();
  target.Reset();

  EXPECT_HRESULT_SUCCEEDED(describedby_relation->get_target(1, &target));
  target.As(&ax_target);
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_child2), unique_id);
  ax_target.Reset();
  target.Reset();
  describedby_relation.Reset();

  // Test the reverse relations.
  EXPECT_HRESULT_SUCCEEDED(ax_child1->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_child1->GetCOM()->get_relation(0, &description_for_relation));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", std::wstring(relation_type.Get()));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_target(0, &target));
  target.As(&ax_target);
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_root), unique_id);
  ax_target.Reset();
  target.Reset();
  description_for_relation.Reset();

  EXPECT_HRESULT_SUCCEEDED(ax_child2->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_child2->GetCOM()->get_relation(0, &description_for_relation));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", std::wstring(relation_type.Get()));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_target(0, &target));
  target.As(&ax_target);
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_root), unique_id);
  ax_target.Reset();
  target.Reset();

  // Try adding one more relation.
  std::vector<int32_t> labelledby_ids = {3};
  child1.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                             labelledby_ids);
  AXUpdatesAndEvents event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(child1);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  EXPECT_HRESULT_SUCCEEDED(ax_child1->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(2, n_relations);
  EXPECT_HRESULT_SUCCEEDED(ax_child2->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(2, n_relations);
}

}  // namespace ui
