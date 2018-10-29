// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_client.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/embed_root.h"
#include "ui/aura/mus/embed_root_delegate.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_client_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/window_port_mus_test_helper.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace aura {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(uint8_t, kTestPropertyKey1, 0);
DEFINE_UI_CLASS_PROPERTY_KEY(uint16_t, kTestPropertyKey2, 0);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kTestPropertyKey3, false);
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kTestPropertyKey4, nullptr);

const char kTestPropertyServerKey1[] = "test-property-server1";
const char kTestPropertyServerKey2[] = "test-property-server2";
const char kTestPropertyServerKey3[] = "test-property-server3";
const char kTestPropertyServerKey4[] = "test-property-server4";

ws::Id server_id(Window* window) {
  return window ? WindowMus::Get(window)->server_id() : 0;
}

std::unique_ptr<Window> CreateWindowUsingId(
    WindowTreeClient* window_tree_client,
    ws::Id server_id,
    Window* parent = nullptr) {
  ws::mojom::WindowData window_data;
  window_data.window_id = server_id;
  WindowMus* window_mus =
      WindowTreeClientPrivate(window_tree_client)
          .NewWindowFromWindowData(WindowMus::Get(parent), window_data);
  // WindowTreeClient implicitly creates the Window but doesn't own it.
  // Pass ownership to the caller.
  return base::WrapUnique<Window>(window_mus->GetWindow());
}

void SetWindowVisibility(Window* window, bool visible) {
  if (visible)
    window->Show();
  else
    window->Hide();
}

bool IsWindowHostVisible(Window* window) {
  return window->GetRootWindow()->GetHost()->compositor()->IsVisible();
}

// Register some test window properties for aura/mus conversion.
void RegisterTestProperties(PropertyConverter* converter) {
  converter->RegisterPrimitiveProperty(
      kTestPropertyKey1, kTestPropertyServerKey1,
      PropertyConverter::CreateAcceptAnyValueCallback());
  converter->RegisterPrimitiveProperty(
      kTestPropertyKey2, kTestPropertyServerKey2,
      PropertyConverter::CreateAcceptAnyValueCallback());
  converter->RegisterPrimitiveProperty(
      kTestPropertyKey3, kTestPropertyServerKey3,
      PropertyConverter::CreateAcceptAnyValueCallback());
  converter->RegisterWindowPtrProperty(kTestPropertyKey4,
                                       kTestPropertyServerKey4);
}

// Convert a primitive aura property value to a mus transport value.
// Note that this implicitly casts arguments to the aura storage type, int64_t.
std::vector<uint8_t> ConvertToPropertyTransportValue(int64_t value) {
  return mojo::ConvertTo<std::vector<uint8_t>>(value);
}

void OnWindowMoveDone(int* call_count, bool* last_result, bool result) {
  (*call_count)++;
  *last_result = result;
}

// A simple event observer that records the last observed event.
class TestEventObserver : public ui::EventObserver {
 public:
  TestEventObserver() = default;
  ~TestEventObserver() override = default;

  const ui::Event* last_event() const { return last_event_.get(); }
  void ResetLastEvent() { last_event_.reset(); }

 private:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    last_event_ = ui::Event::Clone(event);
  }

  std::unique_ptr<ui::Event> last_event_;

  DISALLOW_COPY_AND_ASSIGN(TestEventObserver);
};

}  // namespace

class WindowTreeClientTest : public test::AuraMusClientTestBase {
 public:
  WindowTreeClientTest() = default;
  ~WindowTreeClientTest() override = default;

  struct TopLevel {
    std::unique_ptr<client::DefaultCaptureClient> capture_client;
    std::unique_ptr<WindowTreeHostMus> host;
  };

  std::unique_ptr<TopLevel> CreateWindowTreeHostForTopLevel() {
    std::unique_ptr<TopLevel> top_level = std::make_unique<TopLevel>();
    top_level->host = std::make_unique<WindowTreeHostMus>(
        CreateInitParamsForTopLevel(window_tree_client_impl()));
    top_level->host->InitHost();
    Window* top_level_window = top_level->host->window();
    top_level->capture_client =
        std::make_unique<client::DefaultCaptureClient>();
    client::SetCaptureClient(top_level_window, top_level->capture_client.get());
    window_tree_client_impl()->capture_synchronizer()->AttachToCaptureClient(
        top_level->capture_client.get());

    // Ack the request to the windowtree to create the new window.
    uint32_t change_id = 0;
    EXPECT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
        WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
    EXPECT_EQ(window_tree()->window_id(), server_id(top_level_window));

    ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
    data->window_id = server_id(top_level_window);
    data->visible = true;
    window_tree_client()->OnTopLevelCreated(
        change_id, std::move(data), next_display_id_++, true, base::nullopt);
    EXPECT_EQ(0u, window_tree()->GetChangeCountForType(
                      WindowTreeChangeType::VISIBLE));
    EXPECT_TRUE(top_level_window->TargetVisibility());
    return top_level;
  }

 private:
  int64_t next_display_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientTest);
};

class WindowTreeClientTestSurfaceSync
    : public WindowTreeClientTest,
      public ::testing::WithParamInterface<bool> {
 public:
  WindowTreeClientTestSurfaceSync() {}
  ~WindowTreeClientTestSurfaceSync() override {}

  // WindowTreeClientTest:
  void SetUp() override {
    if (GetParam()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kForceDeviceScaleFactor, "2");
    }
    WindowTreeClientTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientTestSurfaceSync);
};

// WindowTreeClientTest with --force-device-scale-factor=2.
class WindowTreeClientTestHighDPI : public WindowTreeClientTest {
 public:
  WindowTreeClientTestHighDPI() = default;
  ~WindowTreeClientTestHighDPI() override = default;

  // WindowTreeClientTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "2");
    WindowTreeClientTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientTestHighDPI);
};

// Verifies bounds are reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetBoundsFailed) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  const gfx::Rect original_bounds(window.bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, window.bounds());
  window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window.bounds());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(original_bounds, window.bounds());
}

// Verifies bounds and the viz::LocalSurfaceId associated with the bounds are
// reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetBoundsFailedLocalSurfaceId) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  WindowPortMusTestHelper(&window).SimulateEmbedding();

  const gfx::Rect original_bounds(window.bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, window.bounds());
  window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window.bounds());
  WindowMus* window_mus = WindowMus::Get(&window);
  ASSERT_NE(nullptr, window_mus);
  EXPECT_TRUE(window_mus->GetLocalSurfaceId().is_valid());

  // Reverting the change should also revert the viz::LocalSurfaceId.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(original_bounds, window.bounds());
  EXPECT_FALSE(window_mus->GetLocalSurfaceId().is_valid());
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        WindowTreeClientTestSurfaceSync,
                        ::testing::Bool());

// Verifies that windows with an embedding create a ClientSurfaceEmbedder.
TEST_P(WindowTreeClientTestSurfaceSync, ClientSurfaceEmbedderCreated) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  WindowPortMusTestHelper(&window).SimulateEmbedding();

  // The window will allocate a viz::LocalSurfaceId once it has a bounds.
  WindowPortMus* window_port_mus = WindowPortMus::Get(&window);
  ASSERT_NE(nullptr, window_port_mus);
  EXPECT_FALSE(WindowMus::Get(&window)->GetLocalSurfaceId().is_valid());
  // A ClientSurfaceEmbedder is only created once there is bounds and a
  // FrameSinkId.
  EXPECT_EQ(nullptr, window_port_mus->client_surface_embedder());
  gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, window.bounds());
  window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window.bounds());
  EXPECT_TRUE(WindowMus::Get(&window)->GetLocalSurfaceId().is_valid());

  // Once the bounds have been set, the ClientSurfaceEmbedder should be created.
  ClientSurfaceEmbedder* client_surface_embedder =
      window_port_mus->client_surface_embedder();
  ASSERT_NE(nullptr, client_surface_embedder);

  EXPECT_EQ(nullptr, client_surface_embedder->BottomGutterForTesting());
  EXPECT_EQ(nullptr, client_surface_embedder->RightGutterForTesting());
}

// Verifies that the viz::LocalSurfaceId generated by an embedder changes when
// the size changes, but not when the position changes.
TEST_P(WindowTreeClientTestSurfaceSync, SetBoundsLocalSurfaceIdChanges) {
  ASSERT_EQ(base::nullopt, window_tree()->last_local_surface_id());
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  WindowPortMusTestHelper(&window).SimulateEmbedding();

  // Resize the window and verify that we've allocated a viz::LocalSurfaceId.
  const gfx::Rect new_bounds(0, 0, 100, 100);
  ASSERT_NE(new_bounds, window.bounds());
  window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window.bounds());
  base::Optional<viz::LocalSurfaceId> last_local_surface_id =
      window_tree()->last_local_surface_id();
  ASSERT_NE(base::nullopt, last_local_surface_id);

  // Resize the window again and verify that the viz::LocalSurfaceId has
  // changed.
  const gfx::Rect new_bounds2(0, 0, 100, 102);
  ASSERT_NE(new_bounds2, window.bounds());
  window.SetBounds(new_bounds2);
  EXPECT_EQ(new_bounds2, window.bounds());
  base::Optional<viz::LocalSurfaceId> last_local_surface_id2 =
      window_tree()->last_local_surface_id();
  ASSERT_NE(base::nullopt, last_local_surface_id2);
  EXPECT_NE(last_local_surface_id2, last_local_surface_id);

  // Moving the window but not changing the size should not allocate a new
  // viz::LocalSurfaceId.
  const gfx::Rect new_bounds3(1337, 7331, 100, 102);
  ASSERT_NE(new_bounds3, window.bounds());
  window.SetBounds(new_bounds3);
  EXPECT_EQ(new_bounds3, window.bounds());
  base::Optional<viz::LocalSurfaceId> last_local_surface_id3 =
      window_tree()->last_local_surface_id();
  ASSERT_NE(base::nullopt, last_local_surface_id3);
  EXPECT_EQ(last_local_surface_id2, last_local_surface_id3);
}

// Verifies a new window from the server doesn't result in attempting to add
// the window back to the server.
TEST_F(WindowTreeClientTest, AddFromServerDoesntAddAgain) {
  const ws::Id child_window_id = server_id(root_window()) + 11;
  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->parent_id = server_id(root_window());
  data->window_id = child_window_id;
  data->bounds = gfx::Rect(1, 2, 3, 4);
  data->visible = false;
  std::vector<ws::mojom::WindowDataPtr> data_array(1);
  data_array[0] = std::move(data);
  ASSERT_TRUE(root_window()->children().empty());
  window_tree_client()->OnWindowHierarchyChanged(
      child_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* child = root_window()->children()[0];
  EXPECT_FALSE(child->TargetVisibility());
}

// Verifies a reparent from the server doesn't attempt signal the server.
TEST_F(WindowTreeClientTest, ReparentFromServerDoesntAddAgain) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  window_tree()->AckAllChanges();
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowHierarchyChanged(
      server_id(&window1), server_id(root_window()), server_id(&window2),
      std::vector<ws::mojom::WindowDataPtr>());
  ASSERT_FALSE(window_tree()->has_change());
  EXPECT_EQ(&window2, window1.parent());
  EXPECT_EQ(root_window(), window2.parent());
  window1.parent()->RemoveChild(&window1);
}

