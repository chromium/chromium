// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which displays the number of selected items, designed
 * to be used as an overlay on top of <cr-toolbar>. See <history-toolbar> for an
 * example usage.
 *
 * Note that the embedder is expected to set position: relative to make the
 * absolute positioning of this element work, and the cr-toolbar should have the
 * has-overlay attribute set when its overlay is shown to prevent access through
 * tab-traversal.
 */

import '../cr_icon_button/cr_icon_button.js';
import '../icons_lit.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrA11yAnnouncerElement} from '../cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from '../cr_a11y_announcer/cr_a11y_announcer.js';

import {getCss} from './cr_toolbar_selection_overlay.css.js';
import {getHtml} from './cr_toolbar_selection_overlay.html.js';

export class CrToolbarSelectionOverlayElement extends CrLitElement {
  static get is() {
    return 'cr-toolbar-selection-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      show: {
        type: Boolean,
        reflect: true,
      },

      cancelLabel: {type: String},
      selectionLabel: {type: String},
    };
  }

  show: boolean = false;
  cancelLabel: string = '';
  selectionLabel: string = '';

  override firstUpdated() {
    this.setAttribute('role', 'toolbar');
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // Parent element is responsible for updating `selectionLabel` when `show`
    // changes.
    if (changedProperties.has('selectionLabel')) {
      if (changedProperties.get('selectionLabel') === undefined &&
          this.selectionLabel === '') {
        return;
      }
      this.setAttribute('aria-label', this.selectionLabel);
      const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
      announcer.announce(this.selectionLabel);
    }
  }

  protected onClearSelectionClick_() {
    this.fire('clear-selected-items');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-selection-overlay': CrToolbarSelectionOverlayElement;
  }
}

customElements.define(
    CrToolbarSelectionOverlayElement.is, CrToolbarSelectionOverlayElement);
