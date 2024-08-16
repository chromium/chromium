// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_win.h"

#include <oleacc.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/views/accessibility/test_list_grid_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_base.h"

using base::win::ScopedBstr;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace views {
namespace test {

#define EXPECT_UIA_BOOL_EQ(node, property_id, expected)               \
  {                                                                   \
    ScopedVariant expectedVariant(expected);                          \
    ASSERT_EQ(VT_BOOL, expectedVariant.type());                       \
    ScopedVariant actual;                                             \
    ASSERT_HRESULT_SUCCEEDED(                                         \
        node->GetPropertyValue(property_id, actual.Receive()));       \
    EXPECT_EQ(expectedVariant.ptr()->boolVal, actual.ptr()->boolVal); \
  }

namespace {

// Whether |left| represents the same COM object as |right|.
template <typename T, typename U>
bool IsSameObject(T* left, U* right) {
  if (!left && !right)
    return true;

  if (!left || !right)
    return false;

  ComPtr<IUnknown> left_unknown;
  left->QueryInterface(IID_PPV_ARGS(&left_unknown));

  ComPtr<IUnknown> right_unknown;
  right->QueryInterface(IID_PPV_ARGS(&right_unknown));

  return left_unknown == right_unknown;
}

}  // namespace

class ViewAXPlatformNodeDelegateWinTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateWinTest() = default;
  ~ViewAXPlatformNodeDelegateWinTest() override = default;

 protected:
  void GetIAccessible2InterfaceForView(View* view, IAccessible2_2** result) {
    ComPtr<IAccessible> view_accessible(view->GetNativeViewAccessible());
    ComPtr<IServiceProvider> service_provider;
    ASSERT_EQ(S_OK, view_accessible.As(&service_provider));
    ASSERT_EQ(S_OK, service_provider->QueryService(IID_IAccessible2_2, result));
  }

  ComPtr<IRawElementProviderSimple> GetIRawElementProviderSimple(View* view) {
    ComPtr<IRawElementProviderSimple> result;
    EXPECT_HRESULT_SUCCEEDED(view->GetNativeViewAccessible()->QueryInterface(
        __uuidof(IRawElementProviderSimple), &result));
    return result;
  }

  ComPtr<IAccessible2> ToIAccessible2(ComPtr<IAccessible> accessible) {
    CHECK(accessible);
    ComPtr<IServiceProvider> service_provider;
    accessible.As(&service_provider);
    ComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2,
                                                   IID_PPV_ARGS(&result))));
    return result;
  }
};

