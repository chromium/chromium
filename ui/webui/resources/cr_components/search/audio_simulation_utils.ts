// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface Bump {
  startTime: number;
  duration: number;
  maxVol: number;
}

const VOWEL_GROUP_EXCEPTIONS = [
  /iu/,  // Like in "chromium".
  /eo/,  // Like in "stereo".
  /ia/,  // Like in "dial", "media".
];

/* Fastest speakers speak 10-12 syllables/sec. Average is 4-5;
 * accounting for latency, make it 7.5 -> 8 frames per syllable.
 * using 10 -> ~0.167 syllable/frame -> 6 frames per syllable.
 */
export const MIN_FRAMES_PER_SYLLABLE: number = 6;
export const MAX_FRAMES_PER_SYLLABLE: number = 8;

// -12 frames = ~200ms compensation due to speech webkit latency.
export const FRAME_LATENCY: number = -12;
export const CIRCLE_RAD: number = Math.PI * 2;
export const MS_PER_FRAME: number = 16.67;
export const AMPLITUDE_DECAY_RATE: number = 0.85;

// Simulated volume spike bump settings for speech received state.
export const SPEECH_RECEIVED_BUMP_DURATION_MULT: number = 15;
export const SPEECH_RECEIVED_BUMP_DURATION_OFFSET: number = 25;
export const SPEECH_RECEIVED_BUMP_MAX_VOL_MULT: number = 0.14;
export const SPEECH_RECEIVED_BUMP_MAX_VOL_OFFSET: number = 0.05;

// Simulated volume bump settings for individual spoken syllables.
export const SYLLABLE_BUMP_DURATION_MULT: number = 25;
export const SYLLABLE_BUMP_DURATION_OFFSET: number = 15;
export const SYLLABLE_BUMP_MAX_VOL_MULT: number = 0.12;
export const SYLLABLE_BUMP_MAX_VOL_OFFSET: number = 0.08;

export function clamp(value: number, minVal: number, maxVal: number): number {
  return Math.min(Math.max(value, minVal), maxVal);
}

export function bezierEasing(
    controlX1: number, controlX2: number, timeProgress: number): number {
  /*
   * Solve a Cubic Bezier curve for a specific time "t":
   * Used to make the wave sensitivity non-linear (reacts more to soft
   * sounds). Standard Newton-Raphson implementation.
   */
  if (timeProgress <= 0) {
    return 0;
  }
  if (timeProgress >= 1) {
    return 1;
  }
  if (controlX1 === 0 && controlX2 === 1) {
    return timeProgress;
  }

  let currentT = timeProgress;
  // Pre-calculate coefficients
  const coeffA = 3 * controlX1;
  const coeffB = 3 * (controlX2 - controlX1) - coeffA;
  const coeffC = 1 - coeffA - coeffB;

  // Newton-Raphson iteration to find t for a given x (`timeProgress`)
  for (let i = 0; i < 8; i++) {
    const currentX =
        ((coeffC * currentT + coeffB) * currentT + coeffA) * currentT;
    if (Math.abs(currentX - timeProgress) < 1e-6) {
      break;
    }
    const currentSlope =
        (3 * coeffC * currentT + 2 * coeffB) * currentT + coeffA;
    if (Math.abs(currentSlope) < 1e-6) {
      break;
    }
    currentT -= (currentX - timeProgress) / currentSlope;
  }
  return 3 * currentT * currentT - 2 * currentT * currentT * currentT;
}

// Heuristic based on number of vowel groups, minus edge cases.
export function countSyllablesHeuristic(word: string): number {
  let count = 0;
  word = word.toLocaleLowerCase();
  if (word.length === 0) {
    return 0;
  }
  // Words of length 3 are usually 1 syllable.
  if (word.length <= 3) {
    return 1;
  }

  /* Remove silent 'e', 'es', 'ed' at end, as long as it's not '-ted' or '-ded'.
   * Don't take non-t/non-d in '-[x]ed'.
   */
  word = word.replace(/(?:[^laeiouy]es|(?<=[^td])ed|[^laeiouy]e)$/, '');

  // Remove leading 'y'; it's never a "vowel" like middle y's are.
  word = word.replace(/^y/, '');

  /* Count vowel groups; either 1 or 2 in a row.
   * 2 vowels in row diphthong. Vowels includes y in
   * middle of words. 3 vowel diphthongs excluded for simplicity.
   */
  const vowelGroups = word.match(/[aeiouy]{1,2}/g);

  // Count diphthong exceptions.
  VOWEL_GROUP_EXCEPTIONS.forEach((pattern) => {
    if (pattern.test(word)) {
      count++;
    }
  });

  return vowelGroups ? vowelGroups.length + count : 1 + count;
}

