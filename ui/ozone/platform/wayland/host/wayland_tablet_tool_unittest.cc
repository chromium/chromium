// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_tablet_tool.h"

#include <cursor-shape-v1-server-protocol.h>
#include <tablet-unstable-v2-server-protocol.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_async_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_seat.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_tablet.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::NotNull;

namespace ui {

namespace {

wl::TestZwpTabletSeat* GetTabletSeat(wl::TestWaylandServerThread* server) {
  return server->zwp_tablet_manager()->tablet_seat();
}

wl::MockZwpTabletTool* GetTool(wl::TestWaylandServerThread* server) {
  return GetTabletSeat(server)->tool();
}

wl::MockWpCursorShapeDevice* GetShapeDevice(
    wl::TestWaylandServerThread* server) {
  return GetTool(server)->cursor_shape_device();
}

// Returns a cursor of `type` with a 10x10 bitmap at `scale` and its hotspot at
// (2, 3) in DIPs.
scoped_refptr<BitmapCursor> CreateBitmapCursor(mojom::CursorType type,
                                               int scale = 1) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10 * scale, 10 * scale);
  bitmap.eraseColor(SK_ColorRED);
  return base::MakeRefCounted<BitmapCursor>(
      type, bitmap, gfx::Point(2 * scale, 3 * scale), scale);
}

}  // namespace

class WaylandTabletToolTest : public WaylandTestSimple {
 public:
  explicit WaylandTabletToolTest(bool supports_cursor_shape = true)
      : WaylandTestSimple({.supports_tablet = true,
                           .supports_cursor_shape = supports_cursor_shape}),
        supports_cursor_shape_(supports_cursor_shape) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    // The tablet seat is created when the seat announces its capabilities.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_POINTER);
    });
    ASSERT_TRUE(connection_->seat()->tablet_seat());
    ASSERT_EQ(!!connection_->wayland_cursor_shape(), supports_cursor_shape_);

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* tablet_seat = GetTabletSeat(server);
      ASSERT_TRUE(tablet_seat);
      tablet_seat->AddTablet();
      tablet_seat->AddTool();
    });
  }

 protected:
  // Sends proximity_in for the root surface of `window_`. `expectations` runs
  // on the server thread just before, with the event's serial, so that it can
  // set expectations for the client's response.
  void SendProximityIn(
      base::OnceCallback<void(wl::TestWaylandServerThread*, uint32_t)>
          expectations = base::DoNothing()) {
    PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id(),
                         expectations = std::move(expectations)](
                            wl::TestWaylandServerThread* server) mutable {
      auto* tablet_seat = GetTabletSeat(server);
      auto* surface =
          server->GetObject<wl::MockSurface>(surface_id)->resource();
      const uint32_t serial = server->GetNextSerial();
      std::move(expectations).Run(server, serial);
      zwp_tablet_tool_v2_send_proximity_in(
          tablet_seat->tool()->resource(), serial,
          tablet_seat->tablet()->resource(), surface);
      zwp_tablet_tool_v2_send_frame(tablet_seat->tool()->resource(),
                                    server->GetNextTime());
    });
  }

  void SendProximityOut() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      zwp_tablet_tool_v2_send_proximity_out(GetTool(server)->resource());
      zwp_tablet_tool_v2_send_frame(GetTool(server)->resource(),
                                    server->GetNextTime());
    });
  }

  void SetCursor(scoped_refptr<BitmapCursor> cursor) {
    window_->SetCursor(base::MakeRefCounted<WaylandAsyncCursor>(cursor));
  }

  void SetCursor(mojom::CursorType type) {
    SetCursor(base::MakeRefCounted<BitmapCursor>(type));
  }

  // Waits for the client's requests and verifies the expectations of the
  // tool and its shape device.
  void VerifyAndClearExpectations() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      ::testing::Mock::VerifyAndClearExpectations(GetTool(server));
      if (auto* shape_device = GetShapeDevice(server)) {
        ::testing::Mock::VerifyAndClearExpectations(shape_device);
      }
    });
  }

  // Gives `window_` pointer focus, so that it applies cursors as they load.
  void SendPointerEnter() {
    PostToServerAndWait([surface_id =
                             window_->root_surface()->get_surface_id()](
                            wl::TestWaylandServerThread* server) {
      auto* surface =
          server->GetObject<wl::MockSurface>(surface_id)->resource();
      auto* pointer = server->seat()->pointer()->resource();
      wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
      wl_pointer_send_frame(pointer);
    });
    ASSERT_TRUE(window_->HasPointerFocus());
  }

 private:
  const bool supports_cursor_shape_;
};

