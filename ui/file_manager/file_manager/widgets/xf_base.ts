// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A base class for all Files app(xf) widgets.
 * @suppress {checkTypes} closure can't recognize LitElement
 */

import {classMap} from 'chrome://resources/mwc/lit/directives/class-map.js';
import {ifDefined} from 'chrome://resources/mwc/lit/directives/if-defined.js';
import {repeat} from 'chrome://resources/mwc/lit/directives/repeat.js';
import {styleMap} from 'chrome://resources/mwc/lit/directives/style-map.js';
import {css, CSSResult, CSSResultGroup, html, LitElement, nothing, PropertyValues, svg} from 'lit';
import {customElement, property, query, state} from 'lit/decorators';

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
