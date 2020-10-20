// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef WEBP_TESTS_FUZZER_FUZZ_UTILS_H_
#define WEBP_TESTS_FUZZER_FUZZ_UTILS_H_

#include <stdint.h>
#include <stdlib.h>

#include "./img_alpha.h"
#include "./img_grid.h"
#include "./img_peak.h"
#include "src/dsp/dsp.h"
#include "src/webp/encode.h"

//------------------------------------------------------------------------------
// Arbitrary limits to prevent OOM, timeout, or slow execution.
//
// The decoded image size, and for animations additionally the canvas size.
static const size_t kFuzzPxLimit = 1024 * 1024;
// Demuxed or decoded animation frames.
static const int kFuzzFrameLimit = 3;

// Reads and sums (up to) 128 spread-out bytes.
static WEBP_INLINE uint8_t FuzzHash(const uint8_t* const data, size_t size) {
  uint8_t value = 0;
  size_t incr = size / 128;
  if (!incr) incr = 1;
  for (size_t i = 0; i < size; i += incr) value += data[i];
  return value;
}

//------------------------------------------------------------------------------
// Extract an integer in [0, max_value].

static WEBP_INLINE uint32_t Extract(uint32_t max_value,
                                    const uint8_t data[], size_t size,
                                    uint32_t* const bit_pos) {
  uint32_t v = 0;
  uint32_t range = 1;
  while (*bit_pos < 8 * size && range <= max_value) {
    const uint8_t mask = 1u << (*bit_pos & 7);
    v = (v << 1) | !!(data[*bit_pos >> 3] & mask);
    range <<= 1;
    ++*bit_pos;
  }
  return v % (max_value + 1);
}

//------------------------------------------------------------------------------
// Some functions to override VP8GetCPUInfo and disable some optimizations.

static VP8CPUInfo GetCPUInfo;

