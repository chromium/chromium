// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_TESTHELPERS_MOCK_GESTURE_PROPERTIES_SERVICE_H_
#define UI_OZONE_TESTHELPERS_MOCK_GESTURE_PROPERTIES_SERVICE_H_

#include "gmock/gmock.h"
#include "ui/ozone/public/mojom/gesture_properties_service.mojom.h"

// Mock of GesturePropertiesService's C++ bindings, useful for tests.
class MockGesturePropertiesService
    : public ui::ozone::mojom::GesturePropertiesService {
 public:
  MockGesturePropertiesService();
  ~MockGesturePropertiesService();

  MOCK_METHOD1(ListDevices, void(ListDevicesCallback));
  MOCK_METHOD2(ListProperties, void(int32_t, ListPropertiesCallback));
  MOCK_METHOD3(GetProperty,
               void(int32_t, const std::string&, GetPropertyCallback));
  MOCK_METHOD4(SetProperty,
               void(int32_t,
                    const std::string&,
                    ui::ozone::mojom::GesturePropValuePtr,
                    SetPropertyCallback));
};

// Work around the compile error '[chromium-style] Complex class/struct needs an
// explicit out-of-line constructor.'
inline MockGesturePropertiesService::MockGesturePropertiesService() = default;
inline MockGesturePropertiesService::~MockGesturePropertiesService() = default;

#endif  // UI_OZONE_TESTHELPERS_MOCK_GESTURE_PROPERTIES_SERVICE_H_
