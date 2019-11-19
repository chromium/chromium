// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/action_logger_util.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace test {

std::string DisplaySnapshotToString(const DisplaySnapshot& output) {
  return base::StringPrintf("id=%" PRId64, output.display_id());
}

std::string GetCrtcAction(const DisplaySnapshot& output,
                          const DisplayMode* mode,
                          const gfx::Point& origin) {
  return base::StringPrintf("crtc(display=[%s],x=%d,y=%d,mode=[%s])",
                            DisplaySnapshotToString(output).c_str(), origin.x(),
                            origin.y(),
                            mode ? mode->ToString().c_str() : "NULL");
}

std::string GetSetHDCPStateAction(int64_t display_id, HDCPState state) {
  return base::StringPrintf("set_hdcp(id=%" PRId64 ",state=%d)", display_id,
                            state);
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

}  // namespace test
}  // namespace display