// Verifies properties passed in OnWindowHierarchyChanged() make there way to
// the new window.
TEST_F(WindowTreeClientTest, OnWindowHierarchyChangedWithProperties) {
  RegisterTestProperties(GetPropertyConverter());
  window_tree()->AckAllChanges();
  const ws::Id child_window_id = server_id(root_window()) + 11;
  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  const uint8_t server_test_property1_value = 91;
  data->properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(server_test_property1_value);
  data->properties[ws::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ws::mojom::WindowType::BUBBLE));
  constexpr int kWindowCornerRadiusValue = 6;
  data->properties[ws::mojom::WindowManager::kWindowCornerRadius_Property] =
      ConvertToPropertyTransportValue(kWindowCornerRadiusValue);
  data->parent_id = server_id(root_window());
  data->window_id = child_window_id;
  data->bounds = gfx::Rect(1, 2, 3, 4);
  data->visible = false;
  std::vector<ws::mojom::WindowDataPtr> data_array(1);
  data_array[0] = std::move(data);
  ASSERT_TRUE(root_window()->children().empty());
  window_tree_client()->OnWindowHierarchyChanged(
      child_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* child = root_window()->children()[0];
  EXPECT_FALSE(child->TargetVisibility());
  EXPECT_EQ(server_test_property1_value, child->GetProperty(kTestPropertyKey1));
  EXPECT_EQ(kWindowCornerRadiusValue,
            child->GetProperty(client::kWindowCornerRadiusKey));
  EXPECT_EQ(child->type(), client::WINDOW_TYPE_POPUP);
  EXPECT_EQ(ws::mojom::WindowType::BUBBLE,
            child->GetProperty(client::kWindowTypeKey));
}

// Verifies a move from the server doesn't attempt signal the server.
TEST_F(WindowTreeClientTest, MoveFromServerDoesntAddAgain) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  window_tree()->AckAllChanges();
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowReordered(server_id(&window2),
                                          server_id(&window1),
                                          ws::mojom::OrderDirection::BELOW);
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(2u, root_window()->children().size());
  EXPECT_EQ(&window2, root_window()->children()[0]);
  EXPECT_EQ(&window1, root_window()->children()[1]);
}

TEST_F(WindowTreeClientTest, FocusFromServer) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  ASSERT_TRUE(window1.CanFocus());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(window1.HasFocus());
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowFocused(server_id(&window1));
  ASSERT_FALSE(window_tree()->has_change());
  EXPECT_TRUE(window1.HasFocus());
}

// Simulates a bounds change, and while the bounds change is in flight the
// server replies with a new bounds and the original bounds change fails.
// The server bounds change takes hold along with the associated
// viz::LocalSurfaceId.
TEST_F(WindowTreeClientTest, SetBoundsFailedWithPendingChange) {
  aura::Window root_window(nullptr);
  root_window.Init(ui::LAYER_NOT_DRAWN);
  const gfx::Rect original_bounds(root_window.bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root_window.bounds());
  root_window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, root_window.bounds());

  // Simulate the server responding with a bounds change.
  const gfx::Rect server_changed_bounds(gfx::Rect(0, 0, 101, 102));
  const viz::LocalSurfaceId server_changed_local_surface_id(
      1, base::UnguessableToken::Create());
  window_tree_client()->OnWindowBoundsChanged(
      server_id(&root_window), original_bounds, server_changed_bounds,
      server_changed_local_surface_id);

  WindowMus* root_window_mus = WindowMus::Get(&root_window);
  ASSERT_NE(nullptr, root_window_mus);

  // This shouldn't trigger the bounds changing yet.
  EXPECT_EQ(new_bounds, root_window.bounds());
  EXPECT_FALSE(root_window_mus->GetLocalSurfaceId().is_valid());

  // Tell the client the change failed, which should trigger failing to the
  // most recent bounds from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(server_changed_bounds, root_window.bounds());
  EXPECT_EQ(server_changed_local_surface_id,
            root_window_mus->GetLocalSurfaceId());

  // Simulate server changing back to original bounds. Should take immediately.
  window_tree_client()->OnWindowBoundsChanged(server_id(&root_window),
                                              server_changed_bounds,
                                              original_bounds, base::nullopt);
  EXPECT_EQ(original_bounds, root_window.bounds());
  EXPECT_FALSE(root_window_mus->GetLocalSurfaceId().is_valid());
}

TEST_F(WindowTreeClientTest, TwoInFlightBoundsChangesBothCanceled) {
  aura::Window root_window(nullptr);
  root_window.Init(ui::LAYER_NOT_DRAWN);
  const gfx::Rect original_bounds(root_window.bounds());
  const gfx::Rect bounds1(gfx::Rect(0, 0, 100, 100));
  const gfx::Rect bounds2(gfx::Rect(0, 0, 100, 102));
  root_window.SetBounds(bounds1);
  EXPECT_EQ(bounds1, root_window.bounds());

  root_window.SetBounds(bounds2);
  EXPECT_EQ(bounds2, root_window.bounds());

  // Tell the client the first bounds failed. As there is a still a change in
  // flight nothing should happen.
  ASSERT_TRUE(
      window_tree()->AckFirstChangeOfType(WindowTreeChangeType::BOUNDS, false));
  EXPECT_EQ(bounds2, root_window.bounds());

  // Tell the client the seconds bounds failed. Should now fallback to original
  // value.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(original_bounds, root_window.bounds());
}

TEST_F(WindowTreeClientTest, TwoInFlightTransformsChangesBothCanceled) {
  const gfx::Transform original_transform(root_window()->layer()->transform());
  gfx::Transform transform1;
  transform1.Scale(SkIntToMScalar(2), SkIntToMScalar(2));
  gfx::Transform transform2;
  transform2.Scale(SkIntToMScalar(3), SkIntToMScalar(3));
  root_window()->SetTransform(transform1);
  EXPECT_EQ(transform1, root_window()->layer()->transform());

  root_window()->SetTransform(transform2);
  EXPECT_EQ(transform2, root_window()->layer()->transform());

  // Tell the client the first transform failed. As there is a still a change in
  // flight nothing should happen.
  ASSERT_TRUE(window_tree()->AckFirstChangeOfType(
      WindowTreeChangeType::TRANSFORM, false));
  EXPECT_EQ(transform2, root_window()->layer()->transform());

  // Tell the client the seconds transform failed. Should now fallback to
  // original value.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::TRANSFORM, false));
  EXPECT_EQ(original_transform, root_window()->layer()->transform());
}

// Verifies properties are set if the server replied that the change succeeded.
TEST_F(WindowTreeClientTest, SetPropertySucceeded) {
  ASSERT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  root_window()->SetProperty(client::kAlwaysOnTopKey, true);
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  base::Optional<std::vector<uint8_t>> value =
      window_tree()->GetLastPropertyValue();
  ASSERT_TRUE(value.has_value());
  // PropertyConverter uses int64_t values, even for smaller types, like bool.
  ASSERT_EQ(8u, value->size());
  EXPECT_EQ(1, mojo::ConvertTo<int64_t>(*value));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
}

// Verifies properties are reverted if the server replied that the change
// failed.
TEST_F(WindowTreeClientTest, SetPropertyFailed) {
  ASSERT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  root_window()->SetProperty(client::kAlwaysOnTopKey, true);
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  base::Optional<std::vector<uint8_t>> value =
      window_tree()->GetLastPropertyValue();
  ASSERT_TRUE(value.has_value());
  // PropertyConverter uses int64_t values, even for smaller types, like bool.
  ASSERT_EQ(8u, value->size());
  EXPECT_EQ(1, mojo::ConvertTo<int64_t>(*value));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
}

// Simulates a property change, and while the property change is in flight the
// server replies with a new property and the original property change fails.
TEST_F(WindowTreeClientTest, SetPropertyFailedWithPendingChange) {
  RegisterTestProperties(GetPropertyConverter());
  const uint8_t value1 = 11;
  root_window()->SetProperty(kTestPropertyKey1, value1);
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));

  // Simulate the server responding with a different value.
  const uint8_t server_value = 12;
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(server_value));

  // This shouldn't trigger the property changing yet.
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));

  // Tell the client the change failed, which should trigger failing to the
  // most recent value from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(server_value, root_window()->GetProperty(kTestPropertyKey1));

  // Simulate server changing back to value1. Should take immediately.
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(value1));
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));
}

// Verifies property setting behavior with failures for primitive properties.
TEST_F(WindowTreeClientTest, SetPrimitiveProperties) {
  PropertyConverter* property_converter = GetPropertyConverter();
  RegisterTestProperties(property_converter);

  const uint8_t value1_local = UINT8_MAX / 2;
  const uint8_t value1_server = UINT8_MAX / 3;
  root_window()->SetProperty(kTestPropertyKey1, value1_local);
  EXPECT_EQ(value1_local, root_window()->GetProperty(kTestPropertyKey1));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(value1_server));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(value1_server, root_window()->GetProperty(kTestPropertyKey1));

  const uint16_t value2_local = UINT16_MAX / 3;
  const uint16_t value2_server = UINT16_MAX / 4;
  root_window()->SetProperty(kTestPropertyKey2, value2_local);
  EXPECT_EQ(value2_local, root_window()->GetProperty(kTestPropertyKey2));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey2,
      ConvertToPropertyTransportValue(value2_server));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(value2_server, root_window()->GetProperty(kTestPropertyKey2));

  EXPECT_FALSE(root_window()->GetProperty(kTestPropertyKey3));
  root_window()->SetProperty(kTestPropertyKey3, true);
  EXPECT_TRUE(root_window()->GetProperty(kTestPropertyKey3));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey3,
      ConvertToPropertyTransportValue(false));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_FALSE(root_window()->GetProperty(kTestPropertyKey3));
}

// Verifies property setting behavior for a gfx::Rect* property.
TEST_F(WindowTreeClientTest, SetRectProperty) {
  gfx::Rect example(1, 2, 3, 4);
  ASSERT_EQ(nullptr, root_window()->GetProperty(client::kRestoreBoundsKey));
  root_window()->SetProperty(client::kRestoreBoundsKey, new gfx::Rect(example));
  EXPECT_TRUE(root_window()->GetProperty(client::kRestoreBoundsKey));
  base::Optional<std::vector<uint8_t>> value =
      window_tree()->GetLastPropertyValue();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(example, mojo::ConvertTo<gfx::Rect>(*value));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kRestoreBoundsKey));

  root_window()->SetProperty(client::kRestoreBoundsKey, new gfx::Rect());
  EXPECT_EQ(gfx::Rect(),
            *root_window()->GetProperty(client::kRestoreBoundsKey));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kRestoreBoundsKey));
}