TEST_F(ViewAXPlatformNodeDelegateWinTest, TextfieldAccessibility) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());

  Textfield* textfield = new Textfield;
  textfield->GetViewAccessibility().SetName(u"Name");
  textfield->SetText(u"Value");
  content->AddChildView(textfield);

  ComPtr<IAccessible> content_accessible(content->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  EXPECT_EQ(1, child_count);

  ComPtr<IDispatch> textfield_dispatch;
  ComPtr<IAccessible> textfield_accessible;
  ScopedVariant child_index(1);
  ASSERT_EQ(S_OK,
            content_accessible->get_accChild(child_index, &textfield_dispatch));
  ASSERT_EQ(S_OK, textfield_dispatch.As(&textfield_accessible));

  ASSERT_EQ(S_OK, textfield_accessible->get_accChildCount(&child_count));
  EXPECT_EQ(0, child_count)
      << "Text fields should be leaf nodes on this platform, otherwise no "
         "descendants will be recognized by assistive software.";

  ScopedBstr name;
  ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK,
            textfield_accessible->get_accName(childid_self, name.Receive()));
  EXPECT_STREQ(L"Name", name.Get());

  ScopedBstr value;
  ASSERT_EQ(S_OK,
            textfield_accessible->get_accValue(childid_self, value.Receive()));
  EXPECT_STREQ(L"Value", value.Get());

  ScopedBstr new_value(L"New value");
  ASSERT_EQ(S_OK,
            textfield_accessible->put_accValue(childid_self, new_value.Get()));
  EXPECT_EQ(u"New value", textfield->GetText());
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, TextfieldAssociatedLabel) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());

  Label* label = new Label(u"Label");
  content->AddChildView(label);
  Textfield* textfield = new Textfield;
  textfield->GetViewAccessibility().SetName(*label);
  content->AddChildView(textfield);

  ComPtr<IAccessible> content_accessible(content->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(2L, child_count);

  ComPtr<IDispatch> textfield_dispatch;
  ComPtr<IAccessible> textfield_accessible;
  ScopedVariant child_index(2);
  ASSERT_EQ(S_OK,
            content_accessible->get_accChild(child_index, &textfield_dispatch));
  ASSERT_EQ(S_OK, textfield_dispatch.As(&textfield_accessible));

  ScopedBstr name;
  ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK,
            textfield_accessible->get_accName(childid_self, name.Receive()));
  ASSERT_STREQ(L"Label", name.Get());

  ComPtr<IAccessible2_2> textfield_ia2;
  EXPECT_EQ(S_OK, textfield_accessible.As(&textfield_ia2));
  ScopedBstr type(IA2_RELATION_LABELLED_BY);
  IUnknown** targets;
  LONG n_targets;
  EXPECT_EQ(S_OK, textfield_ia2->get_relationTargetsOfType(
                      type.Get(), 0, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ComPtr<IUnknown> label_unknown(targets[0]);
  ComPtr<IAccessible> label_accessible;
  ASSERT_EQ(S_OK, label_unknown.As(&label_accessible));
  ScopedVariant role;
  EXPECT_EQ(S_OK, label_accessible->get_accRole(childid_self, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_STATICTEXT, V_I4(role.ptr()));
}

// A subclass of ViewAXPlatformNodeDelegateWinTest that we run twice,
// first where we create an transient child widget (child = false), the second
// time where we create a child widget (child = true).
class ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag
    : public ViewAXPlatformNodeDelegateWinTest,
      public testing::WithParamInterface<bool> {
 public:
  ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag() = default;
  ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag(
      const ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag&) = delete;
  ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag& operator=(
      const ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag&) = delete;
  ~ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag,
                         testing::Bool());

TEST_P(ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag, AuraChildWidgets) {
  // Create the parent widget.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 400, 200);
  widget->Init(std::move(init_params));
  widget->Show();

  // Initially it has 1 child.
  ComPtr<IAccessible> root_view_accessible(
      widget->GetRootView()->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  // Create the child widget, one of two ways (see below).
  auto child_widget = std::make_unique<Widget>();
  Widget::InitParams child_init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_BUBBLE);
  child_init_params.parent = widget->GetNativeView();
  child_init_params.bounds = gfx::Rect(30, 40, 100, 50);

  // NOTE: this test is run two times, GetParam() returns a different
  // value each time. The first time we test with child = false,
  // making this an owned widget (a transient child).  The second time
  // we test with child = true, making it a child widget->
  child_init_params.child = GetParam();

  child_widget->Init(std::move(child_init_params));
  child_widget->Show();

  // Now the IAccessible for the parent widget should have 2 children.
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(2L, child_count);

  // Ensure the bounds of the parent widget are as expected.
  ScopedVariant childid_self(CHILDID_SELF);
  LONG x, y, width, height;
  ASSERT_EQ(S_OK, root_view_accessible->accLocation(&x, &y, &width, &height,
                                                    childid_self));
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(400, width);
  EXPECT_EQ(200, height);

  // Get the IAccessible for the second child of the parent widget,
  // which should be the one for our child widget->
  ComPtr<IDispatch> child_widget_dispatch;
  ComPtr<IAccessible> child_widget_accessible;
  ScopedVariant child_index_2(2);
  ASSERT_EQ(S_OK, root_view_accessible->get_accChild(child_index_2,
                                                     &child_widget_dispatch));
  ASSERT_EQ(S_OK, child_widget_dispatch.As(&child_widget_accessible));

  // Check the bounds of the IAccessible for the child widget->
  // This is a sanity check to make sure we have the right object
  // and not some other view.
  ASSERT_EQ(S_OK, child_widget_accessible->accLocation(&x, &y, &width, &height,
                                                       childid_self));
  EXPECT_EQ(30, x);
  EXPECT_EQ(40, y);
  EXPECT_EQ(100, width);
  EXPECT_EQ(50, height);

  // Now make sure that querying the parent of the child gets us back to
  // the original parent.
  ComPtr<IDispatch> child_widget_parent_dispatch;
  ComPtr<IAccessible> child_widget_parent_accessible;
  ASSERT_EQ(S_OK, child_widget_accessible->get_accParent(
                      &child_widget_parent_dispatch));
  ASSERT_EQ(S_OK,
            child_widget_parent_dispatch.As(&child_widget_parent_accessible));
  EXPECT_EQ(root_view_accessible.Get(), child_widget_parent_accessible.Get());
}

// Flaky on Windows: https://crbug.com/461837.
TEST_F(ViewAXPlatformNodeDelegateWinTest, DISABLED_RetrieveAllAlerts) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());

  View* infobar = new View;
  content->AddChildView(infobar);

  View* infobar2 = new View;
  content->AddChildView(infobar2);

  View* root_view = content->parent();
  ASSERT_EQ(nullptr, root_view->parent());

  ComPtr<IAccessible2_2> root_view_accessible;
  GetIAccessible2InterfaceForView(root_view, &root_view_accessible);

  ComPtr<IAccessible2_2> infobar_accessible;
  GetIAccessible2InterfaceForView(infobar, &infobar_accessible);

  ComPtr<IAccessible2_2> infobar2_accessible;
  GetIAccessible2InterfaceForView(infobar2, &infobar2_accessible);

  // Initially, there are no alerts
  ScopedBstr alerts_bstr(L"alerts");
  IUnknown** targets;
  LONG n_targets;
  ASSERT_EQ(S_FALSE, root_view_accessible->get_relationTargetsOfType(
                         alerts_bstr.Get(), 0, &targets, &n_targets));
  ASSERT_EQ(0, n_targets);

  // Fire alert events on the infobars.
  infobar->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  infobar2->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);

  // Now calling get_relationTargetsOfType should retrieve the alerts.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr.Get(), 0, &targets, &n_targets));
  ASSERT_EQ(2, n_targets);
  {
    // SAFETY: get_relationTargetsOfType() is a COM interface which guarantees
    // that exactly n_targets pointers are available starting at targets.
    UNSAFE_BUFFERS(base::span<IUnknown*> targets_span(
                       targets, base::checked_cast<size_t>(n_targets));)
    ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets_span[0]));
    ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets_span[1]));
  }
  CoTaskMemFree(targets);

  // If we set max_targets to 1, we should only get the first one.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr.Get(), 1, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);

  // If we delete the first view, we should only get the second one now.
  delete infobar;
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr.Get(), 0, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);
}