/* Let `bumps` be a queue ordered by start time.
 * Because of varying duration, it is not strictly ordered by
 * end time; only start time. Cannot assume deletion will only
 * happen at the start of the queue due to varying end times. Only
 * write to the index for the next function call if the duration of the sound is
 * not over. Stale bumps are removed via in-place compaction, truncating
 * the array length to the final write index.
 */
export function updateBumpsAndGetSum(
    bumps: Bump[], currentTime: number): number {
  let sum = 0;
  let writeIndex = 0;
  for (let i = 0; i < bumps.length; i++) {
    const bump = bumps[i]!;
    const relativeTime = currentTime - bump.startTime;
    const progress = relativeTime / bump.duration;
    if (progress >= 1) {
      continue;
    }
    if (currentTime >= bump.startTime) {
      /* Multiply by max volume and cos allows for mountain, not
       * slide down (just pure recursive decay). Vertical shift so
       * bottom is 0, then shrink so top is 1.
       * 1 - () to invert and start at 0.
       * Only start if it is not in future.
       */
      let addition =
          (1 - (1 + Math.cos(progress * CIRCLE_RAD)) * 0.5) * bump.maxVol;
      addition = clamp(addition, 0, 1);
      sum += addition;
    }
    bumps[writeIndex] = bump;
    writeIndex++;
  }
  bumps.length = writeIndex;
  return sum;
}

export function makeSimulatedAudioBump(
    bumps: Bump[], durationMultiplier: number, durationOffset: number,
    startTime: number, maxVolMultiplier: number, maxVolOffset: number): void {
  bumps.push({
    duration: Math.random() * durationMultiplier +
        durationOffset,    // In frames @60fps.
    startTime: startTime,  // Can be in future.
    maxVol: maxVolOffset + Math.random() * maxVolMultiplier,
  });
}

export function triggerSyllableBumps(
    bumps: Bump[], words: string[], currentVirtualFrame: number,
    firstSyllable: boolean): {firstSyllable: boolean} {
  /* Keeps every start time after this time (maintains order of time
   * in activeSimulatedBumps_). Start in "future" to account for latency in
   * speech recognition webkit.
   */
  let frameOffset = FRAME_LATENCY;

  words.forEach(word => {
    const syllableCount = countSyllablesHeuristic(word);
    for (let i = 0; i < syllableCount; i++) {
      /* If not first syllable; else, if first syllable already
       * counted by change in speechReceived state, ignore
       * to avoid double counting
       */
      if (!firstSyllable) {
        makeSimulatedAudioBump(
            bumps, SYLLABLE_BUMP_DURATION_MULT, SYLLABLE_BUMP_DURATION_OFFSET,
            currentVirtualFrame + frameOffset, SYLLABLE_BUMP_MAX_VOL_MULT,
            SYLLABLE_BUMP_MAX_VOL_OFFSET);
        /* At least min frames, up to slower end of
         * average frames per syllable:
         */
        frameOffset += MIN_FRAMES_PER_SYLLABLE +
            ((MAX_FRAMES_PER_SYLLABLE - MIN_FRAMES_PER_SYLLABLE) *
             Math.random());
      } else {
        firstSyllable = false;
      }
    }

    // Breath gap:
    frameOffset += 2;
  });
  return {firstSyllable};
}

/**
 * Calculates ambient simulated breathing wave (fast wave + slow wave + random
 * noise floor) with a smooth start ramp over the first 80 frames.
 */
export function getAmbientSimulatedMotion(frame: number): number {
  // Start at 0, then ramp up physics during first 80 frames:
  const startRamp = Math.min(1, frame / 80);

  let ambientSimulatedMotion =  // Fast wave:
      0.01 + (1 + Math.cos((frame / 60) * CIRCLE_RAD)) * 0.05;
  ambientSimulatedMotion *=  // Slow wave:
      0.25 + (1 + Math.cos((frame / 400) * CIRCLE_RAD)) * 0.2 * startRamp;
  // Random noise floor like live mic (random increase):
  ambientSimulatedMotion += 0.01 * Math.random();
  ambientSimulatedMotion *= startRamp;

  return ambientSimulatedMotion;
}

/**
 * Applies standard non-linear sensitivity curve (Bézier easing with control
 * points 0.4, 0.6) so wave reacts more dynamically to soft sounds.
 */
export function applySensitivityEasing(level: number): number {
  return bezierEasing(/*controlX1=*/ 0.4, /*controlX2=*/ 0.6, level);
}
