// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <random>

#include "base/at_exit.h"
#include "base/logging.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/icc_profile.h"

static constexpr size_t kPixels = 256;

static gfx::ColorTransform::TriStim pixels[kPixels];

static void GeneratePixels(size_t hash) {
  static std::uniform_real_distribution<float> uniform(-0.1f, 1.1f);

  std::mt19937_64 random(hash);
  for (size_t i = 0; i < kPixels; ++i)
    pixels[i].SetPoint(uniform(random), uniform(random), uniform(random));
}

static gfx::ColorSpace test;
static gfx::ColorSpace srgb;

static void ColorTransform(size_t hash) {
  const gfx::ColorTransform::Options options;

  std::unique_ptr<gfx::ColorTransform> transform;
  if (hash & 2) {
    transform = gfx::ColorTransform::NewColorTransform(test, srgb, options);
  } else {
    transform = gfx::ColorTransform::NewColorTransform(srgb, test, options);
  }

  transform->Transform(pixels, kPixels);
}

static gfx::ColorSpace CreateRGBColorSpace(size_t hash) {
  auto primaries = static_cast<gfx::ColorSpace::PrimaryID>(
      1 + ((hash >> 0) % (size_t)gfx::ColorSpace::PrimaryID::kMaxValue));
  auto transfer = static_cast<gfx::ColorSpace::TransferID>(
      1 + ((hash >> 8) % (size_t)gfx::ColorSpace::TransferID::kMaxValue));
  auto matrix = static_cast<gfx::ColorSpace::MatrixID>(
      1 + ((hash >> 16) % (size_t)gfx::ColorSpace::MatrixID::kMaxValue));
  auto range = static_cast<gfx::ColorSpace::RangeID>(
      1 + ((hash >> 24) % (size_t)gfx::ColorSpace::RangeID::kMaxValue));

  return gfx::ColorSpace(primaries, transfer, matrix, range);
}

inline size_t Hash(const char* data, size_t size, size_t hash = ~0) {
  for (size_t i = 0; i < size; ++i)
    hash = hash * 131 + *data++;
  return hash;
}

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* environment = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::AtExitManager at_exit;

  constexpr size_t kSizeLimit = 4 * 1024 * 1024;
  if (size < 128 || size > kSizeLimit)
    return 0;

  gfx::ICCProfile profile =
      gfx::ICCProfile::FromData(reinterpret_cast<const char*>(data), size);
  if (!profile.GetColorSpace().IsValid())
    return 0;
  test = profile.GetColorSpace();

  const size_t hash = Hash(reinterpret_cast<const char*>(data), size);
  srgb = CreateRGBColorSpace(hash);
  GeneratePixels(hash);

  ColorTransform(hash);
  return 0;
}