// Test trying to retrieve child widgets during window close does not crash.
TEST_F(ViewAXPlatformNodeDelegateWinTest, GetAllOwnedWidgetsCrash) {
  Widget widget;
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(init_params));
  widget.CloseNow();

  LONG child_count = 0;
  ComPtr<IAccessible> content_accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  EXPECT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  EXPECT_EQ(1L, child_count);
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, WindowHasRoleApplication) {
  // We expect that our internal window object does not expose
  // ROLE_SYSTEM_WINDOW, but ROLE_SYSTEM_PANE instead.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(init_params));

  ComPtr<IAccessible> accessible(
      widget->GetRootView()->GetNativeViewAccessible());
  ScopedVariant childid_self(CHILDID_SELF);
  ScopedVariant role;
  EXPECT_EQ(S_OK, accessible->get_accRole(childid_self, role.Receive()));
  EXPECT_EQ(VT_I4, role.type());
  EXPECT_EQ(ROLE_SYSTEM_PANE, V_I4(role.ptr()));
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, Overrides) {
  // We expect that our internal window object does not expose
  // ROLE_SYSTEM_WINDOW, but ROLE_SYSTEM_PANE instead.
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* contents_view = widget->SetContentsView(std::make_unique<View>());

  View* alert_view = new ScrollView;
  alert_view->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
  alert_view->GetViewAccessibility().SetName(u"Name",
                                             ax::mojom::NameFrom::kAttribute);
  alert_view->GetViewAccessibility().SetDescription("Description");
  alert_view->GetViewAccessibility().SetIsLeaf(true);
  contents_view->AddChildView(alert_view);

  // Descendant should be ignored because the parent uses SetIsLeaf().
  View* ignored_descendant = new View;
  alert_view->AddChildView(ignored_descendant);

  ComPtr<IAccessible> content_accessible(
      contents_view->GetNativeViewAccessible());
  ScopedVariant child_index(1);

  // Role.
  ScopedVariant role;
  EXPECT_EQ(S_OK, content_accessible->get_accRole(child_index, role.Receive()));
  EXPECT_EQ(VT_I4, role.type());
  EXPECT_EQ(ROLE_SYSTEM_ALERT, V_I4(role.ptr()));

  // Name.
  ScopedBstr name;
  ASSERT_EQ(S_OK, content_accessible->get_accName(child_index, name.Receive()));
  ASSERT_STREQ(L"Name", name.Get());

  // Description.
  ScopedBstr description;
  ASSERT_EQ(S_OK, content_accessible->get_accDescription(
                      child_index, description.Receive()));
  ASSERT_STREQ(L"Description", description.Get());

  // Get the child accessible.
  ComPtr<IDispatch> alert_dispatch;
  ComPtr<IAccessible> alert_accessible;
  ASSERT_EQ(S_OK,
            content_accessible->get_accChild(child_index, &alert_dispatch));
  ASSERT_EQ(S_OK, alert_dispatch.As(&alert_accessible));

  // Child accessible is a leaf.
  LONG child_count = 0;
  ASSERT_EQ(S_OK, alert_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(0, child_count);

  ComPtr<IDispatch> child_dispatch;
  ASSERT_EQ(E_INVALIDARG,
            alert_accessible->get_accChild(child_index, &child_dispatch));
  ASSERT_EQ(child_dispatch.Get(), nullptr);
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, GridRowColumnCount) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());
  TestListGridView* grid = new TestListGridView();
  content->AddChildView(grid);

  Microsoft::WRL::ComPtr<IGridProvider> grid_provider;
  EXPECT_HRESULT_SUCCEEDED(
      grid->GetViewAccessibility().GetNativeObject()->QueryInterface(
          __uuidof(IGridProvider), &grid_provider));

  // If set, aria row/column count takes precedence over table row/column count.
  // Expect E_UNEXPECTED if the result is kUnknownAriaColumnOrRowCount (-1) or
  // if neither is set.
  int row_count;
  int column_count;

  // aria row/column count = not set
  // table row/column count = not set
  grid->UnsetAriaTableSize();
  grid->UnsetTableSize();
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_RowCount(&row_count));
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(0, row_count);
  EXPECT_EQ(0, column_count);
  // To do still: When nothing is set, currently
  // AXPlatformNodeDelegateBase::GetTable{Row/Col}Count() returns 0 Should it
  // return std::nullopt if the attribute is not set? Like
  // GetTableAria{Row/Col}Count()
  // EXPECT_EQ(E_UNEXPECTED, grid_provider->get_RowCount(&row_count));

  // aria row/column count = 2
  // table row/column count = not set
  grid->SetAriaTableSize(2, 2);
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_RowCount(&row_count));
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(2, row_count);
  EXPECT_EQ(2, column_count);

  // aria row/column count = kUnknownAriaColumnOrRowCount
  // table row/column count = not set
  grid->SetAriaTableSize(ax::mojom::kUnknownAriaColumnOrRowCount,
                         ax::mojom::kUnknownAriaColumnOrRowCount);
  EXPECT_EQ(E_UNEXPECTED, grid_provider->get_RowCount(&row_count));
  EXPECT_EQ(E_UNEXPECTED, grid_provider->get_ColumnCount(&column_count));

  // aria row/column count = 3
  // table row/column count = 4
  grid->SetAriaTableSize(3, 3);
  grid->SetTableSize(4, 4);
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_RowCount(&row_count));
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(3, row_count);
  EXPECT_EQ(3, column_count);

  // aria row/column count = not set
  // table row/column count = 4
  grid->UnsetAriaTableSize();
  grid->SetTableSize(4, 4);
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_RowCount(&row_count));
  EXPECT_HRESULT_SUCCEEDED(grid_provider->get_ColumnCount(&column_count));
  EXPECT_EQ(4, row_count);
  EXPECT_EQ(4, column_count);

  // aria row/column count = not set
  // table row/column count = kUnknownAriaColumnOrRowCount
  grid->SetTableSize(ax::mojom::kUnknownAriaColumnOrRowCount,
                     ax::mojom::kUnknownAriaColumnOrRowCount);
  EXPECT_EQ(E_UNEXPECTED, grid_provider->get_RowCount(&row_count));
  EXPECT_EQ(E_UNEXPECTED, grid_provider->get_ColumnCount(&column_count));
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, IsUIAControlIsTrueEvenWhenReadonly) {
  // This test ensures that the value returned by
  // AXPlatformNodeWin::IsUIAControl returns true even if the element is
  // read-only. The previous implementation was incorrect and used to return
  // false for read-only views, causing all sorts of issues with ATs.
  //
  // Since we can't test IsUIAControl directly, we go through the
  // UIA_IsControlElementPropertyId, which is computed using IsUIAControl.

  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());

  Textfield* text_field = new Textfield();
  text_field->SetReadOnly(true);
  content->AddChildView(text_field);

  ComPtr<IRawElementProviderSimple> textfield_provider =
      GetIRawElementProviderSimple(text_field);
  EXPECT_UIA_BOOL_EQ(textfield_provider, UIA_IsControlElementPropertyId, true);
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, TextPositionAt) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));

  View* content = widget->SetContentsView(std::make_unique<View>());

  Label* label = new Label(u"Label's Name");
  content->AddChildView(label);
  label->GetViewAccessibility().EnsureAtomicViewAXTreeManager();
  ViewAXPlatformNodeDelegate* label_accessibility =
      static_cast<ViewAXPlatformNodeDelegate*>(&label->GetViewAccessibility());
  label_accessibility->GetData();

  ui::AXNodePosition::AXPositionInstance actual_position =
      label_accessibility->CreateTextPositionAt(
          0, ax::mojom::TextAffinity::kDownstream);
  EXPECT_NE(nullptr, actual_position.get());
  EXPECT_EQ(0, actual_position->text_offset());
  EXPECT_EQ(u"Label's Name", actual_position->GetText());
}

