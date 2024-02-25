/* Copyright 2021 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* This file is generated from:
 *  tests/colors_test.json5
 *  tests/colors_test_palette.json5
 *  tests/typography_test.json5
 */

import {css} from 'lit';
/* SAFETY_BOILERPLATE */

export interface GetColorsCSSOptions {
  /**
   * Generate a css dump which sets variables to either their dark mode or light
   * mode values and ignores the documents prefers-color-scheme.
   */
  lockTheme?: 'light' | 'dark';
  /**
   * Opt into using material 3 color tokens (see go/cros-tokens). If true any
   * legacy mappings specified in the input json5 files will be added into the
   * document.
   */
  useDynamicColors?: boolean;
}

// Use a ternary expression that can only be evaluated at runtime here to force
// closure to leave these string constants as variables instead of inlining them
// in the below template strings. Not doing this results in a unreasonable file
// size increase. See b/209520919.
const DEFAULT_CSS = window ? `
  --google-grey-900-rgb: 32, 33, 36;
  --google-grey-900: rgb(var(--google-grey-900-rgb));

  --cros-text-color-primary-rgb: var(--google-grey-900-rgb);
  --cros-text-color-primary: var(--google-grey-900);

  --cros-toggle-color-rgb: var(--cros-text-color-primary-rgb);
  --cros-toggle-color: rgba(var(--cros-toggle-color-rgb), 0.1);

  --cros-bg-color-elevation-1-rgb: 255, 255, 255;
  --cros-bg-color-elevation-1: rgb(var(--cros-bg-color-elevation-1-rgb));

  --cros-disabled-opacity: 0.38;

  --cros-reference-opacity: var(--cros-disabled-opacity);
` : '';

const DARK_MODE_OVERRIDES_CSS = window ? `
  --cros-text-color-primary-rgb: 255, 255, 255;
  --cros-text-color-primary: rgb(var(--cros-text-color-primary-rgb));

  --cros-toggle-color-rgb: var(--cros-text-color-primary-rgb);
  --cros-toggle-color: rgba(var(--cros-toggle-color-rgb), var(--cros-disabled-opacity));

  --cros-bg-color-elevation-1: color-mix(in srgb, rgb(255, 255, 255) 4.0%, var(--google-grey-900));

  --cros-reference-opacity: 1;
` : '';

const UNTYPED_CSS = window ? `` : '';

const TYPOGRAPHY_CSS = window ? `
  /* font faces */
  @font-face {
    font-family: "GSR";
    src: local("Google Sans Regular");
  }
  @font-face {
    font-family: "GSTM";
    src: local("Google Sans Text Medium");
  }

  /* font families */
  --cros-font-family-test: 'Google Sans', 'Noto Sans', sans-serif;
  --cros-font-family-other: Roboto, 'Noto Sans', sans-serif;

  /* typefaces */
  --cros-headline-1-font: 500 15px/22px var(--cros-font-family-test);
  --cros-headline-1-font-family: var(--cros-font-family-test);
  --cros-headline-1-font-size: 15px;
  --cros-headline-1-font-weight: 500;
  --cros-headline-1-line-height: 22px;
` : '';

const LEGACY_MAPPINGS_CSS = window ? `
    --cros-legacy-color-rgb: var(--cros-text-color-primary-rgb);
    --cros-legacy-color: var(--cros-text-color-primary);
    --cros-legacy-color-light: var(--cros-text-color-primary);
    --cros-legacy-color-dark: var(--cros-text-color-primary);

    --cros-legacy-color-w-opacity-rgb: var(--cros-text-color-primary-rgb);
    --cros-legacy-color-w-opacity: rgba(var(--cros-legacy-color-w-opacity-rgb), 0.3);
    --cros-legacy-color-w-opacity-light: rgba(var(--cros-legacy-color-w-opacity-rgb), 0.3);
    --cros-legacy-color-w-opacity-dark: rgba(var(--cros-legacy-color-w-opacity-rgb), 0.3);
` : '';

