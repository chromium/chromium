// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <cursor-shapes-unstable-v1-client-protocol.h>
#include <linux/input.h>
#include <wayland-server-core.h>
#include <xdg-shell-server-protocol.h>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection_test_api.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_region.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_util.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "wayland-server-protocol.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/platform/wayland/host/wayland_async_cursor.h"
#endif

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::Values;

namespace ui {

namespace {

constexpr float kDefaultCursorScale = 1.f;

constexpr uint32_t kAugmentedSurfaceNotSupportedVersion = 0;

struct PopupPosition {
  gfx::Rect anchor_rect;
  gfx::Size size;
  uint32_t anchor = 0;
  uint32_t gravity = 0;
  uint32_t constraint_adjustment = 0;
};

base::ScopedFD MakeFD() {
  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  auto file =
      base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_CREATE_ALWAYS);
  return base::ScopedFD(file.TakePlatformFile());
}

// Must happen on the server thread.
wl::TestXdgPopup* GetTestXdgPopupByWindow(wl::TestWaylandServerThread* server,
                                          const uint32_t surface_id) {
  DCHECK(server->task_runner()->BelongsToCurrentThread());
  wl::MockSurface* mock_surface =
      server->GetObject<wl::MockSurface>(surface_id);
  if (mock_surface) {
    auto* mock_xdg_surface = mock_surface->xdg_surface();
    if (mock_xdg_surface) {
      return mock_xdg_surface->xdg_popup();
    }
  }
  return nullptr;
}

void AddStateToWlArray(uint32_t state, wl_array* states) {
  *static_cast<uint32_t*>(wl_array_add(states, sizeof state)) = state;
}

wl::ScopedWlArray InitializeWlArrayWithActivatedState() {
  return wl::ScopedWlArray({XDG_TOPLEVEL_STATE_ACTIVATED});
}

wl::ScopedWlArray MakeStateArray(const std::vector<int32_t> states) {
  return wl::ScopedWlArray(states);
}

class MockZcrCursorShapes : public WaylandZcrCursorShapes {
 public:
  MockZcrCursorShapes() : WaylandZcrCursorShapes(nullptr, nullptr) {}
  MockZcrCursorShapes(const MockZcrCursorShapes&) = delete;
  MockZcrCursorShapes& operator=(const MockZcrCursorShapes&) = delete;
  ~MockZcrCursorShapes() override = default;

  MOCK_METHOD(void, SetCursorShape, (int32_t), (override));
};

using BoundsChange = PlatformWindowDelegate::BoundsChange;

constexpr BoundsChange kDefaultBoundsChange{false};

scoped_refptr<PlatformCursor> AsPlatformCursor(
    scoped_refptr<BitmapCursor> bitmap_cursor) {
#if BUILDFLAG(IS_LINUX)
  return base::MakeRefCounted<WaylandAsyncCursor>(bitmap_cursor);
#else
  return bitmap_cursor;
#endif
}

}  // namespace

class WaylandWindowTest : public WaylandTest {
 public:
  WaylandWindowTest()
      : test_mouse_event_(ET_MOUSE_PRESSED,
                          gfx::Point(10, 15),
                          gfx::Point(10, 15),
                          ui::EventTimeStampFromSeconds(123456),
                          EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
                          EF_LEFT_MOUSE_BUTTON) {}

  WaylandWindowTest(const WaylandWindowTest&) = delete;
  WaylandWindowTest& operator=(const WaylandWindowTest&) = delete;

  void SetUp() override {
    WaylandTest::SetUp();

    surface_id_ = window_->root_surface()->get_surface_id();
    PostToServerAndWait(
        [id = surface_id_](wl::TestWaylandServerThread* server) {
          auto* surface = server->GetObject<wl::MockSurface>(id);
          ASSERT_TRUE(surface->xdg_surface());
        });
  }

 protected:
  void SendConfigureEventPopup(WaylandWindow* menu_window,
                               const gfx::Rect& bounds) {
    const uint32_t surface_id = menu_window->root_surface()->get_surface_id();
    PostToServerAndWait(
        [surface_id, bounds](wl::TestWaylandServerThread* server) {
          auto* popup = GetTestXdgPopupByWindow(server, surface_id);
          ASSERT_TRUE(popup);
          xdg_popup_send_configure(popup->resource(), bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height());
        });
  }

  // Simulates up to date buffers coming through viz and being latched.
  // Call this after configures or anything where you want wayland or latched
  // state to update.
  void AdvanceFrameToCurrent(
      WaylandWindow* window,
      const MockWaylandPlatformWindowDelegate& delegate) {
    AdvanceFrameToGivenVizSequenceId(window, delegate, delegate.viz_seq());
  }

  void AdvanceFrameToGivenVizSequenceId(
      WaylandWindow* window,
      const MockWaylandPlatformWindowDelegate& delegate,
      int64_t viz_seq) {
    wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
    window->OnSequencePoint(viz_seq);
    window->root_surface()->ApplyPendingState();
  }

  void InitializeWithSupportedHitTestValues(std::vector<int>* hit_tests) {
    hit_tests->push_back(static_cast<int>(HTBOTTOM));
    hit_tests->push_back(static_cast<int>(HTBOTTOMLEFT));
    hit_tests->push_back(static_cast<int>(HTBOTTOMRIGHT));
    hit_tests->push_back(static_cast<int>(HTLEFT));
    hit_tests->push_back(static_cast<int>(HTRIGHT));
    hit_tests->push_back(static_cast<int>(HTTOP));
    hit_tests->push_back(static_cast<int>(HTTOPLEFT));
    hit_tests->push_back(static_cast<int>(HTTOPRIGHT));
  }

  MockZcrCursorShapes* InstallMockZcrCursorShapes() {
    auto zcr_cursor_shapes = std::make_unique<MockZcrCursorShapes>();
    MockZcrCursorShapes* mock_cursor_shapes = zcr_cursor_shapes.get();
    WaylandConnectionTestApi test_api(connection_.get());
    test_api.SetZcrCursorShapes(std::move(zcr_cursor_shapes));
    return mock_cursor_shapes;
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(&delegate_);
  }

  void VerifyXdgPopupPosition(WaylandWindow* menu_window,
                              const PopupPosition& position) {
    const uint32_t surface_id = menu_window->root_surface()->get_surface_id();
    PostToServerAndWait([surface_id,
                         position](wl::TestWaylandServerThread* server) {
      auto* popup = GetTestXdgPopupByWindow(server, surface_id);
      ASSERT_TRUE(popup);

      EXPECT_EQ(popup->anchor_rect(), position.anchor_rect);
      EXPECT_EQ(popup->size(), position.size);
      EXPECT_EQ(popup->anchor(), position.anchor);
      EXPECT_EQ(popup->gravity(), position.gravity);
      EXPECT_EQ(popup->constraint_adjustment(), position.constraint_adjustment);
    });
  }

  void VerifyCanDispatchMouseEvents(
      WaylandWindow* dispatching_window,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    auto* pointer_focused_window =
        connection_->window_manager()->GetCurrentPointerFocusedWindow();

    ASSERT_TRUE(pointer_focused_window);
    Event::DispatcherApi(&test_mouse_event_).set_target(pointer_focused_window);
    EXPECT_TRUE(dispatching_window->CanDispatchEvent(&test_mouse_event_));
    for (auto* window : non_dispatching_windows) {
      EXPECT_FALSE(window->CanDispatchEvent(&test_mouse_event_));
    }
  }

  void VerifyCanDispatchTouchEvents(
      const std::vector<WaylandWindow*>& dispatching_windows,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    ASSERT_LT(dispatching_windows.size(), 2u);
    auto* touch_focused_window =
        connection_->window_manager()->GetCurrentTouchFocusedWindow();
    // There must be focused window to dispatch.
    if (dispatching_windows.size() == 0) {
      EXPECT_FALSE(touch_focused_window);
    }

    PointerDetails pointer_details(EventPointerType::kTouch, 1);
    TouchEvent test_touch_event(ET_TOUCH_PRESSED, {1, 1}, base::TimeTicks(),
                                pointer_details);
    if (touch_focused_window) {
      Event::DispatcherApi(&test_touch_event).set_target(touch_focused_window);
    }
    for (auto* window : dispatching_windows) {
      EXPECT_TRUE(window->CanDispatchEvent(&test_touch_event));
    }
    for (auto* window : non_dispatching_windows) {
      // Make sure that the CanDispatcEvent works on release build.
#if DCHECK_IS_ON()
      // Disable DCHECK when enabled.
      window->disable_null_target_dcheck_for_testing();
#endif
      EXPECT_FALSE(window->CanDispatchEvent(&test_touch_event));
    }
  }

  void VerifyCanDispatchKeyEvents(
      const std::vector<WaylandWindow*>& dispatching_windows,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    ASSERT_LT(dispatching_windows.size(), 2u);
    auto* keyboard_focused_window =
        connection_->window_manager()->GetCurrentKeyboardFocusedWindow();

    // There must be focused window to dispatch.
    if (dispatching_windows.size() == 0) {
      EXPECT_FALSE(keyboard_focused_window);
    }

    KeyEvent test_key_event(ET_KEY_PRESSED, VKEY_0, 0);
    if (keyboard_focused_window) {
      Event::DispatcherApi(&test_key_event).set_target(keyboard_focused_window);
    }

    for (auto* window : dispatching_windows) {
      EXPECT_TRUE(window->CanDispatchEvent(&test_key_event));
    }
    for (auto* window : non_dispatching_windows) {
      // Make sure that the CanDispatcEvent works on release build.
#if DCHECK_IS_ON()
      // Disable DCHECK when enabled.
      window->disable_null_target_dcheck_for_testing();
#endif
      EXPECT_FALSE(window->CanDispatchEvent(&test_key_event));
    }
  }

  uint32_t GetObjIdForOutput(WaylandOutput::Id id) {
    auto* output_manager = connection_->wayland_output_manager();
    auto* wayland_output = output_manager->GetOutput(id);
    if (wayland_output) {
      return wl_proxy_get_id(
          reinterpret_cast<wl_proxy*>(wayland_output->get_output()));
    }
    return 0;
  }

  // Surface id of |window|'s the root surface. Stored for convenience.
  uint32_t surface_id_ = 0u;

  MouseEvent test_mouse_event_;
};

// Regression test for crbug.com/1433175
TEST_P(WaylandWindowTest, Shutdown) {
  window_->PrepareForShutdown();
  window_->OnDragSessionClose(mojom::DragOperation::kNone);
}

TEST_P(WaylandWindowTest, SetTitle) {
  window_->SetTitle(u"hello");
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    EXPECT_EQ("hello", surface->xdg_surface()->xdg_toplevel()->title());
  });
}

TEST_P(WaylandWindowTest, OnSequencePointConfiguresWaylandWindow) {
  constexpr gfx::Rect kNormalBounds{500, 300};

  // Configure event makes Wayland update bounds, but does not change toplevel
  // input region, opaque region or window geometry immediately. Such actions
  // are postponed to OnSequencePoint();
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())))
        .Times(0);
    EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
  });

  auto state = InitializeWlArrayWithActivatedState();
  constexpr uint32_t kConfigureSerial = 2u;
  SendConfigureEvent(surface_id_, kNormalBounds.size(), state,
                     kConfigureSerial);

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
    EXPECT_CALL(*xdg_surface, AckConfigure(kConfigureSerial));
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_));
    EXPECT_CALL(*mock_surface, SetInputRegion(_));
  });
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

// WaylandSurface state changes are sent to wayland compositor when
// ApplyPendingState() is called.
TEST_P(WaylandWindowTest, ApplyPendingStatesAndCommit) {
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // Set*() calls do not send wl_surface requests.
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetBufferScale(2)).Times(0);
  });

  std::vector<gfx::Rect> region_px = {gfx::Rect{500, 300}};
  window_->root_surface()->set_opaque_region(region_px);
  window_->root_surface()->set_input_region(region_px[0]);
  window_->root_surface()->set_surface_buffer_scale(2);

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // ApplyPendingState() generates wl_surface requests and Commit() causes a
    // wayland connection flush.
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
    EXPECT_CALL(*mock_surface, SetBufferScale(2)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);
  });

  window_->root_surface()->ApplyPendingState();
  window_->root_surface()->Commit();

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
}

// Checks that decoration insets do not change final bounds and that
// WaylandToplevelWindow::HandleToplevelConfigure does correct rounding when
// some sides of insets divides by 2 with remainder.
TEST_P(WaylandWindowTest, SetDecorationInsets) {
  constexpr gfx::Rect kNormalBounds{956, 556};
  constexpr auto kHiDpiScale = 2;
  const BoundsChange kHiDpiBounds{false};

  window_->SetBoundsInDIP(kNormalBounds);

  auto state = InitializeWlArrayWithActivatedState();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output = server->output();
    // Send the window to |output|.
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl_surface_send_enter(surface->resource(), output->resource());
  });

  // Set insets for normal DPI.
  const auto kDecorationInsets = gfx::Insets::TLBR(24, 28, 32, 28);
  auto bounds_with_insets = kNormalBounds;
  bounds_with_insets.Inset(kDecorationInsets);
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  PostToServerAndWait([id = surface_id_, bounds_with_insets](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::MockXdgSurface* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(bounds_with_insets));
  });
  window_->SetDecorationInsets(&kDecorationInsets);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::MockXdgSurface* xdg_surface = surface->xdg_surface();
    // No update to the window geometry.
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });

  SendConfigureEvent(surface_id_, bounds_with_insets.size(), state);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  // Change scale.  This is the only time when we expect the pixel position to
  // change.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kHiDpiBounds))).Times(1);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* output = server->output();
    output->SetScale(kHiDpiScale);
    output->SetDeviceScaleFactor(kHiDpiScale);
    output->Flush();
  });

  // Pretend we are already rendering using new scale.
  window_->root_surface()->set_surface_buffer_scale(kHiDpiScale);

  // Set new insets so that rounding does not result in integer.
  constexpr auto kDecorationInsets_2x = gfx::Insets::TLBR(48, 55, 63, 55);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::MockXdgSurface* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });

  window_->SetDecorationInsets(&kDecorationInsets_2x);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  // Now send configure events many times - bounds mustn't change.
  for (size_t i = 0; i < 10; i++) {
    EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
    PostToServerAndWait(
        [id = surface_id_](wl::TestWaylandServerThread* server) {
          wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
          ASSERT_TRUE(surface);
          wl::MockXdgSurface* xdg_surface = surface->xdg_surface();
          // No update to the window geometry.
          EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
        });
    SendConfigureEvent(surface_id_, bounds_with_insets.size(), state);
    AdvanceFrameToCurrent(window_.get(), delegate_);
  }
}

// Checks that when the window gets some of its edges tiled, it notifies the
// delegate appropriately.
TEST_P(WaylandWindowTest, HandleTiledEdges) {
  constexpr gfx::Rect kWindowBounds{800, 600};

  struct {
    std::vector<xdg_toplevel_state> configured_states;
    WindowTiledEdges expected_tiled_edges;
  } kTestCases[]{
      {{XDG_TOPLEVEL_STATE_TILED_LEFT}, {true, false, false, false}},
      {{XDG_TOPLEVEL_STATE_TILED_RIGHT}, {false, true, false, false}},
      {{XDG_TOPLEVEL_STATE_TILED_TOP}, {false, false, true, false}},
      {{XDG_TOPLEVEL_STATE_TILED_BOTTOM}, {false, false, false, true}},
      {{XDG_TOPLEVEL_STATE_TILED_LEFT, XDG_TOPLEVEL_STATE_TILED_TOP},
       {true, false, true, false}},
      {{XDG_TOPLEVEL_STATE_TILED_LEFT, XDG_TOPLEVEL_STATE_TILED_BOTTOM},
       {true, false, false, true}},
      {{XDG_TOPLEVEL_STATE_TILED_RIGHT, XDG_TOPLEVEL_STATE_TILED_TOP},
       {false, true, true, false}},
      {{XDG_TOPLEVEL_STATE_TILED_RIGHT, XDG_TOPLEVEL_STATE_TILED_BOTTOM},
       {false, true, false, true}},
      {{XDG_TOPLEVEL_STATE_TILED_LEFT, XDG_TOPLEVEL_STATE_TILED_TOP,
        XDG_TOPLEVEL_STATE_TILED_BOTTOM},
       {true, false, true, true}},
      {{XDG_TOPLEVEL_STATE_TILED_RIGHT, XDG_TOPLEVEL_STATE_TILED_TOP,
        XDG_TOPLEVEL_STATE_TILED_BOTTOM},
       {false, true, true, true}},
  };
  for (const auto& test_case : kTestCases) {
    auto configured_states = InitializeWlArrayWithActivatedState();
    for (const auto additional_state : test_case.configured_states) {
      AddStateToWlArray(additional_state, configured_states.get());
    }

    EXPECT_CALL(delegate_,
                OnWindowTiledStateChanged(test_case.expected_tiled_edges))
        .Times(1);
    SendConfigureEvent(surface_id_, kWindowBounds.size(), configured_states);

    VerifyAndClearExpectations();
  }
}

TEST_P(WaylandWindowTest, DisregardUnpassedWindowConfigure) {
  constexpr gfx::Rect kNormalBounds1{500, 300};
  constexpr gfx::Rect kNormalBounds2{800, 600};
  constexpr gfx::Rect kNormalBounds3{700, 400};
  uint32_t serial = 1;

  // Send 3 configures, and skip OnSequencePoint for the result of the second
  // configure. The second configure should not be acked or have its properties
  // applied.
  PostToServerAndWait(
      [id = surface_id_, bounds1 = kNormalBounds1, bounds2 = kNormalBounds2,
       bounds3 = kNormalBounds3](wl::TestWaylandServerThread* server) {
        wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(surface);
        wl::MockXdgSurface* xdg_surface = surface->xdg_surface();

        EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds1.size())));
        EXPECT_CALL(*xdg_surface, AckConfigure(2));
        EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds2.size())))
            .Times(0);
        EXPECT_CALL(*xdg_surface, AckConfigure(3)).Times(0);
        EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds3.size())));
        EXPECT_CALL(*xdg_surface, AckConfigure(4));
      });

  auto state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds1.size(), state, ++serial);
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds2.size(), state, ++serial);
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds3.size(), state, ++serial);

  window_->OnSequencePoint(/*seq=*/1);
  window_->OnSequencePoint(/*seq=*/3);
}

