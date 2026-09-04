// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ProcessedLottieJson parses Lottie JSON structures to enable
 * dynamic theming with Chrome Desktop Design System (CDDS) tokens.
 */

/** String variant of the type field used for comparison during parsing. */
const LOTTIE_GRADIENT_FILL_TYPE = 'gf';

/**
 * The list of Chrome Desktop Design System (CDDS) tokens that are used to
 * identify shapes and colors in Lottie animation data. If token names change,
 * we will need to update this set. Existing token names are very unlikely to
 * change; it is more likely that new tokens may be added if illustration
 * palettes become more complex.
 */
const CDDS_TOKENS: Set<string> = new Set([
  'cdds.sys.color.illo-primary-min',   'cdds.sys.color.illo-primary-low',
  'cdds.sys.color.illo-primary-mid',   'cdds.sys.color.illo-primary-high',
  'cdds.sys.color.illo-primary-max',

  'cdds.sys.color.illo-secondary-min', 'cdds.sys.color.illo-secondary-low',
  'cdds.sys.color.illo-secondary-mid', 'cdds.sys.color.illo-secondary-high',
  'cdds.sys.color.illo-secondary-max',

  'cdds.sys.color.illo-tertiary-min',  'cdds.sys.color.illo-tertiary-low',
  'cdds.sys.color.illo-tertiary-mid',  'cdds.sys.color.illo-tertiary-high',
  'cdds.sys.color.illo-tertiary-max',

  'cdds.sys.color.illo-neutral-min',   'cdds.sys.color.illo-neutral-low',
  'cdds.sys.color.illo-neutral-mid',   'cdds.sys.color.illo-neutral-high',
  'cdds.sys.color.illo-neutral-max',
]);

/**
 * A structure in the JSON data that should correspond to something that needs
 * to be dynamically colored.
 *
 * The short property names ('nm' for Name, 'ty' for Type, 'c' for Color, 'k'
 * for Keyframes/values, etc.) are dictated by the external Lottie (Bodymovin)
 * file format specification, which minifies schema keys to minimize JSON asset
 * size.
 *
 * @see https://lottiefiles.github.io/lottie-docs/shapes/#shape-item
 */
interface DynamicallyColoredObject {
  /**
   * The name ('nm') of a shape. For Material 3 / CDDS compliant animations,
   * this will be the token name of the color to apply (e.g.
   * "cdds.sys.color.illo-primary-max"), which we use to detect which parts of
   * the JSON data correspond to dynamically colored structures.
   *
   * @see https://lottiefiles.github.io/lottie-docs/shapes/#shape-item
   */
  nm: string;

  /**
   * Optional type ('ty') of the structure (e.g. 'fl' for fill, 'st' for stroke,
   * 'gf' for gradient fill, 'gr' for group). This field will be set to
   * `LOTTIE_GRADIENT_FILL_TYPE` ('gf') if it is a gradient fill structure.
   *
   * @see https://lottiefiles.github.io/lottie-docs/shapes/
   */
  ty?: string;
}

/**
 * Definition of a solid color shape from the Lottie specification.
 * - 'c': Color property containing keyframe value 'k' ([r, g, b, a] floats).
 * - 'sc': Solid color hex string (less common).
 *
 * @see https://lottiefiles.github.io/lottie-docs/values/#colors
 * @see https://lottiefiles.github.io/lottie-docs/properties/#animated-property
 */
interface LottieShape extends DynamicallyColoredObject {
  /** If the color is represented as an array of four floats: [r, g, b, a]. */
  c?: {k: LottieRgbaArray};
  /** If the color is represented as a hex string (less common than `c`). */
  sc?: string;
}

/**
 * This definition of this type is taken from
 * https://lottiefiles.github.io/lottie-docs/shapes/#gradient-fill
 */
interface LottieGradientShape extends DynamicallyColoredObject {
  /** Gradient color type. */
  g: {
    /** The number of colors in this gradient. */
    p: number,
    /** Animated Gradient Colors. */
    k: {
      /**
       * Whether the property is animated, should be 0 for this fill type which
       * means the following `k` type is an array of values rather than
       * keyframes.
       */
      a: 0,
      /**
       * Gradient values over time.
       * Values are grouped in contiguous sub-arrays like [t, r, g, b], where
       * [r,g,b] is a decimal representation of color channel values at time
       * `t`. There should be `p` number of sub arrays. If alpha values are
       * included, they are at the end of the array, grouped as [t, a], where a
       * is the alpha value to apply at time `t`. If alpha values are not
       * included, the k.length will be p*4, with alpha values it is p*6. The
       * relationship between the location of a set of [t, r, g, b] values and
       * an alpha value, is described by a_n = 4p + 2i + 1.
       */
      k: number[],
    },
  };
}

