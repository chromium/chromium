// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_properties_service.h"

#include <utility>

namespace ui {

namespace {

using ozone::mojom::GesturePropValue;

ozone::mojom::GesturePropValuePtr GesturePropValueFromProp(GesturesProp* prop) {
  if (prop == nullptr) {
    return nullptr;
  }
  switch (prop->type()) {
    case GesturesProp::PropertyType::PT_INT:
      return GesturePropValue::NewInts(prop->GetIntValue());
    case GesturesProp::PropertyType::PT_SHORT:
      return GesturePropValue::NewShorts(prop->GetShortValue());
    case GesturesProp::PropertyType::PT_BOOL:
      return GesturePropValue::NewBools(prop->GetBoolValue());
    case GesturesProp::PropertyType::PT_STRING:
      return GesturePropValue::NewStr(prop->GetStringValue());
    case GesturesProp::PropertyType::PT_REAL:
      return GesturePropValue::NewReals(prop->GetDoubleValue());
  }
}

bool PropertyTypeMatchesValues(ui::GesturePropertyProvider::PropertyType type,
                               GesturePropValue::Tag values_tag) {
  switch (type) {
    case ui::GesturePropertyProvider::PT_INT:
      return values_tag == GesturePropValue::Tag::kInts;
    case ui::GesturePropertyProvider::PT_SHORT:
      return values_tag == GesturePropValue::Tag::kShorts;
    case ui::GesturePropertyProvider::PT_BOOL:
      return values_tag == GesturePropValue::Tag::kBools;
    case ui::GesturePropertyProvider::PT_STRING:
      return values_tag == GesturePropValue::Tag::kStr;
    case ui::GesturePropertyProvider::PT_REAL:
      return values_tag == GesturePropValue::Tag::kReals;
  }
  // This should never happen.
  return false;
}

bool TrySetPropertyValues(GesturesProp* property,
                          ozone::mojom::GesturePropValuePtr values) {
  switch (property->type()) {
    case ui::GesturePropertyProvider::PT_INT:
      return property->SetIntValue(values->get_ints());
    case ui::GesturePropertyProvider::PT_SHORT:
      return property->SetShortValue(values->get_shorts());
    case ui::GesturePropertyProvider::PT_BOOL:
      return property->SetBoolValue(values->get_bools());
    case ui::GesturePropertyProvider::PT_STRING:
      return property->SetStringValue(values->get_str());
    case ui::GesturePropertyProvider::PT_REAL:
      return property->SetDoubleValue(values->get_reals());
  }
}

}  // namespace

GesturePropertiesService::GesturePropertiesService(
    GesturePropertyProvider* provider,
    mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver)
    : prop_provider_(provider), receiver_(this, std::move(receiver)) {}

void GesturePropertiesService::ListDevices(ListDevicesCallback reply) {
  base::flat_map<int, std::string> response = {};
  std::vector<int> ids;
  prop_provider_->GetDeviceIdsByType(DT_ALL, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    response.emplace(ids[i], prop_provider_->GetDeviceNameById(ids[i]));
  }
  std::move(reply).Run(response);
}

void GesturePropertiesService::ListProperties(int device_id,
                                              ListPropertiesCallback reply) {
  std::vector<std::string> response =
      prop_provider_->GetPropertyNamesById(device_id);
  std::move(reply).Run(response);
}

void GesturePropertiesService::GetProperty(int device_id,
                                           const std::string& name,
                                           GetPropertyCallback reply) {
  bool is_read_only = true;
  GesturesProp* property = prop_provider_->GetProperty(device_id, name);
  ozone::mojom::GesturePropValuePtr prop_value =
      GesturePropValueFromProp(property);
  if (property != nullptr) {
    is_read_only = property->IsReadOnly();
  }
  std::move(reply).Run(is_read_only, std::move(prop_value));
}

void GesturePropertiesService::SetProperty(
    int device_id,
    const std::string& name,
    ozone::mojom::GesturePropValuePtr values,
    SetPropertyCallback reply) {
  GesturesProp* property = prop_provider_->GetProperty(device_id, name);
  if (property == NULL) {
    std::move(reply).Run(ozone::mojom::SetGesturePropErrorCode::NOT_FOUND);
    return;
  }
  if (property->IsReadOnly()) {
    std::move(reply).Run(ozone::mojom::SetGesturePropErrorCode::READ_ONLY);
    return;
  }
  if (!PropertyTypeMatchesValues(property->type(), values->which())) {
    std::move(reply).Run(ozone::mojom::SetGesturePropErrorCode::TYPE_MISMATCH);
    return;
  }
  size_t num_values;
  switch (values->which()) {
    case ozone::mojom::GesturePropValue::Tag::kInts:
      num_values = values->get_ints().size();
      break;
    case ozone::mojom::GesturePropValue::Tag::kShorts:
      num_values = values->get_shorts().size();
      break;
    case ozone::mojom::GesturePropValue::Tag::kBools:
      num_values = values->get_bools().size();
      break;
    case ozone::mojom::GesturePropValue::Tag::kStr:
      num_values = 1;
      break;
    case ozone::mojom::GesturePropValue::Tag::kReals:
      num_values = values->get_reals().size();
      break;
  }
  if (num_values != property->count()) {
    std::move(reply).Run(ozone::mojom::SetGesturePropErrorCode::SIZE_MISMATCH);
    return;
  }

  bool did_set = TrySetPropertyValues(property, std::move(values));
  std::move(reply).Run(
      did_set ? ozone::mojom::SetGesturePropErrorCode::SUCCESS
              : ozone::mojom::SetGesturePropErrorCode::UNKNOWN_ERROR);
}

}  // namespace ui
