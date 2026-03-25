// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_constants_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

display::mojom::DisplayConnectionType EnumTraits<
    display::mojom::DisplayConnectionType,
    display::DisplayConnectionType>::ToMojom(display::DisplayConnectionType
                                                 type) {
  switch (type) {
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_NONE;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_UNKNOWN;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_INTERNAL;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA:
      return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_HDMI;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI:
      return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_NETWORK;
  }
  NOTREACHED();
}

display::DisplayConnectionType EnumTraits<display::mojom::DisplayConnectionType,
                                          display::DisplayConnectionType>::
    FromMojom(display::mojom::DisplayConnectionType type) {
  switch (type) {
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN;
    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_INTERNAL:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI;
    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return display::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK:
      return display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK;
  }
  NOTREACHED();
}

// static
display::mojom::HDCPState
EnumTraits<display::mojom::HDCPState, display::HDCPState>::ToMojom(
    display::HDCPState type) {
  switch (type) {
    case display::HDCPState::HDCP_STATE_UNDESIRED:
      return display::mojom::HDCPState::HDCP_STATE_UNDESIRED;
    case display::HDCPState::HDCP_STATE_DESIRED:
      return display::mojom::HDCPState::HDCP_STATE_DESIRED;
    case display::HDCPState::HDCP_STATE_ENABLED:
      return display::mojom::HDCPState::HDCP_STATE_ENABLED;
  }
  NOTREACHED();
}

// static
display::HDCPState
EnumTraits<display::mojom::HDCPState, display::HDCPState>::FromMojom(
    display::mojom::HDCPState type) {
  switch (type) {
    case display::mojom::HDCPState::HDCP_STATE_UNDESIRED:
      return display::HDCPState::HDCP_STATE_UNDESIRED;
    case display::mojom::HDCPState::HDCP_STATE_DESIRED:
      return display::HDCPState::HDCP_STATE_DESIRED;
    case display::mojom::HDCPState::HDCP_STATE_ENABLED:
      return display::HDCPState::HDCP_STATE_ENABLED;
  }
  NOTREACHED();
}

// static
display::mojom::ContentProtectionMethod EnumTraits<
    display::mojom::ContentProtectionMethod,
    display::ContentProtectionMethod>::ToMojom(display::ContentProtectionMethod
                                                   type) {
  switch (type) {
    case display::ContentProtectionMethod::CONTENT_PROTECTION_METHOD_NONE:
      return display::mojom::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_NONE;
    case display::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_0:
      return display::mojom::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
    case display::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1:
      return display::mojom::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
  }
  NOTREACHED();
}

// static
display::ContentProtectionMethod
EnumTraits<display::mojom::ContentProtectionMethod,
           display::ContentProtectionMethod>::
    FromMojom(display::mojom::ContentProtectionMethod type) {
  switch (type) {
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_NONE:
      return display::ContentProtectionMethod::CONTENT_PROTECTION_METHOD_NONE;
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_0:
      return display::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1:
      return display::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
  }
  NOTREACHED();
}

// static
display::mojom::PanelOrientation EnumTraits<
    display::mojom::PanelOrientation,
    display::PanelOrientation>::ToMojom(display::PanelOrientation rotation) {
  switch (rotation) {
    case display::PanelOrientation::kNormal:
      return display::mojom::PanelOrientation::NORMAL;
    case display::PanelOrientation::kBottomUp:
      return display::mojom::PanelOrientation::BOTTOM_UP;
    case display::PanelOrientation::kLeftUp:
      return display::mojom::PanelOrientation::LEFT_UP;
    case display::PanelOrientation::kRightUp:
      return display::mojom::PanelOrientation::RIGHT_UP;
  }
  NOTREACHED();
}

