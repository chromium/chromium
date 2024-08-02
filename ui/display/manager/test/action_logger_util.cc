// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/manager/test/action_logger_util.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display::test {

std::string GetCrtcAction(
    const display::DisplayConfigurationParams& display_config_params) {
  return base::StringPrintf(
      "crtc(display_id=[%" PRId64 "],x=%d,y=%d,mode=[%s],enable_vrr=%d)",
      display_config_params.id, display_config_params.origin.x(),
      display_config_params.origin.y(),
      display_config_params.mode
          ? display_config_params.mode->ToStringForTest().c_str()
          : "NULL",
      display_config_params.enable_vrr);
}

std::string GetSetHDCPStateAction(int64_t display_id,
                                  HDCPState state,
                                  ContentProtectionMethod protection_method) {
  return base::StringPrintf("set_hdcp(id=%" PRId64 ",state=%d,method=%d)",
                            display_id, state, protection_method);
}

std::string GetSetHdcpKeyPropAction(int64_t display_id, bool success) {
  return base::StringPrintf("set_hdcp_key_prop(id=%" PRId64 ",success=%d)",
                            display_id, success);
}

std::string SetColorCalibrationAction(
    int64_t display_id,
    const display::ColorCalibration& calibration) {
  return base::StringPrintf("set_color_calibration(id=%" PRId64 ")",
                            display_id);
}

std::string SetColorTemperatureAdjustmentAction(
    int64_t display_id,
    const display::ColorTemperatureAdjustment& cta) {
  return base::StringPrintf(
      "set_color_temperature_adjustment(id=%" PRId64 ",cta[%1.2f,%1.2f,%1.2f)",
      display_id, cta.srgb_matrix.vals[0][0], cta.srgb_matrix.vals[1][1],
      cta.srgb_matrix.vals[2][2]);
}

std::string SetGammaAdjustmentAction(int64_t display_id,
                                     const display::GammaAdjustment& gamma) {
  return base::StringPrintf("set_gamma_adjustment(id=%" PRId64 "%s)",
                            display_id,
                            gamma.curve.ToActionString("gamma").c_str());
}

std::string SetPrivacyScreenAction(int64_t display_id, bool enabled) {
  return base::StringPrintf("set_privacy_screen(id=%" PRId64 ",state=%d)",
                            display_id, enabled);
}

std::string JoinActions(const char* action, ...) {
  std::string actions;

  va_list arg_list;
  va_start(arg_list, action);
  while (action) {
    if (!actions.empty())
      actions += ",";
    actions += action;
    action = va_arg(arg_list, const char*);
  }
  va_end(arg_list);
  return actions;
}

}  // namespace display::test