// Verifies property setting behavior for a std::string* property.
TEST_F(WindowTreeClientTest, SetStringProperty) {
  std::string example = "123";
  ASSERT_NE(nullptr, root_window()->GetProperty(client::kNameKey));
  root_window()->SetProperty(client::kNameKey, new std::string(example));
  EXPECT_TRUE(root_window()->GetProperty(client::kNameKey));
  base::Optional<std::vector<uint8_t>> value =
      window_tree()->GetLastPropertyValue();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(example, mojo::ConvertTo<std::string>(*value));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kNameKey));

  root_window()->SetProperty(client::kNameKey, new std::string());
  EXPECT_EQ(std::string(), *root_window()->GetProperty(client::kNameKey));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kNameKey));
}

TEST_F(WindowTreeClientTest, SetWindowPointerProperty) {
  PropertyConverter* property_converter = GetPropertyConverter();
  RegisterTestProperties(property_converter);

  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.Show();
  root_window()->SetProperty(kTestPropertyKey4, &window);
  base::Optional<std::vector<uint8_t>> value =
      window_tree()->GetLastPropertyValue();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(WindowMus::Get(&window)->server_id(),
            mojo::ConvertTo<ws::Id>(*value));
  window_tree()->AckAllChanges();

  root_window()->ClearProperty(kTestPropertyKey4);
  value = window_tree()->GetLastPropertyValue();
  EXPECT_FALSE(value.has_value());
}

// Verifies visible is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetVisibleFailed) {
  const bool original_visible = root_window()->TargetVisibility();
  const bool new_visible = !original_visible;
  SetWindowVisibility(root_window(), new_visible);
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_EQ(original_visible, root_window()->TargetVisibility());
}

// Simulates a visible change, and while the visible change is in flight the
// server replies with a new visible and the original visible change fails.
TEST_F(WindowTreeClientTest, SetVisibleFailedWithPendingChange) {
  const bool original_visible = root_window()->TargetVisibility();
  const bool new_visible = !original_visible;
  SetWindowVisibility(root_window(), new_visible);
  EXPECT_EQ(new_visible, root_window()->TargetVisibility());

  // Simulate the server responding with a visible change.
  const bool server_changed_visible = !new_visible;
  window_tree_client()->OnWindowVisibilityChanged(server_id(root_window()),
                                                  server_changed_visible);

  // This shouldn't trigger visible changing yet.
  EXPECT_EQ(new_visible, root_window()->TargetVisibility());

  // Tell the client the change failed, which should trigger failing to the
  // most recent visible from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_EQ(server_changed_visible, root_window()->TargetVisibility());

  // Simulate server changing back to original visible. Should take immediately.
  window_tree_client()->OnWindowVisibilityChanged(server_id(root_window()),
                                                  original_visible);
  EXPECT_EQ(original_visible, root_window()->TargetVisibility());
}

namespace {

class InputEventBasicTestWindowDelegate : public test::TestWindowDelegate {
 public:
  explicit InputEventBasicTestWindowDelegate(TestWindowTree* test_window_tree)
      : test_window_tree_(test_window_tree) {}
  ~InputEventBasicTestWindowDelegate() override {}

  int move_count() const { return move_count_; }
  int press_count() const { return press_count_; }
  int release_count() const { return release_count_; }
  bool was_acked() const { return was_acked_; }
  const gfx::Point& last_event_location() const { return last_event_location_; }
  void set_event_id(uint32_t event_id) { event_id_ = event_id; }
  bool last_mouse_event_had_native_event() const {
    return last_mouse_event_had_native_event_;
  }
  const gfx::Point& last_native_event_location() const {
    return last_native_event_location_;
  }
  ui::EventPointerType last_pointer_type() const { return last_pointer_type_; }

  // TestWindowDelegate::
  void OnMouseEvent(ui::MouseEvent* event) override {
    was_acked_ = test_window_tree_->WasEventAcked(event_id_);
    if (event->type() == ui::ET_MOUSE_MOVED)
      ++move_count_;
    else if (event->type() == ui::ET_MOUSE_PRESSED)
      ++press_count_;
    else if (event->type() == ui::ET_MOUSE_RELEASED)
      ++release_count_;
    last_event_location_ = event->location();
    last_mouse_event_had_native_event_ = event->HasNativeEvent();
    if (event->HasNativeEvent()) {
      last_native_event_location_ =
          ui::EventSystemLocationFromNative(event->native_event());
    }
    last_pointer_type_ = event->pointer_details().pointer_type;
    event->SetHandled();
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    was_acked_ = test_window_tree_->WasEventAcked(event_id_);
    if (event->type() == ui::ET_TOUCH_PRESSED)
      ++press_count_;
    else if (event->type() == ui::ET_TOUCH_RELEASED)
      ++release_count_;
    last_event_location_ = event->location();
    last_pointer_type_ = event->pointer_details().pointer_type;
    event->SetHandled();
  }

  void reset() {
    was_acked_ = false;
    move_count_ = 0;
    press_count_ = 0;
    release_count_ = 0;
    last_event_location_ = gfx::Point();
    event_id_ = 0;
    last_mouse_event_had_native_event_ = false;
    last_native_event_location_ = gfx::Point();
    last_pointer_type_ = ui::EventPointerType::POINTER_TYPE_UNKNOWN;
  }

 private:
  TestWindowTree* test_window_tree_;
  bool was_acked_ = false;
  int move_count_ = 0;
  int press_count_ = 0;
  int release_count_ = false;
  gfx::Point last_event_location_;
  uint32_t event_id_ = 0;
  bool last_mouse_event_had_native_event_ = false;
  gfx::Point last_native_event_location_;
  ui::EventPointerType last_pointer_type_ =
      ui::EventPointerType::POINTER_TYPE_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(InputEventBasicTestWindowDelegate);
};

class InputEventBasicTestEventHandler : public ui::test::TestEventHandler {
 public:
  explicit InputEventBasicTestEventHandler(Window* target_window)
      : target_window_(target_window) {}
  ~InputEventBasicTestEventHandler() override {}

  int move_count() const { return move_count_; }
  const gfx::Point& last_event_location() const { return last_event_location_; }
  void set_event_id(uint32_t event_id) { event_id_ = event_id; }

  // ui::test::TestEventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->target() == target_window_) {
      if (event->type() == ui::ET_MOUSE_MOVED)
        ++move_count_;
      last_event_location_ = event->location();
      event->SetHandled();
    }
  }

  void reset() {
    move_count_ = 0;
    last_event_location_ = gfx::Point();
    event_id_ = 0;
  }

 private:
  Window* target_window_ = nullptr;
  int move_count_ = 0;
  gfx::Point last_event_location_;
  uint32_t event_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InputEventBasicTestEventHandler);
};

}  // namespace

TEST_F(WindowTreeClientTest, InputEventBasic) {
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();
  EXPECT_EQ(0, window_delegate.move_count());
  EXPECT_FALSE(window_delegate.was_acked());
  const gfx::Point event_location_in_child(2, 3);
  const uint32_t event_id = 1;
  window_delegate.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location_in_child,
                         gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate.move_count());
  EXPECT_FALSE(window_delegate.was_acked());
  EXPECT_EQ(event_location_in_child, window_delegate.last_event_location());
}

TEST_F(WindowTreeClientTest, InputEventMouse) {
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();
  EXPECT_EQ(0, window_delegate.move_count());
  const gfx::Point event_location(2, 3);
  const uint32_t event_id = 1;
  window_delegate.set_event_id(event_id);
  ui::MouseEvent mouse_event(
      ui::ET_MOUSE_MOVED, event_location, event_location, ui::EventTimeForNow(),
      ui::EF_NONE, ui::EF_NONE,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE, 0));
  window_tree_client()->OnWindowInputEvent(event_id, server_id(&child),
                                           window_tree_host.display_id(),
                                           ui::Event::Clone(mouse_event), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate.move_count());
  EXPECT_EQ(event_location, window_delegate.last_event_location());
}

TEST_F(WindowTreeClientTest, InputEventPen) {
  // Create a root window.
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();

  // Create a child window with a test delegate to sense events.
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();

  // Dispatch a pen event to the child window.
  const gfx::Point event_location(2, 3);
  const uint32_t event_id = 1;
  window_delegate.set_event_id(event_id);
  ui::MouseEvent mouse_event(
      ui::ET_MOUSE_PRESSED, event_location, event_location,
      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_PEN, 0));
  window_tree_client()->OnWindowInputEvent(event_id, server_id(&child),
                                           window_tree_host.display_id(),
                                           ui::Event::Clone(mouse_event), 0);

  // Pen event was handled.
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(ui::EventPointerType::POINTER_TYPE_PEN,
            window_delegate.last_pointer_type());
}

TEST_F(WindowTreeClientTest, InputEventFindTargetAndConversion) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  InputEventBasicTestWindowDelegate window_delegate1(window_tree());
  Window child1(&window_delegate1);
  child1.Init(ui::LAYER_NOT_DRAWN);
  child1.SetEventTargeter(std::make_unique<WindowTargeter>());
  top_level->AddChild(&child1);
  child1.SetBounds(gfx::Rect(10, 10, 100, 100));
  child1.Show();
  InputEventBasicTestWindowDelegate window_delegate2(window_tree());
  Window child2(&window_delegate2);
  child2.Init(ui::LAYER_NOT_DRAWN);
  child1.AddChild(&child2);
  child2.SetBounds(gfx::Rect(20, 30, 100, 100));
  child2.Show();

  EXPECT_EQ(0, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());

  // child1 has a targeter set and event_location is (50, 60), child2
  // should get the event even though mus-ws wants to send to child1.
  const gfx::Point event_location(50, 60);
  uint32_t event_id = 1;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location, gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child1), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(0, window_delegate1.move_count());
  EXPECT_EQ(1, window_delegate2.move_count());
  EXPECT_EQ(gfx::Point(30, 30), window_delegate2.last_event_location());
  window_delegate1.reset();
  window_delegate2.reset();

  // Remove the targeter for child1 and specify the event to go to child1. This
  // time child1 should receive the event not child2.
  child1.SetEventTargeter(nullptr);
  event_id = 2;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event1(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location, gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child1), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event1.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());
  EXPECT_EQ(gfx::Point(50, 60), window_delegate1.last_event_location());
}

