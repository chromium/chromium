// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_remote_host.h"

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/mus/mus_client_test_api.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace {

// Returns a well-known tree ID for the test widget.
const ui::AXTreeID& TestAXTreeID() {
  static const base::NoDestructor<ui::AXTreeID> test_ax_tree_id(
      ui::AXTreeID::FromString("123"));
  return *test_ax_tree_id;
}

// Simulates the AXHostService in the browser.
class TestAXHostService : public ax::mojom::AXHost {
 public:
  explicit TestAXHostService(bool automation_enabled)
      : automation_enabled_(automation_enabled) {}
  ~TestAXHostService() override = default;

  ax::mojom::AXHostPtr CreateInterfacePtr() {
    ax::mojom::AXHostPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void ResetCounts() {
    remote_host_count_ = 0;
    event_count_ = 0;
    last_tree_id_ = ui::AXTreeIDUnknown();
    last_updates_.clear();
    last_event_ = ui::AXEvent();
  }

  // ax::mojom::AXHost:
  void RegisterRemoteHost(ax::mojom::AXRemoteHostPtr client,
                          RegisterRemoteHostCallback cb) override {
    ++remote_host_count_;
    std::move(cb).Run(TestAXTreeID(), automation_enabled_);
    client.FlushForTesting();
  }
  void HandleAccessibilityEvent(const ui::AXTreeID& tree_id,
                                const std::vector<ui::AXTreeUpdate>& updates,
                                const ui::AXEvent& event) override {
    ++event_count_;
    last_tree_id_ = ui::AXTreeID::FromString(tree_id);
    last_updates_ = updates;
    last_event_ = event;
  }

  mojo::Binding<ax::mojom::AXHost> binding_{this};
  bool automation_enabled_ = false;
  int remote_host_count_ = 0;
  int event_count_ = 0;
  ui::AXTreeID last_tree_id_ = ui::AXTreeIDUnknown();
  std::vector<ui::AXTreeUpdate> last_updates_;
  ui::AXEvent last_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAXHostService);
};

// TestView senses accessibility actions.
class TestView : public View {
 public:
  TestView() { set_owned_by_client(); }
  ~TestView() override = default;

  void ResetCounts() {
    action_count_ = 0;
    last_action_ = {};
    event_count_ = 0;
    last_event_type_ = ax::mojom::Event::kNone;
  }

  // View:
  bool HandleAccessibleAction(const ui::AXActionData& action) override {
    ++action_count_;
    last_action_ = action;
    return true;
  }
  void OnAccessibilityEvent(ax::mojom::Event event_type) override {
    ++event_count_;
    last_event_type_ = event_type;
  }

  int action_count_ = 0;
  ui::AXActionData last_action_;
  int event_count_ = 0;
  ax::mojom::Event last_event_type_ = ax::mojom::Event::kNone;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

AXRemoteHost* CreateRemote(TestAXHostService* service) {
  std::unique_ptr<AXRemoteHost> remote = std::make_unique<AXRemoteHost>();
  remote->InitForTesting(service->CreateInterfacePtr());
  remote->FlushForTesting();
  // Install the AXRemoteHost on MusClient so it monitors Widget creation.
  MusClientTestApi::SetAXRemoteHost(std::move(remote));
  return MusClient::Get()->ax_remote_host();
}

std::unique_ptr<Widget> CreateTestWidget() {
  std::unique_ptr<Widget> widget = std::make_unique<Widget>();
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(1, 2, 333, 444);
  widget->Init(params);
  return widget;
}

using AXRemoteHostTest = ViewsTestBase;

TEST_F(AXRemoteHostTest, CreateRemote) {
  TestAXHostService service(false /*automation_enabled*/);
  CreateRemote(&service);

  // Client registered itself with service.
  EXPECT_EQ(1, service.remote_host_count_);
}

TEST_F(AXRemoteHostTest, AutomationEnabled) {
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  remote->FlushForTesting();

  // Tree ID is assigned.
  std::string* tree_id_ptr =
      widget->GetNativeWindow()->GetProperty(ui::kChildAXTreeID);
  ASSERT_TRUE(tree_id_ptr);
  ui::AXTreeID tree_id = ui::AXTreeID::FromString(*tree_id_ptr);
  EXPECT_EQ(TestAXTreeID(), tree_id);

  // Event was sent with initial hierarchy.
  EXPECT_EQ(TestAXTreeID(), service.last_tree_id_);
  EXPECT_EQ(ax::mojom::Event::kLoadComplete, service.last_event_.event_type);
  EXPECT_EQ(AXAuraObjCache::GetInstance()->GetID(
                widget->widget_delegate()->GetContentsView()),
            service.last_event_.id);
}

// Regression test for https://crbug.com/876407
TEST_F(AXRemoteHostTest, AutomationEnabledTwice) {
  // If ChromeVox has ever been open during the user session then the remote app
  // can see automation enabled at startup.
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  remote->FlushForTesting();

  // Remote host sent load complete.
  EXPECT_EQ(ax::mojom::Event::kLoadComplete, service.last_event_.event_type);
  service.ResetCounts();

  // Simulate host service asking to enable again. This happens if ChromeVox is
  // re-enabled after the remote app is open.
  remote->OnAutomationEnabled(true);
  remote->FlushForTesting();

  // Load complete was sent again after the second enable.
  EXPECT_EQ(ax::mojom::Event::kLoadComplete, service.last_event_.event_type);
}

// Verifies that a remote app doesn't crash if a View triggers an accessibility
// event before it is attached to a Widget. https://crbug.com/889121
TEST_F(AXRemoteHostTest, SendEventOnViewWithNoWidget) {
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  remote->FlushForTesting();

  // Create a view that is not yet associated with the widget.
  views::View view;
  remote->HandleEvent(&view, ax::mojom::Event::kLocationChanged);
  // No crash.
}

// Verifies that the AXRemoteHost stops monitoring widgets that are closed
// asynchronously, like when ash requests close via DesktopWindowTreeHostMus.
// https://crbug.com/869608
TEST_F(AXRemoteHostTest, AsyncWidgetClose) {
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  remote->FlushForTesting();

  Widget* widget = new Widget();  // Owned by native widget.
  Widget::InitParams params;
  params.bounds = gfx::Rect(1, 2, 333, 444);
  widget->Init(params);
  widget->Show();

  // AXRemoteHost is tracking the widget.
  EXPECT_TRUE(remote->widget_for_testing());

  // Asynchronously close the widget using Close() instead of CloseNow().
  widget->Close();

  // AXRemoteHost stops tracking the widget, even though it isn't destroyed yet.
  EXPECT_FALSE(remote->widget_for_testing());

  // Widget finishes closing.
  base::RunLoop().RunUntilIdle();
  // No crash.
}

TEST_F(AXRemoteHostTest, CreateWidgetThenEnableAutomation) {
  TestAXHostService service(false /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  remote->FlushForTesting();

  // No events were sent because automation isn't enabled.
  EXPECT_EQ(0, service.event_count_);

  remote->OnAutomationEnabled(true);
  remote->FlushForTesting();

  // Event was sent with initial hierarchy.
  EXPECT_EQ(ax::mojom::Event::kLoadComplete, service.last_event_.event_type);
  EXPECT_EQ(AXAuraObjCache::GetInstance()->GetID(
                widget->widget_delegate()->GetContentsView()),
            service.last_event_.id);
}

TEST_F(AXRemoteHostTest, PerformAction) {
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);

  // Create a view to sense the action.
  TestView view;
  view.SetBounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  widget->GetRootView()->AddChildView(&view);
  AXAuraObjCache::GetInstance()->GetOrCreate(&view);

  // Request an action on the view.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kScrollDown;
  action.target_node_id = AXAuraObjCache::GetInstance()->GetID(&view);
  remote->PerformAction(action);

  // View received the action.
  EXPECT_EQ(1, view.action_count_);
  EXPECT_EQ(ax::mojom::Action::kScrollDown, view.last_action_.action);
}

TEST_F(AXRemoteHostTest, PerformHitTest) {
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);

  // Create a view to sense the action.
  TestView view;
  view.SetBounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  widget->GetRootView()->AddChildView(&view);

  // Clear event counts triggered by view creation and bounds set.
  view.ResetCounts();

  // Request a hit test.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kHitTest;
  action.hit_test_event_to_fire = ax::mojom::Event::kHitTestResult;
  action.target_point = gfx::Point(5, 5);
  remote->PerformAction(action);

  // View sensed a hit.
  EXPECT_EQ(1, view.event_count_);
  EXPECT_EQ(ax::mojom::Event::kHitTestResult, view.last_event_type_);
  view.ResetCounts();

  // Try to hit a point outside the view.
  action.target_point = gfx::Point(101, 101);
  remote->PerformAction(action);

  // View wasn't hit.
  EXPECT_EQ(0, view.event_count_);
  EXPECT_EQ(ax::mojom::Event::kNone, view.last_event_type_);
}

TEST_F(AXRemoteHostTest, ScaleFactor) {
  // Set the primary display to high DPI.
  ScreenMus* screen = static_cast<ScreenMus*>(display::Screen::GetScreen());
  display::Display display = screen->GetPrimaryDisplay();
  display.set_device_scale_factor(2.f);
  screen->display_list().UpdateDisplay(display);

  // Create a widget.
  TestAXHostService service(true /*automation_enabled*/);
  AXRemoteHost* remote = CreateRemote(&service);
  std::unique_ptr<Widget> widget = CreateTestWidget();
  remote->FlushForTesting();

  // Widget transform is scaled by a factor of 2.
  ASSERT_FALSE(service.last_updates_.empty());
  gfx::Transform* transform = service.last_updates_[0].nodes[0].transform.get();
  ASSERT_TRUE(transform);
  EXPECT_TRUE(transform->IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f), transform->Scale2d());
}

}  // namespace
}  // namespace views
