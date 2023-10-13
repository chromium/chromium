// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/event_rewriter_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ui {
namespace {

constexpr auto kModifierKeyUsageMappings =
    base::MakeFixedFlatMap<DomCode, ModifierKeyUsageMetric>({
        {DomCode::CONTROL_LEFT, ModifierKeyUsageMetric::kControlLeft},
        {DomCode::CONTROL_RIGHT, ModifierKeyUsageMetric::kControlRight},
        {DomCode::META_LEFT, ModifierKeyUsageMetric::kMetaLeft},
        {DomCode::META_RIGHT, ModifierKeyUsageMetric::kMetaRight},
        {DomCode::ALT_LEFT, ModifierKeyUsageMetric::kAltLeft},
        {DomCode::ALT_RIGHT, ModifierKeyUsageMetric::kAltRight},
        {DomCode::SHIFT_LEFT, ModifierKeyUsageMetric::kShiftLeft},
        {DomCode::SHIFT_RIGHT, ModifierKeyUsageMetric::kShiftRight},
        {DomCode::BACKSPACE, ModifierKeyUsageMetric::kBackspace},
        {DomCode::ESCAPE, ModifierKeyUsageMetric::kEscape},
        {DomCode::CAPS_LOCK, ModifierKeyUsageMetric::kCapsLock},
        {DomCode::LAUNCH_ASSISTANT, ModifierKeyUsageMetric::kAssistant},
    });

// Returns the name to be used as a part of histogram name.
// If empty, no histogram is expected.
std::string_view GetDeviceNameForHistogram(
    KeyboardCapability::DeviceType device_type) {
  // Using switch-case here in order catch the case that a new entry
  // is added to DeviceType.
  switch (device_type) {
    case KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard:
      return "Internal";
    case KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      return "AppleExternal";
    case KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
      return "CrOSExternal";
    case KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case KeyboardCapability::DeviceType::
        kDeviceExternalNullTopRowChromeOsKeyboard:
    case KeyboardCapability::DeviceType::kDeviceExternalUnknown:
      return "External";
    case KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
    case KeyboardCapability::DeviceType::kDeviceUnknown:
      // No histogram.
      return "";
  }
}

void RecordModifierKeyPressedRemappingInternal(
    const KeyboardCapability& keyboard_capability,
    const std::string_view metric_name,
    int device_id,
    DomCode dom_code) {
  auto* it = kModifierKeyUsageMappings.find(dom_code);
  if (it == kModifierKeyUsageMappings.end()) {
    return;
  }

  auto device_name =
      GetDeviceNameForHistogram(keyboard_capability.GetDeviceType(device_id));
  if (device_name.empty()) {
    return;
  }

  base::UmaHistogramEnumeration(base::StrCat({"ChromeOS.Inputs.Keyboard.",
                                              metric_name, ".", device_name}),
                                it->second);
}

}  // namespace

void RecordModifierKeyPressedBeforeRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code) {
  RecordModifierKeyPressedRemappingInternal(
      keyboard_capability, "ModifierPressed", device_id, dom_code);
}

void RecordModifierKeyPressedAfterRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code) {
  RecordModifierKeyPressedRemappingInternal(
      keyboard_capability, "RemappedModifierPressed", device_id, dom_code);
}

}  // namespace ui