// static
display::PanelOrientation
EnumTraits<display::mojom::PanelOrientation, display::PanelOrientation>::
    FromMojom(display::mojom::PanelOrientation rotation) {
  switch (rotation) {
    case display::mojom::PanelOrientation::NORMAL:
      return display::PanelOrientation::kNormal;
    case display::mojom::PanelOrientation::BOTTOM_UP:
      return display::PanelOrientation::kBottomUp;
    case display::mojom::PanelOrientation::LEFT_UP:
      return display::PanelOrientation::kLeftUp;
    case display::mojom::PanelOrientation::RIGHT_UP:
      return display::PanelOrientation::kRightUp;
  }
  NOTREACHED();
}

// static
display::mojom::PrivacyScreenState EnumTraits<
    display::mojom::PrivacyScreenState,
    display::PrivacyScreenState>::ToMojom(display::PrivacyScreenState state) {
  switch (state) {
    case display::PrivacyScreenState::kDisabled:
      return display::mojom::PrivacyScreenState::DISABLED;
    case display::PrivacyScreenState::kEnabled:
      return display::mojom::PrivacyScreenState::ENABLED;
    case display::PrivacyScreenState::kDisabledLocked:
      return display::mojom::PrivacyScreenState::DISABLED_LOCKED;
    case display::PrivacyScreenState::kEnabledLocked:
      return display::mojom::PrivacyScreenState::ENABLED_LOCKED;
    case display::PrivacyScreenState::kNotSupported:
      return display::mojom::PrivacyScreenState::NOT_SUPPORTED;
  }
  NOTREACHED();
}

// static
display::PrivacyScreenState
EnumTraits<display::mojom::PrivacyScreenState, display::PrivacyScreenState>::
    FromMojom(display::mojom::PrivacyScreenState state) {
  switch (state) {
    case display::mojom::PrivacyScreenState::DISABLED:
      return display::PrivacyScreenState::kDisabled;
    case display::mojom::PrivacyScreenState::ENABLED:
      return display::PrivacyScreenState::kEnabled;
    case display::mojom::PrivacyScreenState::DISABLED_LOCKED:
      return display::PrivacyScreenState::kDisabledLocked;
    case display::mojom::PrivacyScreenState::ENABLED_LOCKED:
      return display::PrivacyScreenState::kEnabledLocked;
    case display::mojom::PrivacyScreenState::NOT_SUPPORTED:
      return display::PrivacyScreenState::kNotSupported;
  }
  NOTREACHED();
}

// static
display::mojom::VariableRefreshRateState
EnumTraits<display::mojom::VariableRefreshRateState,
           display::VariableRefreshRateState>::
    ToMojom(display::VariableRefreshRateState state) {
  switch (state) {
    case display::VariableRefreshRateState::kVrrDisabled:
      return display::mojom::VariableRefreshRateState::kVrrDisabled;
    case display::VariableRefreshRateState::kVrrEnabled:
      return display::mojom::VariableRefreshRateState::kVrrEnabled;
    case display::VariableRefreshRateState::kVrrNotCapable:
      return display::mojom::VariableRefreshRateState::kVrrNotCapable;
  }
  NOTREACHED();
}

// static
display::VariableRefreshRateState
EnumTraits<display::mojom::VariableRefreshRateState,
           display::VariableRefreshRateState>::
    FromMojom(display::mojom::VariableRefreshRateState state) {
  switch (state) {
    case display::mojom::VariableRefreshRateState::kVrrDisabled:
      return display::VariableRefreshRateState::kVrrDisabled;
    case display::mojom::VariableRefreshRateState::kVrrEnabled:
      return display::VariableRefreshRateState::kVrrEnabled;
    case display::mojom::VariableRefreshRateState::kVrrNotCapable:
      return display::VariableRefreshRateState::kVrrNotCapable;
  }
  NOTREACHED();
}

// static
bool StructTraits<display::mojom::ModesetFlagsDataView, display::ModesetFlags>::
    Read(display::mojom::ModesetFlagsDataView data,
         display::ModesetFlags* out_flags) {
  *out_flags = display::ModesetFlags::FromEnumBitmask(data.bitmask());
  return true;
}

}  // namespace mojo
