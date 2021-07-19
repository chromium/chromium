/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* This file is generated from:
 *  colors_test.json5
 *  colors_test_palette.json5
 */

import {css} from 'lit-element';
/* SAFETY_BOILERPLATE */

export interface GetColorsCSSOptions {
  lockTheme?: 'light'|'dark';
}

const DEFAULT_CSS = `
html:not(body) {
  --google-grey-900-rgb: 32, 33, 36;
  --google-grey-900: rgb(var(--google-grey-900-rgb));

  --cros-text-color-primary-rgb: var(--google-grey-900-rgb);
  --cros-text-color-primary: var(--google-grey-900);

  --cros-toggle-color-rgb: var(--cros-text-color-primary-rgb);
  --cros-toggle-color: rgba(var(--cros-toggle-color-rgb), 0.1);

  --cros-disabled-opacity: 0.38;

  --cros-reference-opacity: var(--cros-disabled-opacity);
}
`;

const DARK_MODE_OVERRIDES_CSS = `
html:not(body) {
  --cros-text-color-primary-rgb: 255, 255, 255;
  --cros-text-color-primary: rgb(var(--cros-text-color-primary-rgb));

  --cros-toggle-color-rgb: var(--cros-text-color-primary-rgb);
  --cros-toggle-color: rgba(var(--cros-toggle-color-rgb), var(--cros-disabled-opacity));

  --cros-reference-opacity: 1;
}
`;

/**
 * Returns a string containing all semantic colors exported in this file as
 * css variables. This string an be used to construct a stylesheet which can be
 * placed in the dom at runtime, see tools/style_variable_generator/README.md
 * for more info. Ensure the css returned by this function is added to the dom
 * before the rendering of the first element on the page so you are guaranteed
 * that all TS constant references resolve correctly.
 */
export function getColorsCSS(options?: GetColorsCSSOptions) {
  let cssString;
  if (options?.lockTheme === 'light') {
    // Tag strings which are safe with a special comment so copybara can add
    // the right safety wrappers whem moving this code into Google3.
    cssString = /* SAFE */ (DEFAULT_CSS);
  } else if (options?.lockTheme === 'dark') {
    cssString = /* SAFE */ (`
      ${DEFAULT_CSS}
      ${DARK_MODE_OVERRIDES_CSS}
    `);
  } else {
    cssString = /* SAFE */ (`
      ${DEFAULT_CSS}

      @media (prefers-color-scheme: dark) {
        ${DARK_MODE_OVERRIDES_CSS}
      }
    `);
  }

  return cssString;
}

export const GOOGLE_GREY_900 = css`var(--google-grey-900)`;
export const TEXT_COLOR_PRIMARY = css`var(--cros-text-color-primary)`;
export const TOGGLE_COLOR = css`var(--cros-toggle-color)`;
export const DISABLED_OPACITY = css`var(--cros-disabled-opacity)`;
export const REFERENCE_OPACITY = css`var(--cros-reference-opacity)`;