TEST_F(WindowTreeClientTest, InputEventCustomWindowTargeter) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  InputEventBasicTestWindowDelegate window_delegate1(window_tree());
  Window child1(&window_delegate1);
  child1.Init(ui::LAYER_NOT_DRAWN);
  child1.SetEventTargeter(std::make_unique<test::TestWindowTargeter>());
  top_level->AddChild(&child1);
  child1.SetBounds(gfx::Rect(10, 10, 100, 100));
  child1.Show();
  InputEventBasicTestWindowDelegate window_delegate2(window_tree());
  Window child2(&window_delegate2);
  child2.Init(ui::LAYER_NOT_DRAWN);
  child1.AddChild(&child2);
  child2.SetBounds(gfx::Rect(20, 30, 100, 100));
  child2.Show();

  EXPECT_EQ(0, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());

  // child1 has a custom targeter set which would always return itself as the
  // target window therefore event should go to child1 unlike
  // WindowTreeClientTest.InputEventFindTargetAndConversion.
  const gfx::Point event_location(50, 60);
  uint32_t event_id = 1;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location, gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child1), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());
  EXPECT_EQ(gfx::Point(50, 60), window_delegate1.last_event_location());
  window_delegate1.reset();
  window_delegate2.reset();

  // child1 should get the event even though mus-ws specifies child2 and it's
  // actually in child2's space. Event location will be transformed.
  event_id = 2;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child2), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());
  EXPECT_EQ(gfx::Point(70, 90), window_delegate1.last_event_location());
}

TEST_F(WindowTreeClientTest, InputEventCaptureWindow) {
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host->window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host->SetBoundsInPixels(bounds);
  window_tree_host->InitHost();
  window_tree_host->Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host->GetBoundsInPixels());
  std::unique_ptr<InputEventBasicTestWindowDelegate> window_delegate1(
      std::make_unique<InputEventBasicTestWindowDelegate>(window_tree()));
  std::unique_ptr<Window> child1(
      std::make_unique<Window>(window_delegate1.get()));
  child1->Init(ui::LAYER_NOT_DRAWN);
  child1->SetEventTargeter(std::make_unique<test::TestWindowTargeter>());
  top_level->AddChild(child1.get());
  child1->SetBounds(gfx::Rect(10, 10, 100, 100));
  child1->Show();
  std::unique_ptr<InputEventBasicTestWindowDelegate> window_delegate2(
      std::make_unique<InputEventBasicTestWindowDelegate>(window_tree()));
  std::unique_ptr<Window> child2(
      std::make_unique<Window>(window_delegate2.get()));
  child2->Init(ui::LAYER_NOT_DRAWN);
  child1->AddChild(child2.get());
  child2->SetBounds(gfx::Rect(20, 30, 100, 100));
  child2->Show();

  EXPECT_EQ(0, window_delegate1->move_count());
  EXPECT_EQ(0, window_delegate2->move_count());

  // child1 has a custom targeter set which would always return itself as the
  // target window therefore event should go to child1.
  const gfx::Point event_location(50, 60);
  const gfx::Point root_location;
  uint32_t event_id = 1;
  window_delegate1->set_event_id(event_id);
  window_delegate2->set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location, root_location,
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(child1.get()), window_tree_host->display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1->move_count());
  EXPECT_EQ(0, window_delegate2->move_count());
  EXPECT_EQ(gfx::Point(50, 60), window_delegate1->last_event_location());
  window_delegate1->reset();
  window_delegate2->reset();

  // Set capture to |child2|. Capture takes precedence, and because the event is
  // converted local coordinates are converted from the original target to root
  // and then to capture target.
  std::unique_ptr<client::DefaultCaptureClient> capture_client(
      std::make_unique<client::DefaultCaptureClient>());
  client::SetCaptureClient(top_level, capture_client.get());
  child2->SetCapture();
  EXPECT_EQ(child2.get(), client::GetCaptureWindow(child2->GetRootWindow()));
  event_id = 2;
  window_delegate1->set_event_id(event_id);
  window_delegate2->set_event_id(event_id);
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(child1.get()), window_tree_host->display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(0, window_delegate1->move_count());
  EXPECT_EQ(1, window_delegate2->move_count());
  gfx::Point location_in_child2(event_location);
  Window::ConvertPointToTarget(child1.get(), top_level, &location_in_child2);
  Window::ConvertPointToTarget(top_level, child2.get(), &location_in_child2);
  EXPECT_EQ(location_in_child2, window_delegate2->last_event_location());
  child2.reset();
  child1.reset();
  window_tree_host.reset();
  capture_client.reset();
}

TEST_F(WindowTreeClientTest, InputEventRootWindow) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  InputEventBasicTestEventHandler root_handler(top_level);
  top_level->AddPreTargetHandler(&root_handler);
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  InputEventBasicTestWindowDelegate child_delegate(window_tree());
  Window child(&child_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();

  EXPECT_EQ(0, root_handler.move_count());
  EXPECT_EQ(0, child_delegate.move_count());

  const gfx::Point event_location_in_child(20, 30);
  const uint32_t event_id = 1;
  root_handler.set_event_id(event_id);
  child_delegate.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location_in_child,
                         gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(top_level), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);

  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, root_handler.move_count());
  EXPECT_EQ(gfx::Point(20, 30), root_handler.last_event_location());
  EXPECT_EQ(0, child_delegate.move_count());
  EXPECT_EQ(gfx::Point(), child_delegate.last_event_location());
  top_level->RemovePreTargetHandler(&root_handler);
}

TEST_F(WindowTreeClientTest, InputMouseEventNoWindow) {
  Env* env = Env::GetInstance();
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();

  EXPECT_EQ(0, window_delegate.press_count());
  EXPECT_FALSE(env->IsMouseButtonDown());
  EXPECT_FALSE(env->mouse_button_flags());
  EXPECT_EQ(gfx::Point(), env->last_mouse_location());

  const gfx::Point event_location(2, 3);
  uint32_t event_id = 1;
  window_delegate.set_event_id(event_id);
  ui::MouseEvent mouse_event_down(
      ui::ET_MOUSE_PRESSED, event_location, event_location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child), window_tree_host.display_id(),
      ui::Event::Clone(mouse_event_down), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate.press_count());
  EXPECT_TRUE(env->IsMouseButtonDown());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, env->mouse_button_flags());
  EXPECT_EQ(event_location, env->last_mouse_location());
  window_delegate.reset();

  const gfx::Point event_location1(4, 5);
  event_id = 2;
  window_delegate.set_event_id(event_id);
  ui::MouseEvent mouse_event_up(
      ui::ET_MOUSE_RELEASED, event_location1, event_location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE, 0));
  window_tree_client()->OnWindowInputEvent(event_id, kInvalidServerId,
                                           window_tree_host.display_id(),
                                           ui::Event::Clone(mouse_event_up), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  // WindowTreeClient::OnWindowInputEvent cannot find a target window with
  // kInvalidServerId but should use the event to update event states kept in
  // aura::Env, location shouldn't be updated.
  EXPECT_EQ(ws::mojom::EventResult::UNHANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(0, window_delegate.release_count());
  EXPECT_FALSE(env->IsMouseButtonDown());
  EXPECT_FALSE(env->mouse_button_flags());
  EXPECT_EQ(event_location, env->last_mouse_location());
}

TEST_F(WindowTreeClientTest, InputTouchEventNoWindow) {
  Env* env = Env::GetInstance();
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBoundsInPixels());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(10, 10, 100, 100));
  child.Show();

  EXPECT_EQ(0, window_delegate.press_count());
  EXPECT_FALSE(env->is_touch_down());

  const gfx::Point event_location(2, 3);
  uint32_t event_id = 1;
  window_delegate.set_event_id(event_id);
  ui::TouchEvent touch_event_down(
      ui::ET_TOUCH_PRESSED, event_location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child), window_tree_host.display_id(),
      ui::Event::Clone(touch_event_down), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate.press_count());
  EXPECT_TRUE(env->is_touch_down());
  window_delegate.reset();

  event_id = 2;
  window_delegate.set_event_id(event_id);
  ui::TouchEvent touch_event_up(
      ui::ET_TOUCH_RELEASED, event_location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  window_tree_client()->OnWindowInputEvent(event_id, kInvalidServerId,
                                           window_tree_host.display_id(),
                                           ui::Event::Clone(touch_event_up), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  // WindowTreeClient::OnWindowInputEvent cannot find a target window with
  // kInvalidServerId but should use the event to update event states kept in
  // aura::Env.
  EXPECT_EQ(ws::mojom::EventResult::UNHANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(0, window_delegate.release_count());
  EXPECT_FALSE(env->is_touch_down());
}

// Tests observation of events that did not hit a target in this window tree.
TEST_F(WindowTreeClientTest, OnObservedInputEvent) {
  std::unique_ptr<Window> top_level(std::make_unique<Window>(nullptr));
  top_level->SetType(client::WINDOW_TYPE_NORMAL);
  top_level->Init(ui::LAYER_NOT_DRAWN);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  // Start observing touch press events.
  TestEventObserver test_event_observer;
  top_level->env()->AddEventObserver(&test_event_observer, top_level->env(),
                                     {ui::ET_TOUCH_PRESSED});

  // Simulate the server sending an observed event.
  ui::TouchEvent touch_press(
      ui::ET_TOUCH_PRESSED, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  window_tree_client()->OnObservedInputEvent(ui::Event::Clone(touch_press));

  // The observer should have been notified of the event.
  ASSERT_TRUE(test_event_observer.last_event());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, test_event_observer.last_event()->type());
  test_event_observer.ResetLastEvent();

  // Remove the event observer.
  top_level->env()->RemoveEventObserver(&test_event_observer);

  // Simulate another event from the server.
  window_tree_client()->OnObservedInputEvent(ui::Event::Clone(touch_press));

  // The observer should not have been notified of the event.
  EXPECT_FALSE(test_event_observer.last_event());
}

// Tests observation of events that did hit a target in this window tree.
TEST_F(WindowTreeClientTest, OnWindowInputEventWithObserver) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(50, 50, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(bounds.size(), top_level->bounds().size());

  // Start observing touch press events.
  TestEventObserver test_event_observer;
  top_level->env()->AddEventObserver(&test_event_observer, top_level->env(),
                                     {ui::ET_TOUCH_PRESSED});

  // Simulate the server dispatching an event that also matched the observer.
  ui::TouchEvent touch_press(
      ui::ET_TOUCH_PRESSED, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  window_tree_client()->OnWindowInputEvent(1, server_id(top_level), 0,
                                           ui::Event::Clone(touch_press), true);

  // The observer should have been notified of the event.
  ASSERT_TRUE(test_event_observer.last_event());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, test_event_observer.last_event()->type());
  top_level->env()->RemoveEventObserver(&test_event_observer);
}

// Verifies focus is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetFocusFailed) {
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child);
  // AuraTestHelper::SetUp sets the active focus client and focus client root,
  // root_window is assumed to have focus until we actually focus on a
  // certain window.
  EXPECT_EQ(WindowMus::Get(root_window()),
            window_tree_client_impl()->focus_synchronizer()->focused_window());
  child.Focus();
  ASSERT_TRUE(child.HasFocus());
  EXPECT_EQ(&child, client::GetFocusClient(&child)->GetFocusedWindow());
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, false));
  // If the change failed, we fall back to the revert_value which is the
  // current focused_window.
  EXPECT_EQ(root_window(), client::GetFocusClient(&child)->GetFocusedWindow());
}

