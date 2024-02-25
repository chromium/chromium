// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SKIA_GOLD_MATCHING_ALGORITHM_H_
#define UI_BASE_TEST_SKIA_GOLD_MATCHING_ALGORITHM_H_

#include <string>

namespace base {
class CommandLine;
}

namespace ui {
namespace test {

// Describes what algorithm to use to match golden images.
// Fuzzy and Sobel algorithms have adjustable parameters:
//
// Fuzzy has the max number of different pixels, the max per-channel delta sum
// (i.e. how much a pixel can differ by and still be considered the same), and
// how many border pixels to ignore. The number of ignored border pixels is
// mainly to make the Sobel filter work better, as if an edge goes all the way
// to the border of the image, the pixels along the border won't be blacked
// out due to the Sobel filter.
//
// Sobel has the parameters of fuzzy + the edge threshold, which determines how
// much of the image should be blacked out based on the generated Sobel filter.
//
// In general, it works like this:
// 1. Try an exact comparison check against all approved images for the test,
// returning success if a hash matches.
// 2. Get the most recent image for the trace (test + any reported JSON keys,
// e.g. OS/hardware information)
// 3. If the algorithm is Sobel, generate a Sobel filter and black out parts
// of the image based on the provided edge threshold.
// 4. Do a fuzzy comparison of the images.
//
// To determine the suitable parameter values using historical data for a test:
// https://cs.chromium.org/chromium/src/content/test/gpu/gold_inexact_matching/determine_gold_inexact_parameters.py
class SkiaGoldMatchingAlgorithm {
 public:
  SkiaGoldMatchingAlgorithm();
  virtual ~SkiaGoldMatchingAlgorithm();
  // Append the algorithm parameter to |cmd|.
  virtual void AppendAlgorithmToCmdline(base::CommandLine& cmd) const;

  // The algorithm name for commandline.
  virtual std::string GetCommandLineSwitchName() const = 0;
};

class ExactSkiaGoldMatchingAlgorithm : public SkiaGoldMatchingAlgorithm {
 public:
  ExactSkiaGoldMatchingAlgorithm();
  ~ExactSkiaGoldMatchingAlgorithm() override;
  void AppendAlgorithmToCmdline(base::CommandLine& cmd) const override;

 protected:
  std::string GetCommandLineSwitchName() const override;
};

class FuzzySkiaGoldMatchingAlgorithm : public SkiaGoldMatchingAlgorithm {
 public:
  FuzzySkiaGoldMatchingAlgorithm(int max_different_pixels,
                                 int pixel_delta_threshold,
                                 int ignored_border_thickness = 0);
  ~FuzzySkiaGoldMatchingAlgorithm() override;

  void AppendAlgorithmToCmdline(base::CommandLine& cmd) const override;

 protected:
  std::string GetCommandLineSwitchName() const override;

 private:
  int max_different_pixels_{0};
  int pixel_delta_threshold_{0};
  int ignored_border_thickness_{0};
};

class SobelSkiaGoldMatchingAlgorithm : public FuzzySkiaGoldMatchingAlgorithm {
 public:
  SobelSkiaGoldMatchingAlgorithm(int max_different_pixels,
                                 int pixel_delta_threshold,
                                 int edge_threshold,
                                 int ignored_border_thickness = 0);
  ~SobelSkiaGoldMatchingAlgorithm() override;

  void AppendAlgorithmToCmdline(base::CommandLine& cmd) const override;

 protected:
  std::string GetCommandLineSwitchName() const override;

 private:
  int edge_threshold_{0};
};

// With this algorithm, an image is regarded to match a golden image when:
//   1. the image is an exact match, and
//   2. the set of golden images for the test must only contain a single image.
// This algorithm is used in Ash pixel testing to facilitate the gold image
// revision update.
// The main difference between this and `ExactSkiaGoldMatchingAlgorithm` is:
// * ExactSkiaGoldMatchingAlgorithm
//   Your test can match multiple valid Gold images. For a test run, if the
//   pixel output is one of the Gold images, we consider it's valid and test
//   would pass.
//   Any committers can approve a new Gold image. The new Gold image would be
//   added to the valid Gold images set.
//
// * PositiveIfOnlyImageAlgorithm
//   There is only 1 valid Gold image. No one can approve a new Gold image if
//   there is already a Gold image. In order to updating the Gold image, you
//   need to add a revision to the screenshot name, so it's treated as a new
//   test from Gold's perspective.
//
// Say you have a button pixel test with screenshot name `pwd_manager_button1`.
// With ExactSkiaGoldMatchingAlgorithm:
//   When a cl updates all button styles, the cl owner may approve all new
//   styles via Skia Gold, gets a button style owner to approve it, and merges
//   the change.
//   In this case, the feature owner(pwd manager UI owner) would not get
//   notified. If you as feature owner don't want this behavior, you can use
// PositiveIfOnlyImageAlgorithm:
//   Your screenshot name would be `pwd_manager_button1.rev0`. When a cl updates
//   all button styles, the cl owner need to edit your test case, change the
//   screenshot name to `pwd_manager_button1.rev1`, and seek feature owner's
//   approval for the test case change. In this way, feature owner would know
//   the UI changes.
//
// This algorithm would increase the Gold image maintenance cost. Toolkit owners
// who regularly update styles might be hindered as they need to edit all test
// cases. So use your best judgement for the algorithm you choose.
class PositiveIfOnlyImageAlgorithm : public SkiaGoldMatchingAlgorithm {
 public:
  PositiveIfOnlyImageAlgorithm();
  ~PositiveIfOnlyImageAlgorithm() override;

 private:
  // SkiaGoldMatchingAlgorithm:
  void AppendAlgorithmToCmdline(base::CommandLine& cmd) const override;
  std::string GetCommandLineSwitchName() const override;
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SKIA_GOLD_MATCHING_ALGORITHM_H_