//
// TableView tests.
//

namespace {
class TestTableModel : public ui::TableModel {
 public:
  TestTableModel() = default;

  TestTableModel(const TestTableModel&) = delete;
  TestTableModel& operator=(const TestTableModel&) = delete;

  // ui::TableModel:
  size_t RowCount() override { return 3; }

  std::u16string GetText(size_t row, int column_id) override {
    constexpr std::array<std::array<const char* const, 5>, 3> cells = {{
        {{"Australia", "24,584,620", "1,323,421,072,479"}},
        {{"Spain", "46,647,428", "1,314,314,164,402"}},
        {{"Nigeria", "190.873,244", "375,745,486,521"}},
    }};

    return base::ASCIIToUTF16(cells[row % 5][column_id]);
  }

  void SetObserver(ui::TableModelObserver* observer) override {}
};
}  // namespace

class ViewAXPlatformNodeDelegateWinTableTest
    : public ViewAXPlatformNodeDelegateWinTest {
  void SetUp() override {
    ViewAXPlatformNodeDelegateWinTest::SetUp();

    std::vector<ui::TableColumn> columns;
    columns.push_back(TestTableColumn(0, "Country"));
    columns.push_back(TestTableColumn(1, "Population"));
    columns.push_back(TestTableColumn(2, "GDP"));

    model_ = std::make_unique<TestTableModel>();
    auto table = std::make_unique<TableView>(model_.get(), columns,
                                             TableType::kTextOnly, true);
    table_ = table.get();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams init_params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    init_params.bounds = gfx::Rect(0, 0, 400, 400);
    widget_->Init(std::move(init_params));

    View* content = widget_->SetContentsView(std::make_unique<View>());
    content->AddChildView(
        TableView::CreateScrollViewWithTable(std::move(table)));
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewAXPlatformNodeDelegateWinTest::TearDown();
  }

  ui::TableColumn TestTableColumn(int id, const std::string& title) {
    ui::TableColumn column;
    column.id = id;
    column.title = base::ASCIIToUTF16(title.c_str());
    column.sortable = true;
    return column;
  }

 protected:
  std::unique_ptr<TestTableModel> model_;
  std::unique_ptr<Widget> widget_;
  raw_ptr<TableView> table_ = nullptr;  // Owned by parent.
};