static WEBP_INLINE int GetCPUInfoNoSSE41(CPUFeature feature) {
  if (feature == kSSE4_1 || feature == kAVX) return 0;
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoNoAVX(CPUFeature feature) {
  if (feature == kAVX) return 0;
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoForceSlowSSSE3(CPUFeature feature) {
  if (feature == kSlowSSSE3 && GetCPUInfo(kSSE3)) {
    return 1;  // we have SSE3 -> force SlowSSSE3
  }
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoOnlyC(CPUFeature feature) {
  (void)feature;
  return 0;
}

static WEBP_INLINE void ExtractAndDisableOptimizations(
    VP8CPUInfo default_VP8GetCPUInfo, const uint8_t data[], size_t size,
    uint32_t* const bit_pos) {
  GetCPUInfo = default_VP8GetCPUInfo;
  const VP8CPUInfo kVP8CPUInfos[5] = {GetCPUInfoOnlyC, GetCPUInfoForceSlowSSSE3,
                                      GetCPUInfoNoSSE41, GetCPUInfoNoAVX,
                                      GetCPUInfo};
  int VP8GetCPUInfo_index = Extract(4, data, size, bit_pos);
  VP8GetCPUInfo = kVP8CPUInfos[VP8GetCPUInfo_index];
}

//------------------------------------------------------------------------------

static WEBP_INLINE int ExtractWebPConfig(WebPConfig* const config,
                                         const uint8_t data[], size_t size,
                                         uint32_t* const bit_pos) {
  if (config == NULL || !WebPConfigInit(config)) return 0;
  config->lossless = Extract(1, data, size, bit_pos);
  config->quality = Extract(100, data, size, bit_pos);
  config->method = Extract(6, data, size, bit_pos);
  config->image_hint =
      (WebPImageHint)Extract(WEBP_HINT_LAST - 1, data, size, bit_pos);
  config->segments = 1 + Extract(3, data, size, bit_pos);
  config->sns_strength = Extract(100, data, size, bit_pos);
  config->filter_strength = Extract(100, data, size, bit_pos);
  config->filter_sharpness = Extract(7, data, size, bit_pos);
  config->filter_type = Extract(1, data, size, bit_pos);
  config->autofilter = Extract(1, data, size, bit_pos);
  config->alpha_compression = Extract(1, data, size, bit_pos);
  config->alpha_filtering = Extract(2, data, size, bit_pos);
  config->alpha_quality = Extract(100, data, size, bit_pos);
  config->pass = 1 + Extract(9, data, size, bit_pos);
  config->show_compressed = 1;
  config->preprocessing = Extract(2, data, size, bit_pos);
  config->partitions = Extract(3, data, size, bit_pos);
  config->partition_limit = 10 * Extract(10, data, size, bit_pos);
  config->emulate_jpeg_size = Extract(1, data, size, bit_pos);
  config->thread_level = Extract(1, data, size, bit_pos);
  config->low_memory = Extract(1, data, size, bit_pos);
  config->near_lossless = 20 * Extract(5, data, size, bit_pos);
  config->exact = Extract(1, data, size, bit_pos);
  config->use_delta_palette = Extract(1, data, size, bit_pos);
  config->use_sharp_yuv = Extract(1, data, size, bit_pos);
  return WebPValidateConfig(config);
}

//------------------------------------------------------------------------------

static WEBP_INLINE int ExtractSourcePicture(WebPPicture* const pic,
                                            const uint8_t data[], size_t size,
                                            uint32_t* const bit_pos) {
  if (pic == NULL) return 0;

  // Pick a source picture.
  const uint8_t* kImagesData[] = {
      kImgAlphaData,
      kImgGridData,
      kImgPeakData
  };
  const int kImagesWidth[] = {
      kImgAlphaWidth,
      kImgGridWidth,
      kImgPeakWidth
  };
  const int kImagesHeight[] = {
      kImgAlphaHeight,
      kImgGridHeight,
      kImgPeakHeight
  };
  const size_t kNbImages = sizeof(kImagesData) / sizeof(kImagesData[0]);
  const size_t image_index = Extract(kNbImages - 1, data, size, bit_pos);
  const uint8_t* const image_data = kImagesData[image_index];
  pic->width = kImagesWidth[image_index];
  pic->height = kImagesHeight[image_index];
  pic->argb_stride = pic->width * 4 * sizeof(uint8_t);

  // Read the bytes.
  return WebPPictureImportRGBA(pic, image_data, pic->argb_stride);
}

//------------------------------------------------------------------------------

static WEBP_INLINE int Max(int a, int b) { return ((a < b) ? b : a); }

static WEBP_INLINE int ExtractAndCropOrScale(WebPPicture* const pic,
                                             const uint8_t data[], size_t size,
                                             uint32_t* const bit_pos) {
  if (pic == NULL) return 0;
#if !defined(WEBP_REDUCE_SIZE)
  const int alter_input = Extract(1, data, size, bit_pos);
  const int crop_or_scale = Extract(1, data, size, bit_pos);
  const int width_ratio = 1 + Extract(7, data, size, bit_pos);
  const int height_ratio = 1 + Extract(7, data, size, bit_pos);
  if (alter_input) {
    if (crop_or_scale) {
      const uint32_t left_ratio = 1 + Extract(7, data, size, bit_pos);
      const uint32_t top_ratio = 1 + Extract(7, data, size, bit_pos);
      const int cropped_width = Max(1, pic->width / width_ratio);
      const int cropped_height = Max(1, pic->height / height_ratio);
      const int cropped_left = (pic->width - cropped_width) / left_ratio;
      const int cropped_top = (pic->height - cropped_height) / top_ratio;
      return WebPPictureCrop(pic, cropped_left, cropped_top, cropped_width,
                             cropped_height);
    } else {
      const int scaled_width = 1 + (pic->width * width_ratio) / 8;
      const int scaled_height = 1 + (pic->height * height_ratio) / 8;
      return WebPPictureRescale(pic, scaled_width, scaled_height);
    }
  }
#else   // defined(WEBP_REDUCE_SIZE)
  (void)data;
  (void)size;
  (void)bit_pos;
#endif  // !defined(WEBP_REDUCE_SIZE)
  return 1;
}

#endif  // WEBP_TESTS_FUZZER_FUZZ_UTILS_H_