TEST_P(WaylandWindowTest, MismatchedSequencePoints) {
  constexpr gfx::Rect kNormalBounds1{500, 300};
  constexpr gfx::Rect kNormalBounds2{800, 600};
  constexpr gfx::Rect kNormalBounds3{700, 400};

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();

    // OnSequencePoint with mismatched sequence points from configure
    // events does not acknowledge toplevel configure.
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
    EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
  });

  auto state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds1.size(), state);
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds2.size(), state);
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds3.size(), state);
  // Needs sequence point > 0 to latch.
  window_->OnSequencePoint(0);
}

TEST_P(WaylandWindowTest, OnSequencePointClearsPreviousUnackedConfigures) {
  constexpr gfx::Rect kNormalBounds1{500, 300};
  constexpr gfx::Rect kNormalBounds2{800, 600};
  constexpr gfx::Rect kNormalBounds3{700, 400};
  uint32_t serial = 1;
  auto state = InitializeWlArrayWithActivatedState();

  // Send 3 configures. Waiting to advance the frame (and call
  // OnSequencePoint(3)) should mean acking and processing the completion of
  // first two configures will be skipped.
  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds1](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())))
        .Times(0);
    EXPECT_CALL(*xdg_surface, AckConfigure(2)).Times(0);
  });
  SendConfigureEvent(surface_id_, kNormalBounds1.size(), state, ++serial);

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds2](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())))
        .Times(0);
    EXPECT_CALL(*xdg_surface, AckConfigure(3)).Times(0);
  });
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds2.size(), state, ++serial);

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds3](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
    EXPECT_CALL(*xdg_surface, AckConfigure(4));
  });
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, kNormalBounds3.size(), state, ++serial);
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

// This test is specifically to guard against origin being set to (0, 0)
// thus lacros can be restored to correct display (crbug.com/1423690)
TEST_P(WaylandWindowTest, RestoredBoundsSetWithCorrectOrigin) {
  constexpr gfx::Rect kNormalBounds{1376, 10, 500, 300};
  constexpr gfx::Rect kMaximizedBounds{1366, 0, 800, 600};

  // Make sure the window has normal state initially.
  window_->SetBoundsInDIP(kNormalBounds);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  // Deactivate the surface.
  auto empty_state = MakeStateArray({});
  SendConfigureEvent(surface_id_, {0, 0}, empty_state);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  WaylandWindow::WindowStates window_states{.is_maximized = true,
                                            .is_activated = true};

  window_->HandleToplevelConfigure(kMaximizedBounds.width(),
                                   kMaximizedBounds.height(), window_states);

  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  EXPECT_EQ(window_->GetRestoredBoundsInDIP(), kNormalBounds);
}

TEST_P(WaylandWindowTest, MaximizeAndRestore) {
  constexpr gfx::Rect kNormalBounds{500, 300};
  constexpr gfx::Rect kMaximizedBounds{800, 600};

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->SetBoundsInDIP(gfx::Rect(kNormalBounds.size()));
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  // Deactivate the surface.
  auto empty_state = MakeStateArray({});
  SendConfigureEvent(surface_id_, {0, 0}, empty_state);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  PostToServerAndWait([id = surface_id_, bounds = kMaximizedBounds](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), SetMaximized());
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
  });
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  // Emulate a piece of behaviour of BrowserDesktopWindowTreeHostLinux, which is
  // the real delegate.  Its OnWindowStateChanged() may (through some chain of
  // calls) invoke SetWindowGeometry(), but that should not happen during the
  // change of the window state.
  // See https://crbug.com/1223005.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .Times(1)
      .WillOnce(
          testing::Invoke([this]() { window_->SetDecorationInsets({}); }));
  window_->Maximize();
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), active_maximized);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  auto inactive_maximized = MakeStateArray({XDG_TOPLEVEL_STATE_MAXIMIZED});
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), inactive_maximized);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), active_maximized);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
  });
  // Emulate a piece of behaviour of BrowserDesktopWindowTreeHostLinux, which is
  // the real delegate.  Its OnWindowStateChanged() may (through some chain of
  // calls) invoke SetWindowGeometry(), but that should not happen during the
  // change of the window state.
  // See https://crbug.com/1223005.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .Times(1)
      .WillOnce(
          testing::Invoke([this]() { window_->SetDecorationInsets({}); }));
  EXPECT_CALL(delegate_, OnActivationChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), UnsetMaximized());
  });
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, active);
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests the event sequence where a minimize request is initiated by the client.
TEST_P(WaylandWindowTest, ClientInitiatedMinimize) {
  wl::ScopedWlArray states({});

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(surface_id_, {0, 0}, states);

  // Have the client minimize the window which should synchronously update the
  // window's local state and notify the delegate.
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    EXPECT_CALL(*mock_surface->xdg_surface()->xdg_toplevel(), SetMinimized());
  });
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Minimize();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Simulate an event from the server sent in response to the above request.
  // This should not result in another call to delegates as this will have
  // already been propagated in the above call to WaylandWindow::Minimize().
  window_->HandleAuraToplevelConfigure(0, 0, 0, 0, {.is_minimized = true});
  window_->HandleSurfaceConfigure(3);
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);
}

// Tests the event sequence where a minimize event is initiated by the server
// and the client's window is in a non-minimized state.
TEST_P(WaylandWindowTest, ServerInitiatedMinimize) {
  wl::ScopedWlArray states({});

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(surface_id_, {0, 0}, states);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  // A subsequent minimize event from the server should notify delegates of the
  // window state change.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->HandleAuraToplevelConfigure(0, 0, 0, 0, {.is_minimized = true});
  window_->HandleSurfaceConfigure(3);
  EXPECT_EQ(PlatformWindowState::kMinimized, window_->GetPlatformWindowState());
}

#else

TEST_P(WaylandWindowTest, Minimize) {
  // This mustn't run with aura shell.
  if (GetParam().enable_aura_shell == wl::EnableAuraShellProtocol::kEnabled) {
    GTEST_SKIP();
  }

  wl::ScopedWlArray states({});

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(surface_id_, {0, 0}, states);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), SetMinimized());
  });
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Minimize();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Reinitialize wl_array, which removes previous old states.
  states = wl::ScopedWlArray({});
  SendConfigureEvent(surface_id_, {0, 0}, states);

  // Wayland compositor doesn't notify clients about minimized state, but rather
  // if a window is not activated. Thus, a WaylandToplevelWindow marks itself as
  // being minimized and and sets state to minimized. Thus, the state mustn't
  // change after the configuration event is sent.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Send one additional empty configuration event (which means the surface is
  // not maximized, fullscreen or activated) to ensure, WaylandWindow stays in
  // the same minimized state, but the delegate is always notified.
  //
  // TODO(tonikito): Improve filtering of delegate notification here.
  ui::PlatformWindowState state;
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&state), InvokeWithoutArgs([&]() {
                              EXPECT_EQ(state, PlatformWindowState::kMinimized);
                            })));
  SendConfigureEvent(surface_id_, {0, 0}, states);

  // And one last time to ensure the behaviour.
  SendConfigureEvent(surface_id_, {0, 0}, states);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests the event sequence where a toplevel window is minimized and a restore
// event is initiated by the server.
TEST_P(WaylandWindowTest, ServerInitiatedRestoreFromMinimizedState) {
  wl::ScopedWlArray states({});

  // Make sure the window is initialized to the normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(surface_id_, {0, 0}, states);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  // A subsequent minimize event from the server should notify delegates of the
  // window state change.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->HandleAuraToplevelConfigure(0, 0, 0, 0, {.is_minimized = true});
  window_->HandleSurfaceConfigure(3);
  EXPECT_EQ(PlatformWindowState::kMinimized, window_->GetPlatformWindowState());

  if (window_->SupportsConfigureMinimizedState()) {
    // If the minimized state is supported via the zaura extension, while
    // minimized a restore event from the server should return the window to the
    // normal state.
    EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
    window_->HandleAuraToplevelConfigure(0, 0, 0, 0, {});
    window_->HandleSurfaceConfigure(4);
    EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  } else {
    // If the minimized state is not supported, the server initiated restore
    // event with no window activation should not restore the window. It should
    // instead leave the window in the minimized state.
    EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(0);
    window_->HandleAuraToplevelConfigure(0, 0, 0, 0, {});
    window_->HandleSurfaceConfigure(4);
    EXPECT_EQ(PlatformWindowState::kMinimized,
              window_->GetPlatformWindowState());
  }
}

TEST_P(WaylandWindowTest, SetFullscreenAndRestore) {
  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);

  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), SetFullscreen());
  });
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->SetFullscreen(true, display::kInvalidDisplayId);
  // Make sure than WaylandWindow manually handles fullscreen states. Check the
  // comment in the WaylandWindow::SetFullscreen.
  EXPECT_EQ(window_->GetPlatformWindowState(),
            PlatformWindowState::kFullScreen);
  SendConfigureEvent(surface_id_, {0, 0}, states);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), UnsetFullscreen());
  });

  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Restore();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
}

TEST_P(WaylandWindowTest, StartWithFullscreen) {
  MockWaylandPlatformWindowDelegate delegate;
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(100, 100);
  properties.type = PlatformWindowType::kWindow;
  // We need to create a window avoid calling Show() on it as it is what upper
  // views layer does - when Widget initialize DesktopWindowTreeHost, the Show()
  // is called later down the road, but Maximize may be called earlier. We
  // cannot process them and set a pending state instead, because ShellSurface
  // is not created by that moment.
  auto window =
      delegate.CreateWaylandWindow(connection_.get(), std::move(properties));

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window->GetPlatformWindowState());

  // The state must not be changed to the fullscreen before the surface is
  // activated.
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface =
        server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_FALSE(mock_surface->xdg_surface());
  });
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);
  window->SetFullscreen(true, display::kInvalidDisplayId);
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  Mock::VerifyAndClearExpectations(&delegate);

  // We must receive a state change after Show is called.
  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PlatformWindowState::kNormal),
                                   Eq(PlatformWindowState::kFullScreen)))
      .Times(1);

  // Show and Activate the surface.
  window->Show(false);

  Mock::VerifyAndClearExpectations(&delegate);

  // We mustn't receive any state changes if that does not differ from the last
  // state.
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);
  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);
  SendConfigureEvent(surface_id, {0, 0}, states);

  // It must be still the same state.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);
  Mock::VerifyAndClearExpectations(&delegate);
}

TEST_P(WaylandWindowTest, StartMaximized) {
  MockWaylandPlatformWindowDelegate delegate;
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(100, 100);
  properties.type = PlatformWindowType::kWindow;
  // We need to create a window avoid calling Show() on it as it is what upper
  // views layer does - when Widget initialize DesktopWindowTreeHost, the Show()
  // is called later down the road, but Maximize may be called earlier. We
  // cannot process them and set a pending state instead, because ShellSurface
  // is not created by that moment.
  auto window =
      delegate.CreateWaylandWindow(connection_.get(), std::move(properties));

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window->GetPlatformWindowState());

  // The state gets changed to maximize and the delegate notified.
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface =
        server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_FALSE(mock_surface->xdg_surface());
  });
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);

  window->Maximize();
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  Mock::VerifyAndClearExpectations(&delegate);

  // We must receive a state change after Show is called.
  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PlatformWindowState::kNormal),
                                   Eq(PlatformWindowState::kMaximized)))
      .Times(1);

  // Show the window now.
  window->Show(false);

  Mock::VerifyAndClearExpectations(&delegate);

  // Window show state should be already up to date, so delegate is not
  // notified.
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Activate the surface.
  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id, {0, 0}, states);

  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  Mock::VerifyAndClearExpectations(&delegate);
}

TEST_P(WaylandWindowTest, CompositorSideStateChanges) {
  // Real insets used by default on HiDPI.
  const auto kInsets = gfx::Insets::TLBR(38, 44, 55, 44);
  const auto kNormalBounds = window_->GetBoundsInDIP();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);

  // Set nonzero insets and ensure that they are only used when the window has
  // normal state.
  // See https://crbug.com/1274629
  window_->SetDecorationInsets(&kInsets);

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect{2000, 2000}));
  });

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id_, {2000, 2000}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Unmaximize
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  PostToServerAndWait(
      [id = surface_id_, insets = kInsets,
       bounds = kNormalBounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect(
                        insets.left(), insets.top(),
                        bounds.width() - (insets.left() + insets.right()),
                        bounds.height() - (insets.top() + insets.bottom()))));
      });
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Now, set to fullscreen.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(delegate_, OnFullscreenModeChanged()).Times(1);
#endif
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(2005, 2005)));
  });
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);
  SendConfigureEvent(surface_id_, {2005, 2005}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Unfullscreen
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(delegate_, OnFullscreenModeChanged()).Times(1);
#endif
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  PostToServerAndWait(
      [id = surface_id_, insets = kInsets,
       bounds = kNormalBounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect(
                        insets.left(), insets.top(),
                        bounds.width() - (insets.left() + insets.right()),
                        bounds.height() - (insets.top() + insets.bottom()))));
      });
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Now, maximize, fullscreen and restore.
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(2000, 2000)));
  });
  states = InitializeWlArrayWithActivatedState();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id_, {2000, 2000}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(delegate_, OnFullscreenModeChanged()).Times(1);
#endif
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(2005, 2005)));
  });
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);
  SendConfigureEvent(surface_id_, {2005, 2005}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Restore
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(delegate_, OnFullscreenModeChanged()).Times(1);
#endif
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  PostToServerAndWait(
      [id = surface_id_, insets = kInsets,
       bounds = kNormalBounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect(
                        insets.left(), insets.top(),
                        bounds.width() - (insets.left() + insets.right()),
                        bounds.height() - (insets.top() + insets.bottom()))));
      });
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

TEST_P(WaylandWindowTest, SetMaximizedFullscreenAndRestore) {
  constexpr gfx::Rect kNormalBounds{500, 300};
  constexpr gfx::Rect kMaximizedBounds{800, 600};

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->SetBoundsInDIP(gfx::Rect(kNormalBounds.size()));
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  // Deactivate the surface.
  wl::ScopedWlArray empty_state({});
  SendConfigureEvent(surface_id_, {0, 0}, empty_state);

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  PostToServerAndWait([id = surface_id_, bounds = kMaximizedBounds](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), SetMaximized());
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
  });
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Maximize();
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), active_maximized);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), SetFullscreen());
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->SetFullscreen(true, display::kInvalidDisplayId);
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, active_maximized.get());
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), active_maximized);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())));
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), UnsetFullscreen());
  });
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Restore();
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, active);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximize) {
  const gfx::Rect current_bounds = window_->GetBoundsInDIP();

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_TRUE(restored_bounds.IsEmpty());
  gfx::Rect bounds = window_->GetBoundsInDIP();

  constexpr gfx::Rect kMaximizedBounds(1024, 768);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->Maximize();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  PostToServerAndWait(
      [id = surface_id_, current_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect{current_bounds.size()}));
      });
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  bounds = window_->GetBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterFullscreen) {
  const gfx::Rect current_bounds = window_->GetBoundsInDIP();

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
  gfx::Rect bounds = window_->GetBoundsInDIP();

  constexpr gfx::Rect kFullscreenBounds(1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->SetFullscreen(true, display::kInvalidDisplayId);
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);
  SendConfigureEvent(surface_id_, kFullscreenBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  PostToServerAndWait(
      [id = surface_id_, current_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect(current_bounds.size())));
      });
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  bounds = window_->GetBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximizeAndFullscreen) {
  const gfx::Rect current_bounds = window_->GetBoundsInDIP();

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
  gfx::Rect bounds = window_->GetBoundsInDIP();

  constexpr gfx::Rect kMaximizedBounds(1024, 768);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->Maximize();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);

  constexpr gfx::Rect kFullscreenBounds(1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->SetFullscreen(true, display::kInvalidDisplayId);
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN);
  SendConfigureEvent(surface_id_, kFullscreenBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  gfx::Rect fullscreen_restore_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, fullscreen_restore_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->Maximize();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, fullscreen_restore_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  PostToServerAndWait(
      [id = surface_id_, current_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface,
                    SetWindowGeometry(gfx::Rect(current_bounds.size())));
      });
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  bounds = window_->GetBoundsInDIP();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, SetCanMaximize) {
  if (GetParam().enable_aura_shell != wl::EnableAuraShellProtocol::kEnabled) {
    GTEST_SKIP();
  }

  EXPECT_CALL(delegate_, CanMaximize).WillOnce(Return(true));
  window_->SizeConstraintsChanged();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_TRUE(zaura_toplevel->can_maximize());
  });

  EXPECT_CALL(delegate_, CanMaximize).WillOnce(Return(false));
  window_->SizeConstraintsChanged();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_FALSE(zaura_toplevel->can_maximize());
  });
}

TEST_P(WaylandWindowTest, SetCanFullscreen) {
  if (GetParam().enable_aura_shell != wl::EnableAuraShellProtocol::kEnabled) {
    GTEST_SKIP();
  }

  EXPECT_CALL(delegate_, CanFullscreen).WillOnce(Return(true));
  window_->SizeConstraintsChanged();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_TRUE(zaura_toplevel->can_fullscreen());
  });

  EXPECT_CALL(delegate_, CanFullscreen).WillOnce(Return(false));
  window_->SizeConstraintsChanged();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_FALSE(zaura_toplevel->can_fullscreen());
  });
}