// Simulates a focus change, and while the focus change is in flight the server
// replies with a new focus and the original focus change fails.
TEST_F(WindowTreeClientTest, SetFocusFailedWithPendingChange) {
  Window child1(nullptr);
  child1.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child1);
  Window child2(nullptr);
  child2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child2);
  Window* original_focus = client::GetFocusClient(&child1)->GetFocusedWindow();
  Window* new_focus = &child1;
  ASSERT_NE(new_focus, original_focus);
  new_focus->Focus();
  ASSERT_TRUE(new_focus->HasFocus());

  // Simulate the server responding with a focus change.
  window_tree_client()->OnWindowFocused(server_id(&child2));

  // This shouldn't trigger focus changing yet.
  EXPECT_TRUE(child1.HasFocus());

  // Tell the client the change failed, which should trigger failing to the
  // most recent focus from server.
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, false));
  EXPECT_FALSE(child1.HasFocus());
  EXPECT_TRUE(child2.HasFocus());
  EXPECT_EQ(&child2, client::GetFocusClient(&child1)->GetFocusedWindow());

  // Simulate server changing focus to child1. Should take immediately.
  window_tree_client()->OnWindowFocused(server_id(&child1));
  EXPECT_TRUE(child1.HasFocus());
}

TEST_F(WindowTreeClientTest, FocusOnRemovedWindowWithoutInFlightFocusChange) {
  std::unique_ptr<Window> child1(std::make_unique<Window>(nullptr));
  child1->Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(child1.get());
  Window child2(nullptr);
  child2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child2);

  child1->Focus();
  // Acked for the focus change.
  EXPECT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, true));

  // Destroy child1, which should set focus to null.
  child1.reset(nullptr);
  EXPECT_EQ(nullptr, client::GetFocusClient(root_window())->GetFocusedWindow());
  // The reset of the focus shouldn't be sent to the server.
  EXPECT_EQ(0u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::FOCUS));

  // Server changes focus to 2.
  window_tree_client()->OnWindowFocused(server_id(&child2));
  // Should take effect immediately.
  EXPECT_TRUE(child2.HasFocus());
}

class ToggleVisibilityFromDestroyedObserver : public WindowObserver {
 public:
  explicit ToggleVisibilityFromDestroyedObserver(Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ToggleVisibilityFromDestroyedObserver() { EXPECT_FALSE(window_); }

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override {
    SetWindowVisibility(window, !window->TargetVisibility());
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ToggleVisibilityFromDestroyedObserver);
};

TEST_F(WindowTreeClientTest, ToggleVisibilityFromWindowDestroyed) {
  std::unique_ptr<Window> child(std::make_unique<Window>(nullptr));
  child->Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(child.get());
  ToggleVisibilityFromDestroyedObserver toggler(child.get());
  // Destroying the window triggers
  // ToggleVisibilityFromDestroyedObserver::OnWindowDestroyed(), which toggles
  // the visibility of the window. Ack the change, which should not crash or
  // trigger DCHECKs.
  child.reset();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, true));
}

TEST_F(WindowTreeClientTest, NewTopLevelWindow) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host->InitHost();
  EXPECT_FALSE(window_tree_host->window()->TargetVisibility());
  aura::Window* top_level = window_tree_host->window();
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());
  EXPECT_TRUE(window_tree_client_impl()->GetRoots().count(top_level) > 0u);

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  EXPECT_EQ(window_tree()->window_id(), server_id(top_level));

  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  const int64_t display_id = 1;
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, false, base::nullopt);

  EXPECT_FALSE(window_tree_host->window()->TargetVisibility());

  // Should not be able to add a top level as a child of another window.
  // TODO(sky): decide how to handle this.
  // root_window()->AddChild(top_level);
  // ASSERT_EQ(nullptr, top_level->parent());

  // Destroy the first root, shouldn't initiate tear down.
  window_tree_host.reset();
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());
}

TEST_F(WindowTreeClientTest, NewTopLevelWindowGetsPropertiesFromData) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());

  EXPECT_FALSE(IsWindowHostVisible(top_level));
  EXPECT_FALSE(top_level->TargetVisibility());

  window_tree_host.InitHost();
  EXPECT_FALSE(window_tree_host.window()->TargetVisibility());

  // Ack the request to the windowtree to create the new window.
  EXPECT_EQ(window_tree()->window_id(), server_id(top_level));

  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  data->bounds.SetRect(1, 2, 3, 4);
  data->visible = true;
  const int64_t display_id = 10;
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, true, base::nullopt);
  EXPECT_EQ(
      0u, window_tree()->GetChangeCountForType(WindowTreeChangeType::VISIBLE));

  // Make sure all the properties took.
  EXPECT_TRUE(IsWindowHostVisible(top_level));
  EXPECT_TRUE(top_level->TargetVisibility());
  EXPECT_EQ(display_id, window_tree_host.display_id());
  EXPECT_EQ(gfx::Rect(0, 0, 3, 4), top_level->bounds());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), top_level->GetHost()->GetBoundsInPixels());
}

TEST_F(WindowTreeClientTest, NewWindowGetsAllChangesInFlight) {
  RegisterTestProperties(GetPropertyConverter());

  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  EXPECT_FALSE(top_level->TargetVisibility());

  window_tree_host.InitHost();

  // Make visibility go from false->true->false. Don't ack immediately.
  top_level->Show();
  top_level->Hide();

  // Change bounds to 5, 6, 7, 8.
  window_tree_host.SetBoundsInPixels(gfx::Rect(5, 6, 7, 8));
  EXPECT_EQ(gfx::Rect(0, 0, 7, 8), window_tree_host.window()->bounds());

  const uint8_t explicitly_set_test_property1_value = 2;
  top_level->SetProperty(kTestPropertyKey1,
                         explicitly_set_test_property1_value);

  // Ack the new window top level top_level Vis and bounds shouldn't change.
  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  const gfx::Rect bounds_from_server(1, 2, 3, 4);
  data->bounds = bounds_from_server;
  data->visible = true;
  const uint8_t server_test_property1_value = 3;
  data->properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(server_test_property1_value);
  const uint8_t server_test_property2_value = 4;
  data->properties[kTestPropertyServerKey2] =
      ConvertToPropertyTransportValue(server_test_property2_value);
  const int64_t display_id = 1;
  // Get the id of the in flight change for creating the new top_level.
  uint32_t new_window_in_flight_change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &new_window_in_flight_change_id));
  window_tree_client()->OnTopLevelCreated(new_window_in_flight_change_id,
                                          std::move(data), display_id, true,
                                          base::nullopt);

  // The only value that should take effect is the property for 'yy' as it was
  // not in flight.
  EXPECT_FALSE(top_level->TargetVisibility());
  EXPECT_EQ(gfx::Rect(5, 6, 7, 8), window_tree_host.GetBoundsInPixels());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 8), top_level->bounds());
  EXPECT_EQ(explicitly_set_test_property1_value,
            top_level->GetProperty(kTestPropertyKey1));
  EXPECT_EQ(server_test_property2_value,
            top_level->GetProperty(kTestPropertyKey2));

  // Tell the client the changes failed. This should cause the values to change
  // to that of the server.
  ASSERT_TRUE(window_tree()->AckFirstChangeOfType(WindowTreeChangeType::VISIBLE,
                                                  false));
  EXPECT_FALSE(top_level->TargetVisibility());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_TRUE(top_level->TargetVisibility());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::BOUNDS, false);
  // The bounds of the top_level is always at the origin.
  EXPECT_EQ(gfx::Rect(bounds_from_server.size()), top_level->bounds());
  // But the bounds of the WindowTreeHost is display relative.
  EXPECT_EQ(bounds_from_server,
            top_level->GetRootWindow()->GetHost()->GetBoundsInPixels());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::PROPERTY, false);
  EXPECT_EQ(server_test_property1_value,
            top_level->GetProperty(kTestPropertyKey1));
  EXPECT_EQ(server_test_property2_value,
            top_level->GetProperty(kTestPropertyKey2));
}

TEST_F(WindowTreeClientTest, NewWindowGetsProperties) {
  RegisterTestProperties(GetPropertyConverter());
  Window window(nullptr);
  const uint8_t explicitly_set_test_property1_value = 29;
  window.SetProperty(kTestPropertyKey1, explicitly_set_test_property1_value);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>
      transport_properties = window_tree()->GetLastNewWindowProperties();
  ASSERT_TRUE(transport_properties.has_value());
  std::map<std::string, std::vector<uint8_t>> properties =
      mojo::FlatMapToMap(*transport_properties);
  ASSERT_EQ(1u, properties.count(kTestPropertyServerKey1));
  // PropertyConverter uses int64_t values, even for smaller types like uint8_t.
  ASSERT_EQ(8u, properties[kTestPropertyServerKey1].size());
  EXPECT_EQ(static_cast<int64_t>(explicitly_set_test_property1_value),
            mojo::ConvertTo<int64_t>(properties[kTestPropertyServerKey1]));
  ASSERT_EQ(0u, properties.count(kTestPropertyServerKey2));
}

// Assertions around transient windows.
TEST_F(WindowTreeClientTest, Transients) {
  aura::Window root_window(nullptr);
  root_window.Init(ui::LAYER_NOT_DRAWN);
  client::TransientWindowClient* transient_client =
      client::GetTransientWindowClient();
  Window parent(nullptr);
  parent.Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(&parent);
  Window transient(nullptr);
  transient.Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(&transient);
  window_tree()->AckAllChanges();
  transient_client->AddTransientChild(&parent, &transient);
  ASSERT_EQ(1u, window_tree()->GetChangeCountForType(
                    WindowTreeChangeType::ADD_TRANSIENT));
  EXPECT_EQ(server_id(&parent), window_tree()->transient_data().parent_id);
  EXPECT_EQ(server_id(&transient), window_tree()->transient_data().child_id);

  // Remove from the server side.
  window_tree_client()->OnTransientWindowRemoved(server_id(&parent),
                                                 server_id(&transient));
  EXPECT_EQ(nullptr, transient_client->GetTransientParent(&transient));
  window_tree()->AckAllChanges();

  // Add from the server.
  window_tree_client()->OnTransientWindowAdded(server_id(&parent),
                                               server_id(&transient));
  EXPECT_EQ(&parent, transient_client->GetTransientParent(&transient));

  // Remove locally.
  transient_client->RemoveTransientChild(&parent, &transient);
  ASSERT_EQ(1u, window_tree()->GetChangeCountForType(
                    WindowTreeChangeType::REMOVE_TRANSIENT));
  EXPECT_EQ(server_id(&transient), window_tree()->transient_data().child_id);
}

