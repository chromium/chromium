/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow_lite_support/cc/task/core/score_calibration.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "absl/types/optional.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace task {
namespace core {
namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

// Used to prevent log(<=0.0) in ClampedLog() calls.
constexpr float kLogScoreMinimum = 1e-16;

// Returns the following, depending on x:
//   x => threshold: log(x)
//   x < threshold: 2 * log(thresh) - log(2 * thresh - x)
// This form (a) is anti-symmetric about the threshold and (b) has continuous
// value and first derivative. This is done to prevent taking the log of values
// close to 0 which can lead to floating point errors and is better than simple
// clamping since it preserves order for scores less than the threshold.
float ClampedLog(float x, float threshold) {
  if (x < threshold) {
    return 2.0 * std::log(static_cast<double>(threshold)) -
           log(2.0 * threshold - x);
  }
  return std::log(static_cast<double>(x));
}

// Applies the specified score transformation to the provided score.
// Currently supports the following,
//   IDENTITY         : f(x) = x
//   LOG              : f(x) = log(x)
//   INVERSE_LOGISTIC : f(x) = log(x) - log(1-x)
float ApplyScoreTransformation(float score, const ScoreTransformation& type) {
  switch (type) {
    case ScoreTransformation::kIDENTITY:
      return score;
    case ScoreTransformation::kINVERSE_LOGISTIC:
      return (ClampedLog(score, kLogScoreMinimum) -
              ClampedLog(1.0 - score, kLogScoreMinimum));
    case ScoreTransformation::kLOG:
      return ClampedLog(score, kLogScoreMinimum);
  }
}

// Builds a single Sigmoid from the label name and associated CSV file line.
StatusOr<Sigmoid> SigmoidFromLabelAndLine(absl::string_view label,
                                          absl::string_view line) {
  std::vector<absl::string_view> str_params = absl::StrSplit(line, ',');
  if (str_params.size() != 3 && str_params.size() != 4) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected 3 or 4 parameters per line in score "
                        "calibration file, got %d.",
                        str_params.size()),
        TfLiteSupportStatus::kMetadataMalformedScoreCalibrationError);
  }
  std::vector<float> float_params(4);
  for (int i = 0; i < str_params.size(); ++i) {
    if (!absl::SimpleAtof(str_params[i], &float_params[i])) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "Could not parse score calibration parameter as float: %s.",
              str_params[i]),
          TfLiteSupportStatus::kMetadataMalformedScoreCalibrationError);
    }
  }
  Sigmoid sigmoid;
  sigmoid.label = std::string(label);
  sigmoid.scale = float_params[0];
  sigmoid.slope = float_params[1];
  sigmoid.offset = float_params[2];
  if (str_params.size() == 4) {
    sigmoid.min_uncalibrated_score = float_params[3];
  }
  return sigmoid;
}

// Converts a tflite::ScoreTransformationType to its
// tflite::task::vision::ScoreTransformation equivalent.
ScoreTransformation ConvertScoreTransformationType(
    tflite::ScoreTransformationType type) {
  switch (type) {
    case tflite::ScoreTransformationType_IDENTITY:
      return ScoreTransformation::kIDENTITY;
    case tflite::ScoreTransformationType_LOG:
      return ScoreTransformation::kLOG;
    case tflite::ScoreTransformationType_INVERSE_LOGISTIC:
      return ScoreTransformation::kINVERSE_LOGISTIC;
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Sigmoid& s) {
  os << s.label << "," << s.slope << "," << s.offset << "," << s.scale;
  if (s.min_uncalibrated_score.has_value()) {
    os << "," << s.min_uncalibrated_score.value();
  }
  return os;
}

ScoreCalibration::ScoreCalibration() {}
ScoreCalibration::~ScoreCalibration() {}

absl::Status ScoreCalibration::InitializeFromParameters(
    const SigmoidCalibrationParameters& params) {
  sigmoid_parameters_ = std::move(params);
  // Fill in the map from label -> sigmoid.
  sigmoid_parameters_map_.clear();
  for (const auto& sigmoid : sigmoid_parameters_.sigmoid) {
    sigmoid_parameters_map_.insert_or_assign(sigmoid.label, sigmoid);
  }
  return absl::OkStatus();
}

float ScoreCalibration::ComputeCalibratedScore(const std::string& label,
                                               float uncalibrated_score) const {
  absl::optional<Sigmoid> sigmoid = FindSigmoidParameters(label);
  if (!sigmoid.has_value() ||
      (sigmoid.value().min_uncalibrated_score.has_value() &&
       uncalibrated_score < sigmoid.value().min_uncalibrated_score.value())) {
    return sigmoid_parameters_.default_score;
  }

  float transformed_score = ApplyScoreTransformation(
      uncalibrated_score, sigmoid_parameters_.score_transformation);
  float scale_shifted_score =
      transformed_score * sigmoid.value().slope + sigmoid.value().offset;

  // For numerical stability use 1 / (1+exp(-x)) when scale_shifted_score >= 0
  // and exp(x) / (1+exp(x)) when scale_shifted_score < 0.
  if (scale_shifted_score >= 0.0) {
    return sigmoid.value().scale /
           (1.0 + std::exp(static_cast<double>(-scale_shifted_score)));
  } else {
    float score_exp = std::exp(static_cast<double>(scale_shifted_score));
    return sigmoid.value().scale * score_exp / (1.0 + score_exp);
  }
}

absl::optional<Sigmoid> ScoreCalibration::FindSigmoidParameters(
    const std::string& label) const {
  auto it = sigmoid_parameters_map_.find(label);
  if (it != sigmoid_parameters_map_.end()) {
    return it->second;
  } else if (sigmoid_parameters_.default_sigmoid.has_value()) {
    return sigmoid_parameters_.default_sigmoid.value();
  }
  return absl::nullopt;
}

StatusOr<SigmoidCalibrationParameters> BuildSigmoidCalibrationParams(
    const tflite::ScoreCalibrationOptions& score_calibration_options,
    absl::string_view score_calibration_file,
    const std::vector<LabelMapItem>& label_map_items) {
  // Split file lines and perform sanity checks.
  if (score_calibration_file.empty()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Expected non-empty score calibration file.");
  }
  std::vector<absl::string_view> lines =
      absl::StrSplit(score_calibration_file, '\n');
  if (label_map_items.size() != lines.size()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Mismatch between number of labels (%d) and score "
                        "calibration parameters (%d).",
                        label_map_items.size(), lines.size()),
        TfLiteSupportStatus::kMetadataNumLabelsMismatchError);
  }
  // Initialize SigmoidCalibrationParameters with its class-agnostic parameters.
  SigmoidCalibrationParameters sigmoid_params = {};
  sigmoid_params.score_transformation = ConvertScoreTransformationType(
      score_calibration_options.score_transformation());
  sigmoid_params.default_score = score_calibration_options.default_score();
  std::vector<Sigmoid> sigmoid_vector;
  // Fill sigmoids for each class with parameters in the file.
  for (int i = 0; i < label_map_items.size(); ++i) {
    if (lines[i].empty()) {
      continue;
    }
    TFLITE_ASSIGN_OR_RETURN(Sigmoid sigmoid, SigmoidFromLabelAndLine(
                                          label_map_items[i].name, lines[i]));
    sigmoid_vector.emplace_back(std::move(sigmoid));
  }
  sigmoid_params.sigmoid = std::move(sigmoid_vector);

  return sigmoid_params;
}

}  // namespace core
}  // namespace task
}  // namespace tflite
