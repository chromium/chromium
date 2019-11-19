// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_win.h"

#include <oleacc.h>
#include <wrl/client.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

using Microsoft::WRL::ComPtr;
using base::win::ScopedBstr;
using base::win::ScopedVariant;

namespace views {
namespace test {

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
};

TEST_F(ViewAXPlatformNodeDelegateWinTest, TextfieldAccessibility) {
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  View* content = new View;
  widget.SetContentsView(content);

  Textfield* textfield = new Textfield;
  textfield->SetAccessibleName(L"Name");
  textfield->SetText(L"Value");
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

  ScopedBstr name;
  ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK,
            textfield_accessible->get_accName(childid_self, name.Receive()));
  EXPECT_STREQ(L"Name", static_cast<BSTR>(name));

  ScopedBstr value;
  ASSERT_EQ(S_OK,
            textfield_accessible->get_accValue(childid_self, value.Receive()));
  EXPECT_STREQ(L"Value", static_cast<BSTR>(value));

  ScopedBstr new_value(L"New value");
  ASSERT_EQ(S_OK, textfield_accessible->put_accValue(childid_self, new_value));
  EXPECT_STREQ(L"New value", textfield->GetText().c_str());
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, TextfieldAssociatedLabel) {
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  View* content = new View;
  widget.SetContentsView(content);

  Label* label = new Label(L"Label");
  content->AddChildView(label);
  Textfield* textfield = new Textfield;
  textfield->SetAssociatedLabel(label);
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
  ASSERT_STREQ(L"Label", name);

  ComPtr<IAccessible2_2> textfield_ia2;
  EXPECT_EQ(S_OK, textfield_accessible.As(&textfield_ia2));
  ScopedBstr type(IA2_RELATION_LABELLED_BY);
  IUnknown** targets;
  LONG n_targets;
  EXPECT_EQ(S_OK, textfield_ia2->get_relationTargetsOfType(type, 0, &targets,
                                                           &n_targets));
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

INSTANTIATE_TEST_SUITE_P(,
                         ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag,
                         testing::Bool());

TEST_P(ViewAXPlatformNodeDelegateWinTestWithBoolChildFlag, AuraChildWidgets) {
  // Create the parent widget.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(0, 0, 400, 200);
  widget.Init(std::move(init_params));
  widget.Show();

  // Initially it has 1 child.
  ComPtr<IAccessible> root_view_accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  // Create the child widget, one of two ways (see below).
  Widget child_widget;
  Widget::InitParams child_init_params =
      CreateParams(Widget::InitParams::TYPE_BUBBLE);
  child_init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_init_params.parent = widget.GetNativeView();
  child_init_params.bounds = gfx::Rect(30, 40, 100, 50);

  // NOTE: this test is run two times, GetParam() returns a different
  // value each time. The first time we test with child = false,
  // making this an owned widget (a transient child).  The second time
  // we test with child = true, making it a child widget.
  child_init_params.child = GetParam();

  child_widget.Init(std::move(child_init_params));
  child_widget.Show();

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
  // which should be the one for our child widget.
  ComPtr<IDispatch> child_widget_dispatch;
  ComPtr<IAccessible> child_widget_accessible;
  ScopedVariant child_index_2(2);
  ASSERT_EQ(S_OK, root_view_accessible->get_accChild(child_index_2,
                                                     &child_widget_dispatch));
  ASSERT_EQ(S_OK, child_widget_dispatch.As(&child_widget_accessible));

  // Check the bounds of the IAccessible for the child widget.
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
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  View* content = new View;
  widget.SetContentsView(content);

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
  long n_targets;
  ASSERT_EQ(S_FALSE, root_view_accessible->get_relationTargetsOfType(
                         alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(0, n_targets);

  // Fire alert events on the infobars.
  infobar->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  infobar2->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);

  // Now calling get_relationTargetsOfType should retrieve the alerts.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(2, n_targets);
  ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets[0]));
  ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets[1]));
  CoTaskMemFree(targets);

  // If we set max_targets to 1, we should only get the first one.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr, 1, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);

  // If we delete the first view, we should only get the second one now.
  delete infobar;
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
                      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);
}

// Test trying to retrieve child widgets during window close does not crash.
TEST_F(ViewAXPlatformNodeDelegateWinTest, GetAllOwnedWidgetsCrash) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  ComPtr<IAccessible> accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  ScopedVariant childid_self(CHILDID_SELF);
  ScopedVariant role;
  EXPECT_EQ(S_OK, accessible->get_accRole(childid_self, role.Receive()));
  EXPECT_EQ(VT_I4, role.type());
  EXPECT_EQ(ROLE_SYSTEM_PANE, V_I4(role.ptr()));
}

TEST_F(ViewAXPlatformNodeDelegateWinTest, Overrides) {
  // We expect that our internal window object does not expose
  // ROLE_SYSTEM_WINDOW, but ROLE_SYSTEM_PANE instead.
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  View* contents_view = new View;
  widget.SetContentsView(contents_view);

  View* alert_view = new ScrollView;
  alert_view->GetViewAccessibility().OverrideRole(ax::mojom::Role::kAlert);
  alert_view->GetViewAccessibility().OverrideName(L"Name");
  alert_view->GetViewAccessibility().OverrideDescription("Description");
  alert_view->GetViewAccessibility().OverrideIsLeaf(true);
  contents_view->AddChildView(alert_view);

  // Descendant should be ignored because the parent uses OverrideIsLeaf().
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
  ASSERT_STREQ(L"Name", name);

  // Description.
  ScopedBstr description;
  ASSERT_EQ(S_OK, content_accessible->get_accDescription(
                      child_index, description.Receive()));
  ASSERT_STREQ(L"Description", description);

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
}  // namespace test
}  // namespace views
