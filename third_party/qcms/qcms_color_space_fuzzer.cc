// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>

#include "testing/libfuzzer/fuzzers/color_space_data.h"
#include "third_party/qcms/src/qcms.h"

static constexpr size_t kPixels = 2048 / 4;

static uint32_t pixels[kPixels];

static void GeneratePixels(size_t hash) {
  static std::uniform_int_distribution<uint32_t> uniform(0u, ~0u);

  std::mt19937_64 random(hash);
  for (size_t i = 0; i < std::size(pixels); ++i)
    pixels[i] = uniform(random);
}

static qcms_profile* test;
static qcms_profile* srgb;

static void ColorTransform(bool input) {
  if (!test)
    return;

  const qcms_intent intent = QCMS_INTENT_DEFAULT;
  const qcms_data_type format = QCMS_DATA_RGBA_8;

  auto* transform =
      input ? qcms_transform_create(test, format, srgb, format, intent)
            : qcms_transform_create(srgb, format, test, format, intent);
  if (!transform)
    return;

  static uint32_t output[kPixels];

  qcms_transform_data(transform, pixels, output, kPixels);
  qcms_transform_release(transform);
}

static qcms_profile* SelectProfile(size_t hash) {
  static qcms_profile* profiles[4] = {
      qcms_profile_from_memory(kSRGBData, std::size(kSRGBData)),
      qcms_profile_from_memory(kSRGBPara, std::size(kSRGBPara)),
      qcms_profile_from_memory(kAdobeData, std::size(kAdobeData)),
      qcms_profile_sRGB(),
  };

  return profiles[hash & 3];
}

inline size_t Hash(const char* data, size_t size, size_t hash = ~0) {
  for (size_t i = 0; i < size; ++i)
    hash = hash * 131 + *data++;
  return hash;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kSizeLimit = 4 * 1024 * 1024;
  if (size < 128 || size > kSizeLimit)
    return 0;

  test = qcms_profile_from_memory(data, size);
  if (!test)
    return 0;

  const size_t hash = Hash(reinterpret_cast<const char*>(data), size);
  srgb = SelectProfile(hash);
  GeneratePixels(hash);

  qcms_profile_precache_output_transform(srgb);
  ColorTransform(true);

  qcms_profile_precache_output_transform(test);
  ColorTransform(false);

  qcms_profile_release(test);
  return 0;
}