TEST_F(WindowTreeClientTest, DontRestackTransientsFromOtherClients) {
  // Create a window from another client with 3 children.
  const int32_t other_client_id = 11 << 16;
  int32_t other_client_window_id = 1;
  std::unique_ptr<Window> other_client_window = CreateWindowUsingId(
      window_tree_client_impl(), other_client_id | other_client_window_id++,
      root_window());
  std::unique_ptr<Window> other_client_child_window1 = CreateWindowUsingId(
      window_tree_client_impl(), other_client_id | other_client_window_id++,
      other_client_window.get());
  std::unique_ptr<Window> other_client_child_window2 = CreateWindowUsingId(
      window_tree_client_impl(), other_client_id | other_client_window_id++,
      other_client_window.get());
  std::unique_ptr<Window> other_client_child_window3 = CreateWindowUsingId(
      window_tree_client_impl(), other_client_id | other_client_window_id++,
      other_client_window.get());
  window_tree()->AckAllChanges();

  // Make |other_client_child_window3| a transient child of
  // |other_client_child_window1|. This should *not* reorder locally as the
  // windows are parented to a window owned by another client.
  window_tree_client()->OnTransientWindowAdded(
      server_id(other_client_child_window1.get()),
      server_id(other_client_child_window3.get()));
  // There should be no changes sent to the server, and the children should
  // not have been reordered.
  EXPECT_EQ(0u, window_tree()->number_of_changes());
  ASSERT_EQ(3u, other_client_window->children().size());
  EXPECT_EQ(other_client_child_window2.get(),
            other_client_window->children()[1]);

  // Make sure transient was wired correctly though.
  client::TransientWindowClient* transient_client =
      client::GetTransientWindowClient();
  EXPECT_EQ(
      other_client_child_window1.get(),
      transient_client->GetTransientParent(other_client_child_window3.get()));
}

// Verifies adding/removing a transient child notifies the server of the restack
// when the change originates from the server.
TEST_F(WindowTreeClientTest, TransientChildServerMutateNotifiesOfRestack) {
  aura::Window root_window(nullptr);
  root_window.Init(ui::LAYER_NOT_DRAWN);
  Window* w1 = new Window(nullptr);
  w1->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w1);
  Window* w2 = new Window(nullptr);
  w2->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w2);
  Window* w3 = new Window(nullptr);
  w3->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w3);
  // Three children of root: |w1|, |w2| and |w3| (in that order). Make |w1| a
  // transient child of |w2|. Should trigger moving |w1| on top of |w2|, but not
  // notify the server of the reorder.
  window_tree()->AckAllChanges();
  window_tree_client()->OnTransientWindowAdded(server_id(w2), server_id(w1));
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w1, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  // Only reorders should be generated.
  EXPECT_NE(0u, window_tree()->number_of_changes());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::REORDER, true);
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Make |w3| also a transient child of |w2|.
  window_tree_client()->OnTransientWindowAdded(server_id(w2), server_id(w3));
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w1, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  // Only reorders should be generated.
  EXPECT_NE(0u, window_tree()->number_of_changes());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::REORDER, true);
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Remove |w1| as a transient child, this should move |w3| on top of |w2|.
  window_tree_client()->OnTransientWindowRemoved(server_id(w2), server_id(w1));
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w3, root_window.children()[1]);
  EXPECT_EQ(w1, root_window.children()[2]);
  // Only reorders should be generated.
  EXPECT_NE(0u, window_tree()->number_of_changes());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::REORDER, true);
  EXPECT_EQ(0u, window_tree()->number_of_changes());
}

// Verifies adding/removing a transient child notifies the server of the
// restacks;
TEST_F(WindowTreeClientTest, TransientChildClientMutateNotifiesOfRestack) {
  aura::Window root_window(nullptr);
  root_window.Init(ui::LAYER_NOT_DRAWN);

  client::TransientWindowClient* transient_client =
      client::GetTransientWindowClient();
  Window* w1 = new Window(nullptr);
  w1->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w1);
  Window* w2 = new Window(nullptr);
  w2->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w2);
  Window* w3 = new Window(nullptr);
  w3->Init(ui::LAYER_NOT_DRAWN);
  root_window.AddChild(w3);
  // Three children of root: |w1|, |w2| and |w3| (in that order). Make |w1| a
  // transient child of |w2|. Should trigger moving |w1| on top of |w2|, and
  // notify notify the server of the reorder.
  window_tree()->AckAllChanges();
  transient_client->AddTransientChild(w2, w1);
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w1, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::ADD_TRANSIENT, true));
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::REORDER, true));
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Make |w3| also a transient child of |w2|. Order shouldn't change.
  transient_client->AddTransientChild(w2, w3);
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w1, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::ADD_TRANSIENT, true));
  // While the order doesn't change, internally aura shuffles things around,
  // hence the REORDERs.
  EXPECT_NE(0u, window_tree()->number_of_changes());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::REORDER, true);
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Remove |w1| as a transient child, this should move |w3| on top of |w2|.
  transient_client->RemoveTransientChild(w2, w1);
  EXPECT_EQ(w2, root_window.children()[0]);
  EXPECT_EQ(w3, root_window.children()[1]);
  EXPECT_EQ(w1, root_window.children()[2]);
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::REMOVE_TRANSIENT, true));
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::REORDER, true));
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Make |w1| the first child and ensure a REORDER was scheduled.
  root_window.StackChildAtBottom(w1);
  EXPECT_EQ(w1, root_window.children()[0]);
  EXPECT_EQ(w2, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  EXPECT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::REORDER, true));
  EXPECT_EQ(0u, window_tree()->number_of_changes());

  // Try stacking |w2| above |w3|. This should be disallowed as that would
  // result in placing |w2| above its transient child.
  root_window.StackChildAbove(w2, w3);
  EXPECT_EQ(w1, root_window.children()[0]);
  EXPECT_EQ(w2, root_window.children()[1]);
  EXPECT_EQ(w3, root_window.children()[2]);
  // The stack above is followed by a reorder from TransientWindowManager,
  // hence multiple changes.
  EXPECT_NE(0u, window_tree()->number_of_changes());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::REORDER, true);
  EXPECT_EQ(0u, window_tree()->number_of_changes());
}

TEST_F(WindowTreeClientTest, TopLevelWindowDestroyedBeforeCreateComplete) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host->InitHost();
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());

  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = server_id(window_tree_host->window());

  // Destroy the window before the server has a chance to ack the window
  // creation.
  window_tree_host.reset();
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());

  // Get the id of the in flight change for creating the new window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));

  const int64_t display_id = 1;
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, true, base::nullopt);
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());
}

TEST_F(WindowTreeClientTest, NewTopLevelWindowGetsProperties) {
  RegisterTestProperties(GetPropertyConverter());
  const uint8_t property_value = 11;
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(property_value);
  const char kUnknownPropertyKey[] = "unknown-property";
  using UnknownPropertyType = int32_t;
  const UnknownPropertyType kUnknownPropertyValue = 101;
  properties[kUnknownPropertyKey] =
      mojo::ConvertTo<std::vector<uint8_t>>(kUnknownPropertyValue);
  WindowTreeHostMus window_tree_host(CreateInitParamsForTopLevel(
      window_tree_client_impl(), std::move(properties)));
  window_tree_host.InitHost();
  window_tree_host.window()->Show();
  // Verify the property made it to the window.
  EXPECT_EQ(property_value,
            window_tree_host.window()->GetProperty(kTestPropertyKey1));

  // Get the id of the in flight change for creating the new top level window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));

  // Verify the properties were sent to the server.
  base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>
      transport_properties = window_tree()->GetLastNewWindowProperties();
  ASSERT_TRUE(transport_properties.has_value());
  std::map<std::string, std::vector<uint8_t>> properties2 =
      mojo::FlatMapToMap(*transport_properties);
  ASSERT_EQ(1u, properties2.count(kTestPropertyServerKey1));
  // PropertyConverter uses int64_t values, even for smaller types like uint8_t.
  ASSERT_EQ(8u, properties2[kTestPropertyServerKey1].size());
  EXPECT_EQ(static_cast<int64_t>(property_value),
            mojo::ConvertTo<int64_t>(properties2[kTestPropertyServerKey1]));

  ASSERT_EQ(1u, properties2.count(kUnknownPropertyKey));
  ASSERT_EQ(sizeof(UnknownPropertyType),
            properties2[kUnknownPropertyKey].size());
  EXPECT_EQ(kUnknownPropertyValue, mojo::ConvertTo<UnknownPropertyType>(
                                       properties2[kUnknownPropertyKey]));
}

namespace {

class CloseWindowWindowTreeHostObserver : public aura::WindowTreeHostObserver {
 public:
  CloseWindowWindowTreeHostObserver() {}
  ~CloseWindowWindowTreeHostObserver() override {}

  bool root_destroyed() const { return root_destroyed_; }

  // aura::WindowTreeHostObserver::
  void OnHostCloseRequested(aura::WindowTreeHost* host) override {
    root_destroyed_ = true;
  }

 private:
  bool root_destroyed_ = false;

  DISALLOW_COPY_AND_ASSIGN(CloseWindowWindowTreeHostObserver);
};

}  // namespace

TEST_F(WindowTreeClientTest, CloseWindow) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host.InitHost();
  CloseWindowWindowTreeHostObserver observer;
  window_tree_host.AddObserver(&observer);
  Window* top_level = window_tree_host.window();

  // Close a root window should send close request to the observer of its
  // WindowTreeHost.
  EXPECT_FALSE(observer.root_destroyed());
  window_tree_client()->RequestClose(server_id(top_level));
  EXPECT_TRUE(observer.root_destroyed());
}

// Tests both SetCapture and ReleaseCapture, to ensure that Window is properly
// updated on failures.
TEST_F(WindowTreeClientTest, ExplicitCapture) {
  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(root_window()->HasCapture());

  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_TRUE(root_window()->HasCapture());

  root_window()->ReleaseCapture();
  EXPECT_FALSE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_TRUE(root_window()->HasCapture());

  root_window()->ReleaseCapture();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_FALSE(root_window()->HasCapture());
}

// Tests that when capture is lost, while there is a release capture request
// inflight, that the revert value of that request is updated correctly.
TEST_F(WindowTreeClientTest, LostCaptureDifferentInFlightChange) {
  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_TRUE(root_window()->HasCapture());

  // The ReleaseCapture should be updated to the revert of the SetCapture.
  root_window()->ReleaseCapture();

  window_tree_client()->OnCaptureChanged(0, server_id(root_window()));
  EXPECT_FALSE(root_window()->HasCapture());

  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(root_window()->HasCapture());
}

