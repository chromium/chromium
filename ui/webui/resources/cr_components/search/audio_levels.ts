// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Size of the history buffer for smoothing.
const HISTORY_SIZE = 20;
// Number of previous values to average with the current value for smoothing.
const SMOOTHING_WINDOW = 2;
// Minimum frequency bin index for speech band (~109Hz at 16000Hz sample rate).
const SPEECH_BAND_MIN_BIN = 7;
// Maximum frequency bin index for speech band (~1094Hz at 16000Hz sample rate).
const SPEECH_BAND_MAX_BIN = 70;
// Initial noise level estimate in linear scale (corresponding to -60 dBFS).
const INITIAL_NOISE_LEVEL = 0.001;
// Minimum threshold for speech level to be emitted.
const MIN_AUDIO_LEVEL_THRESHOLD = 0.1;
// Minimum SNR in dB to trigger visualization.
const MIN_SNR_DB = 5;
// Maximum SNR in dB for full visualization height.
const MAX_SNR_DB = 30;
// Adaptation factor applied when tracking background noise upward.
const NOISE_INCREASE_SMOOTHING = 0.001;
// Adaptation factor applied when tracking background noise downward.
const NOISE_DECREASE_SMOOTHING = 0.1;

// Calculates the RMS (Root Mean Square) of the frequency band corresponding to
// human speech. Note: Since the input buffer contains logarithmic values in
// decibels (dB), first convert them back to linear amplitude values
// before summing their squares.
function calculateBandLimitedRMS(buffer: Float32Array): number {
  const n = SPEECH_BAND_MAX_BIN - SPEECH_BAND_MIN_BIN + 1;
  let sumSquares = 0;
  for (let i = SPEECH_BAND_MIN_BIN; i <= SPEECH_BAND_MAX_BIN; i++) {
    let db = buffer[i]!;
    if (db < -100) {
      db = -100;  // Clamp -Infinity
    }
    const linear = Math.pow(10, db / 20);
    sumSquares += linear * linear;
  }
  return Math.sqrt(sumSquares / n);
}

// Calculates a normalized speech level based on the Signal-to-Noise Ratio.
// `noiseLevel`: The current estimated background noise level.
// `rms`: The current frame's Root Mean Square amplitude.
// What is returned: A normalized speech level in the range [0, 1].
function calculateSpeechLevel(noiseLevel: number, rms: number): number {
  if (noiseLevel > 0 && rms / noiseLevel > 0.000001) {
    const snrDb = 20 * Math.log10(rms / noiseLevel);
    const clampedSnrDb = Math.min(Math.max(snrDb, MIN_SNR_DB), MAX_SNR_DB);
    return (clampedSnrDb - MIN_SNR_DB) / (MAX_SNR_DB - MIN_SNR_DB);
  }
  return 0;
}

// Solves a 1D cubic Bezier equation using Newton-Raphson approximation.
// Given control points x1 and x2, and a target value v, it finds the parameter
// t such that x(t) = v. This is used for easing the audio level curve
// dynamically. `x1`: The first Bezier control point (recent historical
// minimum). `x2`: The second Bezier control point (recent historical maximum).
// `v`: The target value to solve for.
// What is returned: The parameter t in the range [0, 1].
function solveCubic(x1: number, x2: number, v: number): number {
  if (v <= 0 || v >= 1) {
    return v;
  }

  const epsilon = 1e-6;
  const maxUpdates = 8;

  const c = 3 * x1;
  const b = 3 * (x2 - x1) - c;
  const a = 1 - c - b;

  let t = v;
  for (let i = 0; i < maxUpdates; i++) {
    const result = t * (t * (t * a + b) + c) - v;
    const slope = t * (3 * t * a + 2 * b) + c;
    if (slope < epsilon) {
      break;
    }

    const diff = result / slope;
    if (Math.abs(diff) < epsilon) {
      break;
    }

    t -= diff;
  }

  return t;
}

// Processes audio levels for visualization from a time domain audio buffer.
export class AudioLevelsProcessor {
  private noiseLevel = INITIAL_NOISE_LEVEL;
  // Tracks post-shaped values solely to define min/max bounds for Bezier
  // shaping.
  private readonly history: number[] = new Array(HISTORY_SIZE).fill(0);
  // Tracks raw inputs exclusively for smoothing to prevent recursive feedback
  // loops.
  private readonly rawHistory: number[];
  private accumulatedRmsSum = 0;
  private accumulatedRmsCount = 0;
  private lastUpdateTime: number|null = null;
  private lastEmittedValue = 0;
  private readonly sampleIntervalMs = 60;

