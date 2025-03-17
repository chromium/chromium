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

#include "audio/dsp/attack_release_envelope.h"

#include "gtest/gtest.h"

#include "audio/linear_filters/discretization.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::linear_filters::FirstOrderCoefficientFromTimeConstant;

constexpr float kAttackSeconds = 5.0f;
constexpr float kReleaseSeconds = 0.2f;
constexpr float kSampleRateHz = 1000.0f;

TEST(AttackReleaseEnvelopeTest, ExponentialDecayTest) {
  AttackReleaseEnvelope envelope(
      kAttackSeconds, kReleaseSeconds, kSampleRateHz);

  int tau_samples = std::round(kSampleRateHz * kReleaseSeconds);
  const float alpha =
      FirstOrderCoefficientFromTimeConstant(kReleaseSeconds, kSampleRateHz);

  float first_sample = envelope.Output(1);  // An impulse.
  // Check for exponential decay.
  for (int i = 1; i < tau_samples; ++i) {
    ASSERT_NEAR(envelope.Output(0),
                first_sample * std::pow(1 - alpha, i), 1e-4);
  }
  // The time constant is correct.
  EXPECT_NEAR(envelope.Output(0), first_sample / std::exp(1), 1e-4);
}

TEST(AttackReleaseEnvelopeTest, StepFunctionTest) {
  AttackReleaseEnvelope envelope(
      kAttackSeconds, kReleaseSeconds, kSampleRateHz);

  int tau_samples = std::round(kSampleRateHz * kAttackSeconds);
  const float alpha =
      FirstOrderCoefficientFromTimeConstant(kAttackSeconds, kSampleRateHz);

  // Check for output exponentially approaching input.
  for (int i = 0; i < tau_samples; ++i) {
    // Use a step function.
    ASSERT_NEAR(1 - envelope.Output(1), std::pow(1 - alpha, i + 1), 1e-4);
  }
  // The time constant is correct.
  EXPECT_NEAR(1 - envelope.Output(1), 1.0 / std::exp(1), 1e-4);
}

TEST(AttackReleaseEnvelopeTest, ChangingAttackCoeffsTest) {
  AttackReleaseEnvelope envelope(
      kAttackSeconds, kReleaseSeconds, kSampleRateHz);
  int tau_samples = std::round(kSampleRateHz * kAttackSeconds);
  {
    const float alpha =
        FirstOrderCoefficientFromTimeConstant(kAttackSeconds, kSampleRateHz);

    for (int i = 0; i < tau_samples - 1; ++i) {
      ASSERT_NEAR(1 - envelope.Output(1), std::pow(1 - alpha, i + 1), 1e-4);
    }
    EXPECT_NEAR(1 - envelope.Output(1), 1.0 / std::exp(1), 1e-4);
  }

  float kNewSmallerAttackSeconds = kAttackSeconds / 10.0f;
  envelope.SetAttackTimeSeconds(kNewSmallerAttackSeconds);
  // Burn off all of the state and allow the new time constant to settle.
  int release_tau_samples = std::round(kSampleRateHz * kReleaseSeconds);
  for (int i = 0; i < 10 * release_tau_samples; ++i) {
    envelope.Output(0);
  }

  {
    int new_tau_samples = std::round(kSampleRateHz * kNewSmallerAttackSeconds);
    const float new_alpha = FirstOrderCoefficientFromTimeConstant(
        kNewSmallerAttackSeconds, kSampleRateHz);

    for (int i = 0; i < new_tau_samples - 1; ++i) {
      ASSERT_NEAR(1 - envelope.Output(1), std::pow(1 - new_alpha, i + 1), 1e-4);
    }
    EXPECT_NEAR(1 - envelope.Output(1), 1.0 / std::exp(1), 1e-3);
  }
}

TEST(AttackReleaseEnvelopeTest, ChangingReleaseCoeffsTest) {
  AttackReleaseEnvelope envelope(
      kReleaseSeconds, kReleaseSeconds, kSampleRateHz);
  int tau_samples = std::round(kSampleRateHz * kReleaseSeconds);

  // Set the state at 1 for convenience.
  int attack_tau_samples = std::round(kSampleRateHz * kAttackSeconds);
  for (int i = 0; i < 8 * attack_tau_samples; ++i) {
    envelope.Output(1);
  }
  {
    const float alpha =
        FirstOrderCoefficientFromTimeConstant(kReleaseSeconds, kSampleRateHz);

    for (int i = 0; i < tau_samples - 1; ++i) {
      ASSERT_NEAR(envelope.Output(0), std::pow(1 - alpha, i + 1), 1e-4);
    }
    EXPECT_NEAR(envelope.Output(0), 1.0 / std::exp(1), 1e-4);
  }

  float kNewSmallerReleaseSeconds = kReleaseSeconds / 10.0f;
  envelope.SetReleaseTimeSeconds(kNewSmallerReleaseSeconds);
  // Set the state back to one.
  for (int i = 0; i < 8 * attack_tau_samples; ++i) {
    envelope.Output(1);
  }

  {
    int new_tau_samples = std::round(kSampleRateHz * kNewSmallerReleaseSeconds);
    const float new_alpha = FirstOrderCoefficientFromTimeConstant(
        kNewSmallerReleaseSeconds, kSampleRateHz);

    for (int i = 0; i < new_tau_samples - 1; ++i) {
      ASSERT_NEAR(envelope.Output(0),
                  std::pow(1 - new_alpha, i + 1), 1e-4);
    }
    EXPECT_NEAR(envelope.Output(0), 1.0 / std::exp(1), 1e-3);
  }
}

}  // namespace
}  // namespace audio_dsp
