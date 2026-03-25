// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_CONSTANTS_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_CONSTANTS_MOJOM_TRAITS_H_

#include "ui/display/mojom/display_constants.mojom.h"
#include "ui/display/types/display_constants.h"

namespace mojo {

template <>
struct EnumTraits<display::mojom::DisplayConnectionType,
                  display::DisplayConnectionType> {
  static display::mojom::DisplayConnectionType ToMojom(
      display::DisplayConnectionType type);
  static display::DisplayConnectionType FromMojom(
      display::mojom::DisplayConnectionType type);
};

template <>
struct EnumTraits<display::mojom::HDCPState, display::HDCPState> {
  static display::mojom::HDCPState ToMojom(display::HDCPState type);
  static display::HDCPState FromMojom(display::mojom::HDCPState type);
};

template <>
struct EnumTraits<display::mojom::ContentProtectionMethod,
                  display::ContentProtectionMethod> {
  static display::mojom::ContentProtectionMethod ToMojom(
      display::ContentProtectionMethod type);
  static display::ContentProtectionMethod FromMojom(
      display::mojom::ContentProtectionMethod type);
};

template <>
struct EnumTraits<display::mojom::PanelOrientation, display::PanelOrientation> {
  static display::mojom::PanelOrientation ToMojom(
      display::PanelOrientation type);
  static display::PanelOrientation FromMojom(
      display::mojom::PanelOrientation type);
};

template <>
struct EnumTraits<display::mojom::PrivacyScreenState,
                  display::PrivacyScreenState> {
  static display::mojom::PrivacyScreenState ToMojom(
      display::PrivacyScreenState type);
  static display::PrivacyScreenState FromMojom(
      display::mojom::PrivacyScreenState type);
};

template <>
struct EnumTraits<display::mojom::VariableRefreshRateState,
                  display::VariableRefreshRateState> {
  static display::mojom::VariableRefreshRateState ToMojom(
      display::VariableRefreshRateState type);
  static display::VariableRefreshRateState FromMojom(
      display::mojom::VariableRefreshRateState type);
};

template <>
struct StructTraits<display::mojom::ModesetFlagsDataView,
                    display::ModesetFlags> {
  static uint64_t bitmask(const display::ModesetFlags& flags) {
    return flags.ToEnumBitmask();
  }

  static bool Read(display::mojom::ModesetFlagsDataView data,
                   display::ModesetFlags* out_range);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_CONSTANTS_MOJOM_TRAITS_H_
