// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TABLET_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TABLET_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

class MockWpCursorShapeDevice;

// Manages zwp_tablet_v2 objects.
class TestZwpTablet : public ServerObject {
 public:
  explicit TestZwpTablet(wl_resource* resource);
  TestZwpTablet(const TestZwpTablet&) = delete;
  TestZwpTablet& operator=(const TestZwpTablet&) = delete;
  ~TestZwpTablet() override;
};

// Manages zwp_tablet_tool_v2 objects.
class MockZwpTabletTool : public ServerObject {
 public:
  explicit MockZwpTabletTool(wl_resource* resource);
  MockZwpTabletTool(const MockZwpTabletTool&) = delete;
  MockZwpTabletTool& operator=(const MockZwpTabletTool&) = delete;
  ~MockZwpTabletTool() override;

  MOCK_METHOD(void,
              SetCursor,
              (uint32_t serial,
               wl_resource* surface,
               int32_t hotspot_x,
               int32_t hotspot_y));

  // The wp_cursor_shape_device_v1 the client created for this tool, if any.
  MockWpCursorShapeDevice* cursor_shape_device() const {
    return cursor_shape_device_;
  }
  void set_cursor_shape_device(MockWpCursorShapeDevice* device) {
    cursor_shape_device_ = device;
  }

  base::WeakPtr<MockZwpTabletTool> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<MockWpCursorShapeDevice> cursor_shape_device_ = nullptr;
  base::WeakPtrFactory<MockZwpTabletTool> weak_ptr_factory_{this};
};

// Manages zwp_tablet_seat_v2 objects.
class TestZwpTabletSeat : public ServerObject {
 public:
  explicit TestZwpTabletSeat(wl_resource* resource);
  TestZwpTabletSeat(const TestZwpTabletSeat&) = delete;
  TestZwpTabletSeat& operator=(const TestZwpTabletSeat&) = delete;
  ~TestZwpTabletSeat() override;

  // Announce a new tablet or tool to the client.
  TestZwpTablet* AddTablet();
  MockZwpTabletTool* AddTool();

  // The last added tablet and tool, while they exist.
  TestZwpTablet* tablet() const { return tablet_; }
  MockZwpTabletTool* tool() const { return tool_; }

 private:
  void set_tablet(TestZwpTablet* tablet) { tablet_ = tablet; }
  void set_tool(MockZwpTabletTool* tool) { tool_ = tool; }

  raw_ptr<TestZwpTablet> tablet_ = nullptr;
  raw_ptr<MockZwpTabletTool> tool_ = nullptr;
  base::WeakPtrFactory<TestZwpTabletSeat> weak_ptr_factory_{this};
};

// Manages the zwp_tablet_manager_v2 global.
class TestZwpTabletManager : public GlobalObject {
 public:
  TestZwpTabletManager();
  TestZwpTabletManager(const TestZwpTabletManager&) = delete;
  TestZwpTabletManager& operator=(const TestZwpTabletManager&) = delete;
  ~TestZwpTabletManager() override;

  TestZwpTabletSeat* tablet_seat() const { return tablet_seat_; }
  void set_tablet_seat(TestZwpTabletSeat* tablet_seat) {
    tablet_seat_ = tablet_seat;
  }

  base::WeakPtr<TestZwpTabletManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static void GetTabletSeat(wl_client* client,
                            wl_resource* resource,
                            uint32_t id,
                            wl_resource* seat);

 private:
  raw_ptr<TestZwpTabletSeat> tablet_seat_ = nullptr;
  base::WeakPtrFactory<TestZwpTabletManager> weak_ptr_factory_{this};
};

// Manages wp_cursor_shape_device_v1 objects.
class MockWpCursorShapeDevice : public ServerObject {
 public:
  explicit MockWpCursorShapeDevice(wl_resource* resource);
  MockWpCursorShapeDevice(const MockWpCursorShapeDevice&) = delete;
  MockWpCursorShapeDevice& operator=(const MockWpCursorShapeDevice&) = delete;
  ~MockWpCursorShapeDevice() override;

  MOCK_METHOD(void, SetShape, (uint32_t serial, uint32_t shape));
};

// Manages the wp_cursor_shape_manager_v1 global.
class TestWpCursorShapeManager : public GlobalObject {
 public:
  TestWpCursorShapeManager();
  TestWpCursorShapeManager(const TestWpCursorShapeManager&) = delete;
  TestWpCursorShapeManager& operator=(const TestWpCursorShapeManager&) = delete;
  ~TestWpCursorShapeManager() override;

  static void GetPointer(wl_client* client,
                         wl_resource* resource,
                         uint32_t id,
                         wl_resource* pointer);
  static void GetTabletTool(wl_client* client,
                            wl_resource* resource,
                            uint32_t id,
                            wl_resource* tablet_tool);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TABLET_H_
