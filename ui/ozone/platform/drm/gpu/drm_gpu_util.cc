// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace ui {

bool GetDrmPropertyForName(DrmWrapper* drm,
                           drmModeObjectProperties* properties,
                           const std::string& name,
                           DrmWrapper::Property* property) {
  for (uint32_t i = 0; i < properties->count_props; ++i) {
    ScopedDrmPropertyPtr drm_property(drm->GetProperty(properties->props[i]));
    if (name != drm_property->name)
      continue;

    property->id = drm_property->prop_id;
    property->value = properties->prop_values[i];
    if (property->id)
      return true;
  }

  return false;
}

bool AddPropertyIfValid(drmModeAtomicReq* property_set,
                        uint32_t object_id,
                        const DrmWrapper::Property& property) {
  if (!property.id)
    return true;

  int ret = drmModeAtomicAddProperty(property_set, object_id, property.id,
                                     property.value);
  if (ret < 0) {
    LOG(ERROR) << "Failed to set property object_id=" << object_id
               << " property_id=" << property.id
               << " property_value=" << property.value << " error=" << -ret;
    return false;
  }

  return true;
}

ScopedDrmColorLutPtr CreateLutBlob(
    const std::vector<display::GammaRampRGBEntry>& source) {
  TRACE_EVENT0("drm", "CreateLutBlob");
  if (source.empty())
    return nullptr;

  ScopedDrmColorLutPtr lut(static_cast<drm_color_lut*>(
      malloc(sizeof(drm_color_lut) * source.size())));
  drm_color_lut* p = lut.get();
  for (size_t i = 0; i < source.size(); ++i) {
    p[i].red = source[i].r;
    p[i].green = source[i].g;
    p[i].blue = source[i].b;
  }
  return lut;
}

ScopedDrmColorCtmPtr CreateCTMBlob(const std::vector<float>& color_matrix) {
  if (color_matrix.empty())
    return nullptr;

  ScopedDrmColorCtmPtr ctm(
      static_cast<drm_color_ctm*>(malloc(sizeof(drm_color_ctm))));
  DCHECK_EQ(color_matrix.size(), std::size(ctm->matrix));
  for (size_t i = 0; i < std::size(ctm->matrix); ++i) {
    if (color_matrix[i] < 0) {
      ctm->matrix[i] = static_cast<uint64_t>(-color_matrix[i] * (1ull << 32));
      ctm->matrix[i] |= static_cast<uint64_t>(1) << 63;
    } else {
      ctm->matrix[i] = static_cast<uint64_t>(color_matrix[i] * (1ull << 32));
    }
  }
  return ctm;
}

ScopedDrmModeRectPtr CreateDCBlob(const gfx::Rect& rect) {
  // Damage rect should be non empty and non negative, otherwise there is
  // risk of artifacting and black screens.
  if (rect.width() <= 0) {
    LOG(ERROR) << "Damage rect width must be positive: " << rect.ToString();
    return nullptr;
  }
  if (rect.height() <= 0) {
    LOG(ERROR) << "Damage rect height must be positive: " << rect.ToString();
    return nullptr;
  }
  if (rect.x() < 0) {
    LOG(ERROR) << "Damage rect x1 is negative: " << rect.x();
    return nullptr;
  }
  if (rect.y() < 0) {
    LOG(ERROR) << "Damage rect y1 is negative: " << rect.y();
    return nullptr;
  }

  ScopedDrmModeRectPtr dmg_rect(
      static_cast<drm_mode_rect*>(malloc(sizeof(drm_mode_rect))));
  dmg_rect->x1 = rect.x();
  dmg_rect->y1 = rect.y();
  dmg_rect->x2 = rect.right();
  dmg_rect->y2 = rect.bottom();
  return dmg_rect;
}

std::vector<display::GammaRampRGBEntry> ResampleLut(
    const std::vector<display::GammaRampRGBEntry>& lut_in,
    size_t desired_size) {
  TRACE_EVENT1("drm", "ResampleLut", "desired_size", desired_size);
  if (lut_in.empty())
    return std::vector<display::GammaRampRGBEntry>();

  if (lut_in.size() == desired_size)
    return lut_in;

  std::vector<display::GammaRampRGBEntry> result;
  result.resize(desired_size);

  for (size_t i = 0; i < desired_size; ++i) {
    size_t base_index = lut_in.size() * i / desired_size;
    size_t remaining = lut_in.size() * i % desired_size;
    if (base_index < lut_in.size() - 1) {
      result[i].r = lut_in[base_index].r +
                    (lut_in[base_index + 1].r - lut_in[base_index].r) *
                        remaining / desired_size;
      result[i].g = lut_in[base_index].g +
                    (lut_in[base_index + 1].g - lut_in[base_index].g) *
                        remaining / desired_size;
      result[i].b = lut_in[base_index].b +
                    (lut_in[base_index + 1].b - lut_in[base_index].b) *
                        remaining / desired_size;
    } else {
      result[i] = lut_in.back();
    }
  }

  return result;
}

HardwareDisplayControllerInfoList GetDisplayInfosAndUpdateCrtcs(
    DrmWrapper& drm) {
  auto [displays, invalid_crtcs] = GetDisplayInfosAndInvalidCrtcs(drm);
  // Disable invalid CRTCs to allow the preferred CRTCs to be enabled later
  // instead.
  for (uint32_t crtc : invalid_crtcs) {
    drm.DisableCrtc(crtc);
    VLOG(1) << "Disabled undesired CRTC " << crtc;
  }
  return std::move(displays);
}

void DrmWriteIntoTraceHelper(const drmModeModeInfo& mode_info,
                             perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("name", mode_info.name);
  dict.Add("type", mode_info.type);
  dict.Add("flags", mode_info.flags);
  dict.Add("clock", mode_info.clock);
  dict.Add("hdisplay", mode_info.hdisplay);
  dict.Add("vdisplay", mode_info.vdisplay);
}

}  // namespace ui
