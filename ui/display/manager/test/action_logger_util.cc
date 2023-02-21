// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/action_logger_util.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
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
      display_config_params.mode.has_value()
          ? display_config_params.mode.value()->ToString().c_str()
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

std::string SetColorMatrixAction(int64_t display_id,
                                 const std::vector<float>& color_matrix) {
  std::string ctm;
  for (size_t i = 0; i < color_matrix.size(); ++i)
    ctm += base::StringPrintf(",ctm[%" PRIuS "]=%f", i, color_matrix[i]);

  return base::StringPrintf("set_color_matrix(id=%" PRId64 "%s)", display_id,
                            ctm.c_str());
}

std::string SetGammaCorrectionAction(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  std::string degamma_table;
  for (size_t i = 0; i < degamma_lut.size(); ++i) {
    degamma_table += base::StringPrintf(",degamma[%" PRIuS "]=%04x%04x%04x", i,
                                        degamma_lut[i].r, degamma_lut[i].g,
                                        degamma_lut[i].b);
  }
  std::string gamma_table;
  for (size_t i = 0; i < gamma_lut.size(); ++i) {
    gamma_table +=
        base::StringPrintf(",gamma[%" PRIuS "]=%04x%04x%04x", i, gamma_lut[i].r,
                           gamma_lut[i].g, gamma_lut[i].b);
  }

  return base::StringPrintf("set_gamma_correction(id=%" PRId64 "%s%s)",
                            display_id, degamma_table.c_str(),
                            gamma_table.c_str());
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
