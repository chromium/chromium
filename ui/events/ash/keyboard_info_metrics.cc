// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_info_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ui {

namespace {

KeyboardTopRowLayoutForMetric ConvertTopRowLayoutToMetricEnum(
    ui::KeyboardCapability::KeyboardTopRowLayout top_row_layout,
    bool has_assistant_key,
    bool has_right_alt_key) {
  switch (top_row_layout) {
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
      return KeyboardTopRowLayoutForMetric::kLayout1;
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
      if (has_assistant_key) {
        return KeyboardTopRowLayoutForMetric::kLayout2WithAssistant;
      } else {
        return KeyboardTopRowLayoutForMetric::kLayout2;
      }
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      return KeyboardTopRowLayoutForMetric::kLayout3;
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      return KeyboardTopRowLayoutForMetric::kLayout4;
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      if (has_right_alt_key) {
        return KeyboardTopRowLayoutForMetric::kLayoutCustom2;
      } else {
        return KeyboardTopRowLayoutForMetric::kLayoutCustom1;
      }
    default:
      return KeyboardTopRowLayoutForMetric::kLayoutUnknown;
  }
}

}  // namespace

void RecordKeyboardInfoMetrics(
    const KeyboardCapability::KeyboardInfo& keyboard_info,
    bool has_assistant_key,
    bool has_right_alt_key) {
  if (keyboard_info.device_type !=
      KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    return;
  }

  base::UmaHistogramEnumeration(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ConvertTopRowLayoutToMetricEnum(keyboard_info.top_row_layout,
                                      has_assistant_key, has_right_alt_key));

  base::UmaHistogramCounts100(
      "ChromeOS.Inputs.InternalKeyboard.NumberOfTopRowKeys",
      keyboard_info.top_row_action_keys.size());

  for (const ui::TopRowActionKey& action_key :
       keyboard_info.top_row_action_keys) {
    base::UmaHistogramEnumeration(
        "ChromeOS.Inputs.InternalKeyboard.TopRowKeysPresent", action_key);
  }

  if (keyboard_info.top_row_layout !=
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom) {
    return;
  }

  base::UmaHistogramCounts100(
      "ChromeOS.Inputs.InternalKeyboard.CustomTopRowLayout."
      "NumberOfTopRowKeys",
      keyboard_info.top_row_action_keys.size());

  for (const ui::TopRowActionKey& action_key :
       keyboard_info.top_row_action_keys) {
    base::UmaHistogramEnumeration(
        "ChromeOS.Inputs.InternalKeyboard.CustomTopRowLayout."
        "TopRowKeysPresent",
        action_key);
  }
}

}  // namespace ui
