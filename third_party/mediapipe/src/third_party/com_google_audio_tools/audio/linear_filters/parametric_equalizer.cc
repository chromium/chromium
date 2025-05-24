/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/linear_filters/parametric_equalizer.h"

#include "audio/dsp/decibels.h"
#include "audio/dsp/nelder_mead_searcher.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Cholesky"
#include "third_party/eigen3/Eigen/Core"

namespace linear_filters {

using ::Eigen::ArrayXf;
using ::Eigen::Map;
using ::Eigen::MatrixXf;
using ::audio_dsp::AmplitudeRatioToDecibels;

namespace {

// Helper functions for marshalling and un-marshalling scalar parameters from
// ParametricEqualizerParams to flat Eigen::ArrayXf and vice versa.

// This function is a helper for handling the "regular" part of a
// ParametricEqualizerParams proto. It does not set the DC gain term, i.e.
// eq_params.gain_db, which is handled separately in SetGainsFromArray() below.
void SetEqParamsFromArray(
    const ArrayXf& array,
    const std::function<void(float, EqualizerFilterParams*)>& set_fn,
    ParametricEqualizerParams* eq_params) {
  int current_stage = 0;
  for (int i = 0; i < eq_params->GetTotalNumStages(); ++i) {
    set_fn(array(current_stage++), eq_params->MutableStageParams(i));
  }
}

void SetGainsFromArray(const ArrayXf& gains_db,
                       ParametricEqualizerParams* eq_params) {
  ABSL_CHECK_EQ(gains_db.size(), eq_params->GetTotalNumStages() + 1);
  SetEqParamsFromArray(gains_db.head(gains_db.size() - 1),
                       [](float gain_db, EqualizerFilterParams* filter_stage) {
                         filter_stage->gain_db = gain_db;
                       },
                       eq_params);
  // Handle the extra gain factor here.
  eq_params->SetGainDb(gains_db(gains_db.size() - 1));
}

void SetFrequenciesFromArray(const ArrayXf& log_frequencies_hz,
                             ParametricEqualizerParams* eq_params) {
  ABSL_CHECK_EQ(log_frequencies_hz.size(),
           eq_params->GetTotalNumStages());
  SetEqParamsFromArray(
      log_frequencies_hz.exp(),
      [](float frequency_hz, EqualizerFilterParams* filter_stage) {
        filter_stage->frequency_hz = frequency_hz;
      },
      eq_params);
}

void SetQualityFactorsFromArray(const ArrayXf& quality_factors,
                                ParametricEqualizerParams* eq_params) {
  ABSL_CHECK_EQ(quality_factors.size(),
           eq_params->GetTotalNumStages());
  SetEqParamsFromArray(quality_factors,
                       [](float quality_factor, EqualizerFilterParams* stage) {
                         stage->quality_factor = quality_factor;
                       },
                       eq_params);
}

void SetFrequenciesAndQualityFactorsFromArray(
    const ArrayXf& frequencies_and_quality_factors,
    ParametricEqualizerParams* eq_params) {
  const int half_size = eq_params->GetTotalNumStages();
  ABSL_CHECK_EQ(frequencies_and_quality_factors.size(), 2 * half_size);
  SetFrequenciesFromArray(frequencies_and_quality_factors.head(half_size),
                          eq_params);
  SetQualityFactorsFromArray(frequencies_and_quality_factors.tail(half_size),
                             eq_params);
}

// This function is a helper for handling the "regular" part of a
// ParametricEqualizerParams proto. It does not get the DC gain term, i.e.
// eq_params.gain_db(), which is handled separately in GetGains() below.
ArrayXf GetArrayFromEqParams(
    const ParametricEqualizerParams& eq_params,
    const std::function<float(const EqualizerFilterParams&)>& get_fn) {
  ArrayXf array(eq_params.GetTotalNumStages());
  int current_stage = 0;
  for (int i = 0; i < eq_params.GetTotalNumStages(); ++i) {
    array(current_stage++) = get_fn(eq_params.StageParams(i));
  }
  return array;
}

ArrayXf GetGains(const ParametricEqualizerParams& eq_params) {
  ArrayXf gains_db(eq_params.GetTotalNumStages() + 1);
  gains_db.head(eq_params.GetTotalNumStages()) =
      GetArrayFromEqParams(eq_params, [](const EqualizerFilterParams& stage) {
        return stage.gain_db;
      });
  // Handle the extra gain factor here.
  gains_db(gains_db.size() - 1) = eq_params.GetGainDb();
  return gains_db;
}

ArrayXf GetLogFrequencies(const ParametricEqualizerParams& eq_params) {
  const ArrayXf frequencies_hz = GetArrayFromEqParams(
      eq_params,
      [](const EqualizerFilterParams& stage) { return stage.frequency_hz; });
  return frequencies_hz.log();
}

ArrayXf GetQualityFactors(const ParametricEqualizerParams& eq_params) {
  return GetArrayFromEqParams(
      eq_params,
      [](const EqualizerFilterParams& stage) { return stage.quality_factor; });
}

// Returns an array of all log-frequencies followed by all quality factors.
ArrayXf GetLogFrequenciesAndQualityFactors(
    const ParametricEqualizerParams& eq_params) {
  ArrayXf log_frequencies_and_quality_factors(
      2 * eq_params.GetTotalNumStages());
  log_frequencies_and_quality_factors.head(
      eq_params.GetTotalNumStages()) =
      GetLogFrequencies(eq_params);
  log_frequencies_and_quality_factors.tail(
      eq_params.GetTotalNumStages()) =
      GetQualityFactors(eq_params);
  return log_frequencies_and_quality_factors;
}

}  // namespace

BiquadFilterCascadeCoefficients ParametricEqualizerParams::GetCoefficients(
    float sample_rate_hz) const {
  ABSL_CHECK_GT(sample_rate_hz, 0);

  auto DbToLinear = [](float gain_db) {
    return std::pow(10.0, gain_db / 20.0);
  };

  BiquadFilterCascadeCoefficients cascade_coefficients;
  for (int i = 0; i < GetTotalNumStages(); ++i) {
    if (IsStageEnabled(i)) {
      auto& stage = StageParams(i);
      switch (stage.type) {
        case EqualizerFilterParams::kLowpass:
          cascade_coefficients.AppendBiquad(LowpassBiquadFilterCoefficients(
              sample_rate_hz, stage.frequency_hz, stage.quality_factor));
          break;
        case EqualizerFilterParams::kLowShelf:
          cascade_coefficients.AppendBiquad(LowShelfBiquadFilterCoefficients(
              sample_rate_hz, stage.frequency_hz, stage.quality_factor,
              DbToLinear(stage.gain_db)));
          break;
        case EqualizerFilterParams::kPeak:
          cascade_coefficients.AppendBiquad(
              ParametricPeakBiquadFilterSymmetricCoefficients(
                  sample_rate_hz, stage.frequency_hz, stage.quality_factor,
                  DbToLinear(stage.gain_db)));
          break;
        case EqualizerFilterParams::kHighShelf:
          cascade_coefficients.AppendBiquad(HighShelfBiquadFilterCoefficients(
              sample_rate_hz, stage.frequency_hz,
              stage.quality_factor, DbToLinear(stage.gain_db)));
          break;
        case EqualizerFilterParams::kHighpass:
          cascade_coefficients.AppendBiquad(HighpassBiquadFilterCoefficients(
              sample_rate_hz, stage.frequency_hz,
              stage.quality_factor));
          break;
      }
    } else {
      cascade_coefficients.AppendBiquad(BiquadFilterCoefficients());
    }
  }

  cascade_coefficients.AdjustGain(DbToLinear(GetGainDb()));
  return cascade_coefficients;
}

float ParametricEqualizerParams::GetGainDb() const { return gain_db_; }
void ParametricEqualizerParams::SetGainDb(float gain) { gain_db_ = gain; }

bool ParametricEqualizerParams::IsStageEnabled(int index) const {
  ABSL_DCHECK_LT(index, GetTotalNumStages());
  return stages_enabled_[index];
}
void ParametricEqualizerParams::SetStageEnabled(int index, bool enabled) {
  ABSL_DCHECK_LT(index, GetTotalNumStages());
  stages_enabled_[index] = enabled;
}
const EqualizerFilterParams& ParametricEqualizerParams::StageParams(
    int index) const {
  ABSL_DCHECK_LT(index, GetTotalNumStages());
  return all_stages_[index];
}
EqualizerFilterParams* ParametricEqualizerParams::MutableStageParams(
    int index) {
  ABSL_DCHECK_LT(index, GetTotalNumStages());
  return &all_stages_[index];
}

void ParametricEqualizerParams::AddStage(const EqualizerFilterParams& params) {
  all_stages_.push_back(params);
  stages_enabled_.push_back(true);
}

void ParametricEqualizerParams::AddStage(
    EqualizerFilterParams::Type type, float frequency_hz,
    float quality_factor, float gain_db) {
  all_stages_.emplace_back(type, frequency_hz, quality_factor, gain_db);
  stages_enabled_.push_back(true);
}

void ParametricEqualizerParams::ClearAllGains() {
  SetGainDb(0.0f);
  for (int i = 0; i < stages_enabled_.size(); ++i) {
    if (all_stages_[i].type == EqualizerFilterParams::kLowpass ||
        all_stages_[i].type == EqualizerFilterParams::kHighpass) {
      stages_enabled_[i] = false;
    }
    all_stages_[i].gain_db = 0;
  }
}

int ParametricEqualizerParams::GetNumEnabledStages() const {
  int num_stages = 0;
  for (uint8 enabled : stages_enabled_) {
    num_stages += enabled == true;
  }
  return num_stages;
}
int ParametricEqualizerParams::GetTotalNumStages() const {
  return all_stages_.size();
}

std::string ParametricEqualizerParams::ToString() const {
  std::string info = "{ ";
  for (int i = 0; i < GetTotalNumStages(); ++i) {
    if (!IsStageEnabled(i)) {
      info += "DISABLED ";
    }
    info += StageParams(i).ToString();
  }
  info += absl::StrFormat(" } gain_db: %f ", GetGainDb());
  return info;
}

// Based on Abel and Berners, "Filter Design Using Second-Order Peaking and
// Shelving Sections", 2004.

void SetParametricEqualizerGainsToMatchMagnitudeResponse(
    absl::Span<const float> frequencies_hz,
    absl::Span<const float> magnitude_target_db, ConvergenceParams params,
    float sample_rate_hz, ParametricEqualizerParams* eq_params) {
  ABSL_CHECK_EQ(frequencies_hz.size(), magnitude_target_db.size());
  // Start by setting all the gains to an arbitrary value that is not 0dB. We
  // assume that the filters are self-similar as their gains are changed. This
  // assumption is very reasonable (see figures in paper). Self-similarity in
  // this case means that when normalizing the range of the log transfer
  // function to the range (0, 1) it looks similar for filters designed with
  // different gains, approximately, per the referenced approach of Abel and
  // Berners.
  constexpr float kInitialGainDb = 6.0;
  const int num_filters_and_gain = 1 + eq_params->GetNumEnabledStages();
  // The self similarity property is not true for lowpass/highpass filters.
  for (int i = 0; i < eq_params->GetTotalNumStages(); ++i) {
    if (eq_params->StageParams(i).type == EqualizerFilterParams::kLowpass ||
        eq_params->StageParams(i).type == EqualizerFilterParams::kHighpass) {
      LOG(ERROR) << "Trying to fit equalizer with lowpass or highpass filters. "
          << "You shouldn't expect this to work well.";
    }
  }
  ArrayXf initial_gains_db =  // Plus 1 for gain term.
      ArrayXf::Constant(num_filters_and_gain, kInitialGainDb);
  SetGainsFromArray(initial_gains_db, eq_params);

  Map<const MatrixXf> target_db(magnitude_target_db.data(),
                                magnitude_target_db.size(), 1);
  MatrixXf stage_magnitudes_db(frequencies_hz.size(), num_filters_and_gain);

  // We can do additional iteration improving upon the assumption we made
  // above. Each iteration uses the gains from the previous pass so that the
  // computed transfer function, accounting for the shape change that the
  // filters experience as their gains are altered.
  for (int i = 0; i < params.max_iterations; ++i) {
    // BiquadFilterCascadeCoefficients will apply the gain to one of the stages
    // and cause that transfer function to shift. We will manage the gain
    // term ourselves.
    eq_params->SetGainDb(0);
    BiquadFilterCascadeCoefficients coeffs =
        eq_params->GetCoefficients(sample_rate_hz);
    // Number of frequency points x number of filter stages.
    for (int k = 0; k < stage_magnitudes_db.cols() - 1; ++k) {
      for (int m = 0; m < stage_magnitudes_db.rows(); ++m) {
        stage_magnitudes_db(m, k) =
            AmplitudeRatioToDecibels(coeffs[k].GainMagnitudeAtFrequency(
                frequencies_hz[m], sample_rate_hz));
      }
    }
    stage_magnitudes_db.rightCols(1).setConstant(
        initial_gains_db[stage_magnitudes_db.cols() - 1]);

    // Solve the linear system.
    // https://eigen.tuxfamily.org/dox-devel/group__LeastSquares.html
    //
    // TODO: We can give a perceptual weighting to the fit by
    // multiplying stage_magnitudes_db and target_db by a weighting matrix.
    MatrixXf gramian =
        stage_magnitudes_db.transpose() * stage_magnitudes_db +
        1e-4 * MatrixXf::Identity(num_filters_and_gain, num_filters_and_gain);
    ArrayXf gains_db = gramian.ldlt()
                           .solve(stage_magnitudes_db.transpose() * target_db)
                           .array();
    gains_db *= initial_gains_db;
    float largest_change_db = (gains_db - initial_gains_db).abs().maxCoeff();

    // Update the parameters with the solution weights.
    SetGainsFromArray(gains_db, eq_params);
    initial_gains_db = gains_db;
    // Break early if we have converged.
    if (largest_change_db < params.convergence_threshold_db) {
      break;
    }
  }
}

void SetParametricEqualizerGainsToMatchMagnitudeResponse(
    absl::Span<const float> frequencies_hz,
    absl::Span<const float> magnitude_target_db, float sample_rate_hz,
    ParametricEqualizerParams* eq_params) {
  SetParametricEqualizerGainsToMatchMagnitudeResponse(
      frequencies_hz, magnitude_target_db, ConvergenceParams(), sample_rate_hz,
      eq_params);
}

ArrayXf ParametricEqualizerGainMagnitudeAtFrequencies(
    const ParametricEqualizerParams& eq_params, float sample_rate_hz,
    const ArrayXf& frequencies_hz) {
  const BiquadFilterCascadeCoefficients coeffs =
      eq_params.GetCoefficients(sample_rate_hz);
  ArrayXf gains(frequencies_hz.size());
  // TODO: This could be much more efficient if
  // BiquadFilterCoefficients::EvalTransferFunction and friends, including
  // GainMagnitudeAtFrequency, had an Eigen interface.
  for (int i = 0; i < frequencies_hz.size(); ++i) {
    float combined_gain = 1.0f;
    for (int j = 0; j < coeffs.size(); ++j) {
      combined_gain *=
          coeffs[j].GainMagnitudeAtFrequency(frequencies_hz(i), sample_rate_hz);
    }
    gains(i) = combined_gain;
  }
  return gains;
}

float FitParametricEqualizer(absl::Span<const float> frequencies_hz,
                             absl::Span<const float> magnitude_target_db,
                             float sample_rate_hz,
                             const NelderMeadFitParams& fit_params,
                             ParametricEqualizerParams* eq_params) {
  const Eigen::Map<const ArrayXf> frequencies_hz_map(frequencies_hz.data(),
                                                     frequencies_hz.size());
  const Eigen::Map<const ArrayXf> magnitude_target_db_map(
      magnitude_target_db.data(), magnitude_target_db.size());

  const int num_stages = eq_params->GetTotalNumStages();
  audio_dsp::NelderMeadSearcher<float> searcher(2 * num_stages);

  // Set starting guess from the frequencies and quality factors given in
  // eq_params. NOTICE: We perform the non-linear optimization in log-frequency
  // space to improve the scaling of the problem (i.e. to make the scale of
  // log-frequency and quality factor parameters more similar). The Nelder-Mead
  // algorithm (and other zero- or first-order optimization methods) is prone
  // to converge slowly or even stagnate at a non-stationary point if the
  // problem is poorly scaled.
  const ArrayXf log_frequencies_and_quality_factors =
      GetLogFrequenciesAndQualityFactors(*eq_params);
  ABSL_CHECK_EQ(log_frequencies_and_quality_factors.size(), 2 * num_stages);
  searcher.SetSimplexFromStartingGuess(log_frequencies_and_quality_factors,
                                       0.1);

  // Set constraints.
  ABSL_CHECK_GT(fit_params.min_frequency_hz, 0);
  ABSL_CHECK_GT(fit_params.max_frequency_hz, 0);
  ABSL_CHECK_LT(fit_params.min_frequency_hz, fit_params.max_frequency_hz);
  ArrayXf bounds(2 * num_stages);
  bounds.head(num_stages) =
      ArrayXf::Constant(num_stages, std::log(fit_params.min_frequency_hz));
  bounds.tail(num_stages) =
      ArrayXf::Constant(num_stages, fit_params.min_quality_factor);
  searcher.SetLowerBounds(bounds);
  bounds.head(num_stages) =
      ArrayXf::Constant(num_stages, std::log(fit_params.max_frequency_hz));
  bounds.tail(num_stages) =
      ArrayXf::Constant(num_stages, fit_params.max_quality_factor);
  searcher.SetUpperBounds(bounds);
  VLOG(1) << "Initial simplex =\n" << searcher.GetSimplex();

  // The following lambda implements the objective function. It estimates the
  // gains using the existing iterated linear least-squares method, and as a
  // side-effect updates the gains in eq_params, and returns the corresponding
  // RMS error used to drive the optimization of corner frequencies and quality
  // factors.
  auto magnitude_db_rms_error =
      [&, sample_rate_hz](
          const Eigen::Ref<const ArrayXf>& frequency_and_quality_factors) {
        // 1. Set up EQ params.
        SetFrequenciesAndQualityFactorsFromArray(frequency_and_quality_factors,
                                                 eq_params);
        // 2. Compute optimal gains from linear least-squares fit.
        SetParametricEqualizerGainsToMatchMagnitudeResponse(
            frequencies_hz, magnitude_target_db,
            fit_params.inner_convergence_params, sample_rate_hz, eq_params);
        // 3. Evaluate gains of the computed EQ filter at frequencies_hz.
        ArrayXf magnitude_response_db(magnitude_target_db.size());
        AmplitudeRatioToDecibels(
            ParametricEqualizerGainMagnitudeAtFrequencies(
                *eq_params, sample_rate_hz, frequencies_hz_map),
            &magnitude_response_db);
        // 4. Compute filter response and its magnitude RMS error w.r.t. the
        // desired target response.
        auto magnitude_residual_db =
            magnitude_response_db - magnitude_target_db_map;
        const float magnitude_residual_db_rms =
            std::sqrt(magnitude_residual_db.square().sum() /
                      magnitude_residual_db.size());
        return magnitude_residual_db_rms;
      };
  searcher.SetObjectiveFunction(magnitude_db_rms_error);
  searcher.MinimizeObjective(fit_params.max_iterations,
                             fit_params.magnitude_db_rms_error_stddev_tol,
                             fit_params.magnitude_db_rms_error_tol);
  VLOG(1) << "num_evaluations = " << searcher.num_evaluations();
  VLOG(1) << "Final simplex =\n" << searcher.GetSimplex();
  // Finally call the objective function at the minimum point to set all
  // values in eq_params to their optimimal value.
  return magnitude_db_rms_error(searcher.GetArgMin());
}

}  // namespace linear_filters