TEST_P(WaylandWindowTest, SendsBoundsOnRequest) {
  const gfx::Rect initial_bounds = window_->GetBoundsInDIP();

  const gfx::Rect new_bounds =
      gfx::Rect(initial_bounds.width() + 10, initial_bounds.height() + 10);
  EXPECT_CALL(delegate_, OnBoundsChanged(kDefaultBoundsChange));

  PostToServerAndWait([id = surface_id_,
                       new_bounds](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(new_bounds.size())))
        .Times(1);
  });
  window_->SetBoundsInDIP(new_bounds);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Restored bounds should keep empty value.
  gfx::Rect restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, UpdateWindowRegion) {
  // Change bounds.
  const gfx::Rect initial_bounds = window_->GetBoundsInDIP();
  const gfx::Rect new_bounds =
      gfx::Rect(initial_bounds.width() + 10, initial_bounds.height() + 10);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  });
  window_->SetBoundsInDIP(new_bounds);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  PostToServerAndWait(
      [id = surface_id_, new_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        EXPECT_EQ(mock_surface->opaque_region(), new_bounds);
        EXPECT_EQ(mock_surface->input_region(), new_bounds);
      });

  // Maximize.
  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  });
  constexpr gfx::Rect kMaximizedBounds(1024, 768);
  window_->Maximize();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(surface_id_, kMaximizedBounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_,
                       kMaximizedBounds](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    EXPECT_EQ(mock_surface->opaque_region(), kMaximizedBounds);
    EXPECT_EQ(mock_surface->input_region(), kMaximizedBounds);
  });

  // Restore.
  const gfx::Rect restored_bounds = window_->GetRestoredBoundsInDIP();
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  });
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, active);
  AdvanceFrameToCurrent(window_.get(), delegate_);
  VerifyAndClearExpectations();

  PostToServerAndWait(
      [id = surface_id_, restored_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        EXPECT_EQ(mock_surface->opaque_region(), restored_bounds);
        EXPECT_EQ(mock_surface->input_region(), restored_bounds);
      });
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventFocus) {
  // SetPointerFocusedWindow requires a WaylandPointer.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  SetPointerFocusedWindow(window_.get());
  Event::DispatcherApi(&test_mouse_event_).set_target(window_.get());
  EXPECT_TRUE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, SetCursorUsesZcrCursorShapesForCommonTypes) {
  SetPointerFocusedWindow(window_.get());
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();

  // Verify some commonly-used cursors.
  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_POINTER));
  auto pointer_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kPointer));
  window_->SetCursor(pointer_cursor.get());

  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_HAND));
  auto hand_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kHand));
  window_->SetCursor(hand_cursor.get());

  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_IBEAM));
  auto ibeam_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kIBeam));
  window_->SetCursor(ibeam_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorCallsZcrCursorShapesOncePerCursor) {
  SetPointerFocusedWindow(window_.get());
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();
  auto hand_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kHand));
  // Setting the same cursor twice on the client only calls the server once.
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(1);
  window_->SetCursor(hand_cursor.get());
  window_->SetCursor(hand_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorDoesNotUseZcrCursorShapesForNoneCursor) {
  SetPointerFocusedWindow(window_.get());
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(0);
  auto none_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kNone));
  window_->SetCursor(none_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorDoesNotUseZcrCursorShapesForCustomCursors) {
  SetPointerFocusedWindow(window_.get());
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();

  // Custom cursors require bitmaps, so they do not use server-side cursors.
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(0);
  auto custom_cursor = AsPlatformCursor(
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kCustom, SkBitmap(),
                                         gfx::Point(), kDefaultCursorScale));
  window_->SetCursor(custom_cursor.get());
}

ACTION_P(CloneEvent, ptr) {
  *ptr = arg0->Clone();
}

TEST_P(WaylandWindowTest, DispatchEvent) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  window_->DispatchEvent(&test_mouse_event_);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(mouse_event->location_f(), test_mouse_event_.location_f());
  EXPECT_EQ(mouse_event->root_location_f(),
            test_mouse_event_.root_location_f());
  EXPECT_EQ(mouse_event->time_stamp(), test_mouse_event_.time_stamp());
  EXPECT_EQ(mouse_event->button_flags(), test_mouse_event_.button_flags());
  EXPECT_EQ(mouse_event->changed_button_flags(),
            test_mouse_event_.changed_button_flags());
}

TEST_P(WaylandWindowTest, ConfigureEvent) {
  wl::ScopedWlArray states({});

  // The surface must react on each configure event and send bounds to its
  // delegate.
  constexpr gfx::Size kSize{1000, 1000};
  uint32_t serial = 12u;

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  // Responding to a configure event, the window geometry in here must respect
  // the sizing negotiations specified by the configure event.
  // |xdg_surface| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are received by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface in xdg_shell_v5.
  PostToServerAndWait(
      [id = surface_id_, kSize, serial](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        auto* xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(kSize))).Times(1);
        EXPECT_CALL(*xdg_surface, AckConfigure(serial));
      });
  SendConfigureEvent(surface_id_, kSize, states, serial);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  constexpr gfx::Size kNewSize{1500, 1000};

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  PostToServerAndWait([id = surface_id_, kNewSize,
                       serial](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(kNewSize))).Times(1);
    EXPECT_CALL(*xdg_surface, AckConfigure(serial + 1));
  });
  SendConfigureEvent(surface_id_, kNewSize, states, ++serial);
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

TEST_P(WaylandWindowTest, ConfigureEventWithNulledSize) {
  wl::ScopedWlArray states({});

  // |xdg_surface| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are received by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface in xdg_shell_v5.
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, AckConfigure(14u));
  });

  // If Wayland sends configure event with 0 width and 0 size, client should
  // call back with desired sizes. In this case, that's the actual size of
  // the window.
  SendConfigureEvent(surface_id_, {0, 0}, states, 14u);
}

TEST_P(WaylandWindowTest, ConfigureEventIsNotAckedMultipleTimes) {
  constexpr gfx::Rect kNormalBounds{500, 300};
  constexpr gfx::Rect kSecondBounds{600, 600};

  // Configure event makes Wayland update bounds, but does not change toplevel
  // input region, opaque region or window geometry immediately. Such actions
  // are postponed to OnSequencePoint();
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(bounds.size())))
        .Times(0);
    EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
  });

  auto state = InitializeWlArrayWithActivatedState();
  constexpr uint32_t kConfigureSerial = 2u;
  SendConfigureEvent(surface_id_, kNormalBounds.size(), state,
                     kConfigureSerial);

  PostToServerAndWait([id = surface_id_, bounds = kNormalBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(bounds));
    EXPECT_CALL(*xdg_surface, AckConfigure(kConfigureSerial));
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_));
    EXPECT_CALL(*mock_surface, SetInputRegion(_));
  });

  // Get the viz sequence ID which will let us ack the configure event.
  auto viz_seq = delegate_.viz_seq();

  // Insert another change which will increase the viz sequence ID here.
  // We want to make sure that when this viz sequence ID is reached, we
  // don't send another ack for the configure.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));
  window_->SetBoundsInDIP(kSecondBounds);

  AdvanceFrameToGivenVizSequenceId(window_.get(), delegate_, viz_seq);
  VerifyAndClearExpectations();

  PostToServerAndWait([id = surface_id_, bounds = kSecondBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(bounds));
    EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
    EXPECT_CALL(*mock_surface, SetOpaqueRegion(_));
    EXPECT_CALL(*mock_surface, SetInputRegion(_));
  });
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

TEST_P(WaylandWindowTest, ManyConfigureEventsDoesNotCrash) {
  constexpr uint32_t kConfigureSerial = 2u;

  auto state = InitializeWlArrayWithActivatedState();
  gfx::Size size{500, 300};
  for (int i = 0; i < 3000; ++i) {
    SendConfigureEvent(surface_id_, size, state, kConfigureSerial + i);
    size.Enlarge(1, 1);
  }
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

// If the server immediately changes the bounds after a window is initialised,
// make sure that the client doesn't wait for a new frame to be produced.
// See https://crbug.com/1427954.
TEST_P(WaylandWindowTest, InitialConfigureFollowedByBoundsChangeCompletesAck) {
  constexpr gfx::Rect kFirstBounds{0, 0, 800, 600};
  constexpr gfx::Rect kSecondBounds{50, 50, 800, 600};
  constexpr uint32_t kConfigureSerial = 2u;

  // Make sure that we start off with the initial bounds we expect.
  EXPECT_EQ(kFirstBounds, window_->latched_state().bounds_dip);
  EXPECT_EQ(kFirstBounds, window_->applied_state().bounds_dip);

  // Expect an origin change.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(BoundsChange{true})));

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, AckConfigure(kConfigureSerial)).Times(1);
  });

  window_->HandleAuraToplevelConfigure(
      kSecondBounds.x(), kSecondBounds.y(), kSecondBounds.width(),
      kSecondBounds.height(), {.is_activated = true});
  window_->HandleSurfaceConfigure(kConfigureSerial);

  // Don't call AdvanceFrameToCurrent here, because just updating the
  // bounds origin won't generate a new frame. The client should ack
  // even if there is no new frame produced.
}

TEST_P(WaylandWindowTest, OnActivationChanged) {
  uint32_t serial = 0;

  // Deactivate the surface.
  wl::ScopedWlArray empty_state({});
  SendConfigureEvent(surface_id_, {0, 0}, empty_state, ++serial);

  {
    wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
    EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
    SendConfigureEvent(surface_id_, {0, 0}, states, ++serial);
  }

  wl::ScopedWlArray states({});
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  SendConfigureEvent(surface_id_, {0, 0}, states, ++serial);
}

TEST_P(WaylandWindowTest, OnAcceleratedWidgetDestroy) {
  window_.reset();
}

TEST_P(WaylandWindowTest, CanCreateMenuWindow) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));

  // SetPointerFocus(true) requires a WaylandPointer.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(
        server->seat()->resource(),
        WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  ASSERT_TRUE(connection_->seat()->touch());
  SetPointerFocusedWindow(window_.get());

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  SetPointerFocusedWindow(window_.get());
  window_->set_touch_focus(false);

  // Given that there is no parent passed and we don't have any focused windows,
  // Wayland must still create a window.
  menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  window_->set_touch_focus(true);

  menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);
}

TEST_P(WaylandWindowTest, CreateAndDestroyNestedMenuWindow) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  gfx::AcceleratedWidget menu_window_widget;
  EXPECT_CALL(menu_window_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&menu_window_widget));
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10), &menu_window_delegate,
      widget_);
  EXPECT_TRUE(menu_window);
  ASSERT_NE(menu_window_widget, gfx::kNullAcceleratedWidget);

  MockWaylandPlatformWindowDelegate nested_menu_window_delegate;
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, gfx::Rect(20, 0, 10, 10),
          &nested_menu_window_delegate, menu_window_widget);
  EXPECT_TRUE(nested_menu_window);
}

TEST_P(WaylandWindowTest, DispatchesLocatedEventsToCapturedWindow) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate, widget_);
  EXPECT_TRUE(menu_window);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  SetPointerFocusedWindow(window_.get());

  // Make sure the events are handled by the window that has the pointer focus.
  VerifyCanDispatchMouseEvents(window_.get(), {menu_window.get()});

  // The |window_| that has the pointer focus must receive the event.
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the |window|.
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(10.75),
                           wl_fixed_from_double(20.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(10, 20));

  // Set capture to menu window now.
  menu_window->SetCapture();

  // It's still the |window_| that can dispatch the events, but it will reroute
  // the event to correct window and fix the location.
  VerifyCanDispatchMouseEvents(window_.get(), {menu_window.get()});

  // The |window_| that has the pointer focus must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event2;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event2));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the |window|.
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(10.75),
                           wl_fixed_from_double(20.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event2->IsLocatedEvent());
  EXPECT_EQ(event2->AsLocatedEvent()->location(), gfx::Point(0, 10));

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event3;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event3));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the |window|.
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(2.75),
                           wl_fixed_from_double(8.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event3->IsLocatedEvent());
  EXPECT_EQ(event3->AsLocatedEvent()->location(), gfx::Point(-8, -2));

  // If nested menu window is added, the events are still correctly translated
  // to the captured window.
  MockWaylandPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, gfx::Rect(15, 18, 10, 10),
          &nested_menu_window_delegate, menu_window->GetWidget());
  EXPECT_TRUE(nested_menu_window);

  SetPointerFocusedWindow(nested_menu_window.get());

  // The event is processed by the window that has the pointer focus, but
  // dispatched by the window that has the capture.
  VerifyCanDispatchMouseEvents(nested_menu_window.get(),
                               {window_.get(), menu_window.get()});
  EXPECT_TRUE(menu_window->HasCapture());

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(nested_menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event4;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event4));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the
    // |nested_menu_window|.
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(2.75),
                           wl_fixed_from_double(8.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event4->IsLocatedEvent());
  EXPECT_EQ(event4->AsLocatedEvent()->location(), gfx::Point(7, 16));

  menu_window.reset();
}

// Verify that located events are translated correctly when the windows have
// geometry with non-zero offset.
// See https://crbug.com/1292486.
TEST_P(WaylandWindowTest, ConvertEventToTarget) {
  constexpr gfx::Rect kMainWindowBounds{956, 556};
  const auto kMainWindowInsets = gfx::Insets::TLBR(24, 28, 32, 28);

  auto bounds_with_insets = kMainWindowBounds;
  bounds_with_insets.Inset(kMainWindowInsets);
  EXPECT_CALL(delegate_, OnBoundsChanged(_));
  PostToServerAndWait([id = surface_id_, bounds_with_insets](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(bounds_with_insets));
  });
  window_->SetBoundsInDIP(kMainWindowBounds);
  window_->SetDecorationInsets(&kMainWindowInsets);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Create a menu.
  constexpr gfx::Rect kMenuBounds{100, 100, 80, 50};
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, kMenuBounds, &menu_window_delegate, widget_);
  EXPECT_TRUE(menu_window);

  // Now translate the event located at (0, 0) in the parent window into the
  // coordinate system of the menu.  Its coordinates must be equal to:
  //     -(offset of parent geometry + offset of the menu).
  constexpr gfx::PointF kParentPoint{0, 0};
  ui::MouseEvent event(ui::EventType::ET_MOUSE_MOVED, kParentPoint,
                       kParentPoint, {}, ui::EF_NONE, ui::EF_NONE);

  ui::Event::DispatcherApi dispatcher_api(&event);
  dispatcher_api.set_target(window_.get());

  ui::WaylandEventSource::ConvertEventToTarget(menu_window.get(), &event);
  EXPECT_EQ(event.AsLocatedEvent()->x(),
            -(kMenuBounds.x() + kMainWindowInsets.left()));
  EXPECT_EQ(event.AsLocatedEvent()->y(),
            -(kMenuBounds.y() + kMainWindowInsets.top()));
}

// Tests that the event grabber gets the events processed by its toplevel parent
// window iff they belong to the same "family". Otherwise, events mustn't be
// rerouted from another toplevel window to the event grabber.
TEST_P(WaylandWindowTest,
       DispatchesLocatedEventsToCapturedWindowInTheSameStack) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(30, 40, 20, 50),
      &menu_window_delegate, widget_);
  EXPECT_TRUE(menu_window);

  // Second toplevel window has the same bounds as the |window_|.
  MockWaylandPlatformWindowDelegate toplevel_window2_delegate;
  std::unique_ptr<WaylandWindow> toplevel_window2 =
      CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                    window_->GetBoundsInDIP(),
                                    &toplevel_window2_delegate);
  EXPECT_TRUE(toplevel_window2);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  SetPointerFocusedWindow(window_.get());

  // Make sure the events are handled by the window that has the pointer focus.
  VerifyCanDispatchMouseEvents(window_.get(),
                               {menu_window.get(), toplevel_window2.get()});

  menu_window->SetCapture();

  // The |menu_window| that has capture must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(toplevel_window2_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the |window|.
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(10.75),
                           wl_fixed_from_double(20.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(-20, -20));

  // Now, pretend that the second toplevel window gets the pointer focus - the
  // event grabber must be disragerder now.
  SetPointerFocusedWindow(toplevel_window2.get());

  VerifyCanDispatchMouseEvents(toplevel_window2.get(),
                               {menu_window.get(), window_.get()});

  // The |toplevel_window2| that has capture and must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  event.reset();
  EXPECT_CALL(toplevel_window2_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // The event is sent in local surface coordinates of the |toplevel_window2|
    // (they're basically the same as the |window| has.)
    wl_pointer_send_motion(server->seat()->pointer()->resource(),
                           server->GetNextSerial(), wl_fixed_from_double(10.75),
                           wl_fixed_from_double(20.375));
    wl_pointer_send_frame(server->seat()->pointer()->resource());
  });

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(10, 20));
}

TEST_P(WaylandWindowTest, DispatchesKeyboardEventToToplevelWindow) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate, widget_);
  EXPECT_TRUE(menu_window);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);
  });
  ASSERT_TRUE(connection_->seat()->keyboard());
  SetKeyboardFocusedWindow(menu_window.get());

  // Disable auto-repeat so that it doesn't ruin our expectations.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_keyboard_send_repeat_info(server->seat()->keyboard()->resource(), 0, 0);
  });

  // Even though the menu window has the keyboard focus, the keyboard events are
  // dispatched by the root parent wayland window in the end.
  VerifyCanDispatchKeyEvents({menu_window.get()}, {window_.get()});
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_keyboard_send_key(server->seat()->keyboard()->resource(),
                         server->GetNextSerial(), server->GetNextTime(),
                         30 /* a */, WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  ASSERT_TRUE(event->IsKeyEvent());
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  event.reset();
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_keyboard_send_key(server->seat()->keyboard()->resource(),
                         server->GetNextSerial(), server->GetNextTime(),
                         30 /* a */, WL_KEYBOARD_KEY_STATE_RELEASED);
  });

  ASSERT_TRUE(event->IsKeyEvent());

  // Setting capture doesn't affect the kbd events.
  menu_window->SetCapture();
  VerifyCanDispatchKeyEvents({menu_window.get()}, {window_.get()});

  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  event.reset();
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_keyboard_send_key(server->seat()->keyboard()->resource(),
                         server->GetNextSerial(), server->GetNextTime(),
                         30 /* a */, WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  ASSERT_TRUE(event->IsKeyEvent());
  Mock::VerifyAndClearExpectations(&delegate_);

  menu_window.reset();
}