// Tests that while two windows can inflight capture requests, that the
// WindowTreeClient only identifies one as having the current capture.
TEST_F(WindowTreeClientTest, TwoWindowsRequestCapture) {
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child);
  child.Show();

  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());

  child.SetCapture();
  EXPECT_TRUE(child.HasCapture());
  EXPECT_FALSE(root_window()->HasCapture());

  ASSERT_TRUE(
      window_tree()->AckFirstChangeOfType(WindowTreeChangeType::CAPTURE, true));
  EXPECT_FALSE(root_window()->HasCapture());
  EXPECT_TRUE(child.HasCapture());

  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(child.HasCapture());
  EXPECT_TRUE(root_window()->HasCapture());

  window_tree_client()->OnCaptureChanged(0, server_id(root_window()));
  EXPECT_FALSE(root_window()->HasCapture());
}

TEST_F(WindowTreeClientTest, WindowDestroyedWhileTransientChildHasCapture) {
  std::unique_ptr<Window> transient_parent(std::make_unique<Window>(nullptr));
  transient_parent->Init(ui::LAYER_NOT_DRAWN);
  // Owned by |transient_parent|.
  Window* transient_child = new Window(nullptr);
  transient_child->Init(ui::LAYER_NOT_DRAWN);
  transient_parent->Show();
  transient_child->Show();
  root_window()->AddChild(transient_parent.get());
  root_window()->AddChild(transient_child);

  client::GetTransientWindowClient()->AddTransientChild(transient_parent.get(),
                                                        transient_child);

  WindowTracker tracker;
  tracker.Add(transient_parent.get());
  tracker.Add(transient_child);
  // Request a capture on the transient child, then destroy the transient
  // parent. That will destroy both windows, and should reset the capture window
  // correctly.
  transient_child->SetCapture();
  transient_parent.reset();
  EXPECT_TRUE(tracker.windows().empty());

  // Create a new Window, and attempt to place capture on that.
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  child.Show();
  root_window()->AddChild(&child);
  child.SetCapture();
  EXPECT_TRUE(child.HasCapture());
}

namespace {

class CaptureRecorder : public client::CaptureClientObserver {
 public:
  explicit CaptureRecorder(Window* root_window) : root_window_(root_window) {
    client::GetCaptureClient(root_window)->AddObserver(this);
  }

  ~CaptureRecorder() override {
    client::GetCaptureClient(root_window_)->RemoveObserver(this);
  }

  void reset_capture_captured_count() { capture_changed_count_ = 0; }
  int capture_changed_count() const { return capture_changed_count_; }
  int last_gained_capture_window_id() const {
    return last_gained_capture_window_id_;
  }
  int last_lost_capture_window_id() const {
    return last_lost_capture_window_id_;
  }

  // client::CaptureClientObserver:
  void OnCaptureChanged(Window* lost_capture, Window* gained_capture) override {
    capture_changed_count_++;
    last_gained_capture_window_id_ = gained_capture ? gained_capture->id() : 0;
    last_lost_capture_window_id_ = lost_capture ? lost_capture->id() : 0;
  }

 private:
  Window* root_window_;
  int capture_changed_count_ = 0;
  int last_gained_capture_window_id_ = 0;
  int last_lost_capture_window_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CaptureRecorder);
};

}  // namespace

TEST_F(WindowTreeClientTest, OnWindowTreeCaptureChanged) {
  CaptureRecorder capture_recorder(root_window());

  std::unique_ptr<Window> child1(std::make_unique<Window>(nullptr));
  const int child1_id = 1;
  child1->Init(ui::LAYER_NOT_DRAWN);
  child1->set_id(child1_id);
  child1->Show();
  root_window()->AddChild(child1.get());

  Window child2(nullptr);
  const int child2_id = 2;
  child2.Init(ui::LAYER_NOT_DRAWN);
  child2.set_id(child2_id);
  child2.Show();
  root_window()->AddChild(&child2);

  EXPECT_EQ(0, capture_recorder.capture_changed_count());
  // Give capture to child1 and ensure everyone is notified correctly.
  child1->SetCapture();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child1_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Deleting a window with capture should notify observers as well.
  child1.reset();

  // No capture change is sent during deletion (the server side sees the window
  // deletion too and resets internal state).
  EXPECT_EQ(
      0u, window_tree()->GetChangeCountForType(WindowTreeChangeType::CAPTURE));

  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(0, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(child1_id, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Changes originating from server should notify observers too.
  window_tree_client()->OnCaptureChanged(server_id(&child2), 0);
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child2_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();
}

TEST_F(WindowTreeClientTest, TwoWindowTreesRequestCapture) {
  std::unique_ptr<TopLevel> top_level1 = CreateWindowTreeHostForTopLevel();
  std::unique_ptr<TopLevel> top_level2 = CreateWindowTreeHostForTopLevel();

  aura::Window* root1 = top_level1->host->window();
  aura::Window* root2 = top_level2->host->window();
  auto capture_recorder1 = std::make_unique<CaptureRecorder>(root1);
  auto capture_recorder2 = std::make_unique<CaptureRecorder>(root2);
  EXPECT_NE(client::GetCaptureClient(root1), client::GetCaptureClient(root2));

  EXPECT_EQ(0, capture_recorder1->capture_changed_count());
  EXPECT_EQ(0, capture_recorder2->capture_changed_count());
  // Give capture to root2 and ensure everyone is notified correctly.
  root2->SetCapture();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_EQ(0, capture_recorder1->capture_changed_count());
  EXPECT_EQ(1, capture_recorder2->capture_changed_count());
  EXPECT_EQ(root2->id(), capture_recorder2->last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder2->last_lost_capture_window_id());
  root2->ReleaseCapture();
  capture_recorder1->reset_capture_captured_count();
  capture_recorder2->reset_capture_captured_count();

  // Releasing capture of root2 shouldn't affect root1 capture.
  root2->SetCapture();
  root1->SetCapture();
  root2->ReleaseCapture();
  EXPECT_EQ(1, capture_recorder1->capture_changed_count());
  EXPECT_EQ(2, capture_recorder2->capture_changed_count());
  EXPECT_EQ(root1->id(), capture_recorder1->last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder1->last_lost_capture_window_id());
  EXPECT_EQ(0, capture_recorder2->last_gained_capture_window_id());
  EXPECT_EQ(root2->id(), capture_recorder2->last_lost_capture_window_id());
  capture_recorder1->reset_capture_captured_count();
  capture_recorder2->reset_capture_captured_count();

  // Destroying top_level2 shouldn't affect root1 capture; see crbug.com/873743.
  auto* synchronizer = window_tree_client_impl()->capture_synchronizer();
  EXPECT_EQ(WindowMus::Get(root1), synchronizer->capture_window());
  synchronizer->DetachFromCaptureClient(top_level2->capture_client.get());
  capture_recorder2.reset();
  top_level2->host.reset();
  EXPECT_EQ(WindowMus::Get(root1), synchronizer->capture_window());
  EXPECT_EQ(0, capture_recorder1->capture_changed_count());
  EXPECT_EQ(root1->id(), capture_recorder1->last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder1->last_lost_capture_window_id());
}

TEST_F(WindowTreeClientTest, ModalTypeWindowFail) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetProperty(client::kModalKey, ui::MODAL_TYPE_WINDOW);
  // Make sure server was told about it, and have the server say it failed.
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
  // Type should be back to MODAL_TYPE_NONE as the server didn't accept the
  // change.
  EXPECT_EQ(ui::MODAL_TYPE_NONE, window.GetProperty(client::kModalKey));
  // Server is told that the type is set back to MODAL_TYPE_NONE.
  EXPECT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, true));
  // Type should still remain MODAL_TYPE_NONE.
  EXPECT_EQ(ui::MODAL_TYPE_NONE, window.GetProperty(client::kModalKey));
}

TEST_F(WindowTreeClientTest, ModalTypeNoneFail) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  // First, set modality type to window sucessfully.
  window.SetProperty(client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, true));
  EXPECT_EQ(ui::MODAL_TYPE_WINDOW, window.GetProperty(client::kModalKey));
  // Now, set type to MODAL_TYPE_NONE, and have the server say it failed.
  window.SetProperty(client::kModalKey, ui::MODAL_TYPE_NONE);
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
  // Type should be back to MODAL_TYPE_WINDOW as the server didn't accept the
  // change.
  EXPECT_EQ(ui::MODAL_TYPE_WINDOW, window.GetProperty(client::kModalKey));
  // Server is told that the type is set back to MODAL_TYPE_WINDOW.
  EXPECT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, true));
  // Type should still remain MODAL_TYPE_WINDOW.
  EXPECT_EQ(ui::MODAL_TYPE_WINDOW, window.GetProperty(client::kModalKey));
}

TEST_F(WindowTreeClientTest, ModalTypeSuccess) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);

  // Set modality type to MODAL_TYPE_WINDOW, MODAL_TYPE_SYSTEM, and then back to
  // MODAL_TYPE_NONE, and make sure it succeeds each time.
  ui::ModalType kModalTypes[] = {ui::MODAL_TYPE_WINDOW, ui::MODAL_TYPE_SYSTEM,
                                 ui::MODAL_TYPE_NONE};
  for (size_t i = 0; i < arraysize(kModalTypes); i++) {
    window.SetProperty(client::kModalKey, kModalTypes[i]);
    // Ack change as succeeding.
    ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
        WindowTreeChangeType::MODAL, true));
    EXPECT_EQ(kModalTypes[i], window.GetProperty(client::kModalKey));
  }

  // There should be no more modal changes.
  EXPECT_FALSE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
}

