// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions for color manipulations.
 */

import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

/**
 * Converts an SkColor object to a string in the form
 * "rgba(<red>, <green>, <blue>, <alpha>)".
 * @param skColor The input color.
 * @return The rgba string.
 */
export function skColorToRgba(skColor: SkColor): string {
  const a = (skColor.value >> 24) & 0xff;
  const r = (skColor.value >> 16) & 0xff;
  const g = (skColor.value >> 8) & 0xff;
  const b = skColor.value & 0xff;
  return `rgba(${r}, ${g}, ${b}, ${(a / 255).toFixed(2)})`;
}

/**
 * Converts an SkColor object to a string in the the form "#rrggbb".
 * @param skColor The input color.
 * @return The hex color string,
 */
export function skColorToHexColor(skColor: SkColor): string {
  const r = (skColor.value >> 16) & 0xff;
  const g = (skColor.value >> 8) & 0xff;
  const b = skColor.value & 0xff;
  const rHex = r.toString(16).padStart(2, '0');
  const gHex = g.toString(16).padStart(2, '0');
  const bHex = b.toString(16).padStart(2, '0');
  return `#${rHex}${gHex}${bHex}`;
}

/**
 * Converts a string of the form "#rrggbb" to an SkColor object.
 * @param hexColor The color string.
 * @return The SkColor object,
 */
export function hexColorToSkColor(hexColor: string): SkColor {
  if (!/^#[0-9a-f]{6}$/.test(hexColor)) {
    return {value: 0};
  }
  const r = parseInt(hexColor.substring(1, 3), 16);
  const g = parseInt(hexColor.substring(3, 5), 16);
  const b = parseInt(hexColor.substring(5, 7), 16);
  return {value: 0xff000000 + (r << 16) + (g << 8) + b};
}

/**
 * Converts a string of the form "<red>, <green>, <blue>" to an SkColor
 * object.
 * @param rgb The rgb color string.
 * @return The SkColor object,
 */
export function rgbToSkColor(rgb: string): SkColor {
  const rgbValues = rgb.split(',');
  const hex = rgbValues.map((bit) => {
    bit = parseInt(bit).toString(16);
    return (bit.length === 1) ? '0' + bit : bit;
  });
  return hexColorToSkColor('#' + hex.join(''));
}