// Tests that event is processed by the surface that has the focus. More
// extensive tests are located in wayland touch/keyboard/pointer unittests.
TEST_P(WaylandWindowTest, CanDispatchEvent) {
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  gfx::AcceleratedWidget menu_window_widget;
  EXPECT_CALL(menu_window_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&menu_window_widget));
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10), &menu_window_delegate,
      widget_);
  EXPECT_TRUE(menu_window);

  MockWaylandPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, gfx::Rect(20, 0, 10, 10),
          &nested_menu_window_delegate, menu_window_widget);
  EXPECT_TRUE(nested_menu_window);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER |
                                  WL_SEAT_CAPABILITY_KEYBOARD |
                                  WL_SEAT_CAPABILITY_TOUCH);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  ASSERT_TRUE(connection_->seat()->touch());
  ASSERT_TRUE(connection_->seat()->keyboard());

  // Test that CanDispatchEvent is set correctly.
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* toplevel_surface = server->GetObject<wl::MockSurface>(id);
    wl_pointer_send_enter(server->seat()->pointer()->resource(),
                          server->GetNextSerial(), toplevel_surface->resource(),
                          0, 0);
  });

  // Only |window_| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents(window_.get(),
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* toplevel_surface = server->GetObject<wl::MockSurface>(id);
    wl::ScopedWlArray empty({});
    wl_keyboard_send_enter(server->seat()->keyboard()->resource(),
                           server->GetNextSerial(),
                           toplevel_surface->resource(), empty.get());
  });

  // Only |window_| can dispatch MouseEvents and KeyEvents.
  VerifyCanDispatchMouseEvents(window_.get(),
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents({window_.get()},
                             {menu_window.get(), nested_menu_window.get()});

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* toplevel_surface = server->GetObject<wl::MockSurface>(id);
    wl_touch_send_down(server->seat()->touch()->resource(),
                       server->GetNextSerial(), 0, toplevel_surface->resource(),
                       0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(100));
    wl_touch_send_frame(server->seat()->touch()->resource());
  });

  // Only |window_| can dispatch MouseEvents and KeyEvents.
  VerifyCanDispatchMouseEvents(window_.get(),
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents({window_.get()},
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents({window_.get()},
                             {menu_window.get(), nested_menu_window.get()});

  const uint32_t menu_window_surface_id =
      menu_window->root_surface()->get_surface_id();
  PostToServerAndWait(
      [toplevel_surface_id = surface_id_,
       menu_window_surface_id](wl::TestWaylandServerThread* server) {
        wl::MockSurface* toplevel_surface =
            server->GetObject<wl::MockSurface>(toplevel_surface_id);
        wl::MockSurface* menu_window_surface =
            server->GetObject<wl::MockSurface>(menu_window_surface_id);
        wl_pointer_send_leave(server->seat()->pointer()->resource(),
                              server->GetNextSerial(),
                              toplevel_surface->resource());
        wl_pointer_send_enter(server->seat()->pointer()->resource(),
                              server->GetNextSerial(),
                              menu_window_surface->resource(), 0, 0);
        wl_touch_send_up(server->seat()->touch()->resource(),
                         server->GetNextSerial(), 1000, 0 /* id */);
        wl_touch_send_frame(server->seat()->touch()->resource());

        wl_keyboard_send_leave(server->seat()->keyboard()->resource(),
                               server->GetNextSerial(),
                               toplevel_surface->resource());
      });

  // Only |menu_window| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents(menu_window.get(),
                               {window_.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});

  const uint32_t nested_menu_window_surface_id =
      nested_menu_window->root_surface()->get_surface_id();
  PostToServerAndWait([nested_menu_window_surface_id, menu_window_surface_id](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* menu_window_surface =
        server->GetObject<wl::MockSurface>(menu_window_surface_id);
    wl::MockSurface* nested_menu_window_surface =
        server->GetObject<wl::MockSurface>(nested_menu_window_surface_id);

    wl_pointer_send_leave(server->seat()->pointer()->resource(),
                          server->GetNextSerial(),
                          menu_window_surface->resource());
    wl_pointer_send_enter(server->seat()->pointer()->resource(),
                          server->GetNextSerial(),
                          nested_menu_window_surface->resource(), 0, 0);
  });

  // Only |nested_menu_window| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents(nested_menu_window.get(),
                               {window_.get(), menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
}

TEST_P(WaylandWindowTest, DispatchWindowMove) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    // Focus and press left mouse button, so that serial is sent to client.
    auto* pointer_resource = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer_resource, server->GetNextSerial(),
                          surface->resource(), 0, 0);
    wl_pointer_send_button(pointer_resource, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
  });

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    auto* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface->xdg_toplevel(), Move(_));
  });
  ui::GetWmMoveResizeHandler(*window_)->DispatchHostWindowDragMovement(
      HTCAPTION, gfx::Point());
}

// Makes sure hit tests are converted into right edges.
TEST_P(WaylandWindowTest, DispatchWindowResize) {
  std::vector<int> hit_test_values;
  InitializeWithSupportedHitTestValues(&hit_test_values);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });

  ASSERT_TRUE(connection_->seat()->pointer());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    // Focus and press left mouse button, so that serial is sent to client.
    auto* pointer_resource = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer_resource, server->GetNextSerial(),
                          surface->resource(), 0, 0);
    wl_pointer_send_button(pointer_resource, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
  });

  auto* wm_move_resize_handler = ui::GetWmMoveResizeHandler(*window_);
  for (const int value : hit_test_values) {
    {
      uint32_t direction = wl::IdentifyDirection(value);
      PostToServerAndWait(
          [id = surface_id_, direction](wl::TestWaylandServerThread* server) {
            wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
            auto* xdg_surface = surface->xdg_surface();
            EXPECT_CALL(*xdg_surface->xdg_toplevel(), Resize(_, Eq(direction)));
          });
      wm_move_resize_handler->DispatchHostWindowDragMovement(value,
                                                             gfx::Point());
    }
  }
}

TEST_P(WaylandWindowTest, ToplevelWindowUpdateWindowScale) {
  VerifyAndClearExpectations();

  // Surface scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->applied_state().window_scale);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Configure first output with scale 1.
    wl::TestOutput* output1 = server->output();
    output1->SetPhysicalAndLogicalBounds({1920, 1080});
    output1->Flush();

    // Creating an output with scale 2.
    auto* output2 =
        server->CreateAndInitializeOutput(wl::TestOutputMetrics({1920, 1080}));
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
  });
  WaitForAllDisplaysReady();

  auto* output_manager = connection_->wayland_output_manager();
  EXPECT_EQ(2u, output_manager->GetAllOutputs().size());

  const uint32_t output_id1 =
      GetObjIdForOutput(output_manager->GetAllOutputs().begin()->first);
  PostToServerAndWait(
      [id = surface_id_, output_id1](wl::TestWaylandServerThread* server) {
        wl::TestOutput* output = server->GetObject<wl::TestOutput>(output_id1);
        ASSERT_TRUE(output);

        // Send the window to |output1|.
        wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(surface);
        wl_surface_send_enter(surface->resource(), output->resource());
      });

  // The window's scale and bounds must remain unchanged.
  EXPECT_EQ(1, window_->applied_state().window_scale);
  EXPECT_EQ(gfx::Size(800, 600), window_->applied_state().size_px);
  EXPECT_EQ(gfx::Rect(800, 600), window_->GetBoundsInDIP());

  // Get another output's id.
  const uint32_t output_id2 =
      GetObjIdForOutput(output_manager->GetAllOutputs().rbegin()->first);

  PostToServerAndWait([id = surface_id_, output_id2,
                       output_id1](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(output_id2);
    ASSERT_TRUE(output2);
    wl::TestOutput* output1 = server->GetObject<wl::TestOutput>(output_id1);
    ASSERT_TRUE(output1);
    // Simulating drag process from |output1| to |output2|.
    wl_surface_send_enter(surface->resource(), output2->resource());
    wl_surface_send_leave(surface->resource(), output1->resource());
  });

  // The window must change its scale and bounds to keep DIP bounds the same.
  EXPECT_EQ(2, window_->applied_state().window_scale);
  EXPECT_EQ(gfx::Size(1600, 1200), window_->applied_state().size_px);
  EXPECT_EQ(gfx::Rect(800, 600), window_->GetBoundsInDIP());
}

TEST_P(WaylandWindowTest, ToplevelWindowOnRotateFocus) {
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  base::MockRepeatingCallback<void(uint32_t, uint32_t)> ack_cb;

  // For asserting server requests emitted by the client.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(surface_id_)
        ->xdg_surface()
        ->xdg_toplevel()
        ->zaura_toplevel()
        ->set_ack_rotate_focus_callback(ack_cb.Get());
  });
  SendConfigureEvent(surface_id_, {10, 10},
                     InitializeWlArrayWithActivatedState());
  SetKeyboardFocusedWindow(window_.get());

  using Direction = PlatformWindowDelegate::RotateDirection;
  int serial = 1;

  // Test successful return cases

  // Forward, restart
  EXPECT_CALL(delegate_, OnRotateFocus(Direction::kForward, true))
      .WillOnce(Return(true));
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++, ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART);
  });

  // Backward, no restart
  EXPECT_CALL(delegate_, OnRotateFocus(Direction::kBackward, false))
      .WillOnce(Return(true));
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++,
        ZAURA_TOPLEVEL_ROTATE_DIRECTION_BACKWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_NO_RESTART);
  });

  // Test unsuccessful return cases

  // Forward
  EXPECT_CALL(delegate_, OnRotateFocus(_, _)).WillOnce(Return(false));
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++, ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART);
  });

  // Backward
  EXPECT_CALL(delegate_, OnRotateFocus(_, _)).WillOnce(Return(false));
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++,
        ZAURA_TOPLEVEL_ROTATE_DIRECTION_BACKWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_NO_RESTART);
  });
}

TEST_P(WaylandWindowTest, ToplevelWindowOnRotateFocus_NotActiveOrNotFocused) {
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  base::MockRepeatingCallback<void(uint32_t, uint32_t)> ack_cb;

  // For asserting server requests emitted by the client.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(surface_id_)
        ->xdg_surface()
        ->xdg_toplevel()
        ->zaura_toplevel()
        ->set_ack_rotate_focus_callback(ack_cb.Get());
  });

  int serial = 1;

  // Not active, should fail
  SendConfigureEvent(surface_id_, {10, 10}, wl::ScopedWlArray({}));
  SetKeyboardFocusedWindow(nullptr);

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++, ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART);
  });

  // Now activate, but don't grab keyboard focus, should still be rejected.
  // Not active, should fail
  SendConfigureEvent(surface_id_, {10, 10},
                     InitializeWlArrayWithActivatedState());

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++, ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART);
  });

  // Now grab keyboard focus, we should have great success now.
  SetKeyboardFocusedWindow(window_.get());

  EXPECT_CALL(delegate_, OnRotateFocus(_, _)).WillOnce(Return(true));
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(ack_cb,
                Run(serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_HANDLED));

    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_rotate_focus(
        toplevel->resource(), serial++, ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD,
        ZAURA_TOPLEVEL_ROTATE_RESTART_STATE_RESTART);
  });
}

TEST_P(WaylandWindowTest, WaylandPopupSurfaceScale) {
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Configure an output with scale 1.
    wl::TestOutput* output1 = server->output();
    output1->SetPhysicalAndLogicalBounds({1920, 1080});
    output1->Flush();

    // Creating an output with scale 2.
    auto* output2 = server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({1920, 0, 1920, 1080}));
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
  });
  WaitForAllDisplaysReady();

  auto* output_manager = connection_->wayland_output_manager();
  EXPECT_EQ(2u, output_manager->GetAllOutputs().size());

  const uint32_t output_id1 =
      GetObjIdForOutput(output_manager->GetAllOutputs().begin()->first);
  const uint32_t output_id2 =
      GetObjIdForOutput(output_manager->GetAllOutputs().rbegin()->first);

  std::vector<PlatformWindowType> window_types{PlatformWindowType::kMenu,
                                               PlatformWindowType::kTooltip};
  for (const auto& type : window_types) {
    PostToServerAndWait([id = surface_id_,
                         output_id1](wl::TestWaylandServerThread* server) {
      // Send the window to |output1|.
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
      ASSERT_TRUE(surface);
      wl::TestOutput* output = server->GetObject<wl::TestOutput>(output_id1);
      wl_surface_send_enter(surface->resource(), output->resource());
    });

    // Creating a wayland_popup on |window_|.
    SetPointerFocusedWindow(window_.get());
    gfx::Rect wayland_popup_bounds(15, 15, 10, 10);
    auto wayland_popup = CreateWaylandWindowWithParams(
        type, wayland_popup_bounds, &delegate_, window_->GetWidget());
    EXPECT_TRUE(wayland_popup);
    wayland_popup->Show(false);

    // the wayland_popup window should inherit its buffer scale from the focused
    // window.
    EXPECT_EQ(1, window_->applied_state().window_scale);
    EXPECT_EQ(window_->applied_state().window_scale,
              wayland_popup->applied_state().window_scale);
    EXPECT_EQ(wayland_popup_bounds.size(),
              wayland_popup->applied_state().size_px);
    EXPECT_EQ(wayland_popup_bounds, wayland_popup->GetBoundsInDIP());
    wayland_popup->Hide();

    PostToServerAndWait([id = surface_id_, output_id1,
                         output_id2](wl::TestWaylandServerThread* server) {
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
      ASSERT_TRUE(surface);
      wl::TestOutput* output1 = server->GetObject<wl::TestOutput>(output_id1);
      wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(output_id2);
      // Send the window to |output2|.
      wl_surface_send_enter(surface->resource(), output2->resource());
      wl_surface_send_leave(surface->resource(), output1->resource());
    });

    EXPECT_EQ(2, window_->applied_state().window_scale);
    wayland_popup->Show(false);

    wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

    // |wayland_popup|'s scale and bounds must change whenever its parents
    // scale is changed.
    EXPECT_EQ(window_->applied_state().window_scale,
              wayland_popup->applied_state().window_scale);
    EXPECT_EQ(
        gfx::ScaleToCeiledSize(wayland_popup_bounds.size(),
                               wayland_popup->applied_state().window_scale),
        wayland_popup->applied_state().size_px);

    wayland_popup->Hide();
    SetPointerFocusedWindow(nullptr);

    PostToServerAndWait([id = surface_id_,
                         output_id2](wl::TestWaylandServerThread* server) {
      wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
      ASSERT_TRUE(surface);
      wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(output_id2);
      wl_surface_send_leave(surface->resource(), output2->resource());
    });
  }
}

// Tests that WaylandPopup is able to translate provided bounds via
// PlatformWindowProperties using buffer scale it's going to use that the client
// is not able to determine before PlatformWindow is created. See
// WaylandPopup::OnInitialize for more details.
TEST_P(WaylandWindowTest, WaylandPopupInitialBufferScale) {
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Configure an output with scale 1.
    wl::TestOutput* main_output = server->output();
    main_output->SetPhysicalAndLogicalBounds({1920, 1080});
    main_output->Flush();

    // Creating an output with scale 1.
    server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({1921, 0, 1920, 1080}));
  });

  auto* output_manager = connection_->wayland_output_manager();
  EXPECT_EQ(2u, output_manager->GetAllOutputs().size());

  const WaylandOutput* main_output =
      output_manager->GetAllOutputs().begin()->second.get();
  const WaylandOutput* secondary_output =
      output_manager->GetAllOutputs().rbegin()->second.get();

  struct {
    raw_ptr<const WaylandOutput> output;
    const char* label;
  } screen[] = {{main_output, "main output"},
                {secondary_output, "secondary output"}};

  for (const auto& entered_output : screen) {
    PostToServerAndWait(
        [id = surface_id_, entered_output_id = GetObjIdForOutput(
                               entered_output.output->output_id())](
            wl::TestWaylandServerThread* server) {
          wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
          ASSERT_TRUE(surface);
          wl::TestOutput* output =
              server->GetObject<wl::TestOutput>(entered_output_id);
          wl_surface_send_enter(surface->resource(), output->resource());
        });
    for (auto main_output_scale = 1; main_output_scale < 5;
         main_output_scale++) {
      for (auto secondary_output_scale = 1; secondary_output_scale < 5;
           secondary_output_scale++) {
        PostToServerAndWait(
            [main_output_id = GetObjIdForOutput(main_output->output_id()),
             secondary_output_id =
                 GetObjIdForOutput(secondary_output->output_id()),
             main_output_scale,
             secondary_output_scale](wl::TestWaylandServerThread* server) {
              wl::TestOutput* main_output =
                  server->GetObject<wl::TestOutput>(main_output_id);
              wl::TestOutput* secondary_output =
                  server->GetObject<wl::TestOutput>(secondary_output_id);
              // Update scale factors first.
              main_output->SetScale(main_output_scale);
              main_output->SetDeviceScaleFactor(main_output_scale);
              secondary_output->SetScale(secondary_output_scale);
              secondary_output->SetDeviceScaleFactor(secondary_output_scale);

              main_output->Flush();
              secondary_output->Flush();
            });

        gfx::Rect bounds_dip(15, 15, 10, 10);
        // DesktopWindowTreeHostPlatform uses the scale of the current display
        // of the parent window to translate initial bounds of the popup to
        // pixels.
        const int32_t effective_scale = entered_output.output->scale_factor();
        gfx::Transform transform;
        transform.Scale(effective_scale, effective_scale);
        gfx::Rect wayland_popup_bounds = transform.MapRect(bounds_dip);

        std::unique_ptr<WaylandWindow> wayland_popup =
            CreateWaylandWindowWithParams(PlatformWindowType::kMenu, bounds_dip,
                                          &delegate_, window_->GetWidget());
        EXPECT_TRUE(wayland_popup);

        wayland_popup->Show(false);

        gfx::Size expected_px_size = wayland_popup_bounds.size();
        if (entered_output.output == secondary_output) {
          expected_px_size =
              gfx::ScaleToCeiledSize(bounds_dip.size(), secondary_output_scale);
        }

        EXPECT_EQ(expected_px_size, wayland_popup->applied_state().size_px)
            << " when the window is on " << entered_output.label
            << " that has scale " << entered_output.output->scale_factor();
      }
    }
    PostToServerAndWait(
        [id = surface_id_,
         entered_output_id = GetObjIdForOutput(main_output->output_id())](
            wl::TestWaylandServerThread* server) {
          wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
          ASSERT_TRUE(surface);
          wl::TestOutput* output =
              server->GetObject<wl::TestOutput>(entered_output_id);
          wl_surface_send_leave(surface->resource(), output->resource());
        });
  }
}

