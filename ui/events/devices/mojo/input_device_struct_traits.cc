// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/mojo/input_device_struct_traits.h"

#include "base/logging.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

ui::mojom::InputDeviceType
EnumTraits<ui::mojom::InputDeviceType, ui::InputDeviceType>::ToMojom(
    ui::InputDeviceType type) {
  switch (type) {
    case ui::INPUT_DEVICE_INTERNAL:
      return ui::mojom::InputDeviceType::INPUT_DEVICE_INTERNAL;
    case ui::INPUT_DEVICE_USB:
      return ui::mojom::InputDeviceType::INPUT_DEVICE_USB;
    case ui::INPUT_DEVICE_BLUETOOTH:
      return ui::mojom::InputDeviceType::INPUT_DEVICE_BLUETOOTH;
    case ui::INPUT_DEVICE_UNKNOWN:
      return ui::mojom::InputDeviceType::INPUT_DEVICE_UNKNOWN;
  }
  NOTREACHED();
  return ui::mojom::InputDeviceType::INPUT_DEVICE_UNKNOWN;
}

bool EnumTraits<ui::mojom::InputDeviceType, ui::InputDeviceType>::FromMojom(
    ui::mojom::InputDeviceType type,
    ui::InputDeviceType* output) {
  switch (type) {
    case ui::mojom::InputDeviceType::INPUT_DEVICE_INTERNAL:
      *output = ui::INPUT_DEVICE_INTERNAL;
      break;
    case ui::mojom::InputDeviceType::INPUT_DEVICE_USB:
      *output = ui::INPUT_DEVICE_USB;
      break;
    case ui::mojom::InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      *output = ui::INPUT_DEVICE_BLUETOOTH;
      break;
    case ui::mojom::InputDeviceType::INPUT_DEVICE_UNKNOWN:
      *output = ui::INPUT_DEVICE_UNKNOWN;
      break;
    default:
      // Who knows what values might come over the wire, fail if invalid.
      return false;
  }
  return true;
}

bool StructTraits<ui::mojom::InputDeviceDataView, ui::InputDevice>::Read(
    ui::mojom::InputDeviceDataView data,
    ui::InputDevice* out) {
  out->id = data.id();

  if (!data.ReadType(&out->type))
    return false;

  if (!data.ReadName(&out->name))
    return false;

  std::string sys_path_string;
  if (!data.ReadSysPath(&sys_path_string))
    return false;
  out->sys_path = base::FilePath::FromUTF8Unsafe(sys_path_string);

  out->vendor_id = data.vendor_id();
  out->product_id = data.product_id();

  return true;
}

ui::mojom::StylusState
EnumTraits<ui::mojom::StylusState, ui::StylusState>::ToMojom(
    ui::StylusState type) {
  switch (type) {
    case ui::StylusState::REMOVED:
      return ui::mojom::StylusState::REMOVED;
    case ui::StylusState::INSERTED:
      return ui::mojom::StylusState::INSERTED;
  }
  NOTREACHED();
  return ui::mojom::StylusState::INSERTED;
}

bool EnumTraits<ui::mojom::StylusState, ui::StylusState>::FromMojom(
    ui::mojom::StylusState type,
    ui::StylusState* output) {
  switch (type) {
    case ui::mojom::StylusState::REMOVED:
      *output = ui::StylusState::REMOVED;
      break;
    case ui::mojom::StylusState::INSERTED:
      *output = ui::StylusState::INSERTED;
      break;
    default:
      // Who knows what values might come over the wire, fail if invalid.
      return false;
  }
  return true;
}

bool StructTraits<ui::mojom::TouchscreenDeviceDataView, ui::TouchscreenDevice>::
    Read(ui::mojom::TouchscreenDeviceDataView data,
         ui::TouchscreenDevice* out) {
  if (!data.ReadInputDevice(static_cast<ui::InputDevice*>(out)))
    return false;

  if (!data.ReadSize(&out->size))
    return false;

  out->touch_points = data.touch_points();
  out->has_stylus = data.has_stylus();

  return true;
}

}  // namespace mojo
