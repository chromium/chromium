// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/wayland/color_manager_util.h"

#include <cstdint>

#include "chrome-color-management-server-protocol.h"

namespace ui::wayland {

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID,
    uint32_t version) {
  for (const auto& it : kChromaticityMap) {
    if (it.second.primary == primaryID) {
      if (it.second.version <= version) {
        return it.first;
      }
      return ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT709;
    }
  }
  return ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_UNKNOWN;
}

zcr_color_manager_v1_matrix_names ToColorManagerMatrix(
    gfx::ColorSpace::MatrixID matrixID,
    uint32_t version) {
  for (const auto& it : kMatrixMap) {
    if (it.second.matrix == matrixID) {
      if (it.second.version <= version) {
        return it.first;
      }
      return ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_RGB;
    }
  }
  return ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_UNKNOWN;
}

zcr_color_manager_v1_range_names ToColorManagerRange(
    gfx::ColorSpace::RangeID rangeID,
    uint32_t version) {
  for (const auto& it : kRangeMap) {
    if (it.second.range == rangeID) {
      if (it.second.version <= version) {
        return it.first;
      }
      return ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL;
    }
  }
  return ZCR_COLOR_MANAGER_V1_RANGE_NAMES_UNKNOWN;
}

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID,
    uint32_t version) {
  for (const auto& it : kEotfMap) {
    if (it.second.transfer == transferID) {
      if (it.second.version <= version) {
        return it.first;
      }
      return ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB;
    }
  }
  return ZCR_COLOR_MANAGER_V1_EOTF_NAMES_UNKNOWN;
}

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(gfx::ColorSpace color_space,
                                                   uint32_t version) {
  if (color_space.IsHDR()) {
    for (const auto& it : kHDRTransferMap) {
      if (color_space.IsTransferFunctionEqualTo(it.second.transfer_fn)) {
        if (it.second.version <= version) {
          return it.first;
        }
        return ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB;
      }
    }
  }
  return ToColorManagerEOTF(color_space.GetTransferID(), version);
}
}  // namespace ui::wayland