TEST_P(WaylandWindowTest, WaylandPopupInitialBufferUsesParentScale) {
  VerifyAndClearExpectations();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Configure the the first output with scale 1.
    wl::TestOutput* main_output = server->output();
    main_output->SetPhysicalAndLogicalBounds({1920, 1080});
    main_output->Flush();

    // Creating an output with scale 2.
    auto* output2 = server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({1921, 0, 1920, 1080}));
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
  });
  WaitForAllDisplaysReady();

  auto* output_manager = connection_->wayland_output_manager();
  EXPECT_EQ(2u, output_manager->GetAllOutputs().size());

  const uint32_t secondary_output_id =
      GetObjIdForOutput(output_manager->GetAllOutputs().rbegin()->first);

  PostToServerAndWait([id = surface_id_, secondary_output_id](
                          wl::TestWaylandServerThread* server) {
    // Send the window to |output2|.
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::TestOutput* output =
        server->GetObject<wl::TestOutput>(secondary_output_id);

    wl_surface_send_enter(surface->resource(), output->resource());
  });

  constexpr gfx::Rect kBoundsDip{50, 50, 100, 100};
  const gfx::Size expected_size_px =
      gfx::ScaleToCeiledSize(kBoundsDip.size(), 2);

  std::unique_ptr<WaylandWindow> wayland_popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, kBoundsDip, &delegate_, window_->GetWidget());
  EXPECT_TRUE(wayland_popup);

  wayland_popup->Show(false);

  EXPECT_EQ(expected_size_px, wayland_popup->applied_state().size_px);

  PostToServerAndWait([id = surface_id_, secondary_output_id](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    wl::TestOutput* output =
        server->GetObject<wl::TestOutput>(secondary_output_id);
    wl_surface_send_leave(surface->resource(), output->resource());
  });
}

// Tests that a WaylandWindow uses the entered output with largest scale
// factor as the preferred output. If scale factors are equal, the very first
// entered display is used.
TEST_P(WaylandWindowTest, GetPreferredOutput) {
  VerifyAndClearExpectations();

  // Buffer scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->applied_state().window_scale);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Update first output.
    wl::TestOutput* output1 = server->output();
    output1->SetPhysicalAndLogicalBounds({1920, 1080});
    output1->Flush();

    // Creating a 2nd output.
    server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({1921, 0, 1920, 1080}));
  });

  WaitForAllDisplaysReady();

  // Client side WaylandOutput ids.
  ASSERT_EQ(2u, screen_->GetAllDisplays().size());
  const uint32_t output1_id =
      screen_->GetOutputIdForDisplayId(screen_->GetAllDisplays().at(0).id());
  const uint32_t output2_id =
      screen_->GetOutputIdForDisplayId(screen_->GetAllDisplays().at(1).id());

  // Client side surface.
  WaylandSurface* wayland_surface = window_->root_surface();
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre());

  auto* output_manager = connection_->wayland_output_manager();
  ASSERT_EQ(2u, output_manager->GetAllOutputs().size());

  // Shared wl_output resource ids.
  const uint32_t wl_output1_id = GetObjIdForOutput(output1_id);
  const uint32_t wl_output2_id = GetObjIdForOutput(output2_id);

  PostToServerAndWait([id = surface_id_, wl_output1_id,
                       wl_output2_id](wl::TestWaylandServerThread* server) {
    // Send the window to |output1| and |output2|.
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(id)->resource(),
        server->GetObject<wl::TestOutput>(wl_output1_id)->resource());
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(id)->resource(),
        server->GetObject<wl::TestOutput>(wl_output2_id)->resource());
  });

  // The window entered two outputs.
  EXPECT_THAT(wayland_surface->entered_outputs(),
              ElementsAre(output1_id, output2_id));

  // The window must prefer the output that it entered first.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Create the third output.
    server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({0, 1081, 1920, 1080}));
  });

  WaitForAllDisplaysReady();

  ASSERT_EQ(3u, screen_->GetAllDisplays().size());
  const uint32_t output3_id =
      screen_->GetOutputIdForDisplayId(screen_->GetAllDisplays().at(2).id());

  EXPECT_EQ(3u, output_manager->GetAllOutputs().size());
  const uint32_t wl_output3_id = GetObjIdForOutput(output3_id);

  PostToServerAndWait(
      [id = surface_id_, wl_output3_id](wl::TestWaylandServerThread* server) {
        // Send window into 3rd output.
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output3_id)->resource());
      });

  // The window entered three outputs...
  EXPECT_THAT(wayland_surface->entered_outputs(),
              ElementsAre(output1_id, output2_id, output3_id));

  // but it still must prefer the output that it entered first.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);

  PostToServerAndWait([wl_output2_id](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(wl_output2_id);
    // Pretend that the output2 has scale factor equals to 2 now.
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
    output2->Flush();
  });

  // Entered outputs remain the same.
  EXPECT_THAT(wayland_surface->entered_outputs(),
              ElementsAre(output1_id, output2_id, output3_id));

  // It must be the second entered output now.
  EXPECT_EQ(2, connection_->wayland_output_manager()
                   ->GetOutput(output2_id)
                   ->scale_factor());

  // The window_ must return the output with largest scale.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output2_id);

  PostToServerAndWait([wl_output1_id](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output1 = server->GetObject<wl::TestOutput>(wl_output1_id);
    // Now, the output1 changes its scale factor to 2 as well.
    output1->SetScale(2);
    output1->SetDeviceScaleFactor(2);
    output1->Flush();
  });

  // It must be the very first output now.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);

  PostToServerAndWait([wl_output1_id](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output1 = server->GetObject<wl::TestOutput>(wl_output1_id);
    // Now, the output1 changes its scale factor back to 1.
    output1->SetScale(1);
    output1->SetDeviceScaleFactor(1);
    output1->Flush();
  });

  // It must be the very the second output now.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output2_id);

  PostToServerAndWait([wl_output2_id](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(wl_output2_id);
    // All outputs have scale factor of 1. window_ prefers the output that
    // it entered first again.
    output2->SetScale(1);
    output2->SetDeviceScaleFactor(1);
    output2->Flush();
  });

  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);
}

TEST_P(WaylandWindowTest, GetChildrenPreferredOutput) {
  VerifyAndClearExpectations();

  // Buffer scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->applied_state().window_scale);

  MockWaylandPlatformWindowDelegate menu_window_delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate, window_->GetWidget());

  menu_window->Show(false);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Update the first output.
    wl::TestOutput* output1 = server->output();
    output1->SetPhysicalAndLogicalBounds({1920, 1080});
    output1->Flush();

    // Create a 2nd output.
    server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({1921, 0, 1920, 1080}));
  });

  WaitForAllDisplaysReady();

  // Client side WaylandOutput ids.
  ASSERT_EQ(2u, screen_->GetAllDisplays().size());
  const uint32_t output1_id =
      screen_->GetOutputIdForDisplayId(screen_->GetAllDisplays().at(0).id());
  const uint32_t output2_id =
      screen_->GetOutputIdForDisplayId(screen_->GetAllDisplays().at(1).id());

  auto* output_manager = connection_->wayland_output_manager();
  ASSERT_EQ(2u, output_manager->GetAllOutputs().size());

  // Shared wl_output resource ids.
  const uint32_t wl_output1_id = GetObjIdForOutput(output1_id);
  const uint32_t wl_output2_id = GetObjIdForOutput(output2_id);

  // Client side surfaces.
  WaylandSurface* root_surface = window_->root_surface();
  WaylandSurface* menu_surface = menu_window->root_surface();
  const uint32_t menu_surface_id = menu_surface->get_surface_id();

  // Neither surface should have entered any output.
  EXPECT_THAT(root_surface->entered_outputs(), ElementsAre());
  EXPECT_THAT(menu_surface->entered_outputs(), ElementsAre());

  // Send the toplevel window into output1.
  PostToServerAndWait(
      [id = surface_id_, wl_output1_id](wl::TestWaylandServerThread* server) {
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output1_id)->resource());
      });

  // The toplevel window entered the output.
  EXPECT_THAT(root_surface->entered_outputs(), ElementsAre(output1_id));
  EXPECT_THAT(menu_surface->entered_outputs(), ElementsAre());

  // The menu also thinks it entered the same output.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  // Send the menu window into output2, while the toplevel is still on output1.
  PostToServerAndWait(
      [menu_surface_id, wl_output2_id](wl::TestWaylandServerThread* server) {
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(menu_surface_id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output2_id)->resource());
      });

  // The menu surface should be aware of the output that Wayland sent it.
  EXPECT_THAT(root_surface->entered_outputs(), ElementsAre(output1_id));
  EXPECT_THAT(menu_surface->entered_outputs(), ElementsAre(output2_id));

  // The menu still prefers output1.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  // Send the toplevel window into output2.
  PostToServerAndWait(
      [id = surface_id_, wl_output2_id](wl::TestWaylandServerThread* server) {
        wl_surface_send_enter(
            server->GetObject<wl::MockSurface>(id)->resource(),
            server->GetObject<wl::TestOutput>(wl_output2_id)->resource());
      });

  // The toplevel window has now entered 2 outputs, in chronological order.
  EXPECT_THAT(root_surface->entered_outputs(),
              ElementsAre(output1_id, output2_id));
  EXPECT_THAT(menu_surface->entered_outputs(), ElementsAre(output2_id));

  // With the same scale factor, the toplevel window prefers the earlier output.
  // The menu window always prefers whatever the parent prefers.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output1_id);
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  PostToServerAndWait([wl_output2_id](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output2 = server->GetObject<wl::TestOutput>(wl_output2_id);
    // Now, the output2 changes its scale factor to 2.
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
    output2->Flush();
  });

  // The toplevel window prefers the output with higher scale factor.
  // The menu window still prefers whatever the parent prefers.
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(), output2_id);
  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());
}

// Tests that xdg_popup is configured with default anchor properties and bounds
// if delegate doesn't have anchor properties set.
TEST_P(WaylandWindowTest, PopupPassesDefaultAnchorInformation) {
  PopupPosition menu_window_positioner, nested_menu_window_positioner;

  menu_window_positioner = {gfx::Rect(439, 46, 1, 1), gfx::Size(287, 409),
                            XDG_POSITIONER_ANCHOR_TOP_LEFT,
                            XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
                            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};
  nested_menu_window_positioner = {gfx::Rect(285, 1, 1, 1), gfx::Size(305, 99),
                                   XDG_POSITIONER_ANCHOR_TOP_LEFT,
                                   XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
                                   XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};

  auto* toplevel_window = window_.get();
  toplevel_window->SetBoundsInDIP(gfx::Rect(739, 574));

  // Case 1: properties are not provided. In this case, bounds' origin must
  // be used as anchor rect and anchor position, gravity and constraints should
  // be normal.
  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootMenu));
  EXPECT_CALL(menu_window_delegate, GetOwnedWindowAnchorAndRectInDIP())
      .WillOnce(Return(absl::nullopt));
  gfx::Rect menu_window_bounds(gfx::Point(439, 46),
                               menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &menu_window_delegate,
      toplevel_window->GetWidget());
  EXPECT_TRUE(menu_window);

  VerifyXdgPopupPosition(menu_window.get(), menu_window_positioner);

  EXPECT_CALL(menu_window_delegate, OnBoundsChanged(_)).Times(0);
  SendConfigureEventPopup(menu_window.get(), menu_window_bounds);

  EXPECT_EQ(menu_window->GetBoundsInDIP(), menu_window_bounds);

  // Case 2: the nested menu window is positioned normally.
  MockWaylandPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kChildMenu));
  gfx::Rect nested_menu_window_bounds(gfx::Point(724, 47),
                                      nested_menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, nested_menu_window_bounds,
          &nested_menu_window_delegate, menu_window->GetWidget());
  EXPECT_TRUE(nested_menu_window);

  VerifyXdgPopupPosition(nested_menu_window.get(),
                         nested_menu_window_positioner);
}

// Tests that xdg_popup is configured with anchor properties received from
// delegate.
TEST_P(WaylandWindowTest, PopupPassesSetAnchorInformation) {
  PopupPosition menu_window_positioner, nested_menu_window_positioner;

  menu_window_positioner = {gfx::Rect(468, 46, 28, 28), gfx::Size(320, 404),
                            XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT,
                            XDG_POSITIONER_GRAVITY_BOTTOM_LEFT,
                            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};
  nested_menu_window_positioner = {
      gfx::Rect(4, 83, 312, 1), gfx::Size(480, 294),
      XDG_POSITIONER_ANCHOR_TOP_RIGHT, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
          XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X};

  auto* toplevel_window = window_.get();
  toplevel_window->SetBoundsInDIP(gfx::Rect(508, 212));

  MockWaylandPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootMenu));
  ui::OwnedWindowAnchor anchor = {
      gfx::Rect(menu_window_positioner.anchor_rect),
      OwnedWindowAnchorPosition::kBottomRight,
      OwnedWindowAnchorGravity::kBottomLeft,
      OwnedWindowConstraintAdjustment::kAdjustmentFlipY};
  EXPECT_CALL(menu_window_delegate, GetOwnedWindowAnchorAndRectInDIP())
      .WillOnce(Return(anchor));
  gfx::Rect menu_window_bounds(gfx::Point(176, 74),
                               menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &menu_window_delegate,
      toplevel_window->GetWidget());
  EXPECT_TRUE(menu_window);

  VerifyXdgPopupPosition(menu_window.get(), menu_window_positioner);

  MockWaylandPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kChildMenu));
  anchor = {{180, 157, 312, 1},
            OwnedWindowAnchorPosition::kTopRight,
            OwnedWindowAnchorGravity::kBottomRight,
            OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
                OwnedWindowConstraintAdjustment::kAdjustmentFlipX};
  EXPECT_CALL(nested_menu_window_delegate, GetOwnedWindowAnchorAndRectInDIP())
      .WillOnce(Return(anchor));
  gfx::Rect nested_menu_window_bounds(gfx::Point(492, 157),
                                      nested_menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, nested_menu_window_bounds,
          &nested_menu_window_delegate, menu_window->GetWidget());
  EXPECT_TRUE(nested_menu_window);

  VerifyXdgPopupPosition(nested_menu_window.get(),
                         nested_menu_window_positioner);
}

TEST_P(WaylandWindowTest, SetBoundsResizesEmptySizes) {
  auto* toplevel_window = window_.get();
  toplevel_window->SetBoundsInDIP(gfx::Rect(666, 666));

  testing::NiceMock<MockWaylandPlatformWindowDelegate> popup_delegate;
  gfx::Rect menu_window_bounds(gfx::Point(0, 0), {0, 0});
  std::unique_ptr<WaylandWindow> popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &popup_delegate,
      toplevel_window->GetWidget());
  EXPECT_TRUE(popup);

  popup->SetBoundsInDIP({0, 0, 0, 0});

  VerifyXdgPopupPosition(
      popup.get(),
      {gfx::Rect(0, 0, 1, 1), gfx::Size(1, 1), XDG_POSITIONER_ANCHOR_TOP_LEFT,
       XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
       XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y});
}

TEST_P(WaylandWindowTest, SetOpaqueRegion) {
  gfx::Rect new_bounds(500, 600);
  SkIRect rect =
      SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  auto state_array = MakeStateArray({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(surface_id_, new_bounds.size(), state_array);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  VerifyAndClearExpectations();
  PostToServerAndWait(
      [id = surface_id_, new_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        EXPECT_EQ(mock_surface->opaque_region(), new_bounds);
      });

  new_bounds.set_size(gfx::Size(1000, 534));
  rect = SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  SendConfigureEvent(surface_id_, new_bounds.size(), state_array);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  VerifyAndClearExpectations();
  PostToServerAndWait(
      [id = surface_id_, new_bounds](wl::TestWaylandServerThread* server) {
        wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
        ASSERT_TRUE(mock_surface);
        EXPECT_EQ(mock_surface->opaque_region(), new_bounds);
      });
}

TEST_P(WaylandWindowTest, OnCloseRequest) {
  EXPECT_CALL(delegate_, OnCloseRequest());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    wl::MockXdgSurface* xdg_surface = mock_surface->xdg_surface();
    xdg_toplevel_send_close(xdg_surface->xdg_toplevel()->resource());
  });
}

TEST_P(WaylandWindowTest, WaylandPopupSimpleParent) {
  VerifyAndClearExpectations();

  EXPECT_CALL(delegate_, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  // WaylandPopup must ignore the parent provided by aura and should always
  // use focused window instead.
  gfx::Rect wayland_popup_bounds(gfx::Point(15, 15), gfx::Size(10, 10));
  std::unique_ptr<WaylandWindow> wayland_popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kTooltip, wayland_popup_bounds, &delegate_,
      window_->GetWidget());
  EXPECT_TRUE(wayland_popup);

  wayland_popup->Show(false);

  PostToServerAndWait(
      [id = wayland_popup->root_surface()->get_surface_id(),
       wayland_popup_bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface_popup = server->GetObject<wl::MockSurface>(id);
        auto* mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

        EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
                  wayland_popup_bounds.origin());
        EXPECT_EQ(mock_surface_popup->opaque_region(),
                  gfx::Rect(wayland_popup_bounds.size()));
      });

  wayland_popup->Hide();
}

TEST_P(WaylandWindowTest, WaylandPopupNestedParent) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Point(10, 10), gfx::Size(100, 100));
  auto menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &delegate_,
      window_->GetWidget());
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();
  SetPointerFocusedWindow(menu_window.get());

  std::vector<PlatformWindowType> window_types{PlatformWindowType::kMenu,
                                               PlatformWindowType::kTooltip};
  for (const auto& type : window_types) {
    gfx::Rect nested_wayland_popup_bounds(gfx::Point(15, 15),
                                          gfx::Size(10, 10));
    auto nested_wayland_popup =
        CreateWaylandWindowWithParams(type, nested_wayland_popup_bounds,
                                      &delegate_, menu_window->GetWidget());
    EXPECT_TRUE(nested_wayland_popup);

    VerifyAndClearExpectations();

    nested_wayland_popup->Show(false);

    PostToServerAndWait(
        [id = nested_wayland_popup->root_surface()->get_surface_id(),
         nested_wayland_popup_bounds,
         menu_window_bounds](wl::TestWaylandServerThread* server) {
          auto* mock_surface_nested = server->GetObject<wl::MockSurface>(id);
          auto* mock_xdg_popup_nested =
              mock_surface_nested->xdg_surface()->xdg_popup();

          auto new_origin = nested_wayland_popup_bounds.origin() -
                            menu_window_bounds.origin().OffsetFromOrigin();
          EXPECT_EQ(mock_xdg_popup_nested->anchor_rect().origin(), new_origin);
          EXPECT_EQ(mock_surface_nested->opaque_region(),
                    gfx::Rect(nested_wayland_popup_bounds.size()));
        });

    SetPointerFocusedWindow(nullptr);
    nested_wayland_popup->Hide();
  }
}