// For compositors without wp_cursor_shape_manager_v1, e.g. GNOME 46.
class WaylandTabletToolNoCursorShapeTest : public WaylandTabletToolTest {
 public:
  WaylandTabletToolNoCursorShapeTest()
      : WaylandTabletToolTest(/*supports_cursor_shape=*/false) {}
};

// Regression test for crbug.com/447172706: "cursor: none" must hide the cursor
// of a tablet tool, not only the one of wl_pointer.
TEST_F(WaylandTabletToolTest, SetCursorNoneHidesToolCursor) {
  uint32_t proximity_serial = 0;
  SendProximityIn(base::BindLambdaForTesting(
      [&](wl::TestWaylandServerThread*, uint32_t serial) {
        proximity_serial = serial;
      }));

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, nullptr, 0, 0));
  });
  SetCursor(mojom::CursorType::kNone);
  VerifyAndClearExpectations();
}

TEST_F(WaylandTabletToolTest, SetCursorUsesCursorShape) {
  uint32_t proximity_serial = 0;
  SendProximityIn(base::BindLambdaForTesting(
      [&](wl::TestWaylandServerThread*, uint32_t serial) {
        proximity_serial = serial;
      }));

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    ASSERT_TRUE(GetShapeDevice(server));
    EXPECT_CALL(
        *GetShapeDevice(server),
        SetShape(proximity_serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR));
  });
  SetCursor(mojom::CursorType::kCross);
  VerifyAndClearExpectations();

  // Showing a cursor again after hiding it uses the shape too.
  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, nullptr, 0, 0));
  });
  SetCursor(mojom::CursorType::kNone);
  VerifyAndClearExpectations();

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(
        *GetShapeDevice(server),
        SetShape(proximity_serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER));
  });
  SetCursor(mojom::CursorType::kHand);
  VerifyAndClearExpectations();
}

// The tool's cursor follows the window's cursor even when wl_pointer leaves
// the window while the tool stays in proximity.
TEST_F(WaylandTabletToolTest, SetCursorWithoutPointerFocus) {
  uint32_t proximity_serial = 0;
  SendProximityIn(base::BindLambdaForTesting(
      [&](wl::TestWaylandServerThread*, uint32_t serial) {
        proximity_serial = serial;
      }));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* pointer = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
    wl_pointer_send_frame(pointer);
  });
  ASSERT_FALSE(window_->HasPointerFocus());

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, nullptr, 0, 0));
  });
  SetCursor(mojom::CursorType::kNone);
  VerifyAndClearExpectations();
}

// Cursors without a shape are drawn on a surface of the tool's own.
TEST_F(WaylandTabletToolTest, SetCursorUsesBitmapForCustomCursors) {
  uint32_t proximity_serial = 0;
  SendProximityIn(base::BindLambdaForTesting(
      [&](wl::TestWaylandServerThread*, uint32_t serial) {
        proximity_serial = serial;
      }));

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, NotNull(), 2, 3));
    EXPECT_CALL(*GetShapeDevice(server), SetShape(_, _)).Times(0);
  });
  SetCursor(CreateBitmapCursor(mojom::CursorType::kCustom));
  VerifyAndClearExpectations();
}

