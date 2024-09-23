// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_constants_mojom_traits.h"

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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE;
}

bool EnumTraits<display::mojom::DisplayConnectionType,
                display::DisplayConnectionType>::
    FromMojom(display::mojom::DisplayConnectionType type,
              display::DisplayConnectionType* out) {
  switch (type) {
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE;
      return true;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN;
      return true;
    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_INTERNAL:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL;
      return true;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA;
      return true;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI;
      return true;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI;
      return true;
    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      *out =
          display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
      return true;
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK;
      return true;
  }
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::HDCPState::HDCP_STATE_UNDESIRED;
}

// static
bool EnumTraits<display::mojom::HDCPState, display::HDCPState>::FromMojom(
    display::mojom::HDCPState type,
    display::HDCPState* out) {
  switch (type) {
    case display::mojom::HDCPState::HDCP_STATE_UNDESIRED:
      *out = display::HDCPState::HDCP_STATE_UNDESIRED;
      return true;
    case display::mojom::HDCPState::HDCP_STATE_DESIRED:
      *out = display::HDCPState::HDCP_STATE_DESIRED;
      return true;
    case display::mojom::HDCPState::HDCP_STATE_ENABLED:
      *out = display::HDCPState::HDCP_STATE_ENABLED;
      return true;
  }
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::ContentProtectionMethod::
      CONTENT_PROTECTION_METHOD_NONE;
}

// static
bool EnumTraits<display::mojom::ContentProtectionMethod,
                display::ContentProtectionMethod>::
    FromMojom(display::mojom::ContentProtectionMethod type,
              display::ContentProtectionMethod* out) {
  switch (type) {
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_NONE:
      *out = display::ContentProtectionMethod::CONTENT_PROTECTION_METHOD_NONE;
      return true;
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_0:
      *out = display::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
      return true;
    case display::mojom::ContentProtectionMethod::
        CONTENT_PROTECTION_METHOD_HDCP_TYPE_1:
      *out = display::ContentProtectionMethod::
          CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
      return true;
  }
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::PanelOrientation::NORMAL;
}

// static
bool EnumTraits<display::mojom::PanelOrientation, display::PanelOrientation>::
    FromMojom(display::mojom::PanelOrientation rotation,
              display::PanelOrientation* out) {
  switch (rotation) {
    case display::mojom::PanelOrientation::NORMAL:
      *out = display::PanelOrientation::kNormal;
      return true;
    case display::mojom::PanelOrientation::BOTTOM_UP:
      *out = display::PanelOrientation::kBottomUp;
      return true;
    case display::mojom::PanelOrientation::LEFT_UP:
      *out = display::PanelOrientation::kLeftUp;
      return true;
    case display::mojom::PanelOrientation::RIGHT_UP:
      *out = display::PanelOrientation::kRightUp;
      return true;
  }
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::PrivacyScreenState::NOT_SUPPORTED;
}

// static
bool EnumTraits<display::mojom::PrivacyScreenState,
                display::PrivacyScreenState>::
    FromMojom(display::mojom::PrivacyScreenState state,
              display::PrivacyScreenState* out) {
  switch (state) {
    case display::mojom::PrivacyScreenState::DISABLED:
      *out = display::PrivacyScreenState::kDisabled;
      return true;
    case display::mojom::PrivacyScreenState::ENABLED:
      *out = display::PrivacyScreenState::kEnabled;
      return true;
    case display::mojom::PrivacyScreenState::DISABLED_LOCKED:
      *out = display::PrivacyScreenState::kDisabledLocked;
      return true;
    case display::mojom::PrivacyScreenState::ENABLED_LOCKED:
      *out = display::PrivacyScreenState::kEnabledLocked;
      return true;
    case display::mojom::PrivacyScreenState::NOT_SUPPORTED:
      *out = display::PrivacyScreenState::kNotSupported;
      return true;
  }
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return display::mojom::VariableRefreshRateState::kVrrNotCapable;
}

// static
bool EnumTraits<display::mojom::VariableRefreshRateState,
                display::VariableRefreshRateState>::
    FromMojom(display::mojom::VariableRefreshRateState state,
              display::VariableRefreshRateState* out) {
  switch (state) {
    case display::mojom::VariableRefreshRateState::kVrrDisabled:
      *out = display::VariableRefreshRateState::kVrrDisabled;
      return true;
    case display::mojom::VariableRefreshRateState::kVrrEnabled:
      *out = display::VariableRefreshRateState::kVrrEnabled;
      return true;
    case display::mojom::VariableRefreshRateState::kVrrNotCapable:
      *out = display::VariableRefreshRateState::kVrrNotCapable;
      return true;
  }
  return false;
}

// static
bool StructTraits<display::mojom::ModesetFlagsDataView, display::ModesetFlags>::
    Read(display::mojom::ModesetFlagsDataView data,
         display::ModesetFlags* out_flags) {
  *out_flags = display::ModesetFlags::FromEnumBitmask(data.bitmask());
  return true;
}

}  // namespace mojo