/**
 * Returns a string containing all semantic colors exported in this file as
 * css variables. This string an be used to construct a stylesheet which can be
 * placed in the dom at runtime, see tools/style_variable_generator/README.md
 * for more info. Ensure the css returned by this function is added to the dom
 * before the rendering of the first element on the page so you are guaranteed
 * that all TS constant references resolve correctly.
 */
export function getColorsCSS(options?: GetColorsCSSOptions) {
  // Tag strings which are safe with a special comment so copybara can add
  // the right safety wrappers whem moving this code into Google3.
  let cssString = /* SAFE */ ("");

  if (options?.lockTheme === 'light' && !!options?.useDynamicColors === true) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
        ${LEGACY_MAPPINGS_CSS}
      }
      :host([inverted-colors]) {
        ${DARK_MODE_OVERRIDES_CSS}
      }

    `);
  }
  if (options?.lockTheme === 'light' && !!options?.useDynamicColors === false) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
      }
      :host([inverted-colors]) {
        ${DARK_MODE_OVERRIDES_CSS}
      }

    `);
  }
  if (options?.lockTheme === 'dark' && !!options?.useDynamicColors === true) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
        ${DARK_MODE_OVERRIDES_CSS}
        ${LEGACY_MAPPINGS_CSS}
      }
      :host([inverted-colors]) {
        ${DEFAULT_CSS}
      }

    `);
  }
  if (options?.lockTheme === 'dark' && !!options?.useDynamicColors === false) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
        ${DARK_MODE_OVERRIDES_CSS}
      }
      :host([inverted-colors]) {
        ${DEFAULT_CSS}
      }

    `);
  }
  if (options?.lockTheme === undefined && !!options?.useDynamicColors === true) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
        ${LEGACY_MAPPINGS_CSS}
      }
      :host([inverted-colors]) {
        ${DARK_MODE_OVERRIDES_CSS}
      }

      @media (prefers-color-scheme: dark) {
        html:not(body), :host {
          ${DARK_MODE_OVERRIDES_CSS}
          ${LEGACY_MAPPINGS_CSS}
        }
        :host([inverted-colors]) {
          ${DEFAULT_CSS}
        }
      }
    `);
  }
  if (options?.lockTheme === undefined && !!options?.useDynamicColors === false) {
    cssString = /* SAFE */ (`
      html:not(body), :host {
        ${DEFAULT_CSS}
        ${UNTYPED_CSS}
        ${TYPOGRAPHY_CSS}
      }
      :host([inverted-colors]) {
        ${DARK_MODE_OVERRIDES_CSS}
      }

      @media (prefers-color-scheme: dark) {
        html:not(body), :host {
          ${DARK_MODE_OVERRIDES_CSS}
        }
        :host([inverted-colors]) {
          ${DEFAULT_CSS}
        }
      }
    `);
  }
  return cssString;
}

export const GOOGLE_GREY_900 = css`var(--google-grey-900)`;
export const TEXT_COLOR_PRIMARY = css`var(--cros-text-color-primary)`;
export const TOGGLE_COLOR = css`var(--cros-toggle-color)`;
export const BG_COLOR_ELEVATION_1 = css`var(--cros-bg-color-elevation-1)`;
export const DISABLED_OPACITY = css`var(--cros-disabled-opacity)`;
export const REFERENCE_OPACITY = css`var(--cros-reference-opacity)`;

export const FONT_FAMILY_TEST = css`var(--cros-font-family-test)`;
export const FONT_FAMILY_OTHER = css`var(--cros-font-family-other)`;
export const HEADLINE_1_FONT = css`var(--cros-headline-1-font)`;
export const HEADLINE_1_FONT_FAMILY = css`var(--cros-headline-1-font-family)`;
export const HEADLINE_1_FONT_SIZE = css`var(--cros-headline-1-font-size)`;
export const HEADLINE_1_FONT_WEIGHT = css`var(--cros-headline-1-font-weight)`;
export const HEADLINE_1_LINE_HEIGHT = css`var(--cros-headline-1-line-height)`;
