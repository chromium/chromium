// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTIES_SERVICE_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTIES_SERVICE_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#include "ui/ozone/public/mojom/gesture_properties_service.mojom.h"

class GesturePropertyProvider;

namespace ui {

class COMPONENT_EXPORT(EVDEV) GesturePropertiesService
    : public ui::ozone::mojom::GesturePropertiesService {
 public:
  GesturePropertiesService(
      GesturePropertyProvider* provider,
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver);

  void ListDevices(ListDevicesCallback callback) override;
  void ListProperties(int32_t device_id,
                      ListPropertiesCallback callback) override;
  void GetProperty(int32_t device_id,
                   const std::string& name,
                   GetPropertyCallback callback) override;
  void SetProperty(int32_t device_id,
                   const std::string& name,
                   ui::ozone::mojom::GesturePropValuePtr values,
                   SetPropertyCallback callback) override;

 private:
  GesturePropertyProvider* prop_provider_;
  mojo::Receiver<ui::ozone::mojom::GesturePropertiesService> receiver_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTIES_SERVICE_H_
