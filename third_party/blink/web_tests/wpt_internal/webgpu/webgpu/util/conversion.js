/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../common/framework/util/util.js';
import { clamp } from './math.js';

export function floatAsNormalizedInteger(float, bits, signed) {
  if (signed) {
    assert(float >= -1 && float <= 1);
    const max = Math.pow(2, bits - 1) - 1;
    return Math.round(float * max);
  } else {
    assert(float >= 0 && float <= 1);
    const max = Math.pow(2, bits) - 1;
    return Math.round(float * max);
  }
}

export function normalizedIntegerAsFloat(integer, bits, signed) {
  assert(Number.isInteger(integer));
  if (signed) {
    const max = Math.pow(2, bits - 1) - 1;
    assert(integer >= -max - 1 && integer <= max);
    if (integer === -max - 1) {
      integer = -max;
    }
    return integer / max;
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(integer >= 0 && integer <= max);
    return integer / max;
  }
}

// Does not handle clamping, underflow, overflow, denormalized numbers
export function float32ToFloatBits(n, signBits, exponentBits, mantissaBits, bias) {
  assert(exponentBits <= 8);
  assert(mantissaBits <= 23);
  assert(Number.isFinite(n));

  if (n === 0) {
    return 0;
  }

  if (signBits === 0) {
    assert(n >= 0);
  }

  const buf = new DataView(new ArrayBuffer(Float32Array.BYTES_PER_ELEMENT));
  buf.setFloat32(0, n, true);
  const bits = buf.getUint32(0, true);
  // bits (32): seeeeeeeefffffffffffffffffffffff

  const mantissaBitsToDiscard = 23 - mantissaBits;

  // 0 or 1
  const sign = (bits >> 31) & signBits;

  // >> to remove mantissa, & to remove sign, - 127 to remove bias.
  const exp = ((bits >> 23) & 0xff) - 127;

  // Convert to the new biased exponent.
  const newBiasedExp = bias + exp;
  assert(newBiasedExp >= 0 && newBiasedExp < 1 << exponentBits);

  // Mask only the mantissa, and discard the lower bits.
  const newMantissa = (bits & 0x7fffff) >> mantissaBitsToDiscard;
  return (sign << (exponentBits + mantissaBits)) | (newBiasedExp << mantissaBits) | newMantissa;
}

// Three partial-precision floating-point numbers encoded into a single 32-bit value all
// sharing the same 5-bit exponent.
// There is no sign bit, and there is a shared 5-bit biased (15) exponent and a 9-bit
// mantissa for each channel. The mantissa does NOT have an implicit leading "1.",
// and instead has an implicit leading "0.".
export function packRGB9E5UFloat(r, g, b) {
  for (const v of [r, g, b]) {
    assert(v >= 0 && v < Math.pow(2, 16));
  }

  const buf = new DataView(new ArrayBuffer(Float32Array.BYTES_PER_ELEMENT));
  const extractMantissaAndExponent = n => {
    const mantissaBits = 9;
    buf.setFloat32(0, n, true);
    const bits = buf.getUint32(0, true);
    // >> to remove mantissa, & to remove sign
    let biasedExponent = (bits >> 23) & 0xff;
    const mantissaBitsToDiscard = 23 - mantissaBits;
    let mantissa = (bits & 0x7fffff) >> mantissaBitsToDiscard;

    // RGB9E5UFloat has an implicit leading 0. instead of a leading 1.,
    // so we need to move the 1. into the mantissa and bump the exponent.
    // For float32 encoding, the leading 1 is only present if the biased
    // exponent is non-zero.
    if (biasedExponent !== 0) {
      mantissa = (mantissa >> 1) | 0b100000000;
      biasedExponent += 1;
    }
    return { biasedExponent, mantissa };
  };

  const { biasedExponent: rExp, mantissa: rOrigMantissa } = extractMantissaAndExponent(r);
  const { biasedExponent: gExp, mantissa: gOrigMantissa } = extractMantissaAndExponent(g);
  const { biasedExponent: bExp, mantissa: bOrigMantissa } = extractMantissaAndExponent(b);

  // Use the largest exponent, and shift the mantissa accordingly
  const exp = Math.max(rExp, gExp, bExp);
  const rMantissa = rOrigMantissa >> (exp - rExp);
  const gMantissa = gOrigMantissa >> (exp - gExp);
  const bMantissa = bOrigMantissa >> (exp - bExp);

  const bias = 15;
  const biasedExp = exp === 0 ? 0 : exp - 127 + bias;
  assert(biasedExp >= 0 && biasedExp <= 31);
  return rMantissa | (gMantissa << 9) | (bMantissa << 18) | (biasedExp << 27);
}

export function assertInIntegerRange(n, bits, signed) {
  if (signed) {
    const min = -Math.pow(2, bits - 1);
    const max = Math.pow(2, bits - 1) - 1;
    assert(n >= min && n <= max);
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(n >= 0 && n <= max);
  }
}

export function gammaCompress(n) {
  n = n <= 0.0031308 ? (323 * n) / 25 : (211 * Math.pow(n, 5 / 12) - 11) / 200;
  return clamp(n, 0, 1);
}

export function gammaDecompress(n) {
  n = n <= 0.04045 ? (n * 25) / 323 : Math.pow((200 * n + 11) / 211, 12 / 5);
  return clamp(n, 0, 1);
}