// Tests that size constraints returned by the `ui::PlatformWindowDelegate` are
// obeyed by the window when its bounds are set internally via its
// SetBoundsInDIP() implementation.
TEST_P(WaylandWindowTest, SizeConstraintsInternal) {
  constexpr gfx::Size kMinSize{100, 100};
  constexpr gfx::Size kMaxSize{300, 300};

  window_->SetBoundsInDIP({0, 0, 200, 200});

  gfx::Rect even_smaller_bounds(kMinSize);
  even_smaller_bounds.Inset(10);
  even_smaller_bounds.set_origin({0, 0});

  EXPECT_CALL(delegate_, GetMinimumSizeForWindow()).WillOnce(Return(kMinSize));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  window_->SetBoundsInDIP(even_smaller_bounds);

  VerifyAndClearExpectations();

  gfx::Rect even_greater_bounds(kMaxSize);
  even_greater_bounds.Outset(10);
  even_greater_bounds.set_origin({0, 0});

  EXPECT_CALL(delegate_, GetMaximumSizeForWindow()).WillOnce(Return(kMaxSize));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  window_->SetBoundsInDIP(even_greater_bounds);
}

// Tests that size constraints returned by the `ui::PlatformWindowDelegate` are
// obeyed by the window when its bounds are set externally via the configure
// event sent by the compositor.
TEST_P(WaylandWindowTest, SizeConstraintsExternal) {
  constexpr gfx::Size kMinSize{100, 100};
  constexpr gfx::Size kMaxSize{300, 300};

  EXPECT_CALL(delegate_, GetMinimumSizeForWindow())
      .WillRepeatedly(Return(kMinSize));
  EXPECT_CALL(delegate_, GetMaximumSizeForWindow())
      .WillRepeatedly(Return(kMaxSize));

  window_->SetBoundsInDIP({0, 0, 200, 200});

  auto state = InitializeWlArrayWithActivatedState();

  gfx::Rect even_smaller_bounds(kMinSize);
  even_smaller_bounds.Inset(10);
  even_smaller_bounds.set_origin({0, 0});

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  SendConfigureEvent(surface_id_, even_smaller_bounds.size(), state);
  VerifyAndClearExpectations();

  EXPECT_CALL(delegate_, GetMinimumSizeForWindow())
      .WillRepeatedly(Return(kMinSize));
  EXPECT_CALL(delegate_, GetMaximumSizeForWindow())
      .WillRepeatedly(Return(kMaxSize));

  gfx::Rect even_greater_bounds(kMaxSize);
  even_greater_bounds.Outset(10);
  even_greater_bounds.set_origin({0, 0});

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kDefaultBoundsChange)));

  SendConfigureEvent(surface_id_, even_greater_bounds.size(), state);
}

TEST_P(WaylandWindowTest, OnSizeConstraintsChanged) {
  const bool kBooleans[] = {false, true};
  for (bool has_min_size : kBooleans) {
    for (bool has_max_size : kBooleans) {
      absl::optional<gfx::Size> min_size =
          has_min_size ? absl::optional<gfx::Size>(gfx::Size(100, 200))
                       : absl::nullopt;
      absl::optional<gfx::Size> max_size =
          has_max_size ? absl::optional<gfx::Size>(gfx::Size(300, 400))
                       : absl::nullopt;
      EXPECT_CALL(delegate_, GetMinimumSizeForWindow())
          .WillOnce(Return(min_size));
      EXPECT_CALL(delegate_, GetMaximumSizeForWindow())
          .WillOnce(Return(max_size));

      PostToServerAndWait([id = surface_id_, has_min_size,
                           has_max_size](wl::TestWaylandServerThread* server) {
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        auto* mock_xdg_surface = mock_surface->xdg_surface();
        EXPECT_CALL(*mock_xdg_surface->xdg_toplevel(), SetMinSize(100, 200))
            .Times(has_min_size ? 1 : 0);
        EXPECT_CALL(*mock_xdg_surface->xdg_toplevel(), SetMaxSize(300, 400))
            .Times(has_max_size ? 1 : 0);
      });

      window_->SizeConstraintsChanged();
      VerifyAndClearExpectations();
    }
  }
}

TEST_P(WaylandWindowTest, DestroysCreatesSurfaceOnHideShow) {
  MockWaylandPlatformWindowDelegate delegate;
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              gfx::Rect(100, 100), &delegate);
  ASSERT_TRUE(window);

  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_TRUE(mock_surface->xdg_surface());
    EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());
  });

  window->Hide();

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_FALSE(mock_surface->xdg_surface());
  });

  window->Show(false);

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_TRUE(mock_surface->xdg_surface());
    EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());
  });
}

TEST_P(WaylandWindowTest, DestroysCreatesPopupsOnHideShow) {
  MockWaylandPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                              gfx::Rect(50, 50), &delegate,
                                              window_->GetWidget());
  ASSERT_TRUE(window);

  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_TRUE(mock_surface->xdg_surface());
    EXPECT_TRUE(mock_surface->xdg_surface()->xdg_popup());
  });

  window->Hide();

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_FALSE(mock_surface->xdg_surface());
  });

  window->Show(false);

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_TRUE(mock_surface->xdg_surface());
    EXPECT_TRUE(mock_surface->xdg_surface()->xdg_popup());
  });
}

TEST_P(WaylandWindowTest, ReattachesBackgroundOnShow) {
  EXPECT_TRUE(connection_->buffer_manager_host());

  auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  // Setup wl_buffers.
  constexpr uint32_t buffer_id1 = 1;
  constexpr uint32_t buffer_id2 = 2;
  gfx::Size buffer_size(1024, 768);
  auto length = 1024 * 768 * 4;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id1);
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id2);

  base::RunLoop().RunUntilIdle();

  // Create window.
  MockWaylandPlatformWindowDelegate delegate;
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              gfx::Rect(100, 100), &delegate);
  ASSERT_TRUE(window);
  auto states = InitializeWlArrayWithActivatedState();

  // Configure window to be ready to attach wl_buffers.
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_TRUE(mock_surface->xdg_surface());
    EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());
  });
  SendConfigureEvent(surface_id, {100, 100}, states);

  // Commit a frame with only background.
  std::vector<wl::WaylandOverlayConfig> overlays;
  wl::WaylandOverlayConfig background;
  background.z_order = INT32_MIN;
  background.buffer_id = buffer_id1;
  overlays.push_back(std::move(background));
  buffer_manager_gpu_->CommitOverlays(window->GetWidget(), 1u, gfx::FrameData(),
                                      std::move(overlays));

  // Let mojo messages from gpu to host go through.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    mock_surface->SendFrameCallback();
  });

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    EXPECT_NE(mock_surface->attached_buffer(), nullptr);
  });

  window->Hide();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    mock_surface->SendFrameCallback();
  });

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    mock_surface->ReleaseBuffer(mock_surface->attached_buffer());
  });
  window->Show(false);

  SendConfigureEvent(surface_id, {100, 100}, states);

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    // Expects to receive an attach request on next frame.
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  });

  // Commit a frame with only the primary_plane.
  overlays.clear();
  wl::WaylandOverlayConfig primary;
  primary.z_order = 0;
  primary.buffer_id = buffer_id2;
  overlays.push_back(std::move(primary));
  buffer_manager_gpu_->CommitOverlays(window->GetWidget(), 2u, gfx::FrameData(),
                                      std::move(overlays));

  // Let mojo messages from gpu to host go through.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    // WaylandWindow should automatically reattach the background.
    EXPECT_NE(mock_surface->attached_buffer(), nullptr);
  });
}

// TODO(https://crbug.com/1448391): Reenable for Lacros when adjusted for screen
// coordinates.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SetsPropertiesOnShow DISABLED_SetsPropertiesOnShow
#else
#define MAYBE_SetsPropertiesOnShow SetsPropertiesOnShow
#endif
// Tests that if the window gets hidden and shown again, the title, app id and
// size constraints remain the same.
TEST_P(WaylandWindowTest, MAYBE_SetsPropertiesOnShow) {
  constexpr char kAppId[] = "wayland_test";
  const std::u16string kTitle(u"WaylandWindowTest");

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(100, 100);
  properties.type = PlatformWindowType::kWindow;
  properties.wm_class_class = kAppId;

  MockWaylandPlatformWindowDelegate delegate;
  auto window =
      delegate.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_TRUE(window);
  window->Show(false);

  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id, app_id = window->GetWindowUniqueId()](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();
    // Only app id must be set now.
    EXPECT_EQ(app_id, mock_xdg_toplevel->app_id());
    EXPECT_TRUE(mock_xdg_toplevel->title().empty());
    EXPECT_TRUE(mock_xdg_toplevel->min_size().IsEmpty());
    EXPECT_TRUE(mock_xdg_toplevel->max_size().IsEmpty());
  });

  // Now, propagate size constraints and title.
  absl::optional<gfx::Size> min_size(gfx::Size(1, 1));
  absl::optional<gfx::Size> max_size(gfx::Size(100, 100));
  EXPECT_CALL(delegate, GetMinimumSizeForWindow())
      .WillRepeatedly(Return(min_size));
  EXPECT_CALL(delegate, GetMaximumSizeForWindow())
      .WillRepeatedly(Return(max_size));

  PostToServerAndWait([surface_id, min_size, max_size,
                       kTitle](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();
    EXPECT_CALL(*mock_xdg_toplevel, SetMinSize(min_size.value().width(),
                                               min_size.value().height()));
    EXPECT_CALL(*mock_xdg_toplevel, SetMaxSize(max_size.value().width(),
                                               max_size.value().height()));
    EXPECT_CALL(*mock_xdg_toplevel, SetTitle(base::UTF16ToUTF8(kTitle)));
  });

  window->SetTitle(kTitle);
  window->SizeConstraintsChanged();

  window->Hide();

  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());

  window->Show(false);

  PostToServerAndWait([surface_id, min_size, max_size, kTitle,
                       app_id = window->GetWindowUniqueId()](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();

    mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();
    // We can't mock all those methods above as long as the xdg_toplevel is
    // created and destroyed on each show and hide call. However, it is the same
    // WaylandToplevelWindow object that cached the values we set and must
    // restore them on Show().
    EXPECT_EQ(mock_xdg_toplevel->min_size(), min_size.value());
    EXPECT_EQ(mock_xdg_toplevel->max_size(), max_size.value());
    EXPECT_EQ(app_id, mock_xdg_toplevel->app_id());
    EXPECT_EQ(mock_xdg_toplevel->title(), base::UTF16ToUTF8(kTitle));
  });
}

// Tests that a popup window is created using the serial of button press
// events as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnButtonPressSerial) {
  for (bool use_explicit_grab : {false, true}) {
    base::test::ScopedCommandLine command_line_;
    if (use_explicit_grab) {
      command_line_.GetProcessCommandLine()->AppendSwitch(
          switches::kUseWaylandExplicitGrab);
    }

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(
          server->seat()->resource(),
          WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    });

    constexpr uint32_t keyboard_enter_serial = 1;
    constexpr uint32_t pointer_enter_serial = 2;
    constexpr uint32_t button_press_serial = 3;
    constexpr uint32_t button_release_serial = 4;

    PostToServerAndWait([id =
                             surface_id_](wl::TestWaylandServerThread* server) {
      wl::MockSurface* toplevel_surface =
          server->GetObject<wl::MockSurface>(id);
      wl::ScopedWlArray empty({});
      wl_keyboard_send_enter(server->seat()->keyboard()->resource(),
                             keyboard_enter_serial,
                             toplevel_surface->resource(), empty.get());

      wl_pointer_send_enter(server->seat()->pointer()->resource(),
                            pointer_enter_serial, toplevel_surface->resource(),
                            wl_fixed_from_int(0), wl_fixed_from_int(0));

      // Send two events - button down and button up.
      wl_pointer_send_button(server->seat()->pointer()->resource(),
                             button_press_serial, 1002, BTN_LEFT,
                             WL_POINTER_BUTTON_STATE_PRESSED);
      wl_pointer_send_button(server->seat()->pointer()->resource(),
                             button_release_serial, 1004, BTN_LEFT,
                             WL_POINTER_BUTTON_STATE_RELEASED);
    });

    // Create a popup window and verify the client used correct serial.
    MockWaylandPlatformWindowDelegate delegate;
    EXPECT_CALL(delegate, GetMenuType())
        .WillOnce(Return(MenuType::kRootContextMenu));
    auto popup = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                               gfx::Rect(50, 50), &delegate,
                                               window_->GetWidget());
    ASSERT_TRUE(popup);

    const uint32_t surface_id = popup->root_surface()->get_surface_id();
    // Unfortunately, everything has to be captured as |use_explicit_grab| may
    // not be used and |maybe_unused| doesn't work with lambda captures.
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* test_popup = GetTestXdgPopupByWindow(server, surface_id);
      ASSERT_TRUE(test_popup);

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      if (use_explicit_grab) {
        EXPECT_NE(test_popup->grab_serial(), button_release_serial);
        EXPECT_EQ(test_popup->grab_serial(), button_press_serial);
      } else {
        EXPECT_EQ(test_popup->grab_serial(), 0U);
      }
#else
      // crbug.com/1320528: Lacros uses explicit grab always.
      EXPECT_NE(test_popup->grab_serial(), button_release_serial);
      EXPECT_EQ(test_popup->grab_serial(), button_press_serial);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    });
  }
}

// Tests that a popup window is created using the serial of touch down events
// as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnTouchDownSerial) {
  for (bool use_explicit_grab : {false, true}) {
    base::test::ScopedCommandLine command_line_;
    if (use_explicit_grab) {
      command_line_.GetProcessCommandLine()->AppendSwitch(
          switches::kUseWaylandExplicitGrab);
    }
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(
          server->seat()->resource(),
          WL_SEAT_CAPABILITY_TOUCH | WL_SEAT_CAPABILITY_KEYBOARD);
    });

    constexpr uint32_t enter_serial = 1;
    constexpr uint32_t touch_down_serial = 2;
    constexpr uint32_t touch_up_serial = 3;

    PostToServerAndWait([id =
                             surface_id_](wl::TestWaylandServerThread* server) {
      wl::MockSurface* toplevel_surface =
          server->GetObject<wl::MockSurface>(id);
      wl::ScopedWlArray empty({});
      wl_keyboard_send_enter(server->seat()->keyboard()->resource(),
                             enter_serial, toplevel_surface->resource(),
                             empty.get());

      // Send two events - touch down and touch up.
      wl_touch_send_down(server->seat()->touch()->resource(), touch_down_serial,
                         0, toplevel_surface->resource(), 0 /* id */,
                         wl_fixed_from_int(50), wl_fixed_from_int(100));
      wl_touch_send_frame(server->seat()->touch()->resource());

      wl_touch_send_up(server->seat()->touch()->resource(), touch_up_serial,
                       1000, 0 /* id */);
      wl_touch_send_frame(server->seat()->touch()->resource());
    });

    // Create a popup window and verify the client used correct serial.
    MockWaylandPlatformWindowDelegate delegate;
    EXPECT_CALL(delegate, GetMenuType())
        .WillRepeatedly(Return(MenuType::kRootContextMenu));
    auto popup = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                               gfx::Rect(50, 50), &delegate,
                                               window_->GetWidget());
    ASSERT_TRUE(popup);

    const uint32_t surface_id = popup->root_surface()->get_surface_id();
    // Unfortunately, everything has to be captured as |use_explicit_grab| may
    // not be used and |maybe_unused| doesn't work with lambda captures.
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* test_popup = GetTestXdgPopupByWindow(server, surface_id);
      ASSERT_TRUE(test_popup);
      // crbug.com/1320528: Lacros uses explicit grab always.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      // Unless the use-wayland-explicit-grab switch is set, touch events
      // are the exception, i.e: the serial sent before the "up" event
      // (latest) cannot be used, otherwise, some compositors may dismiss
      // popups.
      if (!use_explicit_grab)
        EXPECT_EQ(test_popup->grab_serial(), 0U);
#endif
    });

    popup->Hide();

    PostToServerAndWait([id =
                             surface_id_](wl::TestWaylandServerThread* server) {
      wl::MockSurface* toplevel_surface =
          server->GetObject<wl::MockSurface>(id);
      // Send a single down event now.
      wl_touch_send_down(server->seat()->touch()->resource(), touch_down_serial,
                         0, toplevel_surface->resource(), 0 /* id */,
                         wl_fixed_from_int(50), wl_fixed_from_int(100));
      wl_touch_send_frame(server->seat()->touch()->resource());
    });

    popup->Show(false);

    // Unfortunately, everything has to be captured as |use_explicit_grab| may
    // not be used and |maybe_unused| doesn't work with lambda captures.
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* test_popup = GetTestXdgPopupByWindow(server, surface_id);
      ASSERT_TRUE(test_popup);

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      uint32_t expected_serial = touch_down_serial;
      auto env = base::Environment::Create();
      if (base::nix::GetDesktopEnvironment(env.get()) ==
          base::nix::DESKTOP_ENVIRONMENT_GNOME) {
        // We do not grab with touch events on gnome shell.
        expected_serial = 0u;
      }
      if (use_explicit_grab) {
        EXPECT_EQ(test_popup->grab_serial(), expected_serial);
      } else {
        EXPECT_EQ(test_popup->grab_serial(), 0U);
      }
#else
      // crbug.com/1320528: Lacros uses explicit grab always.
      EXPECT_EQ(test_popup->grab_serial(), touch_down_serial);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    });
  }
}

