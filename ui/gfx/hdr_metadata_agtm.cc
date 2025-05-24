// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata_agtm.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/hdr_metadata.h"

namespace gfx {

namespace {

bool ReadFloat(const base::DictValue* dict, const char* key, float& value) {
  if (!dict) {
    return false;
  }
  if (auto v = dict->FindDouble(key)) {
    value = v.value();
    return true;
  }
  return false;
}

// Read a piecewise cubic into a sampled SkImage
bool ReadPiecewiseCubic(const base::DictValue* dict, sk_sp<SkImage>& curve) {
  if (!dict) {
    DVLOG(1) << "Piecewise cubic is missing";
    return false;
  }
  // The parameters for the piecewise cubic segment.
  float m_min = 0.f, m_span = 0.f;
  float x0 = -1.f, y0 = 0.f, m0 = 0.f;
  float x1 = -1.f, y1 = 0.f, m1 = 0.f;

  // The output SkBitmap.
  const size_t kSamples = 1024;
  SkBitmap bm;
  if (!bm.tryAllocPixels(SkImageInfo::Make(kSamples, 1, kA16_unorm_SkColorType,
                                           kUnpremul_SkAlphaType))) {
    DVLOG(1) << "Failed to allocate pixels for gain curve";
    return false;
  }

  // The sample x value that is being written into `bm`.
  size_t xi = 0;

  // Function to evalue the current piecewise cubic segment.
  auto eval_piecewise_cubic = [&](float x) {
    // If x0 >= x1, then hold the y0 value. This can happen for repeated control
    // points, or initializing values beyond the control points.
    if (x0 >= x1) {
      return y0;
    }
    const float m0p = (m_min + m_span * m0) * (x1 - x0);
    const float m1p = (m_min + m_span * m1) * (x1 - x0);
    const float c3 = (2.0 * y0 + m0p - 2.0 * y1 + m1p);
    const float c2 = (-3.0 * y0 + 3.0 * y1 - 2.0 * m0p - m1p);
    const float c1 = m0p;
    const float c0 = y0;
    const float t = (x - x0) / (x1 - x0);
    return c0 + t * (c1 + t * (c2 + t * c3));
  };

  // Function to increment `xi` until it is past `x1`, writing values into `bm`
  // along the way.
  auto write_samples_through_x1 = [&]() {
    while (xi < kSamples && xi / (kSamples - 1.f) < x1) {
      const float y = eval_piecewise_cubic(xi / (kSamples - 1.f));
      *bm.pixmap().writable_addr16(xi, 0) =
          std::round(65535.f * std::clamp(y, 0.f, 1.f));
      xi += 1;
    }
  };

  // Read the parameters and control points, writing to `bm` along the way.
  if (!ReadFloat(dict, "m_min", m_min) && !ReadFloat(dict, "m_span", m_span)) {
    DVLOG(1) << "Missing slope minimum or span";
    return false;
  }
  const auto* cp_list = dict->FindList("control_points");
  if (!cp_list || cp_list->size() < 1 || cp_list->size() > 64) {
    DVLOG(1) << "Control point list missing or incorrect size";
    return false;
  }
  bool is_first_control_point = true;
  for (const auto& control_point : *cp_list) {
    x0 = x1;
    y0 = y1;
    m0 = m1;
    const auto* control_point_dict = control_point.GetIfDict();
    if (!ReadFloat(control_point_dict, "x", x1) ||
        !ReadFloat(control_point_dict, "y", y1) ||
        !ReadFloat(control_point_dict, "m", m1)) {
      DVLOG(1) << "Control point missing x, y, or m value";
      return false;
    }
    // Repeat the first control point, so that the piecewise cubic segment
    // function will evaluate to a constant.
    if (is_first_control_point) {
      is_first_control_point = false;
      x0 = x1;
      y0 = y1;
      m0 = m1;
    }
    // Samples must be sorted in increasing order.
    if (x0 > x1) {
      DVLOG(1) << "Sample x values not sorted";
      return false;
    }
    // The function must have C0 continuity.
    if (x0 == x1 && y0 != y1) {
      DVLOG(1) << "Function not continuous";
      return false;
    }
    // Write and increment xi until we need to read past x1.
    write_samples_through_x1();
  }

  // Write all samples past the last control point. Repeat the last control
  // point, so that the piecewise cubic segment function will evaluate to a
  // constant.
  x0 = x1;
  y0 = y1;
  m0 = m1;
  write_samples_through_x1();

  curve = SkImages::RasterFromPixmapCopy(bm.pixmap());
  return true;
}

bool ReadAgtmAlternate(const base::DictValue* dict,
                       HdrMetadataAgtmParsed::Alternate& altr) {
  if (!ReadFloat(dict, "hdr_headroom", altr.hdr_headroom)) {
    DVLOG(1) << "Alternate representation missing HDR headroom";
    return false;
  }
  if (const auto* v = dict->FindDict("component_mix_params")) {
    if (!ReadFloat(v, "red", altr.mix_rgbx[0]) &&
        !ReadFloat(v, "green", altr.mix_rgbx[1]) &&
        !ReadFloat(v, "blue", altr.mix_rgbx[2]) &&
        !ReadFloat(v, "max", altr.mix_Mmcx[0]) &&
        !ReadFloat(v, "min", altr.mix_Mmcx[1]) &&
        !ReadFloat(v, "component", altr.mix_Mmcx[2])) {
      DVLOG(1) << "Alternate missing component mix params";
      return false;
    }
  } else {
    DVLOG(1) << "Alternate missing component mix dictionary";
    return false;
  }
  if (!ReadPiecewiseCubic(dict->FindDict("piecewise_cubic"), altr.curve)) {
    DVLOG(1) << "Failed to piecewise cubic";
    return false;
  }
  return true;
}

bool ReadAgtmRoot(const base::Value& value, HdrMetadataAgtmParsed& params) {
  const auto* dict = value.GetIfDict();
  if (!dict) {
    DVLOG(1) << "Agtm root is not dictionary";
    return false;
  }
  if (!ReadFloat(dict, "hdr_reference_white", params.hdr_reference_white) ||
      !ReadFloat(dict, "baseline_hdr_headroom", params.baseline_hdr_headroom) ||
      !ReadFloat(dict, "baseline_max_component",
                 params.baseline_max_component) ||
      !ReadFloat(dict, "gain_min", params.gain_min) ||
      !ReadFloat(dict, "gain_span", params.gain_span) ||
      !ReadFloat(dict, "gain_application_offset",
                 params.gain_application_offset)) {
    DVLOG(1) << "Required values are absent";
    return false;
  }
  if (auto v = dict->FindInt("gain_application_space_primaries")) {
    params.gain_application_color_space =
        SkColorSpace::MakeCICP(static_cast<SkNamedPrimaries::CicpId>(v.value()),
                               SkNamedTransferFn::CicpId::kLinear);
  }
  if (!params.gain_application_color_space) {
    DVLOG(1) << "Invalid or absent gain application space primaries";
    return false;
  }
  const auto* altr_list = dict->FindList("alternates");
  if (altr_list) {
    if (altr_list->size() > 4) {
      DVLOG(1) << "Too many alternates";
      return false;
    }
    for (const auto& altr_value : *altr_list) {
      HdrMetadataAgtmParsed::Alternate altr;
      if (!ReadAgtmAlternate(altr_value.GetIfDict(), altr)) {
        DVLOG(1) << "Failed to read alternate parameters";
        return false;
      }
      params.alternates.push_back(altr);
    }
  }
  return true;
}

}  // namespace

HdrMetadataAgtmParsed::HdrMetadataAgtmParsed() = default;
HdrMetadataAgtmParsed::~HdrMetadataAgtmParsed() = default;

bool HdrMetadataAgtmParsed::Parse(const HdrMetadataAgtm& agtm) {
  if (!HdrMetadataAgtm::IsEnabled()) {
    return false;
  }
  if (!agtm.payload) {
    DVLOG(1) << "Empty AGTM payload";
    return false;
  }
  auto value = base::JSONReader::Read(
      std::string_view(reinterpret_cast<const char*>(agtm.payload->data()),
                       agtm.payload->size()));
  if (!value) {
    DVLOG(1) << "Failed to parse AGTM metadata JSON";
    return false;
  }
  if (!ReadAgtmRoot(value.value(), *this)) {
    return false;
  }
  return true;
}

void HdrMetadataAgtmParsed::ComputeAlternateWeights(float H_target,
                                                    size_t& i,
                                                    float& w_i,
                                                    size_t& j,
                                                    float& w_j) const {
  const float H_base = baseline_hdr_headroom;

  // Let H_i and H_j be the HDR headrooms of the ith and jth representation.
  // Set them to the same value to indicate that the ith representation should
  // be used in full.
  i = j = kBaselineIndex;
  float H_i = H_base;
  float H_j = H_base;

  // Special-case the absence of any alternate representations.
  if (alternates.empty()) {
    w_i = w_j = 0.f;
    return;
  }

  for (i = 0; i < alternates.size(); ++i) {
    j = kBaselineIndex;
    H_i = alternates[i].hdr_headroom;
    H_j = H_base;

    // Mix of i = 0 and potentially baseline
    if (H_target <= H_i) {
      DCHECK_EQ(i, 0u);
      j = kBaselineIndex;
      if (H_base <= H_i) {
        H_j = H_base;
      } else {
        H_j = H_i;
      }
      break;
    }

    // Mix of i = N-1 and potentially baseline.
    if (i == alternates.size() - 1) {
      DCHECK_GT(H_target, H_i);
      j = kBaselineIndex;
      if (H_base >= H_i) {
        H_j = H_base;
      } else {
        H_j = H_i;
      }
      break;
    }

    // Consider the interval between alternate representations i and i+1 only if
    // H_target is in that interval.
    j = i + 1;
    H_j = alternates[j].hdr_headroom;
    if (H_i <= H_target && H_target <= H_j) {
      // If it's the case that i < target < base < i+1, then we will only mix
      // i and base.
      if (H_i <= H_target && H_target <= H_base && H_base <= H_j) {
        j = kBaselineIndex;
        H_j = H_base;
      }

      // If it's the case that i < base < target < i+1, then we will only mix
      // i+1 and base.
      if (H_i <= H_base && H_base <= H_target && H_target <= H_j) {
        i = j;
        H_i = H_j;
        j = kBaselineIndex;
        H_j = H_base;
        break;
      }

      // Otherwise, it's the case that i < target < i+1 and base isn't in that
      // interval.
      DCHECK(H_i <= H_target && H_target <= H_j);
      break;
    }
  }

  // Compute the weights for the two representations.
  if (H_j == H_i) {
    w_i = 1.f;
  } else {
    w_i = std::min(std::max((H_target - H_j) / (H_i - H_j), 0.f), 1.f);
  }
  w_j = 1.f - w_i;

  // Zero out baseline weights.
  if (i == kBaselineIndex) {
    w_i = 0.f;
  }
  if (j == kBaselineIndex) {
    w_j = 0.f;
  }
}

HdrMetadataAgtmParsed::Alternate::Alternate() = default;
HdrMetadataAgtmParsed::Alternate::Alternate(const Alternate&) = default;
HdrMetadataAgtmParsed::Alternate::Alternate(Alternate&&) = default;
HdrMetadataAgtmParsed::Alternate& HdrMetadataAgtmParsed::Alternate::operator=(
    const Alternate&) = default;
HdrMetadataAgtmParsed::Alternate& HdrMetadataAgtmParsed::Alternate::operator=(
    Alternate&&) = default;
HdrMetadataAgtmParsed::Alternate::~Alternate() = default;

}  // namespace gfx