// A tool entering proximity takes on the window's current cursor, even if the
// window had pointer focus already and so doesn't update its cursor.
TEST_F(WaylandTabletToolTest, ProximityInAppliesWindowCursor) {
  // The shape device is created on the first proximity_in.
  SendProximityIn();
  SendProximityOut();

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* pointer = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
  });
  ASSERT_TRUE(window_->HasPointerFocus());
  SetCursor(mojom::CursorType::kNone);

  SendProximityIn(
      base::BindOnce([](wl::TestWaylandServerThread* server, uint32_t serial) {
        EXPECT_CALL(*GetTool(server), SetCursor(serial, nullptr, 0, 0));
        EXPECT_CALL(*GetShapeDevice(server), SetShape(_, _)).Times(0);
      }));
  VerifyAndClearExpectations();
}

// Without a window cursor, a tool entering proximity gets the default one.
TEST_F(WaylandTabletToolTest, ProximityInWithoutWindowCursorUsesDefault) {
  // The shape device is created on the first proximity_in, so the expectation
  // can only be checked on the second one.
  SendProximityIn();
  SendProximityOut();

  SendProximityIn(
      base::BindOnce([](wl::TestWaylandServerThread* server, uint32_t serial) {
        EXPECT_CALL(*GetShapeDevice(server),
                    SetShape(serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT));
      }));
  VerifyAndClearExpectations();
}

TEST_F(WaylandTabletToolTest, NoCursorUpdatesAfterProximityOut) {
  SendProximityIn();
  SendProximityOut();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(_, _, _, _)).Times(0);
    EXPECT_CALL(*GetShapeDevice(server), SetShape(_, _)).Times(0);
  });
  SetCursor(mojom::CursorType::kNone);
  SetCursor(mojom::CursorType::kCross);
  VerifyAndClearExpectations();
}

// Regression test for crbug.com/403365464: without the cursor shape protocol,
// a tool entering proximity must still get a cursor.
TEST_F(WaylandTabletToolNoCursorShapeTest, ProximityInAppliesWindowCursor) {
  SendPointerEnter();
  SetCursor(CreateBitmapCursor(mojom::CursorType::kPointer));

  SendProximityIn(
      base::BindOnce([](wl::TestWaylandServerThread* server, uint32_t serial) {
        EXPECT_CALL(*GetTool(server), SetCursor(serial, NotNull(), 2, 3));
      }));
  VerifyAndClearExpectations();
}

TEST_F(WaylandTabletToolNoCursorShapeTest, SetCursorUsesBitmap) {
  uint32_t proximity_serial = 0;
  SendProximityIn(base::BindLambdaForTesting(
      [&](wl::TestWaylandServerThread*, uint32_t serial) {
        proximity_serial = serial;
      }));

  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, NotNull(), 2, 3));
  });
  SetCursor(CreateBitmapCursor(mojom::CursorType::kCross));
  VerifyAndClearExpectations();

  // The cursor can be hidden and shown again.
  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, nullptr, 0, 0));
  });
  SetCursor(mojom::CursorType::kNone);
  VerifyAndClearExpectations();

  // The hotspot is in DIPs.
  PostToServerAndWait([proximity_serial](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*GetTool(server), SetCursor(proximity_serial, NotNull(), 2, 3));
  });
  SetCursor(CreateBitmapCursor(mojom::CursorType::kHand, /*scale=*/2));
  VerifyAndClearExpectations();
}

// Without a window cursor, the compositor's cursor is left as is.
TEST_F(WaylandTabletToolNoCursorShapeTest, ProximityInWithoutWindowCursor) {
  SendProximityIn(
      base::BindOnce([](wl::TestWaylandServerThread* server, uint32_t) {
        EXPECT_CALL(*GetTool(server), SetCursor(_, _, _, _)).Times(0);
      }));
  VerifyAndClearExpectations();
}

}  // namespace ui
