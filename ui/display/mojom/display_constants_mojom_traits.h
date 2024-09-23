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
  static bool FromMojom(display::mojom::DisplayConnectionType type,
                        display::DisplayConnectionType* out);
};

template <>
struct EnumTraits<display::mojom::HDCPState, display::HDCPState> {
  static display::mojom::HDCPState ToMojom(display::HDCPState type);
  static bool FromMojom(display::mojom::HDCPState type,
                        display::HDCPState* out);
};

template <>
struct EnumTraits<display::mojom::ContentProtectionMethod,
                  display::ContentProtectionMethod> {
  static display::mojom::ContentProtectionMethod ToMojom(
      display::ContentProtectionMethod type);
  static bool FromMojom(display::mojom::ContentProtectionMethod type,
                        display::ContentProtectionMethod* out);
};

template <>
struct EnumTraits<display::mojom::PanelOrientation, display::PanelOrientation> {
  static display::mojom::PanelOrientation ToMojom(
      display::PanelOrientation type);
  static bool FromMojom(display::mojom::PanelOrientation type,
                        display::PanelOrientation* out);
};

template <>
struct EnumTraits<display::mojom::PrivacyScreenState,
                  display::PrivacyScreenState> {
  static display::mojom::PrivacyScreenState ToMojom(
      display::PrivacyScreenState type);
  static bool FromMojom(display::mojom::PrivacyScreenState type,
                        display::PrivacyScreenState* out);
};

template <>
struct EnumTraits<display::mojom::VariableRefreshRateState,
                  display::VariableRefreshRateState> {
  static display::mojom::VariableRefreshRateState ToMojom(
      display::VariableRefreshRateState type);
  static bool FromMojom(display::mojom::VariableRefreshRateState type,
                        display::VariableRefreshRateState* out);
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