  constructor(private readonly dynamicShapingCurveMinSecondValue = 0.3) {
    this.rawHistory = new Array(SMOOTHING_WINDOW).fill(0);
  }

  // Processes a frequency domain audio buffer and returns a normalized speech
  // level in [0, 1].
  process(buffer: Float32Array, nowMs: number): number {
    const rms = calculateBandLimitedRMS(buffer);
    return this.accumulateRmsAndProcessInterval(rms, nowMs);
  }

  private accumulateRmsAndProcessInterval(rms: number, nowMs: number): number {
    this.accumulatedRmsSum += rms;
    this.accumulatedRmsCount++;

    if (this.lastUpdateTime === null) {
      this.lastUpdateTime = nowMs - this.sampleIntervalMs;
    }

    if (nowMs - this.lastUpdateTime < this.sampleIntervalMs) {
      return this.lastEmittedValue;
    }

    const averageRms = this.accumulatedRmsSum / this.accumulatedRmsCount;
    this.accumulatedRmsSum = 0;
    this.accumulatedRmsCount = 0;

    if (nowMs - this.lastUpdateTime > this.sampleIntervalMs * 2) {
      this.lastUpdateTime = nowMs;
    } else {
      this.lastUpdateTime += this.sampleIntervalMs;
    }

    return this.processAverageRms(averageRms);
  }

  private processAverageRms(averageRms: number): number {
    this.updateNoiseLevel(averageRms);
    const speechLevel = calculateSpeechLevel(this.noiseLevel, averageRms);
    const valueToEmit =
        speechLevel >= MIN_AUDIO_LEVEL_THRESHOLD ? speechLevel : 0;
    const smoothed = this.applySmoothing(valueToEmit);
    const clampedEased = this.applyDynamicShaping(smoothed);
    this.updateHistory(clampedEased);
    this.updateRawHistory(valueToEmit);
    this.lastEmittedValue = clampedEased;
    return clampedEased;
  }

  // Applies smoothing to the current value using the recent history.
  // `valueToEmit`: The raw calculated speech level for the current frame.
  // What is returned: The smoothed speech level.
  private applySmoothing(valueToEmit: number): number {
    let sum = 0;
    for (let i = 0; i < this.rawHistory.length; i++) {
      sum += this.rawHistory[i]!;
    }
    return (valueToEmit + sum) / (this.rawHistory.length + 1);
  }

  // Applies a dynamic shaping curve (cubic easing) based on the recent min/max
  // history values. This automatically scales and normalizes the visualization
  // based on the user's recent speaking volume, making it look responsive
  // whether the user is whispering or speaking loudly.
  // `smoothedValue`: The smoothed speech level to be shaped.
  // What is returned: The eased and clamped speech level.
  private applyDynamicShaping(smoothedValue: number): number {
    const max = Math.max(
        ...this.history,
        this.dynamicShapingCurveMinSecondValue,
    );
    const min = Math.min(...this.history);

    const t = solveCubic(min, max, smoothedValue);
    const eased = t * t * (3 - 2 * t);
    return Math.max(0, Math.min(1, eased));
  }

  // Updates the history buffer with the newly calculated value.
  // `newValue`: The newest normalized speech level to add to history.
  private updateHistory(newValue: number): void {
    this.history.shift();
    this.history.push(newValue);
  }

  // Updates the raw history buffer used for smoothing.
  private updateRawHistory(newValue: number): void {
    this.rawHistory.shift();
    this.rawHistory.push(newValue);
  }

  // Adaptively tracks background noise levels using an envelope follower with
  // asymmetric smoothing. When signal volume rises above estimated noise
  // (speech), it adapts very slowly (NOISE_INCREASE_SMOOTHING) to avoid
  // tracking speech as noise. When it drops below (silence), it adapts quickly
  // (NOISE_DECREASE_SMOOTHING) to establish the baseline floor. `rms`: The
  // current frame's Root Mean Square amplitude.
  private updateNoiseLevel(rms: number): void {
    if (this.noiseLevel < rms) {
      this.noiseLevel = (1 - NOISE_INCREASE_SMOOTHING) * this.noiseLevel +
          NOISE_INCREASE_SMOOTHING * rms;
    } else {
      this.noiseLevel = (1 - NOISE_DECREASE_SMOOTHING) * this.noiseLevel +
          NOISE_DECREASE_SMOOTHING * rms;
    }
  }
}