// Tests nested menu windows get the topmost window in the stack of windows
// within the same family/tree.
TEST_P(WaylandWindowTest, NestedPopupWindowsGetCorrectParent) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(10, 20, 20, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &delegate_,
      window_->GetWidget());
  EXPECT_TRUE(menu_window);

  EXPECT_TRUE(menu_window->parent_window() == window_.get());

  gfx::Rect menu_window_bounds2(gfx::Rect(20, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds2, &delegate_,
      menu_window->GetWidget());
  EXPECT_TRUE(menu_window2);

  EXPECT_TRUE(menu_window2->parent_window() == menu_window.get());

  gfx::Rect menu_window_bounds3(gfx::Rect(30, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window3 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds3, &delegate_,
      menu_window2->GetWidget());
  EXPECT_TRUE(menu_window3);

  EXPECT_TRUE(menu_window3->parent_window() == menu_window2.get());

  gfx::Rect menu_window_bounds4(gfx::Rect(40, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window4 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds4, &delegate_,
      menu_window3->GetWidget());
  EXPECT_TRUE(menu_window4);

  EXPECT_TRUE(menu_window4->parent_window() == menu_window3.get());
}

TEST_P(WaylandWindowTest, DoesNotGrabPopupIfNoSeat) {
  // Create a popup window and verify the grab serial is not set.
  MockWaylandPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  auto popup = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                             gfx::Rect(50, 50), &delegate,
                                             window_->GetWidget());
  ASSERT_TRUE(popup);

  PostToServerAndWait([surface_id = popup->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* test_popup = GetTestXdgPopupByWindow(server, surface_id);
    ASSERT_TRUE(test_popup);
    EXPECT_EQ(test_popup->grab_serial(), 0u);
  });
}

// Regression test for https://crbug.com/1247799.
TEST_P(WaylandWindowTest, DoesNotGrabPopupUnlessParentHasGrab) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  SetPointerFocusedWindow(window_.get());

  // Emulate a root menu creation with no serial available and ensure
  // ozone/wayland does not attempt to grab it.
  connection_->serial_tracker().ClearForTesting();

  MockWaylandPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> root_menu;
  EXPECT_CALL(delegate, GetMenuType()).WillOnce(Return(MenuType::kRootMenu));
  root_menu = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                            gfx::Rect(50, 50), &delegate,
                                            window_->GetWidget());
  VerifyAndClearExpectations();
  Mock::VerifyAndClearExpectations(&delegate);
  ASSERT_TRUE(root_menu);

  PostToServerAndWait(
      [surface_id = root_menu->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* server_root_menu = GetTestXdgPopupByWindow(server, surface_id);
        ASSERT_TRUE(server_root_menu);
        EXPECT_EQ(server_root_menu->grab_serial(), 0u);
      });

  // Emulate a nested menu creation triggered by a mouse button event and ensure
  // ozone/wayland does not attempt to grab it, as its parent also has not grab.
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  PostToServerAndWait(
      [surface_id = root_menu->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* server_root_menu_surface =
            server->GetObject<wl::MockSurface>(surface_id);
        ASSERT_TRUE(server_root_menu_surface);

        auto* pointer_resource = server->seat()->pointer()->resource();
        wl_pointer_send_enter(pointer_resource, 3u /*serial*/,
                              server_root_menu_surface->resource(), 0, 0);
        wl_pointer_send_frame(pointer_resource);
        wl_pointer_send_button(pointer_resource, 4u /*serial*/, 1, BTN_LEFT,
                               WL_POINTER_BUTTON_STATE_PRESSED);
        wl_pointer_send_frame(pointer_resource);
      });
  Mock::VerifyAndClearExpectations(&delegate);

  MockWaylandPlatformWindowDelegate delegate_2;
  std::unique_ptr<WaylandWindow> child_menu;
  EXPECT_CALL(delegate_2, GetMenuType()).WillOnce(Return(MenuType::kChildMenu));
  child_menu = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                             gfx::Rect(10, 10), &delegate_2,
                                             root_menu->GetWidget());
  VerifyAndClearExpectations();
  Mock::VerifyAndClearExpectations(&delegate_2);
  ASSERT_TRUE(child_menu);

  PostToServerAndWait(
      [surface_id = child_menu->root_surface()->get_surface_id(),
       root_menu_id = root_menu->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* server_child_menu = GetTestXdgPopupByWindow(server, surface_id);
        ASSERT_TRUE(server_child_menu);
        EXPECT_EQ(server_child_menu->grab_serial(), 0u);

        auto* pointer_resource = server->seat()->pointer()->resource();
        auto* server_root_menu_surface =
            server->GetObject<wl::MockSurface>(root_menu_id);
        wl_pointer_send_leave(pointer_resource, 5u /*serial*/,
                              server_root_menu_surface->resource());
        wl_pointer_send_frame(pointer_resource);
      });
}

TEST_P(WaylandWindowTest, InitialBounds) {
  testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_2;
  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(10, 10, 200, 200), &delegate_2);
  toplevel->HandleAuraToplevelConfigure(0, 0, 0, 0, {false, false, true});
  toplevel->HandleSurfaceConfigure(2);
  EXPECT_EQ(gfx::Rect(10, 10, 200, 200), toplevel->GetBoundsInDIP());
}

TEST_P(WaylandWindowTest, PrimarySnappedState) {
  testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_2;
  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 200, 200), &delegate_2);
  toplevel->HandleAuraToplevelConfigure(0, 0, 100, 200,
                                        {.is_maximized = false,
                                         .is_fullscreen = false,
                                         .is_activated = true,
                                         .is_snapped_primary = true});
  toplevel->HandleSurfaceConfigure(2);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), toplevel->GetBoundsInDIP());
}

TEST_P(WaylandWindowTest, SecondarySnappedState) {
  testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_2;
  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 200, 200), &delegate_2);
  toplevel->HandleAuraToplevelConfigure(100, 0, 100, 200,
                                        {.is_maximized = false,
                                         .is_fullscreen = false,
                                         .is_activated = true,
                                         .is_snapped_secondary = true});
  toplevel->HandleSurfaceConfigure(2);
  EXPECT_EQ(gfx::Rect(100, 0, 100, 200), toplevel->GetBoundsInDIP());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(WaylandWindowTest, ImmersiveFullscreen) {
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_2;
  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(10, 10, 200, 200), &delegate_2);

  EXPECT_CALL(delegate_2, OnImmersiveModeChanged(true)).Times(1);
  toplevel->HandleAuraToplevelConfigure(0, 0, 0, 0,
                                        {.is_maximized = false,
                                         .is_fullscreen = true,
                                         .is_immersive_fullscreen = true,
                                         .is_activated = true});
  toplevel->HandleSurfaceConfigure(2);
}

TEST_P(WaylandWindowTest, ImmersiveFullscreen_Disabled) {
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  uint32_t serial = 0;

  testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate_2;
  auto toplevel = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(10, 10, 200, 200), &delegate_2);

  // First we have to enable it, or the top level window will not detect
  // immersive state change.
  toplevel->HandleAuraToplevelConfigure(0, 0, 0, 0,
                                        {.is_maximized = false,
                                         .is_fullscreen = true,
                                         .is_immersive_fullscreen = true,
                                         .is_activated = true});
  toplevel->HandleSurfaceConfigure(++serial);

  EXPECT_CALL(delegate_2, OnImmersiveModeChanged(false)).Times(1);
  toplevel->HandleAuraToplevelConfigure(0, 0, 0, 0,
                                        {.is_maximized = false,
                                         .is_fullscreen = false,
                                         .is_immersive_fullscreen = false,
                                         .is_activated = true});
  toplevel->HandleSurfaceConfigure(++serial);
}

#endif

namespace {

class WaylandSubsurfaceTest : public WaylandWindowTest {
 public:
  WaylandSubsurfaceTest() = default;
  ~WaylandSubsurfaceTest() override = default;

 protected:
  void OneWaylandSubsurfaceTestHelper(
      const gfx::RectF& subsurface_bounds,
      const gfx::RectF& expected_subsurface_bounds) {
    VerifyAndClearExpectations();

    std::unique_ptr<WaylandWindow> window = CreateWaylandWindowWithParams(
        PlatformWindowType::kWindow, gfx::Rect(640, 480), &delegate_);
    EXPECT_TRUE(window);

    bool result = window->RequestSubsurface();
    EXPECT_TRUE(result);
    connection_->Flush();

    WaylandSubsurface* wayland_subsurface =
        window->wayland_subsurfaces().begin()->get();

    PostToServerAndWait(
        [subsurface_id =
             wayland_subsurface->wayland_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* mock_surface_subsurface =
              server->GetObject<wl::MockSurface>(subsurface_id);
          EXPECT_TRUE(mock_surface_subsurface);
        });
    wayland_subsurface->ConfigureAndShowSurface(
        subsurface_bounds, gfx::RectF(0, 0, 640, 480) /*parent_bounds_px*/,
        absl::nullopt /*clip_rect_px*/, gfx::OVERLAY_TRANSFORM_NONE,
        1.f /*buffer_scale*/, nullptr, nullptr);
    connection_->Flush();

    PostToServerAndWait(
        [surface_id = window->root_surface()->get_surface_id(),
         subsurface_id =
             wayland_subsurface->wayland_surface()->get_surface_id(),
         expected_subsurface_bounds](wl::TestWaylandServerThread* server) {
          auto* mock_surface_root_window =
              server->GetObject<wl::MockSurface>(surface_id);
          auto* mock_surface_subsurface =
              server->GetObject<wl::MockSurface>(subsurface_id);
          EXPECT_TRUE(mock_surface_subsurface);
          auto* test_subsurface = mock_surface_subsurface->sub_surface();
          EXPECT_TRUE(test_subsurface);
          auto* parent_resource = mock_surface_root_window->resource();
          EXPECT_EQ(parent_resource, test_subsurface->parent_resource());
          // The conversion from double to fixed and back is necessary because
          // it
          // happens during the roundtrip, and it creates significant error.
          gfx::PointF expected_position(wl_fixed_to_double(wl_fixed_from_double(
                                            expected_subsurface_bounds.x())),
                                        wl_fixed_to_double(wl_fixed_from_double(
                                            expected_subsurface_bounds.y())));
          EXPECT_EQ(test_subsurface->position(), expected_position);
          EXPECT_TRUE(test_subsurface->sync());
        });
  }

  std::vector<WaylandSubsurface*> RequestWaylandSubsurface(uint32_t n) {
    VerifyAndClearExpectations();
    std::vector<WaylandSubsurface*> res =
        std::vector<WaylandSubsurface*>{window_->primary_subsurface()};
    for (uint32_t i = 0; i < n - 1; ++i) {
      window_->RequestSubsurface();
    }
    for (auto& subsurface : window_->wayland_subsurfaces()) {
      res.push_back(subsurface.get());
    }
    return res;
  }
};

}  // namespace

// Tests integer and non integer size/position support with and without surface
// augmenter.
TEST_P(WaylandSubsurfaceTest, OneWaylandSubsurfaceInteger) {
  ASSERT_FALSE(connection_->surface_augmenter());

  constexpr gfx::RectF test_data[2][2] = {
      {gfx::RectF({15.12, 15.912}, {10.351, 10.742}),
       gfx::RectF({16, 16}, {11, 11})},
      {gfx::RectF({7.041, 8.583}, {13.452, 20.231}),
       gfx::RectF({7.041, 8.583}, {13.452, 20.231})}};

  for (const auto& item : test_data) {
    OneWaylandSubsurfaceTestHelper(item[0] /* subsurface_bounds */,
                                   item[1] /* expected_subsurface_bounds */);

    // Initialize the surface augmenter now.
    InitializeSurfaceAugmenter();
    ASSERT_TRUE(connection_->surface_augmenter());
  };
}

TEST_P(WaylandSubsurfaceTest, OneWaylandSubsurfaceNonInteger) {
  ASSERT_FALSE(connection_->surface_augmenter());

  constexpr gfx::RectF test_data[2][2] = {
      {gfx::RectF({15, 15}, {10, 10}), gfx::RectF({15, 15}, {10, 10})},
      {gfx::RectF({7, 8}, {16, 18}), gfx::RectF({7, 8}, {16, 18})}};

  for (const auto& item : test_data) {
    OneWaylandSubsurfaceTestHelper(item[0] /* subsurface_bounds */,
                                   item[1] /* expected_subsurface_bounds */);

    // Initialize the surface augmenter now.
    InitializeSurfaceAugmenter();
    ASSERT_TRUE(connection_->surface_augmenter());
  }
}

TEST_P(WaylandSubsurfaceTest, NoDuplicateSubsurfaceRequests) {
  auto subsurfaces = RequestWaylandSubsurface(3);
  for (auto* subsurface : subsurfaces) {
    subsurface->ConfigureAndShowSurface(
        gfx::RectF(1.f, 2.f, 10.f, 20.f), gfx::RectF(0.f, 0.f, 800.f, 600.f),
        absl::nullopt, gfx::OVERLAY_TRANSFORM_NONE, 1.f, nullptr, nullptr);
  }
  connection_->Flush();

  PostToServerAndWait(
      [subsurface_id1 = subsurfaces[0]->wayland_surface()->get_surface_id(),
       subsurface_id2 = subsurfaces[1]->wayland_surface()->get_surface_id(),
       subsurface_id3 = subsurfaces[2]->wayland_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        // From top to bottom: subsurfaces[2], subsurfaces[1], subsurfaces[0].
        wl::TestSubSurface* test_subs[3] = {
            server->GetObject<wl::MockSurface>(subsurface_id1)->sub_surface(),
            server->GetObject<wl::MockSurface>(subsurface_id2)->sub_surface(),
            server->GetObject<wl::MockSurface>(subsurface_id3)->sub_surface()};

        EXPECT_CALL(*test_subs[0], PlaceAbove(_)).Times(1);
        EXPECT_CALL(*test_subs[0], PlaceBelow(_)).Times(0);
        EXPECT_CALL(*test_subs[0], SetPosition(_, _)).Times(1);
        EXPECT_CALL(*test_subs[1], PlaceAbove(_)).Times(0);
        EXPECT_CALL(*test_subs[1], PlaceBelow(_)).Times(0);
        EXPECT_CALL(*test_subs[1], SetPosition(_, _)).Times(0);
        EXPECT_CALL(*test_subs[2], PlaceAbove(_)).Times(0);
        EXPECT_CALL(*test_subs[2], PlaceBelow(_)).Times(0);
        EXPECT_CALL(*test_subs[2], SetPosition(_, _)).Times(0);
      });

  // Stack subsurfaces[0] to be from bottom to top, and change its position.
  subsurfaces[0]->ConfigureAndShowSurface(
      gfx::RectF(0.f, 0.f, 10.f, 20.f), gfx::RectF(0.f, 0.f, 800.f, 600.f),
      absl::nullopt, gfx::OVERLAY_TRANSFORM_NONE, 1.f, subsurfaces[2], nullptr);
  subsurfaces[1]->ConfigureAndShowSurface(
      gfx::RectF(1.f, 2.f, 10.f, 20.f), gfx::RectF(0.f, 0.f, 800.f, 600.f),
      absl::nullopt, gfx::OVERLAY_TRANSFORM_NONE, 1.f, nullptr, subsurfaces[2]);
  subsurfaces[2]->ConfigureAndShowSurface(
      gfx::RectF(1.f, 2.f, 10.f, 20.f), gfx::RectF(0.f, 0.f, 800.f, 600.f),
      absl::nullopt, gfx::OVERLAY_TRANSFORM_NONE, 1.f, nullptr, subsurfaces[0]);
  connection_->Flush();

  VerifyAndClearExpectations();
}

TEST_P(WaylandWindowTest, NoDuplicateViewporterRequests) {
  EXPECT_TRUE(connection_->buffer_manager_host());

  auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  // Setup wl_buffers.
  constexpr uint32_t buffer_id = 1;
  gfx::Size buffer_size(1024, 768);
  auto length = 1024 * 768 * 4;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id);
  base::RunLoop().RunUntilIdle();

  auto* surface = window_->root_surface();
  PostToServerAndWait([surface_id = surface->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* test_viewport =
        server->GetObject<wl::MockSurface>(surface_id)->viewport();

    // Set viewport src and dst.
    EXPECT_CALL(*test_viewport, SetSource(512, 384, 512, 384)).Times(1);
    EXPECT_CALL(*test_viewport, SetDestination(800, 600)).Times(1);
  });

  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));

  surface->set_buffer_crop({0.5, 0.5, 0.5, 0.5});
  surface->set_viewport_destination({800, 600});
  surface->ApplyPendingState();
  surface->Commit();
  connection_->Flush();

  VerifyAndClearExpectations();

  PostToServerAndWait([surface_id = surface->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* test_viewport =
        server->GetObject<wl::MockSurface>(surface_id)->viewport();
    Mock::VerifyAndClearExpectations(test_viewport);
    // Duplicate viewport requests are not sent.
    EXPECT_CALL(*test_viewport, SetSource(_, _, _, _)).Times(0);
    EXPECT_CALL(*test_viewport, SetDestination(_, _)).Times(0);
  });

  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));

  surface->set_buffer_crop({0.5, 0.5, 0.5, 0.5});
  surface->set_viewport_destination({800, 600});
  surface->ApplyPendingState();
  surface->Commit();
  connection_->Flush();

  VerifyAndClearExpectations();

  PostToServerAndWait([surface_id = surface->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* test_viewport =
        server->GetObject<wl::MockSurface>(surface_id)->viewport();
    Mock::VerifyAndClearExpectations(test_viewport);
    // Unset viewport src and dst.
    EXPECT_CALL(*test_viewport, SetSource(-1, -1, -1, -1)).Times(1);
    EXPECT_CALL(*test_viewport, SetDestination(-1, -1)).Times(1);
  });

  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));

  surface->set_buffer_crop({0., 0., 1., 1.});
  surface->set_viewport_destination({1024, 768});
  surface->ApplyPendingState();
  surface->Commit();
  connection_->Flush();

  VerifyAndClearExpectations();

  PostToServerAndWait([surface_id = surface->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* test_viewport =
        server->GetObject<wl::MockSurface>(surface_id)->viewport();
    Mock::VerifyAndClearExpectations(test_viewport);
    // Duplicate viewport requests are not sent.
    EXPECT_CALL(*test_viewport, SetSource(_, _, _, _)).Times(0);
    EXPECT_CALL(*test_viewport, SetDestination(_, _)).Times(0);
  });

  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));

  surface->set_buffer_crop({0., 0., 1., 1.});
  surface->set_viewport_destination({1024, 768});
  surface->ApplyPendingState();
  surface->Commit();
  connection_->Flush();

  VerifyAndClearExpectations();
}