TEST_F(ViewAXPlatformNodeDelegateWinTableTest, TableCellAttributes) {
  ComPtr<IAccessible2_2> table_accessible;
  GetIAccessible2InterfaceForView(table_, &table_accessible);

  auto get_attributes = [&](int row_child, int cell_child) -> std::wstring {
    ComPtr<IDispatch> row_dispatch;
    CHECK_EQ(S_OK, table_accessible->get_accChild(ScopedVariant(row_child),
                                                  &row_dispatch));
    ComPtr<IAccessible> row;
    CHECK_EQ(S_OK, row_dispatch.As(&row));
    ComPtr<IAccessible2> ia2_row = ToIAccessible2(row);

    ComPtr<IDispatch> cell_dispatch;
    CHECK_EQ(S_OK,
             row->get_accChild(ScopedVariant(cell_child), &cell_dispatch));
    ComPtr<IAccessible> cell;
    CHECK_EQ(S_OK, cell_dispatch.As(&cell));
    ComPtr<IAccessible2> ia2_cell = ToIAccessible2(cell);

    ScopedBstr attributes_bstr;
    CHECK_EQ(S_OK, ia2_cell->get_attributes(attributes_bstr.Receive()));
    std::wstring attributes(attributes_bstr.Get());
    return attributes;
  };

  // These strings should NOT contain rowindex or colindex, since those
  // imply an ARIA override.
  EXPECT_EQ(
      get_attributes(1, 1),
      L"name-from:attribute;explicit-name:true;sort:none;class:AXVirtualView;");
  EXPECT_EQ(
      get_attributes(1, 2),
      L"name-from:attribute;explicit-name:true;sort:none;class:AXVirtualView;");
  EXPECT_EQ(get_attributes(2, 1),
            L"hidden:true;name-from:attribute;explicit-name:true;class:"
            L"AXVirtualView;");
  EXPECT_EQ(get_attributes(2, 2),
            L"hidden:true;name-from:attribute;explicit-name:true;class:"
            L"AXVirtualView;");
}

}  // namespace test

