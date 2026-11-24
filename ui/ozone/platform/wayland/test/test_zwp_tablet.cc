// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_tablet.h"

#include <cursor-shape-v1-server-protocol.h>
#include <tablet-unstable-v2-server-protocol.h>

#include "base/functional/bind.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"

namespace wl {

namespace {

constexpr uint32_t kTabletManagerVersion = 1;
constexpr uint32_t kCursorShapeManagerVersion = 1;

using NiceMockZwpTabletTool = ::testing::NiceMock<MockZwpTabletTool>;
using NiceMockWpCursorShapeDevice =
    ::testing::NiceMock<MockWpCursorShapeDevice>;

void SetToolCursor(wl_client* client,
                   wl_resource* resource,
                   uint32_t serial,
                   wl_resource* surface,
                   int32_t hotspot_x,
                   int32_t hotspot_y) {
  GetUserDataAs<MockZwpTabletTool>(resource)->SetCursor(serial, surface,
                                                        hotspot_x, hotspot_y);
}

void SetShape(wl_client* client,
              wl_resource* resource,
              uint32_t serial,
              uint32_t shape) {
  GetUserDataAs<MockWpCursorShapeDevice>(resource)->SetShape(serial, shape);
}

const struct zwp_tablet_v2_interface kTestZwpTabletImpl = {
    .destroy = &DestroyResource,
};

const struct zwp_tablet_tool_v2_interface kMockZwpTabletToolImpl = {
    .set_cursor = &SetToolCursor,
    .destroy = &DestroyResource,
};

const struct zwp_tablet_seat_v2_interface kTestZwpTabletSeatImpl = {
    .destroy = &DestroyResource,
};

const struct zwp_tablet_manager_v2_interface kTestZwpTabletManagerImpl = {
    .get_tablet_seat = &TestZwpTabletManager::GetTabletSeat,
    .destroy = &DestroyResource,
};

const struct wp_cursor_shape_device_v1_interface kMockWpCursorShapeDeviceImpl =
    {
        .destroy = &DestroyResource,
        .set_shape = &SetShape,
};

const struct wp_cursor_shape_manager_v1_interface
    kTestWpCursorShapeManagerImpl = {
        .destroy = &DestroyResource,
        .get_pointer = &TestWpCursorShapeManager::GetPointer,
        .get_tablet_tool_v2 = &TestWpCursorShapeManager::GetTabletTool,
};

}  // namespace

TestZwpTablet::TestZwpTablet(wl_resource* resource) : ServerObject(resource) {}

TestZwpTablet::~TestZwpTablet() = default;

MockZwpTabletTool::MockZwpTabletTool(wl_resource* resource)
    : ServerObject(resource) {}

MockZwpTabletTool::~MockZwpTabletTool() = default;

TestZwpTabletSeat::TestZwpTabletSeat(wl_resource* resource)
    : ServerObject(resource) {}

TestZwpTabletSeat::~TestZwpTabletSeat() = default;

TestZwpTablet* TestZwpTabletSeat::AddTablet() {
  wl_resource* tablet_resource = CreateResourceWithImpl<TestZwpTablet>(
      wl_resource_get_client(resource()), &zwp_tablet_v2_interface,
      wl_resource_get_version(resource()), &kTestZwpTabletImpl, /*id=*/0,
      base::BindOnce(&TestZwpTabletSeat::set_tablet,
                     weak_ptr_factory_.GetWeakPtr(), nullptr));
  zwp_tablet_seat_v2_send_tablet_added(resource(), tablet_resource);
  zwp_tablet_v2_send_done(tablet_resource);
  tablet_ = GetUserDataAs<TestZwpTablet>(tablet_resource);
  return tablet_;
}

MockZwpTabletTool* TestZwpTabletSeat::AddTool() {
  wl_resource* tool_resource = CreateResourceWithImpl<NiceMockZwpTabletTool>(
      wl_resource_get_client(resource()), &zwp_tablet_tool_v2_interface,
      wl_resource_get_version(resource()), &kMockZwpTabletToolImpl, /*id=*/0,
      base::BindOnce(&TestZwpTabletSeat::set_tool,
                     weak_ptr_factory_.GetWeakPtr(), nullptr));
  zwp_tablet_seat_v2_send_tool_added(resource(), tool_resource);
  zwp_tablet_tool_v2_send_type(tool_resource, ZWP_TABLET_TOOL_V2_TYPE_PEN);
  zwp_tablet_tool_v2_send_done(tool_resource);
  tool_ = GetUserDataAs<MockZwpTabletTool>(tool_resource);
  return tool_;
}

TestZwpTabletManager::TestZwpTabletManager()
    : GlobalObject(&zwp_tablet_manager_v2_interface,
                   &kTestZwpTabletManagerImpl,
                   kTabletManagerVersion) {}

TestZwpTabletManager::~TestZwpTabletManager() = default;

// static
void TestZwpTabletManager::GetTabletSeat(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat) {
  auto* manager = GetUserDataAs<TestZwpTabletManager>(resource);
  wl_resource* tablet_seat_resource = CreateResourceWithImpl<TestZwpTabletSeat>(
      client, &zwp_tablet_seat_v2_interface, wl_resource_get_version(resource),
      &kTestZwpTabletSeatImpl, id,
      base::BindOnce(&TestZwpTabletManager::set_tablet_seat,
                     manager->GetWeakPtr(), nullptr));
  manager->set_tablet_seat(
      GetUserDataAs<TestZwpTabletSeat>(tablet_seat_resource));
}

MockWpCursorShapeDevice::MockWpCursorShapeDevice(wl_resource* resource)
    : ServerObject(resource) {}

MockWpCursorShapeDevice::~MockWpCursorShapeDevice() = default;

TestWpCursorShapeManager::TestWpCursorShapeManager()
    : GlobalObject(&wp_cursor_shape_manager_v1_interface,
                   &kTestWpCursorShapeManagerImpl,
                   kCursorShapeManagerVersion) {}

TestWpCursorShapeManager::~TestWpCursorShapeManager() = default;

// static
void TestWpCursorShapeManager::GetPointer(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id,
                                          wl_resource* pointer) {
  auto* mock_pointer = GetUserDataAs<MockPointer>(pointer);
  wl_resource* device_resource =
      CreateResourceWithImpl<NiceMockWpCursorShapeDevice>(
          client, &wp_cursor_shape_device_v1_interface,
          wl_resource_get_version(resource), &kMockWpCursorShapeDeviceImpl, id,
          base::BindOnce(&MockPointer::set_cursor_shape_device,
                         mock_pointer->GetWeakPtr(), nullptr));
  mock_pointer->set_cursor_shape_device(
      GetUserDataAs<MockWpCursorShapeDevice>(device_resource));
}

// static
void TestWpCursorShapeManager::GetTabletTool(wl_client* client,
                                             wl_resource* resource,
                                             uint32_t id,
                                             wl_resource* tablet_tool) {
  auto* tool = GetUserDataAs<MockZwpTabletTool>(tablet_tool);
  wl_resource* device_resource =
      CreateResourceWithImpl<NiceMockWpCursorShapeDevice>(
          client, &wp_cursor_shape_device_v1_interface,
          wl_resource_get_version(resource), &kMockWpCursorShapeDeviceImpl, id,
          base::BindOnce(&MockZwpTabletTool::set_cursor_shape_device,
                         tool->GetWeakPtr(), nullptr));
  tool->set_cursor_shape_device(
      GetUserDataAs<MockWpCursorShapeDevice>(device_resource));
}

}  // namespace wl
