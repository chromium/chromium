// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A base class for all Files app(xf) widgets.
 * @suppress {checkTypes} closure can't recognize LitElement
 */

import '../common/js/tslib_shim.js';

import {classMap, css, CSSResult, CSSResultGroup, customElement, html, ifDefined, LitElement, nothing, property, PropertyValues, query, repeat, state, styleMap, svg} from 'chrome://resources/mwc/lit/index.js';

export {
  classMap,
  css,
  CSSResult,
  CSSResultGroup,
  customElement,
  html,
  ifDefined,
  nothing,
  property,
  PropertyValues,
  query,
  repeat,
  state,
  styleMap,
  svg,
};

/**
 * A base class for all Files app(xf) widgets.
 */
export class XfBase extends LitElement {}