/**
 * A fixed length array type for encoding Lottie colors. The format is [r, g, b,
 * alpha], where each entry is a value between [0-1].
 */
type LottieRgbaArray = [number, number, number, number];

/**
 * A type for storing a css variable and a list of known shapes within the
 * currently loaded animation that have the css variable color applied.
 * There will be one of these structures per token in CDDS_TOKENS that
 * appears in the animation data.
 */
interface TokenColor {
  cssVar: string;
  shapes: LottieShape[];
  gradients: Array<{location: LottieGradientShape, stopIndex: number}>;
}

/**
 * Helper function for converting a 6-character hex color string to an RGB
 * tuple.
 */
function hexToRgb(hexString: string): [number, number, number] {
  const hex = hexString.replace(/^#/, '');
  const r = parseInt(hex.substring(0, 2), 16);
  const g = parseInt(hex.substring(2, 4), 16);
  const b = parseInt(hex.substring(4, 6), 16);
  return [r, g, b];
}

/**
 * Takes in a css color in one of the following formats
 *   - #rrggbb
 *   - #rrggbbaa
 *   - rgb(0-255,0-255,0-255)
 *   - rgba(0-255,0-255,0-255,0-1)
 *   - color(srgb 0-1 0-1 0-1)
 *   - color-mix() see http://go/mdn/CSS/color_value/color-mix
 * And returns the color in the #rrggbbaa format.
 */
function toRgbaHexString(color: string): string {
  /**
   * Converts a string with a number between 0 and 255 in it to a 2 character
   * hex string.
   */
  function fourBitNumberStringToHex(value: string) {
    return Number(value.trim()).toString(16).padStart(2, '0');
  }

  /**
   * Converts a string with a number between 0 and 1 in it to a 2 character
   * hex string.
   */
  function normalizedNumberStringToHex(value: string) {
    const n = Math.floor(Number(value.trim()) * 255);
    return n.toString(16).padStart(2, '0');
  }

  color = color.trim();
  if (color.startsWith('color-mix(')) {
    // color-mix's need to be actually be put into the DOM and rendered for us
    // to get a final rgb color. We do that here however getComputedStyle can
    // return any number of rgb formats so we just override color and pass
    // through to the rest of the logic in this function to normalize to
    // #rrggbbaa.
    const tempDiv = document.createElement('div');
    tempDiv.style.backgroundColor = color;
    tempDiv.style.display = 'none';
    document.body.appendChild(tempDiv);
    color = getComputedStyle(tempDiv).backgroundColor.trim();
    tempDiv.remove();
  }

  // This is not an exhaustive list of all possible ways to represent a color
  // in CSS. Instead this covers all forms the semantic variable generator
  // or getComputedStyle() is known to output.
  if (color.startsWith('#') && color.length === 7) {
    // #rrggbb.
    return `${color}ff`;
  } else if (color.startsWith('#') && color.length === 9) {
    // #rrggbbaa.
    return color;
  } else if (color.startsWith('rgb(')) {
    // rgb(r,g,b).
    const [r, g, b] = color.substring(4, color.length - 1)
                          .split(',')
                          .map(fourBitNumberStringToHex);
    return `#${r}${g}${b}ff`;
  } else if (color.startsWith('rgba(')) {
    // rgba(r,g,b,a).
    const parts = color.substring(5, color.length - 1).split(',');
    const [r, g, b] = parts.slice(0, 3).map(fourBitNumberStringToHex);
    const a = normalizedNumberStringToHex(parts[3] as string);
    return `#${r}${g}${b}${a}`;
  } else if (color.startsWith('color(srgb')) {
    // color(srgb r g b).
    color = color.replace(/\s+/g, ' ');
    const [r, g, b] = color.substring(11, color.length - 1)
                          .split(' ')
                          .map(normalizedNumberStringToHex);
    return `#${r}${g}${b}ff`;
  }

  throw new Error(`Could not parse color: "${color}"`);
}

/**
 * Helper function for converting between the hexadecimal string we get from the
 * computed style to LottieRgbaArray type. Since these come directly
 * from the computed style and color pipeline, we can be confident that we are
 * only going to be parsing 8 digit hexadecimal strings.
 */
function convertHexToLottieRgba(hexString: string): LottieRgbaArray {
  let r: number;
  let g: number;
  let b: number;
  let alpha: number;
  if (hexString.length === 9) {
    // Assume #rrggbbaa format.
    const hexRgb = hexString.slice(0, -2);
    const alphaString = hexString.slice(-2);
    [r, g, b] = hexToRgb(hexRgb);
    alpha = Number(`0x${alphaString}`);
  } else {
    // Assume #rrggbb format.
    [r, g, b] = hexToRgb(hexString);
    alpha = 255;
  }

  return [r / 255, g / 255, b / 255, alpha / 255];
}

/**
 * Converts a CDDS token string to its corresponding Chromium CSS custom
 * property.
 * e.g. "cdds.sys.color.illo-primary-max" -> "--color-sys-illo-primary-max"
 */
function convertCddsTokenToCssVariable(token: string): string {
  const name = token.replace('cdds.sys.color.', '').replaceAll('.', '-');
  return `--color-sys-${name}`;
}

function getOrCreateTokenColor(
    colors: Map<string, TokenColor>, tokenName: string): TokenColor {
  if (!colors.has(tokenName)) {
    colors.set(tokenName, {
      cssVar: convertCddsTokenToCssVariable(tokenName),
      shapes: [],
      gradients: [],
    });
  }
  return colors.get(tokenName)!;
}

function isCddsToken(token: string): boolean {
  return CDDS_TOKENS.has(token);
}

function isValidShapeName(tokenName: string|null): boolean {
  if (tokenName === null) {
    return false;
  }
  const tokens = tokenName.split(',');
  return tokens.some(t => isCddsToken(t.trim()));
}

/**
 * A class wrapping a lottie JSON file that has been injected with dynamic
 * variables that can be hot swapped with new values at runtime.
 */
export class ProcessedLottieJson<T extends object = Record<string, unknown>> {
  private colorReferences_ = new Map<string, TokenColor>();
  numMappedColors = 0;

  constructor(private readonly animationData_: T) {
    this.traverseJson_(this.animationData_);
  }

  /**
   * Recursively traverses through a jsonObj, looking for known keys and tokens,
   * and saving them in the `shapes` and `gradients` map.
   */
  private traverseJson_(node: unknown) {
    if (!node || typeof node !== 'object') {
      return;
    }

    const shape =
        node as Partial<DynamicallyColoredObject>| Partial<LottieGradientShape>;
    const shapeName = shape.nm || null;

    if (isValidShapeName(shapeName)) {
      this.numMappedColors++;

      // Attempt to parse the object as a gradient, otherwise we assume it is
      // a regular shape. If more complex animation types get added, this
      // logic will need to be updated along with the types.
      if (shape.ty === LOTTIE_GRADIENT_FILL_TYPE) {
        const stops = shapeName!.split(',');
        for (let i = 0; i < stops.length; i++) {
          const stopTokenName = stops[i]!.trim();
          if (!isCddsToken(stopTokenName)) {
            continue;
          }

          const tokenColor =
              getOrCreateTokenColor(this.colorReferences_, stopTokenName);
          tokenColor.gradients.push(
              {location: shape as LottieGradientShape, stopIndex: i});
        }
      } else {
        const tokenColor =
            getOrCreateTokenColor(this.colorReferences_, shapeName!.trim());
        tokenColor.shapes.push(shape as LottieShape);
      }
    }

    for (const value of Object.values(node)) {
      this.traverseJson_(value);
    }
  }

  /**
   * Finds all dynamic colors in the underlying json file and updates their
   * values to match the current color palette in the document.
   *
   * @param styles A CSSStyleDeclaration from getComputedStyle() on the element
   * to pull css variable values from.
   * @returns A list of warnings in attempting to update the color on the json
   * file.
   */
  updateColors(styles: CSSStyleDeclaration): string[] {
    const warnings: string[] = [];
    for (const color of this.colorReferences_.values()) {
      let computedColor: string;
      try {
        computedColor =
            toRgbaHexString(styles.getPropertyValue(color.cssVar).trim());
      } catch {
        warnings.push(`Unable to get value of ${
            color.cssVar}. Are token css vars installed in this page?`);
        computedColor = '#000000';
      }
      const colorArray = convertHexToLottieRgba(computedColor);
      for (const location of color.shapes) {
        if (location.c) {
          location.c.k = colorArray;
        } else if (location.sc) {
          location.sc = computedColor;
        } else {
          warnings.push(
              `Unable to assign color to shape: ${JSON.stringify(location)}`);
        }
      }

      for (const {location, stopIndex} of color.gradients) {
        const gradientFillPoints = location.g.k.k;
        gradientFillPoints[(4 * stopIndex) + 1] = colorArray[0]!;
        gradientFillPoints[(4 * stopIndex) + 2] = colorArray[1]!;
        gradientFillPoints[(4 * stopIndex) + 3] = colorArray[2]!;
      }
    }

    return warnings;
  }

  getAnimationData(): T {
    return this.animationData_;
  }
}
