// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A base class for all Files app(xf) widgets.
 * @suppress {checkTypes} closure can't recognize LitElement
 */

import '../common/js/tslib_shim.js';

import {customElement, property, query, state} from 'chrome://resources/mwc/lit/decorators.js';
import {classMap} from 'chrome://resources/mwc/lit/directives/class-map.js';
import {css, html, LitElement, PropertyValues} from 'chrome://resources/mwc/lit/index.js';

export {
  classMap,
  css,
  customElement,
  html,
  property,
  PropertyValues,
  query,
  state,
};

/**
 * A base class for all Files app(xf) widgets.
 */
export class XfBase extends LitElement {}