// Needs to be in the views namespace.
class ViewAXPlatformNodeDelegateWinInnerTextRangeTest
    : public test::ViewAXPlatformNodeDelegateWinTest {
 public:
  void SetUp() override {
    ViewAXPlatformNodeDelegateWinTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(features::kUiaProvider);

    widget_ = std::make_unique<Widget>();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));

    textfield_ = new Textfield();
    textfield_->SetBounds(0, 0, 100, 40);
    widget_->GetContentsView()->AddChildView(textfield_.get());

    TextfieldTestApi textfield_test_api(textfield_);
    textfield_test_api.GetRenderText()->set_glyph_width_for_test(5);
    textfield_test_api.GetRenderText()->set_glyph_height_for_test(8);

    label_ = new Label();
    widget_->GetContentsView()->AddChildView(label_.get());

    // TODO(crbug.com/40924888): This is not obvious, but the
    // AtomicViewAXTreeManager gets initialized from this GetData() call. This
    // won't be needed anymore once we finish the ViewsAX project and remove the
    // temporary solution.
    textfield_delegate()->GetData();
    CHECK(textfield_delegate()->GetAtomicViewAXTreeManagerForTesting());

    label_delegate()->GetData();
    CHECK(label_delegate()->GetAtomicViewAXTreeManagerForTesting());
  }

  void TearDown() override {
    textfield_ = nullptr;
    label_ = nullptr;
    if (!widget_->IsClosed()) {
      widget_->Close();
    }
    ViewsTestBase::TearDown();
  }

  ViewAXPlatformNodeDelegate* textfield_delegate() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &textfield_->GetViewAccessibility());
  }
  ViewAXPlatformNodeDelegate* label_delegate() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &label_->GetViewAccessibility());
  }

 protected:
  raw_ptr<Textfield> textfield_ = nullptr;  // Owned by views hierarchy.
  raw_ptr<Label> label_ = nullptr;          // Owned by views hierarchy.
  std::unique_ptr<Widget> widget_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
       EmptyTextfield_NonEmptyRect) {
  ui::AXOffscreenResult offscreen_result;

  gfx::Rect textfield_bounds = gfx::Rect(0, 0, 50, 100);
  textfield_->SetBoundsRect(textfield_bounds);
  gfx::Insets insets = textfield_->GetInsets();

  // An empty text field should expose bounds with a fixed width of 1.
  gfx::Rect bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      0, 1, ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(insets.left(), insets.top(), 1,
                textfield_bounds.height() - insets.top() - insets.bottom()),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest, EmptyLabel_EmptyRect) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  // An empty text field should expose bounds with a fixed width of 1.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      0, 1, ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest, Textfield_LTR) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  // This string contains a glyph formed of 3 codepoints (the middle up, the
  // thumbs up emoji). This test validates that we expose the bounds of
  // individual glyphs, not codepoints.
  const char16_t kText[] = u"a\U0001F44D\uFE0Fb";
  constexpr gfx::Range kRange1 = gfx::Range(0, 1);  // Range of character 'a'.
  constexpr gfx::Range kRange2 =
      gfx::Range(1, 2);  // Range of the middle glyph.
  constexpr gfx::Range kRange3 = gfx::Range(2, 3);  // Range of character 'b'.
  constexpr gfx::Range kRange4 = gfx::Range(0, 3);  // Range of the entire text.

  constexpr int kGlyphWidth = 5;
  gfx::Rect textfield_bounds = gfx::Rect(0, 0, 10 * kGlyphWidth, 100);
  textfield_->SetBoundsRect(textfield_bounds);
  gfx::Insets insets = textfield_->GetInsets();

  textfield_->SetText(kText);
  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  textfield_delegate()->GetData();

  int height = textfield_bounds.height() - insets.top() - insets.bottom();
  int initial_x = 2 * insets.left();

  // Range 1: 'a'.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange1.start(), kRange1.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x, insets.top(), kGlyphWidth, height), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 2: middle glyph.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(initial_x + kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 3: 'b'.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange3.start(), kRange3.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(initial_x + 2 * kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 4: all text.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange4.start(), kRange4.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x, insets.top(), 3 * kGlyphWidth, height),
            bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
       Textfield_TextOverflow) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  // This string contains text that is too long to fit in the textfield.
  const char16_t kText[] = u"3.1415926535897932384626433832795";

  constexpr int kGlyphWidth = 5;
  // The textfield is 5 glyphs wide, so the text will overflow.
  gfx::Rect textfield_bounds = gfx::Rect(0, 0, 5 * kGlyphWidth, 100);
  textfield_->SetBoundsRect(textfield_bounds);
  gfx::Insets insets = textfield_->GetInsets();

  textfield_->SetText(kText);
  ui::AXNodeID id = textfield_delegate()->GetData().id;

  // Initialize the textfield's scroll offset to 0.
  ui::AXActionData set_selection_action_data_1;
  set_selection_action_data_1.action = ax::mojom::Action::kSetSelection;
  set_selection_action_data_1.anchor_node_id = id;
  set_selection_action_data_1.focus_node_id = id;
  set_selection_action_data_1.focus_offset = 0;
  set_selection_action_data_1.anchor_offset = 0;
  textfield_delegate()->AccessibilityPerformAction(set_selection_action_data_1);
  EXPECT_EQ(textfield_delegate()->GetData().GetIntAttribute(
                ax::mojom::IntAttribute::kScrollX),
            0);

  int height = textfield_bounds.height() - insets.top() - insets.bottom();
  int initial_x = 2 * insets.left();

  // 1. Check the bounds of the first 5 characters. They are on screen.
  constexpr gfx::Range kRange1 = gfx::Range(0, 5);
  // The expected width is as follows because we clip bounds to the container.
  int expected_width = textfield_bounds.width() - insets.left();
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange1.start(), kRange1.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x, insets.top(), expected_width, height), bounds);

  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // 2. Check the bounds of the last character. It's offscreen, because the
  // scroll offset is still 0 for now.
  constexpr size_t text_length = std::size(kText) - 1;
  constexpr gfx::Range kRange2 = gfx::Range(text_length - 1, text_length);
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(insets.left(), insets.top(), 0, 0), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOffscreen);

  // 3. Set the selection to the last character. This will scroll the textfield
  // and it should be onscreen now.
  // Perform the scroll.
  ui::AXActionData set_selection_action_data_2;
  set_selection_action_data_2.action = ax::mojom::Action::kSetSelection;
  set_selection_action_data_2.anchor_node_id = id;
  set_selection_action_data_2.focus_node_id = id;
  set_selection_action_data_2.focus_offset = kRange2.start();
  set_selection_action_data_2.anchor_offset = kRange2.end();
  textfield_delegate()->AccessibilityPerformAction(set_selection_action_data_2);
  int scroll_x = textfield_delegate()->GetData().GetIntAttribute(
      ax::mojom::IntAttribute::kScrollX);
  EXPECT_LT(scroll_x, 0);

  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  textfield_delegate()->GetData();

  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x + kRange2.start() * kGlyphWidth + scroll_x,
                      insets.top(), kGlyphWidth, height),
            bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest, Label_LTR) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  constexpr int kGlyphWidth = 5;
  constexpr int kGlyphHeight = 8;
  const char16_t kText[] = u"a\U0001F44D\uFE0Fb";
  constexpr gfx::Range kRange1 = gfx::Range(0, 1);  // Range of character 'a'.
  constexpr gfx::Range kRange2 = gfx::Range(1, 2);  // Range of the emoji.
  constexpr gfx::Range kRange3 = gfx::Range(2, 3);  // Range of character 'b'.
  constexpr gfx::Range kRange4 = gfx::Range(0, 3);  // Range of the entire text.

  label_->SetText(kText);
  label_->SetBoundsRect(gfx::Rect(0, 0, 10 * kGlyphWidth, 100));
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Since the glyph size can vary from one machine to another, we force set a
  // fixed glyph size to make the test deterministic and not flaky.
  label_->MaybeBuildDisplayText();
  gfx::test::RenderTextTestApi render_text_test_api(
      label_->display_text_.get());
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  render_text_test_api.SetGlyphHeight(kGlyphHeight);

  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  label_delegate()->GetData();

  // Range 1: 'a'.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange1.start(), kRange1.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(0, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 2: middle glyph.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(5, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 3: 'b'.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange3.start(), kRange3.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(10, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 4: all text.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange4.start(), kRange4.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(0, 0, 15, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest, Textfield_RTL) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  const char16_t kText[] = u"اللغة ";
  constexpr gfx::Range kRange1 = gfx::Range(0, 1);
  constexpr gfx::Range kRange2 = gfx::Range(1, 2);
  constexpr gfx::Range kRange3 = gfx::Range(2, 3);
  constexpr gfx::Range kRange4 = gfx::Range(3, 4);
  constexpr gfx::Range kRange5 = gfx::Range(4, 5);
  constexpr gfx::Range kRange6 = gfx::Range(0, 5);

  base::i18n::SetRTLForTesting(true);

  constexpr int kGlyphWidth = 5;
  gfx::Rect textfield_bounds = gfx::Rect(0, 0, 15 * kGlyphWidth, 100);
  textfield_->SetBoundsRect(textfield_bounds);
  gfx::Insets insets = textfield_->GetInsets();
  textfield_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  textfield_->SetText(kText);
  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  textfield_delegate()->GetData();

  int height = textfield_bounds.height() - insets.top() - insets.bottom();
  // TODO(accessibility): The initial x offset should be the result of an
  // operation that takes into account the left insets, but because of
  // https://crbug.com/1508209, it happens to be consistently 11 on all try
  // bots. If this test starts failings, please reach out to
  // benjamin.beaudry@microsoft.com.
  //
  // 11 comes from 10 for the horizontal insets + 1 for the cursor width.
  int initial_x = widget_->GetWindowBoundsInScreen().width() - 11;

  // Range 1.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange1.start(), kRange1.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x, insets.top(), kGlyphWidth, height), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 2.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(initial_x - kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 3.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange3.start(), kRange3.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(initial_x - 2 * kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 4.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange4.start(), kRange4.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(initial_x - 3 * kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 5.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange5.start(), kRange5.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  // TODO(accessibility): The x offset here should be smaller than the one of
  // the previous glyph, but it's not. Investigate.
  EXPECT_EQ(
      gfx::Rect(initial_x - 4 * kGlyphWidth, insets.top(), kGlyphWidth, height),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 6.
  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      kRange6.start(), kRange6.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(initial_x - 4 * kGlyphWidth, insets.top(),
                      5 * kGlyphWidth, height),
            bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest, Label_RTL) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  constexpr int kGlyphWidth = 5;
  constexpr int kGlyphHeight = 8;
  const char16_t kText[] = u"اللغة ";
  constexpr gfx::Range kRange1 = gfx::Range(0, 1);
  constexpr gfx::Range kRange2 = gfx::Range(1, 2);
  constexpr gfx::Range kRange3 = gfx::Range(2, 3);
  constexpr gfx::Range kRange4 = gfx::Range(3, 4);
  constexpr gfx::Range kRange5 = gfx::Range(4, 5);
  constexpr gfx::Range kRange6 = gfx::Range(0, 5);

  base::i18n::SetRTLForTesting(true);

  label_->SetText(kText);
  label_->SetBoundsRect(gfx::Rect(0, 0, 10 * kGlyphWidth, 100));
  label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  // Since the glyph size can vary from one machine to another, we force set a
  // fixed glyph size to make the test deterministic and not flaky.
  label_->MaybeBuildDisplayText();
  gfx::test::RenderTextTestApi render_text_test_api(
      label_->display_text_.get());
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  render_text_test_api.SetGlyphHeight(kGlyphHeight);

  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  label_delegate()->GetData();

  // Range 1.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange1.start(), kRange1.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(170, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 2.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange2.start(), kRange2.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(165, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 3.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange3.start(), kRange3.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(160, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 4.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange4.start(), kRange4.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(155, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 5.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange5.start(), kRange5.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  // TODO(accessibility): The x offset here should be greater than the one of
  // the previous range, the previous glyph, but it's not. Investigate.
  EXPECT_EQ(gfx::Rect(150, 0, 5, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);

  // Range 6.
  bounds = label_delegate()->GetInnerTextRangeBoundsRect(
      kRange6.start(), kRange6.end(), ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(gfx::Rect(150, 0, 25, 100), bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
       Textfield_CreatePositionAt) {
  const std::u16string kText = u"text";
  textfield_->SetText(kText);

  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  ui::AXNodeData data = textfield_delegate()->GetData();

  ui::AXNodeID expected_node_id = data.id;
  ui::AXNodePosition::AXPositionInstance position;

  // 1. Validate that we can create a position at the beginning of the text.
  position = textfield_delegate()->CreatePositionAt(
      0, ax::mojom::TextAffinity::kDownstream);
  EXPECT_EQ(0, position->text_offset());
  EXPECT_EQ(expected_node_id, position->anchor_id());
  EXPECT_TRUE(position->IsTextPosition());

  // 2. Validate that we can create a position at the end of the text.
  position = textfield_delegate()->CreatePositionAt(
      kText.length(), ax::mojom::TextAffinity::kDownstream);
  EXPECT_EQ(kText.length(), static_cast<size_t>(position->text_offset()));
  EXPECT_EQ(expected_node_id, position->anchor_id());
  EXPECT_TRUE(position->IsTextPosition());

  // TODO(accessibility): Uncomment once https://crbug.com/1404289 is fixed.
  // // 3. Validate that we can't create a position at an invalid offset.
  // position = textfield_delegate()->CreatePositionAt(kText.length() + 1,
  // ax::mojom::TextAffinity::kDownstream); LOG(INFO) << position->ToString();
  // EXPECT_TRUE(position->IsNullPosition());

  // // 4. Clear the text and validate that we can't create a position.
  // position = textfield_delegate()->CreatePositionAt(0,
  // ax::mojom::TextAffinity::kDownstream);
  // EXPECT_TRUE(position->IsNullPosition());
}

TEST_F(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
       Textfield_ScreenPhysicalPixels) {
  ui::AXOffscreenResult offscreen_result;
  gfx::Rect bounds;

  base::i18n::SetRTLForTesting(false);

  constexpr int kGlyphWidth = 5;

  gfx::RenderText* render_text = TextfieldTestApi(textfield_).GetRenderText();
  render_text->set_glyph_width_for_test(5);
  gfx::Rect textfield_bounds = gfx::Rect(0, 0, 10 * kGlyphWidth, 100);
  textfield_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  textfield_->SetBoundsRect(textfield_bounds);
  gfx::Insets insets = textfield_->GetInsets();
  const char16_t kText[] = u"a";

  textfield_->SetText(kText);

  // TODO(crbug.com/40924888): This is not obvious, but we need to call
  // `GetData` to refresh the text offsets and accessible name. This won't be
  // needed anymore once we finish the ViewsAX project and remove the temporary
  // solution.
  textfield_delegate()->GetData();

  bounds = textfield_delegate()->GetInnerTextRangeBoundsRect(
      0, 1, ui::AXCoordinateSystem::kScreenPhysicalPixels,
      ui::AXClippingBehavior::kClipped, &offscreen_result);
  EXPECT_EQ(
      gfx::Rect(2 * insets.left(), 0 + insets.top(), kGlyphWidth,
                textfield_bounds.height() - insets.top() - insets.bottom()),
      bounds);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

}  // namespace views
