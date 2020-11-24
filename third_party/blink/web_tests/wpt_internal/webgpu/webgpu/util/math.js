/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../common/framework/util/util.js';
export function align(n, alignment) {
  return Math.ceil(n / alignment) * alignment;
}

export function isAligned(n, alignment) {
  return n === align(n, alignment);
}

export const kMaxSafeMultipleOf8 = Number.MAX_SAFE_INTEGER - 7;

export function clamp(n, min, max) {
  assert(max >= min);
  return Math.min(Math.max(n, min), max);
}