// Verifies OnWindowHierarchyChanged() deals correctly with identifying existing
// windows.
TEST_F(WindowTreeClientTest, OnWindowHierarchyChangedWithExistingWindow) {
  Window* window1 = new Window(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  Window* window2 = new Window(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  window_tree()->AckAllChanges();
  const ws::Id server_window_id = server_id(root_window()) + 11;
  ws::mojom::WindowDataPtr data1 = ws::mojom::WindowData::New();
  ws::mojom::WindowDataPtr data2 = ws::mojom::WindowData::New();
  ws::mojom::WindowDataPtr data3 = ws::mojom::WindowData::New();
  data1->parent_id = server_id(root_window());
  data1->window_id = server_window_id;
  data1->bounds = gfx::Rect(1, 2, 3, 4);
  data2->parent_id = server_window_id;
  data2->window_id = WindowMus::Get(window1)->server_id();
  data2->bounds = gfx::Rect(1, 2, 3, 4);
  data3->parent_id = server_window_id;
  data3->window_id = WindowMus::Get(window2)->server_id();
  data3->bounds = gfx::Rect(1, 2, 3, 4);
  std::vector<ws::mojom::WindowDataPtr> data_array(3);
  data_array[0] = std::move(data1);
  data_array[1] = std::move(data2);
  data_array[2] = std::move(data3);
  window_tree_client()->OnWindowHierarchyChanged(
      server_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* server_window = root_window()->children()[0];
  EXPECT_EQ(window1->parent(), server_window);
  EXPECT_EQ(window2->parent(), server_window);
  ASSERT_EQ(2u, server_window->children().size());
  EXPECT_EQ(window1, server_window->children()[0]);
  EXPECT_EQ(window2, server_window->children()[1]);
}

// Ensures when WindowTreeClient::OnWindowDeleted() is called nothing is
// scheduled on the server side.
TEST_F(WindowTreeClientTest, OnWindowDeletedDoesntNotifyServer) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window* window2 = new Window(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  window1.AddChild(window2);
  window_tree()->AckAllChanges();
  window_tree_client()->OnWindowDeleted(server_id(window2));
  EXPECT_FALSE(window_tree()->has_change());
}

TEST_F(WindowTreeClientTestHighDPI, SetBounds) {
  const gfx::Rect original_bounds(root_window()->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root_window()->bounds());
  root_window()->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, root_window()->bounds());

  // Simulate the server responding with a bounds change. Server operates in
  // dips.
  const gfx::Rect server_changed_bounds(gfx::Rect(0, 0, 200, 200));
  window_tree_client()->OnWindowBoundsChanged(
      server_id(root_window()), original_bounds, server_changed_bounds,
      base::nullopt);
  EXPECT_EQ(server_changed_bounds, root_window()->bounds());
}

TEST_F(WindowTreeClientTestHighDPI, NewTopLevelWindowBounds) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  Window* top_level = window_tree_host.window();
  window_tree_host.InitHost();

  gfx::Rect bounds(2, 4, 6, 8);
  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  data->bounds = bounds;
  const int64_t display_id = 10;
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, true, base::nullopt);

  // aura::Window should operate in DIP and aura::WindowTreeHost should operate
  // in pixels. Only the size is compared for |top_level| as the WindowTreeHost
  // is the place that maintains the location.
  EXPECT_EQ(bounds.size(), top_level->bounds().size());
  const float device_scale_factor = 2.0f;
  EXPECT_EQ(gfx::ConvertRectToPixel(device_scale_factor, bounds),
            top_level->GetHost()->GetBoundsInPixels());
}

TEST_F(WindowTreeClientTestHighDPI, ObservedInputEventsInDip) {
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display primary_display = screen->GetPrimaryDisplay();
  ASSERT_EQ(2.0f, primary_display.device_scale_factor());

  std::unique_ptr<Window> top_level(std::make_unique<Window>(nullptr));
  top_level->SetType(client::WINDOW_TYPE_NORMAL);
  top_level->Init(ui::LAYER_NOT_DRAWN);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  // Start observing touch press events.
  TestEventObserver test_event_observer;
  top_level->env()->AddEventObserver(&test_event_observer, top_level->env(),
                                     {ui::ET_TOUCH_PRESSED});

  // Simulate the server sending an observed event in screen dip coordinates.
  const gfx::Point location(10, 12);
  ui::TouchEvent touch_press(
      ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  window_tree_client()->OnObservedInputEvent(ui::Event::Clone(touch_press));

  // The observer should have received the event in screen dip coordinates.
  const ui::Event* last_event = test_event_observer.last_event();
  ASSERT_TRUE(last_event);
  EXPECT_EQ(location, last_event->AsLocatedEvent()->location());
  EXPECT_EQ(location, last_event->AsLocatedEvent()->root_location());
}

TEST_F(WindowTreeClientTestHighDPI, InputEventsInDip) {
  WindowTreeHostMus window_tree_host(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display;
  ASSERT_TRUE(
      screen->GetDisplayWithDisplayId(window_tree_host.display_id(), &display));
  ASSERT_EQ(2.0f, display.device_scale_factor());

  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds_in_pixels(0, 0, 100, 100);
  window_tree_host.SetBoundsInPixels(bounds_in_pixels);
  window_tree_host.InitHost();
  window_tree_host.Show();
  EXPECT_EQ(gfx::ConvertRectToDIP(2.0f, bounds_in_pixels), top_level->bounds());
  EXPECT_EQ(bounds_in_pixels, window_tree_host.GetBoundsInPixels());

  InputEventBasicTestWindowDelegate window_delegate1(window_tree());
  Window child1(&window_delegate1);
  child1.Init(ui::LAYER_NOT_DRAWN);
  child1.SetEventTargeter(std::make_unique<test::TestWindowTargeter>());
  top_level->AddChild(&child1);
  child1.SetBounds(gfx::Rect(10, 10, 100, 100));
  child1.Show();
  InputEventBasicTestWindowDelegate window_delegate2(window_tree());
  Window child2(&window_delegate2);
  child2.Init(ui::LAYER_NOT_DRAWN);
  child1.AddChild(&child2);
  child2.SetBounds(gfx::Rect(20, 30, 100, 100));
  child2.Show();

  EXPECT_EQ(0, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());

  // child1 has a custom targeter set which would always return itself as the
  // target window therefore event should go to child1 and should be in dip.
  const gfx::Point event_location(50, 60);
  uint32_t event_id = 1;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child1), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());
  EXPECT_EQ(event_location, window_delegate1.last_event_location());
  window_delegate1.reset();
  window_delegate2.reset();

  // Event location will be transformed and should be in dip.
  event_id = 2;
  window_delegate1.set_event_id(event_id);
  window_delegate2.set_event_id(event_id);
  window_tree_client()->OnWindowInputEvent(
      event_id, server_id(&child2), window_tree_host.display_id(),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(event_id));
  EXPECT_EQ(ws::mojom::EventResult::HANDLED,
            window_tree()->GetEventResult(event_id));
  EXPECT_EQ(1, window_delegate1.move_count());
  EXPECT_EQ(0, window_delegate2.move_count());
  gfx::Point transformed_event_location(event_location.x() + 20,
                                        event_location.y() + 30);
  EXPECT_EQ(transformed_event_location, window_delegate1.last_event_location());
}

using WindowTreeClientDestructionTest = test::AuraTestBaseMus;

TEST_F(WindowTreeClientDestructionTest, Shutdown) {
  // Windows should be able to outlive the WindowTreeClient.
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  DeleteWindowTreeClient();

  // And it should be possible to create Windows after the WindowTreeClient has
  // been deleted.
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
}

TEST_F(WindowTreeClientDestructionTest, WindowsFromOtherConnectionsDeleted) {
  std::unique_ptr<Window> other_client_window =
      CreateWindowUsingId(window_tree_client_impl(), 10, nullptr);
  WindowTracker window_tracker;
  window_tracker.Add(other_client_window.get());
  other_client_window.release();
  DeleteWindowTreeClient();
  // Deleting WindowTreeClient should delete the Window that was in
  // |window_tracker|.
  EXPECT_TRUE(window_tracker.windows().empty());
}

class TestEmbedRootDelegate : public EmbedRootDelegate {
 public:
  TestEmbedRootDelegate() = default;
  ~TestEmbedRootDelegate() override = default;

  // EmbedRootDelegate:
  void OnEmbedTokenAvailable(const base::UnguessableToken& token) override {}
  void OnEmbed(Window* window) override {}
  void OnUnembed() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestEmbedRootDelegate);
};

// Verifies we don't crash when focus changes to a window in an EmbedRoot.
TEST_F(WindowTreeClientTest, ChangeFocusInEmbedRootWindow) {
  TestEmbedRootDelegate embed_root_delegate;
  std::unique_ptr<EmbedRoot> embed_root =
      window_tree_client_impl()->CreateEmbedRoot(&embed_root_delegate);
  WindowTreeClientPrivate(window_tree_client_impl())
      .CallOnEmbedFromToken(embed_root.get());
  ASSERT_TRUE(embed_root->window());
  window_tree_client()->OnWindowFocused(server_id(embed_root->window()));
}

TEST_F(WindowTreeClientTest, PerformWindowMove) {
  int call_count = 0;
  bool last_result = false;

  WindowTreeHostMus* host_mus = static_cast<WindowTreeHostMus*>(host());
  host_mus->PerformWindowMove(
      ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      base::Bind(&OnWindowMoveDone, &call_count, &last_result));
  EXPECT_EQ(0, call_count);

  window_tree()->AckAllChanges();
  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(last_result);

  host_mus->PerformWindowMove(
      ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      base::Bind(&OnWindowMoveDone, &call_count, &last_result));
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::OTHER, false);
  EXPECT_EQ(2, call_count);
  EXPECT_FALSE(last_result);
}

TEST_F(WindowTreeClientTest, PerformWindowMoveDoneAfterDelete) {
  int call_count = 0;
  bool last_result = false;

  auto host_mus = std::make_unique<WindowTreeHostMus>(
      CreateInitParamsForTopLevel(window_tree_client_impl()));
  host_mus->InitHost();
  window_tree()->AckAllChanges();

  host_mus->PerformWindowMove(
      ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      base::Bind(&OnWindowMoveDone, &call_count, &last_result));
  EXPECT_EQ(0, call_count);

  host_mus.reset();
  window_tree()->AckAllChanges();

  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(last_result);
}

// Verifies occlusion state from server is applied to underlying window.
TEST_F(WindowTreeClientTest, OcclusionStateFromServer) {
  struct {
    const char* name;
    bool window_is_visible;
    ws::mojom::OcclusionState changed_state_from_server;
    Window::OcclusionState expected_state;
  } kTestCases[] = {
      // VISIBLE is set when window is visible.
      {"visible-set", true, ws::mojom::OcclusionState::kVisible,
       Window::OcclusionState::VISIBLE},
      // VISIBLE is not set when window hidden.
      {"visible-not-set", false, ws::mojom::OcclusionState::kVisible,
       Window::OcclusionState::HIDDEN},

      // OCCLUDED is always set.
      {"occluded-with-visible", true, ws::mojom::OcclusionState::kOccluded,
       Window::OcclusionState::OCCLUDED},
      {"occluded-with-invisible", false, ws::mojom::OcclusionState::kOccluded,
       Window::OcclusionState::OCCLUDED},

      // HIDDEN is set when window target visibility is false.
      {"hidden-set", false, ws::mojom::OcclusionState::kHidden,
       Window::OcclusionState::HIDDEN},
      // HIDDEN is not set when window target visibility is true.
      {"hidden-not-set", true, ws::mojom::OcclusionState::kHidden,
       Window::OcclusionState::VISIBLE},
  };

  for (const auto& test : kTestCases) {
    Window window(nullptr);
    window.Init(ui::LAYER_NOT_DRAWN);
    window.set_owned_by_parent(false);
    root_window()->AddChild(&window);

    window.TrackOcclusionState();
    ASSERT_EQ(Window::OcclusionState::HIDDEN, window.occlusion_state());

    if (test.window_is_visible && !window.IsVisible()) {
      window.Show();
    } else if (!test.window_is_visible && window.IsVisible()) {
      window.Hide();
    }

    window_tree_client()->OnOcclusionStateChanged(
        server_id(&window), test.changed_state_from_server);
    EXPECT_EQ(test.expected_state, window.occlusion_state()) << test.name;
  }
}

}  // namespace aura
