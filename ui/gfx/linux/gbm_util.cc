// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/gbm_util.h"

#include "base/notreached.h"
#include "ui/gfx/linux/gbm_defines.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check_op.h"
#include "base/environment.h"
#include "ui/gfx/switches.h"
#endif

namespace ui {
#if BUILDFLAG(IS_CHROMEOS)
namespace {
constexpr base::StringPiece kEnableIntelMediaCompressionEnvVar =
    "ENABLE_INTEL_MEDIA_COMPRESSION";
}
#endif

uint32_t BufferUsageToGbmFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT:
      return GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_SCANOUT |
             GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_DECODER;
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_PROTECTED |
             GBM_BO_USE_HW_VIDEO_DECODER;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_ENCODER;
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE |
             GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_ENCODER |
             GBM_BO_USE_SW_READ_OFTEN;
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_FRONT_RENDERING;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void EnsureIntelMediaCompressionEnvVarIsSet() {
  auto environment = base::Environment::Create();
  CHECK(environment);
  const std::string value_to_set(
      base::FeatureList::IsEnabled(features::kEnableIntelMediaCompression)
          ? "1"
          : "0");
  if (environment->HasVar(kEnableIntelMediaCompressionEnvVar)) {
    std::string env_var_value;
    CHECK(environment->GetVar(kEnableIntelMediaCompressionEnvVar,
                              &env_var_value));
    CHECK_EQ(env_var_value, value_to_set);
    return;
  }
  CHECK(environment->SetVar(kEnableIntelMediaCompressionEnvVar, value_to_set));
}

bool IntelMediaCompressionEnvVarIsSet() {
  auto environment = base::Environment::Create();
  CHECK(environment);
  std::string env_var_value;
  return environment->GetVar(kEnableIntelMediaCompressionEnvVar,
                             &env_var_value) &&
         (env_var_value == "0" || env_var_value == "1");
}
#endif

}  // namespace ui
