// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/event_rewriter_metrics.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_info_metrics.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"

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
        {DomCode::FN, ModifierKeyUsageMetric::kFunction},
    });

std::optional<std::string> GetModifierNameFromModifierKeyUsage(
    ModifierKeyUsageMetric val) {
  switch (val) {
    case ModifierKeyUsageMetric::kMetaLeft:
    case ModifierKeyUsageMetric::kMetaRight:
      return "Meta";
    case ModifierKeyUsageMetric::kControlLeft:
    case ModifierKeyUsageMetric::kControlRight:
      return "Control";
    case ModifierKeyUsageMetric::kAltLeft:
    case ModifierKeyUsageMetric::kAltRight:
      return "Alt";
    case ModifierKeyUsageMetric::kShiftLeft:
    case ModifierKeyUsageMetric::kShiftRight:
      return std::nullopt;
    case ModifierKeyUsageMetric::kCapsLock:
      return "CapsLock";
    case ModifierKeyUsageMetric::kBackspace:
      return "Backspace";
    case ModifierKeyUsageMetric::kEscape:
      return "Escape";
    case ModifierKeyUsageMetric::kAssistant:
      return "Assistant";
    case ModifierKeyUsageMetric::kFunction:
      return "Function";
    case ModifierKeyUsageMetric::kRightAlt:
      return "RightAlt";
  }
}

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

void RecordKeyUsageMetric(const KeyboardCapability& keyboard_capability,
                          KeyboardCapability::DeviceType device_type,
                          int device_id,
                          DomCode dom_code,
                          DomCode original_dom_code,
                          ui::ModifierKeyUsageMetric modifier_key) {
  if (device_type != KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    return;
  }

  auto modifier_name = GetModifierNameFromModifierKeyUsage(modifier_key);
  if (!modifier_name) {
    return;
  }

  const bool dom_codes_match = dom_code == original_dom_code;
  const bool rewritten_to_right_alt =
      dom_codes_match && modifier_key == ModifierKeyUsageMetric::kRightAlt &&
      !keyboard_capability.HasRightAltKey(device_id);
  const bool rewritten_to_assistant =
      dom_codes_match && modifier_key == ModifierKeyUsageMetric::kAssistant &&
      !keyboard_capability.HasAssistantKey(device_id);
  const bool modifier_was_rewritten =
      !dom_codes_match || rewritten_to_assistant || rewritten_to_right_alt;
  base::UmaHistogramEnumeration(
      base::StrCat({"ChromeOS.Inputs.KeyUsage.Internal.", *modifier_name}),
      modifier_was_rewritten ? KeyUsageCategory::kVirtuallyPressed
                             : KeyUsageCategory::kPhysicallyPressed);
}

}  // namespace

void RecordModifierKeyPressedBeforeRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code) {
  auto it = kModifierKeyUsageMappings.find(dom_code);
  if (it == kModifierKeyUsageMappings.end()) {
    return;
  }

  auto device_name =
      GetDeviceNameForHistogram(keyboard_capability.GetDeviceType(device_id));
  if (device_name.empty()) {
    return;
  }

  auto modifier_key = it->second;

  if (modifier_key == ModifierKeyUsageMetric::kAssistant &&
      keyboard_capability.HasRightAltKey(device_id)) {
    modifier_key = ModifierKeyUsageMetric::kRightAlt;
  }

  if ((modifier_key == ModifierKeyUsageMetric::kFunction ||
       modifier_key == ModifierKeyUsageMetric::kRightAlt) &&
      !keyboard_capability.HasRightAltKey(device_id)) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"ChromeOS.Inputs.Keyboard.ModifierPressed.", device_name}),
      modifier_key);
}

void RecordModifierKeyPressedAfterRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code,
    DomCode original_dom_code,
    bool is_right_alt_key) {
  auto it = kModifierKeyUsageMappings.find(dom_code);
  if (it == kModifierKeyUsageMappings.end()) {
    return;
  }

  const auto device_type = keyboard_capability.GetDeviceType(device_id);
  auto device_name = GetDeviceNameForHistogram(device_type);
  if (device_name.empty()) {
    return;
  }

  auto modifier_key = it->second;
  if (modifier_key == ModifierKeyUsageMetric::kAssistant && is_right_alt_key) {
    modifier_key = ModifierKeyUsageMetric::kRightAlt;
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ChromeOS.Inputs.Keyboard.RemappedModifierPressed.", device_name}),
      modifier_key);
  RecordKeyUsageMetric(keyboard_capability, device_type, device_id, dom_code,
                       original_dom_code, modifier_key);
}

}  // namespace ui