// Tests that WaylandPopups can be repositioned.
TEST_P(WaylandWindowTest, RepositionPopups) {
  VerifyAndClearExpectations();

  EXPECT_CALL(delegate_, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  gfx::Rect menu_window_bounds(gfx::Rect(6, 20, 8, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_bounds, &delegate_,
      window_->GetWidget());
  EXPECT_TRUE(menu_window);
  EXPECT_TRUE(menu_window->IsVisible());

  PostToServerAndWait(
      [id = menu_window->root_surface()->get_surface_id(),
       menu_window_bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface_popup = server->GetObject<wl::MockSurface>(id);
        auto* mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

        EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
                  menu_window_bounds.origin());
        EXPECT_EQ(mock_xdg_popup->size(), menu_window_bounds.size());
        EXPECT_EQ(mock_surface_popup->opaque_region(),
                  gfx::Rect(menu_window_bounds.size()));
      });

  VerifyAndClearExpectations();

  const gfx::Rect damage_rect{menu_window_bounds.width(),
                              menu_window_bounds.height()};
  EXPECT_CALL(delegate_, OnDamageRect(Eq(damage_rect))).Times(1);
  menu_window_bounds.set_origin({10, 10});
  menu_window->SetBoundsInDIP(menu_window_bounds);

  PostToServerAndWait(
      [id = menu_window->root_surface()->get_surface_id(),
       menu_window_bounds](wl::TestWaylandServerThread* server) {
        // Xdg objects can be recreated depending on the version of the xdg
        // shell.
        auto* mock_surface_popup = server->GetObject<wl::MockSurface>(id);
        auto* mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

        EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
                  menu_window_bounds.origin());
        EXPECT_EQ(mock_xdg_popup->size(), menu_window_bounds.size());
        EXPECT_EQ(mock_surface_popup->opaque_region(),
                  gfx::Rect(menu_window_bounds.size()));
      });

  // This will send a configure event for the xdg_surface that backs the
  // xdg_popup. Size and state are not used there.
  SendConfigureEvent(menu_window->root_surface()->get_surface_id(), {0, 0},
                     wl::ScopedWlArray({}));

  VerifyAndClearExpectations();
}

// If buffers are not attached (aka WaylandBufferManagerHost is not used for
// buffer management), WaylandSurface::Commit mustn't result in creation of
// surface sync.
TEST_P(WaylandWindowTest, DoesNotCreateSurfaceSyncOnCommitWithoutBuffers) {
  EXPECT_THAT(window_->root_surface()->surface_sync_, nullptr);
  window_->root_surface()->Commit();
  EXPECT_THAT(window_->root_surface()->surface_sync_, nullptr);
}

TEST_P(WaylandWindowTest, StartWithMinimized) {
  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, {0, 0}, states);

  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Minimize();
  // The state of the window has to be already minimized.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // We don't receive any state change if that does not differ from the last
  // state.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(0);
  // It must be still the same minimized state.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);
  ui::PlatformWindowState state;
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&state), InvokeWithoutArgs([&]() {
                              EXPECT_EQ(state, PlatformWindowState::kMinimized);
                            })));
  // The window geometry has to be set to the current bounds of the window for
  // minimized state.
  EXPECT_EQ(gfx::Rect(800, 600), window_->GetBoundsInDIP());
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(id);
    auto* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
  });
  // Send one additional empty configuration event for minimized state.
  // (which means the surface is not maximized, fullscreen or activated)
  states = wl::ScopedWlArray({});
  SendConfigureEvent(surface_id_, {0, 0}, states);
}

class BlockableWaylandToplevelWindow : public WaylandToplevelWindow {
 public:
  BlockableWaylandToplevelWindow(MockWaylandPlatformWindowDelegate* delegate,
                                 WaylandConnection* connection)
      : WaylandToplevelWindow(delegate, connection) {}

  static std::unique_ptr<BlockableWaylandToplevelWindow> Create(
      const gfx::Rect& bounds,
      WaylandConnection* connection,
      MockWaylandPlatformWindowDelegate* delegate) {
    auto window =
        std::make_unique<BlockableWaylandToplevelWindow>(delegate, connection);

    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = PlatformWindowType::kWindow;
    properties.parent_widget = gfx::kNullAcceleratedWidget;
    window->Initialize(std::move(properties));
    window->Show(false);
    return window;
  }

  // WaylandToplevelWindow overrides:
  uint32_t DispatchEvent(const PlatformEvent& platform_event) override {
    ui::Event* event(platform_event);
    if (event->type() == ET_TOUCH_RELEASED && !blocked_) {
      base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
      blocked_ = true;

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(async_task_), run_loop.QuitClosure()));
      run_loop.Run();
      blocked_ = false;
    }

    return WaylandToplevelWindow::DispatchEvent(platform_event);
  }

  void SetAsyncTask(
      base::RepeatingCallback<void(base::OnceClosure)> async_task) {
    async_task_ = std::move(async_task);
  }

 private:
  bool blocked_ = false;
  base::RepeatingCallback<void(base::OnceClosure)> async_task_;
};

// This test ensures that Ozone/Wayland does not crash while handling a
// sequence of two or more touch down/up actions, where the first one blocks
// unfinished before the second pair comes in.
//
// This mimics the behavior of a modal dialog that comes up as a result of
// the first touch down/up action, and blocks the original flow, before it gets
// handled completely.
// The test is flaky. https://crbug.com/1305272.
TEST_P(WaylandWindowTest, DISABLED_BlockingTouchDownUp_NoCrash) {
  window_.reset();

  MockWaylandPlatformWindowDelegate delegate;
  auto window = BlockableWaylandToplevelWindow::Create(
      gfx::Rect(800, 600), connection_.get(), &delegate);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(
        server->seat()->resource(),
        WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH);
  });
  ASSERT_TRUE(connection_->seat()->pointer());
  ASSERT_TRUE(connection_->seat()->touch());
  window->set_touch_focus(true);

  // Test that CanDispatchEvent is set correctly.
  VerifyCanDispatchTouchEvents({window.get()}, {});

  // Steps to be executed after the handling of the first touch down/up
  // pair blocks.
  auto async_task = base::BindLambdaForTesting([&](base::OnceClosure closure) {
    PostToServerAndWait([id = window->root_surface()->get_surface_id()](
                            wl::TestWaylandServerThread* server) {
      wl::MockSurface* toplevel_surface =
          server->GetObject<wl::MockSurface>(id);
      wl_touch_send_down(server->seat()->touch()->resource(),
                         server->GetNextSerial(), 0,
                         toplevel_surface->resource(), 0 /* id */,
                         wl_fixed_from_int(100), wl_fixed_from_int(100));
      wl_touch_send_up(server->seat()->touch()->resource(),
                       server->GetNextSerial(), 2000, 0 /* id */);
      wl_touch_send_frame(server->seat()->touch()->resource());
    });

    std::move(closure).Run();
  });
  window->SetAsyncTask(std::move(async_task));

  PostToServerAndWait([id = window->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    wl::MockSurface* toplevel_surface = server->GetObject<wl::MockSurface>(id);
    // Start executing the first touch down/up pair.
    wl_touch_send_down(server->seat()->touch()->resource(),
                       server->GetNextSerial(), 0, toplevel_surface->resource(),
                       0 /* id */, wl_fixed_from_int(50),
                       wl_fixed_from_int(50));
    wl_touch_send_up(server->seat()->touch()->resource(),
                     server->GetNextSerial(), 1000, 0 /* id */);
    wl_touch_send_frame(server->seat()->touch()->resource());
  });
}

// Make sure that changing focus during dispatch will not re-dispatch the event
// to the newly focused window. (crbug.com/1339082);
// Flaky on device/VM: https://crbug.com/1348046
TEST_P(WaylandWindowTest, ChangeFocusDuringDispatch) {
  MockPlatformWindowDelegate other_delegate;
  gfx::AcceleratedWidget other_widget = gfx::kNullAcceleratedWidget;
  EXPECT_CALL(other_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&other_widget));

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(10, 10);
  properties.type = PlatformWindowType::kWindow;
  auto other_window = WaylandWindow::Create(&other_delegate, connection_.get(),
                                            std::move(properties));
  ASSERT_NE(other_widget, gfx::kNullAcceleratedWidget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });

  ASSERT_TRUE(connection_->seat()->pointer());

  int count = 0;
  EXPECT_CALL(other_delegate, DispatchEvent(_)).Times(1);
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    count++;
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      PostToServerAndWait(
          [id = surface_id_,
           other_id = other_window->root_surface()->get_surface_id()](
              wl::TestWaylandServerThread* server) {
            wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
            ASSERT_TRUE(surface);
            wl::MockSurface* other_surface =
                server->GetObject<wl::MockSurface>(other_id);
            ASSERT_TRUE(other_surface);
            auto* pointer = server->seat()->pointer();
            wl_pointer_send_leave(pointer->resource(), 3, surface->resource());
            wl_pointer_send_frame(pointer->resource());

            wl_pointer_send_enter(pointer->resource(), 4,
                                  other_surface->resource(), 0, 0);
            wl_pointer_send_frame(pointer->resource());
          });
    }
  });

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(surface);
    auto* pointer = server->seat()->pointer();

    wl_pointer_send_enter(pointer->resource(), 1, surface->resource(), 0, 0);
    // The Enter event is coupled with the frame event.
    wl_pointer_send_frame(pointer->resource());
    wl_pointer_send_button(pointer->resource(), 2, 1004, BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer->resource());
  });

  EXPECT_EQ(count, 3);
}

TEST_P(WaylandWindowTest, WindowMovedResized) {
  const gfx::Rect initial_bounds = window_->GetBoundsInDIP();

  gfx::Rect new_bounds(initial_bounds);
  new_bounds.set_x(new_bounds.origin().x() + 10);
  new_bounds.set_y(new_bounds.origin().y() + 10);
  // Configure is not necessary to just move.
  EXPECT_CALL(delegate_, OnBoundsChanged(BoundsChange(true)));
  PostToServerAndWait([id = surface_id_,
                       new_bounds](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    auto* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(new_bounds.size())))
        .Times(0);
  });
  window_->SetBoundsInDIP(new_bounds);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // Resize and move.
  new_bounds.Inset(5);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(BoundsChange(true)))).Times(1);
  PostToServerAndWait([id = surface_id_,
                       new_bounds](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    auto* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(new_bounds.size())))
        .Times(0);
  });
  window_->SetBoundsInDIP(new_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(BoundsChange(true)))).Times(1);
  PostToServerAndWait([id = surface_id_,
                       new_bounds](wl::TestWaylandServerThread* server) {
    wl::MockSurface* surface = server->GetObject<wl::MockSurface>(id);
    auto* xdg_surface = surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(gfx::Rect(new_bounds.size())))
        .Times(1);
  });
  wl::ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(surface_id_, new_bounds.size(), states);
  AdvanceFrameToCurrent(window_.get(), delegate_);
}

// Make sure that creating a window with DIP bounds creates a window with
// the same DIP bounds with various fractional scales.
TEST_P(WaylandWindowTest, NoRoundingErrorInDIP) {
  VerifyAndClearExpectations();
  auto* primary_output =
      connection_->wayland_output_manager()->GetPrimaryOutput();
  constexpr float kScales[] = {display::kDsf_1_777, display::kDsf_2_252,
                               display::kDsf_2_666, display::kDsf_1_8};
  for (float scale : kScales) {
    primary_output->SetScaleFactorForTesting(scale);
    // Update to delegate to use the correct scale;
    window_->UpdateWindowScale(true);

    testing::NiceMock<MockWaylandPlatformWindowDelegate> delegate;
    std::unique_ptr<WaylandWindow> wayland_window =
        CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                      gfx::Rect(20, 0, 100, 100), &delegate);
    for (int i = 100; i < 3000; i++) {
      const gfx::Rect kBoundsDip{20, 0, i, 3000 - i};
      const gfx::Rect bounds_in_px = delegate_.ConvertRectToPixels(kBoundsDip);
      wayland_window->SetBoundsInDIP(kBoundsDip);
      AdvanceFrameToCurrent(wayland_window.get(), delegate);
      EXPECT_EQ(bounds_in_px.size(), wayland_window->applied_state().size_px);
      EXPECT_EQ(kBoundsDip, wayland_window->GetBoundsInDIP());
    }
  }
  VerifyAndClearExpectations();
}

// Make sure that the window scale change is applied on the latest
// in_flight_requests.
TEST_P(WaylandWindowTest, ScaleChangeWhenStateRequestThrottoled) {
  VerifyAndClearExpectations();

  gfx::Rect bounds_dip;
  auto* toplevel = static_cast<WaylandToplevelWindow*>(window_.get());
  for (int i = 300; i <= 600; i++) {
    bounds_dip = {0, 0, i, 900 - i};
    toplevel->HandleToplevelConfigure(bounds_dip.width(), bounds_dip.height(),
                                      {});
    toplevel->HandleSurfaceConfigure(i);
  }
  // latest bounds_dip is throttled, and not applied, scale factor is 1.
  EXPECT_NE(bounds_dip.size(), toplevel->applied_state().bounds_dip.size());
  EXPECT_EQ(bounds_dip.size(),
            delegate_.ConvertRectToPixels(bounds_dip).size());

  // Update to delegate to use the correct scale;
  constexpr float kScale = display::kDsf_1_777;
  auto* primary_output =
      connection_->wayland_output_manager()->GetPrimaryOutput();
  primary_output->SetScaleFactorForTesting(kScale);
  toplevel->UpdateWindowScale(true);
  AdvanceFrameToCurrent(window_.get(), delegate_);

  // bounds_dip advances to be applied, and scaled correctly.
  EXPECT_EQ(bounds_dip.size(), toplevel->applied_state().bounds_dip.size());
  EXPECT_NE(bounds_dip.size(),
            delegate_.ConvertRectToPixels(bounds_dip).size());
  EXPECT_EQ(window_->applied_state().size_px,
            delegate_.ConvertRectToPixels(bounds_dip).size());
  VerifyAndClearExpectations();
}

// Asserts the server receives the correct region when SetShape() is called for
// toplevel windows.
TEST_P(WaylandWindowTest, SetShape) {
  // SetShape() is only supported with zaura_shell.
  if (GetParam().enable_aura_shell != wl::EnableAuraShellProtocol::kEnabled) {
    GTEST_SKIP();
  }

  // Define a custom window shape and generate the corresponding region.
  const PlatformWindow::ShapeRects shape_rects = {{10, 10, 40, 40},
                                                  {20, 20, 50, 50}};
  wl::TestRegion shape_region;
  for (const auto& rect : shape_rects) {
    shape_region.op(
        SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        SkRegion::kUnion_Op);
  }

  // Set the toplevel window shape.
  window_->SetShape(std::make_unique<PlatformWindow::ShapeRects>(shape_rects),
                    {});

  // Validate the server has received the appropriate region for the toplevel.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_EQ(shape_region, zaura_toplevel->shape());
  });

  // Unset the toplevel window shape.
  window_->SetShape(nullptr, {});

  // Validate the server has received and unset the window shape.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_FALSE(zaura_toplevel->shape().has_value());
  });
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Asserts the server receives the correct height when `SetTopInset()` is called
// for toplevel windows.
TEST_P(WaylandWindowTest, SetTopInset) {
  // `SetTopInset()` is only supported with zaura_shell.
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  MockWaylandPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> toplevel_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                    gfx::Rect(300, 300), &delegate);

  toplevel_window->AsWaylandToplevelWindow()->SetTopInset(32);

  // Validate the server has received the appropriate top inset for the
  // toplevel.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(
        toplevel_window->root_surface()->get_surface_id());
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_EQ(32, zaura_toplevel->top_inset());
  });

  toplevel_window->AsWaylandToplevelWindow()->SetTopInset(0);

  // Validate the server has received the appropriate top inset for the
  // toplevel.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(
        toplevel_window->root_surface()->get_surface_id());
    ASSERT_TRUE(surface);

    wl::TestZAuraToplevel* zaura_toplevel =
        surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    ASSERT_TRUE(zaura_toplevel);
    EXPECT_EQ(0, zaura_toplevel->top_inset());
  });
}

// Tests that the platform window gets the notification when overview mode
// changes.
TEST_P(WaylandWindowTest, OverviewMode) {
  // Only supported with zaura_shell.
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  EXPECT_CALL(delegate_, OnOverviewModeChanged(Eq(true))).Times(1);
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_overview_change(toplevel->resource(),
                                        ZAURA_TOPLEVEL_IN_OVERVIEW_IN_OVERVIEW);
  });

  EXPECT_CALL(delegate_, OnOverviewModeChanged(Eq(false))).Times(1);
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id_);
    auto* toplevel = surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
    zaura_toplevel_send_overview_change(
        toplevel->resource(), ZAURA_TOPLEVEL_IN_OVERVIEW_NOT_IN_OVERVIEW);
  });
}
#endif

// Tests setting and unsetting float state on a wayland toplevel window.
TEST_P(WaylandWindowTest, SetUnsetFloat) {
  if (!IsAuraShellEnabled()) {
    GTEST_SKIP();
  }

  auto post_to_server_and_wait = [&]() {
    base::RunLoop run_loop;
    PostToServerAndWait(run_loop.QuitClosure());
    run_loop.Run();
  };

  // Sets up a callback to verify server function calls.
  base::MockRepeatingCallback<void(bool, uint32_t)> set_unset_float_cb;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(surface_id_)
        ->xdg_surface()
        ->xdg_toplevel()
        ->zaura_toplevel()
        ->set_set_unset_float_callback(set_unset_float_cb.Get());
  });

  window_->AsWaylandToplevelWindow()->SetFloatToLocation(
      ui::WaylandFloatStartLocation::kBottomRight);
  EXPECT_CALL(set_unset_float_cb, Run(/*floated=*/true, 0));
  post_to_server_and_wait();

  window_->AsWaylandToplevelWindow()->SetFloatToLocation(
      ui::WaylandFloatStartLocation::kBottomLeft);
  EXPECT_CALL(set_unset_float_cb, Run(/*floated=*/true, 1));
  post_to_server_and_wait();

  window_->AsWaylandToplevelWindow()->UnSetFloat();
  EXPECT_CALL(set_unset_float_cb, Run(/*floated=*/false, _));
  post_to_server_and_wait();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowTest,
                         Values(wl::ServerConfig{}));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandWindowTest,
    Values(wl::ServerConfig{.enable_aura_shell =
                                wl::EnableAuraShellProtocol::kEnabled},
           wl::ServerConfig{
               .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled,
               .use_aura_output_manager = true}));
#endif

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSubsurfaceTest,
                         Values(wl::ServerConfig{}));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandSubsurfaceTest,
    Values(wl::ServerConfig{.enable_aura_shell =
                                wl::EnableAuraShellProtocol::kEnabled},
           wl::ServerConfig{
               .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled,
               .use_aura_output_manager = true}));
#endif

}  // namespace ui
