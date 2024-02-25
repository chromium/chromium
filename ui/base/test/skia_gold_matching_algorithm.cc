// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_matching_algorithm.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace ui {
namespace {
constexpr char kMaxDifferentPixels[] = "fuzzy_max_different_pixels";
constexpr char kPixelDeltaThreshold[] = "fuzzy_pixel_delta_threshold";
constexpr char kEdgeThreshold[] = "sobel_edge_threshold";
constexpr char kIgnoredBorderThickness[] = "fuzzy_ignored_border_thickness";
}  // namespace

namespace test {

// SkiaGoldMatchingAlgorithm ---------------------------------------------------

SkiaGoldMatchingAlgorithm::SkiaGoldMatchingAlgorithm() = default;

SkiaGoldMatchingAlgorithm::~SkiaGoldMatchingAlgorithm() = default;

void SkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(
    base::CommandLine& cmd) const {
  cmd.AppendSwitchASCII(
      "add-test-optional-key",
      base::JoinString({"image_matching_algorithm", GetCommandLineSwitchName()},
                       ":"));
}

// ExactSkiaGoldMatchingAlgorithm ----------------------------------------------

ExactSkiaGoldMatchingAlgorithm::ExactSkiaGoldMatchingAlgorithm() = default;

ExactSkiaGoldMatchingAlgorithm::~ExactSkiaGoldMatchingAlgorithm() = default;

std::string ExactSkiaGoldMatchingAlgorithm::GetCommandLineSwitchName() const {
  return "exact";
}

void ExactSkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(
    base::CommandLine& cmd) const {
  // Do not call base class AppendAlgorithmToCmdline.
  // Nothing to append.
}

// FuzzySkiaGoldMatchingAlgorithm ----------------------------------------------

FuzzySkiaGoldMatchingAlgorithm::~FuzzySkiaGoldMatchingAlgorithm() = default;

std::string FuzzySkiaGoldMatchingAlgorithm::GetCommandLineSwitchName() const {
  return "fuzzy";
}

FuzzySkiaGoldMatchingAlgorithm::FuzzySkiaGoldMatchingAlgorithm(
    int max_different_pixels,
    int pixel_delta_threshold,
    int ignored_border_thickness)
    : max_different_pixels_(max_different_pixels),
      pixel_delta_threshold_(pixel_delta_threshold),
      ignored_border_thickness_(ignored_border_thickness) {
  DCHECK_GT(max_different_pixels, 0);
  DCHECK_GT(pixel_delta_threshold, 0);
  DCHECK_LE(pixel_delta_threshold, 255 * 4);
  DCHECK_GE(ignored_border_thickness, 0);
}

void FuzzySkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(
    base::CommandLine& cmd) const {
  SkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(cmd);
  cmd.AppendSwitchASCII(
      "add-test-optional-key",
      base::JoinString(
          {kMaxDifferentPixels, base::NumberToString(max_different_pixels_)},
          ":"));
  cmd.AppendSwitchASCII(
      "add-test-optional-key",
      base::JoinString(
          {kPixelDeltaThreshold, base::NumberToString(pixel_delta_threshold_)},
          ":"));

  if (ignored_border_thickness_)
    cmd.AppendSwitchASCII(
        "add-test-optional-key",
        base::JoinString({kIgnoredBorderThickness,
                          base::NumberToString(ignored_border_thickness_)},
                         ":"));
}

// SobelSkiaGoldMatchingAlgorithm ----------------------------------------------

SobelSkiaGoldMatchingAlgorithm::~SobelSkiaGoldMatchingAlgorithm() = default;

std::string SobelSkiaGoldMatchingAlgorithm::GetCommandLineSwitchName() const {
  return "sobel";
}

SobelSkiaGoldMatchingAlgorithm::SobelSkiaGoldMatchingAlgorithm(
    int max_different_pixels,
    int pixel_delta_threshold,
    int edge_threshold,
    int ignored_border_thickness)
    : FuzzySkiaGoldMatchingAlgorithm(max_different_pixels,
                                     pixel_delta_threshold,
                                     ignored_border_thickness),
      edge_threshold_(edge_threshold) {
  DCHECK_GE(edge_threshold, 0);
  DCHECK_LE(edge_threshold, 255);
}

void SobelSkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(
    base::CommandLine& cmd) const {
  FuzzySkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(cmd);
  cmd.AppendSwitchASCII(
      "add-test-optional-key",
      base::JoinString({kEdgeThreshold, base::NumberToString(edge_threshold_)},
                       ":"));
}

// PositiveIfOnlyImageAlgorithm ------------------------------------------------

PositiveIfOnlyImageAlgorithm::PositiveIfOnlyImageAlgorithm() = default;

PositiveIfOnlyImageAlgorithm::~PositiveIfOnlyImageAlgorithm() = default;

void PositiveIfOnlyImageAlgorithm::AppendAlgorithmToCmdline(
    base::CommandLine& cmd) const {
  SkiaGoldMatchingAlgorithm::AppendAlgorithmToCmdline(cmd);

  // Disallow the user from triaging images.
  cmd.AppendSwitchASCII("add-test-optional-key", "disallow_triaging:true");
}

std::string PositiveIfOnlyImageAlgorithm::GetCommandLineSwitchName() const {
  return "positive_if_only_image";
}

}  // namespace test
}  // namespace ui
